/*
 * STM32H7 port internals. Split of responsibilities:
 *  - STM32CubeMX (.ioc, generated into board/stm32h743_mower/cubemx) owns pin
 *    mux, DMA stream assignment, NVIC priorities and HAL_*_MspInit().
 *  - This port owns peripheral *behaviour* (bit timing, protocols, buffers).
 * In CubeMX, untick "Generate function call" for FDCAN1, USART2, USART3,
 * I2C1, QUADSPI, LTDC, FMC so the port is the only caller of HAL_xxx_Init().
 */
#ifndef PORT_STM32H7_H
#define PORT_STM32H7_H

#include "stm32h7xx_hal.h"

extern FDCAN_HandleTypeDef hfdcan1;
extern UART_HandleTypeDef  huart2;      /* GNSS */
extern UART_HandleTypeDef  huart3;      /* PSoC 4 BLE */
extern I2C_HandleTypeDef   hi2c1;       /* RTC + touch */
extern QSPI_HandleTypeDef  hqspi;
extern LTDC_HandleTypeDef  hltdc;
extern SDRAM_HandleTypeDef hsdram1;
extern TIM_HandleTypeDef   htim_backlight; /* PWM, channel 1 */
extern IWDG_HandleTypeDef  hiwdg1;

/* NVIC: all peripheral IRQs that call into FreeRTOS must have a numerically
 * >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5). FDCAN 5, UART DMA 6,
 * LTDC 7, everything else >= 8. */

#endif
