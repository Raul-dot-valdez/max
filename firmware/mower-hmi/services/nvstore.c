#include "nvstore.h"

#include <string.h>

#include "crc.h"
#include "osal.h"

#define NV_MAGIC     0x4E56u /* "NV" */
#define NV_KEY_HDR   0xFFF0u
#define NV_MAX_KEYS  96
#define REC          64u

typedef struct PAL_PACKED {
    uint16_t magic;
    uint16_t key;
    uint8_t  len;
    uint8_t  rsv[3];
    uint32_t seq;            /* header: sector generation; values: write counter */
    uint8_t  data[NV_VAL_MAX];
    uint32_t crc;            /* CRC-32 over bytes 0..59 */
} nv_rec_t;

_Static_assert(sizeof(nv_rec_t) == REC, "nv record must be 64 bytes");

typedef struct {
    uint16_t key;
    uint32_t off; /* offset within active sector */
} nv_slot_t;

static struct {
    osal_mutex_t lock;
    flash_dev_t *dev;
    uint32_t     sec[2], sec_size;
    int          active;
    uint32_t     gen, wr_off, seq;
    nv_slot_t    map[NV_MAX_KEYS];
    int          nkeys;
} s;

static bool rec_ok(const nv_rec_t *r)
{
    return r->magic == NV_MAGIC && r->len <= NV_VAL_MAX && crc32_ieee(r, REC - 4, 0) == r->crc;
}

static bool rec_erased(const nv_rec_t *r)
{
    const uint8_t *p = (const uint8_t *)r;
    for (unsigned i = 0; i < REC; i++)
        if (p[i] != 0xFF) return false;
    return true;
}

static bool rd(int sec, uint32_t off, nv_rec_t *r)
{
    return s.dev->read(s.dev, s.sec[sec] + off, r, REC) == PAL_OK;
}

static pal_status_t wr(int sec, uint32_t off, const nv_rec_t *r)
{
    return s.dev->prog(s.dev, s.sec[sec] + off, r, REC);
}

static void make(nv_rec_t *r, uint16_t key, const void *v, uint8_t len, uint32_t seq)
{
    memset(r, 0xFF, sizeof(*r));
    r->magic = NV_MAGIC;
    r->key = key;
    r->len = len;
    r->seq = seq;
    memset(r->data, 0, NV_VAL_MAX);
    if (len)
        memcpy(r->data, v, len);
    r->crc = crc32_ieee(r, REC - 4, 0);
}

static nv_slot_t *find(uint16_t key)
{
    for (int i = 0; i < s.nkeys; i++)
        if (s.map[i].key == key) return &s.map[i];
    return NULL;
}

static bool map_put(uint16_t key, uint32_t off)
{
    nv_slot_t *m = find(key);
    if (!m) {
        if (s.nkeys >= NV_MAX_KEYS) return false;
        m = &s.map[s.nkeys++];
        m->key = key;
    }
    m->off = off;
    return true;
}

static bool header_gen(int sec, uint32_t *gen)
{
    nv_rec_t r;
    if (!rd(sec, 0, &r) || !rec_ok(&r) || r.key != NV_KEY_HDR) return false;
    *gen = r.seq;
    return true;
}

static void scan_active(void)
{
    nv_rec_t r;
    s.nkeys = 0;
    s.wr_off = REC;
    for (uint32_t off = REC; off + REC <= s.sec_size; off += REC) {
        if (!rd(s.active, off, &r)) break;
        if (rec_erased(&r)) continue;
        s.wr_off = off + REC; /* past anything programmed, valid or torn */
        if (rec_ok(&r) && r.key != NV_KEY_HDR) {
            map_put(r.key, off);
            if (r.seq >= s.seq) s.seq = r.seq + 1;
        }
    }
}

static pal_status_t format(int sec, uint32_t gen)
{
    nv_rec_t h;
    for (uint32_t off = 0; off < s.sec_size; off += s.dev->erase_size)
        if (s.dev->erase(s.dev, s.sec[sec] + off) != PAL_OK) return PAL_ERR;
    make(&h, NV_KEY_HDR, NULL, 0, gen);
    return wr(sec, 0, &h);
}

