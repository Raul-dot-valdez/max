#!/usr/bin/env python3
"""Synthetic J1939 dataset for a commercial zero-turn mower.

Physics-informed, stochastic, deterministic per seed. Produces:
  signals_1hz.csv.gz   decoded signals as the display sees them, plus ground-truth (gt_*) columns
  labels.csv           fault and event intervals with onset/end in true UTC
  sessions.csv         one row per key-on session
  can/*.log            raw J1939 frames (Linux candump format) for sample sessions
  windows.npz          60 s ML windows + labels (int8 calibration / training, e.g. GD32 Embedded AI)
  metadata.json        config, generator version, signal dictionary

Usage:  python3 synth.py [--config config_default.json] [--out out_dir] [--days N] [--machines N] [--seed S]
"""
import argparse
import datetime as dt
import gzip
import hashlib
import json
import math
import os
import random
import sys

import numpy as np

import j1939_codec as jc

GENERATOR_VERSION = "0.2.0"
HERE = os.path.dirname(os.path.abspath(__file__))
UTC = dt.timezone.utc

# Monthly mean daily-high temperature (°C) for the default climate profile.
CLIMATES = {
    "temperate_continental": {4: 11, 5: 19, 6: 24, 7: 27, 8: 25, 9: 20, 10: 12, 11: 5, 3: 3},
    "hot_dry": {4: 26, 5: 31, 6: 36, 7: 39, 8: 38, 9: 34, 10: 28, 11: 21, 3: 22},
    "maritime": {4: 13, 5: 16, 6: 19, 7: 22, 8: 22, 9: 19, 10: 15, 11: 11, 3: 10},
}

FEATURES = ["rpm", "load_pct", "coolant_c", "oil_press_kpa", "fuel_rate_lph",
            "hyd_oil_c", "ambient_c", "wheel_speed_kmh", "blade_engaged"]
CLASSES = ["normal", "screen_clog", "low_oil", "hyd_overheat", "dull_blades"]

# Engine families (firmware architecture 2, docs/fleet-expansion/fleet_profiles.json). They change
# what the display can see, not the machine physics:
#   liquid_j1939  liquid-cooled engine with a J1939 ECU (phase 1 default)
#   air_efi       air-cooled EFI with a J1939 ECU (Kawasaki FX EFI, Vanguard EFI): no coolant SPN 110;
#                 the engine's thermal signal is oil temperature SPN 175, which runs hotter
#   carb          carbureted, no ECU, no J1939: rpm from ignition pulses, fuel sender, battery,
#                 PTO switch and a low-oil-pressure switch, read by the display's own inputs
ENGINE_FAMILIES = ("liquid_j1939", "air_efi", "carb")
AIR_COOLED_OFFSET_C = 25.0     # air-cooled oil runs about this much above the liquid-cooled coolant state
OIL_SWITCH_KPA = 70.0          # low-oil-pressure switch closes below this
CARB_HIDDEN = ("load_pct", "coolant_c", "oil_temp_c", "engine_temp_c", "oil_press_kpa", "fuel_rate_lph",
               "hyd_oil_c", "ambient_c", "wheel_speed_kmh")


def clamp(x, lo, hi):
    return lo if x < lo else hi if x > hi else x


def seeded(*parts):
    h = hashlib.sha256("|".join(str(p) for p in parts).encode()).hexdigest()
    return random.Random(int(h[:16], 16))


# ---------------------------------------------------------------------------
# Machine and real-time clock models
# ---------------------------------------------------------------------------

class RTC:
    """M1 display real-time clock as the device would record it.

    mode: 'battery' (coin cell, survives storage), 'supercap' (survives backup_days),
          'none' (no RTC: clock restarts at 2000-01-01 on every power-up until synced).
    Drift: per-unit offset in ppm plus the tuning-fork crystal curve -0.034 ppm/°C² × (T-25)².
    """
    EPOCH_RESET = dt.datetime(2000, 1, 1, tzinfo=UTC)

    def __init__(self, rng, mode, backup_days):
        self.mode = mode
        self.backup_s = backup_days * 86400
        self.base_ppm = rng.gauss(0, 10)
        self.offset = rng.uniform(-3, 3)  # seconds, device clock minus true time
        self.valid = mode != "none"

    def ppm(self, temp_c):
        return self.base_ppm - 0.034 * (temp_c - 25.0) ** 2

    def advance(self, seconds, temp_c):
        self.offset += seconds * self.ppm(temp_c) * 1e-6

    def power_gap(self, gap_s, true_time_at_power_up):
        """Called at power-up after an off period. Returns an event string or None."""
        if self.mode == "none" or (self.mode == "supercap" and gap_s > self.backup_s):
            self.offset = (self.EPOCH_RESET - true_time_at_power_up).total_seconds()
            self.valid = False
            return "rtc_reset"
        return None

    def sync(self, rng):
        self.offset = rng.uniform(-0.5, 0.5)
        self.valid = True

    def read(self, true_time):
        return true_time + dt.timedelta(seconds=self.offset)


