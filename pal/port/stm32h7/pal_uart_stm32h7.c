/*
 * PAL UART on STM32H7: circular DMA + idle-line detection
 * (HAL_UARTEx_ReceiveToIdle_DMA). DMA buffers sit in D2 SRAM, which the MPU
 * marks non-cacheable, so no cache maintenance is needed on receive.
 */
#include "pal_uart.h"
#include "port_stm32h7.h"

UART_HandleTypeDef huart2, huart3;

#define RX_BUF 512u

typedef struct {
    UART_HandleTypeDef *h;
    USART_TypeDef      *inst;
    uint8_t            *buf;
    uint16_t            pos;
    pal_uart_rx_cb_t    cb;
    void               *ctx;
} port_t;

static uint8_t rx0[RX_BUF] PAL_DMA_BSS;
static uint8_t rx1[RX_BUF] PAL_DMA_BSS;

static port_t ports[2] = {
    { &huart2, USART2, rx0, 0, NULL, NULL },
    { &huart3, USART3, rx1, 0, NULL, NULL },
};

pal_status_t pal_uart_init(uint8_t port, uint32_t baud)
{
    if (port >= 2) return PAL_EINVAL;
    port_t *p = &ports[port];
    p->h->Instance = p->inst;
    p->h->Init.BaudRate = baud;
    p->h->Init.WordLength = UART_WORDLENGTH_8B;
    p->h->Init.StopBits = UART_STOPBITS_1;
    p->h->Init.Parity = UART_PARITY_NONE;
    p->h->Init.Mode = UART_MODE_TX_RX;
    p->h->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    p->h->Init.OverSampling = UART_OVERSAMPLING_16;
    p->h->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    p->h->Init.ClockPrescaler = UART_PRESCALER_DIV1;
    p->h->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(p->h) != HAL_OK) return PAL_ERR;
    HAL_UARTEx_DisableFifoMode(p->h);
    p->pos = 0;
    if (HAL_UARTEx_ReceiveToIdle_DMA(p->h, p->buf, RX_BUF) != HAL_OK) return PAL_ERR;
    return PAL_OK;
}

void pal_uart_set_rx_callback(uint8_t port, pal_uart_rx_cb_t cb, void *ctx)
{
    if (port >= 2) return;
    ports[port].cb = cb;
    ports[port].ctx = ctx;
}

pal_status_t pal_uart_write(uint8_t port, const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (port >= 2) return PAL_EINVAL;
    HAL_StatusTypeDef st = HAL_UART_Transmit(ports[port].h, (uint8_t *)data, (uint16_t)len, timeout_ms);
    return st == HAL_OK ? PAL_OK : (st == HAL_TIMEOUT ? PAL_TIMEOUT : PAL_ERR);
}

/* Size = current write index in the circular buffer (half, full or idle event). */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *h, uint16_t size)
{
    for (int i = 0; i < 2; i++) {
        port_t *p = &ports[i];
        if (p->h != h) continue;
        if (size != p->pos) {
            if (size > p->pos) {
                if (p->cb) p->cb(&p->buf[p->pos], size - p->pos, p->ctx);
            } else {
                if (p->cb) p->cb(&p->buf[p->pos], RX_BUF - p->pos, p->ctx);
                if (p->cb && size) p->cb(p->buf, size, p->ctx);
            }
            p->pos = size == RX_BUF ? 0 : size;
        }
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *h)
{
    /* Noise/overrun on a long harness: restart reception, never stall. */
    for (int i = 0; i < 2; i++)
        if (ports[i].h == h) {
            ports[i].pos = 0;
            HAL_UARTEx_ReceiveToIdle_DMA(h, ports[i].buf, RX_BUF);
        }
}

void USART2_IRQHandler(void) { HAL_UART_IRQHandler(&huart2); }
void USART3_IRQHandler(void) { HAL_UART_IRQHandler(&huart3); }
