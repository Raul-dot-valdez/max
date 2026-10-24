/*
 * Fleet data log: power-fail-safe ring of fixed 64-byte records on external
 * NOR (44 MB partition = ~720k records, ~2000 engine hours at one periodic
 * record per 10 s plus events).
 *
 * - Every record carries a monotonically increasing sequence number and a
 *   CRC-16; the mobile app syncs by sequence ("give me everything after N").
 * - Records are written into erased slots only. The sector ahead of the
 *   write pointer is erased when the pointer enters it, dropping the oldest
 *   64 records. A torn write (power cut) only ever invalidates one slot.
 * - Boot recovery scans the first slot of every sector, so mount time is
 *   bounded (~11k small reads) and does not depend on log fill level.
 */
#ifndef DATALOG_H
#define DATALOG_H

#include "flash_dev.h"

#define DL_RECORD_SIZE   64u
#define DL_PAYLOAD_MAX   50u
#define DL_MAGIC         0xA17Eu
#define DL_VERSION       1u

typedef enum {
    DL_T_PERIODIC = 1,
    DL_T_EVENT    = 2,
    DL_T_DTC      = 3,
    DL_T_TRIP     = 4,
} dl_type_t;

typedef struct PAL_PACKED {
    uint16_t magic;
    uint8_t  type;
    uint8_t  ver;
    uint32_t seq;
    uint32_t utc;
    uint8_t  payload[DL_PAYLOAD_MAX];
    uint16_t crc;     /* CRC-16/CCITT over bytes 0..61 */
} dl_record_t;

/* ---- Payload layouts (little-endian, packed) ----------------------------- */

#define DL_F_KEY_ON      0x01
#define DL_F_ENGINE_ON   0x02
#define DL_F_BLADE_ON    0x04
#define DL_F_GPS_FIX     0x08
#define DL_F_LOW_FUEL    0x10
#define DL_F_ANOMALY     0x20
#define DL_F_OUT_FENCE   0x40

typedef struct PAL_PACKED {
    uint16_t rpm;
    uint8_t  load_pct;
    int8_t   coolant_c;
    uint16_t oil_kpa;
    uint8_t  fuel_pct_x2;
    uint16_t fuel_rate_dlph;     /* 0.1 L/h */
    uint32_t engine_hours_x20;   /* 0.05 h, same resolution as SPN 247 */
    int32_t  lat_e7, lon_e7;
    uint16_t speed_kmh_x10;
    int8_t   hyd_c;
    uint8_t  battery_v_x10;
    uint8_t  flags;              /* DL_F_* */
    uint8_t  anomaly_score;
    uint32_t job_area_m2;
    uint16_t runtime_left_min;
    uint8_t  sats;
    uint8_t  hdop_x10;
} dl_periodic_t;

typedef enum {
    EV_BOOT = 1,          /* a = reset cause */
    EV_KEY_ON,
    EV_KEY_OFF,
    EV_LOW_FUEL,          /* a = level % x10 */
    EV_REFUEL,            /* a = before % x10, b = after % x10 */
    EV_FUEL_DROP,         /* possible theft / leak: a = before, b = after */
    EV_GEOFENCE_EXIT,     /* a = fence id */
    EV_GEOFENCE_ENTER,
    EV_ANOMALY,           /* a = score, b = feature index */
    EV_BLADE_IMPACT,      /* a = rpm before, b = rpm after */
    EV_SERVICE_DUE,       /* a = engine hours x10 */
    EV_TIME_SET,          /* a = old utc, b = new utc */
    EV_CAN_BUS_OFF,
    EV_BLE_SYNC,          /* a = acked seq */
    EV_OVERSPEED,         /* a = speed x10 */
} dl_event_code_t;

typedef struct PAL_PACKED {
    uint16_t code;
    int32_t  a, b;
    int32_t  lat_e7, lon_e7;
} dl_event_t;

typedef struct PAL_PACKED {
    uint8_t  sa;
    uint32_t spn;
    uint8_t  fmi;
    uint8_t  oc;
    uint8_t  active;  /* 1 = became active, 0 = cleared */
} dl_dtc_t;

typedef struct PAL_PACKED {
    uint32_t start_utc;
    uint32_t key_s;
    uint32_t engine_s;
    uint32_t blade_s;
    uint32_t fuel_used_ml;
    uint32_t area_m2;
    uint32_t distance_m;
    uint8_t  max_coolant_c_p40;
    uint8_t  max_hyd_c_p40;
    uint8_t  dtc_events;
    uint8_t  anomaly_events;
} dl_trip_t;

/* ---- API (thread-safe) ---------------------------------------------------- */

/* sector_index: caller-provided RAM, one uint32_t per erase sector of the
 * partition (44 MB / 4 KB = 11264 entries = 44 KB, placed in SDRAM). */
pal_status_t dl_init(flash_dev_t *dev, uint32_t part_off, uint32_t part_size, uint32_t *sector_index);
pal_status_t dl_append(uint8_t type, uint32_t utc, const void *payload, size_t len);
pal_status_t dl_read(uint32_t seq, dl_record_t *out);
uint32_t     dl_next_seq(void);    /* sequence the next append will get */
uint32_t     dl_oldest_seq(void);  /* oldest record still stored */
bool         dl_record_valid(const dl_record_t *r);

#endif
