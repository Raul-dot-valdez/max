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
} s;

static const char *k_names[] = { "normal", "screen_clog", "low_oil", "hyd_overheat" };

/* Builds without an AI backend link this default; ai_health then stays inactive. */
PAL_WEAK const ai_backend_t *ai_backend_get(void) { return NULL; }

const char *ai_class_name(ai_class_t c) { return (unsigned)c < 4 ? k_names[c] : "?"; }

pal_status_t ai_health_init(void)
{
    memset(&s, 0, sizeof(s));
    s.be = ai_backend_get();
    return (s.be && s.be->init) ? s.be->init() : PAL_ERR;
}

void ai_health_reset(void)
{
    s.head = s.count = s.since = 0;
    s.repeats = 0;
    s.last_fault = AI_CLASS_NORMAL;
}

/* Same thresholds as rules_baseline() in tools/edge_ai_poc/train_phase1.py */
static ai_class_t rules(const float *x)
{
    if (x[AI_F_RPM] > 2000 && x[AI_F_OILP] < 150) return AI_CLASS_LOW_OIL;
    if (x[AI_F_BLADE] > 0.5f && x[AI_F_COOLANT] > 95) return AI_CLASS_SCREEN_CLOG;
    if (x[AI_F_HYD] > 90) return AI_CLASS_HYD_OVERHEAT;
    return AI_CLASS_NORMAL;
}

bool ai_health_push(const float sample[AI_F_COUNT], ai_health_result_t *out)
{
    if (!s.be)
        return false;
    float clean[AI_N_FEAT];
    bool complete = true;
    for (int f = 0; f < AI_N_FEAT; f++) {
        if (isnan(sample[f])) {
            if (!s.have_valid[f]) complete = false;
            clean[f] = s.last_valid[f]; /* hold last valid value across short dropouts */
        } else {
            clean[f] = s.last_valid[f] = sample[f];
            s.have_valid[f] = true;
        }
    }
    if (!complete)
        return false;

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

    float probs[AI_N_CLASS];
    if (s.be->infer(s.window, probs) != PAL_OK)
        return false;
    s.inferences++;

    ai_class_t m = AI_CLASS_NORMAL;
    for (int c = 1; c < AI_N_CLASS; c++)
        if (probs[c] > probs[m]) m = (ai_class_t)c;
    ai_class_t r = rules(clean);

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
        out->prob = probs[m];
        out->alert = s.alert;
        out->new_alert = new_alert;
        out->inferences = s.inferences;
    }
    return true;
}
