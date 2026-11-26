# Synthetic mower J1939 dataset: design and expert debate

## The brief

Create a **simple, sustainable, close-to-real** J1939 dataset for a commercial zero-turn mower, with the signals a maxAI 430-class display sees and realistic real-time-clock behaviour. It must be good enough to develop and test:

- display firmware: decoding, staleness, diagnostics
- the fuel and runtime estimator
- on-device AI: anomaly and fault models, int8 calibration for tools such as GD32 Embedded AI
- the fleet-data pipeline

It must also be honest about being synthetic.

## The panel

| Role | Lens |
|---|---|
| Senior mower system architect (chair) | Duty cycles, what really breaks on mowers, what the display must show |
| Synthetic-data scientist | Generation method, label quality, avoiding leakage, distribution checks |
| Powertrain and thermal modeller | Engine load, governor, cooling, fuel, hydraulics |
| J1939 network engineer | Message set, rates, scaling, transport protocol, request/response |
| Dealer service technician | Real failure modes, how operators actually work |
| Embedded timekeeping engineer | RTC hardware, drift, backup, sync sources |
| TinyML engineer | Windows, classes, feature set, class balance |
| Data governance lead | Licensing, versioning, provenance, sustainability |

## Debate 1: physics simulation or a generative model?

- **Synthetic-data scientist:** "A GAN or diffusion model trained on fleet logs gives the most realistic texture."
- **Powertrain modeller:** "We have no fleet logs yet, so there's nothing to train it on. And a generative model can produce physically impossible data, such as fuel burn without engine load or coolant cooling under full load."
- **Data governance lead:** "Physics with explicit parameters is explainable, reproducible from a seed, and free of licence questions."

**Verdict.** Use a physics-informed stochastic simulator now. Each signal is driven by a simple, documented model: governor droop, a heat balance with a thermostat and radiator, brake-specific fuel consumption, hydraulic heat, and a fuel sender with slosh. Randomness comes from the operator, the site and the weather. Later, when real logs exist, fit the simulator's parameters to them and, if needed, learn a *residual* noise model on top. Never replace the physics.

## Debate 2: how simple is "simple"?

- **J1939 engineer:** "Model the full bus: every PGN at its real rate, address claim, requests, multi-packet DM1."
- **TinyML engineer:** "Training needs clean 1 Hz tables, not 50 Hz frames."
- **Mower architect:** "Both, but not for everything."

**Verdict.** Two products from one simulation:

1. **`signals_1hz.csv.gz`**: decoded values exactly as the display stores them (J1939 quantisation applied), plus ground-truth `gt_*` columns that never appear on the bus.
2. **`can/*.log`**: raw frames in Linux `candump` format for a few sample sessions, at real rates (EEC1 every 20 ms, EEC2 50 ms, LFE1 and CCVS1 100 ms, EFL/P1 500 ms, ET1, DD1, VEP1, AMB, VF and DM1 every 1 s). They include address claims, the display requesting engine hours every 10 s with the engine responding, and DM1 sent with BAM multi-packet transport when more than one fault is active.

The core simulation runs at 1 Hz, which keeps a season of data fast to generate. Sub-second detail (blade-strike rpm dips of about 300 ms, interpolation, bus jitter) is synthesised only when writing CAN logs.

## Debate 3: which signals?

**Verdict.** Keep the set to what small off-highway engines and mower controllers actually broadcast, and stay consistent with the firmware decoder.

| Signal | Message | PGN | SPN | Why |
|---|---|---|---|---|
| Engine speed, demand / actual torque | EEC1 | 61444 | 190, 512, 513 | Load, governor behaviour, blade-strike dips |
| Load at current speed, pedal | EEC2 | 61443 | 92, 91 | Duty cycle |
| Coolant, fuel and oil temperature | ET1 | 65262 | 110, 174, 175 | Cooling health: radiator screen clogging is the classic mower failure |
| Oil pressure | EFL/P1 | 65263 | 100 | Oil leak and pump wear |
| Fuel rate, throttle | LFE1 | 65266 | 183, 51 | Runtime estimate, efficiency, dull blades |
| Fuel level | DD1 | 65276 | 96 | Runtime left, refuel and theft detection, sender faults |
| Battery and charging voltage | VEP1 | 65271 | 168, 167, 158 | Charging system, crank dips |
| Wheel-based speed | CCVS1 | 65265 | 84 | Duty cycle, productivity |
| Ambient, barometric, inlet | AMB | 65269 | 171, 108, 172 | Context for thermal models |
| Hydraulic oil temperature | VF | 65128 | 1638 | Hydrostatic drive health |
| Engine hours | HOURS | 65253 | 247 | On request, as in real machines |
| Active faults and lamps | DM1 | 65226 | — | Ground-truth-linked DTCs |
| Blade engaged, deck height | Proprietary B (placeholder) | 0xFF10 | — | Mowing state; layout must come from the OEM |
| Time and date (optional) | TD | 65254 | 959–964 | A clock source on the bus, if the machine has one |

## Debate 4: what goes wrong, and how do we label it?

- **Dealer technician:** "On mowers it's grass:
  - clippings clog the radiator screen and hydraulic cooler on dry days, and temperatures creep up over hours
  - blades go dull and the engine works harder
  - stones and stumps hit the blades
  - fuel goes missing overnight from unlocked tanks
  - oil leaks
  - level senders fail
  - hydraulic cooling fans quit."
