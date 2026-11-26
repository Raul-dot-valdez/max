/*
 * PAL internal flash on STM32H743: two 1 MB banks, 128 KB sectors,
 * 256-bit (32-byte) flash words with ECC. A word may be programmed once per
 * erase; a torn program leaves an ECC error that faults on read, which is why
 * nvstore never re-programs and why the ECC double-error IRQ is handled.
 */
#include "pal_flash.h"
#include "port_stm32h7.h"

#include <string.h>

#define WORD 32u

uint32_t pal_flash_prog_unit(void) { return WORD; }
uint32_t pal_flash_sector_size(uint32_t addr) { (void)addr; return FLASH_SECTOR_SIZE; }

pal_status_t pal_flash_erase_sector(uint32_t addr)
{
    FLASH_EraseInitTypeDef e = {
        .TypeErase = FLASH_TYPEERASE_SECTORS,
        .Banks = addr >= FLASH_BANK2_BASE ? FLASH_BANK_2 : FLASH_BANK_1,
        .Sector = ((addr - FLASH_BANK1_BASE) % 0x100000u) / FLASH_SECTOR_SIZE,
        .NbSectors = 1,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,
    };
    uint32_t err = 0;
    HAL_FLASH_Unlock();
    HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&e, &err);
    HAL_FLASH_Lock();
    SCB_InvalidateDCache_by_Addr((uint32_t *)addr, FLASH_SECTOR_SIZE);
    return st == HAL_OK ? PAL_OK : PAL_ERR;
}

pal_status_t pal_flash_program(uint32_t addr, const void *data, size_t len)
{
    if ((addr % WORD) || (len % WORD)) return PAL_EINVAL;
    uint32_t word[WORD / 4];
    HAL_StatusTypeDef st = HAL_OK;
    HAL_FLASH_Unlock();
    for (size_t off = 0; off < len && st == HAL_OK; off += WORD) {
        memcpy(word, (const uint8_t *)data + off, WORD); /* source must be word aligned */
        st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, addr + off, (uint32_t)(uintptr_t)word);
    }
    HAL_FLASH_Lock();
    SCB_InvalidateDCache_by_Addr((uint32_t *)addr, (int32_t)len);
    return st == HAL_OK ? PAL_OK : PAL_ERR;
}

pal_status_t pal_flash_read(uint32_t addr, void *data, size_t len)
{
    memcpy(data, (const void *)(uintptr_t)addr, len);
    return PAL_OK;
}
