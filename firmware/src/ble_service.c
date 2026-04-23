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
#include <zephyr/bluetooth/att.h>
#include <zephyr/sys/util.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_SUFFIX_LEN  5
#define DEVICE_NAME_MAX_LEN     (sizeof(DEVICE_NAME) - 1 + DEVICE_NAME_SUFFIX_LEN)
#define TRIPS_NOTIFY_STACK_SIZE 2048

/* Advertising work */
static struct k_work adv_work;
static struct bt_conn *current_conn;
static struct k_work_q trips_notify_work_q;
static struct k_work trips_notify_work;
K_THREAD_STACK_DEFINE(trips_notify_stack, TRIPS_NOTIFY_STACK_SIZE);
static char device_name[DEVICE_NAME_MAX_LEN + 1];

static void update_device_name(void)
{
	bt_addr_le_t addr;
	size_t count = 1;
	size_t base_len = sizeof(DEVICE_NAME) - 1;
	int written;

	memcpy(device_name, DEVICE_NAME, base_len);
	device_name[base_len] = '\0';

	bt_id_get(&addr, &count);
	if (count == 0U) {
		printk("BLE: Using base device name '%s' (no identity address)\n", device_name);
		return;
	}

	written = snprintk(device_name, sizeof(device_name), "%s-%02X%02X",
				 DEVICE_NAME,
				 addr.a.val[1],
				 addr.a.val[0]);
	if (written < 0 || written >= (int)sizeof(device_name)) {
		memcpy(device_name, DEVICE_NAME, base_len);
		device_name[base_len] = '\0';
		printk("BLE: Failed to append address suffix, using base name '%s'\n", device_name);
		return;
	}

	printk("BLE: Advertising as '%s'\n", device_name);
}

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
 * BLE read handler for trips JSON
 */
