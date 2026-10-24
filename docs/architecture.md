# Mower HMI firmware architecture

7" touch display for an OEM commercial mower. It reads the machine over J1939, estimates runtime and fuel, logs fleet data, adds GNSS-based features, and syncs to a phone app over Bluetooth.

## 1. System block diagram

```
                ┌──────────────────────────── STM32H743 (400 MHz M7) ─────────────────────────────┐
 Vehicle CAN ──►│ FDCAN1 ─► J1939 stack ─► vehicle_data ─┬─► fuel_runtime ─┐                       │
 (J1939 250k)   │                                        ├─► anomaly (ML)  ├─► ui_model ─► GUI ────┼─► LTDC ─► 7" 800x480
                │ USART2 ─► NMEA ─► gps_services ────────┘                 │   (TouchGFX | Qt MCU) │   GT911 touch (I2C)
 GNSS ─────────►│                                                          ├─► datalog ─► W25Q512 ─┼─ QUADSPI (64 MB NOR)
                │ I2C1 ─► RV-3028 RTC (time base)                          │                       │
 PSoC 4 BLE ◄──►│ USART3 ◄─► psoc_link ◄─► fleet_ble ◄─────────────────────┘                       │
 (BT 4.2)       │ internal flash: bootloader | app | nvstore A/B         FMC ─► IS42S32800J 32 MB    │
                └──────────────────────────────────────────────────────────────────────────────────┘
```

## 2. Layering and the MCU-portability contract

| Layer | Folder | May include vendor headers? | Changes when the MCU vendor changes? |
|---|---|---|---|
| App composition | `app/` | No | No |
| GUI bridge | `gui/ui_model.*` | No | No |
| GUI backend | `gui/touchgfx`, `gui/qtmcu` | GUI framework only | TouchGFX: yes (ST-only licence). Qt for MCUs: no |
| Services | `services/` | No | No |
| Protocol middleware | `middleware/j1939` | No | No |
| Device drivers | `drivers/` | No | No (they sit on the PAL) |
| **PAL** API | `pal/include` | No | No |
| **PAL port** | `pal/port/stm32h7` | **Yes** | **Rewritten: about 900 lines** |
| OSAL | `pal/osal` | RTOS headers | Only if the RTOS changes |

Rules that keep this honest:

1. **The prefix is `pal_`, not `hal_`.** ST, NXP and Renesas all ship a `HAL_` namespace. A port file must include both without clashes.
2. **Logical resources, not peripherals.** App code says `BOARD_CAN_VEHICLE` and `PAL_PIN_IGN_SENSE`, never `FDCAN1` or `GPIOK`. A respin edits `config/board_config.h` and the port's pin table only.
3. **Link-time selection for the PAL; runtime ops structs for devices.** The MCU is fixed per build, so `pal_*` functions are plain functions chosen by `PAL_PORT` in CMake, with no indirection cost. External parts can vary per board variant (NOR vendor, RTC), so they go behind `flash_dev_t` / `rtc_dev_t` function tables.
4. **No GUI calls from app code.** The app publishes one display-ready `ui_model_t` at 10 Hz and reads `ui_cmd_t` back. TouchGFX is free but licensed for STM32 only, so moving to NXP i.MX RT, Renesas RA8D1 or Infineon TRAVEO T2G means switching to the Qt for MCUs backend. Vehicle logic is untouched.
5. **CI enforces it.** `make -C tests` builds every portable module with the host compiler, with no SDK present. If someone adds `#include "stm32h7xx.h"` to a service, the build fails.

### Porting checklist (e.g. NXP i.MX RT1170)

