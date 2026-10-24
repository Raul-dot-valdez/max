/*
 * STM32 <-> PSoC 4 BLE inter-processor link: COBS-framed, CRC-16 protected.
 *
 * The codec is pure C with no PAL dependency, so the same file is compiled
 * into the PSoC 4 firmware (psoc4_ble/) — one framing implementation, two CPUs.
 *
 *   wire:  COBS( type | seq | payload[0..LINK_MAX_PAYLOAD] | crc16_le ) 0x00
 *
 * LINK_MAX_PAYLOAD = 240 so one frame fits one BLE 4.2 notification at the
 * maximum ATT MTU of 247 (244-byte value) with Data Length Extension.
 */
#ifndef PSOC_LINK_H
#define PSOC_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LINK_MAX_PAYLOAD 240u
#define LINK_MAX_RAW     (2u + LINK_MAX_PAYLOAD + 2u)
#define LINK_MAX_ENCODED (LINK_MAX_RAW + LINK_MAX_RAW / 254u + 2u)

typedef enum {
    LINK_T_PING        = 0x01, /* either direction, payload echoed */
    LINK_T_STATUS      = 0x02, /* PSoC -> STM: [connected, encrypted, bonded, mtu_lo, mtu_hi] */
    LINK_T_CREDIT      = 0x03, /* PSoC -> STM: [n] notifications completed, may send n more */
    LINK_T_DATA_TO_APP = 0x10, /* STM -> PSoC -> phone (GATT notification) */
    LINK_T_DATA_FROM_APP = 0x11, /* phone -> PSoC -> STM (GATT write) */
    LINK_T_ADV_CONFIG  = 0x20, /* STM -> PSoC: device name / machine ID for advertising */
    LINK_T_PAIR_CONFIRM = 0x21, /* PSoC -> STM: [u32 LE] 6-digit LESC numeric-comparison code */
    LINK_T_PAIR_REPLY  = 0x22, /* STM -> PSoC: [1 accept | 0 reject] from the operator */
} link_type_t;

typedef struct {
    uint8_t  type;
    uint8_t  seq;
    uint16_t len;
    uint8_t  payload[LINK_MAX_PAYLOAD];
} link_frame_t;

typedef struct {
    uint8_t buf[LINK_MAX_ENCODED];
    size_t  n;
    uint32_t crc_errors;
    uint32_t overruns;
} link_decoder_t;

/* Returns encoded length including trailing 0x00, or 0 if out_max too small. */
size_t link_encode(const link_frame_t *f, uint8_t *out, size_t out_max);
void   link_decoder_init(link_decoder_t *d);
/* Feed one byte; returns true when *out holds a complete, CRC-valid frame. */
bool   link_decode_byte(link_decoder_t *d, uint8_t b, link_frame_t *out);

#endif
