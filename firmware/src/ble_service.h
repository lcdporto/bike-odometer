/*
 * BLE Service module - GATT service and advertising
 */

#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <stdbool.h>

/**
 * @brief Initialize BLE (enable Bluetooth stack)
 * @return 0 on success, negative errno on failure
 */
int ble_service_init(void);

/**
 * @brief Start BLE advertising
 */
void ble_service_start_advertising(void);

#endif /* BLE_SERVICE_H */
