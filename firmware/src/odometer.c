/*
 * Odometer module - Pulse counting and NVM-direct trip storage
 *
 * Architecture:
 * - Only current (active) trip kept in RAM (~204 bytes)
 * - Completed trips stored directly to NVS
 * - Trips read from NVS on-demand
 * - Retains the newest 3000 trips in a power-fail-safe circular archive
 */

#include "odometer.h"
#include "ble_service.h"
#include "battery.h"
#include "rtc.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/atomic.h>
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

/* NVS IDs. Archive slots use 1000..4000; logical trip IDs are stored in each
 * record and are never reused when a physical slot is recycled.
 */
#define NVS_ID_ARCHIVE_METADATA   2
#define NVS_ID_WHEEL_SIZE         8
#define NVS_ID_ACTIVE_TRIP        10
#define NVS_ID_TRIP_BASE          1000
#define ARCHIVE_SLOT_COUNT        (MAX_TRIPS + 1U)
#define ARCHIVE_MAGIC             0x424F444FU
#define ARCHIVE_VERSION           1U
#define FIRST_TRIP_ID             1000U

struct archived_trip {
	uint32_t trip_id;
	struct trip_entry trip;
};

struct archive_metadata {
	uint32_t magic;
	uint32_t version;
	uint32_t count;
	uint32_t oldest_slot;
	uint32_t next_trip_id;
};

static struct nvs_fs nvs;
static bool nvs_ready;

/* Module state */
static volatile uint32_t pulse_count;
static struct gpio_callback pulse_cb_data;
static struct k_timer bin_timer;
static struct k_work save_work;
static struct k_work movement_work;
static atomic_t parked;
static atomic_t movement_window_armed = ATOMIC_INIT(1);

/* Current trip - only one trip in RAM at a time */
static struct trip_entry current_trip;
static bool in_active_trip;
static uint32_t current_trip_id;
static struct archive_metadata archive = {
	.magic = ARCHIVE_MAGIC,
	.version = ARCHIVE_VERSION,
	.next_trip_id = FIRST_TRIP_ID,
};

/* Wheel size (hundredths of inches, e.g., 2600 = 26.00") */
static uint32_t wheel_size_x100 = DEFAULT_WHEEL_SIZE_X100;

/* Pending bin data */
static uint32_t pending_pulses;
static bool pending_save;

#define ZERO_BINS_BEFORE_SLEEP 2
#define MOVEMENT_ADVERTISING_SECONDS 60U
static uint32_t consecutive_zero_bins;

/* Forward declarations */
static void pulse_gpio_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
static void bin_timer_handler(struct k_timer *timer);
static void save_work_handler(struct k_work *work);
static void movement_work_handler(struct k_work *work);
static void store_bin(uint32_t pulses);
static int save_current_trip_to_nvs(bool completed);
static int save_archive_metadata(const struct archive_metadata *metadata);
static int read_archive_slot(uint32_t slot, struct archived_trip *record);
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

	/* Open one BLE window on the first movement after boot or a quiet bin.
	 * This is deliberately independent of parked: an active trip restored
	 * from NVS, or movement resuming before the second zero bin, must still
	 * be discoverable. Continued wheel pulses do not extend the window.
	 */
	if (atomic_cas(&movement_window_armed, 1, 0)) {
		k_work_submit(&movement_work);
	}
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
	unsigned int key;
	bool movement_already_pending;

	#ifdef DEBUG
	printk("Entering low power idle... GRTC keeps running, P0.%02d will wake\n", pulse_gpio.pin);
	#endif

	/* Mark parked before stopping the timer. A Hall edge that arrives during
	 * this transition will then queue movement_work on the same work queue,
	 * after this handler has finished.
	 */
	key = irq_lock();
	atomic_set(&parked, 1);
	k_timer_stop(&bin_timer);
	movement_already_pending = pulse_count != 0U;
	if (movement_already_pending) {
		atomic_set(&parked, 0);
		atomic_set(&movement_window_armed, 0);
	}
	irq_unlock(key);

	/* Close the transition race: a pulse may have arrived after the empty
	 * bin was captured but before parked was set.
	 */
	if (movement_already_pending) {
		k_work_submit(&movement_work);
		return;
	}

	/* Keep System ON so GRTC and k_uptime_get() continue advancing. With no
	 * advertising and no periodic timer, Zephyr can remain in idle until the
	 * Hall GPIO interrupt submits movement_work.
	 */
	if (!ble_service_is_connected()) {
		ble_service_stop_advertising();
	}
}

