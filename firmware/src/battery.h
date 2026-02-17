/*
 * Battery module - CR2032 voltage estimation via ADC
 */

#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>

/**
 * @brief Read battery voltage and estimate charge level
 * 
 * Should be called early at boot with minimal system load
 * to get accurate reading given CR2032 high internal resistance.
 * 
 * @return 0 on success, negative errno on failure
 */
int battery_measure(void);

/**
 * @brief Get last measured battery voltage in millivolts
 */
uint16_t battery_get_voltage_mv(void);

/**
 * @brief Get estimated battery percentage (0-100)
 */
uint8_t battery_get_percent(void);

#endif /* BATTERY_H */