class Machine:
    def __init__(self, cfg, idx):
        self.id = f"MWR-{idx + 1:03d}"
        r = seeded(cfg["seed"], "machine", idx)
        self.rng = r
        p = cfg["machine"]
        self.tank_l = p["tank_l"]
        self.rated_kw = p["rated_kw"] * r.uniform(0.97, 1.03)
        self.idle_rpm = p["idle_rpm"] + r.uniform(-30, 30)
        self.work_rpm = p["work_rpm"] + r.uniform(-40, 40)
        self.deck_mm = r.choice([64, 76, 89, 102])
        self.sender_bias = r.uniform(-1.2, 1.2)
        mode = cfg["rtc"]["modes"][idx % len(cfg["rtc"]["modes"])]
        self.rtc = RTC(r, mode, cfg["rtc"]["supercap_backup_days"])
        fams = cfg.get("engine_families", ["liquid_j1939"])
        self.family = fams[idx % len(fams)]
        if self.family not in ENGINE_FAMILIES:
            raise ValueError(f"unknown engine family {self.family}")
        # persistent state
        self.coolant = self.oil_t = self.hyd = 10.0
        self.fuel_l = self.tank_l * r.uniform(0.6, 0.95)
        self.hours = r.uniform(120, 1800)
        self.clog = 0.05
        self.dull = r.uniform(0.0, 0.3)
        self.air = r.uniform(0.0, 0.3)
        self.oil_loss = 0.0
        self.oil_leak_active = False
        self.sender_fault = False
        self.repair_pending = False
        self.hyd_fan_fault = False
        self.low_oil_s = 0
        self.last_off = None
        self.boot_id = 0
        self.dtc_oc = {}
        self.last_amb = 10.0


# ---------------------------------------------------------------------------
# Weather and plan
# ---------------------------------------------------------------------------

def ambient_at(cfg, local_t, day_rng_offset):
    table = CLIMATES[cfg["climate"]]
    high = table.get(local_t.month, 15) + day_rng_offset
    h = local_t.hour + local_t.minute / 60
    # daily minimum ~05:00, maximum ~15:00
    return high - 7.0 + 7.0 * math.sin((h - 9.0) / 24 * 2 * math.pi) * 1.0 + (3.0 if 11 <= h <= 17 else 0)


def plan_day(cfg, m, day, rng):
    """List of sessions (one per job site). Each: dict with start (local), phases."""
    wk = cfg["work_days"]
    if day.weekday() not in wk or rng.random() < cfg["rain_day_prob"]:
        return []
    t = dt.datetime.combine(day, dt.time(7, 0)) + dt.timedelta(minutes=rng.gauss(15, 15))
    end = dt.datetime.combine(day, dt.time(16, 0)) + dt.timedelta(minutes=rng.gauss(0, 30))
    sessions = []
    lunch_done = False
    while t < end - dt.timedelta(minutes=40):
        site = {
            "grass": clamp(rng.gauss(0.85, 0.25), 0.3, 1.4),
            "slope": clamp(abs(rng.gauss(4, 4)), 0, 18),
            "stones": rng.choice([0.0, 0.1, 0.3, 0.8]),
            "pass_m": rng.uniform(30, 120),
            "dry": rng.random() < 0.6,
        }
        mow_min = clamp(rng.gauss(45, 20), 12, 110)
        phases = [("crank", 3), ("warm_idle", int(rng.uniform(40, 150))),
                  ("transport", int(rng.uniform(90, 300))), ("mowing", int(mow_min * 60)),
                  ("transport", int(rng.uniform(60, 200))), ("idle", int(rng.uniform(20, 60)))]
        dur = sum(p[1] for p in phases)
        sessions.append({"start": t, "site": site, "phases": phases})
        t += dt.timedelta(seconds=dur) + dt.timedelta(minutes=rng.uniform(12, 35))  # trailer to next site
        if not lunch_done and t.hour >= 11:
            sessions[-1]["lunch_after"] = True
            t += dt.timedelta(minutes=rng.uniform(30, 50))
            lunch_done = True
    return sessions


# ---------------------------------------------------------------------------
# Simulation
# ---------------------------------------------------------------------------

def fault_schedule(cfg):
    """Discrete faults per machine. Each machine draws independently; if no machine drew a
    fault type, one machine is forced to have it, so every dataset covers every fault type."""
    fz = cfg["faults"]
    probs = {"oil_leak": fz["oil_leak_machine_prob"], "sender_fault": fz["sender_fault_machine_prob"],
             "hyd_fan_fault": fz["hyd_fan_fault_machine_prob"]}
    sched = {}
    for ftype, p in probs.items():
        fr = seeded(cfg["seed"], "faults", ftype)
        sched[ftype] = {mi: fr.randrange(2, cfg["days"]) for mi in range(cfg["machines"]) if fr.random() < p}
        if not sched[ftype] and cfg["faults"].get("guarantee_coverage", True):
            sched[ftype][fr.randrange(cfg["machines"])] = fr.randrange(2, cfg["days"])
    return sched


