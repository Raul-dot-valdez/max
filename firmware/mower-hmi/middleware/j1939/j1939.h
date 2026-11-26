/*
 * SAE J1939 data link (J1939-21) and network management (J1939-81) for an
 * HMI node: address claim with arbitrary-address fallback, Request/NACK,
 * and Transport Protocol receive (BAM + RTS/CTS, up to 1785 bytes) so
 * multi-packet DM1 fault lists from the engine ECU are reassembled.
 *
 * Not tied to any MCU: frames come in through j1939_on_can_frame() and go
 * out through the tx callback in j1939_cfg_t.
 */
#ifndef J1939_H
#define J1939_H

#include "pal_can.h"

#define J1939_ADDR_GLOBAL   0xFFu
#define J1939_ADDR_NULL     0xFEu
#define J1939_TP_MAX        1785u

#define J1939_PGN_REQUEST   0x0EA00u /* 59904 */
#define J1939_PGN_ACK       0x0E800u /* 59392 */
#define J1939_PGN_ADDR_CLM  0x0EE00u /* 60928 */
#define J1939_PGN_TP_CM     0x0EC00u /* 60416 */
#define J1939_PGN_TP_DT     0x0EB00u /* 60160 */

typedef struct {
    uint32_t       pgn;
    uint8_t        prio, sa, da;
    uint16_t       len;
    const uint8_t *data;
    uint32_t       ts_ms;
} j1939_msg_t;

typedef void (*j1939_rx_fn_t)(const j1939_msg_t *m, void *ctx);
/* Return true if the request was answered; false makes the stack NACK it
 * (only for destination-specific requests, per J1939-21). */
typedef bool (*j1939_req_fn_t)(uint32_t pgn, uint8_t requester, void *ctx);
typedef pal_status_t (*j1939_tx_fn_t)(const pal_can_frame_t *f);

typedef struct {
    uint64_t       name;
    uint8_t        preferred_sa;
    bool           arbitrary_capable;
    j1939_tx_fn_t  tx;
    j1939_rx_fn_t  on_rx;
    j1939_req_fn_t on_request;
    void          *ctx;
} j1939_cfg_t;

typedef enum { J1939_AC_CLAIMING, J1939_AC_OK, J1939_AC_FAILED } j1939_ac_state_t;

uint64_t         j1939_make_name(uint32_t identity, uint16_t mfg, uint8_t ecu_inst, uint8_t func_inst,
                                 uint8_t func, uint8_t vsys, uint8_t vsys_inst, uint8_t industry,
                                 bool arbitrary);
void             j1939_init(const j1939_cfg_t *cfg, uint32_t now_ms);
void             j1939_on_can_frame(const pal_can_frame_t *f); /* task context */
void             j1939_tick(uint32_t now_ms);                  /* call every <= 50 ms */
pal_status_t     j1939_send(uint32_t pgn, uint8_t prio, uint8_t da, const uint8_t *d, uint8_t len);
pal_status_t     j1939_request(uint32_t pgn, uint8_t da);
uint8_t          j1939_address(void);
j1939_ac_state_t j1939_ac_state(void);

/* Exposed for tests */
uint32_t j1939_build_id(uint8_t prio, uint32_t pgn, uint8_t da, uint8_t sa);
void     j1939_parse_id(uint32_t id, uint32_t *pgn, uint8_t *prio, uint8_t *da, uint8_t *sa);

#endif
