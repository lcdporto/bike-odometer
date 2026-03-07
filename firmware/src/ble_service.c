/*
 * BLE Service module - GATT service and advertising
 */

#include "ble_service.h"
#include "odometer.h"
#include "battery.h"
#include "rtc.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

/* Advertising work */
static struct k_work adv_work;

/*
 * BLE read handler for pulse count
 */
/*
 * BLE read handler for battery info
 */
static ssize_t read_battery(struct bt_conn *conn,
							const struct bt_gatt_attr *attr,
							void *buf, uint16_t len, uint16_t offset)
{
	uint16_t voltage_mv = battery_get_voltage_mv();
	uint8_t percent = battery_get_percent();

	char str[32];
	int n = snprintf(str, sizeof(str), "{\"mv\":%u,\"pct\":%u}", voltage_mv, percent);
	if (n < 0) {
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	return bt_gatt_attr_read(conn, attr, buf, len, offset, str, (size_t)n);
}

/*
 * BLE read handler for pulse count
 */
static ssize_t read_pulse(struct bt_conn *conn,
						  const struct bt_gatt_attr *attr,
						  void *buf, uint16_t len, uint16_t offset)
{
	uint32_t pulses = odometer_get_pulse_count();

	char str[16];
	int n = snprintf(str, sizeof(str), "%u", pulses);
	if (n < 0) {
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	return bt_gatt_attr_read(conn, attr, buf, len, offset, str, (size_t)n);
}

/*
 * BLE read handler for odometer (all-time total)
 */
static ssize_t read_odometer(struct bt_conn *conn,
						     const struct bt_gatt_attr *attr,
						     void *buf, uint16_t len, uint16_t offset)
{
	uint64_t total_pulses = odometer_get_total_pulses();
	uint32_t total_distance_m = odometer_get_total_distance_m();
	size_t daily_count = odometer_get_daily_total_count();

	char str[64];
	int n = snprintf(str, sizeof(str), "{\"pulses\":%llu,\"meters\":%u,\"days\":%zu}",
	                 (unsigned long long)total_pulses, total_distance_m, daily_count);
	if (n < 0) {
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	return bt_gatt_attr_read(conn, attr, buf, len, offset, str, (size_t)n);
}

/*
 * BLE read handler for trips JSON
 */
static ssize_t read_trips(struct bt_conn *conn,
						  const struct bt_gatt_attr *attr,
						  void *buf, uint16_t len, uint16_t offset)
{
	size_t trip_count = odometer_get_trip_count();
	struct trip_entry trip;

	static char out[4096];
	size_t pos = 0;
	int written = snprintf(out, sizeof(out), "{\"trips\":[");
	if (written < 0 || (size_t)written >= sizeof(out)) {
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
	pos = (size_t)written;

	for (size_t t = 0; t < trip_count; t++) {
		char trip_json[1024];
		size_t tpos = 0;

		if (odometer_read_trip(t, &trip) != 0) {
			continue;
		}

		written = snprintf(trip_json + tpos, sizeof(trip_json) - tpos,
					   "%s{\"startDate\":%llu,\"buckets\":[",
					   (t > 0) ? "," : "",
					   (unsigned long long)trip.start_timestamp_ms);
		if (written < 0 || (size_t)written >= (sizeof(trip_json) - tpos)) {
			break;
		}
		tpos += (size_t)written;

		for (uint16_t i = 0; i < trip.bucket_count; i++) {
			written = snprintf(trip_json + tpos, sizeof(trip_json) - tpos,
					   "%s%u", (i > 0) ? "," : "", trip.buckets[i]);
			if (written < 0 || (size_t)written >= (sizeof(trip_json) - tpos)) {
				goto done_trips;
			}
			tpos += (size_t)written;
		}

		written = snprintf(trip_json + tpos, sizeof(trip_json) - tpos, "]}");
		if (written < 0 || (size_t)written >= (sizeof(trip_json) - tpos)) {
			break;
		}
		tpos += (size_t)written;

		if (tpos >= (sizeof(out) - pos - 2)) {
			printk("BLE trips payload truncated at trip %zu/%zu\n", t, trip_count);
			break;
		}

		memcpy(out + pos, trip_json, tpos);
		pos += tpos;
	}

done_trips:
	written = snprintf(out + pos, sizeof(out) - pos, "]}");
	if (written < 0 || (size_t)written >= (sizeof(out) - pos)) {
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
	pos += (size_t)written;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, out, pos);
}

/* Service and characteristic UUIDs */
static struct bt_uuid_128 bike_svc_uuid = BT_UUID_INIT_128(
	0x78,0x56,0x34,0x12,0x34,0x12,0x78,0x56,0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78);

static struct bt_uuid_128 bike_pulse_uuid = BT_UUID_INIT_128(
	0x79,0x56,0x34,0x12,0x34,0x12,0x78,0x56,0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78);

static struct bt_uuid_128 bike_trips_uuid = BT_UUID_INIT_128(
	0x88,0x56,0x34,0x12,0x34,0x12,0x78,0x56,0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78);

static struct bt_uuid_128 bike_battery_uuid = BT_UUID_INIT_128(
	0x8A,0x56,0x34,0x12,0x34,0x12,0x78,0x56,0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78);

static struct bt_uuid_128 bike_time_uuid = BT_UUID_INIT_128(
	0x8B,0x56,0x34,0x12,0x34,0x12,0x78,0x56,0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78);

static struct bt_uuid_128 bike_odometer_uuid = BT_UUID_INIT_128(
	0x8C,0x56,0x34,0x12,0x34,0x12,0x78,0x56,0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78);

static struct bt_uuid_128 bike_wheelsize_uuid = BT_UUID_INIT_128(
	0x8D,0x56,0x34,0x12,0x34,0x12,0x78,0x56,0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78);

/* Characteristic Presentation Format: UTF-8 string */
static const struct bt_gatt_cpf trips_cpf = {
	.format = 0x19,
	.exponent = 0,
	.unit = 0x2700,
	.name_space = 0x01,
	.description = 0x0000,
};

static const struct bt_gatt_cpf pulse_cpf = {
	.format = 0x19,
	.exponent = 0,
	.unit = 0x2700,
	.name_space = 0x01,
	.description = 0x0000,
};

static const struct bt_gatt_cpf battery_cpf = {
	.format = 0x19,  /* UTF-8 string (JSON) */
	.exponent = 0,
	.unit = 0x2700,
	.name_space = 0x01,
	.description = 0x0000,
};

static const struct bt_gatt_cpf time_cpf = {
	.format = 0x08,  /* uint64 */
	.exponent = 0,
	.unit = 0x2703,  /* seconds */
	.name_space = 0x01,
	.description = 0x0000,
};

static const struct bt_gatt_cpf odometer_cpf = {
	.format = 0x19,  /* UTF-8 string (JSON) */
	.exponent = 0,
	.unit = 0x2700,
	.name_space = 0x01,
	.description = 0x0000,
};

static const struct bt_gatt_cpf wheelsize_cpf = {
	.format = 0x19,  /* UTF-8 string */
	.exponent = 0,
	.unit = 0x2712,  /* inch */
	.name_space = 0x01,
	.description = 0x0000,
};

/*
 * BLE read handler for wheel size (returns "26.30" format)
 */
static ssize_t read_wheelsize(struct bt_conn *conn,
							  const struct bt_gatt_attr *attr,
							  void *buf, uint16_t len, uint16_t offset)
{
	uint32_t size_x100 = odometer_get_wheel_size_x100();
	char str[16];
	int n = snprintf(str, sizeof(str), "%u.%02u", size_x100 / 100, size_x100 % 100);
	if (n < 0) {
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
	return bt_gatt_attr_read(conn, attr, buf, len, offset, str, (size_t)n);
}

/*
 * BLE write handler for wheel size (expects "26.30" format)
 */
static ssize_t write_wheelsize(struct bt_conn *conn,
							   const struct bt_gatt_attr *attr,
							   const void *buf, uint16_t len,
							   uint16_t offset, uint8_t flags)
{
	if (offset != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	char str[16];
	if (len >= sizeof(str)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	memcpy(str, buf, len);
	str[len] = '\0';

	/* Parse "26.30" format */
	char *dot = strchr(str, '.');
	uint32_t whole = (uint32_t)strtoul(str, NULL, 10);
	uint32_t frac = 0;
	if (dot && dot[1]) {
		frac = (uint32_t)strtoul(dot + 1, NULL, 10);
		/* Handle single digit fraction (e.g., "26.3" -> 30) */
		if (dot[2] == '\0') {
			frac *= 10;
		}
	}

	uint32_t size_x100 = whole * 100 + frac;
	if (size_x100 < 1000 || size_x100 > 4000) {
		/* Sanity check: 10" - 40" range */
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	odometer_set_wheel_size_x100(size_x100);
	printk("BLE: Wheel size set to %u.%02u\"\n", size_x100 / 100, size_x100 % 100);

	return len;
}

/*
 * BLE read handler for time
 */
static ssize_t read_time(struct bt_conn *conn,
						 const struct bt_gatt_attr *attr,
						 void *buf, uint16_t len, uint16_t offset)
{
	uint64_t unix_time = rtc_get_time();

	char str[32];
	int n = snprintf(str, sizeof(str), "%llu", (unsigned long long)unix_time);
	if (n < 0) {
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	return bt_gatt_attr_read(conn, attr, buf, len, offset, str, (size_t)n);
}

/*
 * BLE write handler for time
 */
static ssize_t write_time(struct bt_conn *conn,
						  const struct bt_gatt_attr *attr,
						  const void *buf, uint16_t len,
						  uint16_t offset, uint8_t flags)
{
	if (offset != 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	/* Parse unix timestamp from string */
	char str[32];
	if (len >= sizeof(str)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	memcpy(str, buf, len);
	str[len] = '\0';

	uint64_t unix_time = strtoull(str, NULL, 10);
	if (unix_time == 0) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	rtc_set_time(unix_time);
	printk("BLE: Time set to %llu\n", (unsigned long long)unix_time);

	return len;
}

/* GATT service definition */
BT_GATT_SERVICE_DEFINE(bike_svc,
	BT_GATT_PRIMARY_SERVICE(&bike_svc_uuid.uuid),
	BT_GATT_CHARACTERISTIC(&bike_pulse_uuid.uuid,
						   BT_GATT_CHRC_READ,
						   BT_GATT_PERM_READ,
						   read_pulse, NULL, NULL),
	BT_GATT_CUD("Pulse Count", BT_GATT_PERM_READ),
	BT_GATT_CPF(&pulse_cpf),
	BT_GATT_CHARACTERISTIC(&bike_trips_uuid.uuid,
						   BT_GATT_CHRC_READ,
						   BT_GATT_PERM_READ,
						   read_trips, NULL, NULL),
	BT_GATT_CUD("Trips", BT_GATT_PERM_READ),
	BT_GATT_CPF(&trips_cpf),
	BT_GATT_CHARACTERISTIC(&bike_battery_uuid.uuid,
						   BT_GATT_CHRC_READ,
						   BT_GATT_PERM_READ,
						   read_battery, NULL, NULL),
	BT_GATT_CUD("Battery", BT_GATT_PERM_READ),
	BT_GATT_CPF(&battery_cpf),
	BT_GATT_CHARACTERISTIC(&bike_time_uuid.uuid,
						   BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
						   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
						   read_time, write_time, NULL),
	BT_GATT_CUD("Unix Time", BT_GATT_PERM_READ),
	BT_GATT_CPF(&time_cpf),
	BT_GATT_CHARACTERISTIC(&bike_odometer_uuid.uuid,
						   BT_GATT_CHRC_READ,
						   BT_GATT_PERM_READ,
						   read_odometer, NULL, NULL),
	BT_GATT_CUD("Odometer", BT_GATT_PERM_READ),
	BT_GATT_CPF(&odometer_cpf),
	BT_GATT_CHARACTERISTIC(&bike_wheelsize_uuid.uuid,
						   BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
						   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
						   read_wheelsize, write_wheelsize, NULL),
	BT_GATT_CUD("Wheel Size", BT_GATT_PERM_READ),
	BT_GATT_CPF(&wheelsize_cpf),
);

/* Advertising data */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL,
		0x78,0x56,0x34,0x12,0x34,0x12,0x78,0x56,
		0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78),
};

static void adv_work_handler(struct k_work *work)
{
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

	if (err) {
		printk("Advertising with UUID failed (err %d), retrying without scan response\n", err);
		err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), NULL, 0);
	}

	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed, err 0x%02x %s\n", err, bt_hci_err_to_str(err));
		return;
	}

	printk("Connected\n");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected, reason 0x%02x %s\n", reason, bt_hci_err_to_str(reason));
}

static void recycled_cb(void)
{
	printk("Connection object available from previous conn. Disconnect is complete!\n");
	ble_service_start_advertising();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected        = connected,
	.disconnected     = disconnected,
	.recycled         = recycled_cb,
};

/*
 * Public API
 */

int ble_service_init(void)
{
	int err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return err;
	}

	printk("Bluetooth initialized\n");

	k_work_init(&adv_work, adv_work_handler);

	return 0;
}

void ble_service_start_advertising(void)
{
	k_work_submit(&adv_work);
}
