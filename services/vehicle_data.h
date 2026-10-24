/*
 * Vehicle data broker: the single source of truth for decoded machine
 * signals. Producers (J1939 decoder) write; consumers (GUI bridge, logger,
 * fuel/runtime, ML) take consistent snapshots. Every signal carries its own
 * staleness timeout, so a cut CAN harness shows "--" on screen instead of
 * the last good value frozen forever.
 */
#ifndef VEHICLE_DATA_H
#define VEHICLE_DATA_H

#include "pal_types.h"

typedef enum {
    VD_ENGINE_RPM,
    VD_ENGINE_LOAD_PCT,
    VD_COOLANT_C,
    VD_OIL_PRESS_KPA,
    VD_FUEL_RATE_LPH,
    VD_FUEL_LEVEL_PCT,
    VD_ENGINE_HOURS,
    VD_BATTERY_V,
    VD_WHEEL_SPEED_KMH,
    VD_HYD_OIL_C,
    VD_BLADE_ENGAGED,     /* 0/1, OEM proprietary PGN */
    VD_DECK_HEIGHT_MM,    /* OEM proprietary PGN */
    VD_COUNT
} vd_id_t;

#define VD_MAX_DTC 16

typedef struct {
    uint8_t  sa;
    uint32_t spn;
    uint8_t  fmi;
    uint8_t  oc;
} vd_dtc_t;

typedef struct {
    float    value[VD_COUNT];
    bool     valid[VD_COUNT];      /* present and not stale at snapshot time */
    uint8_t  dtc_count;
    vd_dtc_t dtc[VD_MAX_DTC];
    uint8_t  lamps;                /* MIL|RSL|AWL|PL, 2 bits each, from DM1 byte 0 (OR of all sources) */
    uint32_t dtc_generation;       /* increments whenever the active DTC set changes */
} vd_snapshot_t;

void vd_init(void);
void vd_set(vd_id_t id, float v, uint32_t now_ms);
void vd_set_invalid(vd_id_t id, uint32_t now_ms); /* J1939 "error"/"not available" */
bool vd_get(vd_id_t id, float *out, uint32_t now_ms);
void vd_snapshot(vd_snapshot_t *out, uint32_t now_ms);
/* Replace the active DTC list reported by one source address. */
void vd_set_dtcs(uint8_t sa, uint8_t lamps, const vd_dtc_t *dtc, uint8_t n, uint32_t now_ms);

#endif
