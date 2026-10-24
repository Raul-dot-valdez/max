#include "fuel_runtime.h"

#include <math.h>
#include <string.h>

#include "datalog.h"

#define RPM_RUNNING        400.0f
#define LEVEL_TAU_S        60.0f
#define RATE_TAU_S         120.0f
#define REG_PERIOD_S       30u    /* regression sample every 30 s of engine run */
#define REG_N              60     /* 30 min window */
#define REG_MIN_N          20     /* need 10 min before trusting the slope */
#define STEP_PCT           8.0f   /* refuel / drop detection threshold */
#define STEP_CONFIRM_S     15u
#define KEYON_SETTLE_S     10u
#define MIN_BURN_LPH       0.2f

static struct {
    fr_cfg_t       cfg;
    fr_counters_t *c;
    fr_event_fn_t  ev;
    fr_state_t     st;

    float    med[5];
    uint8_t  med_n, med_i;
    bool     filt_init;
    float    filt;
    uint32_t step_up_s, step_dn_s;
    uint32_t keyon_s;
    bool     keyon_checked, prev_key;

    bool     rate_init;
    float    rate_filt;

    float    reg_x[REG_N], reg_y[REG_N];
    uint8_t  reg_n, reg_i;
    uint32_t reg_tick;
    uint32_t run_s; /* engine-run seconds since boot (regression x-axis) */

    bool     service_flagged;
} s;

static void emit(uint16_t code, int32_t a, int32_t b)
{
    if (s.ev) s.ev(code, a, b);
}

void fr_init(const fr_cfg_t *cfg, fr_counters_t *ctr, fr_event_fn_t ev)
{
    memset(&s, 0, sizeof(s));
    s.cfg = *cfg;
    s.c = ctr;
    s.ev = ev;
    if (ctr->magic != FR_COUNTERS_MAGIC) {
        memset(ctr, 0, sizeof(*ctr));
        ctr->magic = FR_COUNTERS_MAGIC;
        ctr->next_service_h = cfg->service_interval_h;
        ctr->level_at_off = NAN;
    }
}

void fr_set_cfg(const fr_cfg_t *cfg) { s.cfg = *cfg; }

static float median5(void)
{
    float t[5];
    uint8_t n = s.med_n;
    memcpy(t, s.med, sizeof(t));
    for (uint8_t i = 1; i < n; i++)
        for (uint8_t j = i; j > 0 && t[j - 1] > t[j]; j--) {
            float x = t[j]; t[j] = t[j - 1]; t[j - 1] = x;
        }
    return t[n / 2];
}

static void reg_reset(void) { s.reg_n = s.reg_i = 0; }

static bool reg_slope(float *slope)
{
    if (s.reg_n < REG_MIN_N) return false;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (int i = 0; i < s.reg_n; i++) {
        sx += s.reg_x[i]; sy += s.reg_y[i];
        sxx += (double)s.reg_x[i] * s.reg_x[i];
        sxy += (double)s.reg_x[i] * s.reg_y[i];
    }
    double d = s.reg_n * sxx - sx * sx;
    if (fabs(d) < 1e-9) return false;
    *slope = (float)((s.reg_n * sxy - sx * sy) / d);
    return true;
}