def simulate(cfg, out):
    rows, labels, sess_rows = [], [], []
    can_targets = []
    tz = dt.timedelta(hours=cfg["utc_offset_hours"])
    start = dt.date.fromisoformat(cfg["start_date"])
    faults = cfg["faults"]

    schedule = fault_schedule(cfg)
    for mi in range(cfg["machines"]):
        m = Machine(cfg, mi)
        storage_gap = cfg["rtc"]["initial_storage_days"] * 86400
        m.last_off = dt.datetime.combine(start, dt.time(0, 0), tzinfo=UTC) - dt.timedelta(seconds=storage_gap)
        session_no = 0
        open_labels = {}

        oil_leak_day = schedule["oil_leak"].get(mi)
        sender_fault_day = schedule["sender_fault"].get(mi)
        fan_fault_day = schedule["hyd_fan_fault"].get(mi)

        for d in range(cfg["days"]):
            day = start + dt.timedelta(days=d)
            dr = seeded(cfg["seed"], "day", mi, d)
            day_temp = dr.gauss(0, 3) + (cfg["heat_wave_extra_c"] if dr.random() < cfg["heat_wave_day_prob"] else 0.0)
            sessions = plan_day(cfg, m, day, dr)

            if oil_leak_day is not None and d == oil_leak_day:
                m.oil_leak_active = True
                labels.append(dict(machine=m.id, type="oil_leak", start=_iso(dt.datetime.combine(day, dt.time(7, 0), tzinfo=UTC) - tz), end=None, severity="high",
                                   note="engine oil loss ramps over following running hours"))
                open_labels["oil_leak"] = labels[-1]
            if sender_fault_day is not None and d in (sender_fault_day, sender_fault_day + 1):
                if d == sender_fault_day:
                    labels.append(dict(machine=m.id, type="fuel_sender_fault", start=_iso(dt.datetime.combine(day, dt.time(6, 0), tzinfo=UTC) - tz),
                                       end=_iso(dt.datetime.combine(day + dt.timedelta(days=1), dt.time(23, 59), tzinfo=UTC) - tz),
                                       severity="medium", note="intermittent fuel level sender: DD1 sends 'not available', DM1 SPN 96 FMI 2"))
                m.sender_fault = True
            elif m.sender_fault:
                m.sender_fault = False

            m.hyd_fan_fault = fan_fault_day is not None and d == fan_fault_day
            if m.hyd_fan_fault:
                labels.append(dict(machine=m.id, type="hyd_cooler_fan_fault", start=_iso(dt.datetime.combine(day, dt.time(6, 0), tzinfo=UTC) - tz),
                                   end=_iso(dt.datetime.combine(day, dt.time(23, 59), tzinfo=UTC) - tz), severity="high", note="hydraulic oil cooler fan inoperative for the day"))
            # maintenance at start of day
            if m.repair_pending and m.oil_leak_active:
                t_fix = dt.datetime.combine(day, dt.time(6, 45), tzinfo=UTC) - tz
                if "oil_leak" in open_labels:
                    open_labels.pop("oil_leak")["end"] = _iso(t_fix)
                labels.append(dict(machine=m.id, type="oil_leak_repaired", start=_iso(t_fix), end=None, severity="info", note="dealer repair after low-oil-pressure DTC"))
                m.oil_leak_active, m.oil_loss, m.repair_pending, m.low_oil_s = False, 0.0, False, 0
            if m.dull > 0.45 and dr.random() < faults["sharpen_prob"]:
                labels.append(dict(machine=m.id, type="blades_sharpened", start=_iso(dt.datetime.combine(day, dt.time(6, 30), tzinfo=UTC) - tz), end=None, severity="info", note=""))
                m.dull = dr.uniform(0.0, 0.05)
            if m.air > 0.6:
                labels.append(dict(machine=m.id, type="air_filter_replaced", start=_iso(dt.datetime.combine(day, dt.time(6, 30), tzinfo=UTC) - tz), end=None, severity="info", note=""))
                m.air = 0.0

            for si, s in enumerate(sessions):
                session_no += 1
                t_true = (s["start"].replace(tzinfo=UTC) - tz)  # local -> UTC
                gap = (t_true - m.last_off).total_seconds()

                # --- off-period physics -----------------------------------------
                amb_off = ambient_at(cfg, s["start"], day_temp) - 3
                for attr, tau in (("coolant", 2400), ("oil_t", 3000), ("hyd", 3600)):
                    v = getattr(m, attr)
                    setattr(m, attr, amb_off + (v - amb_off) * math.exp(-gap / tau))
                m.rtc.advance(gap, amb_off)

                # overnight fuel theft (first session of the day)
                if si == 0 and gap > 6 * 3600 and dr.random() < faults["theft_per_night_prob"]:
                    before = m.fuel_l / m.tank_l * 100
                    m.fuel_l *= dr.uniform(0.55, 0.85)
                    labels.append(dict(machine=m.id, type="fuel_theft", start=_iso(m.last_off), end=_iso(t_true),
                                       severity="high", note=f"level {before:.0f}% -> {m.fuel_l / m.tank_l * 100:.0f}%"))
                # refuel when low
                if m.fuel_l / m.tank_l < 0.25 and dr.random() < 0.9:
                    labels.append(dict(machine=m.id, type="refuel", start=_iso(t_true), end=None, severity="info",
                                       note=f"{m.fuel_l / m.tank_l * 100:.0f}% -> full"))
                    m.fuel_l = m.tank_l * dr.uniform(0.92, 1.0)

                # --- power-up: RTC -----------------------------------------------
                m.boot_id += 1
                ev = m.rtc.power_gap(gap, t_true)
                if ev:
                    labels.append(dict(machine=m.id, type=ev, start=_iso(t_true), end=None, severity="medium",
                                       note=f"RTC mode {m.rtc.mode}; off for {gap / 86400:.1f} days"))
                synced = False
                if cfg["rtc"]["td_source_on_bus"] or dr.random() < cfg["rtc"]["phone_sync_prob_per_session"]:
                    m.rtc.sync(dr)
                    synced = True
                    labels.append(dict(machine=m.id, type="rtc_sync", start=_iso(t_true), end=None, severity="info",
                                       note="J1939 TD (PGN 65254)" if cfg["rtc"]["td_source_on_bus"] else "phone app over BLE"))

                srows = run_session(cfg, m, s, t_true, dr, labels, session_no)
                rows.extend(srows)
                t_end = t_true + dt.timedelta(seconds=len(srows))
                m.last_off = t_end
                if s.get("lunch_after") and dr.random() < faults["screen_clean_prob_lunch"]:
                    m.clog = 0.05
                    labels.append(dict(machine=m.id, type="screen_cleaned", start=_iso(t_end), end=None, severity="info", note="operator cleaned radiator screen"))
                sess_rows.append(dict(machine=m.id, session=session_no, boot_id=m.boot_id,
                                      start_utc=_iso(t_true), end_utc=_iso(t_end), duration_s=len(srows),
                                      rtc_mode=m.rtc.mode, rtc_synced=synced, engine_family=m.family,
                                      rtc_error_s_at_start=round(srows[0]["t_rtc_error_s"], 3) if srows else None,
                                      site_grass=round(s["site"]["grass"], 2), site_slope_deg=round(s["site"]["slope"], 1)))
                if m.family != "carb" and srows and sum(1 for c in can_targets if c[0] is m) < cfg["can_log"]["sessions_per_machine"]:
                    can_targets.append((m, srows, session_no))

            if dr.random() < faults["screen_clean_prob_end_of_day"]:
                m.clog = 0.05

        if "oil_leak" in open_labels:
            open_labels["oil_leak"]["end"] = _iso(m.last_off)

    return rows, labels, sess_rows, can_targets


