# PSoC 4 BLE co-processor

Part: CY8C4248LQI-BL583 (PSoC 4 BLE, Bluetooth 4.2). Toolchain: PSoC Creator 4.4, BLE component v3.x.

## Project setup

1. New PSoC Creator project for CY8C4248LQI-BL583.
2. Add `main.c` from this folder, plus `../drivers/psoc_link/psoc_link.c`, `../lib/crc.c`, and the include paths `../drivers/psoc_link` and `../lib`. Both CPUs compile the same framing code.
3. Add an SCB UART component named `UART`: 921600 baud, 8N1, RX/TX buffers of 512 bytes, wired to the STM32 USART3. Clock from the 24 MHz IMO, and check the actual baud error in the component dialog: it must stay within ±2 %.
4. Add a BLE component in the GAP Peripheral / GATT Server role:
   - **Service "Fleet"**, custom 128-bit UUID (generate one for AI Terra and keep it fixed across releases).
     - Characteristic **RX**: Write Without Response, max length 244, permission "Authenticated encryption".
     - Characteristic **TX**: Notify plus CCCD, max length 244, permission "Authenticated encryption".
   - GAP security: Mode 1 Level 4 (LE Secure Connections), IO capability "Display Yes/No", bonding enabled, max 4 bonded devices.
   - Link layer: enable the Data Length Extension (251-byte PDUs) and an ATT MTU of 247.
   - Advertising: fast (30–60 ms) for 30 s, then slow (1 s). Advertising data carries the service UUID.

## Behaviour

- The PSoC is stateless apart from its bonds. Every fleet command is interpreted on the STM32 (`services/fleet_ble.c`).
- The STM32 receives no data, and the PSoC drops writes, until the link is authenticated and encrypted. An unauthenticated link is disconnected.
- **Numeric comparison:** the 6-digit code goes to the STM32 (`LINK_T_PAIR_CONFIRM`) and is shown on the HMI. The operator presses Accept or Reject on the touchscreen, and the result comes back as `LINK_T_PAIR_REPLY`. Without physical access to the cab, a phone cannot pair.
- **Flow control:** each notification queued in the stack earns one credit back to the STM32 once the stack is free (`LINK_T_CREDIT`). The STM32 never sends more than its credits allow, so the 921.6 kbit/s UART cannot overrun the radio.

## Throughput expectation

BLE 4.2 with DLE and a 7.5–15 ms connection interval on a modern phone sustains roughly 20–60 kB/s. One periodic record every 10 s of engine time is about 23 kB per engine-hour, so a full day of mowing syncs in a few seconds.