static void movement_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	consecutive_zero_bins = 0;
	if (atomic_cas(&parked, 1, 0)) {
		k_timer_start(&bin_timer,
			      K_MINUTES(BIN_INTERVAL_MINUTES),
			      K_MINUTES(BIN_INTERVAL_MINUTES));
	}

	/* Refresh the resting-voltage estimate after a parked interval. This
	 * keeps the value read by the phone representative of the current ride,
	 * rather than retaining the measurement taken at the previous boot.
	 */
	if (battery_measure() != 0) {
		printk("Movement wake: battery measurement failed\n");
	}

	ble_service_start_advertising_for(MOVEMENT_ADVERTISING_SECONDS);

#ifdef DEBUG
	printk("Movement wake: binning resumed and BLE window opened\n");
#endif
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
		/* The next Hall edge represents movement resuming after quiet and
		 * should open a fresh, bounded BLE synchronization window.
		 */
		atomic_set(&movement_window_armed, 1);

		if (in_active_trip) {
#ifdef DEBUG
			printk("Trip ended (no pulses), saving trip %u\n", current_trip_id);
#endif
			if (save_current_trip_to_nvs(true) == 0) {
				in_active_trip = false;
				current_trip_id = 0;
			} else {
				printk("NVS: Trip %u retained in RAM after save failure\n",
				       current_trip_id);
			}
		}

		consecutive_zero_bins++;
#ifdef DEBUG
		printk("Zero bins: %u/%d before sleep\n", consecutive_zero_bins, ZERO_BINS_BEFORE_SLEEP);
#endif

		/* Keep retrying if the completed trip is still only in RAM. */
		if (consecutive_zero_bins >= ZERO_BINS_BEFORE_SLEEP &&
		    !in_active_trip) {
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
		current_trip_id = archive.next_trip_id;
		in_active_trip = true;

#ifdef DEBUG
		printk("Started new trip (id=%u, ts=%llu)\n",
		       current_trip_id, (unsigned long long)ts);
#endif
	}

	/* Check if current trip is full */
	if (current_trip.bucket_count >= BUCKETS_PER_TRIP) {
#ifdef DEBUG
		printk("Trip bucket limit reached, saving and starting new\n");
#endif
		if (save_current_trip_to_nvs(true) == 0) {
			in_active_trip = false;
			current_trip_id = 0;
			store_bin(pulses);  /* Recursive call starts new trip */
		} else {
			printk("NVS: Full trip %u retained in RAM after save failure\n",
			       current_trip_id);
		}
		return;
	}

	/* Add bucket to current trip */
	current_trip.buckets[current_trip.bucket_count] = pulses;
	current_trip.bucket_count++;

#ifdef DEBUG
	uint32_t distance = pulses * WHEEL_CIRCUMFERENCE_MM(wheel_size_x100);
	printk("Bucket stored: bucket=%u pulses=%u\n",
	       current_trip.bucket_count, pulses);
#endif

	/* Backup current trip periodically (every 30 min = 6 bins) */
	if (current_trip.bucket_count % 6 == 0) {
		int err = save_current_trip_to_nvs(false);
		if (err) {
			printk("NVS: Active trip backup failed (err %d)\n", err);
		}
	}
}

