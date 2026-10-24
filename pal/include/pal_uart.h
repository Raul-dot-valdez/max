/*
 * PAL UART: DMA-backed receive delivered in chunks (idle-line or half/full
 * buffer), blocking transmit. Used for the GNSS receiver and the PSoC 4 BLE
 * co-processor.
 */
#ifndef PAL_UART_H
#define PAL_UART_H

#include "pal_types.h"

/* Called from interrupt context with a contiguous chunk of received bytes. */
typedef void (*pal_uart_rx_cb_t)(const uint8_t *data, size_t len, void *ctx);

pal_status_t pal_uart_init(uint8_t port, uint32_t baud);
void         pal_uart_set_rx_callback(uint8_t port, pal_uart_rx_cb_t cb, void *ctx);
pal_status_t pal_uart_write(uint8_t port, const uint8_t *data, size_t len, uint32_t timeout_ms);

#endif /* PAL_UART_H */