static ssize_t read_trips(struct bt_conn *conn,
						  const struct bt_gatt_attr *attr,
						  void *buf, uint16_t len, uint16_t offset)
{
	size_t trip_count = odometer_get_trip_count();
	struct trip_entry trip;

	static char out[BT_ATT_MAX_ATTRIBUTE_LEN];
	size_t pos = 0;
	int written = snprintf(out, sizeof(out), "{\"trips\":[");
	if (written < 0 || (size_t)written >= sizeof(out)) {
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}
	pos = (size_t)written;

	for (size_t t = 0; t < trip_count; t++) {
		char trip_json[1024];
		size_t tpos = 0;
		uint32_t trip_id;

		if (odometer_read_trip(t, &trip) != 0) {
			continue;
		}

		trip_id = odometer_get_trip_id(t);

		written = snprintf(trip_json + tpos, sizeof(trip_json) - tpos,
					   "%s{\"id\":%u,\"startDate\":%llu,\"buckets\":[",
					   (t > 0) ? "," : "",
					   trip_id,
					   (unsigned long long)trip.start_timestamp_s);
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
			printk("BLE trips payload truncated at trip %zu/%zu (%u-byte ATT limit)\n",
			       t, trip_count, BT_ATT_MAX_ATTRIBUTE_LEN);
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

static struct bt_uuid_128 bike_wheelsize_uuid = BT_UUID_INIT_128(
	0x8D,0x56,0x34,0x12,0x34,0x12,0x78,0x56,0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78);

enum trips_stream_phase {
	TRIPS_STREAM_START,
	TRIPS_STREAM_TRIP_START,
	TRIPS_STREAM_BUCKET,
	TRIPS_STREAM_TRIP_END,
	TRIPS_STREAM_END,
	TRIPS_STREAM_DONE,
};

struct trips_stream_state {
	bool notify_enabled;
	bool active;
	bool notify_in_progress;
	bool sent_any_trip;
	enum trips_stream_phase phase;
	size_t phase_offset;
	size_t trip_count;
	size_t trip_index;
	uint32_t trip_id;
	uint16_t bucket_index;
	struct trip_entry trip;
	struct bt_gatt_notify_params notify_params;
	char notify_buf[CONFIG_BT_L2CAP_TX_MTU];
};

static struct trips_stream_state trips_stream;

static void trips_notify_work_handler(struct k_work *work);
static void trips_notify_complete(struct bt_conn *conn, void *user_data);
static void trips_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value);
static ssize_t write_trips(struct bt_conn *conn,
					   const struct bt_gatt_attr *attr,
					   const void *buf, uint16_t len,
					   uint16_t offset, uint8_t flags);

static void trips_stream_reset(void)
{
	trips_stream.active = false;
	trips_stream.sent_any_trip = false;
	trips_stream.notify_in_progress = false;
	trips_stream.phase = TRIPS_STREAM_DONE;
	trips_stream.phase_offset = 0;
	trips_stream.trip_count = 0;
	trips_stream.trip_index = 0;
	trips_stream.trip_id = 0;
	trips_stream.bucket_index = 0;
}

static size_t trips_stream_append(char *dst, size_t dst_len, size_t dst_pos,
					  const char *src, size_t src_len,
					  size_t *src_offset)
{
	size_t remaining = src_len - *src_offset;
	size_t space = dst_len - dst_pos;
	size_t copy_len = MIN(remaining, space);

	if (copy_len == 0U) {
		return 0U;
	}

	memcpy(dst + dst_pos, src + *src_offset, copy_len);
	*src_offset += copy_len;

	return copy_len;
}

static void trips_stream_advance_to_next_trip(void)
{
	while (trips_stream.trip_index < trips_stream.trip_count) {
		if (odometer_read_trip(trips_stream.trip_index, &trips_stream.trip) == 0) {
			trips_stream.trip_id = odometer_get_trip_id(trips_stream.trip_index);
			trips_stream.phase = TRIPS_STREAM_TRIP_START;
			trips_stream.phase_offset = 0;
			trips_stream.bucket_index = 0;
			trips_stream.trip_index++;
			return;
		}

		printk("BLE: Failed to read trip %zu\n", trips_stream.trip_index);
		trips_stream.trip_index++;
	}

	trips_stream.phase = TRIPS_STREAM_END;
	trips_stream.phase_offset = 0;
}

static size_t trips_stream_build_chunk(char *chunk, size_t chunk_size)
{
	static const char stream_start[] = "{\"trips\":[";
	static const char stream_end[] = "]}";
	static const char trip_end[] = "]}";
	size_t len = 0;

	while (len < chunk_size && trips_stream.phase != TRIPS_STREAM_DONE) {
		char part[112];
		size_t part_len = 0;
		size_t copied;
		int written;

		switch (trips_stream.phase) {
		case TRIPS_STREAM_START:
			copied = trips_stream_append(chunk, chunk_size, len,
						    stream_start, sizeof(stream_start) - 1,
						    &trips_stream.phase_offset);
			len += copied;
			if (trips_stream.phase_offset == sizeof(stream_start) - 1) {
				trips_stream.phase_offset = 0;
				trips_stream_advance_to_next_trip();
			}
			break;

		case TRIPS_STREAM_TRIP_START:
			written = snprintf(part, sizeof(part), "%s{\"id\":%u,\"startDate\":%llu,\"buckets\":[",
					   trips_stream.sent_any_trip ? "," : "",
					   trips_stream.trip_id,
					   (unsigned long long)trips_stream.trip.start_timestamp_s);
			if (written < 0 || (size_t)written >= sizeof(part)) {
				printk("BLE: Failed to format trip header\n");
				trips_stream_reset();
				return 0;
			}

			part_len = (size_t)written;
			copied = trips_stream_append(chunk, chunk_size, len,
						    part, part_len,
						    &trips_stream.phase_offset);
			len += copied;
			if (trips_stream.phase_offset == part_len) {
				trips_stream.phase_offset = 0;
				trips_stream.sent_any_trip = true;
				trips_stream.phase = trips_stream.trip.bucket_count > 0 ?
					TRIPS_STREAM_BUCKET : TRIPS_STREAM_TRIP_END;
			}
			break;

		case TRIPS_STREAM_BUCKET:
			written = snprintf(part, sizeof(part), "%s%u",
					   trips_stream.bucket_index > 0 ? "," : "",
					   trips_stream.trip.buckets[trips_stream.bucket_index]);
			if (written < 0 || (size_t)written >= sizeof(part)) {
				printk("BLE: Failed to format trip bucket\n");
				trips_stream_reset();
				return 0;
			}

			part_len = (size_t)written;
			copied = trips_stream_append(chunk, chunk_size, len,
						    part, part_len,
						    &trips_stream.phase_offset);
			len += copied;
			if (trips_stream.phase_offset == part_len) {
				trips_stream.phase_offset = 0;
				trips_stream.bucket_index++;
				if (trips_stream.bucket_index >= trips_stream.trip.bucket_count) {
					trips_stream.phase = TRIPS_STREAM_TRIP_END;
				}
			}
			break;

		case TRIPS_STREAM_TRIP_END:
			copied = trips_stream_append(chunk, chunk_size, len,
						    trip_end, sizeof(trip_end) - 1,
						    &trips_stream.phase_offset);
			len += copied;
			if (trips_stream.phase_offset == sizeof(trip_end) - 1) {
				trips_stream.phase_offset = 0;
				trips_stream_advance_to_next_trip();
			}
			break;

		case TRIPS_STREAM_END:
			copied = trips_stream_append(chunk, chunk_size, len,
						    stream_end, sizeof(stream_end) - 1,
						    &trips_stream.phase_offset);
			len += copied;
			if (trips_stream.phase_offset == sizeof(stream_end) - 1) {
				trips_stream.phase_offset = 0;
				trips_stream.phase = TRIPS_STREAM_DONE;
				trips_stream.active = false;
			}
			break;

		case TRIPS_STREAM_DONE:
		default:
			break;
		}

		if (copied == 0U) {
			break;
		}
	}

	return len;
}

static void trips_stream_start(void)
{
	trips_stream.active = true;
	trips_stream.sent_any_trip = false;
	trips_stream.phase = TRIPS_STREAM_START;
	trips_stream.phase_offset = 0;
	trips_stream.trip_count = odometer_get_trip_count();
	trips_stream.trip_index = 0;
	trips_stream.trip_id = 0;
	trips_stream.bucket_index = 0;

	printk("BLE: Starting trips notification stream (%zu trips)\n", trips_stream.trip_count);
	k_work_submit_to_queue(&trips_notify_work_q, &trips_notify_work);
}

static void trips_notify_complete(struct bt_conn *conn, void *user_data)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(user_data);

	trips_stream.notify_in_progress = false;
	if (trips_stream.active) {
		k_work_submit_to_queue(&trips_notify_work_q, &trips_notify_work);
	} else {
		printk("BLE: Trips notification stream complete\n");
	}
}

