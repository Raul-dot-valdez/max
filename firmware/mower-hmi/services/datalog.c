#include "datalog.h"

#include <string.h>

#include "crc.h"
#include "osal.h"

#define SEQ_NONE 0xFFFFFFFFu

_Static_assert(sizeof(dl_record_t) == DL_RECORD_SIZE, "record must be 64 bytes");
_Static_assert(sizeof(dl_periodic_t) <= DL_PAYLOAD_MAX, "periodic payload too big");
_Static_assert(sizeof(dl_trip_t) <= DL_PAYLOAD_MAX, "trip payload too big");

static struct {
    osal_mutex_t lock;
    flash_dev_t *dev;
    uint32_t     off, nsec, spr, nslots;
    uint32_t    *idx;        /* first sequence number stored in each sector, or SEQ_NONE */
    uint32_t     head_slot;  /* next slot to program */
    uint32_t     next_seq;
    uint32_t     cache_sec;  /* last sector hit by dl_read (sequential sync is the common case) */
    bool         ready;
} s;

static uint32_t slot_addr(uint32_t slot) { return s.off + slot * DL_RECORD_SIZE; }

bool dl_record_valid(const dl_record_t *r)
{
    return r->magic == DL_MAGIC && crc16_ccitt(r, DL_RECORD_SIZE - 2, 0xFFFF) == r->crc;
}

static bool read_slot(uint32_t slot, dl_record_t *r)
{
    return s.dev->read(s.dev, slot_addr(slot), r, sizeof(*r)) == PAL_OK;
}

static bool is_erased(const dl_record_t *r)
{
    const uint8_t *p = (const uint8_t *)r;
    for (size_t i = 0; i < sizeof(*r); i++)
        if (p[i] != 0xFF)
            return false;
    return true;
}

pal_status_t dl_init(flash_dev_t *dev, uint32_t part_off, uint32_t part_size, uint32_t *sector_index)
{
    dl_record_t r;
    if (!s.lock)
        s.lock = osal_mutex_create();
    s.dev = dev;
    s.off = part_off;
    s.spr = dev->erase_size / DL_RECORD_SIZE;
    s.nsec = part_size / dev->erase_size;
    s.nslots = s.nsec * s.spr;
    s.idx = sector_index;
    s.cache_sec = 0;
    if (s.nsec < 2 || (part_off % dev->erase_size))
        return PAL_EINVAL;

    /* 1. index the first valid record of every sector */
    int32_t best = -1;
    for (uint32_t i = 0; i < s.nsec; i++) {
        s.idx[i] = SEQ_NONE;
        for (uint32_t j = 0; j < s.spr; j++) {
            if (!read_slot(i * s.spr + j, &r))
                return PAL_ERR;
            if (dl_record_valid(&r)) {
                s.idx[i] = r.seq - j; /* normalised to slot 0 so reads can compute offsets */
                break;
            }
            if (is_erased(&r) || j > 0)
                break; /* only keep scanning past a torn slot 0 */
        }
        if (s.idx[i] != SEQ_NONE && (best < 0 || s.idx[i] > s.idx[best]))
            best = (int32_t)i;
    }

    if (best < 0) {
        s.head_slot = 0;
        s.next_seq = 1;
    } else {
        /* 2. find the end of the newest sector */
        uint32_t base = (uint32_t)best * s.spr, j, last = SEQ_NONE;
        for (j = 0; j < s.spr; j++) {
            if (!read_slot(base + j, &r))
                return PAL_ERR;
            if (dl_record_valid(&r)) { last = r.seq; continue; }
            if (is_erased(&r)) break;
            /* torn slot: skip it, never program over it */
        }
        s.next_seq = (last == SEQ_NONE ? s.idx[best] : last) + 1;
        s.head_slot = (base + j) % s.nslots;
    }
    s.ready = true;
    return PAL_OK;
}

