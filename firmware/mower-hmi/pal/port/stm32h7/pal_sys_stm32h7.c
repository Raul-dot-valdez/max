/*
 * STM32H743 system bring-up.
 *
 * Clock plan (HSE 25 MHz, VOS1 -> 400 MHz max: the -40..125 °C grade does not
 * allow VOS0/480 MHz, and a sealed cab display in direct sun needs the margin):
 *   PLL1: /5 x160 = 800 MHz VCO; P/2 = 400 MHz SYSCLK, Q/10 = 80 MHz FDCAN
 *   PLL2: /5 x160 = 800 MHz VCO; R/4 = 200 MHz FMC -> SDCLK 100 MHz
 *   PLL3: /5 x132 = 660 MHz VCO; R/20 = 33 MHz LTDC pixel clock
 *   AHB 200 MHz (QUADSPI kernel), APB1/2/3/4 100 MHz
 *
 * MPU map:
 *   0  0x60000000 256 MB  no access (stops M7 speculative reads hitting the
 *                         unused FMC NOR/PSRAM bank — ST AN4861)
 *   1  0xC0000000  32 MB  SDRAM, normal, write-back read/write-allocate
 *   2  0xC0000000   2 MB  framebuffers, normal, write-through (overrides 1)
 *   3  0x30000000 512 KB  D2 SRAM (DMA buffers), normal, non-cacheable
 *   4  0x90000000  64 MB  QSPI window, normal, write-through, read-only
 */
#include "pal_sys.h"
#include "port_stm32h7.h"

#include <string.h>

#include "board_config.h"

SDRAM_HandleTypeDef hsdram1;
IWDG_HandleTypeDef  hiwdg1;

extern uint32_t _ssdram_bss, _esdram_bss; /* linker script */

static void fault(void)
{
    __disable_irq();
    for (;;) {} /* IWDG not yet running this early: hard stop, LED blink is board MSP's job */
}

static void clock_config(void)
{
    RCC_OscInitTypeDef osc = { 0 };
    RCC_ClkInitTypeDef clk = { 0 };
    RCC_PeriphCLKInitTypeDef per = { 0 };

    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSI;
    osc.HSEState = RCC_HSE_ON;
    osc.LSIState = RCC_LSI_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 5;
    osc.PLL.PLLN = 160;
    osc.PLL.PLLP = 2;
    osc.PLL.PLLQ = 10;
    osc.PLL.PLLR = 2;
    osc.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
    osc.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    osc.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) fault();

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
                    RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.SYSCLKDivider = RCC_SYSCLK_DIV1;
    clk.AHBCLKDivider = RCC_HCLK_DIV2;
    clk.APB3CLKDivider = RCC_APB3_DIV2;
    clk.APB1CLKDivider = RCC_APB1_DIV2;
    clk.APB2CLKDivider = RCC_APB2_DIV2;
    clk.APB4CLKDivider = RCC_APB4_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) fault();

    per.PeriphClockSelection = RCC_PERIPHCLK_FMC | RCC_PERIPHCLK_LTDC | RCC_PERIPHCLK_FDCAN |
                               RCC_PERIPHCLK_QSPI;
    per.PLL2.PLL2M = 5;
    per.PLL2.PLL2N = 160;
    per.PLL2.PLL2P = 2;
    per.PLL2.PLL2Q = 2;
    per.PLL2.PLL2R = 4;
    per.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_2;
    per.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    per.PLL3.PLL3M = 5;
    per.PLL3.PLL3N = 132;
    per.PLL3.PLL3P = 2;
    per.PLL3.PLL3Q = 2;
    per.PLL3.PLL3R = 20;
    per.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
    per.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
    per.FmcClockSelection = RCC_FMCCLKSOURCE_PLL2;
    per.QspiClockSelection = RCC_QSPICLKSOURCE_D1HCLK;
    per.FdcanClockSelection = RCC_FDCANCLKSOURCE_PLL;
    if (HAL_RCCEx_PeriphCLKConfig(&per) != HAL_OK) fault();
}