static int save_archive_metadata(const struct archive_metadata *metadata)
{
	int rc = nvs_write(&nvs, NVS_ID_ARCHIVE_METADATA,
			   metadata, sizeof(*metadata));

	if (rc != sizeof(*metadata) && rc != 0) {
		printk("NVS: Archive metadata write failed (err %d)\n", rc);
		return rc < 0 ? rc : -EIO;
	}

	return 0;
}

static int read_archive_slot(uint32_t slot, struct archived_trip *record)
{
	uint16_t nvs_id;
	ssize_t rc;

	if (slot >= ARCHIVE_SLOT_COUNT || record == NULL) {
		return -EINVAL;
	}

	nvs_id = (uint16_t)(NVS_ID_TRIP_BASE + slot);
	rc = nvs_read(&nvs, nvs_id, record, sizeof(*record));
	if (rc == sizeof(*record)) {
		return 0;
	}

	return rc < 0 ? (int)rc : -EIO;
}

/*
 * Save the active trip. Completed trips are written to the spare ring slot
 * first, then archive metadata is committed. At capacity, this makes the
 * previous oldest slot the new spare without reusing its public trip ID.
 */
static int save_current_trip_to_nvs(bool completed)
{
	struct archived_trip record = {
		.trip_id = current_trip_id,
		.trip = current_trip,
	};
	int rc;

	if (!nvs_ready) {
		return -ENODEV;
	}

	if (!completed) {
		rc = nvs_write(&nvs, NVS_ID_ACTIVE_TRIP, &record, sizeof(record));
		if (rc != sizeof(record) && rc != 0) {
			return rc < 0 ? rc : -EIO;
		}
		return 0;
	}

	uint32_t target_slot = (archive.oldest_slot + archive.count) %
			       ARCHIVE_SLOT_COUNT;
	uint16_t target_id = (uint16_t)(NVS_ID_TRIP_BASE + target_slot);

	rc = nvs_write(&nvs, target_id, &record, sizeof(record));
	if (rc != sizeof(record) && rc != 0) {
		printk("NVS: Trip %u write failed (err %d)\n", current_trip_id, rc);
		return rc < 0 ? rc : -EIO;
	}

	struct archive_metadata updated = archive;
	updated.next_trip_id++;
	if (updated.count < MAX_TRIPS) {
		updated.count++;
	} else {
		updated.oldest_slot = (updated.oldest_slot + 1U) %
				      ARCHIVE_SLOT_COUNT;
	}

	rc = save_archive_metadata(&updated);
	if (rc) {
		/* Metadata still points to the previous archive. Because target_slot
		 * was the spare slot, all previously committed trips remain valid.
		 */
		return rc;
	}

	archive = updated;
	rc = nvs_delete(&nvs, NVS_ID_ACTIVE_TRIP);
	if (rc && rc != -ENOENT) {
		printk("NVS: Active-trip cleanup failed (err %d)\n", rc);
	}

#ifdef DEBUG
	printk("NVS: Trip %u committed to slot %u (count=%u, oldest=%u)\n",
	       record.trip_id, target_slot, archive.count, archive.oldest_slot);
#endif
	return 0;
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
	err = gpio_add_callback(pulse_gpio.port, &pulse_cb_data);
	if (err) {
		printk("Failed to add pulse GPIO callback (err %d)\n", err);
		return err;
	}

	err = gpio_pin_interrupt_configure_dt(&pulse_gpio,
					      GPIO_INT_EDGE_TO_ACTIVE);
	if (err) {
		printk("Failed to configure pulse GPIO interrupt (err %d)\n", err);
		gpio_remove_callback(pulse_gpio.port, &pulse_cb_data);
		return err;
	}
	printk("Pulse GPIO configured on P0.%02d\n", pulse_gpio.pin);

	k_work_init(&save_work, save_work_handler);
	k_work_init(&movement_work, movement_work_handler);

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
	return archive.count + (in_active_trip ? 1U : 0U);
}

