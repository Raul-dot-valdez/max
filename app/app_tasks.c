/*
 * Application composition: owns task layout, inter-task queues and the
 * glue between services. Vendor-neutral (PAL + OSAL only).
 *
 *  task      prio  period        role
 *  wdg       7     500 ms        supervises heartbeats, kicks IWDG
 *  can       6     event/20 ms   J1939 stack, signal decode, blade impact
 *  gnss      5     event         NMEA, geofence, coverage, time sync
 *  ble       4     event/20 ms   PSoC link, fleet protocol, log streaming
 *  app       4     100 ms        fuel/runtime, ML, logging policy, UI model, power
 *  gui       3     vsync         TouchGFX / Qt for MCUs
 *  log       2     event         sole writer of the NOR data log
 */
#include "app_tasks.h"

#include <math.h>
#include <string.h>

#include "anomaly.h"
#include "board_config.h"
#include "datalog.h"
#include "fleet_ble.h"
#include "fuel_runtime.h"
#include "gps_services.h"
#include "gt911.h"
#include "j1939.h"
#include "j1939_signals.h"
#include "nmea.h"
#include "nvstore.h"
#include "osal.h"
#include "pal_can.h"
#include "pal_display.h"
#include "pal_flash.h"
#include "pal_gpio.h"
#include "pal_i2c.h"
#include "pal_sys.h"
#include "pal_uart.h"
#include "psoc_link.h"
#include "rtc.h"
#include "ui_model.h"
#include "vehicle_data.h"
#include "w25q.h"

#define PRIO_WDG   7
#define PRIO_CAN   6
#define PRIO_GNSS  5
#define PRIO_BLE   4
#define PRIO_APP   4
#define PRIO_GUI   3
#define PRIO_LOG   2

#define PERIODIC_LOG_RUN_S   10u
#define PERIODIC_LOG_IDLE_S  60u
#define SAVE_COUNTERS_S      360u
#define SAVE_MODEL_S         1800u
#define HOURS_REQ_MS         10000u
#define GNSS_STALE_MS        3000u
#define KEY_OFF_DEBOUNCE     5      /* x 100 ms */
#define SHUTDOWN_FLUSH_MS    2000u
#define BL_DAY_PCT           100u
#define BL_NIGHT_PCT         35u

typedef struct {
    uint8_t  type, len;
    uint32_t utc;
    uint8_t  p[DL_PAYLOAD_MAX];
} log_req_t;

typedef struct {
    uint8_t n;
    uint8_t b[31];
} chunk_t;

/* ---- storage & large buffers ------------------------------------------- */
static flash_dev_t nor, iflash;
static rtc_dev_t   rtc;
static bool        rtc_ok, nor_ok;
static uint32_t    dl_index[BOARD_NOR_LOG_SIZE / 4096u] PAL_SDRAM_BSS;
static uint8_t     cov_grid[COV_GRID_BYTES] PAL_SDRAM_BSS;

/* ---- persistent state (guarded by mtx_app) ------------------------------- */
static fr_counters_t counters;
static fr_cfg_t      fuel_cfg;
static an_model_t    an_model;
static an_result_t   an_res;
static float         deck_width_m = APP_DECK_WIDTH_M;
static uint32_t      log_acked;
static char          machine_id[18] = "UNSET";

/* ---- sync ---------------------------------------------------------------- */
static osal_queue_t q_can, q_gnss, q_link, q_log;
static osal_mutex_t mtx_app;   /* fuel/runtime, ML, counters, config */
static osal_mutex_t mtx_gps;   /* gps_services state */
static osal_mutex_t mtx_fix;   /* latest GNSS fix */
static gnss_fix_t   g_fix;
static uint32_t     g_fix_ms;
static volatile uint32_t hb_bits;
static volatile bool     ble_connected;
static volatile uint32_t ble_pair_code;      /* bit 31 = request pending, bits 0..19 = code (000000 is valid) */
static volatile int8_t   ble_pair_reply = -1; /* set by app task, sent by ble task */
static volatile uint8_t  backlight_user = 0; /* 0 = automatic day/night */

/* ---- time: utc = uptime_s + offset; both 32-bit, so reads are atomic ---- */
static volatile uint32_t uptime_s;
static volatile int32_t  utc_offset;
static volatile bool     utc_valid;
static uint32_t          last_rtc_write_s;

/* ---- trip ---------------------------------------------------------------- */
static struct {
    uint32_t start_utc, key_s0, engine_s0, blade_s0;
    float    fuel0, dist0;
    float    max_coolant, max_hyd;
    uint8_t  dtc_events, anomaly_events;
} trip;

/* ======================================================================== */

void app_heartbeat(uint32_t bit) { hb_bits |= bit; }

uint32_t app_utc(void) { return utc_valid ? (uint32_t)((int32_t)uptime_s + utc_offset) : 0; }

static void app_set_utc(uint32_t utc, bool write_rtc)
{
    uint32_t old = app_utc();
    utc_offset = (int32_t)(utc - uptime_s);
    utc_valid = true;
    if (write_rtc && rtc_ok && (uptime_s - last_rtc_write_s > 3600u || old == 0 ||
                                (old > utc ? old - utc : utc - old) > 2u)) {
        rtc.set(&rtc, utc);
        last_rtc_write_s = uptime_s;
    }
    if (old && (old > utc ? old - utc : utc - old) > 2u)
        app_log_event(EV_TIME_SET, (int32_t)old, (int32_t)utc);
}