| PAL file | STM32H7 | RT1170 equivalent |
|---|---|---|
| `pal_can` | FDCAN classic | FlexCAN3 (`FLEXCAN_TransferReceiveFifoNonBlocking`) |
| `pal_uart` | USART + DMA idle line | LPUART + eDMA, idle-line interrupt |
| `pal_i2c` | I2C v2 | LPI2C |
| `pal_qspi` | QUADSPI | FlexSPI (IP command + AHB mapping) |
| `pal_flash` | internal flash, 32-byte words | no internal flash: map to a FlexSPI NOR partition |
| `pal_display` | LTDC | LCDIFv2 (+ PXP for 2D) |
| `pal_sys` | RCC/MPU/FMC SDRAM | CCM/MPU/SEMC SDRAM |

## 3. Memory map

### Internal flash (2 MB, 2 banks × 8 × 128 KB, 256-bit ECC words)

| Range | Size | Use |
|---|---|---|
| 0x0800_0000 | 128 KB | Bootloader: verifies the image CRC, copies a staged update from NOR |
| 0x0802_0000 | 1664 KB | Application, plus TouchGFX fonts and texts |
| 0x081C_0000 | 128 KB | nvstore sector A |
| 0x081E_0000 | 128 KB | nvstore sector B |

### Internal SRAM (1 MB)

| Region | Size | Use |
|---|---|---|
| ITCM | 64 KB | Reserved for hot ISR / DSP code |
| DTCM | 128 KB | `.data`, `.bss`, MSP stack (CPU-only, zero wait) |
| AXI SRAM (D1) | 512 KB | FreeRTOS heap (task stacks, queues) |
| SRAM1-3 (D2) | 288 KB | DMA buffers, non-cacheable via MPU |
| SRAM4 (D3) | 64 KB | Spare / low-power domain |
| Backup SRAM | 4 KB | Crash context (future) |

> The H743 has no *internal SDRAM*: it has about 1 MB of internal SRAM, mapped above. Large buffers go to the external ISSI SDRAM.

### External SDRAM: ISSI IS42S32800J (automotive part: IS45S32800J-A2), 32 MB on FMC bank 1 at 100 MHz

| Address | Size | Use | MPU attribute |
|---|---|---|---|
| 0xC000_0000 | 750 KB | Framebuffer 0 (RGB565) | write-through |
| 0xC00C_0000 | 750 KB | Framebuffer 1 | write-through |
| 0xC020_0000 | 16 MB | GUI bitmap cache (copied from NOR at boot) | write-back |
| 0xC120_0000 | about 14 MB | `.sdram_bss`: log sector index (44 KB), coverage grid (2 MB), headroom | write-back |

LTDC bandwidth: 800×480×2 B×60 Hz = 46 MB/s, about 12 % of the 400 MB/s the 32-bit, 100 MHz SDRAM bus offers. DMA2D and CPU traffic have plenty of headroom.

### External NOR: Winbond W25Q512JV, 64 MB, QUADSPI at 100 MHz

| Offset | Size | Use |
|---|---|---|
| 0x000_0000 | 16 MB | GUI bitmaps (`ExtFlashSection`), written by the CubeProgrammer external loader |
| 0x100_0000 | 4 MB | Firmware update staging (bootloader copies to internal flash) |
| 0x140_0000 | 44 MB | Fleet data log ring: 720,896 × 64-byte records |

**External flash arbitration.** QUADSPI is either memory-mapped (XIP reads) or in indirect mode (program/erase), never both. TouchGFX would normally read bitmaps straight from the mapped NOR, which would block logging. The design resolves it as follows: at boot the NOR is mapped, every bitmap is copied to the SDRAM cache (`Bitmap::cacheAll()`), and the NOR is unmapped. From then on the logger is the only user. Fonts and texts are linked into internal flash for the same reason.

## 4. Task model (FreeRTOS)

