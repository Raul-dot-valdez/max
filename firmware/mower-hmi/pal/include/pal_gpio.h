/*
 * PAL GPIO by logical function, not by port/pin. The board port owns the
 * mapping table, so moving a signal on a new PCB never touches app code.
 */
#ifndef PAL_GPIO_H
#define PAL_GPIO_H

#include "pal_types.h"

typedef enum {
    PAL_PIN_POWER_HOLD,   /* out: keeps the supply latched after ignition off */
    PAL_PIN_IGN_SENSE,    /* in:  ignition / key switch (high = on) */
    PAL_PIN_BACKLIGHT_EN, /* out */
    PAL_PIN_LCD_RESET,    /* out, active low */
    PAL_PIN_TOUCH_RESET,  /* out, active low */
    PAL_PIN_TOUCH_INT,    /* in / out during GT911 address select */
    PAL_PIN_CAN_STBY,     /* out: transceiver standby (high = standby) */
    PAL_PIN_GNSS_RESET,   /* out, active low */
    PAL_PIN_BLE_RESET,    /* out, active low: PSoC 4 XRES */
    PAL_PIN_BUZZER,       /* out */
    PAL_PIN_COUNT
} pal_pin_t;

void pal_gpio_init(void);
void pal_gpio_write(pal_pin_t pin, bool level);
bool pal_gpio_read(pal_pin_t pin);
void pal_gpio_set_output(pal_pin_t pin, bool output);

#endif /* PAL_GPIO_H */
