/*
 * PAL QSPI on STM32H7 QUADSPI (single flash, bank 1).
 * Kernel clock HCLK3 = 200 MHz, prescaler 1 -> 100 MHz SCK (W25Q512JV: 133 MHz max).
 */
#include "pal_qspi.h"
#include "port_stm32h7.h"

QSPI_HandleTypeDef hqspi;

static uint32_t lines_instr(uint8_t n) { return n == 4 ? QSPI_INSTRUCTION_4_LINES : n == 2 ? QSPI_INSTRUCTION_2_LINES : QSPI_INSTRUCTION_1_LINE; }
static uint32_t lines_addr(uint8_t n)  { return n == 4 ? QSPI_ADDRESS_4_LINES : n == 2 ? QSPI_ADDRESS_2_LINES : QSPI_ADDRESS_1_LINE; }
static uint32_t lines_data(uint8_t n)  { return n == 4 ? QSPI_DATA_4_LINES : n == 2 ? QSPI_DATA_2_LINES : n == 1 ? QSPI_DATA_1_LINE : QSPI_DATA_NONE; }

static void to_hal(const pal_qspi_cmd_t *c, QSPI_CommandTypeDef *h, size_t len)
{
    h->Instruction = c->instr;
    h->InstructionMode = lines_instr(c->instr_lines);
    h->Address = c->addr;
    h->AddressMode = c->addr_bytes ? lines_addr(c->addr_lines) : QSPI_ADDRESS_NONE;
    h->AddressSize = c->addr_bytes == 4 ? QSPI_ADDRESS_32_BITS : QSPI_ADDRESS_24_BITS;
    h->AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    h->AlternateBytes = 0;
    h->AlternateBytesSize = QSPI_ALTERNATE_BYTES_8_BITS;
    h->DummyCycles = c->dummy_cycles;
    h->DataMode = lines_data(c->data_lines);
    h->NbData = (uint32_t)len;
    h->DdrMode = QSPI_DDR_MODE_DISABLE;
    h->DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    h->SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
}

pal_status_t pal_qspi_init(uint32_t flash_size_bytes)
{
    uint32_t log2 = 0;
    while ((1u << (log2 + 1)) <= flash_size_bytes) log2++;
    hqspi.Instance = QUADSPI;
    hqspi.Init.ClockPrescaler = 1;
    hqspi.Init.FifoThreshold = 4;
    hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;
    hqspi.Init.FlashSize = log2 - 1;
    hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_2_CYCLE; /* tSHSL >= 10 ns */
    hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
    hqspi.Init.FlashID = QSPI_FLASH_ID_1;
    hqspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;
    return HAL_QSPI_Init(&hqspi) == HAL_OK ? PAL_OK : PAL_ERR;
}

pal_status_t pal_qspi_command(const pal_qspi_cmd_t *c, const uint8_t *tx, uint8_t *rx,
                              size_t len, uint32_t timeout_ms)
{
    QSPI_CommandTypeDef h;
    to_hal(c, &h, c->data_lines ? len : 0);
    if (HAL_QSPI_Command(&hqspi, &h, timeout_ms) != HAL_OK) return PAL_ERR;
    if (!c->data_lines || !len) return PAL_OK;
    HAL_StatusTypeDef st = tx ? HAL_QSPI_Transmit(&hqspi, (uint8_t *)tx, timeout_ms)
                              : HAL_QSPI_Receive(&hqspi, rx, timeout_ms);
    return st == HAL_OK ? PAL_OK : (st == HAL_TIMEOUT ? PAL_TIMEOUT : PAL_ERR);
}

pal_status_t pal_qspi_mmap_enable(const pal_qspi_cmd_t *c)
{
    QSPI_CommandTypeDef h;
    QSPI_MemoryMappedTypeDef m = { .TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE, .TimeOutPeriod = 0 };
    to_hal(c, &h, 0);
    return HAL_QSPI_MemoryMapped(&hqspi, &h, &m) == HAL_OK ? PAL_OK : PAL_ERR;
}

pal_status_t pal_qspi_mmap_disable(void)
{
    /* Abort leaves memory-mapped mode; invalidate cached lines of the window. */
    if (HAL_QSPI_Abort(&hqspi) != HAL_OK) return PAL_ERR;
    SCB_InvalidateDCache_by_Addr((uint32_t *)QSPI_BASE, 0x4000000);
    return PAL_OK;
}

uintptr_t pal_qspi_mmap_base(void) { return QSPI_BASE; }
