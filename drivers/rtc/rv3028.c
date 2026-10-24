/*
 * Micro Crystal RV-3028-C7: ~45 nA backup current, AEC-Q200, ±1 ppm after
 * factory calibration. Uses the chip's 32-bit UNIX time counter instead of
 * the BCD calendar registers: no BCD, no leap-year logic in the driver.
 */
#include "rtc.h"

#include "pal_i2c.h"
#include "pal_sys.h"

#define RV3028_ADDR      0x52
#define REG_STATUS       0x0E
#define REG_CONTROL1     0x0F
#define REG_UNIX0        0x1B
#define REG_EE_ADDR      0x25
#define REG_EE_CMD       0x27
#define REG_EE_BACKUP    0x37   /* RAM mirror of EEPROM config byte */

#define STATUS_PORF      0x01
#define STATUS_EEBUSY    0x80
#define CTRL1_EERD       0x08   /* disable automatic EEPROM refresh */
#define BACKUP_BSM_MASK  0x0C
#define BACKUP_BSM_LEVEL 0x0C   /* level switching mode: switch to VBACKUP when VDD < VBACKUP and < 2 V */
#define EECMD_UPDATE     0x11   /* copy all config RAM to EEPROM */

static uint8_t s_bus;

static pal_status_t rd(uint8_t reg, uint8_t *v, size_t n)
{
    return pal_i2c_xfer(s_bus, RV3028_ADDR, &reg, 1, v, n, 10);
}

static pal_status_t wr(uint8_t reg, const uint8_t *v, size_t n)
{
    uint8_t buf[8];
    if (n > sizeof(buf) - 1)
        return PAL_EINVAL;
    buf[0] = reg;
    for (size_t i = 0; i < n; i++)
        buf[i + 1] = v[i];
    return pal_i2c_xfer(s_bus, RV3028_ADDR, buf, n + 1, NULL, 0, 10);
}

static pal_status_t wait_ee(void)
{
    uint8_t st;
    for (int i = 0; i < 50; i++) {
        if (rd(REG_STATUS, &st, 1) != PAL_OK)
            return PAL_ERR;
        if (!(st & STATUS_EEBUSY))
            return PAL_OK;
        pal_sys_delay_ms(2);
    }
    return PAL_TIMEOUT;
}

/* Persist backup-switchover mode in EEPROM once; factory default is "disabled",
 * which would lose time on every key-off. */
static pal_status_t ensure_backup_switchover(void)
{
    uint8_t v, c1;
    if (rd(REG_EE_BACKUP, &v, 1) != PAL_OK)
        return PAL_ERR;
    if ((v & BACKUP_BSM_MASK) == BACKUP_BSM_LEVEL)
        return PAL_OK;

    if (rd(REG_CONTROL1, &c1, 1) != PAL_OK)
        return PAL_ERR;
    uint8_t c1_eerd = c1 | CTRL1_EERD;
    wr(REG_CONTROL1, &c1_eerd, 1);

    v = (uint8_t)((v & ~BACKUP_BSM_MASK) | BACKUP_BSM_LEVEL);
    wr(REG_EE_BACKUP, &v, 1);
    pal_status_t st = wait_ee();
    if (st == PAL_OK) {
        uint8_t z = 0x00, u = EECMD_UPDATE;
        wr(REG_EE_CMD, &z, 1);
        wr(REG_EE_CMD, &u, 1);
        st = wait_ee();
    }
    wr(REG_CONTROL1, &c1, 1);
    return st;
}

static pal_status_t get(rtc_dev_t *d, uint32_t *unix_s, bool *valid)
{
    (void)d;
    uint8_t a[4], b[4], st;
    if (rd(REG_STATUS, &st, 1) != PAL_OK)
        return PAL_ERR;
    /* The counter is not latched across a multi-byte read: read until two
     * consecutive reads agree (at most one second boundary can intervene). */
    for (int i = 0; i < 3; i++) {
        if (rd(REG_UNIX0, a, 4) != PAL_OK || rd(REG_UNIX0, b, 4) != PAL_OK)
            return PAL_ERR;
        if (a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]) {
            *unix_s = (uint32_t)a[0] | ((uint32_t)a[1] << 8) | ((uint32_t)a[2] << 16) |
                      ((uint32_t)a[3] << 24);
            *valid = !(st & STATUS_PORF) && *unix_s > 1700000000u;
            return PAL_OK;
        }
    }
    return PAL_ERR;
}

static pal_status_t set(rtc_dev_t *d, uint32_t unix_s)
{
    (void)d;
    uint8_t v[4] = { (uint8_t)unix_s, (uint8_t)(unix_s >> 8), (uint8_t)(unix_s >> 16),
                     (uint8_t)(unix_s >> 24) };
    if (wr(REG_UNIX0, v, 4) != PAL_OK)
        return PAL_ERR;
    uint8_t st;
    if (rd(REG_STATUS, &st, 1) != PAL_OK)
        return PAL_ERR;
    st &= (uint8_t)~STATUS_PORF;
    return wr(REG_STATUS, &st, 1);
}

pal_status_t rv3028_init(rtc_dev_t *dev, uint8_t i2c_bus)
{
    s_bus = i2c_bus;
    dev->get = get;
    dev->set = set;
    dev->ctx = NULL;
    return ensure_backup_switchover();
}
