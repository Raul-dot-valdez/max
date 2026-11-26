# max
architecture reviews, strategy decks

## Contents

| Path | What it is |
|---|---|
| `firmware/mower-hmi/` | STM32H743 mower HMI firmware: PAL / OSAL portability layer, J1939, data logging, fuel and runtime estimates, GPS services, BLE fleet link (PSoC 4), TouchGFX / Qt for MCUs GUI back ends, edge-AI health classifier. Host tests: `make -C firmware/mower-hmi/tests` |
| `firmware/mower-hmi/docs/firmware_architecture_1.md` | maxAI 430 firmware map as built: layers, data flow, tasks, what must shrink to fit the 430 |
| `firmware/mower-hmi/docs/firmware_architecture_2.md` | One firmware for every C1 and C2 mower: machine profiles, analog inputs, AI heads |
| `firmware/mower-hmi/ai/` | Edge-AI module, with backends for ST Edge AI (default), TFLite Micro and GD32 Embedded AI. Path guides: `PATH_STM32H743.md`, `PATH_GD32H759.md` |
| `firmware/mower-hmi/tools/mower_j1939_synth/` | Synthetic mower J1939 dataset generator and validator (`DESIGN.md` holds the expert debate) |
| `firmware/mower-hmi/tools/edge_ai_poc/` | Edge-AI PoC phase 1: training, TFLM check, artifacts for the vendor tools, `PHASE1_REPORT.md` |
| `firmware/mower-hmi/tools/resource_budget/` | Worst-case graphics + AI resource budget for maxAI 430-class displays |
| `docs/architecture/` | Scag EVZ MowNet architecture, maxAI edge-AI architecture, maxAI 430 graphics + AI budget (HTML pages, open in a browser) |
| `docs/strategy/` | maxAI graphics framework strategy (HTML and deck), maxAI SDK review deck (PPTX + PDF), C1 North Star and family-by-family edge-AI rollout debate |
| `docs/wainbee/` | Wainbee working-session deck and `maximatecc_for_claude.md` |
| `docs/fleet-expansion/` | C1 + C2 fleet review: models, engine families, J1939 signal matrix, expert debate |
| `docs/cra/` | maxAI 430 CRA dual roadmap workbook (XLSX) |
| `data/mower_j1939_sample/` | Sample of the synthetic dataset: candump logs, 1 Hz signals, labels, `windows.npz` |
| `tools/deck-build/` | pptxgenjs scripts that build the decks. They need `pptxgenjs` (`npm install`) and the theme helper from the pptx skill: set `APPLY_THEME=/path/to/apply_theme.js` |
| `tools/cra-roadmap/` | openpyxl script that builds the CRA workbook |


The full synthetic dataset (about 50 MB) is not stored here. Regenerate it with
`python3 firmware/mower-hmi/tools/mower_j1939_synth/synth.py --out out`.
