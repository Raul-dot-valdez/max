#include "anomaly.h"

#include <math.h>
#include <string.h>

#include "datalog.h"

#define N_LEARN          600u    /* 10 min of 1 Hz samples per bin */
#define WARMUP_S         600u    /* coolant/hydraulics stabilised */
#define ALPHA_ADAPT      (1.0f / 3600.0f)
#define Z_ALERT          4.0f
#define Z_CLEAR          3.0f
#define ALERT_CONFIRM_S  30u
#define RPM_MIN          800.0f
#define RPM_MAX          3600.0f
#define IMPACT_RATIO     0.82f
#define IMPACT_TAU_MS    800.0f
#define IMPACT_HOLDOFF   5000u
#define ENGAGE_SETTLE_MS 3000u /* clutch engagement itself pulls rpm down */

/* Variance floors: sensor resolution + normal jitter. Prevents a very steady
 * bin from alarming on a 1-count change. */
static const float k_sigma_floor[AN_FEAT] = { 1.5f, 15.0f, 0.3f, 2.0f };

static struct {
    an_model_t   *m;
    an_event_fn_t ev;
    uint32_t      hi_s, lo_s;
    bool          alert;
    /* blade impact */
    float         rpm_ref;
    uint32_t      last_ms, last_impact_ms, engaged_ms;
    bool          ref_init;
} s;

void an_init(an_model_t *model, an_event_fn_t ev)
{
    memset(&s, 0, sizeof(s));
    s.m = model;
    s.ev = ev;
    if (model->magic != AN_MODEL_MAGIC) {
        memset(model, 0, sizeof(*model));
        model->magic = AN_MODEL_MAGIC;
    }
}

static int bin_index(float rpm, float load)
{
    int r = (int)((rpm - RPM_MIN) / ((RPM_MAX - RPM_MIN) / AN_RPM_BINS));
    int l = (int)(load / (100.0f / AN_LOAD_BINS));
    if (r < 0 || r >= AN_RPM_BINS) return -1;
    if (l < 0) l = 0;
    if (l >= AN_LOAD_BINS) l = AN_LOAD_BINS - 1;
    return r * AN_LOAD_BINS + l;
}

void an_update_1hz(const an_input_t *in, an_result_t *out)
{
    out->score = 0;
    out->feature = -1;
    out->alert = s.alert;

    unsigned trained = 0;
    for (int i = 0; i < AN_BINS; i++)
        trained += s.m->bin[i].n >= N_LEARN;
    out->learned_pct = (uint8_t)(trained * 100u / AN_BINS);

    int b = bin_index(in->rpm, in->load_pct);
    if (b < 0 || in->engine_run_s < WARMUP_S)
        return;
    an_bin_t *bin = &s.m->bin[b];

    if (bin->n < N_LEARN) {
        /* Welford */
        bin->n++;
        for (int f = 0; f < AN_FEAT; f++) {
            if (!in->valid[f]) continue;
            float d = in->x[f] - bin->mean[f];
            bin->mean[f] += d / (float)bin->n;
            bin->var[f] += d * (in->x[f] - bin->mean[f]);
        }
        if (bin->n == N_LEARN)
            for (int f = 0; f < AN_FEAT; f++)
                bin->var[f] /= (float)(N_LEARN - 1);
        return;
    }

    float worst = 0;
    for (int f = 0; f < AN_FEAT; f++) {
        if (!in->valid[f]) continue;
        float sd = sqrtf(bin->var[f]);
        if (sd < k_sigma_floor[f]) sd = k_sigma_floor[f];
        float z = fabsf(in->x[f] - bin->mean[f]) / sd;
        if (z > worst) { worst = z; out->feature = (int8_t)f; }
    }
    float sc = worst * 32.0f;
    out->score = (uint8_t)(sc > 255.0f ? 255.0f : sc);

    s.hi_s = worst > Z_ALERT ? s.hi_s + 1 : 0;
    s.lo_s = worst < Z_CLEAR ? s.lo_s + 1 : 0;
    if (!s.alert && s.hi_s >= ALERT_CONFIRM_S) {
        s.alert = true;
        if (s.ev) s.ev(EV_ANOMALY, out->score, out->feature);
    } else if (s.alert && s.lo_s >= ALERT_CONFIRM_S) {
        s.alert = false;
    }
    out->alert = s.alert;

    /* slow adaptation to wear, never while abnormal */
    if (worst < Z_CLEAR) {
        for (int f = 0; f < AN_FEAT; f++) {
            if (!in->valid[f]) continue;
            float d = in->x[f] - bin->mean[f];
            bin->mean[f] += ALPHA_ADAPT * d;
            bin->var[f] = (1.0f - ALPHA_ADAPT) * (bin->var[f] + ALPHA_ADAPT * d * d);
        }
    }
}

bool an_blade_impact(float rpm, bool blade_on, uint32_t now_ms)
{
    bool hit = false;
    if (!s.ref_init || !blade_on || rpm < RPM_MIN) {
        s.rpm_ref = rpm;
        s.ref_init = true;
        s.last_ms = s.engaged_ms = now_ms;
        return false;
    }
    float dt = (float)(now_ms - s.last_ms);
    s.last_ms = now_ms;
    if (now_ms - s.engaged_ms > ENGAGE_SETTLE_MS && rpm < IMPACT_RATIO * s.rpm_ref &&
        now_ms - s.last_impact_ms > IMPACT_HOLDOFF) {
        s.last_impact_ms = now_ms;
        if (s.ev) s.ev(EV_BLADE_IMPACT, (int32_t)s.rpm_ref, (int32_t)rpm);
        hit = true;
    }
    float a = dt / (IMPACT_TAU_MS + dt);
    s.rpm_ref += a * (rpm - s.rpm_ref);
    return hit;
}
