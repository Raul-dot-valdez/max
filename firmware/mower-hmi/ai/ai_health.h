/*
 * Machine-health classifier (edge-AI PoC phase 1), shared by the STM32H743 and GD32H759 paths.
 *
 * Every second the app pushes one sample of 9 J1939-derived features. Every 30 s, once 60 s
 * of history exist, the module:
 *   1. normalises the window with the training scaler (generated/ai_scaler.h)
 *   2. runs the linked backend (int8 1-D CNN)
 *   3. applies physics rules on the latest sample
 *   4. combines them: rules decide screen clog and hydraulic overheat, the model decides low oil
 *      (the hybrid scored best in phase 1)
 *   5. raises an alert after the same fault class is seen twice in a row (one minute)
 *
 * Advisory only. Not a safety function.
 */
#ifndef AI_HEALTH_H
#define AI_HEALTH_H

#include "pal_types.h"

typedef enum {
    AI_CLASS_NORMAL = 0,
    AI_CLASS_SCREEN_CLOG = 1,
    AI_CLASS_LOW_OIL = 2,
    AI_CLASS_HYD_OVERHEAT = 3,
} ai_class_t;

/* Feature order matches training: rpm, load %, coolant °C, oil kPa, fuel L/h,
 * hydraulic °C, ambient °C, wheel km/h, blade engaged (0/1). NaN = not available. */
enum { AI_F_RPM, AI_F_LOAD, AI_F_COOLANT, AI_F_OILP, AI_F_FUEL, AI_F_HYD, AI_F_AMB, AI_F_SPEED, AI_F_BLADE, AI_F_COUNT };

typedef struct {
    ai_class_t cls;          /* hybrid decision */
    ai_class_t model_cls;    /* what the model alone said */
    ai_class_t rule_cls;     /* what the rules alone said */
    float      prob;         /* model probability of model_cls */
    bool       alert;        /* debounced: true once a fault class repeats */
    bool       new_alert;    /* true only on the inference that raised the alert */
    uint32_t   inferences;
} ai_health_result_t;

pal_status_t ai_health_init(void);
/* Call at 1 Hz while the engine runs. Returns true when *out holds a new result. */
bool ai_health_push(const float sample[AI_F_COUNT], ai_health_result_t *out);
/* Call at key-off or engine stop: clears the window so stale history is never classified. */
void ai_health_reset(void);
const char *ai_class_name(ai_class_t c);

#endif
