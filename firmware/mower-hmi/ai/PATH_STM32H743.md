# Edge-AI path A: STM32H743 (maxAI 280 / 430 class)

**Primary toolchain: ST Edge AI** (decision of 2026-11-26). TensorFlow Lite Micro stays as the
open-source reference and fallback; both use the same `ai_backend_t` contract, so switching is one
CMake option.

| Backend | File | CMake |
|---|---|---|
| ST Edge AI Core / X-CUBE-AI (default) | `ai/backends/ai_backend_stedgeai.c` | `-DAI_BACKEND=stedgeai -DSTEDGEAI_DIR=<generated>` |
| TensorFlow Lite Micro + CMSIS-NN | `ai/backends/ai_backend_tflm.cc` | `-DAI_BACKEND=tflm -DTFLM_DIR=<tree>` |

Resource budget for graphics + AI together: `tools/resource_budget/` (worst-case scenarios).

## ST Edge AI steps

1. **Analyse** (no board needed):
   `stedgeai analyze -m tools/edge_ai_poc/phase1_artifacts/cnn/model_int8.tflite --target stm32h7`
   Record MACC, weights (ROM) and activations (RAM). Expected order: 44 k MACC, about 9 KB ROM,
   a few KB RAM. Repeat with `lstm/model.h5` (ST Edge AI quantises it; phase 1 TFLite could not).
2. **Validate on the host** with our data:
   `stedgeai validate -m <model> --target stm32h7 --valinput test_x.npy --valoutput test_y.npy`
   (save `test.npz` keys `x` and one-hot `y` as .npy first; VERIFY the option names for your version).
   Pass: model-only macro-F1 >= 0.90.
3. **Benchmark before hardware** in ST Edge AI Developer Cloud on the closest H7 board in the farm
   (VERIFY the board list). Gives real cycles on a Cortex-M7 at 400-480 MHz.
4. **Generate**: `stedgeai generate -m <model> --target stm32h7 --name network -o <GEN_DIR>`,
   or use X-CUBE-AI inside STM32CubeMX next to X-CUBE-TOUCHGFX in the same project.
5. **Build**: `cmake ... -DAI_BACKEND=stedgeai -DSTEDGEAI_DIR=<GEN_DIR>`. Activations go to DTCM
   (`.dtcm_bss`), weights stay in internal flash.
6. **Validate on target**: `stedgeai validate --mode target` over the debug UART with the
   validation firmware, then time `infer()` with DWT inside the real app while the GUI runs
   (scenarios S2 and S4 of the budget).
7. **NanoEdge AI Studio** for the dull-blade trend (phase 2): anomaly-detection library with
   on-device learning per machine, fed with trip summaries, not 60 s windows. Check the licence
   terms for production use.

## TFLM reference path

## What runs

| Item | Value |
|---|---|
| Model | int8 1-D CNN, `tools/edge_ai_poc/phase1_artifacts/cnn/model_int8.tflite` |
| Embedded copy | `ai/generated/model_int8_tflite.h` (8928 B, 16-byte aligned, place in internal flash) |
| Input | 60 s x 9 features, normalised with `ai/generated/ai_scaler.h`, int8 (scale 0.0513, zero 4) |
| Output | 4 class probabilities: normal, screen clog, low oil, hydraulic overheat |
| Operators | CONV_2D, FULLY_CONNECTED, MEAN, RESHAPE, SOFTMAX |
| Compute | 43,664 MACs per inference, one inference every 30 s |
| Tensor arena | 16 KB reserved (measure and trim, see step 4), in DTCM or AXI SRAM, never SDRAM |
| Latency estimate | 0.18 to 0.36 ms at 480 MHz (estimate, not yet measured) |

## Build steps

1. Export a TFLM tree with CMSIS-NN kernels:
   ```
   git clone https://github.com/tensorflow/tflite-micro && cd tflite-micro
   python3 tensorflow/lite/micro/tools/project_generation/create_tflm_tree.py \
       --makefile_options="OPTIMIZED_KERNEL_DIR=cmsis_nn TARGET=cortex_m_generic TARGET_ARCH=cortex-m7+fp" \
       /path/to/tflm_tree
   ```
2. Configure the firmware:
   ```
   cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
         -DSTM32CUBE_H7=/path/to/STM32CubeH7 -DAI_BACKEND=tflm -DTFLM_DIR=/path/to/tflm_tree
   cmake --build build
   ```
   STM32CubeIDE works too: add `ai/`, `ai/backends/ai_backend_tflm.cc` and the TFLM tree, define
   `TF_LITE_STATIC_MEMORY`, `CMSIS_NN`.
3. Enable the M7 I-cache and D-cache before `ai_health_init()` (already done in the board init).
4. Measure the arena once with `tflite::RecordingMicroInterpreter` and set `kArenaBytes` to the
   measured value plus 1 KB.
5. Time `ai_backend_get()->infer()` with the DWT cycle counter over the 574 windows in
   `phase1_artifacts/test.npz` (send them over the debug UART or bake a few into flash) and compare
   predicted classes with `report.json`. Pass: same macro-F1 as the host TFLM check (0.908 model only).

## Host evidence already produced

`tools/edge_ai_poc/tflm_check.py` ran the same `.tflite` through TFLM kernels on Linux:
4550 of 4590 test windows bit-exact with the TFLite reference, max difference 10 int8 steps,
macro-F1 0.9083. The 40 differing windows come from reference vs. TFLM rounding in MEAN/SOFTMAX.
