#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <soc.h>
#include <zephyr/irq.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/settings/settings.h>

#include <zephyr/sys/util.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* Use GPIO directly for LEDs instead of DK library */
#define LED_GPIO_NODE DT_NODELABEL(gpio0)

#ifndef RUN_STATUS_PIN
#define RUN_STATUS_PIN 13
#endif

#ifndef CON_STATUS_PIN
#define CON_STATUS_PIN 14
#endif

static const struct device *led_dev;

/*
 * DEBUG NOTE:
 * This file contains debug helpers (printk logging and simple bin dumps)
 * used during development. Define `DEBUG_ODOMETER` to enable extra debug
 * prints. Remove or disable these before production as desired.
 */
#define DEBUG_ODOMETER 1

#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

#define RUN_STATUS_LED          RUN_STATUS_PIN
#define CON_STATUS_LED		CON_STATUS_PIN
#define RUN_LED_BLINK_INTERVAL	1000	

static struct k_work adv_work;

/* --- Pulse counting and 5-minute binning configuration --- */
/* Change these to match your board's GPIO controller and pin */
#define PULSE_GPIO_NODE DT_NODELABEL(gpio0)

#ifndef PULSE_PIN
#define PULSE_PIN 11
#endif

/* Wheel circumference in millimeters (default 2100mm = 2.1m). Adjust to your wheel. */
#ifndef WHEEL_CIRCUMFERENCE_MM
#define WHEEL_CIRCUMFERENCE_MM 2100U
#endif

#define BIN_INTERVAL_MINUTES 5
#define BIN_HISTORY 12 /* keep last 12 bins (1 hour) */

static volatile uint32_t pulse_count;
static struct gpio_callback pulse_cb_data;
static struct k_timer bin_timer;

struct bin_entry {
	uint32_t pulses;
	uint32_t distance_mm;
	uint64_t timestamp_ms; /* k_uptime_get() at bin time */
};

static struct bin_entry bins[BIN_HISTORY];
static size_t bin_index;

static void pulse_gpio_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	unsigned int key = irq_lock();
	pulse_count++;
#ifdef DEBUG_ODOMETER
	printk("DEBUG_ODOMETER: pulse detected pins=0x%08x total=%u\n", pins, pulse_count);
#endif
	irq_unlock(key);
}

static void store_bin(uint32_t pulses)
{
	uint32_t distance = pulses * WHEEL_CIRCUMFERENCE_MM;
	uint64_t ts = k_uptime_get();

	bins[bin_index].pulses = pulses;
	bins[bin_index].distance_mm = distance;
	bins[bin_index].timestamp_ms = ts;

	/* persist this bin into settings under key "odom/bin/N" */
#if IS_ENABLED(CONFIG_SETTINGS)
	struct {
		uint64_t timestamp_ms;
		uint32_t pulses;
		uint32_t distance_mm;
	} persist = { .timestamp_ms = ts, .pulses = pulses, .distance_mm = distance };

	char key[32];
	snprintf(key, sizeof(key), "odom/bin/%zu", bin_index);
	int serr = settings_save_one(key, &persist, sizeof(persist));
	if (serr) {
		printk("settings_save_one failed (%d)\n", serr);
	}
#endif

	printk("Bin stored: pulses=%u distance_m=%.3f ts_ms=%llu\n",
		   pulses, distance / 1000.0, (unsigned long long)ts);

	bin_index = (bin_index + 1) % BIN_HISTORY;
}

#if IS_ENABLED(CONFIG_SETTINGS)
static int odom_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	if (!name) {
		return -ENOENT;
	}

	/* expected name: "bin/<index>" */
	if (strncmp(name, "bin/", 4) != 0) {
		return -ENOENT;
	}

	int idx = atoi(name + 4);
	if (idx < 0 || idx >= (int)BIN_HISTORY) {
		return -EINVAL;
	}

	struct {
		uint64_t timestamp_ms;
		uint32_t pulses;
		uint32_t distance_mm;
	} persist;

	if (len != sizeof(persist)) {
		return -EINVAL;
	}

	ssize_t rc = read_cb(cb_arg, &persist, sizeof(persist));
	if (rc < 0) {
		return rc;
	}

	bins[idx].pulses = persist.pulses;
	bins[idx].distance_mm = persist.distance_mm;
	bins[idx].timestamp_ms = persist.timestamp_ms;

	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(odom, "odom", NULL, odom_set, NULL, 0);
