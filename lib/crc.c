/*
 * Portable table-less CRCs. Throughput is ample for 64-byte log records and
 * link frames; a port may swap in the MCU's CRC peripheral behind the same API.
 */
#include "crc.h"

uint16_t crc16_ccitt(const void *data, size_t len, uint16_t crc)
{
    const uint8_t *p = data;
    while (len--) {
        crc ^= (uint16_t)(*p++) << 8;
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

uint32_t crc32_ieee(const void *data, size_t len, uint32_t crc)
{
    const uint8_t *p = data;
    crc = ~crc;
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++)
            crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }
    return ~crc;
}
