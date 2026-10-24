/* Goodix GT911 capacitive touch controller (5 points, we report the first). */
#ifndef GT911_H
#define GT911_H

#include "pal_types.h"

typedef struct {
    bool     pressed;
    uint16_t x, y;
} touch_point_t;

pal_status_t gt911_init(uint8_t i2c_bus);
pal_status_t gt911_read(touch_point_t *pt);

#endif