static void log_post(uint8_t type, const void *p, size_t len)
{
    log_req_t r = { .type = type, .len = (uint8_t)len, .utc = app_utc() };
    memcpy(r.p, p, len);
    osal_queue_send(q_log, &r, 0); /* never block a producer; a full queue drops */
}

void app_log_event(uint16_t code, int32_t a, int32_t b)
{
    dl_event_t e = { .code = code, .a = a, .b = b };
    osal_mutex_lock(mtx_fix);
    if (g_fix.valid) {
        e.lat_e7 = g_fix.lat_e7;
        e.lon_e7 = g_fix.lon_e7;
    }
    osal_mutex_unlock(mtx_fix);
    if (code == EV_ANOMALY) trip.anomaly_events++;
    log_post(DL_T_EVENT, &e, sizeof(e));
}

/* ======================================================================== */
/* Internal-flash adapter for nvstore                                        */

static pal_status_t if_read(flash_dev_t *d, uint32_t a, void *b, size_t n) { (void)d; return pal_flash_read(a, b, n); }
static pal_status_t if_prog(flash_dev_t *d, uint32_t a, const void *b, size_t n) { (void)d; return pal_flash_program(a, b, n); }
static pal_status_t if_erase(flash_dev_t *d, uint32_t a) { (void)d; return pal_flash_erase_sector(a); }

/* ======================================================================== */
/* CAN / J1939                                                                */

static void can_rx_isr(const pal_can_frame_t *f, void *ctx)
{
    (void)ctx;
    osal_queue_send_isr(q_can, f);
}

static pal_status_t can_tx(const pal_can_frame_t *f) { return pal_can_send(BOARD_CAN_VEHICLE, f); }

static bool on_j1939_request(uint32_t pgn, uint8_t requester, void *ctx)
{
    (void)pgn; (void)requester; (void)ctx;
    return false; /* we publish nothing on request yet; destination-specific requests get NACKed */
}

static void on_j1939(const j1939_msg_t *m, void *ctx)
{
    j1939_signals_on_rx(m, ctx);
    if (m->pgn == PGN_EEC1) {
        float rpm, blade;
        if (vd_get(VD_ENGINE_RPM, &rpm, m->ts_ms)) {
            bool b = vd_get(VD_BLADE_ENGAGED, &blade, m->ts_ms) && blade > 0.5f;
            osal_mutex_lock(mtx_app);
            an_blade_impact(rpm, b, m->ts_ms);
            osal_mutex_unlock(mtx_app);
        }
    }
}

static void task_can(void *arg)
{
    (void)arg;
    uint32_t last_req = 0;
    bool busoff_logged = false;
    uint32_t uid = pal_sys_uid32();
    j1939_cfg_t cfg = {
        .name = j1939_make_name(uid, BOARD_J1939_MFG_CODE, 0, 0, BOARD_J1939_FUNCTION, 0, 0,
                                BOARD_J1939_INDUSTRY, BOARD_J1939_ARBITRARY),
        .preferred_sa = BOARD_J1939_PREFERRED_SA,
        .arbitrary_capable = BOARD_J1939_ARBITRARY,
        .tx = can_tx,
        .on_rx = on_j1939,
        .on_request = on_j1939_request,
    };
    j1939_init(&cfg, osal_now_ms());

    for (;;) {
        pal_can_frame_t f;
        if (osal_queue_recv(q_can, &f, 20)) {
            f.ts_ms = osal_now_ms(); /* one clock domain for all timeouts */
            j1939_on_can_frame(&f);
        }
        uint32_t now = osal_now_ms();
        j1939_tick(now);
        if (now - last_req >= HOURS_REQ_MS && j1939_ac_state() == J1939_AC_OK) {
            j1939_request(PGN_HOURS, J1939_ADDR_GLOBAL);
            last_req = now;
        }
        if (pal_can_state(BOARD_CAN_VEHICLE) == PAL_CAN_STATE_BUS_OFF) {
            if (!busoff_logged) app_log_event(EV_CAN_BUS_OFF, 0, 0);
            busoff_logged = true;
            pal_can_recover(BOARD_CAN_VEHICLE);
        } else {
            busoff_logged = false;
        }
        app_heartbeat(APP_HB_CAN);
    }
}

/* ======================================================================== */
/* UART chunking shared by GNSS and BLE                                       */

static void uart_to_queue(osal_queue_t q, const uint8_t *d, size_t n)
{
    while (n) {
        chunk_t c;
        c.n = (uint8_t)(n > sizeof(c.b) ? sizeof(c.b) : n);
        memcpy(c.b, d, c.n);
        osal_queue_send_isr(q, &c);
        d += c.n;
        n -= c.n;
    }
}

static void gnss_rx_isr(const uint8_t *d, size_t n, void *ctx) { (void)ctx; uart_to_queue(q_gnss, d, n); }
static void link_rx_isr(const uint8_t *d, size_t n, void *ctx) { (void)ctx; uart_to_queue(q_link, d, n); }

/* ======================================================================== */
/* GNSS                                                                        */