#endif

static void bin_timer_handler(struct k_timer *timer)
{
	uint32_t pulses;
	unsigned int key = irq_lock();
	pulses = pulse_count;
	pulse_count = 0;
	irq_unlock(key);

	store_bin(pulses);
}

static ssize_t read_pulse(struct bt_conn *conn,
						 const struct bt_gatt_attr *attr,
						 void *buf, uint16_t len, uint16_t offset)
{
	uint32_t pulses;
	unsigned int key = irq_lock();
	pulses = pulse_count;
	irq_unlock(key);

	/* Return pulses as ASCII text (UTF-8) so central sees a text value */
	char str[16];
	int n = snprintf(str, sizeof(str), "%u", pulses);
	if (n < 0) {
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	return bt_gatt_attr_read(conn, attr, buf, len, offset, str, (size_t)n);
}

static ssize_t read_trips(struct bt_conn *conn,
						  const struct bt_gatt_attr *attr,
						  void *buf, uint16_t len, uint16_t offset)
{
	/* Build a minimal JSON payload with one trip using stored bins.
	 * Format: {"trips":[{"id":1,"startDate":<ms>,"buckets":[n,n,...]}]}
	 */
	char out[1024];
	size_t pos = 0;

	pos += snprintf(out + pos, sizeof(out) - pos, "{\"trips\":[{\"id\":1,\"startDate\":");

	/* find oldest timestamp among stored bins (non-zero) */
	uint64_t start_ts = 0;
	for (size_t i = 0; i < BIN_HISTORY; i++) {
		if (bins[i].timestamp_ms != 0) {
			if (start_ts == 0 || bins[i].timestamp_ms < start_ts) {
				start_ts = bins[i].timestamp_ms;
			}
		}
	}

	pos += snprintf(out + pos, sizeof(out) - pos, "%llu,\"buckets\":[",
					(unsigned long long)start_ts);

	/* append buckets in chronological order (oldest first) */
	bool first = true;
	for (size_t i = 0; i < BIN_HISTORY; i++) {
		size_t idx = (bin_index + i) % BIN_HISTORY; /* start from oldest relative to current index */
		uint32_t pulses = bins[idx].pulses;
		if (bins[idx].timestamp_ms == 0) {
			/* skip empty entries */
			continue;
		}
		if (!first) {
			pos += snprintf(out + pos, sizeof(out) - pos, ",");
		}
		first = false;
		pos += snprintf(out + pos, sizeof(out) - pos, "%u", pulses);
	}

	pos += snprintf(out + pos, sizeof(out) - pos, "]}]}\n");

	return bt_gatt_attr_read(conn, attr, buf, len, offset, out, strlen(out));
}

/* Simple custom service exposing pulse count as a readable 32-bit little-endian field */
static struct bt_uuid_128 bike_svc_uuid = BT_UUID_INIT_128(
	0x78,0x56,0x34,0x12,0x34,0x12,0x78,0x56,0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78);

static struct bt_uuid_128 bike_pulse_uuid = BT_UUID_INIT_128(
	0x79,0x56,0x34,0x12,0x34,0x12,0x78,0x56,0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78);

BT_GATT_SERVICE_DEFINE(bike_svc,
	BT_GATT_PRIMARY_SERVICE(&bike_svc_uuid.uuid),
	BT_GATT_CHARACTERISTIC(&bike_pulse_uuid.uuid,
						   BT_GATT_CHRC_READ,
						   BT_GATT_PERM_READ,
						   read_pulse, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(0x88,0x56,0x34,0x12,0x34,0x12,0x78,0x56,0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78),
						   BT_GATT_CHRC_READ,
						   BT_GATT_PERM_READ,
						   read_trips, NULL, NULL),
);


static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {};

static void adv_work_handler(struct k_work *work)
{
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
}

static void advertising_start(void)
{
	k_work_submit(&adv_work);
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed, err 0x%02x %s\n", err, bt_hci_err_to_str(err));
		return;
	}

	printk("Connected\n");

	if (led_dev) {
		int r = gpio_pin_set(led_dev, CON_STATUS_PIN, 1);
		if (r) {
			printk("gpio_pin_set(CON) failed: %d\n", r);
		}
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected, reason 0x%02x %s\n", reason, bt_hci_err_to_str(reason));

	if (led_dev) {
		int r = gpio_pin_set(led_dev, CON_STATUS_PIN, 0);
		if (r) {
			printk("gpio_pin_set(CON) failed: %d\n", r);
		}
	}
}

static void recycled_cb(void)
{
	printk("Connection object available from previous conn. Disconnect is complete!\n");
	advertising_start();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected        = connected,
	.disconnected     = disconnected,
	.recycled         = recycled_cb,
};

int main(void)
{
	int blink_status = 0;
	int err;

	printk("Starting Bluetooth Peripheral LBS sample\n");

	/* Initialize LED GPIOs */
	led_dev = DEVICE_DT_GET(LED_GPIO_NODE);
	if (!device_is_ready(led_dev)) {
		printk("LED GPIO device not ready\n");
		led_dev = NULL;
	} else {
		printk("LED GPIO device ready\n");
		/* nRF DK LEDs are active-low; configure with GPIO_ACTIVE_LOW so
		 * logical '1' means LED off and '0' means LED on. Keep initial
		 * state inactive (off).
		 */
		err = gpio_pin_configure(led_dev, RUN_STATUS_PIN, GPIO_OUTPUT_INACTIVE | GPIO_ACTIVE_LOW);
		if (err) {
			printk("Failed to configure RUN LED pin %d (err %d)\n", RUN_STATUS_PIN, err);
			return 0;
		} else {
			printk("Configured RUN LED pin %d\n", RUN_STATUS_PIN);
		}
		err = gpio_pin_configure(led_dev, CON_STATUS_PIN, GPIO_OUTPUT_INACTIVE | GPIO_ACTIVE_LOW);
		if (err) {
			printk("Failed to configure CON LED pin %d (err %d)\n", CON_STATUS_PIN, err);
			return 0;
		} else {
			printk("Configured CON LED pin %d\n", CON_STATUS_PIN);
		}
	}

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	/* --- Initialize pulse GPIO and 5-minute bin timer --- */
	{
		const struct device *gpio_dev = DEVICE_DT_GET(PULSE_GPIO_NODE);

		if (!device_is_ready(gpio_dev)) {
			printk("Pulse GPIO device not ready\n");
		} else {
			err = gpio_pin_configure(gpio_dev, PULSE_PIN, GPIO_INPUT | GPIO_PULL_UP);
			if (err) {
				printk("Failed to configure pulse pin %d (err %d)\n", PULSE_PIN, err);
			} else {
				gpio_init_callback(&pulse_cb_data, pulse_gpio_callback, (1U << PULSE_PIN));
				gpio_add_callback(gpio_dev, &pulse_cb_data);
				gpio_pin_interrupt_configure(gpio_dev, PULSE_PIN, GPIO_INT_EDGE_TO_ACTIVE);
				printk("Pulse GPIO configured on gpio0 pin %d\n", PULSE_PIN);
			}
		}

		k_timer_init(&bin_timer, bin_timer_handler, NULL);
		k_timer_start(&bin_timer, K_MINUTES(BIN_INTERVAL_MINUTES), K_MINUTES(BIN_INTERVAL_MINUTES));


	}

	k_work_init(&adv_work, adv_work_handler);
	advertising_start();

	for (;;) {
		if (led_dev) {
			int r = gpio_pin_set(led_dev, RUN_STATUS_PIN, (++blink_status) % 2);
			if (r) {
				printk("gpio_pin_set(RUN) failed: %d\n", r);
			}
			if ((blink_status & 0x7) == 0) {
				printk("blink_status=%d\n", blink_status);
			}
		}
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));
	}
}
