/*
 * LED module - Status LED control
 */

#include "leds.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define LED_GPIO_NODE DT_NODELABEL(gpio0)

#ifndef RUN_STATUS_PIN
#define RUN_STATUS_PIN 13
#endif

#ifndef CON_STATUS_PIN
#define CON_STATUS_PIN 14
#endif

static const struct device *led_dev;
static bool run_led_state;

int leds_init(void)
{
	led_dev = DEVICE_DT_GET(LED_GPIO_NODE);
	if (!device_is_ready(led_dev)) {
		printk("LED GPIO device not ready\n");
		led_dev = NULL;
		return -ENODEV;
	}

	printk("LED GPIO device ready\n");

	/* nRF DK LEDs are active-low; configure with GPIO_ACTIVE_LOW so
	 * logical '1' means LED on and '0' means LED off.
	 */
	int err = gpio_pin_configure(led_dev, RUN_STATUS_PIN, GPIO_OUTPUT_INACTIVE | GPIO_ACTIVE_LOW);
	if (err) {
		printk("Failed to configure RUN LED pin %d (err %d)\n", RUN_STATUS_PIN, err);
		return err;
	}
	printk("Configured RUN LED pin %d\n", RUN_STATUS_PIN);

	err = gpio_pin_configure(led_dev, CON_STATUS_PIN, GPIO_OUTPUT_INACTIVE | GPIO_ACTIVE_LOW);
	if (err) {
		printk("Failed to configure CON LED pin %d (err %d)\n", CON_STATUS_PIN, err);
		return err;
	}
	printk("Configured CON LED pin %d\n", CON_STATUS_PIN);

	return 0;
}

void leds_set_run(bool on)
{
	if (led_dev) {
		gpio_pin_set(led_dev, RUN_STATUS_PIN, on ? 1 : 0);
		run_led_state = on;
	}
}

void leds_set_connection(bool on)
{
	if (led_dev) {
		gpio_pin_set(led_dev, CON_STATUS_PIN, on ? 1 : 0);
	}
}

void leds_toggle_run(void)
{
	run_led_state = !run_led_state;
	leds_set_run(run_led_state);
}
