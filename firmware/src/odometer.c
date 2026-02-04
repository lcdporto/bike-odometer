/*
 * Odometer module - Pulse counting and trip storage
 */

#include "odometer.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/printk.h>
#include <zephyr/irq.h>
#include <zephyr/pm/pm.h>
#include <zephyr/sys/poweroff.h>
#include <hal/nrf_gpio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define DEBUG_ODOMETER 1

/* GPIO configuration */
#define PULSE_GPIO_NODE DT_NODELABEL(gpio0)

#ifndef PULSE_PIN
#define PULSE_PIN 11
#endif

/* Module state */
static volatile uint32_t pulse_count;
static struct gpio_callback pulse_cb_data;
static struct k_timer bin_timer;
static struct k_work save_work;

/* Trip storage */
static struct trip_entry trips[MAX_TRIPS];
static size_t trip_count;
static bool in_active_trip;  /* true if last bin had pulses */

/* Pending bin data to save (set by timer, saved by work handler) */
static uint32_t pending_pulses;
static bool pending_save;

/* Deep sleep after this many consecutive zero-pulse bins */
#define ZERO_BINS_BEFORE_SLEEP 2
static uint32_t consecutive_zero_bins;

/* Forward declarations */
static void pulse_gpio_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
static void bin_timer_handler(struct k_timer *timer);
static void save_work_handler(struct k_work *work);
static void store_bin(uint32_t pulses);
static void save_trips_to_nvm(void);
static void enter_deep_sleep(void);

/*
 * GPIO interrupt callback for pulse detection
 */
static void pulse_gpio_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	unsigned int key = irq_lock();
	pulse_count++;
#ifdef DEBUG_ODOMETER
	printk("DEBUG_ODOMETER: pulse detected pins=0x%08x total=%u\n", pins, pulse_count);
#endif
	irq_unlock(key);
}

/*
 * Timer callback - captures pulse count and schedules save work
 */
static void bin_timer_handler(struct k_timer *timer)
{
	unsigned int key = irq_lock();
	pending_pulses = pulse_count;
	pulse_count = 0;
	pending_save = true;
	irq_unlock(key);

	/* Schedule work to run in thread context for flash write */
	k_work_submit(&save_work);
}

/*
 * Enter deep sleep (system off) with pulse pin as wake-up source
 */
static void enter_deep_sleep(void)
{
	printk("Entering deep sleep... pulse pin %d will wake device\\n", PULSE_PIN);

	/* Give time for the message to be transmitted */
	k_sleep(K_MSEC(100));

	/* Configure the pulse pin as a wake-up source using SENSE
	 * The pin is configured with pull-up, so we sense LOW (falling edge)
	 */
	nrf_gpio_cfg_sense_input(NRF_GPIO_PIN_MAP(0, PULSE_PIN),
							 NRF_GPIO_PIN_PULLUP,
							 NRF_GPIO_PIN_SENSE_LOW);

	/* Enter system off mode - device will reset on wake-up */
	sys_poweroff();

	/* Should not reach here */
	printk("Deep sleep failed!\\n");
}

/*
 * Work handler - stores bin and persists to NVM (runs in thread context)
 */
static void save_work_handler(struct k_work *work)
{
	if (!pending_save) {
		return;
	}
	pending_save = false;
	store_bin(pending_pulses);
}

/*
 * Store a bin entry - handles trip logic
 */
static void store_bin(uint32_t pulses)
{
	uint64_t ts = k_uptime_get();

	/* If no pulses, end the current trip (if any) */
	if (pulses == 0) {
		if (in_active_trip) {
			printk("Trip ended (no pulses)\n");
			in_active_trip = false;
			save_trips_to_nvm();  /* Ensure trip is saved before potential sleep */
		}
		
		consecutive_zero_bins++;
		printk("Zero bins: %u/%d before sleep\n", consecutive_zero_bins, ZERO_BINS_BEFORE_SLEEP);
		
		if (consecutive_zero_bins >= ZERO_BINS_BEFORE_SLEEP) {
			enter_deep_sleep();
		}
		return;
	}

	/* We have pulses - reset consecutive zero counter */
	consecutive_zero_bins = 0;

	/* We have pulses - check if we need to start a new trip */
	if (!in_active_trip) {
		/* Start a new trip */
		if (trip_count >= MAX_TRIPS) {
			/* Shift trips to make room (discard oldest) */
			memmove(&trips[0], &trips[1], sizeof(struct trip_entry) * (MAX_TRIPS - 1));
			trip_count = MAX_TRIPS - 1;
			printk("Discarded oldest trip to make room\n");
		}

		/* Initialize new trip */
		size_t new_idx = trip_count;
		memset(&trips[new_idx], 0, sizeof(struct trip_entry));
		trips[new_idx].start_timestamp_ms = ts;
		trips[new_idx].bucket_count = 0;
		trip_count++;
		in_active_trip = true;

		printk("Started new trip (id=%zu, ts=%llu)\n", trip_count, (unsigned long long)ts);
	}

	/* Add bucket to current trip */
	size_t current_trip = trip_count - 1;
	struct trip_entry *trip = &trips[current_trip];

	if (trip->bucket_count >= BUCKETS_PER_TRIP) {
		/* Trip is full - start a new one */
		printk("Trip bucket limit reached, starting new trip\n");
		in_active_trip = false;
		store_bin(pulses);  /* Recursive call will start new trip */
		return;
	}

	trip->buckets[trip->bucket_count] = pulses;
	trip->bucket_count++;

	uint32_t distance = pulses * WHEEL_CIRCUMFERENCE_MM;
	printk("Bucket stored: trip=%zu bucket=%zu pulses=%u distance_m=%.3f\n",
		   current_trip + 1, trip->bucket_count, pulses, distance / 1000.0);

	/* Persist to NVM */
	save_trips_to_nvm();
}

