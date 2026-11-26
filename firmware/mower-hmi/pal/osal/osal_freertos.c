/* OSAL on FreeRTOS. Static allocation is used for queues/mutexes in a later
 * hardening pass (configSUPPORT_STATIC_ALLOCATION); heap_4 for now. */
#include "osal.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "pal_sys.h"

/* configAPPLICATION_ALLOCATED_HEAP: heap_4 arena placed in AXI SRAM by the linker */
uint8_t ucHeap[configTOTAL_HEAP_SIZE] __attribute__((section(".freertos_heap"), aligned(8)));

osal_mutex_t osal_mutex_create(void)
{
    return (osal_mutex_t)xSemaphoreCreateMutex();
}

void osal_mutex_lock(osal_mutex_t m)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xSemaphoreTake((SemaphoreHandle_t)m, portMAX_DELAY);
}

void osal_mutex_unlock(osal_mutex_t m)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xSemaphoreGive((SemaphoreHandle_t)m);
}

osal_queue_t osal_queue_create(size_t depth, size_t item_size)
{
    return (osal_queue_t)xQueueCreate(depth, item_size);
}

static TickType_t ticks(uint32_t ms)
{
    return ms == OSAL_WAIT_FOREVER ? portMAX_DELAY : pdMS_TO_TICKS(ms);
}

bool osal_queue_send(osal_queue_t q, const void *item, uint32_t timeout_ms)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
        timeout_ms = 0;
    return xQueueSend((QueueHandle_t)q, item, ticks(timeout_ms)) == pdPASS;
}

bool osal_queue_send_isr(osal_queue_t q, const void *item)
{
    BaseType_t woken = pdFALSE;
    BaseType_t ok = xQueueSendFromISR((QueueHandle_t)q, item, &woken);
    portYIELD_FROM_ISR(woken);
    return ok == pdPASS;
}

bool osal_queue_recv(osal_queue_t q, void *item, uint32_t timeout_ms)
{
    return xQueueReceive((QueueHandle_t)q, item, ticks(timeout_ms)) == pdPASS;
}

size_t osal_queue_count(osal_queue_t q)
{
    return (size_t)uxQueueMessagesWaiting((QueueHandle_t)q);
}

bool osal_task_create(osal_task_fn_t fn, const char *name, uint32_t stack_bytes, uint8_t prio, void *arg)
{
    return xTaskCreate(fn, name, (configSTACK_DEPTH_TYPE)(stack_bytes / sizeof(StackType_t)), arg,
                       prio, NULL) == pdPASS;
}

void osal_start(void) { vTaskStartScheduler(); }

uint32_t osal_now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void osal_sleep_ms(uint32_t ms)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
        pal_sys_delay_ms(ms);
    else
        vTaskDelay(pdMS_TO_TICKS(ms) ? pdMS_TO_TICKS(ms) : 1);
}

/* ---- FreeRTOS hooks: fail safe, let the IWDG reset us ------------------- */

void vApplicationStackOverflowHook(TaskHandle_t t, char *name)
{
    (void)t; (void)name;
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}