| Task | Prio | Wakes on | Stack | Owns |
|---|---|---|---|---|
| wdg | 7 | 500 ms | 1 KB | IWDG: kicks only if **every** worker task checked in |
| can | 6 | CAN RX queue / 20 ms | 4 KB | J1939 stack, signal decode, blade-impact detector |
| gnss | 5 | UART chunks | 3 KB | NMEA, geofences, coverage, GNSS→RTC time sync |
| ble | 4 | UART chunks / 20 ms | 4 KB | PSoC link, fleet protocol, log streaming |
| app | 4 | 100 ms | 6 KB | fuel/runtime, ML, logging policy, UI model, power |
| gui | 3 | vsync | 8 KB | TouchGFX or Qt for MCUs |
| log | 2 | log queue | 3 KB | **only** writer of the NOR log |

- ISRs only copy into queues (`osal_queue_send_isr`). No decoding happens in interrupt context.
- One clock domain: CAN frames are stamped with `osal_now_ms()` when dequeued, so J1939 timeouts, signal staleness and the TP timers all use the same clock.
- Lock order is fixed: `mtx_app` → `mtx_gps` → `mtx_fix`. Event callbacks may take `mtx_fix` while their caller holds `mtx_app` or `mtx_gps`. No path takes them in the reverse order.
- Every IRQ that calls FreeRTOS has NVIC priority ≥ 5 (`configMAX_SYSCALL_INTERRUPT_PRIORITY`).

## 5. J1939 integration

| PGN | Name | SPN used | Rate | Staleness timeout |
|---|---|---|---|---|
| 61444 | EEC1 | 190 engine speed | 10–20 ms | 300 ms |
| 61443 | EEC2 | 92 load at speed | 50 ms | 300 ms |
| 65262 | ET1 | 110 coolant temp | 1 s | 3 s |
| 65263 | EFL/P1 | 100 oil pressure | 0.5 s | 3 s |
| 65266 | LFE1 | 183 fuel rate | 100 ms | 1 s |
| 65276 | DD1 | 96 fuel level | 1 s | 3 s |
| 65253 | HOURS | 247 engine hours | **on request**: we request every 10 s | 35 s |
| 65271 | VEP1 | 168 battery potential | 1 s | 3 s |
| 65265 | CCVS1 | 84 wheel speed | 100 ms | 300 ms |
| 65128 | VF | 1638 hydraulic temp | 1 s | 3 s |
| 65226 | DM1 | active DTCs, lamps (single frame or BAM/RTS-CTS) | 1 s | 3.5 s |
| 0xFF10 | OEM prop. B | blade clutch, deck height | **OEM to define** | 1 s |

