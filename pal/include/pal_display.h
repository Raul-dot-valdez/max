/*
 * PAL display: parallel-RGB controller with a single scan-out layer.
 * STM32H7: LTDC (+DMA2D used directly by TouchGFX's ST HAL).
 * NXP RT1170: eLCDIF/LCDIFv2 + PXP. Renesas RA8D1: GLCDC + DRW.
 */
#ifndef PAL_DISPLAY_H
#define PAL_DISPLAY_H

#include "pal_types.h"

typedef enum { PAL_PIXFMT_RGB565, PAL_PIXFMT_RGB888, PAL_PIXFMT_ARGB8888 } pal_pixfmt_t;

typedef struct {
    uint16_t width, height;
    uint16_t hsync, hbp, hfp;
    uint16_t vsync, vbp, vfp;
    uint32_t pixel_clock_hz;
    bool     hsync_active_high, vsync_active_high, de_active_high, pclk_inverted;
} pal_panel_timing_t;

typedef void (*pal_vsync_cb_t)(void *ctx); /* ISR context, once per frame */

pal_status_t pal_display_init(const pal_panel_timing_t *t, pal_pixfmt_t fmt, uintptr_t fb);
/* Latched at the next vertical blank: tear-free double buffering. */
void         pal_display_set_framebuffer(uintptr_t fb);
void         pal_display_set_vsync_callback(pal_vsync_cb_t cb, void *ctx);
void         pal_display_backlight(uint8_t percent);

#endif /* PAL_DISPLAY_H */
