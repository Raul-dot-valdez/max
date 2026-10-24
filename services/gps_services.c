#include "gps_services.h"

#include <math.h>
#include <string.h>

#include "datalog.h"

#define DEG2RAD        (3.14159265358979323846 / 180.0)
#define M_PER_DEG_LAT  110540.0
#define M_PER_DEG_LON  111320.0
#define FENCE_DEBOUNCE 3       /* consecutive fixes before a fence transition */
#define MAX_HDOP_X10   30      /* ignore poor fixes for coverage/distance */
#define MIN_SPEED_X10  5       /* 0.5 km/h: below this, position noise dominates */
#define MAX_JUMP_M     25.0f

typedef struct {
    bool       used;
    geofence_t g;
    bool       inside;
    uint8_t    cnt;
} fence_t;

static struct {
    uint8_t       *grid;
    float          deck_w;
    gps_event_fn_t ev;
    fence_t        fence[GEOFENCE_SLOTS];
    gps_stats_t    st;

    bool     origin_set;
    double   lat0, lon0, coslat0;
    bool     have_prev;
    bool     prev_cov;   /* previous fix was also taken with blades on */
    float    px, py;
    int32_t  plat, plon;
    uint32_t prev_ms;
    float    swept_m2;
    float    blade_s;
    uint32_t last_daylight_utc;
} s;

/* ---- geometry ------------------------------------------------------------ */

bool geo_in_polygon(const geofence_t *g, int32_t lat_e7, int32_t lon_e7)
{
    bool in = false;
    double y = lat_e7, x = lon_e7;
    for (uint8_t i = 0, j = (uint8_t)(g->n - 1); i < g->n; j = i++) {
        double yi = g->pt[i].lat_e7, xi = g->pt[i].lon_e7;
        double yj = g->pt[j].lat_e7, xj = g->pt[j].lon_e7;
        if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi)
            in = !in;
    }
    return in;
}

float geo_distance_m(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2)
{
    double la = (lat1 + (double)lat2) * 0.5e-7 * DEG2RAD;
    double dy = (lat2 - (double)lat1) * 1e-7 * M_PER_DEG_LAT;
    double dx = (lon2 - (double)lon1) * 1e-7 * M_PER_DEG_LON * cos(la);
    return (float)sqrt(dx * dx + dy * dy);
}

float geo_solar_elevation_deg(uint32_t utc, int32_t lat_e7, int32_t lon_e7)
{
    double d = utc / 86400.0 - 10957.5; /* days since J2000.0 */
    double g = fmod(357.529 + 0.98560028 * d, 360.0) * DEG2RAD;
    double q = fmod(280.459 + 0.98564736 * d, 360.0);
    double L = (q + 1.915 * sin(g) + 0.020 * sin(2 * g)) * DEG2RAD;
    double e = (23.439 - 0.00000036 * d) * DEG2RAD;
    double ra = atan2(cos(e) * sin(L), cos(L)) / DEG2RAD;
    double dec = asin(sin(e) * sin(L));
    double gmst = fmod(18.697374558 + 24.06570982441908 * d, 24.0);
    double ha = (gmst * 15.0 + lon_e7 * 1e-7 - ra) * DEG2RAD;
    double lat = lat_e7 * 1e-7 * DEG2RAD;
    double el = asin(sin(lat) * sin(dec) + cos(lat) * cos(dec) * cos(ha));
    return (float)(el / DEG2RAD);
}

/* ---- coverage grid ------------------------------------------------------- */

static void to_local(int32_t lat, int32_t lon, float *x, float *y)
{
    *x = (float)((lon * 1e-7 - s.lon0) * M_PER_DEG_LON * s.coslat0);
    *y = (float)((lat * 1e-7 - s.lat0) * M_PER_DEG_LAT);
}