static void task_gnss(void *arg)
{
    (void)arg;
    static nmea_parser_t p;
    nmea_init(&p);
    for (;;) {
        chunk_t c;
        if (osal_queue_recv(q_gnss, &c, 500)) {
            for (uint8_t i = 0; i < c.n; i++) {
                gnss_fix_t fx;
                if (!nmea_feed(&p, (char)c.b[i], &fx))
                    continue;
                uint32_t now = osal_now_ms();
                osal_mutex_lock(mtx_fix);
                g_fix = fx;
                g_fix_ms = now;
                osal_mutex_unlock(mtx_fix);

                float rpm = 0, blade = 0;
                bool engine = vd_get(VD_ENGINE_RPM, &rpm, now) && rpm > 400.0f;
                bool blade_on = vd_get(VD_BLADE_ENGAGED, &blade, now) && blade > 0.5f;
                osal_mutex_lock(mtx_gps);
                gps_svc_update(&fx, blade_on, engine, now);
                osal_mutex_unlock(mtx_gps);

                /* GNSS is the time authority when it has a fix */
                if (fx.valid && fx.utc_valid) {
                    uint32_t u = app_utc();
                    if (!utc_valid || (u > fx.utc ? u - fx.utc : fx.utc - u) > 2u)
                        app_set_utc(fx.utc, true);
                }
            }
        }
        app_heartbeat(APP_HB_GNSS);
    }
}

/* ======================================================================== */
/* BLE co-processor + fleet protocol                                          */

static uint8_t link_seq;

static pal_status_t link_send(uint8_t type, const uint8_t *d, size_t n)
{
    static link_frame_t f;
    static uint8_t enc[LINK_MAX_ENCODED];
    if (n > LINK_MAX_PAYLOAD) return PAL_EINVAL;
    f.type = type;
    f.seq = link_seq++;
    f.len = (uint16_t)n;
    memcpy(f.payload, d, n);
    size_t e = link_encode(&f, enc, sizeof(enc));
    return e ? pal_uart_write(BOARD_UART_BLE, enc, e, 50) : PAL_ERR;
}

static pal_status_t fb_send(const uint8_t *d, size_t n) { return link_send(LINK_T_DATA_TO_APP, d, n); }

static void fb_get_info(fb_info_t *o)
{
    o->proto = FB_PROTO_VERSION;
    o->fw[0] = FW_VERSION_MAJOR; o->fw[1] = FW_VERSION_MINOR; o->fw[2] = FW_VERSION_PATCH;
    o->hw_uid = pal_sys_uid32();
    memcpy(o->machine_id, machine_id, sizeof(o->machine_id));
    o->log_oldest = dl_oldest_seq();
    o->log_next = dl_next_seq();
    o->log_acked = log_acked;
    o->j1939_sa = j1939_address();
}

static void fb_get_summary(fb_summary_t *o)
{
    fr_state_t fs;
    gps_stats_t gs;
    vd_snapshot_t v;
    gnss_fix_t fx;
    uint32_t now = osal_now_ms();
    vd_snapshot(&v, now);
    osal_mutex_lock(mtx_app);
    fr_get(&fs);
    an_result_t an = an_res;
    osal_mutex_unlock(mtx_app);
    osal_mutex_lock(mtx_gps);
    gps_svc_get(&gs);
    osal_mutex_unlock(mtx_gps);
    osal_mutex_lock(mtx_fix);
    fx = g_fix;
    osal_mutex_unlock(mtx_fix);

    o->utc = app_utc();
    o->engine_h_x10 = (uint32_t)(fs.engine_hours * 10.0f);
    o->blade_h_x10 = (uint32_t)(fs.blade_hours * 10.0f);
    o->service_in_h_x10 = (int32_t)(fs.hours_to_service * 10.0f);
    o->fuel_pct_x2 = fs.level_valid ? (uint8_t)(fs.level_pct * 2.0f) : 0xFF;
    o->fuel_l_x10 = (uint16_t)(fs.remaining_l * 10.0f);
    o->burn_lph_x10 = fs.burn_valid ? (uint16_t)(fs.burn_lph * 10.0f) : 0xFFFF;
    o->runtime_left_min = fs.runtime_valid ? (uint16_t)(fs.runtime_left_h * 60.0f) : 0xFFFF;
    o->lat_e7 = fx.lat_e7;
    o->lon_e7 = fx.lon_e7;
    o->gps_fix = fx.valid;
    o->dtc_count = v.dtc_count;
    o->lamps = v.lamps;
    o->anomaly_score = an.score;
    o->job_area_m2 = (uint32_t)gs.job_area_m2;
    o->productivity_m2_h = (uint16_t)(gs.productivity_m2_h > 65535 ? 65535 : gs.productivity_m2_h);
    o->flags = (uint8_t)((fs.engine_running ? DL_F_ENGINE_ON : 0) | DL_F_KEY_ON |
                         (fs.low_fuel ? DL_F_LOW_FUEL : 0) | (an.alert ? DL_F_ANOMALY : 0) |
                         (gs.outside_keep_in ? DL_F_OUT_FENCE : 0) | (fx.valid ? DL_F_GPS_FIX : 0));
}

static void fb_set_time(uint32_t utc) { app_set_utc(utc, true); }

