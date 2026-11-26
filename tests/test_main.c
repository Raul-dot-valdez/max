/*
 * Host unit tests for all portable firmware modules.
 * Build & run:  make -C tests
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ai_backend.h"
#include "ai_health.h"
#include "anomaly.h"
#include "crc.h"
#include "datalog.h"
#include "flash_sim.h"
#include "fleet_ble.h"
#include "fuel_runtime.h"
#include "gps_services.h"
#include "j1939.h"
#include "j1939_signals.h"
#include "nmea.h"
#include "nvstore.h"
#include "psoc_link.h"
#include "vehicle_data.h"

extern uint32_t g_host_ms;

static int g_fail, g_checks;
#define CHECK(c) do { g_checks++; if (!(c)) { g_fail++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b, tol) CHECK(fabs((double)(a) - (double)(b)) <= (tol))

static uint32_t rng = 12345;
static float noise(float amp) /* uniform [-amp, amp] */
{
    rng = rng * 1103515245u + 12345u;
    return amp * (((rng >> 8) & 0xFFFF) / 32767.5f - 1.0f);
}

/* ---- event capture shared by services --------------------------------- */
static uint16_t ev_codes[64];
static int ev_n;
static void ev_cb(uint16_t code, int32_t a, int32_t b) { (void)a; (void)b; if (ev_n < 64) ev_codes[ev_n++] = code; }
static int ev_count(uint16_t code) { int n = 0; for (int i = 0; i < ev_n; i++) n += ev_codes[i] == code; return n; }

/* ======================================================================== */

static void test_crc(void)
{
    puts("crc");
    CHECK(crc16_ccitt("123456789", 9, 0xFFFF) == 0x29B1);
    CHECK(crc32_ieee("123456789", 9, 0) == 0xCBF43926u);
}

static void test_link(void)
{
    puts("psoc_link codec");
    static link_frame_t in, out;
    static uint8_t enc[LINK_MAX_ENCODED];
    link_decoder_t d;
    link_decoder_init(&d);

    in.type = LINK_T_DATA_TO_APP;
    in.seq = 7;
    in.len = LINK_MAX_PAYLOAD;
    for (unsigned i = 0; i < LINK_MAX_PAYLOAD; i++) in.payload[i] = (uint8_t)(i % 3 == 0 ? 0 : i); /* many zeros */
    size_t n = link_encode(&in, enc, sizeof(enc));
    CHECK(n > 0 && enc[n - 1] == 0);
    for (size_t i = 0; i + 1 < n; i++) CHECK(enc[i] != 0);
    bool got = false;
    for (size_t i = 0; i < n; i++) got |= link_decode_byte(&d, enc[i], &out);
    CHECK(got && out.type == in.type && out.seq == 7 && out.len == in.len);
    CHECK(memcmp(out.payload, in.payload, in.len) == 0);

    /* long non-zero run (>254) exercises COBS 0xFF blocks */
    memset(in.payload, 0xAB, LINK_MAX_PAYLOAD);
    n = link_encode(&in, enc, sizeof(enc));
    got = false;
    for (size_t i = 0; i < n; i++) got |= link_decode_byte(&d, enc[i], &out);
    CHECK(got && memcmp(out.payload, in.payload, in.len) == 0);

    /* corruption is rejected */
    enc[5] ^= 0x10;
    got = false;
    for (size_t i = 0; i < n; i++) got |= link_decode_byte(&d, enc[i], &out);
    CHECK(!got && d.crc_errors == 1);
}

/* ---- J1939 ---------------------------------------------------------------- */

static pal_can_frame_t tx_log[32];
static int tx_n;
static pal_status_t can_tx(const pal_can_frame_t *f) { if (tx_n < 32) tx_log[tx_n++] = *f; return PAL_OK; }

static void feed(uint8_t prio, uint32_t pgn, uint8_t da, uint8_t sa, const uint8_t *d, uint8_t len)
{
    pal_can_frame_t f = { .id = j1939_build_id(prio, pgn, da, sa), .ext = 1, .dlc = len, .ts_ms = g_host_ms };
    memcpy(f.data, d, len);
    j1939_on_can_frame(&f);
}

