#include "fleet_ble.h"

#include <string.h>

#include "psoc_link.h"

#define RECS_PER_CHUNK 3u  /* 3 + 5 + 3*64 = 200 bytes <= LINK_MAX_PAYLOAD */
#define MAX_CREDITS    16u

static struct {
    const fleet_ble_ops_t *ops;
    bool     connected, encrypted;
    uint8_t  credits;
    bool     streaming;
    uint8_t  req;
    uint32_t next, end;
} s;

static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

static void frame(uint8_t code, uint8_t req, uint8_t status, const void *p, size_t n)
{
    uint8_t buf[LINK_MAX_PAYLOAD];
    if (n > sizeof(buf) - 3)
        return;
    buf[0] = code;
    buf[1] = req;
    buf[2] = status;
    if (n)
        memcpy(&buf[3], p, n);
    s.ops->send(buf, n + 3);
}

static void reply(uint8_t cmd, uint8_t req, uint8_t status, const void *p, size_t n)
{
    frame((uint8_t)(cmd | 0x80), req, status, p, n);
}

void fleet_ble_init(const fleet_ble_ops_t *ops)
{
    memset(&s, 0, sizeof(s));
    s.ops = ops;
}

void fleet_ble_on_status(bool connected, bool encrypted)
{
    if (!connected) {
        s.streaming = false;
        s.credits = 0;
    }
    s.connected = connected;
    s.encrypted = connected && encrypted;
}

void fleet_ble_on_credit(uint8_t n)
{
    unsigned c = s.credits + n;
    s.credits = (uint8_t)(c > MAX_CREDITS ? MAX_CREDITS : c);
}

void fleet_ble_on_rx(const uint8_t *p, size_t n)
{
    if (n < 2)
        return;
    uint8_t cmd = p[0], req = p[1];
    const uint8_t *a = p + 2;
    size_t an = n - 2;

    if (!s.encrypted) {
        reply(cmd, req, FB_ERR_AUTH, NULL, 0);
        return;
    }

    switch (cmd) {
    case FB_CMD_GET_INFO: {
        fb_info_t info;
        memset(&info, 0, sizeof(info));
        s.ops->get_info(&info);
        reply(cmd, req, FB_OK, &info, sizeof(info));
        break;
    }
    case FB_CMD_GET_SUMMARY: {
        fb_summary_t sum;
        memset(&sum, 0, sizeof(sum));
        s.ops->get_summary(&sum);
        reply(cmd, req, FB_OK, &sum, sizeof(sum));
        break;
    }
    case FB_CMD_READ_LOG: {
        if (an < 6) { reply(cmd, req, FB_ERR_ARGS, NULL, 0); break; }
        uint32_t from = rd32(a), oldest = dl_oldest_seq(), nxt = dl_next_seq();
        uint16_t max = rd16(a + 4);
        if (from < oldest) from = oldest;
        uint32_t end = (nxt - from > max) ? from + max : nxt;
        s.streaming = from < end;
        s.next = from;
        s.end = end;
        s.req = req;
        uint8_t r[8];
        wr32(r, from);
        wr32(r + 4, end);
        reply(cmd, req, FB_OK, r, sizeof(r));
        if (!s.streaming) {
            wr32(r, nxt);
            frame(FB_EVT_LOG_END, req, FB_OK, r, 4);
        }
        break;
    }
    case FB_CMD_ACK_LOG:
        if (an < 4) { reply(cmd, req, FB_ERR_ARGS, NULL, 0); break; }
        s.ops->ack_log(rd32(a));
        reply(cmd, req, FB_OK, NULL, 0);
        break;
    case FB_CMD_SET_TIME:
        if (an < 4 || rd32(a) < 1700000000u) { reply(cmd, req, FB_ERR_ARGS, NULL, 0); break; }
        s.ops->set_time(rd32(a));
        reply(cmd, req, FB_OK, NULL, 0);
        break;
    case FB_CMD_SET_GEOFENCE: {
        geofence_t g;
        const size_t hdr = offsetof(geofence_t, pt);
        if (an < 1 + hdr) { reply(cmd, req, FB_ERR_ARGS, NULL, 0); break; }
        memset(&g, 0, sizeof(g));
        memcpy(&g, a + 1, hdr);
        size_t need = 1 + hdr + (size_t)g.n * sizeof(g.pt[0]);
        if (g.n > GEOFENCE_MAX_PTS || an < need) { reply(cmd, req, FB_ERR_ARGS, NULL, 0); break; }
        memcpy(g.pt, a + 1 + hdr, (size_t)g.n * sizeof(g.pt[0]));
        reply(cmd, req, s.ops->set_geofence(a[0], &g) ? FB_OK : FB_ERR_ARGS, NULL, 0);
        break;
    }
    case FB_CMD_SET_PARAM:
        if (an < 6) { reply(cmd, req, FB_ERR_ARGS, NULL, 0); break; }
        reply(cmd, req, s.ops->set_param(rd16(a), (int32_t)rd32(a + 2)) ? FB_OK : FB_ERR_ARGS, NULL, 0);
        break;
    case FB_CMD_STOP:
        s.streaming = false;
        reply(cmd, req, FB_OK, NULL, 0);
        break;
    default:
        reply(cmd, req, FB_ERR_UNKNOWN, NULL, 0);
        break;
    }
}

void fleet_ble_poll(void)
{
    uint8_t buf[5 + RECS_PER_CHUNK * DL_RECORD_SIZE];
    while (s.streaming && s.credits > 0) {
        uint8_t count = 0;
        uint32_t first = s.next;
        while (count < RECS_PER_CHUNK && s.next < s.end) {
            dl_record_t r;
            pal_status_t st = dl_read(s.next, &r);
            if (st == PAL_ENOENT && s.next < dl_oldest_seq()) {
                /* overwritten while streaming: jump forward, the app sees the gap in seq */
                s.next = dl_oldest_seq();
                if (count == 0) first = s.next;
                else break;
                continue;
            }
            s.next++;
            if (st != PAL_OK) {
                if (count == 0) first = s.next;
                else break;
                continue; /* unreadable slot: skip */
            }
            memcpy(&buf[5 + count * DL_RECORD_SIZE], &r, DL_RECORD_SIZE);
            count++;
        }
        if (count) {
            wr32(buf, first);
            buf[4] = count;
            frame(FB_EVT_LOG_CHUNK, s.req, FB_OK, buf, 5u + count * DL_RECORD_SIZE);
            s.credits--;
        }
        if (s.next >= s.end) {
            wr32(buf, s.next);
            frame(FB_EVT_LOG_END, s.req, FB_OK, buf, 4);
            s.streaming = false;
        }
    }
}