/*
 * Save all trips to NVM
 */
static void save_trips_to_nvm(void)
{
#if IS_ENABLED(CONFIG_SETTINGS)
	/* Save trip count */
	int err = settings_save_one("odom/tcnt", &trip_count, sizeof(trip_count));
	if (err) {
		printk("NVM: save trip_count FAILED (%d)\n", err);
	}

	/* Save in_active_trip flag */
	err = settings_save_one("odom/active", &in_active_trip, sizeof(in_active_trip));
	if (err) {
		printk("NVM: save in_active_trip FAILED (%d)\n", err);
	}

	/* Save each trip */
	for (size_t i = 0; i < trip_count; i++) {
		char key[32];
		snprintf(key, sizeof(key), "odom/trip/%zu", i);
		err = settings_save_one(key, &trips[i], sizeof(struct trip_entry));
		if (err) {
			printk("NVM: save trip[%zu] FAILED (%d)\n", i, err);
		} else {
			printk("NVM: save trip[%zu] OK (buckets=%zu)\n", i, trips[i].bucket_count);
		}
	}
#endif
}

/*
 * Settings handler for loading odometer data from NVM
 */
#if IS_ENABLED(CONFIG_SETTINGS)
static int odom_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	if (!name) {
		return -ENOENT;
	}

	/* Handle trip_count */
	if (strcmp(name, "tcnt") == 0) {
		if (len != sizeof(trip_count)) {
			return -EINVAL;
		}
		ssize_t rc = read_cb(cb_arg, &trip_count, sizeof(trip_count));
		if (rc < 0) {
			return rc;
		}
		if (trip_count > MAX_TRIPS) {
			trip_count = MAX_TRIPS;
		}
		printk("NVM: Loaded trip_count=%zu\n", trip_count);
		return 0;
	}

	/* Handle in_active_trip */
	if (strcmp(name, "active") == 0) {
		if (len != sizeof(in_active_trip)) {
			return -EINVAL;
		}
		ssize_t rc = read_cb(cb_arg, &in_active_trip, sizeof(in_active_trip));
		if (rc < 0) {
			return rc;
		}
		printk("NVM: Loaded in_active_trip=%d\n", in_active_trip);
		return 0;
	}

	/* Handle trip data: "trip/<index>" */
	if (strncmp(name, "trip/", 5) != 0) {
		return -ENOENT;
	}

	int idx = atoi(name + 5);
	if (idx < 0 || idx >= (int)MAX_TRIPS) {
		return -EINVAL;
	}

	if (len != sizeof(struct trip_entry)) {
		return -EINVAL;
	}

	ssize_t rc = read_cb(cb_arg, &trips[idx], sizeof(struct trip_entry));
	if (rc < 0) {
		return rc;
	}

	printk("NVM: Loaded trip[%d]: start_ts=%llu buckets=%zu\n",
		   idx, (unsigned long long)trips[idx].start_timestamp_ms,
		   trips[idx].bucket_count);

	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(odom, "odom", NULL, odom_set, NULL, 0);
#endif

/*
 * Public API
 */

int odometer_init(void)
{
	const struct device *gpio_dev = DEVICE_DT_GET(PULSE_GPIO_NODE);

	if (!device_is_ready(gpio_dev)) {
		printk("Pulse GPIO device not ready\n");
		return -ENODEV;
	}

	int err = gpio_pin_configure(gpio_dev, PULSE_PIN, GPIO_INPUT | GPIO_PULL_UP);
	if (err) {
		printk("Failed to configure pulse pin %d (err %d)\n", PULSE_PIN, err);
		return err;
	}

	gpio_init_callback(&pulse_cb_data, pulse_gpio_callback, (1U << PULSE_PIN));
	gpio_add_callback(gpio_dev, &pulse_cb_data);
	gpio_pin_interrupt_configure(gpio_dev, PULSE_PIN, GPIO_INT_EDGE_TO_ACTIVE);
	printk("Pulse GPIO configured on gpio0 pin %d\n", PULSE_PIN);

	/* Initialize work queue for NVM saves */
	k_work_init(&save_work, save_work_handler);

	/* Start bin timer */
	k_timer_init(&bin_timer, bin_timer_handler, NULL);
	k_timer_start(&bin_timer, K_MINUTES(BIN_INTERVAL_MINUTES), K_MINUTES(BIN_INTERVAL_MINUTES));
	printk("Bin timer started (%d minute interval)\n", BIN_INTERVAL_MINUTES);

	return 0;
}

uint32_t odometer_get_pulse_count(void)
{
	uint32_t pulses;
	unsigned int key = irq_lock();
	pulses = pulse_count;
	irq_unlock(key);
	return pulses;
}

const struct trip_entry *odometer_get_trips(void)
{
	return trips;
}

size_t odometer_get_trip_count(void)
{
	return trip_count;
}

void odometer_load_from_nvm(void)
{
#if IS_ENABLED(CONFIG_SETTINGS)
	int rc = settings_subsys_init();
	if (rc) {
		printk("settings_subsys_init failed (err %d)\n", rc);
		return;
	}
	printk("Settings subsystem initialized\n");

	rc = settings_load();
	if (rc) {
		printk("settings_load failed (err %d)\n", rc);
	} else {
		printk("Settings loaded: %zu trips, active=%d\n", trip_count, in_active_trip);
		for (size_t i = 0; i < trip_count; i++) {
			printk("  trip[%zu]: start=%llu buckets=%zu\n",
				   i, (unsigned long long)trips[i].start_timestamp_ms,
				   trips[i].bucket_count);
		}
	}
#endif
}
