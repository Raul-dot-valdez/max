/*
 * PAL display on STM32H7 LTDC, layer 1 only. Framebuffers in SDRAM
 * (MPU: write-through region) so CPU writes reach the LTDC without explicit
 * cache cleans; TouchGFX draws mostly through DMA2D anyway.
 */
#include "pal_display.h"
#include "pal_gpio.h"
#include "port_stm32h7.h"

LTDC_HandleTypeDef hltdc;

static pal_vsync_cb_t s_vsync;
static void          *s_ctx;
static uint16_t       s_vblank_line;

pal_status_t pal_display_init(const pal_panel_timing_t *t, pal_pixfmt_t fmt, uintptr_t fb)
{
    hltdc.Instance = LTDC;
    hltdc.Init.HSPolarity = t->hsync_active_high ? LTDC_HSPOLARITY_AH : LTDC_HSPOLARITY_AL;
    hltdc.Init.VSPolarity = t->vsync_active_high ? LTDC_VSPOLARITY_AH : LTDC_VSPOLARITY_AL;
    hltdc.Init.DEPolarity = t->de_active_high ? LTDC_DEPOLARITY_AH : LTDC_DEPOLARITY_AL;
    hltdc.Init.PCPolarity = t->pclk_inverted ? LTDC_PCPOLARITY_IIPC : LTDC_PCPOLARITY_IPC;
    hltdc.Init.HorizontalSync = t->hsync - 1;
    hltdc.Init.VerticalSync = t->vsync - 1;
    hltdc.Init.AccumulatedHBP = t->hsync + t->hbp - 1;
    hltdc.Init.AccumulatedVBP = t->vsync + t->vbp - 1;
    hltdc.Init.AccumulatedActiveW = t->hsync + t->hbp + t->width - 1;
    hltdc.Init.AccumulatedActiveH = t->vsync + t->vbp + t->height - 1;
    hltdc.Init.TotalWidth = t->hsync + t->hbp + t->width + t->hfp - 1;
    hltdc.Init.TotalHeigh = t->vsync + t->vbp + t->height + t->vfp - 1; /* sic, HAL field name */
    hltdc.Init.Backcolor.Red = 0;
    hltdc.Init.Backcolor.Green = 0;
    hltdc.Init.Backcolor.Blue = 0;
    if (HAL_LTDC_Init(&hltdc) != HAL_OK) return PAL_ERR;

    LTDC_LayerCfgTypeDef l = { 0 };
    l.WindowX0 = 0;
    l.WindowX1 = t->width;
    l.WindowY0 = 0;
    l.WindowY1 = t->height;
    l.PixelFormat = fmt == PAL_PIXFMT_RGB565 ? LTDC_PIXEL_FORMAT_RGB565
                  : fmt == PAL_PIXFMT_RGB888 ? LTDC_PIXEL_FORMAT_RGB888 : LTDC_PIXEL_FORMAT_ARGB8888;
    l.Alpha = 255;
    l.Alpha0 = 0;
    l.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
    l.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
    l.FBStartAdress = (uint32_t)fb; /* sic */
    l.ImageWidth = t->width;
    l.ImageHeight = t->height;
    if (HAL_LTDC_ConfigLayer(&hltdc, &l, 0) != HAL_OK) return PAL_ERR;

    s_vblank_line = (uint16_t)(t->vsync + t->vbp + t->height);
    HAL_LTDC_ProgramLineEvent(&hltdc, s_vblank_line);
    return PAL_OK;
}

void pal_display_set_framebuffer(uintptr_t fb)
{
    HAL_LTDC_SetAddress_NoReload(&hltdc, (uint32_t)fb, 0);
    HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_VERTICAL_BLANKING);
}

void pal_display_set_vsync_callback(pal_vsync_cb_t cb, void *ctx)
{
    s_vsync = cb;
    s_ctx = ctx;
}

void pal_display_backlight(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim_backlight);
    __HAL_TIM_SET_COMPARE(&htim_backlight, TIM_CHANNEL_1, arr * percent / 100u);
    pal_gpio_write(PAL_PIN_BACKLIGHT_EN, percent != 0);
}

void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef *h)
{
    HAL_LTDC_ProgramLineEvent(h, s_vblank_line); /* line event is one-shot */
    if (s_vsync) s_vsync(s_ctx);
}

void LTDC_IRQHandler(void) { HAL_LTDC_IRQHandler(&hltdc); }
