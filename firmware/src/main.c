/*
 * Bike Odometer - Main application
 *
 * Tracks bike travel distance using a magnet sensor on the wheel.
 * Aggregates pulses into 5-minute bins and exposes data via BLE.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "leds.h"
#include "odometer.h"
#include "ble_service.h"

#define RUN_LED_BLINK_INTERVAL_MS 1000

/* BLE connection callback - updates connection LED */
static void on_ble_connection_changed(bool connected)
{
	leds_set_connection(connected);
}

int main(void)
{
	int err;

	printk("Starting Bike Odometer\n");

	/* Initialize LEDs */
	err = leds_init();
	if (err) {
		printk("LED init failed (err %d)\n", err);
		/* Continue without LEDs */
	}

	/* Initialize BLE */
	err = ble_service_init();
	if (err) {
		printk("BLE init failed (err %d)\n", err);
		return 0;
	}

	/* Load saved data from NVM */
	odometer_load_from_nvm();

	/* Initialize odometer (pulse GPIO and bin timer) */
	err = odometer_init();
	if (err) {
		printk("Odometer init failed (err %d)\n", err);
		/* Continue - BLE will still work */
	}

	/* Register BLE connection callback for LED updates */
	ble_service_set_connection_callback(on_ble_connection_changed);

	/* Start advertising */
	ble_service_start_advertising();

	/* Main loop - blink run LED */
	for (;;) {
		leds_toggle_run();
		k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL_MS));
	}
}
