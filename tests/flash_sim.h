/*
 * NOR flash simulator: erase sets 0xFF, program can only clear bits (AND),
 * like real NOR. Optional power-cut injection tears a program operation.
 */
#ifndef FLASH_SIM_H
#define FLASH_SIM_H

#include <stdlib.h>
#include <string.h>

#include "flash_dev.h"

typedef struct {
    uint8_t *mem;
    int      cut_after_bytes; /* < 0: disabled; else program stops after N bytes, once */
    uint32_t erases, progs;
    bool     strict;          /* fail programs that would need a 0->1 transition */
} flash_sim_t;

static pal_status_t sim_read(flash_dev_t *d, uint32_t a, void *b, size_t n)
{
    flash_sim_t *s = d->ctx;
    if (a + n > d->size) return PAL_EINVAL;
    memcpy(b, s->mem + a, n);
    return PAL_OK;
}

static pal_status_t sim_prog(flash_dev_t *d, uint32_t a, const void *b, size_t n)
{
    flash_sim_t *s = d->ctx;
    const uint8_t *p = b;
    if (a + n > d->size) return PAL_EINVAL;
    s->progs++;
    for (size_t i = 0; i < n; i++) {
        if (s->cut_after_bytes == 0) { s->cut_after_bytes = -1; return PAL_ERR; }
        if (s->cut_after_bytes > 0) s->cut_after_bytes--;
        if (s->strict && (p[i] & ~s->mem[a + i])) return PAL_ERR;
        s->mem[a + i] &= p[i];
    }
    return PAL_OK;
}

static pal_status_t sim_erase(flash_dev_t *d, uint32_t a)
{
    flash_sim_t *s = d->ctx;
    if (a % d->erase_size || a >= d->size) return PAL_EINVAL;
    memset(s->mem + a, 0xFF, d->erase_size);
    s->erases++;
    return PAL_OK;
}

static inline void flash_sim_create(flash_dev_t *d, flash_sim_t *s, uint32_t size, uint32_t erase, uint32_t prog)
{
    memset(s, 0, sizeof(*s));
    s->mem = malloc(size);
    memset(s->mem, 0xFF, size);
    s->cut_after_bytes = -1;
    s->strict = true;
    *d = (flash_dev_t){ .size = size, .erase_size = erase, .prog_size = prog,
                        .read = sim_read, .prog = sim_prog, .erase = sim_erase, .ctx = s };
}

#endif