pal_status_t dl_append(uint8_t type, uint32_t utc, const void *payload, size_t len)
{
    if (len > DL_PAYLOAD_MAX)
        return PAL_EINVAL;
    dl_record_t r;
    memset(&r, 0xFF, sizeof(r));
    r.magic = DL_MAGIC;
    r.type = type;
    r.ver = DL_VERSION;
    r.utc = utc;
    memset(r.payload, 0, sizeof(r.payload));
    memcpy(r.payload, payload, len);

    osal_mutex_lock(s.lock);
    pal_status_t st = s.ready ? PAL_OK : PAL_ERR;
    for (int attempt = 0; st == PAL_OK && attempt < 2; attempt++) {
        uint32_t slot = s.head_slot, sec = slot / s.spr;
        if (slot % s.spr == 0) {
            s.idx[sec] = SEQ_NONE; /* oldest data in this sector is dropped */
            if ((st = s.dev->erase(s.dev, slot_addr(slot))) != PAL_OK)
                break;
        }
        r.seq = s.next_seq;
        r.crc = crc16_ccitt(&r, DL_RECORD_SIZE - 2, 0xFFFF);
        st = s.dev->prog(s.dev, slot_addr(slot), &r, sizeof(r));
        s.head_slot = (slot + 1) % s.nslots;
        if (st == PAL_OK) {
            if (s.idx[sec] == SEQ_NONE)
                s.idx[sec] = r.seq - (slot % s.spr);
            s.next_seq++;
            break;
        }
        st = PAL_OK; /* bad slot: retry once in the next one */
        if (attempt == 1)
            st = PAL_ERR;
    }
    osal_mutex_unlock(s.lock);
    return st;
}

static uint32_t oldest_locked(void)
{
    uint32_t m = SEQ_NONE;
    for (uint32_t i = 0; i < s.nsec; i++)
        if (s.idx[i] < m)
            m = s.idx[i];
    return m == SEQ_NONE ? s.next_seq : (m == 0 ? 1 : m);
}

static bool sector_for(uint32_t seq, uint32_t *sec)
{
    uint32_t c = s.cache_sec;
    uint32_t hi = s.idx[c] != SEQ_NONE ? s.idx[c] + s.spr : 0;
    if (s.idx[c] != SEQ_NONE && seq >= s.idx[c] && seq < hi) {
        *sec = c;
        return true;
    }
    for (uint32_t i = 0; i < s.nsec; i++) {
        if (s.idx[i] != SEQ_NONE && seq >= s.idx[i] && seq < s.idx[i] + s.spr) {
            *sec = s.cache_sec = i;
            return true;
        }
    }
    return false;
}

pal_status_t dl_read(uint32_t seq, dl_record_t *out)
{
    pal_status_t st = PAL_ENOENT;
    uint32_t sec;
    osal_mutex_lock(s.lock);
    if (s.ready && seq < s.next_seq && sector_for(seq, &sec)) {
        /* idx is normalised to slot 0, so the offset is exact unless a torn
         * slot shifted later records; fall back to scanning the sector. */
        uint32_t guess = seq - s.idx[sec];
        if (read_slot(sec * s.spr + guess, out) && dl_record_valid(out) && out->seq == seq) {
            st = PAL_OK;
        } else {
            for (uint32_t j = 0; j < s.spr; j++)
                if (read_slot(sec * s.spr + j, out) && dl_record_valid(out) && out->seq == seq) {
                    st = PAL_OK;
                    break;
                }
        }
    }
    osal_mutex_unlock(s.lock);
    return st;
}

uint32_t dl_next_seq(void)
{
    osal_mutex_lock(s.lock);
    uint32_t v = s.next_seq;
    osal_mutex_unlock(s.lock);
    return v;
}

uint32_t dl_oldest_seq(void)
{
    osal_mutex_lock(s.lock);
    uint32_t v = oldest_locked();
    osal_mutex_unlock(s.lock);
    return v;
}
