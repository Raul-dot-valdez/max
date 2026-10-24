#include "j1939.h"

#include <string.h>

#define TP_CM_RTS     16
#define TP_CM_CTS     17
#define TP_CM_EOMA    19
#define TP_CM_BAM     32
#define TP_CM_ABORT   255

#define TP_ABORT_BUSY      1
#define TP_ABORT_RESOURCES 2
#define TP_ABORT_TIMEOUT   3

#define T_AC_MS       250u  /* address claim contention window */
#define T_TP_T1_MS    750u  /* between data packets */
#define T_TP_T2_MS    1250u /* after we sent CTS */
#define TP_SESSIONS   4
#define ARB_FIRST     128u
#define ARB_LAST      247u

typedef struct {
    bool     active, bam;
    uint8_t  sa, da;
    uint32_t pgn;
    uint16_t size;
    uint8_t  npkts, next, window_end, max_per_cts;
    uint32_t deadline;
    uint8_t  buf[J1939_TP_MAX];
} tp_rx_t;

static struct {
    j1939_cfg_t      cfg;
    uint8_t          sa;
    j1939_ac_state_t ac;
    uint32_t         ac_deadline;
    uint32_t         now;
    tp_rx_t          tp[TP_SESSIONS];
} s;

uint64_t j1939_make_name(uint32_t identity, uint16_t mfg, uint8_t ecu_inst, uint8_t func_inst,
                         uint8_t func, uint8_t vsys, uint8_t vsys_inst, uint8_t industry,
                         bool arbitrary)
{
    return ((uint64_t)(identity & 0x1FFFFF)) |
           ((uint64_t)(mfg & 0x7FF) << 21) |
           ((uint64_t)(ecu_inst & 0x7) << 32) |
           ((uint64_t)(func_inst & 0x1F) << 35) |
           ((uint64_t)func << 40) |
           ((uint64_t)(vsys & 0x7F) << 49) |
           ((uint64_t)(vsys_inst & 0xF) << 56) |
           ((uint64_t)(industry & 0x7) << 60) |
           ((uint64_t)(arbitrary ? 1 : 0) << 63);
}

uint32_t j1939_build_id(uint8_t prio, uint32_t pgn, uint8_t da, uint8_t sa)
{
    uint8_t pf = (uint8_t)(pgn >> 8);
    uint32_t id = ((uint32_t)(prio & 7) << 26) | (sa & 0xFFu);
    if (pf < 240)
        id |= ((pgn & 0x3FF00u) << 8) | ((uint32_t)da << 8);
    else
        id |= (pgn & 0x3FFFFu) << 8;
    return id;
}

void j1939_parse_id(uint32_t id, uint32_t *pgn, uint8_t *prio, uint8_t *da, uint8_t *sa)
{
    uint8_t pf = (uint8_t)(id >> 16), ps = (uint8_t)(id >> 8);
    uint32_t dp = (id >> 24) & 3u;
    *prio = (uint8_t)((id >> 26) & 7u);
    *sa = (uint8_t)id;
    if (pf < 240) {
        *pgn = (dp << 16) | ((uint32_t)pf << 8);
        *da = ps;
    } else {
        *pgn = (dp << 16) | ((uint32_t)pf << 8) | ps;
        *da = J1939_ADDR_GLOBAL;
    }
}

static pal_status_t raw_send(uint8_t prio, uint32_t pgn, uint8_t da, uint8_t sa,
                             const uint8_t *d, uint8_t len)
{
    pal_can_frame_t f = { .id = j1939_build_id(prio, pgn, da, sa), .ext = 1, .dlc = len };
    memcpy(f.data, d, len);
    return s.cfg.tx ? s.cfg.tx(&f) : PAL_ERR;
}

static void name_bytes(uint8_t out[8], uint64_t name)
{
    for (int i = 0; i < 8; i++)
        out[i] = (uint8_t)(name >> (8 * i));
}

static void send_claim(void)
{
    uint8_t d[8];
    name_bytes(d, s.cfg.name);
    raw_send(6, J1939_PGN_ADDR_CLM, J1939_ADDR_GLOBAL,
             s.ac == J1939_AC_FAILED ? J1939_ADDR_NULL : s.sa, d, 8);
}

static void start_claim(uint8_t sa)
{
    s.sa = sa;
    s.ac = J1939_AC_CLAIMING;
    s.ac_deadline = s.now + T_AC_MS;
    send_claim();
}

