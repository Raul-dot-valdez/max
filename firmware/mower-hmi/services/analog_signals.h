/*
 * Turns raw display inputs into canonical signals for machines without an ECU, so the rest of
 * the firmware (fuel / runtime, data log, edge AI) sees the same vehicle_data store either way.
 */
#ifndef ANALOG_SIGNALS_H
#define ANALOG_SIGNALS_H

#include "machine_profile.h"
#include "pal_analog.h"

typedef struct {
    float pulses_per_rev;   /* ignition pulses per crank revolution (VERIFY per engine) */
    float battery_divider;  /* input divider ratio: battery V = pin V x ratio */
    float fuel_empty_ohm;   /* US-standard senders: 240 ohm empty, 33 ohm full */
    float fuel_full_ohm;
} as_cfg_t;

extern const as_cfg_t AS_CFG_DEFAULT;

float as_rpm(uint32_t pulses, uint32_t window_ms, float pulses_per_rev);
float as_battery_v(float pin_mv, float divider);
float as_fuel_pct(float ohm, float empty_ohm, float full_ohm);

/* Publish the signals the profile sources from analog inputs or switches. With no profile yet,
 * only battery voltage is published, and only while no ECU is sending it (auto-detect needs it). */
void as_publish(const pal_analog_raw_t *raw, const as_cfg_t *cfg, const mp_profile_t *p, uint32_t now_ms);

#endif
