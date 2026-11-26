#include "nmea.h"

#include <string.h>

#define MAX_FIELDS 20
#define TOD_NONE   0xFFFFFFFFu

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* Parse "[-]123.4567" into an integer scaled by 10^decimals (extra digits
 * truncated, missing digits zero-padded). */
static bool parse_fixed(const char *s, int decimals, int64_t *out)
{
    int64_t v = 0;
    bool neg = false, any = false;
    int frac = -1;
    if (*s == '-') { neg = true; s++; }
    for (; *s; s++) {
        if (*s == '.') { if (frac >= 0) return false; frac = 0; continue; }
        if (*s < '0' || *s > '9') return false;
        if (frac >= 0) { if (frac == decimals) continue; frac++; }
        v = v * 10 + (*s - '0');
        any = true;
    }
    if (!any) return false;
    if (frac < 0) frac = 0;
    for (; frac < decimals; frac++) v *= 10;
    *out = neg ? -v : v;
    return true;
}

/* "ddmm.mmmmm" / "dddmm.mmmmm" + hemisphere -> 1e-7 degrees */
static bool parse_coord(const char *s, const char *hemi, int32_t *out)
{
    const char *dot = strchr(s, '.');
    size_t int_len = dot ? (size_t)(dot - s) : strlen(s);
    if (int_len < 3 || int_len > 5) return false;
    size_t deg_len = int_len - 2;
    int64_t deg = 0, min_e5;
    for (size_t i = 0; i < deg_len; i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        deg = deg * 10 + (s[i] - '0');
    }
    if (!parse_fixed(s + deg_len, 5, &min_e5) || min_e5 >= 6000000) return false;
    int64_t v = deg * 10000000 + (min_e5 * 100 + 30) / 60;
    if (hemi[0] == 'S' || hemi[0] == 'W') v = -v;
    else if (hemi[0] != 'N' && hemi[0] != 'E') return false;
    *out = (int32_t)v;
    return true;
}

static bool parse_tod(const char *s, uint32_t *tod, uint32_t *secs)
{
    int64_t v;
    if (strlen(s) < 6 || !parse_fixed(s, 2, &v)) return false; /* hhmmss.ss -> hhmmssSS */
    uint32_t hh = (uint32_t)(v / 1000000), mm = (uint32_t)(v / 10000 % 100), ss = (uint32_t)(v / 100 % 100);
    if (hh > 23 || mm > 59 || ss > 60) return false;
    *tod = (uint32_t)v;
    *secs = hh * 3600 + mm * 60 + ss;
    return true;
}

uint32_t nmea_days_from_civil(int y, unsigned m, unsigned d)
{
    /* H. Hinnant's algorithm, valid for the proleptic Gregorian calendar */
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (uint32_t)(era * 146097 + (int)doe - 719468);
}

static void handle_rmc(nmea_parser_t *p, char **f, int nf)
{
    uint32_t tod, secs;
    int64_t v;
    if (nf < 10 || !parse_tod(f[1], &tod, &secs)) return;
    gnss_fix_t *x = &p->fix;
    p->rmc_tod = tod;
    bool active = f[2][0] == 'A';
    int32_t lat, lon;
    if (active && parse_coord(f[3], f[4], &lat) && parse_coord(f[5], f[6], &lon)) {
        x->lat_e7 = lat;
        x->lon_e7 = lon;
    } else {
        active = false;
    }
    p->rmc_active = active;
    x->speed_kmh_x10 = parse_fixed(f[7], 3, &v) ? (uint16_t)((v * 1852 + 50000) / 100000) : 0;
    x->course_x10 = parse_fixed(f[8], 1, &v) ? (uint16_t)v : 0;
    if (strlen(f[9]) == 6) {
        unsigned dd = (unsigned)((f[9][0] - '0') * 10 + f[9][1] - '0');
        unsigned mo = (unsigned)((f[9][2] - '0') * 10 + f[9][3] - '0');
        int yy = 2000 + (f[9][4] - '0') * 10 + f[9][5] - '0';
        if (dd >= 1 && dd <= 31 && mo >= 1 && mo <= 12) {
            x->utc = nmea_days_from_civil(yy, mo, dd) * 86400u + secs;
            x->utc_valid = true;
        }
    }
}

static void handle_gga(nmea_parser_t *p, char **f, int nf)
{
    uint32_t tod, secs;
    int64_t v;
    if (nf < 11 || !parse_tod(f[1], &tod, &secs)) return;
    gnss_fix_t *x = &p->fix;
    p->gga_tod = tod;
    x->quality = (uint8_t)(parse_fixed(f[6], 0, &v) ? v : 0);
    x->sats = (uint8_t)(parse_fixed(f[7], 0, &v) ? v : 0);
    x->hdop_x10 = parse_fixed(f[8], 1, &v) ? (uint16_t)v : 999;
    x->alt_cm = parse_fixed(f[9], 2, &v) ? (int32_t)v : 0;
}

static bool process_line(nmea_parser_t *p, gnss_fix_t *out)
{
    char *s = p->line;
    char *star = strrchr(s, '*');
    if (s[0] != '$' || !star || hexval(star[1]) < 0 || hexval(star[2]) < 0) return false;
    uint8_t ck = 0;
    for (char *c = s + 1; c < star; c++) ck ^= (uint8_t)*c;
    if (ck != (uint8_t)(hexval(star[1]) << 4 | hexval(star[2]))) return false;
    *star = '\0';

    char *f[MAX_FIELDS];
    int nf = 0;
    f[nf++] = s + 1;
    for (char *c = s + 1; *c && nf < MAX_FIELDS; c++)
        if (*c == ',') { *c = '\0'; f[nf++] = c + 1; }
    if (strlen(f[0]) != 5) return false;

    const char *type = f[0] + 2;
    if (!strcmp(type, "RMC")) handle_rmc(p, f, nf);
    else if (!strcmp(type, "GGA")) handle_gga(p, f, nf);
    else return false;

    if (p->rmc_tod != TOD_NONE && p->rmc_tod == p->gga_tod && p->emitted_tod != p->rmc_tod) {
        p->emitted_tod = p->rmc_tod;
        /* sentence order within an epoch varies by receiver: decide validity
         * only once both halves of the epoch are in */
        p->fix.valid = p->rmc_active && p->fix.quality > 0;
        *out = p->fix;
        return true;
    }
    return false;
}

void nmea_init(nmea_parser_t *p)
{
    memset(p, 0, sizeof(*p));
    p->rmc_tod = p->gga_tod = p->emitted_tod = TOD_NONE;
}

bool nmea_feed(nmea_parser_t *p, char c, gnss_fix_t *out)
{
    if (c == '$') { p->n = 0; }
    if (c == '\r') return false;
    if (c == '\n') {
        p->line[p->n] = '\0';
        bool r = p->n > 6 && process_line(p, out);
        p->n = 0;
        return r;
    }
    if (p->n < sizeof(p->line) - 1) p->line[p->n++] = c;
    else p->n = 0; /* overlong: drop */
    return false;
}
