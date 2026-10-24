#include "vehicle_data.h"

#include <string.h>

#include "osal.h"

/* Timeout = 3x the J1939 broadcast rate, per J1939-71 guidance. */
static const uint32_t k_timeout_ms[VD_COUNT] = {
    [VD_ENGINE_RPM]      = 300,   /* EEC1 20 ms (engine-speed dependent) */
    [VD_ENGINE_LOAD_PCT] = 300,   /* EEC2 50 ms */
    [VD_COOLANT_C]       = 3000,  /* ET1 1 s */
    [VD_OIL_PRESS_KPA]   = 3000,  /* EFL/P1 0.5 s */
    [VD_FUEL_RATE_LPH]   = 1000,  /* LFE1 100 ms */
    [VD_FUEL_LEVEL_PCT]  = 3000,  /* DD1 1 s */
    [VD_ENGINE_HOURS]    = 35000, /* HOURS on request, we ask every 10 s */
    [VD_BATTERY_V]       = 3000,  /* VEP1 1 s */
    [VD_WHEEL_SPEED_KMH] = 300,   /* CCVS1 100 ms */
    [VD_HYD_OIL_C]       = 3000,  /* VF 1 s */
    [VD_BLADE_ENGAGED]   = 1000,
    [VD_DECK_HEIGHT_MM]  = 3000,
};

#define DM1_TIMEOUT_MS 3500u /* DM1 is sent every 1 s; drop a source after 3 misses */
#define DM1_SOURCES    4

typedef struct {
    float    v;
    uint32_t ts;
    bool     present, ok;
} vd_sig_t;

typedef struct {
    bool     used;
    uint8_t  sa, lamps, n;
    uint32_t ts;
    vd_dtc_t dtc[VD_MAX_DTC];
} dm1_src_t;

static struct {
    osal_mutex_t lock;
    vd_sig_t     sig[VD_COUNT];
    dm1_src_t    src[DM1_SOURCES];
    uint32_t     generation;
} s;

void vd_init(void)
{
    memset(&s, 0, sizeof(s));
    s.lock = osal_mutex_create();
}

void vd_set(vd_id_t id, float v, uint32_t now_ms)
{
    osal_mutex_lock(s.lock);
    s.sig[id] = (vd_sig_t){ .v = v, .ts = now_ms, .present = true, .ok = true };
    osal_mutex_unlock(s.lock);
}

void vd_set_invalid(vd_id_t id, uint32_t now_ms)
{
    osal_mutex_lock(s.lock);
    s.sig[id].ts = now_ms;
    s.sig[id].present = true;
    s.sig[id].ok = false;
    osal_mutex_unlock(s.lock);
}

static bool fresh(const vd_sig_t *g, vd_id_t id, uint32_t now)
{
    return g->present && g->ok && (now - g->ts) <= k_timeout_ms[id];
}

bool vd_get(vd_id_t id, float *out, uint32_t now_ms)
{
    osal_mutex_lock(s.lock);
    bool ok = fresh(&s.sig[id], id, now_ms);
    if (ok)
        *out = s.sig[id].v;
    osal_mutex_unlock(s.lock);
    return ok;
}

static void dm1_expire(uint32_t now)
{
    for (int i = 0; i < DM1_SOURCES; i++)
        if (s.src[i].used && now - s.src[i].ts > DM1_TIMEOUT_MS) {
            s.src[i].used = false;
            s.generation++;
        }
}

void vd_snapshot(vd_snapshot_t *out, uint32_t now_ms)
{
    memset(out, 0, sizeof(*out));
    osal_mutex_lock(s.lock);
    for (int i = 0; i < VD_COUNT; i++) {
        out->valid[i] = fresh(&s.sig[i], (vd_id_t)i, now_ms);
        out->value[i] = out->valid[i] ? s.sig[i].v : 0.0f;
    }
    dm1_expire(now_ms);
    for (int i = 0; i < DM1_SOURCES; i++) {
        if (!s.src[i].used)
            continue;
        out->lamps |= s.src[i].lamps;
        for (int k = 0; k < s.src[i].n && out->dtc_count < VD_MAX_DTC; k++)
            out->dtc[out->dtc_count++] = s.src[i].dtc[k];
    }
    out->dtc_generation = s.generation;
    osal_mutex_unlock(s.lock);
}

void vd_set_dtcs(uint8_t sa, uint8_t lamps, const vd_dtc_t *dtc, uint8_t n, uint32_t now_ms)
{
    if (n > VD_MAX_DTC)
        n = VD_MAX_DTC;
    osal_mutex_lock(s.lock);
    dm1_src_t *slot = NULL;
    for (int i = 0; i < DM1_SOURCES && !slot; i++)
        if (s.src[i].used && s.src[i].sa == sa)
            slot = &s.src[i];
    for (int i = 0; i < DM1_SOURCES && !slot; i++)
        if (!s.src[i].used)
            slot = &s.src[i];
    if (slot) {
        bool changed = !slot->used || slot->n != n || slot->lamps != lamps ||
                       memcmp(slot->dtc, dtc, n * sizeof(*dtc)) != 0;
        slot->used = true;
        slot->sa = sa;
        slot->lamps = lamps;
        slot->n = n;
        slot->ts = now_ms;
        memcpy(slot->dtc, dtc, n * sizeof(*dtc));
        if (changed)
            s.generation++;
    }
    osal_mutex_unlock(s.lock);
}