static bool fb_set_param(uint16_t id, int32_t v)
{
    bool ok = true;
    osal_mutex_lock(mtx_app);
    switch (id) {
    case FB_PARAM_TANK_L_X10:
        if (v < 50 || v > 5000) { ok = false; break; }
        fuel_cfg.tank_l = v / 10.0f;
        break;
    case FB_PARAM_LOW_FUEL_PCT:
        if (v < 5 || v > 50) { ok = false; break; }
        fuel_cfg.low_fuel_pct = (float)v;
        break;
    case FB_PARAM_SERVICE_INTERVAL_H:
        if (v < 10 || v > 2000) { ok = false; break; }
        fuel_cfg.service_interval_h = (float)v;
        break;
    case FB_PARAM_SERVICE_DONE:
        fr_service_done();
        nv_set(NV_KEY_COUNTERS, &counters, sizeof(counters));
        break;
    case FB_PARAM_DECK_WIDTH_MM:
        if (v < 500 || v > 4000) { ok = false; break; }
        deck_width_m = v / 1000.0f;
        osal_mutex_lock(mtx_gps);
        gps_svc_set_deck_width(deck_width_m);
        osal_mutex_unlock(mtx_gps);
        break;
    case FB_PARAM_RESET_JOB:
        osal_mutex_lock(mtx_gps);
        gps_svc_reset_job();
        osal_mutex_unlock(mtx_gps);
        break;
    default:
        ok = false;
    }
    if (ok) {
        fr_set_cfg(&fuel_cfg);
        nv_set(NV_KEY_FUEL_CFG, &fuel_cfg, sizeof(fuel_cfg));
    }
    osal_mutex_unlock(mtx_app);
    return ok;
}

static bool fb_set_geofence(uint8_t slot, const geofence_t *g)
{
    osal_mutex_lock(mtx_gps);
    bool ok = gps_svc_set_fence(slot, g);
    osal_mutex_unlock(mtx_gps);
    if (ok)
        nv_set_blob((uint16_t)(NV_KEY_GEOFENCE_BASE + slot * 0x10u), g, sizeof(*g));
    return ok;
}

static void fb_ack_log(uint32_t seq)
{
    if (seq >= dl_next_seq()) return;
    log_acked = seq;
    nv_set(NV_KEY_LOG_ACK_SEQ, &log_acked, sizeof(log_acked));
    app_log_event(EV_BLE_SYNC, (int32_t)seq, 0);
}

static const fleet_ble_ops_t fb_ops = {
    .get_info = fb_get_info,
    .get_summary = fb_get_summary,
    .set_time = fb_set_time,
    .set_param = fb_set_param,
    .set_geofence = fb_set_geofence,
    .ack_log = fb_ack_log,
    .send = fb_send,
};

static void task_ble(void *arg)
{
    (void)arg;
    static link_decoder_t dec;
    static link_frame_t fr;
    link_decoder_init(&dec);
    fleet_ble_init(&fb_ops);

    pal_gpio_write(PAL_PIN_BLE_RESET, false);
    osal_sleep_ms(5);
    pal_gpio_write(PAL_PIN_BLE_RESET, true);
    osal_sleep_ms(200);
    link_send(LINK_T_ADV_CONFIG, (const uint8_t *)machine_id, strnlen(machine_id, sizeof(machine_id)));

    for (;;) {
        chunk_t c;
        if (osal_queue_recv(q_link, &c, 20)) {
            for (uint8_t i = 0; i < c.n; i++) {
                if (!link_decode_byte(&dec, c.b[i], &fr))
                    continue;
                switch (fr.type) {
                case LINK_T_STATUS:
                    if (fr.len >= 2) {
                        ble_connected = fr.payload[0];
                        fleet_ble_on_status(fr.payload[0], fr.payload[1]);
                    }
                    break;
                case LINK_T_CREDIT:
                    if (fr.len >= 1) fleet_ble_on_credit(fr.payload[0]);
                    break;
                case LINK_T_DATA_FROM_APP:
                    fleet_ble_on_rx(fr.payload, fr.len);
                    break;
                case LINK_T_PING:
                    link_send(LINK_T_PING, fr.payload, fr.len);
                    break;
                case LINK_T_PAIR_CONFIRM:
                    if (fr.len >= 4)
                        ble_pair_code = 0x80000000u | fr.payload[0] | (fr.payload[1] << 8) |
                                        ((uint32_t)fr.payload[2] << 16);
                    break;
                default:
                    break;
                }
            }
        }
        if (ble_pair_reply >= 0) {
            uint8_t a = (uint8_t)ble_pair_reply;
            ble_pair_reply = -1;
            ble_pair_code = 0;
            link_send(LINK_T_PAIR_REPLY, &a, 1);
        }
        fleet_ble_poll();
        app_heartbeat(APP_HB_BLE);
    }
}

/* ======================================================================== */
/* Log writer                                                                  */

static void task_log(void *arg)
{
    (void)arg;
    for (;;) {
        log_req_t r;
        if (osal_queue_recv(q_log, &r, 500) && nor_ok)
            dl_append(r.type, r.utc, r.p, r.len);
        app_heartbeat(APP_HB_LOG);
    }
}

/* ======================================================================== */
/* App: policy, estimation, UI model, power                                    */

static void log_dtc_changes(const vd_snapshot_t *v)
{
    static vd_dtc_t prev[VD_MAX_DTC];
    static uint8_t nprev;
    static uint32_t gen = 0xFFFFFFFFu;
    if (v->dtc_generation == gen) return;
    gen = v->dtc_generation;
    for (int pass = 0; pass < 2; pass++) {
        const vd_dtc_t *a = pass ? prev : v->dtc, *b = pass ? v->dtc : prev;
        uint8_t na = pass ? nprev : v->dtc_count, nb = pass ? v->dtc_count : nprev;
        for (uint8_t i = 0; i < na; i++) {
            bool found = false;
            for (uint8_t k = 0; k < nb && !found; k++)
                found = a[i].sa == b[k].sa && a[i].spn == b[k].spn && a[i].fmi == b[k].fmi;
            if (found) continue;
            dl_dtc_t d = { .sa = a[i].sa, .spn = a[i].spn, .fmi = a[i].fmi, .oc = a[i].oc,
                           .active = (uint8_t)(pass == 0) };
            log_post(DL_T_DTC, &d, sizeof(d));
            trip.dtc_events++;
        }
    }
    memcpy(prev, v->dtc, sizeof(prev));
    nprev = v->dtc_count;
}

