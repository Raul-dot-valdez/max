/*
 * Fleet-management protocol spoken with the mobile app, carried inside
 * psoc_link DATA frames (one frame = one GATT write / notification).
 *
 *   request : cmd | req_id | args...
 *   response: cmd|0x80 | req_id | status | payload...
 *
 * Security: every command is refused unless the PSoC reports an encrypted,
 * bonded link (BLE 4.2 LE Secure Connections, numeric comparison on the HMI
 * screen). Log streaming is credit-based so the 921.6 kbit/s UART never
 * overruns the ~20-60 kB/s a phone actually sustains over BLE 4.2.
 *
 * Sync model: the app asks for records after the last sequence it has, then
 * ACK_LOG(seq) once its cloud upload succeeded. The HMI persists the ack so
 * the next connection (any phone in the fleet) resumes from there.
 */
#ifndef FLEET_BLE_H
#define FLEET_BLE_H

#include "datalog.h"
#include "gps_services.h"

#define FB_PROTO_VERSION 1

enum {
    FB_CMD_GET_INFO     = 0x01,
    FB_CMD_GET_SUMMARY  = 0x02,
    FB_CMD_READ_LOG     = 0x03, /* from_seq u32, max_records u16 -> chunks then END */
    FB_CMD_ACK_LOG      = 0x04, /* upto_seq u32 */
    FB_CMD_SET_TIME     = 0x05, /* utc u32 */
    FB_CMD_SET_GEOFENCE = 0x06, /* slot u8, geofence_t (header + n points) */
    FB_CMD_SET_PARAM    = 0x07, /* id u16, value i32 */
    FB_CMD_STOP         = 0x08,
    /* Events (HMI -> app, sent as-is, never OR'd with 0x80): code | req_id | status | payload */
    FB_EVT_LOG_CHUNK    = 0xC0, /* during READ_LOG: first_seq u32, count u8, records */
    FB_EVT_LOG_END      = 0xC1, /* next_seq u32 */
};

enum {
    FB_OK = 0,
    FB_ERR_AUTH = 1,
    FB_ERR_ARGS = 2,
    FB_ERR_UNKNOWN = 3,
    FB_ERR_BUSY = 4,
    FB_ERR_STORAGE = 5,
};

enum {
    FB_PARAM_TANK_L_X10        = 1,
    FB_PARAM_LOW_FUEL_PCT      = 2,
    FB_PARAM_SERVICE_INTERVAL_H = 3,
    FB_PARAM_DECK_WIDTH_MM     = 4,
    FB_PARAM_SERVICE_DONE      = 5,
    FB_PARAM_RESET_JOB         = 6,
};

typedef struct PAL_PACKED {
    uint8_t  proto;
    uint8_t  fw[3];
    uint32_t hw_uid;
    char     machine_id[18];
    uint32_t log_oldest, log_next, log_acked;
    uint8_t  j1939_sa;
} fb_info_t;

typedef struct PAL_PACKED {
    uint32_t utc;
    uint32_t engine_h_x10;
    uint32_t blade_h_x10;
    int32_t  service_in_h_x10;
    uint8_t  fuel_pct_x2;
    uint16_t fuel_l_x10;
    uint16_t burn_lph_x10;
    uint16_t runtime_left_min;
    int32_t  lat_e7, lon_e7;
    uint8_t  gps_fix;
    uint8_t  dtc_count;
    uint8_t  lamps;
    uint8_t  anomaly_score;
    uint32_t job_area_m2;
    uint16_t productivity_m2_h;
    uint8_t  flags; /* DL_F_* */
} fb_summary_t;

typedef struct {
    void (*get_info)(fb_info_t *out);
    void (*get_summary)(fb_summary_t *out);
    void (*set_time)(uint32_t utc);
    bool (*set_param)(uint16_t id, int32_t value);
    bool (*set_geofence)(uint8_t slot, const geofence_t *g);
    void (*ack_log)(uint32_t upto_seq);
    pal_status_t (*send)(const uint8_t *data, size_t len); /* one DATA_TO_APP frame */
} fleet_ble_ops_t;

void fleet_ble_init(const fleet_ble_ops_t *ops);
void fleet_ble_on_status(bool connected, bool encrypted);
void fleet_ble_on_credit(uint8_t n);
void fleet_ble_on_rx(const uint8_t *data, size_t len);
void fleet_ble_poll(void); /* streams log chunks while credits allow */

#endif
