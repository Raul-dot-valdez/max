/*
 * PAL internal flash. Only used by services/nvstore through a flash_dev_t
 * adapter. Program granularity differs a lot between vendors (STM32H7: 32-byte
 * flash word with ECC, no re-programming; RT1xxx has no internal flash at all
 * and maps this onto a FlexSPI partition), so callers must honour
 * pal_flash_prog_unit().
 */
#ifndef PAL_FLASH_H
#define PAL_FLASH_H

#include "pal_types.h"

uint32_t     pal_flash_prog_unit(void);
uint32_t     pal_flash_sector_size(uint32_t addr);
pal_status_t pal_flash_erase_sector(uint32_t addr);
pal_status_t pal_flash_program(uint32_t addr, const void *data, size_t len); /* len % prog_unit == 0 */
pal_status_t pal_flash_read(uint32_t addr, void *data, size_t len);

#endif /* PAL_FLASH_H */