static void log_periodic(const vd_snapshot_t *v, const fr_state_t *fs, const gps_stats_t *gs,
                         const gnss_fix_t *fx, bool blade)
{
    dl_periodic_t p;
    memset(&p, 0, sizeof(p));
    p.rpm = (uint16_t)v->value[VD_ENGINE_RPM];
    p.load_pct = (uint8_t)v->value[VD_ENGINE_LOAD_PCT];
    p.coolant_c = (int8_t)v->value[VD_COOLANT_C];
    p.oil_kpa = (uint16_t)v->value[VD_OIL_PRESS_KPA];
    p.fuel_pct_x2 = fs->level_valid ? (uint8_t)(fs->level_pct * 2.0f) : 0xFF;
    p.fuel_rate_dlph = fs->burn_valid ? (uint16_t)(fs->burn_lph * 10.0f) : 0xFFFF;
    p.engine_hours_x20 = (uint32_t)(fs->engine_hours * 20.0f);
    p.lat_e7 = fx->lat_e7;
    p.lon_e7 = fx->lon_e7;
    p.speed_kmh_x10 = fx->speed_kmh_x10;
    p.hyd_c = (int8_t)v->value[VD_HYD_OIL_C];
    p.battery_v_x10 = (uint8_t)(v->value[VD_BATTERY_V] * 10.0f);
    p.flags = (uint8_t)(DL_F_KEY_ON | (fs->engine_running ? DL_F_ENGINE_ON : 0) |
                        (blade ? DL_F_BLADE_ON : 0) | (fx->valid ? DL_F_GPS_FIX : 0) |
                        (fs->low_fuel ? DL_F_LOW_FUEL : 0) | (an_res.alert ? DL_F_ANOMALY : 0) |
                        (gs->outside_keep_in ? DL_F_OUT_FENCE : 0));
    p.anomaly_score = an_res.score;
    p.job_area_m2 = (uint32_t)gs->job_area_m2;
    p.runtime_left_min = fs->runtime_valid ? (uint16_t)(fs->runtime_left_h * 60.0f) : 0xFFFF;
    p.sats = fx->sats;
    p.hdop_x10 = (uint8_t)(fx->hdop_x10 > 255 ? 255 : fx->hdop_x10);
    log_post(DL_T_PERIODIC, &p, sizeof(p));
}

static void build_ui(ui_model_t *m, const vd_snapshot_t *v, const fr_state_t *fs,
                     const gps_stats_t *gs, const gnss_fix_t *fx)
{
    memset(m, 0, sizeof(*m));
    m->rpm = (uint16_t)v->value[VD_ENGINE_RPM];           m->rpm_na = !v->valid[VD_ENGINE_RPM];
    m->coolant_c = (int16_t)v->value[VD_COOLANT_C];       m->coolant_na = !v->valid[VD_COOLANT_C];
    m->oil_kpa = (uint16_t)v->value[VD_OIL_PRESS_KPA];    m->oil_na = !v->valid[VD_OIL_PRESS_KPA];
    m->battery_v_x10 = (uint16_t)(v->value[VD_BATTERY_V] * 10.0f); m->battery_na = !v->valid[VD_BATTERY_V];
    m->hyd_c = (int16_t)v->value[VD_HYD_OIL_C];           m->hyd_na = !v->valid[VD_HYD_OIL_C];
    m->load_pct = (uint8_t)v->value[VD_ENGINE_LOAD_PCT];

    m->fuel_pct = (uint8_t)(fs->level_pct + 0.5f);        m->fuel_na = !fs->level_valid;
    m->fuel_l_x10 = (uint16_t)(fs->remaining_l * 10.0f);
    m->burn_lph_x10 = (uint16_t)(fs->burn_lph * 10.0f);   m->burn_na = !fs->burn_valid;
    m->burn_estimated = fs->burn_from_level;
    m->runtime_left_min = (uint16_t)(fs->runtime_left_h * 60.0f); m->runtime_na = !fs->runtime_valid;
    m->low_fuel = fs->low_fuel;
    m->engine_h_x10 = (uint32_t)(fs->engine_hours * 10.0f);
    m->blade_h_x10 = (uint32_t)(fs->blade_hours * 10.0f);
    m->service_in_h_x10 = (int32_t)(fs->hours_to_service * 10.0f);

    m->blade_on = v->valid[VD_BLADE_ENGAGED] && v->value[VD_BLADE_ENGAGED] > 0.5f;
    m->deck_mm = (uint16_t)v->value[VD_DECK_HEIGHT_MM];
    m->speed_kmh_x10 = v->valid[VD_WHEEL_SPEED_KMH] ? (uint16_t)(v->value[VD_WHEEL_SPEED_KMH] * 10.0f)
                                                     : fx->speed_kmh_x10;

    m->gps_fix = fx->valid;
    m->gps_sats = fx->sats;
    m->lat_e7 = fx->lat_e7;
    m->lon_e7 = fx->lon_e7;
    m->job_area_m2 = (uint32_t)gs->job_area_m2;
    m->productivity_m2_h = (uint16_t)(gs->productivity_m2_h > 65535 ? 65535 : gs->productivity_m2_h);
    m->overlap_pct = gs->overlap_pct;
    m->outside_fence = gs->outside_keep_in;
    m->site = gs->current_site;
    m->night_mode = !gs->daylight;

    m->dtc_count = v->dtc_count;
    for (uint8_t i = 0; i < v->dtc_count && i < UI_MAX_DTC; i++)
        m->dtc[i] = (ui_dtc_t){ .spn = v->dtc[i].spn, .fmi = v->dtc[i].fmi, .oc = v->dtc[i].oc, .sa = v->dtc[i].sa };
    /* DM1 lamp byte: MIL(7:6) RSL(5:4) AWL(3:2) PL(1:0); 01 = on */
    m->lamp_red = ((v->lamps >> 4) & 3) == 1;
    m->lamp_amber = ((v->lamps >> 2) & 3) == 1 || ((v->lamps >> 6) & 3) == 1;
    m->anomaly_score = an_res.score;
    m->anomaly_alert = an_res.alert;
    m->ml_learned_pct = an_res.learned_pct;

    m->ble_connected = ble_connected;
    m->ble_pair_code = ble_pair_code;
    m->utc = app_utc();
}

