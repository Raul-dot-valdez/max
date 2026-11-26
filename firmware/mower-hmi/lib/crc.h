#ifndef CRC_H
#define CRC_H

#include <stddef.h>
#include <stdint.h>

/* CRC-16/CCITT-FALSE: poly 0x1021, start with 0xFFFF. */
uint16_t crc16_ccitt(const void *data, size_t len, uint16_t crc);
/* CRC-32/ISO-HDLC (zlib). Pass 0 to start; chainable. */
uint32_t crc32_ieee(const void *data, size_t len, uint32_t crc);

#endif
