#include "ui_model.h"

#include <string.h>

#include "osal.h"

#define CMD_Q_DEPTH 8

static struct {
    osal_mutex_t   lock;
    osal_queue_t   cmdq;
    ui_model_t     m;
    uint32_t       version;
    ui_notify_fn_t notify;
} s;

void ui_model_init(void)
{
    memset(&s, 0, sizeof(s));
    s.lock = osal_mutex_create();
    s.cmdq = osal_queue_create(CMD_Q_DEPTH, sizeof(ui_cmd_t));
}

void ui_model_publish(const ui_model_t *m)
{
    osal_mutex_lock(s.lock);
    bool changed = memcmp(&s.m, m, sizeof(*m)) != 0;
    if (changed) {
        s.m = *m;
        s.version++;
    }
    ui_notify_fn_t fn = s.notify;
    osal_mutex_unlock(s.lock);
    if (changed && fn)
        fn(m);
}

bool ui_model_fetch(ui_model_t *out, uint32_t *last_version)
{
    bool fresh = false;
    osal_mutex_lock(s.lock);
    if (s.version != *last_version) {
        *out = s.m;
        *last_version = s.version;
        fresh = true;
    }
    osal_mutex_unlock(s.lock);
    return fresh;
}

void ui_model_set_notify(ui_notify_fn_t fn)
{
    osal_mutex_lock(s.lock);
    s.notify = fn;
    osal_mutex_unlock(s.lock);
}

void ui_cmd_post(ui_cmd_type_t type, int32_t arg)
{
    ui_cmd_t c = { type, arg };
    osal_queue_send(s.cmdq, &c, 0);
}

bool ui_cmd_take(ui_cmd_t *out)
{
    return osal_queue_recv(s.cmdq, out, 0);
}
