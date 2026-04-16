/*
 * Odometer module - Pulse counting and NVM-direct trip storage
 *
 * Architecture:
 * - Only current (active) trip kept in RAM (~204 bytes)
 * - Completed trips stored directly to NVS
 * - Trips read from NVS on-demand
 * - Supports 3000+ trips (8+ years)
 */

#include "odometer.h"
#include "rtc.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/printk.h>
#include <zephyr/irq.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

//#define DEBUG 1

/* GPIO configuration from devicetree */
#define PULSE_NODE DT_ALIAS(pulse_sensor)

#if !DT_NODE_EXISTS(PULSE_NODE)
#error "pulse-sensor alias not defined in devicetree"
#endif

static const struct gpio_dt_spec pulse_gpio = GPIO_DT_SPEC_GET(PULSE_NODE, gpios);

/* NVS for direct trip storage */
#define NVS_PARTITION		storage_partition
#define NVS_PARTITION_DEVICE	FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(NVS_PARTITION)
#define NVS_PARTITION_SIZE	FIXED_PARTITION_SIZE(NVS_PARTITION)

/* NVS IDs - trips use IDs 1000+ */
#define NVS_ID_TRIP_COUNT    1
#define NVS_ID_ACTIVE_FLAG   3
#define NVS_ID_WHEEL_SIZE    8
#define NVS_ID_TRIP_BASE     1000  /* Trip IDs: 1000, 1001, 1002, ... */

static struct nvs_fs nvs;
static bool nvs_ready;

/* Module state */
static volatile uint32_t pulse_count;
static struct gpio_callback pulse_cb_data;
static struct k_timer bin_timer;
static struct k_work save_work;

/* Current trip - only one trip in RAM at a time */
static struct trip_entry current_trip;
static bool in_active_trip;

/* Trip count in NVS */
static size_t nvm_trip_count;

/* Wheel size (hundredths of inches, e.g., 2600 = 26.00") */
static uint32_t wheel_size_x100 = DEFAULT_WHEEL_SIZE_X100;

/* Pending bin data */
static uint32_t pending_pulses;
static bool pending_save;

#define ZERO_BINS_BEFORE_SLEEP 2
static uint32_t consecutive_zero_bins;

/* Forward declarations */
static void pulse_gpio_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
static void bin_timer_handler(struct k_timer *timer);
static void save_work_handler(struct k_work *work);
static void store_bin(uint32_t pulses);
static void save_current_trip_to_nvs(bool completed);
static void save_metadata_to_nvs(void);
static void enter_low_power_idle(void);
static int nvs_init_storage(void);

/*
 * GPIO interrupt callback for pulse detection
 */
static void pulse_gpio_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	unsigned int key = irq_lock();
	pulse_count++;
#ifdef DEBUG
	printk("DEBUG: pulse detected pins=0x%08x total=%u\n", pins, pulse_count);
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
 * Enter low power idle mode with pulse pin as wake-up source.
 * Unlike system off, this preserves GRTC so time keeps ticking.
 * The GPIO interrupt will wake the system on pulse detection.
 */
