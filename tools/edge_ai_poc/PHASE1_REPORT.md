# Edge-AI PoC phase 1: report

Data: `tools/mower_j1939_synth` output `windows.npz` (seed 20261126, 3 machines, 21 days).
Script: `train_phase1.py` (TensorFlow 2.21). TFLM check: `tflm_check.py`.

## Task

Classify each 60 s window of 9 J1939 features into normal, screen clog, low oil or hydraulic
overheat, every 30 s, on the display MCU. Advisory only, not a safety function.

## Split

Grouped by machine and session, so no window from a test session is seen in training.
Train 23,927, validation 4,492, test 4,590 windows.

## Results on the test split

| Model | Macro-F1 | Notes |
|---|---|---|
| Rules only | 0.9145 | low oil F1 0.75 |
| CNN float | 0.9052 | 3,148 parameters, 43,664 MACs |
| CNN int8 | 0.9077 | 8,928 B, agrees with float on 99.4 % of windows |
| LSTM float | 0.9468 | 190,528 MACs, int8 conversion crashed in TF 2.21 |
| **Hybrid: rules + CNN int8** | **0.9601** | deployed in `ai/ai_health.c` |

Hybrid per class F1: normal 0.985, screen clog 0.936, low oil 0.920, hydraulic overheat 1.000.
The rules catch clog and overheat well; the model adds what rules miss on low oil.

TFLM kernels on the host: 4550 of 4590 outputs bit-exact with the TFLite reference, macro-F1 0.9083.

## Decisions

1. Dull blades removed from the 60 s classifier. A drift over weeks is not visible in one minute
   (F1 about 0.2 when kept). It belongs in a trend model over trip summaries (phase 2).
2. Hybrid in firmware: model owns low oil, rules own clog and overheat, alert after 2 repeats.
3. CNN is the deployment model on both MCUs; LSTM goes to the GD32 tool as a second candidate.

## Limits, stated plainly

- Synthetic data only. Hydraulic overheat labels follow the visible hydraulic temperature, so
  that class is easy by construction. Real-machine data must confirm every number here.
- Latencies (0.18 to 0.36 ms H743, 0.15 to 0.29 ms GD32) are estimates from MAC count, not
  measurements.
- The GD32 Embedded AI tool runs on Windows and needs the board. It was not run in phase 1. Its
  inputs are ready; see `ai/PATH_GD32H759.md`.

## Reproduce

```
python3 ../mower_j1939_synth/synth.py --out ../mower_j1939_synth/out   # writes windows.npz
python3 train_phase1.py --windows ../mower_j1939_synth/out/windows.npz --out phase1_artifacts
cp phase1_artifacts/ai_scaler.h phase1_artifacts/model_int8_tflite.h ../../ai/generated/
python3 tflm_check.py --windows ../mower_j1939_synth/out/windows.npz --artifacts phase1_artifacts
```
