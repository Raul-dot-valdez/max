/*
 * Winbond W25Q (W25Q512JV, 64 MB) on PAL QSPI. Exposes a flash_dev_t.
 *
 * Memory-mapped mode and indirect (program/erase) mode are mutually
 * exclusive on the QSPI peripheral. The design keeps the device in indirect
 * mode at runtime: GUI bitmaps are copied to SDRAM once at boot while mapped,
 * then w25q_mmap(false) hands the bus to the data logger.
 */
#ifndef W25Q_H
#define W25Q_H

#include "flash_dev.h"

pal_status_t w25q_init(flash_dev_t *dev);      /* probes JEDEC ID, enables QE */
pal_status_t w25q_mmap(bool enable);
uint32_t     w25q_jedec_id(void);

#endif
