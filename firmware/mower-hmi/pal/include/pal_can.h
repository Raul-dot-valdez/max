/*
 * PAL CAN: classic CAN 2.0B, 29-bit identifiers (J1939).
 * Ported: STM32H7 FDCAN (classic mode). Candidates: NXP FlexCAN, Renesas RSCAN/CANFD,
 * Infineon MCMCAN — all map onto this surface.
 */
#ifndef PAL_CAN_H
#define PAL_CAN_H

#include "pal_types.h"

typedef struct {
    uint32_t id;      /* 29-bit when ext = 1 */
    uint8_t  ext;
    uint8_t  dlc;     /* 0..8 */
    uint8_t  data[8];
    uint32_t ts_ms;   /* receive timestamp, filled by the port */
} pal_can_frame_t;

typedef enum {
    PAL_CAN_STATE_ACTIVE,
    PAL_CAN_STATE_PASSIVE,
    PAL_CAN_STATE_BUS_OFF,
} pal_can_state_t;

/* Called from interrupt context. Keep it short: copy into a queue. */
typedef void (*pal_can_rx_cb_t)(const pal_can_frame_t *f, void *ctx);

pal_status_t    pal_can_init(uint8_t bus, uint32_t bitrate);
/* Accept extended frames where (frame_id & mask) == (id & mask). */
pal_status_t    pal_can_add_filter_ext(uint8_t bus, uint8_t idx, uint32_t id, uint32_t mask);
pal_status_t    pal_can_start(uint8_t bus);
pal_status_t    pal_can_send(uint8_t bus, const pal_can_frame_t *f);
void            pal_can_set_rx_callback(uint8_t bus, pal_can_rx_cb_t cb, void *ctx);
pal_can_state_t pal_can_state(uint8_t bus);
/* Bus-off recovery: port restarts the controller after 128x11 recessive bits. */
pal_status_t    pal_can_recover(uint8_t bus);

#endif /* PAL_CAN_H */