static void set_origin(int32_t lat, int32_t lon)
{
    s.lat0 = lat * 1e-7;
    s.lon0 = lon * 1e-7;
    s.coslat0 = cos(s.lat0 * DEG2RAD);
    s.origin_set = true;
    if (s.grid)
        memset(s.grid, 0, COV_GRID_BYTES);
}

/* Cell centres sit at (k + GRID_OFS) * cell. The irregular offset keeps a
 * track that starts at the origin (every job does) from running exactly along
 * centres or edges, where the inside/outside test would be decided by ties. */
#define GRID_OFS 0.37f
static int32_t cell_of(float v) { return (int32_t)floorf(v / COV_CELL_M - GRID_OFS + 0.5f) + (int32_t)(COV_DIM / 2); }
static float   centre_of(int32_t i) { return ((float)(i - (int32_t)(COV_DIM / 2)) + GRID_OFS) * COV_CELL_M; }

/* Mark every cell whose CENTRE lies inside the swept rectangle (segment x deck
 * width). Centre sampling is unbiased; sampling the swath edges instead
 * over-counts by up to one cell per side. Returns false if the swath leaves
 * the grid. */
static bool stamp(float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
    if (len < 0.05f)
        return true;
    float ux = dx / len, uy = dy / len, half = s.deck_w * 0.5f;
    int32_t ix0 = cell_of(fminf(x0, x1) - half), ix1 = cell_of(fmaxf(x0, x1) + half);
    int32_t iy0 = cell_of(fminf(y0, y1) - half), iy1 = cell_of(fmaxf(y0, y1) + half);
    if (ix0 < 0 || iy0 < 0 || ix1 >= (int32_t)COV_DIM || iy1 >= (int32_t)COV_DIM)
        return false;

    uint32_t added = 0;
    for (int32_t iy = iy0; iy <= iy1; iy++) {
        float ry = centre_of(iy) - y0;
        for (int32_t ix = ix0; ix <= ix1; ix++) {
            float rx = centre_of(ix) - x0;
            float along = rx * ux + ry * uy, across = ry * ux - rx * uy;
            if (along < 0 || along >= len || fabsf(across) > half) /* symmetric: direction-independent */
                continue;
            uint32_t bit = (uint32_t)iy * COV_DIM + (uint32_t)ix;
            uint8_t m = (uint8_t)(1u << (bit & 7));
            if (!(s.grid[bit >> 3] & m)) {
                s.grid[bit >> 3] |= m;
                added++;
            }
        }
    }
    s.st.job_area_m2 += added * COV_CELL_M * COV_CELL_M;
    s.swept_m2 += len * s.deck_w;
    float ov = s.swept_m2 > 1.0f ? 100.0f * (1.0f - s.st.job_area_m2 / s.swept_m2) : 0.0f;
    s.st.overlap_pct = (uint8_t)(ov < 0 ? 0 : (ov > 100 ? 100 : ov));
    return true;
}

/* ---- public ---------------------------------------------------------------- */

void gps_svc_init(uint8_t *grid_mem, float deck_width_m, gps_event_fn_t ev)
{
    memset(&s, 0, sizeof(s));
    s.grid = grid_mem;
    s.deck_w = deck_width_m;
    s.ev = ev;
    s.st.current_site = -1;
    s.st.daylight = true;
    if (s.grid)
        memset(s.grid, 0, COV_GRID_BYTES);
}

void gps_svc_set_deck_width(float m) { s.deck_w = m; }

bool gps_svc_set_fence(uint8_t slot, const geofence_t *g)
{
    if (slot >= GEOFENCE_SLOTS || g->n > GEOFENCE_MAX_PTS || (g->n && g->n < 3))
        return false;
    fence_t *f = &s.fence[slot];
    f->used = g->n != 0;
    f->g = *g;
    f->inside = true; /* assume inside until proven otherwise: no alert at boot */
    f->cnt = 0;
    return true;
}

