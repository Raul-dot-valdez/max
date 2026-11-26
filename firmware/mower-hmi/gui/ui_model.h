/*
 * GUI bridge — the ONLY interface between application logic and whichever
 * GUI framework is linked (TouchGFX or Qt for MCUs).
 *
 * Why: TouchGFX is free but licensed for STM32 only. Keeping every screen's
 * data in this plain-C, display-ready struct means a move to NXP/Renesas/
 * Infineon swaps the GUI backend (Qt for MCUs runs on all of them) without
 * touching a line of vehicle logic, and the app never calls GUI APIs.
 *
 * Values are pre-scaled integers so neither backend does float formatting.
 */
#ifndef UI_MODEL_H
#define UI_MODEL_H

#include "pal_types.h"

#define UI_MAX_DTC 8

typedef struct {
    uint32_t spn;
    uint8_t  fmi, oc, sa;
} ui_dtc_t;

typedef enum { UI_VAL_OK = 0, UI_VAL_NA = 1 } ui_validity_t;

typedef struct {
    /* engine */
    uint16_t rpm;               uint8_t rpm_na;
    int16_t  coolant_c;         uint8_t coolant_na;
    uint16_t oil_kpa;           uint8_t oil_na;
    uint16_t battery_v_x10;     uint8_t battery_na;
    int16_t  hyd_c;             uint8_t hyd_na;
    uint8_t  load_pct;
    /* fuel & runtime */
    uint8_t  fuel_pct;          uint8_t fuel_na;
    uint16_t fuel_l_x10;
    uint16_t burn_lph_x10;      uint8_t burn_na, burn_estimated;
    uint16_t runtime_left_min;  uint8_t runtime_na;
    uint8_t  low_fuel;
    uint32_t engine_h_x10;
    uint32_t blade_h_x10;
    int32_t  service_in_h_x10;
    /* machine */
    uint8_t  blade_on;
    uint16_t deck_mm;
    uint16_t speed_kmh_x10;
    /* GPS value features */
    uint8_t  gps_fix, gps_sats;
    int32_t  lat_e7, lon_e7;
    uint32_t job_area_m2;
    uint16_t productivity_m2_h;
    uint8_t  overlap_pct;
    uint8_t  outside_fence;
    int8_t   site;
    uint8_t  night_mode;
    /* diagnostics */
    uint8_t  dtc_count;
    ui_dtc_t dtc[UI_MAX_DTC];
    uint8_t  lamp_red, lamp_amber;
    uint8_t  anomaly_score, anomaly_alert, ml_learned_pct;
    /* connectivity / time */
    uint8_t  ble_connected;
    uint32_t ble_pair_code;     /* bit 31 set: show "Pair phone? code %06u" (bits 0..19) + Accept/Reject */
    uint32_t utc;
    int16_t  utc_offset_min;
} ui_model_t;

typedef enum {
    UI_CMD_NONE,
    UI_CMD_ACK_ALARM,
    UI_CMD_RESET_JOB,
    UI_CMD_SERVICE_DONE,
    UI_CMD_SET_BACKLIGHT,   /* arg: percent */
    UI_CMD_BLE_PAIR_ACCEPT, /* arg: 1 accept, 0 reject (numeric comparison) */
} ui_cmd_type_t;

typedef struct {
    ui_cmd_type_t type;
    int32_t       arg;
} ui_cmd_t;

typedef void (*ui_notify_fn_t)(const ui_model_t *m);

void ui_model_init(void);
/* App side */
void ui_model_publish(const ui_model_t *m);
bool ui_cmd_take(ui_cmd_t *out);
/* GUI side (pull: TouchGFX Model::tick) */
bool ui_model_fetch(ui_model_t *out, uint32_t *last_version);
/* GUI side (push: Qt for MCUs event queue). Called in the publisher's context. */
void ui_model_set_notify(ui_notify_fn_t fn);
void ui_cmd_post(ui_cmd_type_t type, int32_t arg);

#endif
