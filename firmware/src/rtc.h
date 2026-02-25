/*
 * RTC module - Software real-time clock using system uptime
 *
 * Since nRF54L doesn't have a battery-backed RTC, we use a software
 * implementation based on k_uptime_get(). The base time is set via BLE
 * and persisted in NVS.
 *
 * Time accuracy is lost during deep sleep or reset, but is re-synced
 * when the phone reconnects.
 */

#ifndef RTC_H
#define RTC_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize the RTC module
 *
 * Loads the last known time from NVS if available.
 */
void rtc_init(void);

/**
 * @brief Set the current time
 *
 * @param unix_time Unix timestamp (seconds since 1970-01-01 00:00:00 UTC)
 */
void rtc_set_time(uint64_t unix_time);

/**
 * @brief Get the current time
 *
 * @return Unix timestamp, or 0 if time has never been set
 */
uint64_t rtc_get_time(void);

/**
 * @brief Check if time has been set
 *
 * @return true if time has been synchronized via BLE
 */
bool rtc_is_time_set(void);

/**
 * @brief Get time as milliseconds since epoch
 *
 * @return Unix timestamp in milliseconds
 */
uint64_t rtc_get_time_ms(void);

#endif /* RTC_H */
