/* PAL I2C: master, 7-bit addressing. Used for the external RTC and touch controller. */
#ifndef PAL_I2C_H
#define PAL_I2C_H

#include "pal_types.h"

pal_status_t pal_i2c_init(uint8_t bus, uint32_t hz);
/* Write wlen bytes then (if rlen > 0) read rlen bytes. Either side may be zero. */
pal_status_t pal_i2c_xfer(uint8_t bus, uint8_t addr7,
                          const uint8_t *wbuf, size_t wlen,
                          uint8_t *rbuf, size_t rlen, uint32_t timeout_ms);
/* Clock out up to 9 pulses to release a slave stuck holding SDA low. */
pal_status_t pal_i2c_bus_recover(uint8_t bus);

#endif /* PAL_I2C_H */
