#!/usr/bin/env python3
"""Validate a generated dataset.

1. Range and physics checks on signals_1hz.csv.gz (J1939 ranges, fuel mass balance,
   clock behaviour, label coverage).
2. Frame-level check: decode every candump frame with j1939_codec and compare it
   with the 1 Hz signal table.
3. Firmware check (optional, --firmware): replay each candump log through the real
   firmware J1939 stack (tests/replay_candump) and compare what the display would
   show against the generator's values.

Exit code 0 when all checks pass.
"""
import argparse
import json
import os
import subprocess
import sys

import pandas as pd

import j1939_codec as jc

HERE = os.path.dirname(os.path.abspath(__file__))
FW_TESTS = os.path.normpath(os.path.join(HERE, "..", "..", "tests"))

TOL = {  # absolute tolerance after J1939 quantisation and sub-second interpolation
    "coolant_c": 1.01, "oil_press_kpa": 4.01, "fuel_rate_lph": 0.051, "fuel_level_pct": 0.41,
    "battery_v": 0.051, "hyd_oil_c": 1.01, "load_pct": 1.01,
}

failures = []


def check(cond, msg):
    print(("  PASS  " if cond else "  FAIL  ") + msg)
    if not cond:
        failures.append(msg)


def physics(d, labels):
    print("Range and physics checks")
    run = d[d.phase != "crank"]
    check(run.rpm.between(0, 8031.875).all(), "engine speed inside the SPN 190 range")
    check(d.coolant_c.between(-40, 210).all(), "coolant inside the SPN 110 range")
    mow = d[d.phase == "mowing"]
    check(2700 <= mow.rpm.median() <= 3100, f"mowing rpm median {mow.rpm.median():.0f} in 2700-3100")
    check(3.0 <= mow.fuel_rate_lph.mean() <= 7.0, f"mowing fuel rate mean {mow.fuel_rate_lph.mean():.2f} L/h in 3-7")
    warm = mow[mow.uptime_s > 1200]
    check(78 <= warm.coolant_c.median() <= 95, f"warm coolant median {warm.coolant_c.median():.0f} °C in 78-95")
    check(50 <= warm.hyd_oil_c.median() <= 85, f"warm hydraulic oil median {warm.hyd_oil_c.median():.0f} °C in 50-85")
    check(((d.phase == "mowing") == (d.blade_engaged == 1)).all(), "blades engaged exactly while mowing")

    # fuel mass balance per session: integrated fuel rate vs true tank drop (no refuel inside a session)
    errs = []
    for (_, _), s in d.groupby(["machine", "session"]):
        used = s.fuel_rate_lph.sum() / 3600
        drop = (s.gt_fuel_true_pct.iloc[0] - s.gt_fuel_true_pct.iloc[-1]) / 100 * 45
        if used > 0.3:
            errs.append(abs(used - drop) / used)
    check(max(errs) < 0.05, f"fuel mass balance within 5 % per session (worst {max(errs) * 100:.1f} %)")

    check((d.groupby(["machine", "session"]).uptime_s.min() == 0).all(), "uptime restarts at 0 every session")
    check(d.groupby("machine").boot_id.is_monotonic_increasing.all(), "boot id increases per machine")
    for t in ("rtc_reset", "rtc_sync", "blade_strike", "refuel"):
        check((labels.type == t).any(), f"labels include {t}")


def frames_vs_table(d, out):
    print("Frame-level checks (candump vs signal table)")
    meta = json.load(open(os.path.join(out, "metadata.json")))
    for c in meta["can_logs"]:
        s = d[(d.machine == c["machine"]) & (d.session == c["session"])].reset_index(drop=True)
        t0 = pd.Timestamp(s.t_true_utc.iloc[0]).timestamp()
        worst = {k: 0.0 for k in TOL}
        counts = {}
        for line in open(os.path.join(out, c["file"])):
            ts = float(line[1:line.index(")")])
            cid, data = line.split()[2].split("#")
            pgn, _, _, _ = jc.parse_id(int(cid, 16))
            counts[pgn] = counts.get(pgn, 0) + 1
            vals = jc.decode(pgn, [int(data[i:i + 2], 16) for i in range(0, len(data), 2)])
            i = int(ts - t0)
            if i >= len(s):
                continue
            for k, v in vals.items():
                if k in TOL:
                    ref = s.at[i, k]
                    if v is None or pd.isna(ref):
                        continue
                    worst[k] = max(worst[k], abs(v - ref))
        ok = all(worst[k] <= TOL[k] for k in TOL)
        check(ok, f"{c['file']}: decoded frames match the table " + ", ".join(f"{k}≤{worst[k]:.2f}" for k in TOL))
        eec1_hz = counts.get(jc.PGN_EEC1, 0) / max(1, len(s[: int(meta['config']['can_log']['max_minutes'] * 60)]))
        check(40 <= eec1_hz <= 55, f"{c['file']}: EEC1 at {eec1_hz:.1f} Hz (20 ms nominal)")


def firmware(d, out):
    print("Firmware replay (firmware J1939 stack decodes the synthetic bus)")
    exe = os.path.join(FW_TESTS, "replay_candump")
    r = subprocess.run(["make", "-s", "-C", FW_TESTS, "replay_candump"], capture_output=True, text=True)
    if r.returncode != 0:
        check(False, "build replay_candump: " + r.stderr.strip()[-300:])
        return
    meta = json.load(open(os.path.join(out, "metadata.json")))
    for c in meta["can_logs"]:
        s = d[(d.machine == c["machine"]) & (d.session == c["session"])].reset_index(drop=True)
        p = subprocess.run([exe, os.path.join(out, c["file"])], capture_output=True, text=True)
        rows = [json.loads(x) for x in p.stdout.splitlines()]
        worst, compared, stale = {k: 0.0 for k in ("coolant_c", "oil_press_kpa", "fuel_level_pct", "hyd_oil_c", "battery_v")}, 0, 0
        hours_seen = False
        for r_ in rows[5:]:
            i = int(r_["t"]) - 1
            if i < 2 or i >= len(s):
                continue
            compared += 1
            for k in worst:
                a = r_[k]
                # 1 s messages have a random phase: the latest frame may come from the neighbouring row
                refs = [s.at[j, k] for j in (i - 1, i, i + 1) if 0 <= j < len(s) and not pd.isna(s.at[j, k])]
                if a is None or not refs:
                    continue
                worst[k] = max(worst[k], min(abs(a - b) for b in refs))
            hours_seen |= r_["engine_hours"] is not None
        tol = {"coolant_c": 1.01, "oil_press_kpa": 8.01, "fuel_level_pct": 0.81, "hyd_oil_c": 1.01, "battery_v": 0.11}
        ok = p.returncode == 0 and compared > 60 and all(worst[k] <= tol[k] for k in worst)
        check(ok, f"{c['file']}: firmware decode matches over {compared} s; " + ", ".join(f"{k}≤{v:.2f}" for k, v in worst.items()))
        check(hours_seen, f"{c['file']}: engine hours received via request/response (PGN 65253)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(HERE, "out"))
    ap.add_argument("--firmware", action="store_true", help="also replay CAN logs through the firmware J1939 stack")
    a = ap.parse_args()
    d = pd.read_csv(os.path.join(a.out, "signals_1hz.csv.gz"), dtype={"dtcs": str})
    labels = pd.read_csv(os.path.join(a.out, "labels.csv"))
    physics(d, labels)
    frames_vs_table(d, a.out)
    if a.firmware:
        firmware(d, a.out)
    print(f"\n{'ALL CHECKS PASSED' if not failures else str(len(failures)) + ' CHECK(S) FAILED'}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
