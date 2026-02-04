/*
 * LED module - Status LED control
 */

#ifndef LEDS_H
#define LEDS_H

#include <stdbool.h>

/**
 * @brief Initialize status LEDs
 * @return 0 on success, negative errno on failure
 */
int leds_init(void);

/**
 * @brief Set the run status LED state
 * @param on true = LED on, false = LED off
 */
void leds_set_run(bool on);

/**
 * @brief Set the connection status LED state
 * @param on true = LED on, false = LED off
 */
void leds_set_connection(bool on);

/**
 * @brief Toggle the run status LED
 */
void leds_toggle_run(void);

#endif /* LEDS_H */