static void handle_claim(uint8_t sa, const uint8_t *d)
{
    if (sa != s.sa || s.ac == J1939_AC_FAILED)
        return;
    uint64_t other = 0;
    for (int i = 7; i >= 0; i--)
        other = (other << 8) | d[i];
    if (other == s.cfg.name)
        return; /* our own echo */
    if (s.cfg.name < other) {
        send_claim(); /* we win: defend */
        return;
    }
    /* we lose */
    if (s.cfg.arbitrary_capable) {
        uint8_t next = (s.sa >= ARB_FIRST && s.sa < ARB_LAST) ? (uint8_t)(s.sa + 1) : ARB_FIRST;
        if (next != s.cfg.preferred_sa && !(s.sa == ARB_LAST)) {
            start_claim(next);
            return;
        }
    }
    s.ac = J1939_AC_FAILED;
    send_claim(); /* "cannot claim" from the null address */
}

static void send_nack(uint32_t pgn, uint8_t requester)
{
    uint8_t d[8] = { 1, 0xFF, 0xFF, 0xFF, requester,
                     (uint8_t)pgn, (uint8_t)(pgn >> 8), (uint8_t)(pgn >> 16) };
    raw_send(6, J1939_PGN_ACK, J1939_ADDR_GLOBAL, s.sa, d, 8);
}

static void handle_request(uint8_t sa, uint8_t da, const uint8_t *d, uint8_t len)
{
    if (len < 3)
        return;
    uint32_t pgn = d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16);
    if (pgn == J1939_PGN_ADDR_CLM) {
        send_claim();
        return;
    }
    if (s.ac != J1939_AC_OK)
        return;
    bool handled = s.cfg.on_request && s.cfg.on_request(pgn, sa, s.cfg.ctx);
    if (!handled && da == s.sa)
        send_nack(pgn, sa);
}

/* ---- Transport protocol (receive) ------------------------------------- */

static tp_rx_t *tp_find(uint8_t sa, bool bam)
{
    for (int i = 0; i < TP_SESSIONS; i++)
        if (s.tp[i].active && s.tp[i].sa == sa && s.tp[i].bam == bam)
            return &s.tp[i];
    return NULL;
}

static tp_rx_t *tp_alloc(uint8_t sa, bool bam)
{
    tp_rx_t *t = tp_find(sa, bam); /* a new RTS/BAM from the same source replaces the old one */
    for (int i = 0; !t && i < TP_SESSIONS; i++)
        if (!s.tp[i].active)
            t = &s.tp[i];
    return t;
}

static void tp_cm_send(uint8_t da, uint8_t ctrl, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
                       uint32_t pgn)
{
    uint8_t d[8] = { ctrl, b1, b2, b3, b4, (uint8_t)pgn, (uint8_t)(pgn >> 8), (uint8_t)(pgn >> 16) };
    raw_send(7, J1939_PGN_TP_CM, da, s.sa, d, 8);
}

static void tp_send_cts(tp_rx_t *t)
{
    uint8_t remaining = (uint8_t)(t->npkts - t->next + 1);
    uint8_t n = remaining < t->max_per_cts ? remaining : t->max_per_cts;
    t->window_end = (uint8_t)(t->next + n - 1);
    t->deadline = s.now + T_TP_T2_MS;
    tp_cm_send(t->sa, TP_CM_CTS, n, t->next, 0xFF, 0xFF, t->pgn);
}

static void tp_deliver(tp_rx_t *t)
{
    j1939_msg_t m = { .pgn = t->pgn, .prio = 7, .sa = t->sa, .da = t->da,
                      .len = t->size, .data = t->buf, .ts_ms = s.now };
    if (!t->bam)
        tp_cm_send(t->sa, TP_CM_EOMA, (uint8_t)t->size, (uint8_t)(t->size >> 8), t->npkts, 0xFF, t->pgn);
    t->active = false;
    if (s.cfg.on_rx)
        s.cfg.on_rx(&m, s.cfg.ctx);
}

static void handle_tp_cm(uint8_t sa, uint8_t da, const uint8_t *d)
{
    uint32_t pgn = d[5] | ((uint32_t)d[6] << 8) | ((uint32_t)d[7] << 16);
    uint16_t size = (uint16_t)(d[1] | (d[2] << 8));
    uint8_t npkts = d[3];

    switch (d[0]) {
    case TP_CM_BAM:
    case TP_CM_RTS: {
        bool bam = d[0] == TP_CM_BAM;
        if (bam != (da == J1939_ADDR_GLOBAL))
            return;
        tp_rx_t *t = (size > 8 && size <= J1939_TP_MAX && npkts == (size + 6) / 7) ? tp_alloc(sa, bam) : NULL;
        if (!t) {
            if (!bam)
                tp_cm_send(sa, TP_CM_ABORT, TP_ABORT_RESOURCES, 0xFF, 0xFF, 0xFF, pgn);
            return;
        }
        memset(t, 0, offsetof(tp_rx_t, buf));
        t->active = true;
        t->bam = bam;
        t->sa = sa;
        t->da = da;
        t->pgn = pgn;
        t->size = size;
        t->npkts = npkts;
        t->next = 1;
        t->max_per_cts = (bam || d[4] == 0xFF || d[4] == 0) ? 16 : (d[4] < 16 ? d[4] : 16);
        t->deadline = s.now + T_TP_T1_MS;
        if (!bam)
            tp_send_cts(t);
        break;
    }
    case TP_CM_ABORT: {
        tp_rx_t *t = tp_find(sa, false);
        if (t && t->pgn == pgn)
            t->active = false;
        break;
    }
    default:
        break; /* CTS/EOMA only matter to a transmitter; we never send TP */
    }
}

