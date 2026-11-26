/*
 * PSoC 4 BLE (CY8C4248LQI-BL583, Bluetooth 4.2) — BLE co-processor firmware.
 * PSoC Creator project with the BLE component (v3.x) and an SCB UART named "UART".
 *
 * Role: a thin, stateless bridge. All fleet logic lives on the STM32; this
 * chip only owns the radio, bonding and the GATT server, and shuttles frames
 * using the SAME psoc_link codec as the STM32 (../drivers/psoc_link).
 *
 * GATT (configure in the BLE component, see README.md):
 *   AI Terra Fleet service  (128-bit UUID)
 *     RX  characteristic: Write Without Response, authenticated encryption required
 *     TX  characteristic: Notify (CCCD), authenticated encryption required
 * Security: LE Secure Connections, IO capability Display Yes/No -> numeric
 * comparison; the 6-digit code is shown and confirmed on the HMI touchscreen.
 *
 * API names follow the PSoC Creator BLE component v3.x datasheet; generated
 * handle macros (CYBLE_FLEET_*) depend on the names given in the GATT editor.
 */
#include <project.h>
#include <string.h>

#include "psoc_link.h"

#define INITIAL_CREDITS 4u

static link_decoder_t dec;
static link_frame_t   rx_frame, tx_frame;
static uint8_t        enc[LINK_MAX_ENCODED];
static uint8_t        seq;
static uint8_t        connected, encrypted, bonded;
static uint16_t       mtu = CYBLE_GATT_MTU_MIN;  /* 23 until the phone exchanges MTU */
static uint8_t        credits_owed;

static void link_tx(uint8_t type, const uint8_t *p, uint16_t n)
{
    tx_frame.type = type;
    tx_frame.seq = seq++;
    tx_frame.len = n;
    memcpy(tx_frame.payload, p, n);
    size_t e = link_encode(&tx_frame, enc, sizeof(enc));
    if (e)
        UART_SpiUartPutArray(enc, (uint32)e);
}

static void send_status(void)
{
    uint8_t st[5] = { connected, encrypted, bonded, (uint8_t)mtu, (uint8_t)(mtu >> 8) };
    link_tx(LINK_T_STATUS, st, sizeof(st));
}

static void grant(uint8_t n)
{
    link_tx(LINK_T_CREDIT, &n, 1);
}

static void notify_app(const uint8_t *p, uint16_t n)
{
    if (!connected || !encrypted || n > mtu - 3u) {
        grant(1); /* dropped: return the credit so the STM32 does not stall */
        return;
    }
    CYBLE_GATTS_HANDLE_VALUE_NTF_T ntf;
    ntf.attrHandle = CYBLE_FLEET_TX_CHAR_HANDLE;
    ntf.value.val = (uint8 *)p;
    ntf.value.len = n;
    while (CyBle_GattGetBusyStatus() == CYBLE_STACK_STATE_BUSY)
        CyBle_ProcessEvents();
    if (CyBle_GattsNotification(cyBle_connHandle, &ntf) == CYBLE_ERROR_OK)
        credits_owed++; /* returned once the stack has room again */
    else
        grant(1);
}

