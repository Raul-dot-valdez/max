# System HMI firmware

Firmware for a 7" touchscreen display on an OEM commercial mower. It is built on an STM32H743 with FreeRTOS, external ISSI SDRAM, Winbond QSPI NOR, an RV-3028 RTC, a GNSS receiver, and a PSoC 4 BLE (Bluetooth 4.2) co-processor. The display reads the machine over SAE J1939.

**What it does**

- Live engine, fuel and hydraulic data from J1939, plus active fault codes (DM1).
- Fuel remaining and **runtime left**. This works even on engines that don't broadcast fuel rate.
- Lifetime counters: engine, blade and key-on hours, and the service countdown.
- A power-fail-safe fleet data log (44 MB, about 2000 engine hours) that syncs to a mobile app over authenticated BLE.
- GNSS features: mowed area, productivity and overlap; job-site and anti-theft geofences; a position breadcrumb; and automatic day/night display.
- On-device anomaly detection that learns each machine. It also detects blade strikes.
- An MCU-independent architecture. Only `pal/port/<mcu>` and the TouchGFX backend are STM32-specific.

## Layout

```
app/             task composition, power/shutdown, logging policy
config/          board_config.h (BOM, memory map, pins), FreeRTOSConfig.h
pal/include/     platform abstraction API (CAN, UART, I2C, QSPI, flash, GPIO, display, sys) + OSAL
pal/port/stm32h7 the only STM32-specific code
pal/osal/        FreeRTOS binding
drivers/         W25Q NOR, RV-3028 RTC, NMEA GNSS, GT911 touch, PSoC link codec
middleware/j1939 J1939-21/-81 stack and J1939-71/-73 decoders
services/        vehicle data broker, datalog, nvstore, fuel/runtime, GPS services, anomaly ML, fleet BLE protocol
gui/             ui_model bridge, TouchGFX backend, Qt for MCUs backend
psoc4_ble/       PSoC 4 BLE bridge firmware + GATT setup
linker/ cmake/   target build
tests/           host unit tests (no SDK, no hardware)
docs/            architecture.md: design, memory map, task model, specialist review notes, open items
```

## Build

```sh
# Host tests: runs everywhere, no vendor SDK needed
make -C tests            # 1458 checks under ASan/UBSan
make -C tests syntax     # compile-check app + drivers against PAL headers

# Target (requires arm-none-eabi-gcc, STM32CubeH7, and a CubeMX board package)
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
      -DSTM32CUBE_H7=/opt/STM32CubeH7 -DGUI_BACKEND=touchgfx
cmake --build build
```

The target build has not been run yet. It needs the CubeMX-generated board package (`board/stm32h743_mower/cubemx`), the TouchGFX Designer project, and real hardware. See `docs/architecture.md` §12 for the bring-up checklist.

## Independence from the website

This folder is separate from the Astro site at the repository root. The site build (`npm run build` → `dist/`) does not read it.