static void enter_low_power_idle(void)
{
	#ifdef DEBUG
	printk("Entering low power idle... GRTC keeps running, P0.%02d will wake\n", pulse_gpio.pin);
	#endif

	/* Stop the bin timer to avoid periodic wakeups */
	k_timer_stop(&bin_timer);

	/* GPIO interrupt is still configured from init, so any pulse
	 * will wake us up. The system will naturally enter idle sleep.
	 * When a pulse arrives, the ISR fires and wakes main context.
	 */

	/* Wait for pulse - system enters automatic low-power idle */
	while (pulse_count == 0) {
		k_sleep(K_MSEC(100));
	}

	/* Woken by pulse! Restart the bin timer */
	#ifdef DEBUG
	printk("Woken from idle by pulse, resuming normal operation\n");
	#endif
	k_timer_start(&bin_timer, K_MINUTES(BIN_INTERVAL_MINUTES), K_MINUTES(BIN_INTERVAL_MINUTES));
	consecutive_zero_bins = 0;
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
 * Store a bin entry - handles trip logic, daily totals, and all-time counter
 */
static void store_bin(uint32_t pulses)
{
	if (!rtc_is_time_set()) {
		/* RTC not initialized yet: skip trip storage until time is valid. */
		return;
	}

	uint64_t ts = rtc_get_time();

	/* If no pulses, end the current trip */
	if (pulses == 0) {
		if (in_active_trip) {
#ifdef DEBUG
			printk("Trip ended (no pulses), saving to NVS slot %zu\n", nvm_trip_count);
#endif
			save_current_trip_to_nvs(true);  /* completed=true */
			in_active_trip = false;
		}

		consecutive_zero_bins++;
#ifdef DEBUG
		printk("Zero bins: %u/%d before sleep\n", consecutive_zero_bins, ZERO_BINS_BEFORE_SLEEP);
#endif

		if (consecutive_zero_bins >= ZERO_BINS_BEFORE_SLEEP) {
			enter_low_power_idle();
		}
		return;
	}

	/* We have pulses - reset zero counter */
	consecutive_zero_bins = 0;

	/* Start new trip if needed */
	if (!in_active_trip) {
		memset(&current_trip, 0, sizeof(current_trip));
		current_trip.start_timestamp_s = ts;
		current_trip.bucket_count = 0;
		in_active_trip = true;

#ifdef DEBUG
		printk("Started new trip (will be slot %zu, ts=%llu)\n",
		       nvm_trip_count, (unsigned long long)ts);
#endif
	}

	/* Check if current trip is full */
	if (current_trip.bucket_count >= BUCKETS_PER_TRIP) {
#ifdef DEBUG
		printk("Trip bucket limit reached, saving and starting new\n");
#endif
		save_current_trip_to_nvs(true);  /* completed=true */
		in_active_trip = false;
		store_bin(pulses);  /* Recursive call starts new trip */
		return;
	}

	/* Add bucket to current trip */
	current_trip.buckets[current_trip.bucket_count] = pulses;
	current_trip.bucket_count++;

#ifdef DEBUG
	uint32_t distance = pulses * WHEEL_CIRCUMFERENCE_MM(wheel_size_x100);
	printk("Bucket stored: bucket=%u pulses=%u distance_m=%.3f\n",
	       current_trip.bucket_count, pulses, distance / 1000.0);
#endif

	/* Backup current trip periodically (every 30 min = 6 bins) */
	if (current_trip.bucket_count % 6 == 0) {
		save_current_trip_to_nvs(false);  /* completed=false, just backup */
	}
}

/*
 * Save daily totals to NVM (called on day change)
 */
static void save_metadata_to_nvs(void)
{
	if (!nvs_ready) return;

	nvs_write(&nvs, NVS_ID_TRIP_COUNT, &nvm_trip_count, sizeof(nvm_trip_count));
	nvs_write(&nvs, NVS_ID_ACTIVE_FLAG, &in_active_trip, sizeof(in_active_trip));

#ifdef DEBUG
	printk("NVS: Metadata saved (trips=%zu)\n", nvm_trip_count);
#endif
}

/*
 * Save current trip to NVS
 * @param completed: true if trip ended, false if just backup
 */
static void save_current_trip_to_nvs(bool completed)
{
	if (!nvs_ready) return;

	if (completed) {
		/* Trip completed - save to next slot and increment count */
		if (nvm_trip_count < MAX_TRIPS) {
			uint16_t trip_id = NVS_ID_TRIP_BASE + nvm_trip_count;
			int rc = nvs_write(&nvs, trip_id, &current_trip, sizeof(current_trip));
			if (rc > 0) {
				nvm_trip_count++;
#ifdef DEBUG
				printk("NVS: Trip saved to slot %zu (id=%u, buckets=%u)\n",
				       nvm_trip_count - 1, trip_id, current_trip.bucket_count);
#endif
			}
		} else {
#ifdef DEBUG
			printk("NVS: Trip storage full (%zu trips)\n", MAX_TRIPS);
#endif
		}
	} else {
		/* Just backing up in-progress trip */
		uint16_t trip_id = NVS_ID_TRIP_BASE + nvm_trip_count;
		nvs_write(&nvs, trip_id, &current_trip, sizeof(current_trip));
#ifdef DEBUG
		printk("NVS: Trip backup to slot %zu (buckets=%u)\n",
		       nvm_trip_count, current_trip.bucket_count);
#endif
	}

	/* Always update metadata */
	save_metadata_to_nvs();
}

/*
 * Initialize NVS storage
 */
static int nvs_init_storage(void)
{
	struct flash_pages_info info;
	const struct device *flash_dev = NVS_PARTITION_DEVICE;

	if (!device_is_ready(flash_dev)) {
		printk("Flash device not ready\n");
		return -ENODEV;
	}

	nvs.flash_device = flash_dev;
	nvs.offset = NVS_PARTITION_OFFSET;

	int rc = flash_get_page_info_by_offs(flash_dev, nvs.offset, &info);
	if (rc) {
		printk("Unable to get flash page info (err %d)\n", rc);
		return rc;
	}
	nvs.sector_size = info.size;
	nvs.sector_count = NVS_PARTITION_SIZE / info.size;

	rc = nvs_mount(&nvs);
	if (rc) {
		printk("NVS mount failed (err %d)\n", rc);
		return rc;
	}

	nvs_ready = true;
	printk("NVS mounted: %u sectors of %u bytes\n", nvs.sector_count, nvs.sector_size);
	return 0;
}

/*
 * Public API
 */

int odometer_init(void)
{
	if (!gpio_is_ready_dt(&pulse_gpio)) {
		printk("Pulse GPIO device not ready\n");
		return -ENODEV;
	}

	int err = gpio_pin_configure_dt(&pulse_gpio, GPIO_INPUT);
	if (err) {
		printk("Failed to configure pulse pin (err %d)\n", err);
		return err;
	}

	gpio_init_callback(&pulse_cb_data, pulse_gpio_callback, BIT(pulse_gpio.pin));
	gpio_add_callback(pulse_gpio.port, &pulse_cb_data);
	gpio_pin_interrupt_configure_dt(&pulse_gpio, GPIO_INT_EDGE_TO_ACTIVE);
	printk("Pulse GPIO configured on P0.%02d\n", pulse_gpio.pin);

	k_work_init(&save_work, save_work_handler);

	k_timer_init(&bin_timer, bin_timer_handler, NULL);
	k_timer_start(&bin_timer, K_MINUTES(BIN_INTERVAL_MINUTES), K_MINUTES(BIN_INTERVAL_MINUTES));
	printk("Bin timer started (%d minute interval)\n", BIN_INTERVAL_MINUTES);

	return 0;
}

uint32_t odometer_get_pulse_count(void)
{
	uint32_t p;
	unsigned int key = irq_lock();
	p = pulse_count;
	irq_unlock(key);
	return p;
}

size_t odometer_get_trip_count(void)
{
	return nvm_trip_count + (in_active_trip ? 1 : 0);
}

uint32_t odometer_get_trip_id(size_t index)
{
	if (index >= odometer_get_trip_count()) {
		return 0;
	}

	return (uint32_t)(NVS_ID_TRIP_BASE + index);
}

int odometer_read_trip(size_t index, struct trip_entry *trip_out)
{
	if (!trip_out) {
		return -EINVAL;
	}
	if (!nvs_ready) {
		return -ENODEV;
	}

	size_t total = odometer_get_trip_count();
	if (index >= total) {
		return -ENOENT;
	}

	/* If requesting the current active trip */
	if (in_active_trip && index == total - 1) {
		memcpy(trip_out, &current_trip, sizeof(current_trip));
		return 0;
	}

	/* Read from NVS */
	uint16_t trip_id = NVS_ID_TRIP_BASE + index;
	ssize_t rc = nvs_read(&nvs, trip_id, trip_out, sizeof(struct trip_entry));
	if (rc != sizeof(struct trip_entry)) {
		return -EIO;
	}

	return 0;
}

const struct trip_entry *odometer_get_current_trip(void)
{
	return in_active_trip ? &current_trip : NULL;
}

bool odometer_is_trip_active(void)
{
	return in_active_trip;
}

void odometer_load_from_nvm(void)
{
	int rc = nvs_init_storage();
	if (rc) {
		printk("NVS init failed (err %d)\n", rc);
		return;
	}

	/* Load metadata */
	nvs_read(&nvs, NVS_ID_TRIP_COUNT, &nvm_trip_count, sizeof(nvm_trip_count));
	nvs_read(&nvs, NVS_ID_ACTIVE_FLAG, &in_active_trip, sizeof(in_active_trip));

	/* Load wheel size (use default if not stored) */
	uint32_t stored_wheel_size;
	if (nvs_read(&nvs, NVS_ID_WHEEL_SIZE, &stored_wheel_size, sizeof(stored_wheel_size)) > 0) {
		if (stored_wheel_size >= 1000 && stored_wheel_size <= 4000) {
			wheel_size_x100 = stored_wheel_size;
		}
	}

	/* Clamp values */
	if (nvm_trip_count > MAX_TRIPS) nvm_trip_count = MAX_TRIPS;

	/* Restore in-progress trip if active */
	if (in_active_trip) {
		uint16_t trip_id = NVS_ID_TRIP_BASE + nvm_trip_count;
		ssize_t r = nvs_read(&nvs, trip_id, &current_trip, sizeof(current_trip));
		if (r == sizeof(current_trip)) {
			printk("NVS: Restored in-progress trip (buckets=%u)\n", current_trip.bucket_count);
		} else {
			in_active_trip = false;
		}
	}

	printk("NVS loaded:\n");
	printk("  Trips in NVS: %zu (active=%d)\n", nvm_trip_count, in_active_trip);
	printk("  Wheel size: %u.%02u\"\n", wheel_size_x100 / 100, wheel_size_x100 % 100);
}

uint32_t odometer_get_wheel_size_x100(void)
{
	return wheel_size_x100;
}

void odometer_set_wheel_size_x100(uint32_t size_x100)
{
	if (size_x100 < 1000 || size_x100 > 4000) {
		return; /* Sanity check: 10" - 40" range */
	}
	wheel_size_x100 = size_x100;
	if (nvs_ready) {
		nvs_write(&nvs, NVS_ID_WHEEL_SIZE, &wheel_size_x100, sizeof(wheel_size_x100));
	}
}
