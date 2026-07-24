/*
 * Odometer module - Pulse counting and trip storage
 *
 * NVM-Direct Architecture:
 * - Only current trip in RAM (~204 bytes)
 * - Completed trips stored directly to NVM
 * - Trips read on-demand from NVM
 * 
 * Capacity with the shared nRF54L10/nRF54L15 storage layout:
 * - Newest 3000 trips retained in a circular archive
 * - Full 5-min bucket detail for retained trips
 * - Stable, monotonically increasing public trip IDs
 */

#ifndef ODOMETER_H
#define ODOMETER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Default wheel size: 26" = 2600 hundredths of an inch */
#define DEFAULT_WHEEL_SIZE_X100  2600U

/* Calculate wheel circumference in mm from size_x100 (C = pi * D) */
#define WHEEL_CIRCUMFERENCE_MM(size_x100) \
	((uint32_t)((((uint64_t)(size_x100) * 79796ULL) + 50000ULL) / 100000ULL))

/* Storage configuration - 5-min bins, 4h max sessions */
#define BIN_INTERVAL_MINUTES  5     /* 5-min bins (client requirement) */
#define MAX_TRIPS             3000  /* Newest ~8 years at 1 trip/day */
#define BUCKETS_PER_TRIP      48    /* 48 bins * 5 min = 4 hours max per trip */

/* Trip entry with full bucket detail (~204 bytes each) */
struct trip_entry {
	uint64_t start_timestamp_s;
	uint32_t buckets[BUCKETS_PER_TRIP];  /* pulse counts per bucket */
	uint16_t bucket_count;               /* number of valid buckets */
	uint16_t _reserved;                  /* alignment padding */
};

/**
 * @brief Initialize the odometer (pulse GPIO and bin timer)
 * @return 0 on success, negative errno on failure
 */
int odometer_init(void);

/**
 * @brief Get current pulse count (since last bin)
 */
uint32_t odometer_get_pulse_count(void);

/**
 * @brief Get total trip count stored in NVM
 */
size_t odometer_get_trip_count(void);

/**
 * @brief Get the stable BLE/NVS identifier for a trip index
 * @param index Trip index (0 = oldest, trip_count-1 = newest/current)
 * @return Stable identifier, or 0 if index is invalid
 */
uint32_t odometer_get_trip_id(size_t index);

/**
 * @brief Read a specific trip from NVM
 * @param index Trip index (0 = oldest, trip_count-1 = newest)
 * @param trip_out Pointer to trip_entry struct to fill
 * @return 0 on success, negative errno on failure
 */
int odometer_read_trip(size_t index, struct trip_entry *trip_out);

/**
 * @brief Get pointer to current (active) trip, or NULL if no active trip
 */
const struct trip_entry *odometer_get_current_trip(void);

/**
 * @brief Check if there's an active trip in progress
 */
bool odometer_is_trip_active(void);

/**
 * @brief Load odometer data from NVM
 */
void odometer_load_from_nvm(void);

/**
 * @brief Get wheel size in inches * 100 (e.g., 2630 = 26.30")
 */
uint32_t odometer_get_wheel_size_x100(void);

/**
 * @brief Set wheel size in inches * 100 and persist to NVM
 * @param size_x100 Wheel diameter in hundredths of inches
 */
void odometer_set_wheel_size_x100(uint32_t size_x100);

#endif /* ODOMETER_H */
