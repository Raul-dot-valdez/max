/*
 * PAL — Platform Abstraction Layer: common types.
 *
 * The PAL is the only layer that may include vendor headers. Everything
 * above it (drivers, middleware, services, app, GUI bridge) compiles against
 * these headers only. The prefix is "pal_" rather than "hal_" on purpose:
 * STM32Cube, NXP MCUXpresso and Renesas FSP all already own a HAL_ namespace,
 * and a port file has to include both without symbol clashes.
 */
#ifndef PAL_TYPES_H
#define PAL_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    PAL_OK      = 0,
    PAL_ERR     = -1,
    PAL_TIMEOUT = -2,
    PAL_BUSY    = -3,
    PAL_EINVAL  = -4,
    PAL_ENOSPC  = -5,
    PAL_ECRC    = -6,
    PAL_ENOENT  = -7,
    PAL_EAUTH   = -8,
} pal_status_t;

#define PAL_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#if defined(__GNUC__)
#define PAL_PACKED       __attribute__((packed))
#define PAL_SECTION(s)   __attribute__((section(s)))
#define PAL_ALIGNED(n)   __attribute__((aligned(n)))
#define PAL_WEAK         __attribute__((weak))
#else
#error "Add compiler attribute mappings for this toolchain"
#endif

/* Memory placement. On the host build these collapse to ordinary .bss. */
#if defined(PAL_HOST)
#define PAL_DMA_BSS
#define PAL_SDRAM_BSS
#define PAL_FAST_BSS
#else
#define PAL_DMA_BSS   PAL_SECTION(".dma_bss") PAL_ALIGNED(32)  /* non-cacheable, DMA reachable */
#define PAL_SDRAM_BSS PAL_SECTION(".sdram_bss") PAL_ALIGNED(32) /* external SDRAM, zeroed after FMC init */
#define PAL_FAST_BSS  PAL_SECTION(".dtcm_bss")                 /* tightly-coupled, CPU-only */
#endif

#endif /* PAL_TYPES_H */
