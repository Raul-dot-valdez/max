/*
 * GT911: address 0x5D is selected by holding INT low through reset release.
 * For gloves and rain the panel vendor must burn a tuned config (0x8047..);
 * we only verify the product ID here and never rewrite config in the field.
 */
#include "gt911.h"

#include "pal_gpio.h"
#include "pal_i2c.h"
#include "pal_sys.h"

#define GT911_ADDR    0x5D
#define REG_PRODUCT   0x8140
#define REG_STATUS    0x814E
#define REG_POINT1    0x814F

static uint8_t s_bus;

static pal_status_t rd(uint16_t reg, uint8_t *v, size_t n)
{
    uint8_t a[2] = { (uint8_t)(reg >> 8), (uint8_t)reg };
    return pal_i2c_xfer(s_bus, GT911_ADDR, a, 2, v, n, 10);
}

static pal_status_t wr8(uint16_t reg, uint8_t v)
{
    uint8_t a[3] = { (uint8_t)(reg >> 8), (uint8_t)reg, v };
    return pal_i2c_xfer(s_bus, GT911_ADDR, a, 3, NULL, 0, 10);
}

pal_status_t gt911_init(uint8_t i2c_bus)
{
    uint8_t id[4];
    s_bus = i2c_bus;

    pal_gpio_set_output(PAL_PIN_TOUCH_INT, true);
    pal_gpio_write(PAL_PIN_TOUCH_INT, false);
    pal_gpio_write(PAL_PIN_TOUCH_RESET, false);
    pal_sys_delay_ms(10);
    pal_gpio_write(PAL_PIN_TOUCH_RESET, true);
    pal_sys_delay_ms(60);
    pal_gpio_set_output(PAL_PIN_TOUCH_INT, false);

    if (rd(REG_PRODUCT, id, 4) != PAL_OK)
        return PAL_ERR;
    return (id[0] == '9' && id[1] == '1' && id[2] == '1') ? PAL_OK : PAL_ENOENT;
}

pal_status_t gt911_read(touch_point_t *pt)
{
    uint8_t st, p[8];
    if (rd(REG_STATUS, &st, 1) != PAL_OK)
        return PAL_ERR;
    if (!(st & 0x80))
        return PAL_BUSY; /* no new sample */
    uint8_t n = st & 0x0F;
    if (n > 0 && n <= 5 && rd(REG_POINT1, p, sizeof(p)) == PAL_OK) {
        pt->pressed = true;
        pt->x = (uint16_t)(p[1] | (p[2] << 8));
        pt->y = (uint16_t)(p[3] | (p[4] << 8));
    } else {
        pt->pressed = false;
    }
    return wr8(REG_STATUS, 0); /* release the buffer */
}
