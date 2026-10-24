/*
 * On-device (TinyML-class) engine/hydraulic health monitoring.
 *
 * Stage 1 (this file, always on): an operating-point-conditioned baseline
 * that LEARNS THIS MACHINE on the machine. The rpm x load plane is split
 * into 4 x 4 bins; each bin keeps a running mean/variance of coolant temp,
 * oil pressure, fuel rate and hydraulic oil temp. After a bin has seen
 * enough warm-engine samples it starts scoring: the score is the largest
 * |z| across features. Model state is ~1 KB and persists in nvstore, so
 * learning survives key cycles. No cloud training needed, no labelled
 * fault data needed, and it adapts to engine/hydraulic wear slowly
 * (1-hour forgetting) without learning a fault (no update while alarmed).
 *
 * Stage 2 (optional, see docs): a fleet-trained int8 model (TFLite Micro /
 * CMSIS-NN on the M7) consuming the same feature vector, for failure modes
 * that need cross-machine data. It plugs in behind an_input_t.
 *
 * Also: blade-impact detection from EEC1 engine-speed transients while the
 * blade clutch is engaged (rock/stump strike -> inspect blade & spindle).
 */
#ifndef ANOMALY_H
#define ANOMALY_H

#include "pal_types.h"

#define AN_FEAT      4  /* coolant C, oil kPa, fuel L/h, hydraulic C */
#define AN_RPM_BINS  4
#define AN_LOAD_BINS 4
#define AN_BINS      (AN_RPM_BINS * AN_LOAD_BINS)

typedef struct PAL_PACKED {
    float    mean[AN_FEAT];
    float    var[AN_FEAT];  /* M2 accumulator while learning, variance after */
    uint32_t n;
} an_bin_t;

typedef struct PAL_PACKED {
    uint32_t magic;
    an_bin_t bin[AN_BINS];
} an_model_t;

#define AN_MODEL_MAGIC 0x414E4D31u /* "ANM1" */

typedef struct {
    float    rpm, load_pct;
    float    x[AN_FEAT];
    bool     valid[AN_FEAT];
    uint32_t engine_run_s; /* since start, gates out warm-up transients */
} an_input_t;

typedef struct {
    uint8_t score;       /* |z| x 32, saturating */
    bool    alert;
    int8_t  feature;     /* worst feature or -1 */
    uint8_t learned_pct; /* share of bins that are trained */
} an_result_t;

typedef void (*an_event_fn_t)(uint16_t code, int32_t a, int32_t b);

void an_init(an_model_t *model, an_event_fn_t ev); /* model may hold persisted data */
void an_update_1hz(const an_input_t *in, an_result_t *out);
bool an_blade_impact(float rpm, bool blade_on, uint32_t now_ms);

#endif
