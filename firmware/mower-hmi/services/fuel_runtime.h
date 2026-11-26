/*
 * Runtime counters and fuel-remaining estimator.
 *
 * Fuel level senders on mowers are noisy (slosh on slopes, float-arm
 * non-linearity), and many small diesel/EFI engines do not broadcast LFE1
 * fuel rate. The estimator therefore:
 *   - median-filters then low-passes the tank level (tau 60 s);
 *   - uses ECU fuel rate (SPN 183) when present, smoothed over ~2 min;
 *   - otherwise fits a least-squares slope of litres vs ENGINE-RUN seconds
 *     over the last 30 min (wall-clock gaps while parked do not dilute it);
 *   - reports runtime left = (remaining - reserve) / burn rate;
 *   - detects refuel and unexplained level drops (theft/leak), including
 *     across a key-off by comparing against the level persisted at key-off.
 */
#ifndef FUEL_RUNTIME_H
#define FUEL_RUNTIME_H

#include "pal_types.h"

typedef struct {
    float tank_l;
    float low_fuel_pct;
    float reserve_pct;
    float service_interval_h;
} fr_cfg_t;

/* Persisted (nvstore + periodic save). Append fields only. */
typedef struct PAL_PACKED {
    uint32_t magic;
    uint32_t key_s;          /* key-on time, lifetime */
    uint32_t engine_s;       /* engine running (rpm > 400), as counted by this HMI */
    uint32_t blade_s;        /* blades engaged with engine running */
    float    fuel_used_l;    /* integrated from SPN 183 when available */
    float    next_service_h; /* engine-hours at which service is due */
    float    level_at_off;   /* tank % at last key-off (theft/refuel detection) */
} fr_counters_t;

#define FR_COUNTERS_MAGIC 0x46524331u /* "FRC1" */

typedef struct {
    bool  key_on;
    bool  rpm_valid;      float rpm;
    bool  rate_valid;     float rate_lph;
    bool  level_valid;    float level_pct;
    bool  hours_valid;    float ecu_hours;
    bool  blade_on;
} fr_input_t;

typedef struct {
    bool  level_valid;    float level_pct;   float remaining_l;
    bool  burn_valid;     float burn_lph;    bool  burn_from_level;
    bool  runtime_valid;  float runtime_left_h;
    bool  low_fuel;
    float engine_hours;   bool  engine_hours_from_ecu;
    float blade_hours;
    float hours_to_service;
    bool  engine_running;
} fr_state_t;

typedef void (*fr_event_fn_t)(uint16_t code, int32_t a, int32_t b);

void fr_init(const fr_cfg_t *cfg, fr_counters_t *ctr, fr_event_fn_t ev);
void fr_set_cfg(const fr_cfg_t *cfg);
void fr_update_1hz(const fr_input_t *in);
void fr_get(fr_state_t *out);
void fr_key_off(void);           /* stores level_at_off into counters */
void fr_service_done(void);      /* resets the service countdown */

#endif
