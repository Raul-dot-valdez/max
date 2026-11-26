/*
 * Non-volatile key/value store in two internal-flash sectors (A/B).
 *
 * Log-structured: a write appends a 64-byte record (two STM32H7 256-bit
 * flash words, so it never re-programs a word and never breaks ECC). When the
 * active sector fills, live values are compacted into the other sector and
 * its header is written last, so a power cut during compaction leaves the old
 * sector authoritative.
 *
 * Endurance: 2047 records per 128 KB sector. Counters saved every 6 min of
 * key-on time = ~10 writes/h -> one erase per ~200 h per sector; with 10k
 * cycles that is ~4 million operating hours.
 */
#ifndef NVSTORE_H
#define NVSTORE_H

#include "flash_dev.h"

#define NV_VAL_MAX 48u

/* Key map — append only, never renumber (field units would mis-decode). */
enum {
    NV_KEY_COUNTERS      = 0x0001, /* fr_counters_t */
    NV_KEY_FUEL_CFG      = 0x0002,
    NV_KEY_LOG_ACK_SEQ   = 0x0003, /* last log sequence acknowledged by the fleet app */
    NV_KEY_MACHINE_ID    = 0x0004, /* VIN / OEM serial, ASCII */
    NV_KEY_BACKLIGHT     = 0x0005,
    NV_KEY_MACHINE_PROFILE = 0x0006, /* mp_family_t (uint8_t), set by auto-detect or the dealer */
    NV_KEY_GEOFENCE_BASE = 0x0100, /* + slot * 0x10 + chunk */
    NV_KEY_ML_MODEL_BASE = 0x0200, /* anomaly model blob, chunked */
};

/* dev addresses: sec_a and sec_b are byte offsets of the two sectors in dev. */
pal_status_t nv_init(flash_dev_t *dev, uint32_t sec_a, uint32_t sec_b);
pal_status_t nv_set(uint16_t key, const void *val, uint8_t len);
int          nv_get(uint16_t key, void *val, uint8_t max_len); /* length, or -1 if absent */
/* Blobs larger than NV_VAL_MAX are split across consecutive keys. */
pal_status_t nv_set_blob(uint16_t key_base, const void *data, size_t len);
pal_status_t nv_get_blob(uint16_t key_base, void *data, size_t len);
uint32_t     nv_generation(void);

#endif
