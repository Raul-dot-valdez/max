# Synthetic mower J1939 dataset generator

A physics-informed, deterministic generator of J1939 data for a commercial zero-turn mower: what a maxAI 430-class display would receive, plus hidden ground truth for faults and clock behaviour. Read **DESIGN.md** for the expert debate behind every choice.

## Run

```sh
cd firmware/mower-hmi/tools/mower_j1939_synth
python3 synth.py --out out                 # default: 3 machines × 21 days, < 1 min, ~50 MB
python3 validate.py --out out --firmware   # physics, frame and firmware-replay checks
python3 synth.py --out out_small --machines 1 --days 3 --seed 7   # quick variant
```

Requirements: Python 3.10+, `numpy`, `pandas` (validation only), and `gcc` for `--firmware`.

## Outputs

| File | Content |
|---|---|
| `signals_1hz.csv.gz` | One row per second of key-on time: decoded J1939 values (quantised as on the bus), DTCs and lamps, three clocks (`t_true_utc`, `t_rtc`, `uptime_s`), and ground-truth `gt_*` columns |
| `labels.csv` | Events and fault intervals: blade strikes, screen cleaning, refuel, fuel theft, oil leak and repair, hydraulic fan fault, sender fault, RTC reset and sync |
| `sessions.csv` | One row per key-on: site grass load and slope, RTC mode and clock error at power-up |
| `can/*.log` | Raw frames in `candump` format at real J1939 rates for sample sessions (replay with `canplayer` or the firmware's `tests/replay_candump`) |
| `windows.npz` | ML windows: `x` (N × 60 s × 9 features), `y` class (normal, screen_clog, low_oil, hyd_overheat, dull_blades), `meta`. Ready for training or as int8 calibration data |
| `metadata.json` | Generator version, config and hash, signal dictionary (PGN, SPN, scaling), clock definitions |

## Configure

Edit `config_default.json` or pass a copy with `--config`:
- `machines`, `days`, `start_date`, `climate` (`temperate_continental`, `hot_dry`, `maritime`)
- work days, rain and heat-wave probabilities
- machine profile
- fault rates
- RTC modes and storage period
- CAN log length and EEC1 rate
- `engine_families` (generator 0.2.0): list cycled over the machines, from `liquid_j1939`
  (default), `air_efi` (no coolant SPN, oil temperature SPN 175) and `carb` (no ECU, no CAN log;
  rpm, fuel level, battery, PTO and a low-oil switch come from the display's inputs)

`config_fleet.json` is a ready-made mixed fleet: 6 machines, 14 days, all three families. Its
`windows.npz` adds `mask`, `aux` and `family` arrays for training per-family AI heads. See
`docs/firmware_architecture_2.md`.

The same config and seed always give the same data.

## Default dataset at a glance (seed 20261126)

3 machines, 21 days from 1 June 2027: about 1.0 million one-second rows, 310 key-on sessions, about 33,000 ML windows covering all five classes, and three 10-minute CAN logs of about 65,000 frames each. All validation checks pass, including the replay through the firmware J1939 stack.