- **Synthetic-data scientist:** "Labels must come from the simulation's hidden state, not from thresholds on the signals the model will see. Otherwise the labels leak the answer."

**Verdict.** Faults are latent physical states that evolve and get repaired, exactly as in the field:

| Fault | Physics | Repair | Label source |
|---|---|---|---|
| Radiator screen clogging | Grows with grass load on dry sites; reduces radiator heat rejection up to 75 % | Operator cleans at lunch (55 %) or end of day (60 %) | `gt_screen_clog`, `screen_cleaned` events |
| Dull blades | Grows with mowing hours, faster on stony sites; raises blade power up to 35 % | Sharpened when dull | `gt_blade_dullness`, `blades_sharpened` |
| Air filter restriction | Slowly raises fuel use | Replaced at threshold | `gt_air_filter`, `air_filter_replaced` |
| Blade strike | Random per mowing second on stony sites; about 300 ms rpm dip of 16–28 % | — | `blade_strike`, `gt_blade_strike` |
| Oil leak | Oil pressure falls over running hours | Dealer repair after the red lamp has been on for 3 minutes | `oil_leak` interval, `oil_leak_repaired` |
| Hydraulic cooler fan failure | Hydraulic oil runs about 38 °C hotter for a day | Next day | `hyd_cooler_fan_fault` |
| Fuel sender fault | DD1 intermittently "not available"; DM1 SPN 96 FMI 2 | After 2 days | `fuel_sender_fault` |
| Fuel theft | Overnight level drop of 15–45 % | — | `fuel_theft` |

DTCs are generated from the physics, with occurrence counts:
- SPN 110 FMI 16 (coolant high) or FMI 0 (critical)
- SPN 100 FMI 18 (oil pressure low)
- SPN 1638 FMI 16 (hydraulic temperature high)
- SPN 96 FMI 2 (sender erratic)

**Coverage rule:** rare faults left to chance can vanish from a dataset (this happened during development). Each discrete fault type is therefore guaranteed to appear on at least one machine.

## Debate 5: the maxAI 430 clock

- **Timekeeping engineer:** "maximatecc's public 430 specifications don't state whether it has a battery-backed RTC (**verify**). So simulate the three realistic cases:
  - a coin-cell RTC
  - a supercapacitor RTC that holds for about a week
  - no RTC at all."
- **TinyML engineer:** "Models must not learn from wall-clock artefacts."
- **Mower architect:** "Mowers sit in storage all winter. That's when clocks die."

**Verdict.** Every row carries three clocks:

| Column | Meaning |
|---|---|
| `t_true_utc` | Ground truth. Use it for labels and joins |
| `t_rtc` | What the display's clock said. Includes per-unit offset, drift of ±10 ppm, the 32 kHz crystal temperature curve (−0.034 ppm/°C² × (T − 25)²), resets to 2000-01-01 when backup runs out, and syncs from the phone app (or from J1939 TD if enabled) |
| `uptime_s`, `boot_id` | Monotonic time since this power-up; resets at every key-on |

The simulation starts after **150 days of winter storage**. Its findings:
- **Coin-cell clocks** come out of storage about **4–5 minutes off**: the crystal runs slow in the cold.
- **Supercapacitor clocks** reset to the year 2000.
- **Units without an RTC** reset at every power-up until someone syncs them.

Firmware rules this suggests for the maxAI 430:
1. Log both RTC time and uptime.
2. Flag records written before the first sync.
3. Sync from J1939 TD or the phone whenever available.
4. Never use RTC time alone to order events across a power cycle.

## Debate 6: sustainability

- **Data governance lead:** "A dataset nobody can regenerate, explain or update dies after one project."

**Verdict.**

- **Deterministic:** every machine, day and fault schedule is seeded from a hash of the seed and its identifiers, so the same config gives byte-identical output (checked).
- **Versioned:** `metadata.json` records the generator version, the full config and a config hash. Bump `GENERATOR_VERSION` when the physics changes.
- **Validated on every run** (`validate.py`):
  - ranges and physics plausibility
  - fuel mass balance per session within 5 %
  - every CAN frame decoded and compared with the 1 Hz table
  - all CAN logs replayed through the **real firmware J1939 stack** (`tests/replay_candump`), with values compared against the generator's table
- **Small and regenerable:** commit the code and config, not the data. The default run (3 machines, 21 days, about 1 million one-second rows, 310 sessions) takes under a minute and about 50 MB.
- **Calibrate with reality:** when real J1939 logs arrive, fit parameters such as cooling capacity, fuel consumption and the sender noise distribution, then compare distributions signal by signal. Record the fitted parameters as a new config, not as code changes.
- **Open by default (proposal):** publish the generator and a reference dataset under a permissive licence (for example Apache-2.0 for code and CC BY 4.0 for data) as an open mower J1939 benchmark. Partners, universities and customers can then build on it, and real-data calibrations can be contributed back.

## Limits (be honest about these)

- This is synthetic data. Models trained on it must be validated on real machines before any claim is made.
- The blade and deck PGN layout is a placeholder until an OEM supplies its DBC.
- The engine profile is a generic 18.6 kW diesel. Gasoline and electric (BMS) profiles are future work.
- The schedule uses a fixed UTC offset (no DST change within the default season).
- Signals that small engines often lack (intake manifold pressure, exhaust temperature) are deliberately left out.