Network management: we claim preferred address 0x28 (Cab Display #1). If we lose arbitration we fall back to 128–247 (arbitrary-address capable). Destination-specific requests we can't answer get a NACK, and a peer's TP session gets an Abort on timeout, as J1939-21 requires.

## 6. Fuel and runtime

- **Fuel remaining** = filtered level × tank volume. The level is median-of-5 filtered, then low-passed with τ = 60 s, which rejects slosh on slopes.
- **Burn rate:** ECU fuel rate (SPN 183) when it is broadcast, smoothed over about 2 min. Otherwise a least-squares slope of litres against engine-running seconds over a 30-minute window. Many small mower engines don't send LFE1, so the fallback matters. The UI marks this estimate "est.".
- **Runtime left** = (remaining − reserve) / burn rate, shown as "3 h 25".
- **Counters**, persisted every 6 min and at key-off: key-on time, engine time, blade time, fuel used, and the service due point.
- **Events:** low fuel; refuel; unexplained fuel drop (possible theft or leak), including a drop that happened while the machine was off, found by comparing against the level stored at key-off.

## 7. GNSS features (customer value)

| Feature | Who benefits | How |
|---|---|---|
| Mowed area, productivity (m²/blade-hour), overlap % | Contractors bidding and billing jobs | 0.25 m bitmap over a 1 km job area, stamped with the deck swath while the blades are on |
| Job-site fences and engine hours per site | Billing, crew accountability | Up to 4 polygons, set from the app |
| Keep-in fence | Theft / unauthorised use | Exit event, logged with position, shown in the UI |
| Breadcrumb in each periodic record | Fleet map, proof of service | Position every 10 s while running |
| Automatic day/night theme and backlight | Operator comfort, no ambient sensor | Solar elevation from position and UTC |
| Time authority | Log integrity | GNSS UTC disciplines the RV-3028 |

Accuracy is limited by the receiver: about 2 m CEP standalone, better with SBAS. With RTK (fix quality 4), overlap % becomes a real coverage map.

## 8. On-device ML (TinyML)

**Stage 1 (shipped):** a self-learning, operating-point-conditioned anomaly detector (`services/anomaly.c`).
- 16 bins (rpm × load). Per bin it keeps the mean and variance of coolant temp, oil pressure, fuel rate and hydraulic temp. It learns this specific machine in its first hours, and the model persists in nvstore (about 600 B).
- Scoring starts only after 10 minutes of continuous running, so warm-up transients never score. The score is the maximum |z| with per-sensor σ floors, an alert needs 30 s above 4σ, and adaptation pauses while the score is high, so the model doesn't learn a fault as normal.
- Catches: cooling degradation (clogged screens, a common failure on mowers), oil pressure loss, hydraulic overheating, and abnormal fuel draw.
- **Blade-impact detection:** a sudden drop in EEC1 rpm while the blades are engaged is logged with position, prompting a blade and spindle inspection.

**Stage 2 (roadmap):** a fleet-trained int8 model (TFLite Micro + CMSIS-NN on the M7, about 50–200 KB) using the same feature vector, trained on logged fleet data uploaded through the app. The `an_input_t` boundary is the plug-in point.

**Not on the safety path:** generative or large models are not used for any machine-control decision. The HMI is display-only toward the vehicle.

## 9. Fleet data and the mobile app

- **Record format:** 64-byte fixed records with a monotonic sequence number and CRC-16 (`services/datalog.h`). Types: periodic (every 10 s running, 60 s key-on idle), event, DTC change, trip summary.
- **Power-fail safety:** writes only go to erased slots. A torn write invalidates one slot, which is skipped on remount. Remount reads one slot per 4 KB sector. Tested with injected power cuts.
- **Sync protocol** (`services/fleet_ble.h`): `READ_LOG(from_seq, max)` → credit-paced `LOG_CHUNK` events → `LOG_END`; then `ACK_LOG(seq)` once the app's cloud upload succeeds. The ack is stored on the machine, so any phone in the fleet resumes where the last one stopped.
- **Security:** BLE 4.2 LE Secure Connections with numeric comparison confirmed on the HMI touchscreen, which requires physical presence in the cab. Characteristics require authenticated encryption, and the STM32 refuses every command unless the PSoC reports an authenticated link.
- **Configuration from the app:** time, tank volume, low-fuel threshold, service interval, deck width, geofences, service done, job reset.

## 10. Power and robustness

- **Ignition sense** with 500 ms debounce leads to the shutdown sequence: trip record, then counters and ML model saved to nvstore, then the log queue is flushed, then the supply latch (`POWER_HOLD`) is released. The hold-up circuit must cover about 2.5 s at full load.
- **Watchdog:** a 2 s IWDG, kicked by the supervisor only when all 6 worker tasks have checked in. The reset cause is logged as `EV_BOOT` at the next start.
- **CAN bus-off:** logged, then automatic recovery.
- **SDRAM:** a data-line and address-line self-test runs before any SDRAM use. A failure halts the unit before the GUI can show garbage.
- **M7 errata:** the MPU blocks speculative reads to the unused FMC bank at 0x6000_0000.
- **Thermal:** 400 MHz (VOS1) rather than 480 MHz, as the −40…125 °C grade requires. A sealed cab display in direct sun needs that margin.

## 11. Specialist review notes

The design was checked from each discipline the brief named. Each decision below is reflected in the code.

| Discipline | Decision |
|---|---|
| **Firmware architecture (off-highway)** | PAL + OSAL boundary enforced by a host build. Logical resources in `board_config.h`. Single writer per storage device. Ignition-driven shutdown with a supply latch. Watchdog only on full task liveness. |
| **STM32H743** | 400 MHz VOS1 for the high-temperature grade. MPU set up before the caches. Write-through framebuffers. Non-cacheable D2 SRAM for DMA. DTCM for CPU-only data. MPU guard on FMC bank 1. Clock tree derived for exact 250 k CAN, 100 MHz SDRAM and a 33 MHz pixel clock. |
| **Flash memory** | Internal: A/B log-structured nvstore in 64-byte (2 × flash word) records, never re-programs a word (ECC), header written last during compaction. NOR: 4-byte opcodes only (no volatile mode bit to lose on brown-out), QE bit written only if clear, 4 KB-sector ring, assets cached to SDRAM so the bus is free for logging. |
| **Data / fleet** | Sequence-numbered, CRC'd fixed records. Ack-based resumable sync. Sizing: 44 MB holds about 2000 engine hours of history, and one engine-hour of data (about 23 KB) syncs in roughly a second. |
| **TinyML** | Learns on the device without labelled data. Operating-point conditioning avoids false alarms from load changes. Warm-up gating. Adaptation pauses while alarmed. Clear path to a fleet-trained int8 model. |
| **Bluetooth** | PSoC is a stateless bridge sharing the framing code with the STM32. Frames sized to one 4.2 DLE notification (244 B). Credit flow control. LESC numeric comparison on the HMI. Authenticated encryption on every characteristic. |
| **HMI (TouchGFX / Qt)** | One `ui_model_t`, pre-scaled integers, "--" for stale signals, and "est." when runtime comes from the level slope. TouchGFX for STM32 now, Qt for MCUs backend ready for a vendor change. Day/night from GPS. |

## 12. Verification status and open items

**Verified on the host** (`make -C tests`, ASan + UBSan, 1458 checks): CRC; COBS/link codec; J1939 IDs, address claim (win, lose, defend), NACK, BAM and RTS/CTS reassembly, TP timeout abort; signal scaling and the error/not-available ranges; staleness; NMEA (both hemispheres, checksum, no-fix, sentence order); datalog (wrap, remount, injected power cuts); nvstore (compaction, remount, torn record); fuel estimator (level-slope and ECU-rate modes, refuel, drop across key-off, low fuel); GPS (polygon, distance, solar elevation, coverage area along and diagonal to the grid, overlap, fence exit); anomaly detector; fleet protocol (auth gate, credit-paced streaming, ack, geofence parsing).

**Compile-checked against PAL headers only** (`make -C tests syntax`): `app/`, and the W25Q, RV-3028 and GT911 drivers.

**Not yet built:** the STM32H7 port, the TouchGFX and Qt backends, and the PSoC 4 firmware. They need the vendor SDKs (STM32CubeH7, TouchGFX Designer, Qt for MCUs, PSoC Creator) and target hardware. Specific values to confirm during bring-up:

1. I2C `TIMINGR` values are placeholders. Generate them in CubeMX from measured rise and fall times.
2. Check the SDRAM timings against the exact ISSI datasheet revision and speed grade fitted.
3. Panel timings (HBP/HFP/VBP/VFP) are typical 800×480 values. Take them from the chosen panel's datasheet.
4. Get the OEM's DBC for the proprietary deck/blade PGN (0xFF10 is a placeholder).
5. Replace the J1939 manufacturer code `0x7FF` with AI Terra's SAE-assigned code.
6. Check the PSoC BLE component API names (numeric comparison, MTU event) against the installed component version.
7. Bootloader and secure firmware update over BLE are not implemented. The 4 MB NOR staging partition is reserved for them.
8. Handle H7 flash ECC double-error faults (NMI/bus fault) during nvstore reads after a torn program, by marking the record bad and continuing.
9. Hardware-in-the-loop: replay a recorded J1939 trace from the OEM mower, and run a GNSS track replay for coverage.
