#include "analog_signals.h"

const as_cfg_t AS_CFG_DEFAULT = { .pulses_per_rev = 1.0f, .battery_divider = 6.0f,
                                  .fuel_empty_ohm = 240.0f, .fuel_full_ohm = 33.0f };

/* Ports without analog inputs link this default: analog features stay off. */
PAL_WEAK pal_status_t pal_analog_sample(pal_analog_raw_t *raw)
{
    (void)raw;
    return PAL_ERR;
}

float as_rpm(uint32_t pulses, uint32_t window_ms, float pulses_per_rev)
{
    if (window_ms == 0 || pulses_per_rev <= 0.0f)
        return 0.0f;
    return (float)pulses / pulses_per_rev * 60000.0f / (float)window_ms;
}

float as_battery_v(float pin_mv, float divider) { return pin_mv * divider / 1000.0f; }

float as_fuel_pct(float ohm, float empty_ohm, float full_ohm)
{
    float pct = (empty_ohm - ohm) / (empty_ohm - full_ohm) * 100.0f;
    return pct < 0.0f ? 0.0f : (pct > 100.0f ? 100.0f : pct);
}

static bool from_inputs(const mp_profile_t *p, vd_id_t id)
{
    return p && (p->src[id] == MP_SRC_ANALOG || p->src[id] == MP_SRC_SWITCH);
}

void as_publish(const pal_analog_raw_t *raw, const as_cfg_t *cfg, const mp_profile_t *p, uint32_t now_ms)
{
    float tmp;
    if (from_inputs(p, VD_BATTERY_V) || (!p && !vd_get(VD_BATTERY_V, &tmp, now_ms)))
        vd_set(VD_BATTERY_V, as_battery_v(raw->battery_mv, cfg->battery_divider), now_ms);
    if (!p)
        return;
    if (from_inputs(p, VD_ENGINE_RPM))
        vd_set(VD_ENGINE_RPM, as_rpm(raw->pulses, raw->window_ms, cfg->pulses_per_rev), now_ms);
    if (from_inputs(p, VD_FUEL_LEVEL_PCT)) {
        /* open (>1.5x empty) or shorted (<half of full) sender reads as not available */
        if (raw->fuel_sender_ohm > cfg->fuel_empty_ohm * 1.5f || raw->fuel_sender_ohm < cfg->fuel_full_ohm * 0.5f)
            vd_set_invalid(VD_FUEL_LEVEL_PCT, now_ms);
        else
            vd_set(VD_FUEL_LEVEL_PCT, as_fuel_pct(raw->fuel_sender_ohm, cfg->fuel_empty_ohm, cfg->fuel_full_ohm), now_ms);
    }
    if (from_inputs(p, VD_OIL_LOW_SW))
        vd_set(VD_OIL_LOW_SW, raw->oil_switch_closed ? 1.0f : 0.0f, now_ms);
    if (from_inputs(p, VD_BLADE_ENGAGED))
        vd_set(VD_BLADE_ENGAGED, raw->pto_on ? 1.0f : 0.0f, now_ms);
}