/* Copy live records to the other sector, then commit with its header. */
static pal_status_t compact(void)
{
    int dst = !s.active;
    nv_rec_t r;
    for (uint32_t off = 0; off < s.sec_size; off += s.dev->erase_size)
        if (s.dev->erase(s.dev, s.sec[dst] + off) != PAL_OK) return PAL_ERR;
    uint32_t woff = REC;
    for (int i = 0; i < s.nkeys; i++) {
        if (!rd(s.active, s.map[i].off, &r) || wr(dst, woff, &r) != PAL_OK) return PAL_ERR;
        s.map[i].off = woff;
        woff += REC;
    }
    make(&r, NV_KEY_HDR, NULL, 0, s.gen + 1);
    if (wr(dst, 0, &r) != PAL_OK) return PAL_ERR;
    s.gen++;
    s.active = dst;
    s.wr_off = woff;
    return PAL_OK;
}

pal_status_t nv_init(flash_dev_t *dev, uint32_t sec_a, uint32_t sec_b)
{
    uint32_t g[2];
    bool ok[2];
    if (!s.lock) s.lock = osal_mutex_create();
    s.dev = dev;
    s.sec[0] = sec_a;
    s.sec[1] = sec_b;
    s.sec_size = sec_b > sec_a ? sec_b - sec_a : sec_a - sec_b;
    s.seq = 1;
    ok[0] = header_gen(0, &g[0]);
    ok[1] = header_gen(1, &g[1]);

    if (!ok[0] && !ok[1]) {
        s.active = 0;
        s.gen = 1;
        if (format(0, 1) != PAL_OK) return PAL_ERR;
    } else {
        s.active = (ok[1] && (!ok[0] || g[1] > g[0])) ? 1 : 0;
        s.gen = g[s.active];
    }
    scan_active();
    return PAL_OK;
}

pal_status_t nv_set(uint16_t key, const void *val, uint8_t len)
{
    nv_rec_t r, cur;
    if (len > NV_VAL_MAX || key == NV_KEY_HDR) return PAL_EINVAL;
    osal_mutex_lock(s.lock);
    pal_status_t st = PAL_OK;
    nv_slot_t *m = find(key);
    /* skip identical writes: saves wear for periodic saves of unchanged data */
    if (m && rd(s.active, m->off, &cur) && cur.len == len && !memcmp(cur.data, val, len)) {
        osal_mutex_unlock(s.lock);
        return PAL_OK;
    }
    if (!m && s.nkeys >= NV_MAX_KEYS) st = PAL_ENOSPC;
    if (st == PAL_OK && s.wr_off + REC > s.sec_size) st = compact();
    if (st == PAL_OK && s.wr_off + REC > s.sec_size) st = PAL_ENOSPC;
    if (st == PAL_OK) {
        make(&r, key, val, len, s.seq++);
        st = wr(s.active, s.wr_off, &r);
        if (st == PAL_OK) map_put(key, s.wr_off);
        s.wr_off += REC; /* even on failure: never re-program a word */
    }
    osal_mutex_unlock(s.lock);
    return st;
}

int nv_get(uint16_t key, void *val, uint8_t max_len)
{
    nv_rec_t r;
    int n = -1;
    osal_mutex_lock(s.lock);
    nv_slot_t *m = find(key);
    if (m && rd(s.active, m->off, &r) && rec_ok(&r)) {
        n = r.len < max_len ? r.len : max_len;
        memcpy(val, r.data, (size_t)n);
    }
    osal_mutex_unlock(s.lock);
    return n;
}

pal_status_t nv_set_blob(uint16_t key_base, const void *data, size_t len)
{
    const uint8_t *p = data;
    for (uint16_t k = 0; len; k++) {
        uint8_t n = (uint8_t)(len > NV_VAL_MAX ? NV_VAL_MAX : len);
        pal_status_t st = nv_set((uint16_t)(key_base + k), p, n);
        if (st != PAL_OK) return st;
        p += n;
        len -= n;
    }
    return PAL_OK;
}

pal_status_t nv_get_blob(uint16_t key_base, void *data, size_t len)
{
    uint8_t *p = data;
    for (uint16_t k = 0; len; k++) {
        uint8_t n = (uint8_t)(len > NV_VAL_MAX ? NV_VAL_MAX : len);
        if (nv_get((uint16_t)(key_base + k), p, n) != n) return PAL_ENOENT;
        p += n;
        len -= n;
    }
    return PAL_OK;
}

uint32_t nv_generation(void) { return s.gen; }
