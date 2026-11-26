#ifndef APP_TASKS_H
#define APP_TASKS_H

#include "pal_types.h"

#define FW_VERSION_MAJOR 0
#define FW_VERSION_MINOR 1
#define FW_VERSION_PATCH 0

/* Watchdog supervision: every task sets its bit; the supervisor only kicks
 * the IWDG when all bits were seen within the window. */
#define APP_HB_CAN   (1u << 0)
#define APP_HB_GNSS  (1u << 1)
#define APP_HB_APP   (1u << 2)
#define APP_HB_LOG   (1u << 3)
#define APP_HB_BLE   (1u << 4)
#define APP_HB_GUI   (1u << 5)
#define APP_HB_ALL   0x3Fu

void     app_init(void);   /* hardware + services; before the scheduler */
void     app_start(void);  /* creates tasks */
void     app_heartbeat(uint32_t bit);
uint32_t app_utc(void);
void     app_log_event(uint16_t code, int32_t a, int32_t b);

/* Provided by the linked GUI backend (gui/touchgfx or gui/qtmcu). */
void gui_preload_assets(void); /* runs with external NOR memory-mapped, before the scheduler */
void gui_task_entry(void *arg);

#endif
