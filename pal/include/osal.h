/*
 * OSAL — thin RTOS abstraction. FreeRTOS today (pal/osal/osal_freertos.c);
 * Zephyr or ThreadX can be dropped in without touching services or app code.
 * The host test build uses tests/osal_host.c.
 */
#ifndef OSAL_H
#define OSAL_H

#include "pal_types.h"

#define OSAL_WAIT_FOREVER 0xFFFFFFFFu

typedef struct osal_mutex *osal_mutex_t;
typedef struct osal_queue *osal_queue_t;
typedef void (*osal_task_fn_t)(void *arg);

osal_mutex_t osal_mutex_create(void);
void         osal_mutex_lock(osal_mutex_t m);
void         osal_mutex_unlock(osal_mutex_t m);

osal_queue_t osal_queue_create(size_t depth, size_t item_size);
bool         osal_queue_send(osal_queue_t q, const void *item, uint32_t timeout_ms);
bool         osal_queue_send_isr(osal_queue_t q, const void *item); /* never blocks */
bool         osal_queue_recv(osal_queue_t q, void *item, uint32_t timeout_ms);
size_t       osal_queue_count(osal_queue_t q);

bool         osal_task_create(osal_task_fn_t fn, const char *name, uint32_t stack_bytes,
                              uint8_t prio, void *arg);
void         osal_start(void);          /* does not return */
uint32_t     osal_now_ms(void);
void         osal_sleep_ms(uint32_t ms); /* falls back to busy-wait before the scheduler runs */

#endif /* OSAL_H */