uint32_t odometer_get_trip_id(size_t index)
{
	size_t completed_count = archive.count;

	if (index >= completed_count + (in_active_trip ? 1U : 0U)) {
		return 0;
	}

	if (in_active_trip && index == completed_count) {
		return current_trip_id;
	}

	/* IDs are allocated consecutively and never reused. */
	return archive.next_trip_id - archive.count + (uint32_t)index;
}

int odometer_read_trip(size_t index, struct trip_entry *trip_out)
{
	if (!trip_out) {
		return -EINVAL;
	}
	if (!nvs_ready) {
		return -ENODEV;
	}

	size_t completed_count = archive.count;
	size_t total = completed_count + (in_active_trip ? 1U : 0U);
	if (index >= total) {
		return -ENOENT;
	}

	/* If requesting the current active trip */
	if (in_active_trip && index == completed_count) {
		memcpy(trip_out, &current_trip, sizeof(current_trip));
		return 0;
	}

	uint32_t slot = (archive.oldest_slot + (uint32_t)index) %
			ARCHIVE_SLOT_COUNT;
	struct archived_trip record;
	int rc = read_archive_slot(slot, &record);
	if (rc) {
		return rc;
	}

	uint32_t expected_id = archive.next_trip_id - archive.count +
			       (uint32_t)index;
	if (record.trip_id != expected_id) {
		printk("NVS: Trip ID mismatch in slot %u (expected %u, found %u)\n",
		       slot, expected_id, record.trip_id);
		return -EIO;
	}

	memcpy(trip_out, &record.trip, sizeof(*trip_out));
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
	struct archive_metadata stored_archive;
	struct archived_trip active_record;
	bool archive_valid = false;
	int rc = nvs_init_storage();
	if (rc) {
		printk("NVS init failed (err %d)\n", rc);
		return;
	}

	rc = nvs_read(&nvs, NVS_ID_ARCHIVE_METADATA,
		      &stored_archive, sizeof(stored_archive));
	if (rc == sizeof(stored_archive) &&
	    stored_archive.magic == ARCHIVE_MAGIC &&
	    stored_archive.version == ARCHIVE_VERSION &&
	    stored_archive.count <= MAX_TRIPS &&
	    stored_archive.oldest_slot < ARCHIVE_SLOT_COUNT &&
	    stored_archive.next_trip_id >= FIRST_TRIP_ID + stored_archive.count) {
		archive = stored_archive;
		archive_valid = true;
	} else {
		archive = (struct archive_metadata) {
			.magic = ARCHIVE_MAGIC,
			.version = ARCHIVE_VERSION,
			.count = 0,
			.oldest_slot = 0,
			.next_trip_id = FIRST_TRIP_ID,
		};
		rc = save_archive_metadata(&archive);
		if (rc) {
			printk("NVS: Archive initialization failed (err %d)\n", rc);
		}
	}

	/* Load wheel size (use default if not stored) */
	uint32_t stored_wheel_size;
	if (nvs_read(&nvs, NVS_ID_WHEEL_SIZE, &stored_wheel_size, sizeof(stored_wheel_size)) > 0) {
		if (stored_wheel_size >= 1000 && stored_wheel_size <= 4000) {
			wheel_size_x100 = stored_wheel_size;
		}
	}

	/* Restore a current-format active-trip backup. */
	rc = nvs_read(&nvs, NVS_ID_ACTIVE_TRIP,
		      &active_record, sizeof(active_record));
	if (archive_valid && rc == sizeof(active_record) &&
	    active_record.trip_id == archive.next_trip_id) {
		current_trip_id = active_record.trip_id;
		current_trip = active_record.trip;
		in_active_trip = true;
	}

	printk("NVS loaded:\n");
	printk("  Trips in NVS: %u (active=%d, oldest_slot=%u, next_id=%u)\n",
	       archive.count, in_active_trip, archive.oldest_slot,
	       archive.next_trip_id);
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