static void trip_start(void)
{
    trip.start_utc = app_utc();
    trip.key_s0 = counters.key_s;
    trip.engine_s0 = counters.engine_s;
    trip.blade_s0 = counters.blade_s;
    trip.fuel0 = counters.fuel_used_l;
    trip.dist0 = 0;
    trip.max_coolant = trip.max_hyd = -40.0f;
}

static void shutdown_sequence(void)
{
    gps_stats_t gs;
    osal_mutex_lock(mtx_gps);
    gps_svc_get(&gs);
    osal_mutex_unlock(mtx_gps);

    osal_mutex_lock(mtx_app);
    fr_key_off();
    dl_trip_t t = {
        .start_utc = trip.start_utc,
        .key_s = counters.key_s - trip.key_s0,
        .engine_s = counters.engine_s - trip.engine_s0,
        .blade_s = counters.blade_s - trip.blade_s0,
        .fuel_used_ml = (uint32_t)((counters.fuel_used_l - trip.fuel0) * 1000.0f),
        .area_m2 = (uint32_t)gs.job_area_m2,
        .distance_m = (uint32_t)(gs.distance_m - trip.dist0),
        .max_coolant_c_p40 = (uint8_t)(trip.max_coolant + 40.0f),
        .max_hyd_c_p40 = (uint8_t)(trip.max_hyd + 40.0f),
        .dtc_events = trip.dtc_events,
        .anomaly_events = trip.anomaly_events,
    };
    nv_set(NV_KEY_COUNTERS, &counters, sizeof(counters));
    nv_set_blob(NV_KEY_ML_MODEL_BASE, &an_model, sizeof(an_model));
    osal_mutex_unlock(mtx_app);

    app_log_event(EV_KEY_OFF, 0, 0);
    log_post(DL_T_TRIP, &t, sizeof(t));

    uint32_t t0 = osal_now_ms();
    while (osal_queue_count(q_log) && osal_now_ms() - t0 < SHUTDOWN_FLUSH_MS)
        osal_sleep_ms(10);
    osal_sleep_ms(50); /* last record's program cycle */

    pal_display_backlight(0);
    pal_gpio_write(PAL_PIN_POWER_HOLD, false); /* supply drops here */

    /* Still alive: key turned back on during shutdown, or hold circuit stuck. */
    osal_sleep_ms(1000);
    pal_sys_reset();
}