static void mpu_region(uint8_t n, uint32_t base, uint8_t size, uint8_t tex, bool c, bool b,
                       uint8_t ap, bool xn)
{
    MPU_Region_InitTypeDef r = {
        .Enable = MPU_REGION_ENABLE,
        .Number = n,
        .BaseAddress = base,
        .Size = size,
        .SubRegionDisable = 0,
        .TypeExtField = tex,
        .AccessPermission = ap,
        .DisableExec = xn ? MPU_INSTRUCTION_ACCESS_DISABLE : MPU_INSTRUCTION_ACCESS_ENABLE,
        .IsShareable = MPU_ACCESS_NOT_SHAREABLE,
        .IsCacheable = c ? MPU_ACCESS_CACHEABLE : MPU_ACCESS_NOT_CACHEABLE,
        .IsBufferable = b ? MPU_ACCESS_BUFFERABLE : MPU_ACCESS_NOT_BUFFERABLE,
    };
    HAL_MPU_ConfigRegion(&r);
}

static void mpu_config(void)
{
    HAL_MPU_Disable();
    mpu_region(0, 0x60000000u, MPU_REGION_SIZE_256MB, MPU_TEX_LEVEL0, false, false, MPU_REGION_NO_ACCESS, true);
    mpu_region(1, BOARD_SDRAM_BASE, MPU_REGION_SIZE_32MB, MPU_TEX_LEVEL1, true, true, MPU_REGION_FULL_ACCESS, true);
    mpu_region(2, BOARD_FB0_ADDR, MPU_REGION_SIZE_2MB, MPU_TEX_LEVEL0, true, false, MPU_REGION_FULL_ACCESS, true);
    mpu_region(3, 0x30000000u, MPU_REGION_SIZE_512KB, MPU_TEX_LEVEL1, false, false, MPU_REGION_FULL_ACCESS, true);
    mpu_region(4, 0x90000000u, MPU_REGION_SIZE_64MB, MPU_TEX_LEVEL0, true, false, MPU_REGION_PRIV_RO_URO, false);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/* ---- SDRAM: IS42S32800J-6, 8M x 32, 4 banks, 12 row / 9 col bits --------
 * SDCLK 100 MHz (10 ns). From the datasheet (-6 speed grade):
 *   tMRD 2 clk, tXSR 70 ns -> 7, tRAS 42 ns -> 5, tRC 60 ns -> 6,
 *   tWR 2 clk, tRP 18 ns -> 2, tRCD 18 ns -> 2, CAS 3 (margin over CL2).
 * Refresh: 4096 rows / 64 ms = 15.625 us -> (15.625 us x 100 MHz) - 20 = 1542.
 */
#define SDRAM_MODE_BURST_1       0x0000u
#define SDRAM_MODE_SEQUENTIAL    0x0000u
#define SDRAM_MODE_CAS_3         0x0030u
#define SDRAM_MODE_STANDARD      0x0000u
#define SDRAM_MODE_WB_SINGLE     0x0200u
#define SDRAM_REFRESH_COUNT      1542u

static pal_status_t sdram_cmd(uint32_t mode, uint32_t nrefresh, uint32_t mrd)
{
    FMC_SDRAM_CommandTypeDef c = {
        .CommandMode = mode,
        .CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1,
        .AutoRefreshNumber = nrefresh,
        .ModeRegisterDefinition = mrd,
    };
    return HAL_SDRAM_SendCommand(&hsdram1, &c, 0x1000) == HAL_OK ? PAL_OK : PAL_ERR;
}

pal_status_t pal_sdram_init(void)
{
    FMC_SDRAM_TimingTypeDef t = {
        .LoadToActiveDelay = 2,
        .ExitSelfRefreshDelay = 7,
        .SelfRefreshTime = 5,
        .RowCycleDelay = 6,
        .WriteRecoveryTime = 2,
        .RPDelay = 2,
        .RCDDelay = 2,
    };
    hsdram1.Instance = FMC_SDRAM_DEVICE;
    hsdram1.Init.SDBank = FMC_SDRAM_BANK1;
    hsdram1.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_9;
    hsdram1.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_12;
    hsdram1.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_32;
    hsdram1.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
    hsdram1.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_3;
    hsdram1.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
    hsdram1.Init.SDClockPeriod = FMC_SDRAM_CLOCK_PERIOD_2;
    hsdram1.Init.ReadBurst = FMC_SDRAM_RBURST_ENABLE;
    hsdram1.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_0;
    if (HAL_SDRAM_Init(&hsdram1, &t) != HAL_OK) return PAL_ERR;

    if (sdram_cmd(FMC_SDRAM_CMD_CLK_ENABLE, 1, 0) != PAL_OK) return PAL_ERR;
    HAL_Delay(1); /* >= 100 us power-up */
    if (sdram_cmd(FMC_SDRAM_CMD_PALL, 1, 0) != PAL_OK) return PAL_ERR;
    if (sdram_cmd(FMC_SDRAM_CMD_AUTOREFRESH_MODE, 8, 0) != PAL_OK) return PAL_ERR;
    if (sdram_cmd(FMC_SDRAM_CMD_LOAD_MODE, 1, SDRAM_MODE_BURST_1 | SDRAM_MODE_SEQUENTIAL |
                  SDRAM_MODE_CAS_3 | SDRAM_MODE_STANDARD | SDRAM_MODE_WB_SINGLE) != PAL_OK)
        return PAL_ERR;
    return HAL_SDRAM_ProgramRefreshRate(&hsdram1, SDRAM_REFRESH_COUNT) == HAL_OK ? PAL_OK : PAL_ERR;
}

pal_status_t pal_sdram_selftest(uint32_t bytes)
{
    volatile uint32_t *m = (volatile uint32_t *)BOARD_SDRAM_BASE;
    /* data lines: walking ones on one word */
    for (int b = 0; b < 32; b++) {
        m[0] = 1u << b;
        __DSB();
        if (m[0] != (1u << b)) return PAL_ERR;
    }
    /* address lines: power-of-two offsets must not alias */
    uint32_t words = bytes / 4;
    for (uint32_t o = 1; o < words; o <<= 1) m[o] = o;
    m[0] = 0xA5A5A5A5u;
    for (uint32_t o = 1; o < words; o <<= 1)
        if (m[o] != o) return PAL_ERR;
    return PAL_OK;
}

void pal_sys_init(void)
{
    mpu_config(); /* MPU before caches: no line may be allocated under the default map */
    SCB_EnableICache();
    SCB_EnableDCache();
    HAL_Init();
    clock_config();
    if (pal_sdram_init() != PAL_OK || pal_sdram_selftest(BOARD_SDRAM_SIZE) != PAL_OK)
        fault();
    memset(&_ssdram_bss, 0, (size_t)((uintptr_t)&_esdram_bss - (uintptr_t)&_ssdram_bss));
    __HAL_RCC_BKPRAM_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
}

pal_reset_cause_t pal_sys_reset_cause(void)
{
    static int8_t cached = -1;
    if (cached < 0) {
        pal_reset_cause_t c = PAL_RESET_OTHER;
        if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDG1RST)) c = PAL_RESET_WATCHDOG;
        else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)) c = PAL_RESET_SOFTWARE;
        else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST)) c = PAL_RESET_POWER_ON;
        else if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST)) c = PAL_RESET_BROWNOUT;
        else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST)) c = PAL_RESET_PIN;
        __HAL_RCC_CLEAR_RESET_FLAGS();
        cached = (int8_t)c;
    }
    return (pal_reset_cause_t)cached;
}