static void ble_event(uint32 event, void *param)
{
    switch (event) {
    case CYBLE_EVT_STACK_ON:
    case CYBLE_EVT_GAP_DEVICE_DISCONNECTED:
        connected = encrypted = 0;
        mtu = CYBLE_GATT_MTU_MIN;
        credits_owed = 0;
        send_status();
        CyBle_GappStartAdvertisement(CYBLE_ADVERTISING_FAST);
        break;

    case CYBLE_EVT_GAP_DEVICE_CONNECTED:
        connected = 1;
        send_status();
        break;

    case CYBLE_EVT_GAP_NUMERIC_COMPARISON_REQUEST: {
        uint32 code = *(uint32 *)param;
        uint8_t b[4] = { (uint8_t)code, (uint8_t)(code >> 8), (uint8_t)(code >> 16), (uint8_t)(code >> 24) };
        link_tx(LINK_T_PAIR_CONFIRM, b, 4);
        break;
    }

    case CYBLE_EVT_GAP_AUTH_COMPLETE: {
        CYBLE_GAP_AUTH_INFO_T *a = (CYBLE_GAP_AUTH_INFO_T *)param;
        /* only an authenticated (MITM-protected) LESC link is accepted */
        encrypted = (a->security & CYBLE_GAP_SEC_LEVEL_MASK) >= CYBLE_GAP_SEC_LEVEL_3;
        bonded = a->bonding;
        send_status();
        if (encrypted)
            grant(INITIAL_CREDITS);
        else
            CyBle_GapDisconnect(cyBle_connHandle.bdHandle);
        break;
    }

    case CYBLE_EVT_GAP_AUTH_FAILED:
        CyBle_GapDisconnect(cyBle_connHandle.bdHandle);
        break;

    case CYBLE_EVT_GATTS_XCNHG_MTU_REQ: {
        CYBLE_GATT_XCHG_MTU_PARAM_T *m = (CYBLE_GATT_XCHG_MTU_PARAM_T *)param;
        mtu = m->mtu < CYBLE_GATT_MTU ? m->mtu : CYBLE_GATT_MTU;
        send_status();
        break;
    }

    case CYBLE_EVT_GATTS_WRITE_CMD_REQ: {
        CYBLE_GATTS_WRITE_CMD_REQ_PARAM_T *w = (CYBLE_GATTS_WRITE_CMD_REQ_PARAM_T *)param;
        if (w->handleValPair.attrHandle == CYBLE_FLEET_RX_CHAR_HANDLE && encrypted &&
            w->handleValPair.value.len <= LINK_MAX_PAYLOAD)
            link_tx(LINK_T_DATA_FROM_APP, w->handleValPair.value.val, w->handleValPair.value.len);
        break;
    }

    case CYBLE_EVT_GATTS_WRITE_REQ: { /* CCCD enable/disable */
        CYBLE_GATTS_WRITE_REQ_PARAM_T *w = (CYBLE_GATTS_WRITE_REQ_PARAM_T *)param;
        CyBle_GattsWriteAttributeValue(&w->handleValPair, 0, &cyBle_connHandle, CYBLE_GATT_DB_PEER_INITIATED);
        CyBle_GattsWriteRsp(cyBle_connHandle);
        break;
    }

    default:
        break;
    }
}

static void handle_frame(const link_frame_t *f)
{
    switch (f->type) {
    case LINK_T_DATA_TO_APP:
        notify_app(f->payload, f->len);
        break;
    case LINK_T_ADV_CONFIG: { /* "AI Terra <machine id>" as the GAP device name */
        char name[CYBLE_GAP_MAX_LOCAL_NAME_LENGTH + 1] = "AIT ";
        uint16_t n = f->len;
        if (n > sizeof(name) - 5) n = sizeof(name) - 5;
        memcpy(&name[4], f->payload, n);
        name[4 + n] = '\0';
        CyBle_GapSetLocalName(name);
        break;
    }
    case LINK_T_PAIR_REPLY:
        CyBle_GapAuthPassKeyReply(cyBle_connHandle.bdHandle, 0, f->len && f->payload[0]);
        break;
    case LINK_T_PING:
        link_tx(LINK_T_PING, f->payload, f->len);
        break;
    default:
        break;
    }
}

int main(void)
{
    CyGlobalIntEnable;
    UART_Start();
    link_decoder_init(&dec);
    CyBle_Start(ble_event);

    for (;;) {
        CyBle_ProcessEvents();

        while (UART_SpiUartGetRxBufferSize()) {
            uint8_t b = (uint8_t)UART_SpiUartReadRxData();
            if (link_decode_byte(&dec, b, &rx_frame))
                handle_frame(&rx_frame);
        }

        if (credits_owed && CyBle_GattGetBusyStatus() == CYBLE_STACK_STATE_FREE) {
            grant(credits_owed);
            credits_owed = 0;
        }
    }
}
