/*
 * PAL Quad-SPI / Octo-SPI command interface. Generic enough for Winbond,
 * Macronix, ISSI and Infineon NOR parts; the device driver (drivers/w25q)
 * owns the opcodes.
 */
#ifndef PAL_QSPI_H
#define PAL_QSPI_H

#include "pal_types.h"

typedef struct {
    uint8_t  instr;
    uint8_t  instr_lines;  /* 1, 2 or 4 */
    uint8_t  addr_bytes;   /* 0, 3 or 4 */
    uint8_t  addr_lines;
    uint8_t  dummy_cycles;
    uint8_t  data_lines;   /* 0 = no data phase */
    uint32_t addr;
} pal_qspi_cmd_t;

pal_status_t pal_qspi_init(uint32_t flash_size_bytes);
pal_status_t pal_qspi_command(const pal_qspi_cmd_t *cmd, const uint8_t *tx, uint8_t *rx,
                              size_t len, uint32_t timeout_ms);
/* Map the device at PAL_QSPI_MMAP_BASE for read-only XIP / asset access. */
pal_status_t pal_qspi_mmap_enable(const pal_qspi_cmd_t *read_cmd);
pal_status_t pal_qspi_mmap_disable(void);
uintptr_t    pal_qspi_mmap_base(void);

#endif /* PAL_QSPI_H */