static void task_app(void *arg)
{
    (void)arg;
    uint32_t last_ms = osal_now_ms(), acc_ms = 0, tick_s = 0, ms_since_1hz = 0;
    uint32_t last_periodic = 0, last_save = 0, last_model_save = 0, run_streak_s = 0;
    uint8_t key_low = 0;
    vd_snapshot_t v;
    fr_state_t fs;
    gps_stats_t gs;
    gnss_fix_t fx;

    trip_start();
    app_log_event(EV_KEY_ON, 0, 0);

    for (;;) {
        osal_sleep_ms(100);
        uint32_t now = osal_now_ms();
        uint32_t dt = now - last_ms; /* wrap-safe */
        last_ms = now;
        acc_ms += dt;
        while (acc_ms >= 1000) { acc_ms -= 1000; uptime_s++; }

        vd_snapshot(&v, now);
        osal_mutex_lock(mtx_fix);
        if (now - g_fix_ms > GNSS_STALE_MS) g_fix.valid = false;
        fx = g_fix;
        osal_mutex_unlock(mtx_fix);
        osal_mutex_lock(mtx_gps);
        gps_svc_get(&gs);
        osal_mutex_unlock(mtx_gps);

        bool key_on = pal_gpio_read(PAL_PIN_IGN_SENSE);
        bool blade = v.valid[VD_BLADE_ENGAGED] && v.value[VD_BLADE_ENGAGED] > 0.5f;

        ms_since_1hz += dt;
        if (ms_since_1hz >= 1000) {
            ms_since_1hz -= 1000;
            tick_s++;
            fr_input_t in = {
                .key_on = key_on,
                .rpm_valid = v.valid[VD_ENGINE_RPM],        .rpm = v.value[VD_ENGINE_RPM],
                .rate_valid = v.valid[VD_FUEL_RATE_LPH],    .rate_lph = v.value[VD_FUEL_RATE_LPH],
                .level_valid = v.valid[VD_FUEL_LEVEL_PCT],  .level_pct = v.value[VD_FUEL_LEVEL_PCT],
                .hours_valid = v.valid[VD_ENGINE_HOURS],    .ecu_hours = v.value[VD_ENGINE_HOURS],
                .blade_on = blade,
            };
            osal_mutex_lock(mtx_app);
            fr_update_1hz(&in);
            fr_get(&fs);
            run_streak_s = fs.engine_running ? run_streak_s + 1 : 0;
            an_input_t ai = {
                .rpm = v.value[VD_ENGINE_RPM], .load_pct = v.value[VD_ENGINE_LOAD_PCT],
                .x = { v.value[VD_COOLANT_C], v.value[VD_OIL_PRESS_KPA], v.value[VD_FUEL_RATE_LPH], v.value[VD_HYD_OIL_C] },
                .valid = { v.valid[VD_COOLANT_C], v.valid[VD_OIL_PRESS_KPA], v.valid[VD_FUEL_RATE_LPH], v.valid[VD_HYD_OIL_C] },
                .engine_run_s = run_streak_s,
            };
            if (fs.engine_running && v.valid[VD_ENGINE_LOAD_PCT])
                an_update_1hz(&ai, &an_res);
            if (tick_s - last_save >= SAVE_COUNTERS_S) {
                last_save = tick_s;
                nv_set(NV_KEY_COUNTERS, &counters, sizeof(counters));
            }
            if (tick_s - last_model_save >= SAVE_MODEL_S) {
                last_model_save = tick_s;
                nv_set_blob(NV_KEY_ML_MODEL_BASE, &an_model, sizeof(an_model));
            }
            osal_mutex_unlock(mtx_app);

            if (v.valid[VD_COOLANT_C] && v.value[VD_COOLANT_C] > trip.max_coolant) trip.max_coolant = v.value[VD_COOLANT_C];
            if (v.valid[VD_HYD_OIL_C] && v.value[VD_HYD_OIL_C] > trip.max_hyd) trip.max_hyd = v.value[VD_HYD_OIL_C];

            log_dtc_changes(&v);
            uint32_t period = fs.engine_running ? PERIODIC_LOG_RUN_S : PERIODIC_LOG_IDLE_S;
            if (key_on && tick_s - last_periodic >= period) {
                last_periodic = tick_s;
                log_periodic(&v, &fs, &gs, &fx, blade);
            }
        }

        /* operator commands from the GUI */
        ui_cmd_t cmd;
        while (ui_cmd_take(&cmd)) {
            switch (cmd.type) {
            case UI_CMD_RESET_JOB:
                osal_mutex_lock(mtx_gps); gps_svc_reset_job(); osal_mutex_unlock(mtx_gps);
                break;
            case UI_CMD_SERVICE_DONE:
                osal_mutex_lock(mtx_app);
                fr_service_done();
                nv_set(NV_KEY_COUNTERS, &counters, sizeof(counters));
                osal_mutex_unlock(mtx_app);
                break;
            case UI_CMD_BLE_PAIR_ACCEPT:
                if (ble_pair_code) ble_pair_reply = cmd.arg ? 1 : 0;
                break;
            case UI_CMD_SET_BACKLIGHT:
                backlight_user = (uint8_t)(cmd.arg < 5 ? 5 : (cmd.arg > 100 ? 100 : cmd.arg));
                nv_set(NV_KEY_BACKLIGHT, (const void *)&backlight_user, 1);
                break;
            default:
                break;
            }
        }

        pal_display_backlight(backlight_user ? backlight_user : (gs.daylight ? BL_DAY_PCT : BL_NIGHT_PCT));

        ui_model_t m;
        build_ui(&m, &v, &fs, &gs, &fx);
        ui_model_publish(&m);

        key_low = key_on ? 0 : (uint8_t)(key_low + 1);
        if (key_low >= KEY_OFF_DEBOUNCE)
            shutdown_sequence();

        app_heartbeat(APP_HB_APP);
    }
}

/* ======================================================================== */
/* Watchdog supervisor                                                         */

static void task_wdg(void *arg)
{
    (void)arg;
    for (;;) {
        osal_sleep_ms(500);
        if ((hb_bits & APP_HB_ALL) == APP_HB_ALL) {
            hb_bits = 0;
            pal_wdg_kick();
        }
        /* else: a task is stuck; IWDG (2 s) resets us, EV_BOOT logs the cause */
    }
}

/* Default GUI backend hooks when none is linked (headless bring-up builds) */
PAL_WEAK void gui_preload_assets(void) {}
PAL_WEAK void gui_task_entry(void *arg)
{
    (void)arg;
    for (;;) {
        osal_sleep_ms(100);
        app_heartbeat(APP_HB_GUI);
    }
}

/* ======================================================================== */

static const pal_panel_timing_t k_panel = {
    .width = BOARD_LCD_WIDTH, .height = BOARD_LCD_HEIGHT,
    .hsync = 1, .hbp = 46, .hfp = 210,
    .vsync = 1, .vbp = 23, .vfp = 22,
    .pixel_clock_hz = 33000000u,
};

