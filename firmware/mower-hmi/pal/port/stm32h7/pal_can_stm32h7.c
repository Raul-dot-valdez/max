/*
 * PAL CAN on STM32H7 FDCAN1, classic CAN 2.0B.
 * Kernel clock: PLL1Q = 80 MHz. 250 kbit/s: prescaler 20 -> 4 MHz tq,
 * 16 tq/bit, sample point 87.5 % (J1939-11), SJW 1.
 */
#include "pal_can.h"
#include "port_stm32h7.h"

FDCAN_HandleTypeDef hfdcan1;

static pal_can_rx_cb_t s_cb;
static void           *s_ctx;

/* Works with both encodings of DataLength used across Cube H7 releases. */
static const uint32_t k_dlc[9] = {
    FDCAN_DLC_BYTES_0, FDCAN_DLC_BYTES_1, FDCAN_DLC_BYTES_2, FDCAN_DLC_BYTES_3, FDCAN_DLC_BYTES_4,
    FDCAN_DLC_BYTES_5, FDCAN_DLC_BYTES_6, FDCAN_DLC_BYTES_7, FDCAN_DLC_BYTES_8,
};

static uint8_t dlc_to_len(uint32_t dl)
{
    for (uint8_t i = 0; i < 9; i++)
        if (k_dlc[i] == dl) return i;
    return 8;
}

pal_status_t pal_can_init(uint8_t bus, uint32_t bitrate)
{
    if (bus != 0) return PAL_EINVAL;
    uint32_t presc = 80000000u / (bitrate * 16u);
    if (presc == 0 || presc * bitrate * 16u != 80000000u) return PAL_EINVAL;

    hfdcan1.Instance = FDCAN1;
    hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
    hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
    hfdcan1.Init.AutoRetransmission = ENABLE;
    hfdcan1.Init.TransmitPause = DISABLE;
    hfdcan1.Init.ProtocolException = DISABLE;
    hfdcan1.Init.NominalPrescaler = presc;
    hfdcan1.Init.NominalSyncJumpWidth = 1;
    hfdcan1.Init.NominalTimeSeg1 = 13;
    hfdcan1.Init.NominalTimeSeg2 = 2;
    hfdcan1.Init.DataPrescaler = 1;
    hfdcan1.Init.DataSyncJumpWidth = 1;
    hfdcan1.Init.DataTimeSeg1 = 1;
    hfdcan1.Init.DataTimeSeg2 = 1;
    hfdcan1.Init.MessageRAMOffset = 0;
    hfdcan1.Init.StdFiltersNbr = 0;
    hfdcan1.Init.ExtFiltersNbr = 8;
    hfdcan1.Init.RxFifo0ElmtsNbr = 64;
    hfdcan1.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
    hfdcan1.Init.RxFifo1ElmtsNbr = 0;
    hfdcan1.Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_8;
    hfdcan1.Init.RxBuffersNbr = 0;
    hfdcan1.Init.RxBufferSize = FDCAN_DATA_BYTES_8;
    hfdcan1.Init.TxEventsNbr = 0;
    hfdcan1.Init.TxBuffersNbr = 0;
    hfdcan1.Init.TxFifoQueueElmtsNbr = 16;
    hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    hfdcan1.Init.TxElmtSize = FDCAN_DATA_BYTES_8;
    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK) return PAL_ERR;
    if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_REJECT, FDCAN_REJECT,
                                     FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) != HAL_OK)
        return PAL_ERR;
    return PAL_OK;
}

pal_status_t pal_can_add_filter_ext(uint8_t bus, uint8_t idx, uint32_t id, uint32_t mask)
{
    (void)bus;
    FDCAN_FilterTypeDef f = {
        .IdType = FDCAN_EXTENDED_ID,
        .FilterIndex = idx,
        .FilterType = FDCAN_FILTER_MASK,
        .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
        .FilterID1 = id,
        .FilterID2 = mask,
    };
    return HAL_FDCAN_ConfigFilter(&hfdcan1, &f) == HAL_OK ? PAL_OK : PAL_ERR;
}

pal_status_t pal_can_start(uint8_t bus)
{
    (void)bus;
    if (HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_BUS_OFF, 0) != HAL_OK)
        return PAL_ERR;
    return HAL_FDCAN_Start(&hfdcan1) == HAL_OK ? PAL_OK : PAL_ERR;
}

pal_status_t pal_can_send(uint8_t bus, const pal_can_frame_t *f)
{
    (void)bus;
    if (f->dlc > 8) return PAL_EINVAL;
    FDCAN_TxHeaderTypeDef h = {
        .Identifier = f->id,
        .IdType = f->ext ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID,
        .TxFrameType = FDCAN_DATA_FRAME,
        .DataLength = k_dlc[f->dlc],
        .ErrorStateIndicator = FDCAN_ESI_ACTIVE,
        .BitRateSwitch = FDCAN_BRS_OFF,
        .FDFormat = FDCAN_CLASSIC_CAN,
        .TxEventFifoControl = FDCAN_NO_TX_EVENTS,
        .MessageMarker = 0,
    };
    if (HAL_FDCAN_GetTxFifoFreeLevel(&hfdcan1) == 0) return PAL_BUSY;
    return HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &h, (uint8_t *)f->data) == HAL_OK ? PAL_OK : PAL_ERR;
}

void pal_can_set_rx_callback(uint8_t bus, pal_can_rx_cb_t cb, void *ctx)
{
    (void)bus;
    s_cb = cb;
    s_ctx = ctx;
}

pal_can_state_t pal_can_state(uint8_t bus)
{
    (void)bus;
    FDCAN_ProtocolStatusTypeDef ps;
    HAL_FDCAN_GetProtocolStatus(&hfdcan1, &ps);
    if (ps.BusOff) return PAL_CAN_STATE_BUS_OFF;
    if (ps.ErrorPassive) return PAL_CAN_STATE_PASSIVE;
    return PAL_CAN_STATE_ACTIVE;
}

pal_status_t pal_can_recover(uint8_t bus)
{
    (void)bus;
    /* Clearing INIT starts the 128 x 11 recessive-bit recovery sequence. */
    CLEAR_BIT(hfdcan1.Instance->CCCR, FDCAN_CCCR_INIT);
    return PAL_OK;
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *h, uint32_t its)
{
    if (!(its & FDCAN_IT_RX_FIFO0_NEW_MESSAGE)) return;
    FDCAN_RxHeaderTypeDef hdr;
    pal_can_frame_t f;
    while (HAL_FDCAN_GetRxMessage(h, FDCAN_RX_FIFO0, &hdr, f.data) == HAL_OK) {
        f.id = hdr.Identifier;
        f.ext = hdr.IdType == FDCAN_EXTENDED_ID;
        f.dlc = dlc_to_len(hdr.DataLength);
        f.ts_ms = HAL_GetTick();
        if (s_cb) s_cb(&f, s_ctx);
    }
}

void FDCAN1_IT0_IRQHandler(void) { HAL_FDCAN_IRQHandler(&hfdcan1); }