def apply_family(r, m, oil_p_true, rng):
    """Turn the physics row into what a display on this engine family can see."""
    r["engine_family"] = m.family
    r["_hyd_true"] = r["hyd_oil_c"]  # labels use the truth, whatever the display can see
    r["engine_temp_c"] = r["oil_temp_c"]
    r["oil_low_sw"] = None
    if m.family == "air_efi":
        r["engine_temp_c"] = r["oil_temp_c"] = round(m.coolant + AIR_COOLED_OFFSET_C, 1)
        r["coolant_c"] = None
    elif m.family == "carb":
        r["oil_low_sw"] = int(oil_p_true < OIL_SWITCH_KPA and r["phase"] != "crank")
        r["rpm"] = round(r["rpm"] + rng.gauss(0, 15), 1)  # pulse-count quantisation and jitter
        for k in CARB_HIDDEN:
            r[k] = None


def _iso(t):
    return None if t is None else t.astimezone(UTC).isoformat(timespec="seconds").replace("+00:00", "Z")


def run_session(cfg, m, s, t0, rng, labels, session_no):
    site = s["site"]
    fz = cfg["faults"]
    rows = []
    speed = 0.0
    pass_left = 0.0
    turning = 0
    slope_sign = 1
    t = t0
    strike_label_open = False
    amb_day = rng.gauss(0, 2)
    total = sum(p[1] for p in s["phases"])
    phase_idx, phase_left = 0, s["phases"][0][1]
    uptime = 0
    for _ in range(total):
        phase = s["phases"][phase_idx][0]
        local = t + dt.timedelta(hours=cfg["utc_offset_hours"])
        amb = ambient_at(cfg, local, amb_day)
        m.last_amb = amb
        dew = 1.25 if local.hour < 9 else 1.0

        # --- operator / duty cycle -------------------------------------------
        blade = phase == "mowing"
        if phase == "mowing":
            if turning > 0:
                target = 3.0
                turning -= 1
                if turning == 0:
                    pass_left = site["pass_m"] * rng.uniform(0.85, 1.15)
                    slope_sign = -slope_sign
            else:
                target = clamp(rng.gauss(11.5, 0.6) - 0.15 * site["slope"], 6, 14)
                pass_left -= speed / 3.6
                if pass_left <= 0:
                    turning = rng.randint(4, 7)
        elif phase == "transport":
            target = clamp(rng.gauss(14.5, 0.8), 10, 17)
        else:
            target = 0.0
        speed += (target - speed) * 0.45
        speed = max(0.0, speed)
        slope = site["slope"] * slope_sign * (0.6 + 0.4 * rng.random()) if phase in ("mowing", "transport") else 0.0

        # --- engine ----------------------------------------------------------------
        cranking = phase == "crank"
        rpm_set = m.work_rpm if phase in ("mowing", "transport") else m.idle_rpm
        grass = site["grass"] * dew * clamp(rng.gauss(1, 0.12), 0.6, 1.5)
        p_blade = 6.8 * grass * (1 + 0.35 * m.dull) * (0.6 + 0.4 * speed / 11.5) if blade else 0.0
        p_drive = 0.22 * speed * (1 + 0.07 * slope) + (0.35 if speed > 0.5 else 0.0)
        p_aux = 0.6
        p_dem = max(0.3, p_blade + p_drive + p_aux)
        derate = 0.7 if m.coolant > 108 else 1.0
        pmax = m.rated_kw * (rpm_set / 3000.0) * derate
        load = p_dem / pmax
        if cranking:
            rpm = rng.uniform(180, 320)
            load = 0.0
        elif load > 1.0:
            rpm = rpm_set * (1 - 0.06 - 0.25 * (load - 1)) + rng.gauss(0, 12)
            load = 1.0
        else:
            rpm = rpm_set * (1 - 0.03 * load) + rng.gauss(0, 8)
        strike = False
        if blade and rng.random() < site["stones"] * fz["strike_rate_per_mow_hour"] / 3600:
            strike = True
            rpm *= rng.uniform(0.72, 0.84)
            labels.append(dict(machine=m.id, type="blade_strike", start=_iso(t), end=_iso(t + dt.timedelta(seconds=1)),
                               severity="medium", note=f"session {session_no}"))
        p_act = 0.0 if cranking else load * pmax
        fuel = 0.0 if cranking else (p_act * 0.255 / 0.84) * (1 + 0.07 * m.air) + 0.45 * rpm / 1400
        fuel *= rng.gauss(1, 0.02)

        # --- thermal --------------------------------------------------------------
        th = clamp((m.coolant - 80) / 8, 0, 1)
        q_in = 0.8 * p_act + (0.0 if cranking else 1.2)
        q_out = 0.237 * (m.coolant - amb) * (0.04 + 0.96 * th) * (rpm / 3000) * (1 - 0.75 * m.clog)
        m.coolant += (q_in - q_out) / 60.0
        m.oil_t += ((m.coolant + 6 + 8 * load) - m.oil_t) / 240
        hyd_target = amb + 25 + 6.0 * p_drive + 0.8 * abs(slope) + (38.0 if m.hyd_fan_fault else 0.0)
        m.hyd += (hyd_target - m.hyd) / 600

        visc = clamp(1.7 - 0.009 * (m.oil_t - 20), 0.75, 1.7)
        oil_p = 0.0 if cranking else min(420.0, 60 + 0.075 * rpm * visc) * (1 - m.oil_loss) + rng.gauss(0, 4)  # relief valve ~420 kPa
        batt = rng.uniform(10.2, 10.8) if cranking else 14.05 + rng.gauss(0, 0.04)

        # --- fuel tank and sender -----------------------------------------------------
        m.fuel_l = max(0.0, m.fuel_l - fuel / 3600)
        slosh = (abs(slope) / 10) * 2.5 * math.sin(uptime / 3.0) + (1.5 * rng.gauss(0, 1) if turning else 0.0)
        sender = clamp(m.fuel_l / m.tank_l * 100 + m.sender_bias + slosh + rng.gauss(0, 0.6), 0, 100)
        if m.sender_fault and rng.random() < 0.25:
            sender = None

        # --- slow degradation -------------------------------------------------------------
        if not cranking:
            m.hours += 1 / 3600
        if blade:
            m.clog = clamp(m.clog + fz["clog_rate_per_mow_hour"] * grass * (1.0 if site["dry"] else 0.35) / 3600, 0, 1)
            m.dull = clamp(m.dull + fz["dull_rate_per_mow_hour"] * (1 + 3 * site["stones"]) / 3600, 0, 1)
            m.air = clamp(m.air + fz["air_rate_per_mow_hour"] / 3600, 0, 1)
        if m.oil_leak_active and not cranking:
            m.oil_loss = clamp(m.oil_loss + fz["oil_leak_rate_per_run_hour"] / 3600, 0, 0.75)

        # --- diagnostics (DM1) ------------------------------------------------------------
        dtcs = []
        temp_spn = 110 if m.family == "liquid_j1939" else 175  # air-cooled ECUs flag oil temperature
        if m.coolant > 112:
            dtcs.append((temp_spn, 0))
        elif m.coolant > 105:
            dtcs.append((temp_spn, 16))
        if oil_p < 70 and rpm > 2000:
            dtcs.append((100, 18))
            m.low_oil_s += 1
            if m.low_oil_s > 180:  # operator reports the red lamp; machine goes to the dealer
                m.repair_pending = True
        if m.hyd > 95:
            dtcs.append((1638, 16))
        if sender is None:
            dtcs.append((96, 2))
        if m.family == "carb":
            dtcs = []  # no ECU, nothing on a bus; low_oil_s above still drives the dealer visit
        dtc_full = []
        for spn, fmi in dtcs:
            key = (spn, fmi)
            m.dtc_oc[key] = m.dtc_oc.get(key, 0) + (1 if not rows or (spn, fmi) not in rows[-1]["_dtcs"] else 0)
            dtc_full.append((spn, fmi, min(126, m.dtc_oc[key])))
        red = any(f in (0, 18) for _, f, _ in dtc_full)
        amber = bool(dtc_full) and not red

        # --- RTC ----------------------------------------------------------------------------
        m.rtc.advance(1.0, amb + 8)  # cab is warmer than ambient when running
        t_rtc = m.rtc.read(t)

        rows.append({
            "t_true_utc": _iso(t), "t_rtc": t_rtc.isoformat(timespec="seconds").replace("+00:00", "Z"),
            "t_rtc_error_s": m.rtc.offset, "rtc_valid": int(m.rtc.valid), "uptime_s": uptime, "boot_id": m.boot_id,
            "machine": m.id, "session": session_no, "phase": phase,
            "rpm": round(rpm, 1), "load_pct": round(load * 100), "coolant_c": round(m.coolant),
            "oil_temp_c": round(m.oil_t, 1), "oil_press_kpa": max(0, round(oil_p / 4) * 4),
            "fuel_rate_lph": round(fuel / 0.05) * 0.05, "fuel_level_pct": None if sender is None else round(sender / 0.4) * 0.4,
            "battery_v": round(batt / 0.05) * 0.05, "wheel_speed_kmh": round(speed, 2), "hyd_oil_c": round(m.hyd),
            "ambient_c": round(amb, 1), "blade_engaged": int(blade), "deck_height_mm": m.deck_mm,
            "engine_hours": round(m.hours / 0.05) * 0.05, "dtc_count": len(dtc_full),
            "dtcs": ";".join(f"{a}/{b}/{c}" for a, b, c in dtc_full), "lamp_amber": int(amber), "lamp_red": int(red),
            # ground truth (not on the bus)
            "gt_grass_load": round(grass, 3), "gt_slope_deg": round(slope, 1), "gt_screen_clog": round(m.clog, 3),
            "gt_blade_dullness": round(m.dull, 3), "gt_air_filter": round(m.air, 3), "gt_oil_loss": round(m.oil_loss, 3),
            "gt_fuel_true_pct": round(m.fuel_l / m.tank_l * 100, 2), "gt_blade_strike": int(strike),
            "_dtcs": {(a, b) for a, b, _ in dtc_full}, "_dtc_full": dtc_full, "_amber": amber, "_red": red,
            "_pedal": 100.0 if rpm_set == m.work_rpm else 0.0,
        })
        apply_family(rows[-1], m, oil_p, rng)
        t += dt.timedelta(seconds=1)
        uptime += 1
        phase_left -= 1
        if phase_left <= 0 and phase_idx + 1 < len(s["phases"]):
            phase_idx += 1
            phase_left = s["phases"][phase_idx][1]
            if s["phases"][phase_idx][0] == "mowing":
                pass_left = site["pass_m"]
    return rows


