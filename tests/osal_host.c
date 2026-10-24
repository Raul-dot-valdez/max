/* Single-threaded OSAL for host unit tests: time is controlled by the test. */
#include <stdlib.h>
#include <string.h>

#include "osal.h"

uint32_t g_host_ms;

struct osal_queue {
    size_t   depth, item, head, count;
    uint8_t *buf;
};

osal_mutex_t osal_mutex_create(void) { return (osal_mutex_t)1; }
void osal_mutex_lock(osal_mutex_t m) { (void)m; }
void osal_mutex_unlock(osal_mutex_t m) { (void)m; }

osal_queue_t osal_queue_create(size_t depth, size_t item)
{
    osal_queue_t q = calloc(1, sizeof(*q));
    q->depth = depth;
    q->item = item;
    q->buf = calloc(depth, item);
    return q;
}

bool osal_queue_send(osal_queue_t q, const void *it, uint32_t t)
{
    (void)t;
    if (q->count == q->depth) return false;
    memcpy(q->buf + ((q->head + q->count) % q->depth) * q->item, it, q->item);
    q->count++;
    return true;
}

bool osal_queue_send_isr(osal_queue_t q, const void *it) { return osal_queue_send(q, it, 0); }

bool osal_queue_recv(osal_queue_t q, void *it, uint32_t t)
{
    (void)t;
    if (!q->count) return false;
    memcpy(it, q->buf + q->head * q->item, q->item);
    q->head = (q->head + 1) % q->depth;
    q->count--;
    return true;
}

size_t osal_queue_count(osal_queue_t q) { return q->count; }
bool osal_task_create(osal_task_fn_t fn, const char *n, uint32_t s, uint8_t p, void *a)
{ (void)fn; (void)n; (void)s; (void)p; (void)a; return true; }
void osal_start(void) {}
uint32_t osal_now_ms(void) { return g_host_ms; }
void osal_sleep_ms(uint32_t ms) { g_host_ms += ms; }