static void handle_tp_dt(uint8_t sa, uint8_t da, const uint8_t *d, uint8_t len)
{
    tp_rx_t *t = tp_find(sa, da == J1939_ADDR_GLOBAL);
    if (!t || len < 8)
        return;
    if (d[0] != t->next) {
        if (!t->bam)
            tp_cm_send(sa, TP_CM_ABORT, TP_ABORT_TIMEOUT, 0xFF, 0xFF, 0xFF, t->pgn);
        t->active = false;
        return;
    }
    uint16_t off = (uint16_t)((d[0] - 1) * 7);
    uint16_t n = (uint16_t)(t->size - off < 7 ? t->size - off : 7);
    memcpy(&t->buf[off], &d[1], n);
    t->deadline = s.now + T_TP_T1_MS;
    if (t->next == t->npkts) {
        tp_deliver(t);
        return;
    }
    if (!t->bam && t->next == t->window_end) {
        t->next++;
        tp_send_cts(t);
        return;
    }
    t->next++;
}

/* ---- Public ----------------------------------------------------------- */

void j1939_init(const j1939_cfg_t *cfg, uint32_t now_ms)
{
    memset(&s, 0, sizeof(s));
    s.cfg = *cfg;
    s.now = now_ms;
    start_claim(cfg->preferred_sa);
}

void j1939_on_can_frame(const pal_can_frame_t *f)
{
    uint32_t pgn;
    uint8_t prio, da, sa;
    if (!f->ext)
        return;
    j1939_parse_id(f->id, &pgn, &prio, &da, &sa);
    if (f->ts_ms)
        s.now = f->ts_ms;

    bool for_us = da == J1939_ADDR_GLOBAL || da == s.sa;
    switch (pgn) {
    case J1939_PGN_ADDR_CLM:
        if (f->dlc == 8)
            handle_claim(sa, f->data);
        return;
    case J1939_PGN_REQUEST:
        if (for_us)
            handle_request(sa, da, f->data, f->dlc);
        return;
    case J1939_PGN_TP_CM:
        if (for_us && f->dlc == 8)
            handle_tp_cm(sa, da, f->data);
        return;
    case J1939_PGN_TP_DT:
        if (for_us)
            handle_tp_dt(sa, da, f->data, f->dlc);
        return;
    default:
        break;
    }
    if (!for_us || !s.cfg.on_rx)
        return;
    j1939_msg_t m = { .pgn = pgn, .prio = prio, .sa = sa, .da = da,
                      .len = f->dlc, .data = f->data, .ts_ms = s.now };
    s.cfg.on_rx(&m, s.cfg.ctx);
}

void j1939_tick(uint32_t now_ms)
{
    s.now = now_ms;
    if (s.ac == J1939_AC_CLAIMING && (int32_t)(now_ms - s.ac_deadline) >= 0)
        s.ac = J1939_AC_OK;
    for (int i = 0; i < TP_SESSIONS; i++) {
        tp_rx_t *t = &s.tp[i];
        if (t->active && (int32_t)(now_ms - t->deadline) > 0) {
            if (!t->bam)
                tp_cm_send(t->sa, TP_CM_ABORT, TP_ABORT_TIMEOUT, 0xFF, 0xFF, 0xFF, t->pgn);
            t->active = false;
        }
    }
}

pal_status_t j1939_send(uint32_t pgn, uint8_t prio, uint8_t da, const uint8_t *d, uint8_t len)
{
    if (s.ac != J1939_AC_OK)
        return PAL_BUSY;
    if (len > 8)
        return PAL_EINVAL; /* multi-packet transmit not needed by this node */
    return raw_send(prio, pgn, da, s.sa, d, len);
}

pal_status_t j1939_request(uint32_t pgn, uint8_t da)
{
    uint8_t d[3] = { (uint8_t)pgn, (uint8_t)(pgn >> 8), (uint8_t)(pgn >> 16) };
    return j1939_send(J1939_PGN_REQUEST, 6, da, d, 3);
}

uint8_t j1939_address(void) { return s.sa; }
j1939_ac_state_t j1939_ac_state(void) { return s.ac; }