static void trips_notify_work_handler(struct k_work *work)
{
	struct bt_conn *conn = current_conn;
	size_t mtu_payload;
	size_t chunk_len;
	int err;

	ARG_UNUSED(work);

	if (!trips_stream.active || trips_stream.notify_in_progress || !trips_stream.notify_enabled || conn == NULL) {
		return;
	}

	mtu_payload = bt_gatt_get_mtu(conn);
	if (mtu_payload > 3U) {
		mtu_payload -= 3U;
	}
	mtu_payload = MIN(mtu_payload, sizeof(trips_stream.notify_buf));

	chunk_len = trips_stream_build_chunk(trips_stream.notify_buf, mtu_payload);
	if (chunk_len == 0U) {
		return;
	}

	trips_stream.notify_params.uuid = &bike_trips_uuid.uuid;
	trips_stream.notify_params.attr = NULL;
	trips_stream.notify_params.data = trips_stream.notify_buf;
	trips_stream.notify_params.len = (uint16_t)chunk_len;
	trips_stream.notify_params.func = trips_notify_complete;
	trips_stream.notify_params.user_data = NULL;
	trips_stream.notify_in_progress = true;

	err = bt_gatt_notify_cb(conn, &trips_stream.notify_params);
	if (err) {
		trips_stream.notify_in_progress = false;
		trips_stream_reset();
		printk("BLE: Trips notification failed (err %d)\n", err);
	}
}