void pal_sys_reset(void) { NVIC_SystemReset(); }
void pal_sys_delay_ms(uint32_t ms) { HAL_Delay(ms); }

uint32_t pal_sys_uid32(void)
{
    return HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2();
}

void *pal_sys_backup_ram(size_t *size)
{
    *size = 4096;
    return (void *)D3_BKPSRAM_BASE;
}

void pal_wdg_init(uint32_t timeout_ms)
{
    /* LSI ~32 kHz / 64 = 500 Hz */
    hiwdg1.Instance = IWDG1;
    hiwdg1.Init.Prescaler = IWDG_PRESCALER_64;
    hiwdg1.Init.Window = IWDG_WINDOW_DISABLE;
    hiwdg1.Init.Reload = timeout_ms / 2u > 0xFFFu ? 0xFFFu : timeout_ms / 2u;
    HAL_IWDG_Init(&hiwdg1);
    DBGMCU->APB4FZ1 |= DBGMCU_APB4FZ1_DBG_IWDG1; /* freeze while halted in a debugger */
}

void pal_wdg_kick(void) { HAL_IWDG_Refresh(&hiwdg1); }

void pal_cache_clean(const void *addr, size_t len)
{
    SCB_CleanDCache_by_Addr((uint32_t *)((uintptr_t)addr & ~31u), (int32_t)(len + ((uintptr_t)addr & 31u)));
}

void pal_cache_invalidate(void *addr, size_t len)
{
    SCB_InvalidateDCache_by_Addr((uint32_t *)((uintptr_t)addr & ~31u), (int32_t)(len + ((uintptr_t)addr & 31u)));
}

/* FreeRTOS owns SysTick; HAL time base runs on TIM6 (CubeMX "Timebase Source"). */
