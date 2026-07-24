/*
 * Bike Odometer - Main application
 *
 * Tracks bike travel distance using a magnet sensor on the wheel.
 * Aggregates pulses into 5-minute bins and exposes data via BLE.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/settings/settings.h>

#include "battery.h"
#include "odometer.h"
#include "ble_service.h"
#include "rtc.h"

#define BOOT_ADVERTISING_SECONDS 60U

int main(void)
{
	int err;

	printk("Starting Bike Odometer\n");

	/* Measure battery FIRST before any other subsystems
	 * to minimize load on CR2032 (high internal resistance)
	 */
	err = battery_measure();
	if (err) {
		printk("Battery measurement failed (err %d)\n", err);
		/* Continue - battery info will just be unavailable */
	}

	/* Initialize BLE */
	err = ble_service_init();
	if (err) {
		printk("BLE init failed (err %d)\n", err);
		return 0;
	}

	/* Load saved data from NVM */
	odometer_load_from_nvm();

	/* Initialize settings subsystem (for RTC) and load */
	err = settings_subsys_init();
	if (err) {
		printk("Settings init failed (err %d)\n", err);
	} else {
		settings_load();
	}

	/* Initialize RTC (after settings loaded) */
	rtc_init();

	/* Initialize odometer (pulse GPIO and bin timer) */
	err = odometer_init();
	if (err) {
		printk("Odometer init failed (err %d)\n", err);
		/* Continue - BLE will still work */
	}

	/* Allow setup/sync after boot, then leave the radio idle. */
	ble_service_start_advertising_for(BOOT_ADVERTISING_SECONDS);

	/* Main loop */
	for (;;) {
		k_sleep(K_FOREVER);
	}
}