static void test_j1939(void)
{
    puts("j1939");
    uint32_t pgn; uint8_t prio, da, sa;
    j1939_parse_id(0x0CF00400u, &pgn, &prio, &da, &sa); /* EEC1 from engine */
    CHECK(pgn == PGN_EEC1 && prio == 3 && sa == 0 && da == 0xFF);
    CHECK(j1939_build_id(6, J1939_PGN_REQUEST, 0x00, 0x28) == 0x18EA0028u);
    j1939_parse_id(0x18EA0028u, &pgn, &prio, &da, &sa);
    CHECK(pgn == J1939_PGN_REQUEST && da == 0x00 && sa == 0x28);

    vd_init();
    g_host_ms = 1000;
    tx_n = 0;
    uint64_t name = j1939_make_name(1234, 0x7FF, 0, 0, 60, 0, 0, 2, true);
    j1939_cfg_t cfg = { .name = name, .preferred_sa = 0x28, .arbitrary_capable = true,
                        .tx = can_tx, .on_rx = j1939_signals_on_rx };
    j1939_init(&cfg, g_host_ms);
    CHECK(tx_n == 1 && (tx_log[0].id & 0xFF) == 0x28);
    CHECK(j1939_send(PGN_EEC1, 3, 0xFF, (const uint8_t *)"x", 1) == PAL_BUSY); /* not yet claimed */

    /* a node with a lower (higher-priority) NAME claims 0x28 -> we move to 128 */
    uint8_t other[8] = { 0 };
    tx_n = 0;
    feed(6, J1939_PGN_ADDR_CLM, 0xFF, 0x28, other, 8);
    CHECK(j1939_address() == 128 && tx_n == 1 && (tx_log[0].id & 0xFF) == 128);
    g_host_ms += 300;
    j1939_tick(g_host_ms);
    CHECK(j1939_ac_state() == J1939_AC_OK);

    /* a higher NAME contending for our address: we defend */
    uint8_t hi[8]; memset(hi, 0xFF, 8);
    tx_n = 0;
    feed(6, J1939_PGN_ADDR_CLM, 0xFF, 128, hi, 8);
    CHECK(j1939_address() == 128 && tx_n == 1);

    /* destination-specific request for an unsupported PGN -> NACK */
    tx_n = 0;
    uint8_t req[3] = { 0xE5, 0xFE, 0x00 };
    feed(6, J1939_PGN_REQUEST, 128, 0x00, req, 3);
    CHECK(tx_n == 1 && ((tx_log[0].id >> 8) & 0x3FF00) == J1939_PGN_ACK && tx_log[0].data[0] == 1);

    /* signals */
    uint8_t eec1[8] = { 0xFF, 0xFF, 0xFF, 0xE0, 0x2E, 0xFF, 0xFF, 0xFF }; /* 0x2EE0 * 0.125 = 1500 rpm */
    feed(3, PGN_EEC1, 0xFF, 0x00, eec1, 8);
    float v;
    CHECK(vd_get(VD_ENGINE_RPM, &v, g_host_ms) && v == 1500.0f);
    uint8_t dd1[8] = { 0xFF, 125, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; /* 125 * 0.4 = 50 % */
    feed(6, PGN_DD1, 0xFF, 0x00, dd1, 8);
    CHECK(vd_get(VD_FUEL_LEVEL_PCT, &v, g_host_ms) && v == 50.0f);
    uint8_t et1[8] = { 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; /* 0xFE = error */
    feed(6, PGN_ET1, 0xFF, 0x00, et1, 8);
    CHECK(!vd_get(VD_COOLANT_C, &v, g_host_ms));
    g_host_ms += 400;
    CHECK(!vd_get(VD_ENGINE_RPM, &v, g_host_ms)); /* stale after 300 ms */

    /* BAM DM1 with 2 DTCs (2 + 2*4 = 10 bytes -> 2 packets) */
    uint8_t cm[8] = { 32, 10, 0, 2, 0xFF, 0xCA, 0xFE, 0x00 };
    feed(7, J1939_PGN_TP_CM, 0xFF, 0x00, cm, 8);
    /* lamps 0x04 (amber); DTC1 = SPN 100 FMI 1 OC 3; DTC2 = SPN 520192 (0x7F000) FMI 7 OC 1,
     * which straddles the packet boundary (its first byte is the last byte of packet 1) */
    uint8_t dt1[8] = { 1, 0x04, 0xFF, 100, 0, 0x01, 3, 0x00 };
    uint8_t dt2[8] = { 2, 0xF0, 0xE7, 0x01, 0xFF, 0xFF, 0xFF, 0xFF };
    feed(7, J1939_PGN_TP_DT, 0xFF, 0x00, dt1, 8);
    feed(7, J1939_PGN_TP_DT, 0xFF, 0x00, dt2, 8);
    vd_snapshot_t snap;
    vd_snapshot(&snap, g_host_ms);
    CHECK(snap.dtc_count == 2 && snap.lamps == 0x04);
    CHECK(snap.dtc[0].spn == 100 && snap.dtc[0].fmi == 1 && snap.dtc[0].oc == 3);
    CHECK(snap.dtc[1].spn == 520192u && snap.dtc[1].fmi == 7 && snap.dtc[1].oc == 1);

    /* RTS/CTS to us: CTS goes out, then EOMA after the last packet */
    tx_n = 0;
    uint8_t rts[8] = { 16, 10, 0, 2, 0xFF, 0xCA, 0xFE, 0x00 };
    feed(7, J1939_PGN_TP_CM, 128, 0x03, rts, 8);
    CHECK(tx_n == 1 && tx_log[0].data[0] == 17 && tx_log[0].data[1] == 2 && tx_log[0].data[2] == 1);
    feed(7, J1939_PGN_TP_DT, 128, 0x03, dt1, 8);
    feed(7, J1939_PGN_TP_DT, 128, 0x03, dt2, 8);
    CHECK(tx_n == 2 && tx_log[1].data[0] == 19);

    /* RTS then silence -> T2 timeout abort */
    tx_n = 0;
    feed(7, J1939_PGN_TP_CM, 128, 0x05, rts, 8);
    g_host_ms += 1300;
    j1939_tick(g_host_ms);
    CHECK(tx_n == 2 && tx_log[1].data[0] == 255);
}

/* ---- NMEA ---------------------------------------------------------------- */

static void feed_nmea(nmea_parser_t *p, const char *body, gnss_fix_t *out, bool *got)
{
    uint8_t ck = 0;
    char line[128];
    for (const char *c = body; *c; c++) ck ^= (uint8_t)*c;
    snprintf(line, sizeof(line), "$%s*%02X\r\n", body, ck);
    for (char *c = line; *c; c++) *got |= nmea_feed(p, *c, out);
}

static void test_nmea(void)
{
    puts("nmea");
    nmea_parser_t p;
    gnss_fix_t fx;
    bool got = false;
    nmea_init(&p);
    feed_nmea(&p, "GNRMC,123519.00,A,4807.038,N,01131.000,E,022.4,084.4,240526,,,A", &fx, &got);
    CHECK(!got);
    feed_nmea(&p, "GNGGA,123519.00,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,", &fx, &got);
    CHECK(got && fx.valid);
    CHECK(fx.lat_e7 == 481173000);
    CHECK(fx.lon_e7 == 115166667);
    CHECK(fx.speed_kmh_x10 == 415 && fx.course_x10 == 844);
    CHECK(fx.sats == 8 && fx.hdop_x10 == 9 && fx.alt_cm == 54540);
    CHECK(fx.utc_valid && fx.utc == 1779626119u); /* 2026-05-24 12:35:19 UTC */

    /* southern / western hemisphere, bad checksum ignored */
    got = false;
    feed_nmea(&p, "GPRMC,000001.00,A,3351.000,S,15112.000,W,0.0,0.0,010126,,,A", &fx, &got);
    feed_nmea(&p, "GPGGA,000001.00,3351.000,S,15112.000,W,2,12,0.6,10.0,M,0,M,,", &fx, &got);
    CHECK(got && fx.lat_e7 == -338500000 && fx.lon_e7 == -1512000000 && fx.quality == 2);
    got = false;
    const char *bad = "$GPRMC,000002.00,A,3351.000,S,15112.000,W,0.0,0.0,010126,,,A*00\r\n";
    for (const char *c = bad; *c; c++) got |= nmea_feed(&p, *c, &fx);
    CHECK(!got);

    /* no fix */
    got = false;
    feed_nmea(&p, "GPRMC,000003.00,V,,,,,,,010126,,,N", &fx, &got);
    feed_nmea(&p, "GPGGA,000003.00,,,,,0,00,99.9,,,,,,", &fx, &got);
    CHECK(got && !fx.valid);
}

/* ---- datalog -------------------------------------------------------------- */

static void test_datalog(void)
{
    puts("datalog");
    flash_dev_t d;
    flash_sim_t s;
    static uint32_t idx[64];
    flash_sim_create(&d, &s, 16 * 4096, 4096, 256);
    const uint32_t off = 4096, size = 8 * 4096; /* partition: 8 sectors = 512 records */
    dl_record_t r;

    CHECK(dl_init(&d, off, size, idx) == PAL_OK);
    CHECK(dl_next_seq() == 1 && dl_oldest_seq() == 1);
    for (uint32_t i = 0; i < 100; i++) {
        dl_event_t e = { .code = EV_KEY_ON, .a = (int32_t)i };
        CHECK(dl_append(DL_T_EVENT, 1000 + i, &e, sizeof(e)) == PAL_OK);
    }
    CHECK(dl_read(42, &r) == PAL_OK && r.seq == 42 && r.utc == 1041);
    CHECK(dl_read(101, &r) == PAL_ENOENT);

    /* remount */
    CHECK(dl_init(&d, off, size, idx) == PAL_OK);
    CHECK(dl_next_seq() == 101 && dl_oldest_seq() == 1);

    /* wrap: 1000 more records into a 512-slot ring */
    for (uint32_t i = 0; i < 1000; i++) dl_append(DL_T_EVENT, 2000 + i, &i, sizeof(i));
    CHECK(dl_next_seq() == 1101);
    uint32_t oldest = dl_oldest_seq();
    CHECK(oldest > 1101 - 512 && oldest <= 1101 - 448);
    CHECK(dl_read(oldest, &r) == PAL_OK && r.seq == oldest);
    CHECK(dl_read(1100, &r) == PAL_OK && r.seq == 1100);
    CHECK(dl_read(5, &r) == PAL_ENOENT);
    CHECK(s.mem[0] == 0xFF && s.mem[off + size] == 0xFF); /* never wrote outside the partition */

    /* remount after wrap */
    CHECK(dl_init(&d, off, size, idx) == PAL_OK);
    CHECK(dl_next_seq() == 1101 && dl_oldest_seq() == oldest);

    /* power cut in the middle of a record */
    s.cut_after_bytes = 30;
    CHECK(dl_append(DL_T_EVENT, 1, "x", 1) == PAL_OK); /* retried in the next slot */
    CHECK(dl_next_seq() == 1102);
    s.cut_after_bytes = 30;
    uint32_t x = 7;
    dl_append(DL_T_EVENT, 1, &x, 4);
    s.cut_after_bytes = 20;
    dl_append(DL_T_EVENT, 1, &x, 4); /* torn, then retried; simulate reboot right after */
    CHECK(dl_init(&d, off, size, idx) == PAL_OK);
    uint32_t nxt = dl_next_seq();
    CHECK(nxt >= 1103);
    CHECK(dl_append(DL_T_EVENT, 9, &x, 4) == PAL_OK);
    CHECK(dl_read(nxt, &r) == PAL_OK && r.seq == nxt && r.utc == 9);
    CHECK(dl_read(1101, &r) == PAL_OK);
    free(s.mem);
}

/* ---- nvstore -------------------------------------------------------------- */

static void test_nvstore(void)
{
    puts("nvstore");
    flash_dev_t d;
    flash_sim_t s;
    flash_sim_create(&d, &s, 2 * 4096, 4096, 32); /* 2 small sectors: 63 records each */
    CHECK(nv_init(&d, 0, 4096) == PAL_OK);
    uint32_t v = 0, out = 0;
    CHECK(nv_get(1, &out, 4) == -1);
    for (v = 1; v <= 500; v++) {
        CHECK(nv_set(1, &v, 4) == PAL_OK);
        uint32_t w = v * 3;
        CHECK(nv_set(2, &w, 4) == PAL_OK);
    }
    CHECK(nv_get(1, &out, 4) == 4 && out == 500);
    CHECK(nv_get(2, &out, 4) == 4 && out == 1500);
    CHECK(nv_generation() > 10); /* compactions happened */
    uint32_t erases = s.erases;
    CHECK(nv_set(2, &out, 4) == PAL_OK && s.erases == erases); /* identical write skipped */

    uint8_t blob[130], back[130];
    for (int i = 0; i < 130; i++) blob[i] = (uint8_t)(i * 7);
    CHECK(nv_set_blob(0x100, blob, sizeof(blob)) == PAL_OK);

    CHECK(nv_init(&d, 0, 4096) == PAL_OK); /* remount */
    CHECK(nv_get(1, &out, 4) == 4 && out == 500);
    CHECK(nv_get_blob(0x100, back, sizeof(back)) == PAL_OK && !memcmp(blob, back, sizeof(blob)));

    /* torn record: skipped, older value survives */
    s.cut_after_bytes = 10;
    v = 999;
    nv_set(1, &v, 4);
    CHECK(nv_init(&d, 0, 4096) == PAL_OK);
    CHECK(nv_get(1, &out, 4) == 4 && out == 500);
    v = 1000;
    CHECK(nv_set(1, &v, 4) == PAL_OK && nv_get(1, &out, 4) == 4 && out == 1000);
    free(s.mem);
}

/* ---- fuel / runtime ------------------------------------------------------- */

static void test_fuel(void)
{
    puts("fuel_runtime");
    fr_cfg_t cfg = { .tank_l = 40, .low_fuel_pct = 15, .reserve_pct = 5, .service_interval_h = 250 };
    fr_counters_t c = { 0 };
    fr_state_t st;
    ev_n = 0;
    fr_init(&cfg, &c, ev_cb);

    /* no LFE1: 4 L/h burn must be inferred from a noisy level sender */
    float level = 60.0f;
    for (int t = 0; t < 40 * 60; t++) {
        level -= (4.0f / 40.0f * 100.0f) / 3600.0f;
        fr_input_t in = { .key_on = true, .rpm_valid = true, .rpm = 2800, .level_valid = true,
                          .level_pct = level + noise(1.5f), .blade_on = true };
        fr_update_1hz(&in);
    }
    fr_get(&st);
    CHECK(st.burn_valid && st.burn_from_level);
    NEAR(st.burn_lph, 4.0f, 0.4f);
    NEAR(st.level_pct, level, 1.0f);
    float expect_h = (st.remaining_l - 2.0f) / st.burn_lph;
    CHECK(st.runtime_valid);
    NEAR(st.runtime_left_h, expect_h, 0.01f);
    NEAR(st.runtime_left_h, (level * 0.4f - 2.0f) / 4.0f, 0.6f);
    CHECK(c.engine_s == 2400 && c.blade_s == 2400 && c.key_s == 2400);

    /* with LFE1 present the ECU rate wins */
    for (int t = 0; t < 600; t++) {
        fr_input_t in = { .key_on = true, .rpm_valid = true, .rpm = 2800, .rate_valid = true,
                          .rate_lph = 5.0f + noise(0.5f), .level_valid = true, .level_pct = level };
        fr_update_1hz(&in);
    }
    fr_get(&st);
    CHECK(!st.burn_from_level);
    NEAR(st.burn_lph, 5.0f, 0.3f);

    /* refuel to 95 % */
    for (int t = 0; t < 30; t++) {
        fr_input_t in = { .key_on = true, .rpm_valid = true, .rpm = 900, .level_valid = true, .level_pct = 95 };
        fr_update_1hz(&in);
    }
    CHECK(ev_count(EV_REFUEL) == 1);
    fr_get(&st);
    NEAR(st.level_pct, 95.0f, 2.0f);

    /* key off at 95 %, key on at 70 % -> fuel drop (theft/leak) flagged */
    fr_key_off();
    fr_input_t off = { .key_on = false };
    fr_update_1hz(&off);
    for (int t = 0; t < 15; t++) {
        fr_input_t in = { .key_on = true, .level_valid = true, .level_pct = 70 };
        fr_update_1hz(&in);
    }
    CHECK(ev_count(EV_FUEL_DROP) == 1);

    /* low fuel event once */
    for (int t = 0; t < 400; t++) {
        fr_input_t in = { .key_on = true, .level_valid = true, .level_pct = 10 };
        fr_update_1hz(&in);
    }
    CHECK(ev_count(EV_LOW_FUEL) == 1);
}

/* ---- GPS services ---------------------------------------------------------- */

static gnss_fix_t mkfix(double lat, double lon, float kmh)
{
    gnss_fix_t f = { .valid = true, .quality = 1, .sats = 12, .hdop_x10 = 8,
                     .lat_e7 = (int32_t)llround(lat * 1e7), .lon_e7 = (int32_t)llround(lon * 1e7),
                     .speed_kmh_x10 = (uint16_t)(kmh * 10), .utc = 1779624000u, .utc_valid = true };
    return f;
}

static void test_gps(void)
{
    puts("gps_services");
    static uint8_t grid[COV_GRID_BYTES];
    geofence_t sq = { .id = 7, .kind = GEOFENCE_KEEP_IN, .n = 4 };
    sq.pt[0].lat_e7 = 400000000; sq.pt[0].lon_e7 = -900000000;
    sq.pt[1].lat_e7 = 400000000; sq.pt[1].lon_e7 = -899900000;
    sq.pt[2].lat_e7 = 400100000; sq.pt[2].lon_e7 = -899900000;
    sq.pt[3].lat_e7 = 400100000; sq.pt[3].lon_e7 = -900000000;
    CHECK(geo_in_polygon(&sq, 400050000, -899950000));
    CHECK(!geo_in_polygon(&sq, 400150000, -899950000));
    NEAR(geo_distance_m(400000000, -900000000, 400100000, -900000000), 1105.4, 2.0); /* 0.01 deg lat */

    /* solar elevation: equinox noon on the equator ~ overhead; midnight below horizon */
    CHECK(geo_solar_elevation_deg(1773964800u + 12 * 3600 + 7 * 60, 0, 0) > 80.0f); /* 2026-03-20 ~solar noon */
    CHECK(geo_solar_elevation_deg(1773964800u, 0, 0) < -60.0f); /* midnight */

    ev_n = 0;
    gps_svc_init(grid, 1.5f, ev_cb);
    gps_svc_set_fence(0, &sq);

    /* mow 100 m north at 1 m/s inside the fence, 1 Hz fixes */
    double lat0 = 40.002, lon0 = -89.995;
    uint32_t t = 1000;
    for (int i = 0; i <= 100; i++, t += 1000) {
        gnss_fix_t f = mkfix(lat0 + i / 110540.0, lon0, 3.6f);
        gps_svc_update(&f, true, true, t);
    }
    gps_stats_t st;
    gps_svc_get(&st);
    NEAR(st.job_area_m2, 150.0, 15.0);
    NEAR(st.distance_m, 100.0, 1.5);
    CHECK(st.overlap_pct < 10);

    /* mow the same strip back: area barely grows, overlap ~50 % */
    for (int i = 100; i >= 0; i--, t += 1000) {
        gnss_fix_t f = mkfix(lat0 + i / 110540.0, lon0, 3.6f);
        gps_svc_update(&f, true, true, t);
    }
    gps_svc_get(&st);
    NEAR(st.job_area_m2, 150.0, 20.0);
    CHECK(st.overlap_pct >= 40 && st.overlap_pct <= 60);
    CHECK(st.productivity_m2_h > 0);

    /* transport with blades off adds no area */
    float area = st.job_area_m2;
    for (int i = 0; i < 30; i++, t += 1000) {
        gnss_fix_t f = mkfix(lat0, lon0 + i / 85000.0, 3.6f);
        gps_svc_update(&f, false, true, t);
    }
    gps_svc_get(&st);
    CHECK(st.job_area_m2 == area);

    /* diagonal (not grid-aligned) 100 m pass on a fresh job: still ~150 m2 */
    gps_svc_reset_job();
    for (int i = 0; i <= 100; i++, t += 1000) {
        gnss_fix_t f = mkfix(lat0 + i * 0.7071 / 110540.0, lon0 + i * 0.7071 / 85200.0, 3.6f);
        gps_svc_update(&f, true, true, t);
    }
    gps_svc_get(&st);
    NEAR(st.job_area_m2, 150.0, 12.0);

    /* leave the keep-in fence: exit event after debounce */
    CHECK(!st.outside_keep_in && ev_count(EV_GEOFENCE_EXIT) == 0);
    for (int i = 0; i < 4; i++, t += 1000) {
        gnss_fix_t f = mkfix(40.02, -89.995, 5);
        gps_svc_update(&f, false, true, t);
    }
    gps_svc_get(&st);
    CHECK(st.outside_keep_in && ev_count(EV_GEOFENCE_EXIT) == 1);
}

/* ---- anomaly ---------------------------------------------------------------- */

static void test_anomaly(void)
{
    puts("anomaly");
    static an_model_t m;
    an_result_t r;
    memset(&m, 0, sizeof(m));
    ev_n = 0;
    an_init(&m, ev_cb);

    an_input_t in = { .rpm = 3000, .load_pct = 60, .engine_run_s = 1000,
                      .valid = { true, true, true, true } };
    for (int i = 0; i < 2000; i++) {
        in.x[0] = 88 + noise(2); in.x[1] = 300 + noise(20); in.x[2] = 6 + noise(0.4f); in.x[3] = 70 + noise(2);
        an_update_1hz(&in, &r);
    }
    CHECK(!r.alert && r.score < 4 * 32 && r.learned_pct > 0);

    /* coolant creeps 15 °C hot (e.g. clogged deck screen / radiator) */
    for (int i = 0; i < 40; i++) {
        in.x[0] = 103 + noise(2); in.x[1] = 300 + noise(20); in.x[2] = 6 + noise(0.4f); in.x[3] = 70 + noise(2);
        an_update_1hz(&in, &r);
    }
    CHECK(r.alert && r.feature == 0 && ev_count(EV_ANOMALY) == 1);

    /* warm-up period is never scored */
    in.engine_run_s = 100;
    an_result_t r2;
    an_update_1hz(&in, &r2);
    CHECK(r2.score == 0);

    /* blade strike: 3000 rpm steady, sudden drop to 2300 */
    uint32_t t = 0;
    an_blade_impact(3000, false, t);
    bool hit = false;
    for (int i = 0; i < 250; i++) hit |= an_blade_impact(3000 + noise(20), true, t += 20);
    CHECK(!hit);
    hit = an_blade_impact(2300, true, t += 20);
    CHECK(hit && ev_count(EV_BLADE_IMPACT) == 1);
}

/* ---- fleet BLE protocol (with real datalog underneath) --------------------- */

static uint8_t fb_out[64][LINK_MAX_PAYLOAD];
static size_t fb_len[64];
static int fb_n;
static uint32_t acked;
static pal_status_t fb_send(const uint8_t *d, size_t n) { if (fb_n < 64) { memcpy(fb_out[fb_n], d, n); fb_len[fb_n++] = n; } return PAL_OK; }
static void fb_info(fb_info_t *o) { o->proto = FB_PROTO_VERSION; }
static void fb_sum(fb_summary_t *o) { o->fuel_pct_x2 = 100; }
static void fb_time(uint32_t u) { (void)u; }
static bool fb_param(uint16_t id, int32_t v) { return id == FB_PARAM_TANK_L_X10 && v > 0; }
static bool fb_fence(uint8_t s, const geofence_t *g) { return s < 4 && g->n >= 3; }
static void fb_ack(uint32_t s) { acked = s; }

static void test_fleet_ble(void)
{
    puts("fleet_ble");
    flash_dev_t d;
    flash_sim_t s;
    static uint32_t idx[16];
    flash_sim_create(&d, &s, 8 * 4096, 4096, 256);
    dl_init(&d, 0, 8 * 4096, idx);
    for (uint32_t i = 0; i < 10; i++) dl_append(DL_T_EVENT, i, &i, 4);

    fleet_ble_ops_t ops = { fb_info, fb_sum, fb_time, fb_param, fb_fence, fb_ack, fb_send };
    fleet_ble_init(&ops);

    uint8_t get_info[2] = { FB_CMD_GET_INFO, 1 };
    fb_n = 0;
    fleet_ble_on_status(true, false);
    fleet_ble_on_rx(get_info, 2);
    CHECK(fb_n == 1 && fb_out[0][0] == 0x81 && fb_out[0][2] == FB_ERR_AUTH);

    fleet_ble_on_status(true, true);
    fb_n = 0;
    fleet_ble_on_rx(get_info, 2);
    CHECK(fb_n == 1 && fb_out[0][2] == FB_OK && fb_len[0] == 3 + sizeof(fb_info_t));

    uint8_t rd[8] = { FB_CMD_READ_LOG, 2, 1, 0, 0, 0, 100, 0 };
    fb_n = 0;
    fleet_ble_on_rx(rd, sizeof(rd));
    CHECK(fb_n == 1 && fb_out[0][0] == 0x83);
    fleet_ble_poll();
    CHECK(fb_n == 1); /* no credits yet */
    fleet_ble_on_credit(2);
    fleet_ble_poll();
    CHECK(fb_n == 3 && fb_out[1][0] == FB_EVT_LOG_CHUNK && fb_out[1][7] == 3);
    fleet_ble_on_credit(8);
    fleet_ble_poll();
    /* 10 records = 3+3+3+1 chunks, then END */
    CHECK(fb_n == 6 && fb_out[4][7] == 1 && fb_out[5][0] == FB_EVT_LOG_END);
    dl_record_t rec;
    memcpy(&rec, &fb_out[4][8], sizeof(rec));
    CHECK(dl_record_valid(&rec) && rec.seq == 10);

    uint8_t ack[6] = { FB_CMD_ACK_LOG, 3, 10, 0, 0, 0 };
    fleet_ble_on_rx(ack, sizeof(ack));
    CHECK(acked == 10);

    uint8_t gf[2 + 1 + 4 + 3 * 8] = { FB_CMD_SET_GEOFENCE, 4, 0, 1, GEOFENCE_KEEP_IN, 3, 0 };
    fb_n = 0;
    fleet_ble_on_rx(gf, sizeof(gf));
    CHECK(fb_n == 1 && fb_out[0][2] == FB_OK);
    fleet_ble_on_rx(gf, sizeof(gf) - 1); /* truncated */
    CHECK(fb_n == 2 && fb_out[1][2] == FB_ERR_ARGS);
    free(s.mem);
}

/* ---- edge-AI health module (shared by both MCU paths) with a stub backend ---- */

static float stub_last_x[60 * 9];
static int stub_calls;
static float stub_probs[4] = { 1, 0, 0, 0 };
static pal_status_t stub_init(void) { return PAL_OK; }
static pal_status_t stub_infer(const float *x, float *p) { memcpy(stub_last_x, x, sizeof(stub_last_x)); memcpy(p, stub_probs, sizeof(stub_probs)); stub_calls++; return PAL_OK; }
static const ai_backend_t stub_backend = { "stub", stub_init, stub_infer };
const ai_backend_t *ai_backend_get(void) { return &stub_backend; }

static void sample(float *f, float rpm, float cool, float oilp, float hyd)
{
    float v[AI_F_COUNT] = { rpm, 50, cool, oilp, 3.6f, hyd, 22, 10, 1 };
    memcpy(f, v, sizeof(v));
}

static void test_ai_health(void)
{
    puts("ai_health");
    ai_health_result_t r;
    float f[AI_F_COUNT];
    CHECK(ai_health_init() == PAL_OK);
    int results = 0;
    for (int t = 1; t <= 120; t++) {
        sample(f, 2900, 85, 290, 60);
        if (ai_health_push(f, &r)) results++;
    }
    CHECK(results == 3 && stub_calls == 3);              /* first at 60 s, then every 30 s */
    CHECK(r.cls == AI_CLASS_NORMAL && !r.alert);
    /* normalisation: rpm 2900 -> (2900 - mean) / std, oldest sample first */
    NEAR(stub_last_x[0], (2900 - 2894.845459) / 221.851395, 1e-4);

    /* rules: hot coolant with blades engaged -> screen clog, alert on the second repeat */
    for (int t = 0; t < 30; t++) { sample(f, 2900, 99, 290, 60); ai_health_push(f, &r); }
    CHECK(r.cls == AI_CLASS_SCREEN_CLOG && r.rule_cls == AI_CLASS_SCREEN_CLOG && !r.alert);
    bool raised = false;
    for (int t = 0; t < 30; t++) { sample(f, 2900, 99, 290, 60); if (ai_health_push(f, &r)) raised = r.new_alert; }
    CHECK(r.alert && raised);

    /* model owns low oil: rules see nothing (pressure 180 kPa > 150), model says low oil */
    ai_health_reset();
    stub_probs[0] = 0.1f; stub_probs[2] = 0.9f;
    for (int t = 0; t < 90; t++) { sample(f, 2900, 85, 180, 60); ai_health_push(f, &r); }
    CHECK(r.cls == AI_CLASS_LOW_OIL && r.model_cls == AI_CLASS_LOW_OIL && r.rule_cls == AI_CLASS_NORMAL && r.alert);

    /* rules override the model for hydraulic overheat */
    for (int t = 0; t < 30; t++) { sample(f, 2900, 85, 290, 96); ai_health_push(f, &r); }
    CHECK(r.cls == AI_CLASS_HYD_OVERHEAT && r.model_cls == AI_CLASS_LOW_OIL);

    /* not-available values hold the last valid sample; no result before every feature was seen */
    ai_health_init();
    stub_probs[0] = 1; stub_probs[2] = 0;
    float nan_f[AI_F_COUNT];
    for (int i = 0; i < AI_F_COUNT; i++) nan_f[i] = NAN;
    CHECK(!ai_health_push(nan_f, &r));
    for (int t = 0; t < 59; t++) { sample(f, 2900, 85, 290, 60); ai_health_push(f, &r); }
    f[AI_F_OILP] = NAN;
    CHECK(ai_health_push(f, &r) && r.cls == AI_CLASS_NORMAL);
    NEAR(stub_last_x[59 * 9 + AI_F_OILP], (290 - 288.332642) / 38.168674, 1e-4);
    CHECK(strcmp(ai_class_name(AI_CLASS_LOW_OIL), "low_oil") == 0);
}

int main(void)
{
    test_crc();
    test_link();
    test_j1939();
    test_nmea();
    test_datalog();
    test_nvstore();
    test_fuel();
    test_gps();
    test_anomaly();
    test_fleet_ble();
    test_ai_health();
    printf("\n%d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