static void load_persisted(void)
{
    if (nv_get(NV_KEY_COUNTERS, &counters, sizeof(counters)) != (int)sizeof(counters))
        memset(&counters, 0, sizeof(counters)); /* fr_init re-initialises on bad magic */
    fuel_cfg = (fr_cfg_t){ APP_TANK_L_DEFAULT, APP_LOW_FUEL_PCT, APP_RESERVE_PCT, APP_SERVICE_INTERVAL_H };
    nv_get(NV_KEY_FUEL_CFG, &fuel_cfg, sizeof(fuel_cfg));
    nv_get(NV_KEY_LOG_ACK_SEQ, &log_acked, sizeof(log_acked));
    nv_get(NV_KEY_MACHINE_ID, machine_id, sizeof(machine_id) - 1);
    uint8_t bl;
    if (nv_get(NV_KEY_BACKLIGHT, &bl, 1) == 1) backlight_user = bl;
    if (nv_get_blob(NV_KEY_ML_MODEL_BASE, &an_model, sizeof(an_model)) != PAL_OK)
        memset(&an_model, 0, sizeof(an_model));
}

void app_init(void)
{
    pal_gpio_init();
    pal_gpio_write(PAL_PIN_POWER_HOLD, true); /* latch supply first */
    pal_wdg_init(2000);

    q_can  = osal_queue_create(64, sizeof(pal_can_frame_t));
    q_gnss = osal_queue_create(32, sizeof(chunk_t));
    q_link = osal_queue_create(32, sizeof(chunk_t));
    q_log  = osal_queue_create(48, sizeof(log_req_t));
    mtx_app = osal_mutex_create();
    mtx_gps = osal_mutex_create();
    mtx_fix = osal_mutex_create();
    vd_init();
    ui_model_init();

    /* persistent config (internal flash) */
    iflash = (flash_dev_t){ .size = 0x200000u, .erase_size = BOARD_FLASH_SECTOR,
                            .prog_size = pal_flash_prog_unit(),
                            .read = if_read, .prog = if_prog, .erase = if_erase };
    nv_init(&iflash, BOARD_FLASH_NV0_ADDR, BOARD_FLASH_NV1_ADDR);
    load_persisted();
    fr_init(&fuel_cfg, &counters, app_log_event);
    an_init(&an_model, app_log_event);
    gps_svc_init(cov_grid, deck_width_m, app_log_event);
    for (uint8_t i = 0; i < GEOFENCE_SLOTS; i++) {
        geofence_t g;
        if (nv_get_blob((uint16_t)(NV_KEY_GEOFENCE_BASE + i * 0x10u), &g, sizeof(g)) == PAL_OK)
            gps_svc_set_fence(i, &g);
    }

    /* time */
    pal_i2c_init(BOARD_I2C_SYS, BOARD_I2C_HZ);
    rtc_ok = rv3028_init(&rtc, BOARD_I2C_SYS) == PAL_OK;
    uint32_t t;
    bool tv;
    if (rtc_ok && rtc.get(&rtc, &t, &tv) == PAL_OK && tv)
        app_set_utc(t, false);

    /* external NOR: GUI asset cache first (mapped), then hand the bus to the logger */
    nor_ok = w25q_init(&nor) == PAL_OK;
    if (nor_ok && w25q_mmap(true) == PAL_OK) {
        gui_preload_assets();
        w25q_mmap(false);
    }
    if (nor_ok)
        nor_ok = dl_init(&nor, BOARD_NOR_LOG_OFF, BOARD_NOR_LOG_SIZE, dl_index) == PAL_OK;

    /* display + touch */
    pal_gpio_write(PAL_PIN_LCD_RESET, true);
    pal_display_init(&k_panel, PAL_PIXFMT_RGB565, BOARD_FB0_ADDR);
    gt911_init(BOARD_I2C_SYS);

    /* vehicle CAN */
    pal_gpio_write(PAL_PIN_CAN_STBY, false);
    pal_can_init(BOARD_CAN_VEHICLE, BOARD_CAN_BITRATE);
    pal_can_add_filter_ext(BOARD_CAN_VEHICLE, 0, 0, 0); /* accept all extended IDs; J1939 filters in SW */
    pal_can_set_rx_callback(BOARD_CAN_VEHICLE, can_rx_isr, NULL);
    pal_can_start(BOARD_CAN_VEHICLE);

    /* GNSS + BLE co-processor */
    pal_gpio_write(PAL_PIN_GNSS_RESET, true);
    pal_uart_init(BOARD_UART_GNSS, BOARD_UART_GNSS_BAUD);
    pal_uart_set_rx_callback(BOARD_UART_GNSS, gnss_rx_isr, NULL);
    pal_uart_init(BOARD_UART_BLE, BOARD_UART_BLE_BAUD);
    pal_uart_set_rx_callback(BOARD_UART_BLE, link_rx_isr, NULL);

    app_log_event(EV_BOOT, pal_sys_reset_cause(), (int32_t)(nor_ok | (rtc_ok << 1)));
}

void app_start(void)
{
    osal_task_create(task_wdg,  "wdg",  1024, PRIO_WDG,  NULL);
    osal_task_create(task_can,  "can",  4096, PRIO_CAN,  NULL);
    osal_task_create(task_gnss, "gnss", 3072, PRIO_GNSS, NULL);
    osal_task_create(task_ble,  "ble",  4096, PRIO_BLE,  NULL);
    osal_task_create(task_app,  "app",  6144, PRIO_APP,  NULL);
    osal_task_create(gui_task_entry, "gui", 8192, PRIO_GUI, NULL);
    osal_task_create(task_log,  "log",  3072, PRIO_LOG,  NULL);
}
