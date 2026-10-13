#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#include "btstack.h"
#include <stdint.h>
#include <stdbool.h>

void ble_server_init(btstack_packet_handler_t att_packet_handler);
void ble_server_start_advertising(void);
void ble_server_stop_advertising(void);
void ble_server_handle_hci_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

void ble_server_notify_chunk(uint16_t *channels, int chunk_index);

hci_con_handle_t ble_server_get_con_handle(void);

#endif // BLE_SERVER_H