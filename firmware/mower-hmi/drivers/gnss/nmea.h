/*
 * Allocation-free NMEA 0183 parser (RMC + GGA, any talker: GP/GN/GL/GA/BD).
 * Integer coordinates (1e-7 degree) keep full receiver precision without
 * double-precision parsing.
 */
#ifndef NMEA_H
#define NMEA_H

#include "pal_types.h"

typedef struct {
    bool     valid;          /* RMC status A and GGA quality > 0 */
    uint8_t  quality;        /* GGA: 1 GPS, 2 DGPS/SBAS, 4 RTK fixed, 5 RTK float */
    uint8_t  sats;
    uint16_t hdop_x10;
    int32_t  lat_e7, lon_e7;
    int32_t  alt_cm;
    uint16_t speed_kmh_x10;
    uint16_t course_x10;
    uint32_t utc;            /* UNIX seconds, valid when utc_valid */
    bool     utc_valid;
} gnss_fix_t;

typedef struct {
    char       line[96];
    uint8_t    n;
    gnss_fix_t fix;
    uint32_t   rmc_tod, gga_tod, emitted_tod; /* hhmmss*100 + hundredths */
    bool       rmc_active;                    /* RMC status 'A' with parsable position */
} nmea_parser_t;

void nmea_init(nmea_parser_t *p);
/* Returns true once per epoch, when RMC and GGA with the same time-of-day
 * have both been received; *out then holds the merged fix. */
bool nmea_feed(nmea_parser_t *p, char c, gnss_fix_t *out);

/* Exposed for tests and reuse */
uint32_t nmea_days_from_civil(int y, unsigned m, unsigned d);

#endif
