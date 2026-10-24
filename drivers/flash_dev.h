/*
 * Generic NOR-style block device. Implemented by the W25Q QSPI driver, by the
 * internal-flash adapter, and by a RAM simulator in the host tests. Services
 * (datalog, nvstore) only ever see this interface, so swapping the Winbond
 * part for a Macronix/ISSI/Infineon NOR is a driver change only.
 */
#ifndef FLASH_DEV_H
#define FLASH_DEV_H

#include "pal_types.h"

typedef struct flash_dev flash_dev_t;

struct flash_dev {
    uint32_t size;        /* bytes */
    uint32_t erase_size;  /* smallest erase block */
    uint32_t prog_size;   /* page / program granule; writes must not cross it */
    pal_status_t (*read)(flash_dev_t *d, uint32_t addr, void *buf, size_t len);
    pal_status_t (*prog)(flash_dev_t *d, uint32_t addr, const void *buf, size_t len);
    pal_status_t (*erase)(flash_dev_t *d, uint32_t addr); /* one erase block */
    void *ctx;
};

#endif
