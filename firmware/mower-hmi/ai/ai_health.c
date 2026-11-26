#include "ai_health.h"

#include <math.h>
#include <string.h>

#include "ai_backend.h"
#include "generated/ai_scaler.h"

#define PERIOD_S        30u
#define ALERT_REPEATS   2u

_Static_assert(AI_N_FEAT == AI_F_COUNT, "feature count must match the trained model");
_Static_assert(AI_N_CLASS == 4, "class list must match ai_class_t");

static struct {
    float    ring[AI_WINDOW][AI_N_FEAT];
    float    last_valid[AI_N_FEAT];
    bool     have_valid[AI_N_FEAT];
    uint32_t head, count, since;
    float    window[AI_WINDOW * AI_N_FEAT];
    ai_class_t last_fault;
    uint32_t repeats;
    bool     alert;
    uint32_t inferences;
    const ai_backend_t *be;
    const mp_profile_t *prof;
    bool     expected[AI_N_FEAT];
    float    prof_clog;
    float    et_base;        /* air-cooled: learned oil-temperature baseline with blades engaged */
    uint32_t et_n;           /* samples in that baseline */
} s;

#define ET_BASE_TAU_S   1800.0f  /* 30 min: follows weather and season, not a clog building up */
#define ET_BASE_MIN_S   600u     /* 10 min of mowing before the air-cooled clog rule is armed */
#define ET_WARM_C       60.0f    /* ignore a cold engine */

/* Model feature -> canonical signal. */
static const vd_id_t k_feat_vd[AI_N_FEAT] = { VD_ENGINE_RPM, VD_ENGINE_LOAD_PCT, VD_COOLANT_C, VD_OIL_PRESS_KPA,
                                              VD_FUEL_RATE_LPH, VD_HYD_OIL_C, VD_AMBIENT_C, VD_WHEEL_SPEED_KMH,
                                              VD_BLADE_ENGAGED };

static const char *k_names[] = { "normal", "screen_clog", "low_oil", "hyd_overheat" };

/* Builds without an AI backend link this default; ai_health then stays inactive. */
PAL_WEAK const ai_backend_t *ai_backend_get(void) { return NULL; }

const char *ai_class_name(ai_class_t c) { return (unsigned)c < 4 ? k_names[c] : "?"; }

static mp_head_t active_head(void) { return s.prof ? s.prof->head : MP_HEAD_LIQUID; }

void ai_health_set_profile(const mp_profile_t *p)
{
    s.prof = p;
    for (int f = 0; f < AI_N_FEAT; f++) {
        s.expected[f] = !p || p->src[k_feat_vd[f]] != MP_SRC_NONE;
        s.have_valid[f] = false;
    }
    s.prof_clog = p ? p->clog_temp_c : 95.0f;
    s.et_base = 0.0f;
    s.et_n = 0;
    ai_health_reset();
}

pal_status_t ai_health_init(void)
{
    memset(&s, 0, sizeof(s));
    s.be = ai_backend_get();
    ai_health_set_profile(NULL);
    return (s.be && s.be->init) ? s.be->init() : PAL_ERR;
}

void ai_health_reset(void)
{
    s.head = s.count = s.since = 0;
    s.repeats = 0;
    s.last_fault = AI_CLASS_NORMAL;
}

/* Liquid head: same thresholds as rules_baseline() in tools/edge_ai_poc/train_phase1.py.
 * Other heads: physics rules on the signals their profile provides. */
static ai_class_t rules(const float *x, const ai_aux_t *aux)
{
    const float etemp = aux ? aux->engine_temp_c : NAN;
    const float oil_sw = aux ? aux->oil_low_sw : NAN;
    const bool hyd_ok = s.expected[AI_F_HYD];
    switch (active_head()) {
    case MP_HEAD_LIQUID:
        if (x[AI_F_RPM] > 2000 && x[AI_F_OILP] < 150) return AI_CLASS_LOW_OIL;
        if (x[AI_F_BLADE] > 0.5f && x[AI_F_COOLANT] > s.prof_clog) return AI_CLASS_SCREEN_CLOG;
        break;
    case MP_HEAD_AIR_EFI:
        if (x[AI_F_RPM] > 2000 && s.expected[AI_F_OILP] && x[AI_F_OILP] < 150) return AI_CLASS_LOW_OIL;
        if (!isnan(oil_sw) && oil_sw > 0.5f && x[AI_F_RPM] > 1500) return AI_CLASS_LOW_OIL;
        if (x[AI_F_BLADE] > 0.5f && !isnan(etemp) && s.et_n >= ET_BASE_MIN_S &&
            etemp > s.et_base + s.prof->clog_rise_c)
            return AI_CLASS_SCREEN_CLOG;
        break;
    case MP_HEAD_NO_ECU:
        /* no engine temperature on these machines: a clogged screen is not observable */
        if (!isnan(oil_sw) && oil_sw > 0.5f && x[AI_F_RPM] > 1500) return AI_CLASS_LOW_OIL;
        break;
    case MP_HEAD_ELECTRIC:
        return AI_CLASS_NORMAL;
    }
    if (hyd_ok && x[AI_F_HYD] > 90) return AI_CLASS_HYD_OVERHEAT;
    return AI_CLASS_NORMAL;
}

