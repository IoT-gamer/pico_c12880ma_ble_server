#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"
#include "pico/stdio_usb.h"

#include "c12880ma.h"
#include "ble_server.h"

// Define pins matching the standard wiring example
c12880ma_config_t spec_cfg = {
    .clk_pin = 2,
    .st_pin = 3,
    .video_pin = 26, 
    .led_pin = 4,
    .integration_time = 85 // Integration time in milliseconds
};

static btstack_timer_source_t heartbeat;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint16_t spectral_data[C12880MA_CHANNELS];

static int current_state = 0;

static void heartbeat_handler(btstack_timer_source_t *ts) {
    switch (current_state) {
        case 0:
            // FLUSH: Read and discard to clear ambient charge
            c12880ma_read(&spec_cfg, spectral_data);
            
            // Wait for the exact integration time (in milliseconds)
            btstack_run_loop_set_timer(ts, spec_cfg.integration_time);
            current_state = 1;
            break;
            
        case 1:
            // CAPTURE: Read the fresh charge accumulated during the timer delay
            c12880ma_read(&spec_cfg, spectral_data);
            
            // Send the first 96 pixels
            ble_server_notify_chunk(spectral_data, 0);
            btstack_run_loop_set_timer(ts, 15); // Yield CPU to CYW43 for chunk 1
            current_state = 2;
            break;
            
        case 2:
            // Send the middle 96 pixels
            ble_server_notify_chunk(spectral_data, 1);
            btstack_run_loop_set_timer(ts, 15); // Yield CPU to CYW43 for chunk 2
            current_state = 3;
            break;
            
        case 3:
            // Send the final 96 pixels
            ble_server_notify_chunk(spectral_data, 2);
            
            // Frame complete. Wait 100ms before starting the next full cycle
            btstack_run_loop_set_timer(ts, 100); 
            current_state = 0;
            break;
    }
    
    btstack_run_loop_add_timer(ts);
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
            ble_server_start_advertising();
            break;
        case HCI_EVENT_LE_META:
        case HCI_EVENT_DISCONNECTION_COMPLETE:
        case ATT_EVENT_MTU_EXCHANGE_COMPLETE: 
            ble_server_handle_hci_event(packet_type, channel, packet, size);
            break;
    }
}

int main() {
    stdio_init_all();
    sleep_ms(2000); 
    printf("Starting C12880MA Spectrometer BLE Server...\n");

    if (cyw43_arch_init()) {
        printf("Error: Failed to init CYW43.\n");
        return -1;
    }

    // Initialize Spectrometer GPIOs and ADC
    c12880ma_init(&spec_cfg);
    c12880ma_set_led(&spec_cfg, false); 

    // Setup BLE Stack
    l2cap_init();
    sm_init();

    ble_server_init(packet_handler);

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // Start State Machine Timer
    heartbeat.process = &heartbeat_handler;
    btstack_run_loop_set_timer(&heartbeat, 100);
    btstack_run_loop_add_timer(&heartbeat);

    hci_power_control(HCI_POWER_ON);
    btstack_run_loop_execute();

    return 0;
}