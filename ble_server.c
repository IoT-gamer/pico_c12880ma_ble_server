#include "ble_server.h"
#include <stdio.h>
#include <string.h>
#include "btstack.h"
#include "pico/stdlib.h"
#include "c12880ma.h" 

// Include the generated GATT header
#include "spectrometer_profile.h"

static hci_con_handle_t server_con_handle = HCI_CON_HANDLE_INVALID;

// Reference to the sensor config defined in main.c
extern c12880ma_config_t spec_cfg; 

static uint8_t adv_data[] = {
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    0x0E, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 
    'P', 'i', 'c', 'o', '-', 'C', '1', '2', '8', '8', '0', 'M', 'A',
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x00, 0xFF
};
static const uint8_t adv_data_len = sizeof(adv_data);

static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);
static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);

void ble_server_init(btstack_packet_handler_t att_packet_handler) {
    att_server_init(profile_data, att_read_callback, att_write_callback);
    att_server_register_packet_handler(att_packet_handler);
}

void ble_server_start_advertising(void) {
    uint16_t adv_int = 800; 
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int, adv_int, 0, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);
}

void ble_server_stop_advertising(void) {
    gap_advertisements_enable(0);
}

void ble_server_handle_hci_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t event_type = hci_event_packet_get_type(packet);

    switch (event_type) {
        case HCI_EVENT_LE_META:
            if (hci_event_le_meta_get_subevent_code(packet) == HCI_SUBEVENT_LE_CONNECTION_COMPLETE) {
                server_con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                ble_server_stop_advertising();
            }
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            if (hci_event_disconnection_complete_get_connection_handle(packet) == server_con_handle) {
                server_con_handle = HCI_CON_HANDLE_INVALID;
                ble_server_start_advertising();
            }
            break;
    }
}

void ble_server_notify_chunk(uint16_t *channels, int chunk_index) {
    if (server_con_handle == HCI_CON_HANDLE_INVALID) return;
    if (chunk_index < 0 || chunk_index > 2) return;

    uint16_t handles[] = {
        ATT_CHARACTERISTIC_0000FF01_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE,
        ATT_CHARACTERISTIC_0000FF02_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE,
        ATT_CHARACTERISTIC_0000FF03_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE
    };

    uint8_t *data_ptr = (uint8_t *)&channels[chunk_index * 96];
    uint16_t data_len = 96 * sizeof(uint16_t);

    int err = att_server_notify(server_con_handle, handles[chunk_index], data_ptr, data_len);
    if (err) {
        printf("[BLE] Notify failed on chunk %d with error: 0x%02X\n", chunk_index, err);
    }
}

static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size) {
    return 0; // No read characteristics defined in this profile
}

static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size) {
    // Control Characteristic (0xFF04)
    if (att_handle == ATT_CHARACTERISTIC_0000FF04_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE) {
        if (buffer_size < 2) return 0; 
        
        uint8_t cmd = buffer[0];

        if (cmd == 0x01 && buffer_size >= 2) {
            // Cmd 0x01: Set LED (1 Byte Payload)
            uint8_t val = buffer[1];
            c12880ma_set_led(&spec_cfg, val > 0);
        }
        else if (cmd == 0x02 && buffer_size >= 3) {
            // Cmd 0x02: Set Integration Time in milliseconds (2 Byte Payload, Little Endian)
            uint16_t integration_ms = buffer[1] | (buffer[2] << 8);
            
            // Enforce a sensible minimum (1 ms)
            spec_cfg.integration_time = integration_ms < 1 ? 1 : integration_ms;
        }
    }
    return 0;
}