bool ai_health_push(const float sample[AI_F_COUNT], const ai_aux_t *aux, ai_health_result_t *out)
{
    /* the liquid head is the only one with a trained model, and it needs a backend */
    const bool use_model = active_head() == MP_HEAD_LIQUID;
    if (use_model && !s.be)
        return false;
    float clean[AI_N_FEAT];
    bool complete = true;
    for (int f = 0; f < AI_N_FEAT; f++) {
        if (!s.expected[f]) {
            clean[f] = AI_MEAN[f]; /* not fitted on this machine: neutral value, normalises to 0 */
        } else if (isnan(sample[f])) {
            if (!s.have_valid[f]) complete = false;
            clean[f] = s.last_valid[f]; /* hold last valid value across short dropouts */
        } else {
            clean[f] = s.last_valid[f] = sample[f];
            s.have_valid[f] = true;
        }
    }
    if (!complete)
        return false;

    /* air-cooled: learn this machine's normal oil temperature while mowing, but freeze it while a
     * clog alert is active so a slowly clogging screen does not become the new normal */
    if (active_head() == MP_HEAD_AIR_EFI && aux && !isnan(aux->engine_temp_c) && aux->engine_temp_c > ET_WARM_C &&
        clean[AI_F_BLADE] > 0.5f && !(s.alert && s.last_fault == AI_CLASS_SCREEN_CLOG)) {
        if (s.et_n == 0)
            s.et_base = aux->engine_temp_c;
        else
            s.et_base += (aux->engine_temp_c - s.et_base) / ET_BASE_TAU_S;
        s.et_n++;
    }

    memcpy(s.ring[s.head], clean, sizeof(clean));
    s.head = (s.head + 1) % AI_WINDOW;
    if (s.count < AI_WINDOW) s.count++;
    if (++s.since < PERIOD_S || s.count < AI_WINDOW)
        return false;
    s.since = 0;

    for (uint32_t i = 0; i < AI_WINDOW; i++) {
        const float *row = s.ring[(s.head + i) % AI_WINDOW]; /* oldest first */
        for (int f = 0; f < AI_N_FEAT; f++)
            s.window[i * AI_N_FEAT + f] = (row[f] - AI_MEAN[f]) / AI_STD[f];
    }

    float probs[AI_N_CLASS] = { 1.0f, 0.0f, 0.0f, 0.0f };
    ai_class_t m = AI_CLASS_NORMAL;
    if (use_model) {
        if (s.be->infer(s.window, probs) != PAL_OK)
            return false;
        s.inferences++;
        for (int c = 1; c < AI_N_CLASS; c++)
            if (probs[c] > probs[m]) m = (ai_class_t)c;
    }
    ai_class_t r = rules(clean, aux);

    /* hybrid: physics rules own screen clog and hydraulic overheat; the model owns low oil */
    ai_class_t h = AI_CLASS_NORMAL;
    if (m == AI_CLASS_LOW_OIL || r == AI_CLASS_LOW_OIL) h = AI_CLASS_LOW_OIL;
    if (r == AI_CLASS_SCREEN_CLOG || r == AI_CLASS_HYD_OVERHEAT) h = r;

    bool new_alert = false;
    if (h != AI_CLASS_NORMAL && h == s.last_fault) {
        if (++s.repeats >= ALERT_REPEATS && !s.alert) {
            s.alert = true;
            new_alert = true;
        }
    } else {
        s.repeats = (h != AI_CLASS_NORMAL) ? 1 : 0;
        if (h == AI_CLASS_NORMAL) s.alert = false;
    }
    s.last_fault = h;

    if (out) {
        out->cls = h;
        out->model_cls = m;
        out->rule_cls = r;
        out->prob = use_model ? probs[m] : 0.0f;
        out->alert = s.alert;
        out->new_alert = new_alert;
        out->inferences = s.inferences;
        out->model_ran = use_model;
        out->head = active_head();
    }
    return true;
}
