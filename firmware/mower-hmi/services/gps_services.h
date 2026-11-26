/*
 * GNSS-derived customer features:
 *  1. Geofences: keep-in (theft / unauthorised use alert) and job-site
 *     fences (engine hours per site -> contractor billing).
 *  2. Coverage tracking: mowed area, productivity (m2 per blade-hour) and
 *     overlap %, from blade state + track + deck width.
 *  3. Trip distance.
 *  4. Solar elevation from position + UTC -> automatic day/night HMI theme
 *     and backlight, without an ambient light sensor.
 *
 * Accuracy note: standalone GNSS is ~1.5-2.5 m CEP; SBAS helps; area totals
 * are good to a few percent over a job, overlap % is indicative only unless
 * an RTK receiver is fitted (fix quality 4).
 */
#ifndef GPS_SERVICES_H
#define GPS_SERVICES_H

#include "nmea.h"

#define GEOFENCE_SLOTS    4
#define GEOFENCE_MAX_PTS  16
#define COV_DIM           4096u   /* cells per side */
#define COV_CELL_M        0.25f   /* -> 1024 m x 1024 m job area, 2 MB bitmap in SDRAM.
                                     0.25 m keeps lattice aliasing of a 1.5 m swath under
                                     ~6 % worst case (track exactly at 45 deg); 0.5 m gave 18 % */
#define COV_GRID_BYTES    (COV_DIM * COV_DIM / 8u)

typedef enum {
    GEOFENCE_KEEP_IN  = 1, /* alert when the machine leaves */
    GEOFENCE_JOB_SITE = 2, /* accumulate engine time while inside */
} geofence_kind_t;

typedef struct PAL_PACKED {
    uint8_t id;
    uint8_t kind;
    uint8_t n;
    uint8_t rsv;
    struct PAL_PACKED { int32_t lat_e7, lon_e7; } pt[GEOFENCE_MAX_PTS];
} geofence_t;

typedef struct {
    bool     fix;
    float    job_area_m2;
    float    productivity_m2_h;
    uint8_t  overlap_pct;
    float    distance_m;
    bool     outside_keep_in;
    int8_t   current_site;               /* -1 none */
    uint32_t site_engine_s[GEOFENCE_SLOTS];
    bool     daylight;
} gps_stats_t;

typedef void (*gps_event_fn_t)(uint16_t code, int32_t a, int32_t b);

void  gps_svc_init(uint8_t *grid_mem, float deck_width_m, gps_event_fn_t ev);
bool  gps_svc_set_fence(uint8_t slot, const geofence_t *g); /* g->n == 0 clears */
void  gps_svc_update(const gnss_fix_t *fix, bool blade_on, bool engine_on, uint32_t now_ms);
void  gps_svc_get(gps_stats_t *out);
void  gps_svc_reset_job(void);
void  gps_svc_set_deck_width(float m);

/* Exposed for tests */
bool  geo_in_polygon(const geofence_t *g, int32_t lat_e7, int32_t lon_e7);
float geo_distance_m(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2);
float geo_solar_elevation_deg(uint32_t utc, int32_t lat_e7, int32_t lon_e7);

#endif
