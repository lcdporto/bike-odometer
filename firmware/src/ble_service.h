/*
 * BLE Service module - GATT service and advertising
 */

#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize BLE (enable Bluetooth stack)
 * @return 0 on success, negative errno on failure
 */
int ble_service_init(void);

/**
 * @brief Start connectable advertising for a limited time
 *
 * If already advertising, the timeout is restarted. If connected, the request
 * is ignored because the radio is already available to the phone.
 *
 * @param duration_seconds Advertising window length in seconds
 */
void ble_service_start_advertising_for(uint32_t duration_seconds);

/**
 * @brief Stop advertising when the device enters parked mode
 */
void ble_service_stop_advertising(void);

/**
 * @brief Return whether a phone is currently connected
 */
bool ble_service_is_connected(void);

#endif /* BLE_SERVICE_H */
