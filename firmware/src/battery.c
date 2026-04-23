/*
 * Battery module - CR2032 voltage estimation via ADC
 *
 * Uses SAADC internal VDD channel to measure supply voltage.
 * CR2032 discharge curve (approximation):
 *   3.0V = 100% (fresh)
 *   2.9V = 90%
 *   2.8V = 70%
 *   2.7V = 50%
 *   2.5V = 20%
 *   2.4V = 10%
 *   2.0V = 0% (cutoff)
 */

#include "battery.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>
#include <zephyr/devicetree.h>

/* ADC configuration from devicetree */
#define ADC_NODE DT_NODELABEL(adc)
#define ADC_CHANNEL_NODE DT_CHILD(ADC_NODE, channel_0)
#define ADC_CHANNEL_ID 0
#define ADC_RESOLUTION 12

/*
 * Voltage calculation for VDD with gain 1/6 and 0.6V internal reference:
 * - ADC full scale = 0.6V * 6 (reciprocal of 1/6 gain) = 3.6V at input
 * - Input is VDD, so VDD (mV) = raw * 3600 / 4095
 */
#define ADC_FULL_SCALE_MV 3600

static const struct device *adc_dev;
static uint16_t battery_voltage_mv;
static uint8_t battery_percent;
static bool adc_initialized;

static int16_t adc_buffer;

/* Channel config from devicetree */
static const struct adc_channel_cfg channel_cfg = ADC_CHANNEL_CFG_DT(ADC_CHANNEL_NODE);

static struct adc_sequence sequence = {
	.channels = BIT(ADC_CHANNEL_ID),
	.buffer = &adc_buffer,
	.buffer_size = sizeof(adc_buffer),
	.resolution = ADC_RESOLUTION,
};

/*
 * Convert ADC raw value to millivolts
 * VDD (mV) = raw * ADC_FULL_SCALE_MV / 4095
 */
static uint16_t adc_raw_to_mv(int16_t raw)
{
	if (raw < 0) {
		return 0;
	}
	return (uint16_t)((raw * (uint32_t)ADC_FULL_SCALE_MV) / 4095UL);
}

/*
 * Map CR2032 voltage to battery percentage
 * Based on typical CR2032 discharge curve under light load
 */
static uint8_t voltage_to_percent(uint16_t mv)
{
	/* CR2032 voltage thresholds (mV) and corresponding percentages */
	static const struct {
		uint16_t mv;
		uint8_t percent;
	} curve[] = {
		{ 3000, 100 },
		{ 2900,  90 },
		{ 2800,  70 },
		{ 2700,  50 },
		{ 2600,  30 },
		{ 2500,  20 },
		{ 2400,  10 },
		{ 2000,   0 },
	};

	/* Above max */
	if (mv >= curve[0].mv) {
		return curve[0].percent;
	}

	/* Below min */
	if (mv <= curve[ARRAY_SIZE(curve) - 1].mv) {
		return curve[ARRAY_SIZE(curve) - 1].percent;
	}

	/* Linear interpolation between curve points */
	for (size_t i = 0; i < ARRAY_SIZE(curve) - 1; i++) {
		if (mv >= curve[i + 1].mv) {
			uint16_t v_high = curve[i].mv;
			uint16_t v_low = curve[i + 1].mv;
			uint8_t p_high = curve[i].percent;
			uint8_t p_low = curve[i + 1].percent;

			/* Interpolate */
			uint16_t v_range = v_high - v_low;
			uint8_t p_range = p_high - p_low;
			uint16_t v_offset = mv - v_low;

			return p_low + (uint8_t)((v_offset * p_range) / v_range);
		}
	}

	return 0;
}

int battery_measure(void)
{
	int err;

	if (!adc_initialized) {
		adc_dev = DEVICE_DT_GET(ADC_NODE);
		if (!device_is_ready(adc_dev)) {
			printk("Battery: ADC device not ready\n");
			return -ENODEV;
		}

		err = adc_channel_setup(adc_dev, &channel_cfg);
		if (err) {
			printk("Battery: ADC channel setup failed (%d)\n", err);
			return err;
		}
		adc_initialized = true;
	}

	/* Small delay to let voltage stabilize after boot */
	k_sleep(K_MSEC(10));

	/* Take multiple readings and average for better accuracy */
	int32_t sum = 0;
	const int num_readings = 4;

	for (int i = 0; i < num_readings; i++) {
		adc_buffer = 0;
		err = adc_read(adc_dev, &sequence);
		if (err) {
			printk("Battery: ADC read failed (%d)\n", err);
			return err;
		}
		sum += adc_buffer;
		k_sleep(K_MSEC(5));  /* Brief delay between readings */
	}

	int16_t avg_raw = (int16_t)(sum / num_readings);
	battery_voltage_mv = adc_raw_to_mv(avg_raw);
	battery_percent = voltage_to_percent(battery_voltage_mv);

	printk("Battery: raw=%d, voltage=%u mV, charge=%u%%\n",
		   avg_raw, battery_voltage_mv, battery_percent);

	return 0;
}

uint16_t battery_get_voltage_mv(void)
{
	return battery_voltage_mv;
}

uint8_t battery_get_percent(void)
{
	return battery_percent;
}
