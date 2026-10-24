/*
 * External RTC interface. The application only deals in UNIX seconds (UTC);
 * local time and DST are a display concern.
 */
#ifndef RTC_H
#define RTC_H

#include "pal_types.h"

typedef struct rtc_dev rtc_dev_t;

struct rtc_dev {
    /* valid = false after a power-on reset with a flat backup cell */
    pal_status_t (*get)(rtc_dev_t *d, uint32_t *unix_s, bool *valid);
    pal_status_t (*set)(rtc_dev_t *d, uint32_t unix_s);
    void *ctx;
};

/* Micro Crystal RV-3028-C7 */
pal_status_t rv3028_init(rtc_dev_t *dev, uint8_t i2c_bus);

#endif
