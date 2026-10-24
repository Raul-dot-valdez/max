#include "w25q.h"

#include "board_config.h"
#include "osal.h"
#include "pal_qspi.h"
#include "pal_sys.h"

/* 4-byte-address opcodes are used throughout, so the part never needs the
 * volatile 4-byte mode bit (a brown-out reset leaves it in a known state). */
#define OP_WREN        0x06
#define OP_RDSR1       0x05
#define OP_RDSR2       0x35
#define OP_WRSR2       0x31
#define OP_JEDEC       0x9F
#define OP_RST_EN      0x66
#define OP_RST         0x99
#define OP_PP4         0x12   /* page program, 4-byte address, 1-1-1 */
#define OP_QPP4        0x34   /* quad page program, 4-byte address, 1-1-4 */
#define OP_SE4         0x21   /* 4 KB sector erase, 4-byte address */
#define OP_QOR4        0x6C   /* fast read quad output, 4-byte, 8 dummy */
#define OP_QIOR4       0xEC   /* fast read quad I/O, 4-byte, 6 dummy (mmap) */

#define SR1_BUSY       0x01
#define SR2_QE         0x02

#define PAGE_SIZE      256u
#define SECTOR_SIZE    4096u
#define T_PP_MAX_MS    3u
#define T_SE_MAX_MS    400u

static struct {
    osal_mutex_t lock;
    bool         mapped;
    uint32_t     jedec;
} w;

static pal_status_t simple(uint8_t op)
{
    pal_qspi_cmd_t c = { .instr = op, .instr_lines = 1 };
    return pal_qspi_command(&c, NULL, NULL, 0, 10);
}

static pal_status_t read_reg(uint8_t op, uint8_t *v, size_t n)
{
    pal_qspi_cmd_t c = { .instr = op, .instr_lines = 1, .data_lines = 1 };
    return pal_qspi_command(&c, NULL, v, n, 10);
}

static pal_status_t wait_ready(uint32_t timeout_ms)
{
    uint32_t t0 = osal_now_ms();
    uint8_t sr;
    do {
        if (read_reg(OP_RDSR1, &sr, 1) != PAL_OK)
            return PAL_ERR;
        if (!(sr & SR1_BUSY))
            return PAL_OK;
        if (timeout_ms > 5)
            osal_sleep_ms(1);
    } while (osal_now_ms() - t0 <= timeout_ms);
    return PAL_TIMEOUT;
}

static pal_status_t ensure_indirect(void)
{
    if (!w.mapped)
        return PAL_OK;
    pal_status_t st = pal_qspi_mmap_disable();
    if (st == PAL_OK)
        w.mapped = false;
    return st;
}

static pal_status_t dev_read(flash_dev_t *d, uint32_t addr, void *buf, size_t len)
{
    if (addr + len > d->size)
        return PAL_EINVAL;
    osal_mutex_lock(w.lock);
    pal_status_t st = ensure_indirect();
    if (st == PAL_OK) {
        pal_qspi_cmd_t c = { .instr = OP_QOR4, .instr_lines = 1, .addr_bytes = 4,
                             .addr_lines = 1, .dummy_cycles = 8, .data_lines = 4, .addr = addr };
        st = pal_qspi_command(&c, NULL, buf, len, 100);
    }
    osal_mutex_unlock(w.lock);
    return st;
}

static pal_status_t dev_prog(flash_dev_t *d, uint32_t addr, const void *buf, size_t len)
{
    const uint8_t *p = buf;
    if (addr + len > d->size)
        return PAL_EINVAL;
    osal_mutex_lock(w.lock);
    pal_status_t st = ensure_indirect();
    while (st == PAL_OK && len) {
        size_t chunk = PAGE_SIZE - (addr % PAGE_SIZE);
        if (chunk > len)
            chunk = len;
        if ((st = simple(OP_WREN)) != PAL_OK)
            break;
        pal_qspi_cmd_t c = { .instr = OP_QPP4, .instr_lines = 1, .addr_bytes = 4,
                             .addr_lines = 1, .data_lines = 4, .addr = addr };
        if ((st = pal_qspi_command(&c, p, NULL, chunk, 10)) != PAL_OK)
            break;
        st = wait_ready(T_PP_MAX_MS + 2);
        addr += chunk;
        p += chunk;
        len -= chunk;
    }
    osal_mutex_unlock(w.lock);
    return st;
}

static pal_status_t dev_erase(flash_dev_t *d, uint32_t addr)
{
    if (addr >= d->size || (addr % SECTOR_SIZE))
        return PAL_EINVAL;
    osal_mutex_lock(w.lock);
    pal_status_t st = ensure_indirect();
    if (st == PAL_OK)
        st = simple(OP_WREN);
    if (st == PAL_OK) {
        pal_qspi_cmd_t c = { .instr = OP_SE4, .instr_lines = 1, .addr_bytes = 4,
                             .addr_lines = 1, .addr = addr };
        st = pal_qspi_command(&c, NULL, NULL, 0, 10);
    }
    if (st == PAL_OK)
        st = wait_ready(T_SE_MAX_MS + 50);
    osal_mutex_unlock(w.lock);
    return st;
}

pal_status_t w25q_init(flash_dev_t *dev)
{
    uint8_t id[3], sr2;

    w.lock = osal_mutex_create();
    if (pal_qspi_init(BOARD_NOR_SIZE) != PAL_OK)
        return PAL_ERR;

    simple(OP_RST_EN);
    simple(OP_RST);
    pal_sys_delay_ms(1); /* tRST = 30 us */

    if (read_reg(OP_JEDEC, id, 3) != PAL_OK)
        return PAL_ERR;
    w.jedec = ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
    if (id[0] != 0xEF) /* Winbond */
        return PAL_ENOENT;

    /* Quad Enable is non-volatile on W25Q512JV; only write it when clear to
     * avoid needless status-register wear. */
    if (read_reg(OP_RDSR2, &sr2, 1) != PAL_OK)
        return PAL_ERR;
    if (!(sr2 & SR2_QE)) {
        uint8_t v = sr2 | SR2_QE;
        pal_qspi_cmd_t c = { .instr = OP_WRSR2, .instr_lines = 1, .data_lines = 1 };
        simple(OP_WREN);
        if (pal_qspi_command(&c, &v, NULL, 1, 10) != PAL_OK || wait_ready(20) != PAL_OK)
            return PAL_ERR;
    }

    /* Capacity from JEDEC: id[2] = log2(bytes) for W25Q (0x20 -> 64 MB). */
    dev->size       = 1u << id[2];
    dev->erase_size = SECTOR_SIZE;
    dev->prog_size  = PAGE_SIZE;
    dev->read       = dev_read;
    dev->prog       = dev_prog;
    dev->erase      = dev_erase;
    dev->ctx        = NULL;
    return PAL_OK;
}

pal_status_t w25q_mmap(bool enable)
{
    pal_status_t st = PAL_OK;
    osal_mutex_lock(w.lock);
    if (enable && !w.mapped) {
        pal_qspi_cmd_t c = { .instr = OP_QIOR4, .instr_lines = 1, .addr_bytes = 4,
                             .addr_lines = 4, .dummy_cycles = 6, .data_lines = 4 };
        st = pal_qspi_mmap_enable(&c);
        w.mapped = (st == PAL_OK);
    } else if (!enable) {
        st = ensure_indirect();
    }
    osal_mutex_unlock(w.lock);
    return st;
}

uint32_t w25q_jedec_id(void) { return w.jedec; }
