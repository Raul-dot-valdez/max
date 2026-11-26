/*
 * Analog and pulse inputs used on mowers whose engine has no ECU (firmware architecture 2).
 * The M1 has 5 inputs plus a 4-20 mA input; which pin carries which signal is a board
 * and harness decision (config/board_config.h). Not yet implemented in the STM32H7 port:
 * services/analog_signals.c links a weak default that reports PAL_ERR, so the build works and
 * analog features stay off until the port adds this function.
 */
#ifndef PAL_ANALOG_H
#define PAL_ANALOG_H

#include "pal_types.h"

typedef struct {
    uint32_t pulses;          /* ignition / stator pulses counted since the previous sample */
    uint32_t window_ms;       /* length of that counting window */
    float    battery_mv;      /* at the ADC pin, before the divider is undone */
    float    fuel_sender_ohm; /* resistance of the tank sender */
    bool     oil_switch_closed;
    bool     pto_on;
} pal_analog_raw_t;

pal_status_t pal_analog_sample(pal_analog_raw_t *raw);

#endif
