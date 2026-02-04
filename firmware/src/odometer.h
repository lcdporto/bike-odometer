/*
 * Odometer module - Pulse counting and bin storage
 */

#ifndef ODOMETER_H
#define ODOMETER_H

#include <stdint.h>
#include <stddef.h>

/* Wheel circumference in millimeters (default 2100mm = 2.1m). */
#ifndef WHEEL_CIRCUMFERENCE_MM
#define WHEEL_CIRCUMFERENCE_MM 2100U
#endif

#define BIN_INTERVAL_MINUTES 1
#define MAX_TRIPS 5
#define BUCKETS_PER_TRIP 12

struct trip_entry {
	uint64_t start_timestamp_ms;
	uint32_t buckets[BUCKETS_PER_TRIP];  /* pulse counts per bucket */
	size_t bucket_count;
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
 * @brief Get pointer to trips array
 */
const struct trip_entry *odometer_get_trips(void);

/**
 * @brief Get current trip count
 */
size_t odometer_get_trip_count(void);

/**
 * @brief Load odometer data from NVM
 */
void odometer_load_from_nvm(void);

#endif /* ODOMETER_H */