void gps_svc_reset_job(void)
{
    s.origin_set = false;
    s.have_prev = false;
    s.st.job_area_m2 = 0;
    s.swept_m2 = 0;
    s.blade_s = 0;
    s.st.overlap_pct = 0;
    s.st.productivity_m2_h = 0;
}

static void fences_update(const gnss_fix_t *fx, bool engine_on, float dt)
{
    s.st.outside_keep_in = false;
    s.st.current_site = -1;
    for (int i = 0; i < GEOFENCE_SLOTS; i++) {
        fence_t *f = &s.fence[i];
        if (!f->used)
            continue;
        bool in = geo_in_polygon(&f->g, fx->lat_e7, fx->lon_e7);
        f->cnt = (in != f->inside) ? (uint8_t)(f->cnt + 1) : 0;
        if (f->cnt >= FENCE_DEBOUNCE) {
            f->inside = in;
            f->cnt = 0;
            if (s.ev)
                s.ev(in ? EV_GEOFENCE_ENTER : EV_GEOFENCE_EXIT, f->g.id, f->g.kind);
        }
        if (f->g.kind == GEOFENCE_KEEP_IN && !f->inside)
            s.st.outside_keep_in = true;
        if (f->g.kind == GEOFENCE_JOB_SITE && f->inside) {
            s.st.current_site = (int8_t)i;
            if (engine_on)
                s.st.site_engine_s[i] += (uint32_t)(dt + 0.5f);
        }
    }
}

void gps_svc_update(const gnss_fix_t *fx, bool blade_on, bool engine_on, uint32_t now_ms)
{
    float dt = s.prev_ms ? (now_ms - s.prev_ms) / 1000.0f : 0.0f;
    if (dt > 5.0f)
        dt = 5.0f; /* fix outage: do not credit the gap */
    s.prev_ms = now_ms;
    s.st.fix = fx->valid;
    if (!fx->valid) {
        s.have_prev = false;
        return;
    }

    if (fx->utc_valid && fx->utc - s.last_daylight_utc >= 60) {
        s.last_daylight_utc = fx->utc;
        s.st.daylight = geo_solar_elevation_deg(fx->utc, fx->lat_e7, fx->lon_e7) > -6.0f; /* civil twilight */
    }

    fences_update(fx, engine_on, dt);

    bool good = fx->hdop_x10 <= MAX_HDOP_X10;
    bool moving = fx->speed_kmh_x10 >= MIN_SPEED_X10;
    if (!good) {
        s.have_prev = false;
        return;
    }

    if (s.have_prev && moving) {
        float d = geo_distance_m(s.plat, s.plon, fx->lat_e7, fx->lon_e7);
        if (d < MAX_JUMP_M)
            s.st.distance_m += d;
    }

    if (blade_on && engine_on) {
        s.blade_s += dt;
        if (!s.origin_set)
            set_origin(fx->lat_e7, fx->lon_e7);
        float x, y;
        to_local(fx->lat_e7, fx->lon_e7, &x, &y);
        if (s.have_prev && s.prev_cov && moving && s.grid) {
            float jump = sqrtf((x - s.px) * (x - s.px) + (y - s.py) * (y - s.py));
            if (jump < MAX_JUMP_M && !stamp(s.px, s.py, x, y)) {
                /* left the 1 km grid: re-centre here (area total is kept) */
                set_origin(fx->lat_e7, fx->lon_e7);
                to_local(fx->lat_e7, fx->lon_e7, &x, &y);
            }
        }
        s.px = x;
        s.py = y;
        if (s.blade_s > 60.0f)
            s.st.productivity_m2_h = s.st.job_area_m2 / (s.blade_s / 3600.0f);
    }
    s.prev_cov = blade_on && engine_on;
    s.plat = fx->lat_e7;
    s.plon = fx->lon_e7;
    s.have_prev = true;
}

void gps_svc_get(gps_stats_t *out) { *out = s.st; }