static void trips_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);

	trips_stream.notify_enabled = (value & BT_GATT_CCC_NOTIFY) != 0U;
	if (!trips_stream.notify_enabled) {
		trips_stream_reset();
		printk("BLE: Trips notifications disabled\n");
	}
}

static ssize_t write_trips(struct bt_conn *conn,
					   const struct bt_gatt_attr *attr,
					   const void *buf, uint16_t len,
					   uint16_t offset, uint8_t flags)
{
	char command[16];

	ARG_UNUSED(attr);
	ARG_UNUSED(flags);

	if (offset != 0U) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	if (len >= sizeof(command)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	memcpy(command, buf, len);
	command[len] = '\0';

	if (len == 0U || strcmp(command, "start") == 0 || strcmp(command, "1") == 0) {
		if (!trips_stream.notify_enabled) {
			return BT_GATT_ERR(BT_ATT_ERR_CCC_IMPROPER_CONF);
		}

		if (conn == NULL || conn != current_conn) {
			return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
		}

		if (trips_stream.active || trips_stream.notify_in_progress) {
			return BT_GATT_ERR(BT_ATT_ERR_PROCEDURE_IN_PROGRESS);
		}

		trips_stream_start();
		return len;
	}

	if (strcmp(command, "cancel") == 0) {
		trips_stream_reset();
		return len;
	}

	return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
}

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
	BT_GATT_CUD("Pulse", BT_GATT_PERM_READ),
	BT_GATT_CHARACTERISTIC(&bike_trips_uuid.uuid,
					   BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
					   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
					   read_trips, write_trips, NULL),
	BT_GATT_CUD("Trips", BT_GATT_PERM_READ),
	BT_GATT_CCC(trips_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(&bike_battery_uuid.uuid,
						   BT_GATT_CHRC_READ,
						   BT_GATT_PERM_READ,
						   read_battery, NULL, NULL),
	BT_GATT_CUD("Battery", BT_GATT_PERM_READ),
	BT_GATT_CHARACTERISTIC(&bike_time_uuid.uuid,
						   BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
						   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
						   read_time, write_time, NULL),
	BT_GATT_CUD("Time", BT_GATT_PERM_READ),
	BT_GATT_CHARACTERISTIC(&bike_wheelsize_uuid.uuid,
						   BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
						   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
						   read_wheelsize, write_wheelsize, NULL),
	BT_GATT_CUD("Wheel Size", BT_GATT_PERM_READ),
);

/* Advertising data */
static struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, device_name, 0),
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

	if (current_conn != NULL) {
		bt_conn_unref(current_conn);
	}
	current_conn = bt_conn_ref(conn);

	printk("Connected\n");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	trips_stream_reset();
	trips_stream.notify_enabled = false;

	if (current_conn == conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}

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
	update_device_name();
	ad[1].data_len = strlen(device_name);

	k_work_init(&adv_work, adv_work_handler);
	k_work_init(&trips_notify_work, trips_notify_work_handler);
	k_work_queue_start(&trips_notify_work_q,
			   trips_notify_stack,
			   K_THREAD_STACK_SIZEOF(trips_notify_stack),
			   7,
			   NULL);
	trips_stream_reset();

	return 0;
}

void ble_service_start_advertising(void)
{
	k_work_submit(&adv_work);
}