# ---------------------------------------------------------------------------
# CAN log (candump) for sample sessions
# ---------------------------------------------------------------------------

NAME_ENGINE = 0x8000_0000_0000_0001
NAME_DISPLAY = 0x823C_0000_0000_0028
NAME_DECK = 0x8000_0000_0000_0090

PERIODS_MS = {  # J1939-71 typical broadcast rates
    jc.PGN_EEC1: 20, jc.PGN_EEC2: 50, jc.PGN_LFE1: 100, jc.PGN_CCVS1: 100,
    jc.PGN_EFLP1: 500, jc.PGN_ET1: 1000, jc.PGN_DD1: 1000, jc.PGN_VEP1: 1000,
    jc.PGN_AMB: 1000, jc.PGN_VF: 1000, jc.PGN_DM1: 1000, jc.PGN_OEM_DECK: 100,
}
PRIO = {jc.PGN_EEC1: 3, jc.PGN_EEC2: 3}


def write_candump(path, m, rows, cfg, rng):
    max_s = int(cfg["can_log"]["max_minutes"] * 60)
    rows = rows[:max_s]
    t0 = dt.datetime.fromisoformat(rows[0]["t_true_utc"].replace("Z", "+00:00")).timestamp()
    eec1_ms = cfg["can_log"]["eec1_period_ms"]
    periods = dict(PERIODS_MS)
    periods[jc.PGN_EEC1] = eec1_ms
    frames = []

    def add(ts, prio, pgn, sa, data, da=jc.GLOBAL):
        frames.append((ts, jc.can_id(prio, pgn, sa, da), data))

    # power-up address claims
    add(t0 + 0.010, 6, jc.PGN_ADDR_CLAIM, jc.SA_ENGINE, jc.addr_claim(NAME_ENGINE))
    add(t0 + 0.015, 6, jc.PGN_ADDR_CLAIM, jc.SA_DECK, jc.addr_claim(NAME_DECK))
    add(t0 + 0.300, 6, jc.PGN_ADDR_CLAIM, jc.SA_DISPLAY, jc.addr_claim(NAME_DISPLAY))

    n = len(rows)
    for pgn, per in periods.items():
        phase_off = rng.uniform(0, per / 1000)
        k = 0
        while True:
            ts_rel = 0.35 + phase_off + k * per / 1000 + rng.gauss(0, 0.0003)
            k += 1
            i = int(ts_rel)
            if i >= n:
                break
            r = rows[i]
            frac = ts_rel - i
            nxt = rows[min(i + 1, n - 1)]
            ts = t0 + ts_rel
            if pgn == jc.PGN_EEC1:
                rpm = r["rpm"] + (nxt["rpm"] - r["rpm"]) * frac + rng.gauss(0, 4)
                if r["gt_blade_strike"]:  # 300 ms dip, then recovery
                    rpm *= 1.0 - 0.2 * math.exp(-((frac - 0.3) / 0.12) ** 2)
                add(ts, 3, pgn, jc.SA_ENGINE, jc.eec1(rpm, r["load_pct"], min(100, r["load_pct"] + 5)))
            elif pgn == jc.PGN_EEC2:
                add(ts, 3, pgn, jc.SA_ENGINE, jc.eec2(r["_pedal"], r["load_pct"]))
            elif pgn == jc.PGN_LFE1:
                add(ts, 6, pgn, jc.SA_ENGINE, jc.lfe1(r["fuel_rate_lph"], r["_pedal"]))
            elif pgn == jc.PGN_CCVS1:
                v = r["wheel_speed_kmh"] + (nxt["wheel_speed_kmh"] - r["wheel_speed_kmh"]) * frac
                add(ts, 6, pgn, jc.SA_ENGINE, jc.ccvs1(v))
            elif pgn == jc.PGN_EFLP1:
                add(ts, 6, pgn, jc.SA_ENGINE, jc.eflp1(None, r["oil_press_kpa"]))
            elif pgn == jc.PGN_ET1:
                add(ts, 6, pgn, jc.SA_ENGINE, jc.et1(r["coolant_c"], r["ambient_c"] + 10, r["oil_temp_c"]))
            elif pgn == jc.PGN_DD1:
                add(ts, 6, pgn, jc.SA_ENGINE, jc.dd1(r["fuel_level_pct"]))
            elif pgn == jc.PGN_VEP1:
                add(ts, 6, pgn, jc.SA_ENGINE, jc.vep1(r["battery_v"], r["battery_v"], r["battery_v"] - 0.3))
            elif pgn == jc.PGN_AMB:
                add(ts, 6, pgn, jc.SA_ENGINE, jc.amb(101.0, r["ambient_c"], r["ambient_c"] + 6))
            elif pgn == jc.PGN_VF:
                add(ts, 6, pgn, jc.SA_ENGINE, jc.vf(r["hyd_oil_c"]))
            elif pgn == jc.PGN_OEM_DECK:
                add(ts, 6, pgn, jc.SA_DECK, jc.oem_deck(r["blade_engaged"], r["deck_height_mm"]))
            elif pgn == jc.PGN_DM1:
                payload = jc.dm1_payload([d for d in r["_dtc_full"]], r["_amber"], r["_red"])
                if len(payload) <= 8:
                    add(ts, 6, pgn, jc.SA_ENGINE, payload)
                else:
                    for j, (p, da, data) in enumerate(jc.bam_frames(pgn, payload)):
                        add(ts + j * 0.055, 7, p, jc.SA_ENGINE, data, da)

    # display requests engine hours every 10 s; engine answers ~8 ms later
    for k in range(1, n // 10):
        ts = t0 + k * 10 + 0.5
        r = rows[min(int(ts - t0), n - 1)]
        add(ts, 6, jc.PGN_REQUEST, jc.SA_DISPLAY, jc.request(jc.PGN_HOURS), da=jc.SA_ENGINE)
        add(ts + 0.008, 6, jc.PGN_HOURS, jc.SA_ENGINE, jc.hours(r["engine_hours"]))
    # optional time/date broadcast (RTC sync source)
    if cfg["rtc"]["td_source_on_bus"]:
        for k in range(n):
            tt = dt.datetime.fromtimestamp(t0 + k + 0.7, tz=UTC)
            add(t0 + k + 0.7, 6, jc.PGN_TD, 0x17, jc.td(tt))

    frames.sort(key=lambda f: f[0])
    with open(path, "w") as f:
        for ts, cid, data in frames:
            f.write(f"({ts:.6f}) can0 {cid:08X}#{''.join(f'{b:02X}' for b in data)}\n")
    return len(frames)


# ---------------------------------------------------------------------------
# ML windows
# ---------------------------------------------------------------------------

def window_class(r):
    if r["gt_oil_loss"] > 0.3:
        return 2
    if r["gt_screen_clog"] > 0.55 and r["blade_engaged"]:
        return 1
    if r.get("_hyd_true", r["hyd_oil_c"]) > 90:
        return 3
    if r["gt_blade_dullness"] > 0.5 and r["blade_engaged"]:
        return 4
    return 0


AUX = ["engine_temp_c", "oil_low_sw"]


def make_windows(rows, size=60, stride=30):
    """x: model features (missing -> 0.0, as in phase 1); mask: 1 where the family provides the
    feature; aux: extra signals the other AI heads use; family: engine family per window."""
    X, y, meta, mask, aux, fam = [], [], [], [], [], []
    start = 0
    while start < len(rows):
        end = start
        while end < len(rows) and rows[end]["session"] == rows[start]["session"] and rows[end]["machine"] == rows[start]["machine"]:
            end += 1
        sess = rows[start:end]
        for i in range(0, len(sess) - size + 1, stride):
            w = sess[i:i + size]
            if any(r["phase"] in ("crank",) for r in w):
                continue
            X.append([[float(r[f]) if r[f] is not None else 0.0 for f in FEATURES] for r in w])
            mask.append([int(any(r[f] is not None for r in w)) for f in FEATURES])
            aux.append([[float(r[a]) if r[a] is not None else float("nan") for a in AUX] for r in w])
            fam.append(w[0]["engine_family"])
            y.append(window_class(w[-1]))
            meta.append(f'{w[0]["machine"]}|{w[0]["session"]}|{w[0]["t_true_utc"]}')
        start = end
    return (np.array(X, dtype=np.float32), np.array(y, dtype=np.int8), np.array(meta),
            np.array(mask, dtype=np.int8), np.array(aux, dtype=np.float32), np.array(fam))


# ---------------------------------------------------------------------------

SIGNAL_DICTIONARY = [
    ("rpm", "EEC1", 61444, 190, "0.125 rpm/bit", "rpm"),
    ("load_pct", "EEC2", 61443, 92, "1 %/bit", "%"),
    ("coolant_c", "ET1", 65262, 110, "1 °C/bit, -40", "°C"),
    ("oil_temp_c", "ET1", 65262, 175, "0.03125 °C/bit, -273", "°C"),
    ("oil_press_kpa", "EFL/P1", 65263, 100, "4 kPa/bit", "kPa"),
    ("fuel_rate_lph", "LFE1", 65266, 183, "0.05 L/h per bit", "L/h"),
    ("fuel_level_pct", "DD1", 65276, 96, "0.4 %/bit", "%"),
    ("battery_v", "VEP1", 65271, 168, "0.05 V/bit", "V"),
    ("wheel_speed_kmh", "CCVS1", 65265, 84, "1/256 km/h per bit", "km/h"),
    ("hyd_oil_c", "VF", 65128, 1638, "1 °C/bit, -40", "°C"),
    ("ambient_c", "AMB", 65269, 171, "0.03125 °C/bit, -273", "°C"),
    ("engine_temp_c", "ET1", 65262, 175, "engine thermal signal: oil temp (same as oil_temp_c on ECU engines)", "°C"),
    ("oil_low_sw", "display input (no J1939)", None, None, "1 = low-oil-pressure switch closed, carb engines only", "0/1"),
    ("engine_family", "-", None, None, "liquid_j1939 | air_efi | carb", ""),
    ("engine_hours", "HOURS", 65253, 247, "0.05 h/bit (on request)", "h"),
    ("blade_engaged", "Proprietary B (placeholder)", 0xFF10, None, "byte 1 bits 0-1", "0/1"),
    ("deck_height_mm", "Proprietary B (placeholder)", 0xFF10, None, "byte 2, 2 mm/bit", "mm"),
    ("dtcs", "DM1", 65226, None, "SPN/FMI/OC; BAM when more than one", ""),
]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--config", default=os.path.join(HERE, "config_default.json"))
    ap.add_argument("--out", default=os.path.join(HERE, "out"))
    ap.add_argument("--days", type=int)
    ap.add_argument("--machines", type=int)
    ap.add_argument("--seed", type=int)
    a = ap.parse_args()
    with open(a.config) as f:
        cfg = json.load(f)
    for k in ("days", "machines", "seed"):
        if getattr(a, k) is not None:
            cfg[k] = getattr(a, k)
    os.makedirs(os.path.join(a.out, "can"), exist_ok=True)

    rows, labels, sessions, can_targets = simulate(cfg, a.out)
    if not rows:
        sys.exit("no sessions generated: check dates, work_days and rain_day_prob")

    pub = [{k: v for k, v in r.items() if not k.startswith("_")} for r in rows]
    cols = list(pub[0].keys())
    with gzip.open(os.path.join(a.out, "signals_1hz.csv.gz"), "wt") as f:
        f.write(",".join(cols) + "\n")
        for r in pub:
            f.write(",".join("" if r[c] is None else str(r[c]) for c in cols) + "\n")
    _write_csv(os.path.join(a.out, "labels.csv"), labels, ["machine", "type", "start", "end", "severity", "note"])
    _write_csv(os.path.join(a.out, "sessions.csv"), sessions, list(sessions[0].keys()))

    can_files = []
    for m, srows, sn in can_targets:
        p = os.path.join(a.out, "can", f"{m.id}_session{sn:04d}.log")
        nfr = write_candump(p, m, srows, cfg, seeded(cfg["seed"], "can", m.id, sn))
        can_files.append({"file": os.path.relpath(p, a.out), "frames": nfr, "machine": m.id, "session": sn})

    X, y, meta, mask, aux, fam = make_windows(rows)
    np.savez_compressed(os.path.join(a.out, "windows.npz"), x=X, y=y, meta=meta,
                        features=np.array(FEATURES), classes=np.array(CLASSES),
                        mask=mask, aux=aux, aux_features=np.array(AUX), family=fam)

    cfg_hash = hashlib.sha256(json.dumps(cfg, sort_keys=True).encode()).hexdigest()[:12]
    md = {
        "dataset": "Synthetic commercial zero-turn mower J1939 dataset",
        "generator_version": GENERATOR_VERSION, "config_hash": cfg_hash, "config": cfg,
        "rows": len(rows), "sessions": len(sessions), "labels": len(labels),
        "windows": {"count": int(len(y)), "shape": list(X.shape), "class_counts": {CLASSES[i]: int((y == i).sum()) for i in range(len(CLASSES))}},
        "can_logs": can_files,
        "signal_dictionary": [dict(column=c, message=msg, pgn=p, spn=s, scaling=sc, unit=u) for c, msg, p, s, sc, u in SIGNAL_DICTIONARY],
        "timestamps": {
            "t_true_utc": "ground-truth UTC (simulation clock); use for labels and joins",
            "t_rtc": "what the M1 display's real-time clock reported (drift, resets, syncs)",
            "uptime_s": "seconds since this power-up; resets at every key-on (boot_id increments)",
            "can logs": "candump timestamps are a reference logger's clock (true UTC)",
        },
        "ground_truth_columns": [c for c in cols if c.startswith("gt_")],
        "note": "Synthetic data. Proprietary B layout for blade and deck is a placeholder until the OEM defines it.",
    }
    with open(os.path.join(a.out, "metadata.json"), "w") as f:
        json.dump(md, f, indent=2)
    print(json.dumps({k: md[k] for k in ("rows", "sessions", "labels", "windows", "can_logs", "config_hash")}, indent=2))


def _write_csv(path, rows, cols):
    with open(path, "w") as f:
        f.write(",".join(cols) + "\n")
        for r in rows:
            f.write(",".join("" if r.get(c) is None else '"' + str(r.get(c)).replace('"', "'") + '"' if isinstance(r.get(c), str) and "," in str(r.get(c)) else ("" if r.get(c) is None else str(r.get(c))) for c in cols) + "\n")


if __name__ == "__main__":
    main()
