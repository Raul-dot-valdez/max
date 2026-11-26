/*
 * TouchGFX backend hooks for app_tasks.
 *
 * Asset strategy (see docs/architecture.md, "External flash arbitration"):
 * bitmaps live in the W25Q "ExtFlashSection" and are copied into the SDRAM
 * bitmap cache once, while the QSPI is memory-mapped; afterwards the QSPI is
 * returned to indirect mode for the data logger. Fonts and texts stay in
 * internal flash so nothing reads the QSPI at runtime.
 */
#include <touchgfx/Bitmap.hpp>

extern "C" {
#include "app_tasks.h"
#include "board_config.h"

void MX_TouchGFX_Init(void);          /* CubeMX/TouchGFX generated */
void touchgfx_taskEntry(void);        /* CubeMX/TouchGFX generated, never returns */
}

extern "C" void gui_preload_assets(void)
{
    MX_TouchGFX_Init();
    touchgfx::Bitmap::setCache(reinterpret_cast<uint16_t*>(BOARD_GUI_CACHE_ADDR), BOARD_GUI_CACHE_SIZE);
    touchgfx::Bitmap::cacheAll();
}

extern "C" void gui_task_entry(void* arg)
{
    (void)arg;
    touchgfx_taskEntry();
}
