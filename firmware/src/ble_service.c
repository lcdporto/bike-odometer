/*
 * BLE Service module - GATT service and advertising
 */

#include "ble_service.h"
#include "odometer.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <stdio.h>
#include <stdbool.h>

#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

/* Advertising work */
static struct k_work adv_work;

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
	const struct trip_entry *trips = odometer_get_trips();
	size_t trip_count = odometer_get_trip_count();

	char out[1024];
	size_t pos = 0;

	pos += snprintf(out + pos, sizeof(out) - pos, "{\"trips\":[");

	/* Output each trip */
	for (size_t t = 0; t < trip_count; t++) {
		const struct trip_entry *trip = &trips[t];
		
		if (t > 0) {
			pos += snprintf(out + pos, sizeof(out) - pos, ",");
		}

		pos += snprintf(out + pos, sizeof(out) - pos, 
						"{\"id\":%zu,\"startDate\":%llu,\"buckets\":[",
						t + 1, (unsigned long long)trip->start_timestamp_ms);

		/* Output buckets for this trip */
		for (size_t b = 0; b < trip->bucket_count; b++) {
			if (b > 0) {
				pos += snprintf(out + pos, sizeof(out) - pos, ",");
			}
			pos += snprintf(out + pos, sizeof(out) - pos, "%u", trip->buckets[b]);
		}

		pos += snprintf(out + pos, sizeof(out) - pos, "]}");
	}

	pos += snprintf(out + pos, sizeof(out) - pos, "]}\n");

	return bt_gatt_attr_read(conn, attr, buf, len, offset, out, strlen(out));
}

/* Service and characteristic UUIDs */
static struct bt_uuid_128 bike_svc_uuid = BT_UUID_INIT_128(
	0x78,0x56,0x34,0x12,0x34,0x12,0x78,0x56,0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78);

static struct bt_uuid_128 bike_pulse_uuid = BT_UUID_INIT_128(
	0x79,0x56,0x34,0x12,0x34,0x12,0x78,0x56,0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78);

static struct bt_uuid_128 bike_trips_uuid = BT_UUID_INIT_128(
	0x88,0x56,0x34,0x12,0x34,0x12,0x78,0x56,0x12,0x34,0x56,0x78,0x12,0x34,0x56,0x78);

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
);

/* Advertising data */
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
