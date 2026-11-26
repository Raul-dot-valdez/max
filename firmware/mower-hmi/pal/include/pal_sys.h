/*
 * PAL system: clocks, caches/MPU, external SDRAM bring-up, watchdog,
 * reset cause, battery-backed RAM.
 */
#ifndef PAL_SYS_H
#define PAL_SYS_H

#include "pal_types.h"

typedef enum {
    PAL_RESET_POWER_ON,
    PAL_RESET_BROWNOUT,
    PAL_RESET_WATCHDOG,
    PAL_RESET_SOFTWARE,
    PAL_RESET_PIN,
    PAL_RESET_OTHER,
} pal_reset_cause_t;

/* Clocks, MPU, caches, FMC SDRAM. Must run before any .sdram_bss is touched. */
void              pal_sys_init(void);
pal_status_t      pal_sdram_init(void);
pal_status_t      pal_sdram_selftest(uint32_t bytes); /* walking-ones + address-line test */
pal_reset_cause_t pal_sys_reset_cause(void);
void              pal_sys_reset(void);
void              pal_sys_delay_ms(uint32_t ms);      /* busy wait, usable before the scheduler */
uint32_t          pal_sys_uid32(void);                /* folded silicon unique ID */

/* Battery/backup-domain RAM that survives resets (and VBAT-powered off-time). */
void    *pal_sys_backup_ram(size_t *size);

void pal_wdg_init(uint32_t timeout_ms);
void pal_wdg_kick(void);

/* Cache maintenance for buffers shared with bus masters. No-ops on cacheless cores. */
void pal_cache_clean(const void *addr, size_t len);
void pal_cache_invalidate(void *addr, size_t len);

#endif /* PAL_SYS_H */
