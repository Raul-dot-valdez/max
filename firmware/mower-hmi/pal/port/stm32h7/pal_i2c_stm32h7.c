#include "pal_i2c.h"
#include "port_stm32h7.h"

I2C_HandleTypeDef hi2c1;

pal_status_t pal_i2c_init(uint8_t bus, uint32_t hz)
{
    if (bus != 0) return PAL_EINVAL;
    hi2c1.Instance = I2C1;
    /* TIMINGR values are PLACEHOLDERS for a 100 MHz I2C kernel clock. Generate
     * the real ones in CubeMX from the measured bus rise/fall times. */
    hi2c1.Init.Timing = (hz >= 400000u) ? 0x00B03FDBu : 0x307075B1u;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) return PAL_ERR;
    HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE);
    return PAL_OK;
}

pal_status_t pal_i2c_xfer(uint8_t bus, uint8_t addr7, const uint8_t *w, size_t wlen,
                          uint8_t *r, size_t rlen, uint32_t timeout_ms)
{
    (void)bus;
    uint16_t a = (uint16_t)(addr7 << 1);
    HAL_StatusTypeDef st;
    if (rlen && (wlen == 1 || wlen == 2)) {
        /* register read with repeated start */
        uint16_t reg = wlen == 1 ? w[0] : (uint16_t)((w[0] << 8) | w[1]);
        st = HAL_I2C_Mem_Read(&hi2c1, a, reg, wlen == 1 ? I2C_MEMADD_SIZE_8BIT : I2C_MEMADD_SIZE_16BIT,
                              r, (uint16_t)rlen, timeout_ms);
    } else {
        st = HAL_OK;
        if (wlen) st = HAL_I2C_Master_Transmit(&hi2c1, a, (uint8_t *)w, (uint16_t)wlen, timeout_ms);
        if (st == HAL_OK && rlen) st = HAL_I2C_Master_Receive(&hi2c1, a, r, (uint16_t)rlen, timeout_ms);
    }
    if (st == HAL_TIMEOUT || st == HAL_BUSY) {
        pal_i2c_bus_recover(bus);
        return PAL_TIMEOUT;
    }
    return st == HAL_OK ? PAL_OK : PAL_ERR;
}

pal_status_t pal_i2c_bus_recover(uint8_t bus)
{
    (void)bus;
    /* Re-init the peripheral; the 9-clock SCL bit-bang lives in the board
     * MSP (pins are CubeMX-owned) and runs from HAL_I2C_MspInit. */
    HAL_I2C_DeInit(&hi2c1);
    return HAL_I2C_Init(&hi2c1) == HAL_OK ? PAL_OK : PAL_ERR;
}