static void level_update(const fr_input_t *in, bool running)
{
    if (!in->level_valid) return;
    s.med[s.med_i] = in->level_pct;
    s.med_i = (uint8_t)((s.med_i + 1) % 5);
    if (s.med_n < 5) s.med_n++;
    float m = median5();

    if (!s.filt_init) {
        s.filt = m;
        s.filt_init = true;
    }

    /* step detection on the median against the slow filter */
    s.step_up_s = (m - s.filt > STEP_PCT) ? s.step_up_s + 1 : 0;
    s.step_dn_s = (!running && s.filt - m > STEP_PCT) ? s.step_dn_s + 1 : 0;
    if (s.step_up_s >= STEP_CONFIRM_S) {
        emit(EV_REFUEL, (int32_t)(s.filt * 10), (int32_t)(m * 10));
        s.filt = m;
        reg_reset();
        s.step_up_s = 0;
    } else if (s.step_dn_s >= STEP_CONFIRM_S) {
        emit(EV_FUEL_DROP, (int32_t)(s.filt * 10), (int32_t)(m * 10));
        s.filt = m;
        reg_reset();
        s.step_dn_s = 0;
    } else {
        s.filt += (m - s.filt) / LEVEL_TAU_S;
    }

    /* compare with level stored at last key-off once the sender settles */
    if (!s.keyon_checked && s.keyon_s >= KEYON_SETTLE_S && s.med_n == 5) {
        s.keyon_checked = true;
        float prev = s.c->level_at_off;
        if (!isnan(prev)) {
            if (m - prev > STEP_PCT) emit(EV_REFUEL, (int32_t)(prev * 10), (int32_t)(m * 10));
            else if (prev - m > STEP_PCT) emit(EV_FUEL_DROP, (int32_t)(prev * 10), (int32_t)(m * 10));
        }
        s.filt = m;
    }

    bool low_before = s.st.low_fuel;
    if (s.filt < s.cfg.low_fuel_pct) s.st.low_fuel = true;
    else if (s.filt > s.cfg.low_fuel_pct + 2.0f) s.st.low_fuel = false;
    if (s.st.low_fuel && !low_before) emit(EV_LOW_FUEL, (int32_t)(s.filt * 10), 0);

    s.st.level_valid = true;
    s.st.level_pct = s.filt;
    s.st.remaining_l = s.filt * 0.01f * s.cfg.tank_l;
}

void fr_update_1hz(const fr_input_t *in)
{
    bool running = in->key_on && in->rpm_valid && in->rpm > RPM_RUNNING;
    s.st.engine_running = running;

    if (in->key_on && !s.prev_key) {
        s.keyon_s = 0;
        s.keyon_checked = false;
    }
    s.prev_key = in->key_on;
    if (!in->key_on) return;
    s.keyon_s++;

    /* counters */
    s.c->key_s++;
    if (running) {
        s.c->engine_s++;
        s.run_s++;
        if (in->blade_on) s.c->blade_s++;
    }

    level_update(in, running);

    /* burn rate */
    if (in->rate_valid && running) {
        s.c->fuel_used_l += in->rate_lph / 3600.0f;
        if (!s.rate_init) { s.rate_filt = in->rate_lph; s.rate_init = true; }
        s.rate_filt += (in->rate_lph - s.rate_filt) / RATE_TAU_S;
        s.st.burn_valid = true;
        s.st.burn_from_level = false;
        s.st.burn_lph = s.rate_filt;
    } else if (running && s.st.level_valid && ++s.reg_tick >= REG_PERIOD_S) {
        s.reg_tick = 0;
        s.reg_x[s.reg_i] = (float)s.run_s;
        s.reg_y[s.reg_i] = s.st.remaining_l;
        s.reg_i = (uint8_t)((s.reg_i + 1) % REG_N);
        if (s.reg_n < REG_N) s.reg_n++;
        float slope;
        if (!s.rate_init && reg_slope(&slope)) {
            s.st.burn_valid = true;
            s.st.burn_from_level = true;
            s.st.burn_lph = slope < 0 ? -slope * 3600.0f : 0.0f;
        }
    }

    /* runtime left */
    float usable = s.st.remaining_l - s.cfg.reserve_pct * 0.01f * s.cfg.tank_l;
    s.st.runtime_valid = s.st.level_valid && s.st.burn_valid && s.st.burn_lph > MIN_BURN_LPH;
    s.st.runtime_left_h = s.st.runtime_valid ? (usable > 0 ? usable / s.st.burn_lph : 0.0f) : 0.0f;

    /* hours */
    s.st.engine_hours_from_ecu = in->hours_valid;
    s.st.engine_hours = in->hours_valid ? in->ecu_hours : s.c->engine_s / 3600.0f;
    s.st.blade_hours = s.c->blade_s / 3600.0f;
    s.st.hours_to_service = s.c->next_service_h - s.st.engine_hours;
    if (s.st.hours_to_service <= 0 && !s.service_flagged) {
        s.service_flagged = true;
        emit(EV_SERVICE_DUE, (int32_t)(s.st.engine_hours * 10), 0);
    }
}

void fr_get(fr_state_t *out) { *out = s.st; }

void fr_key_off(void)
{
    if (s.st.level_valid) s.c->level_at_off = s.st.level_pct;
}

void fr_service_done(void)
{
    s.c->next_service_h = s.st.engine_hours + s.cfg.service_interval_h;
    s.service_flagged = false;
}
