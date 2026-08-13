/*
 * RTC module - Software real-time clock implementation
 */

#include "rtc.h"

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>


/* State */
static uint64_t base_unix_time;      /* Unix timestamp when time was set */
static int64_t base_uptime_ms;       /* k_uptime_get() when time was set */
static bool time_is_set;

/*
 * Settings handler for loading saved time
 */
#if IS_ENABLED(CONFIG_SETTINGS)
static int rtc_set(const char *name, size_t len,
                   settings_read_cb read_cb, void *cb_arg)
{
    if (strcmp(name, "base") == 0) {
        if (len != sizeof(base_unix_time)) {
            return -EINVAL;
        }
        ssize_t rc = read_cb(cb_arg, &base_unix_time, sizeof(base_unix_time));
        if (rc < 0) {
            return rc;
        }
        /* When loading from NVS, we don't have the original uptime,
         * so we set base_uptime_ms to current uptime. This means
         * the loaded time will be stale (from before sleep/reset),
         * but it's better than nothing until BLE syncs. */
        base_uptime_ms = k_uptime_get();
        time_is_set = (base_unix_time > 0);
        return 0;
    }

    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(rtc, "rtc", NULL, rtc_set, NULL, 0);
#endif

void rtc_init(void)
{
    /* Settings are loaded by main's settings_load() call */
}

void rtc_set_time(uint64_t unix_time)
{
    base_unix_time = unix_time;
    base_uptime_ms = k_uptime_get();
    time_is_set = true;


#if IS_ENABLED(CONFIG_SETTINGS)
    /* Persist to NVS */
    int err = settings_save_one("rtc/base", &base_unix_time, sizeof(base_unix_time));
    if (err) {
    }
#endif
}

uint64_t rtc_get_time(void)
{
    if (!time_is_set) {
        return 0;
    }

    int64_t elapsed_ms = k_uptime_get() - base_uptime_ms;
    uint64_t elapsed_sec = (uint64_t)(elapsed_ms / 1000);

    return base_unix_time + elapsed_sec;
}

uint64_t rtc_get_time_ms(void)
{
    if (!time_is_set) {
        return 0;
    }

    int64_t elapsed_ms = k_uptime_get() - base_uptime_ms;

    return (base_unix_time * 1000ULL) + (uint64_t)elapsed_ms;
}

bool rtc_is_time_set(void)
{
    return time_is_set;
}
