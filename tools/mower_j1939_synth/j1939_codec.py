"""J1939 encode/decode helpers for the synthetic mower dataset.

Byte positions and scaling follow SAE J1939-71 and match the firmware decoder in
middleware/j1939/j1939_signals.c, so generated frames can be replayed through it.
Bytes a message does not carry are sent as 0xFF ("not available").
"""
import math

NA8 = 0xFF
NA16 = 0xFFFF
NA32 = 0xFFFFFFFF

# PGNs
PGN_EEC2 = 61443
PGN_EEC1 = 61444
PGN_REQUEST = 59904
PGN_ADDR_CLAIM = 60928
PGN_TP_CM = 60416
PGN_TP_DT = 60160
PGN_DM1 = 65226
PGN_HOURS = 65253
PGN_TD = 65254
PGN_ET1 = 65262
PGN_EFLP1 = 65263
PGN_CCVS1 = 65265
PGN_LFE1 = 65266
PGN_AMB = 65269
PGN_VEP1 = 65271
PGN_DD1 = 65276
PGN_VF = 65128
PGN_OEM_DECK = 0xFF10  # Proprietary B placeholder: OEM must define the real layout

SA_ENGINE = 0x00
SA_DISPLAY = 0x28  # Cab Display #1
SA_DECK = 0x90     # placeholder address for the deck / blade controller
GLOBAL = 0xFF


def can_id(prio, pgn, sa, da=GLOBAL):
    pf = (pgn >> 8) & 0xFF
    if pf < 240:
        return ((prio & 7) << 26) | ((pgn & 0x3FF00) << 8) | (da << 8) | sa
    return ((prio & 7) << 26) | ((pgn & 0x3FFFF) << 8) | sa


def parse_id(cid):
    prio = (cid >> 26) & 7
    sa = cid & 0xFF
    pf = (cid >> 16) & 0xFF
    ps = (cid >> 8) & 0xFF
    dp = (cid >> 24) & 3
    if pf < 240:
        return (dp << 16) | (pf << 8), prio, sa, ps
    return (dp << 16) | (pf << 8) | ps, prio, sa, GLOBAL


def _raw(value, res, offset, maxraw, na):
    if value is None or (isinstance(value, float) and math.isnan(value)):
        return na
    r = int(round((value - offset) / res))
    return max(0, min(maxraw, r))


def u8(v, res=1.0, off=0.0):
    return _raw(v, res, off, 0xFA, NA8)


def u16(v, res=1.0, off=0.0):
    return _raw(v, res, off, 0xFAFF, NA16)


def u32(v, res=1.0, off=0.0):
    return _raw(v, res, off, 0xFAFFFFFF, NA32)


def le16(x):
    return [x & 0xFF, (x >> 8) & 0xFF]


def le32(x):
    return [x & 0xFF, (x >> 8) & 0xFF, (x >> 16) & 0xFF, (x >> 24) & 0xFF]


# --- encoders: each returns 8 data bytes -----------------------------------

def eec1(rpm, actual_torque_pct, demand_torque_pct):
    # SPN 512 driver demand torque (byte 2), SPN 513 actual torque (byte 3): 1 %/bit, -125 %
    # SPN 190 engine speed (bytes 4-5): 0.125 rpm/bit
    return [0xF0, u8(demand_torque_pct, 1, -125), u8(actual_torque_pct, 1, -125),
            *le16(u16(rpm, 0.125)), NA8, NA8, NA8]


def eec2(pedal_pct, load_pct):
    # SPN 91 accelerator pedal (byte 2): 0.4 %/bit; SPN 92 load at current speed (byte 3): 1 %/bit
    return [0xFF, u8(pedal_pct, 0.4), u8(load_pct, 1), NA8, NA8, NA8, NA8, NA8]


def et1(coolant_c, fuel_c, oil_c):
    # SPN 110 coolant (byte 1), SPN 174 fuel temp (byte 2): 1 °C/bit, -40; SPN 175 oil temp (bytes 3-4): 0.03125 °C/bit, -273
    return [u8(coolant_c, 1, -40), u8(fuel_c, 1, -40), *le16(u16(oil_c, 0.03125, -273)), NA8, NA8, NA8, NA8]


def eflp1(oil_level_pct, oil_kpa):
    # SPN 98 oil level (byte 3): 0.4 %/bit; SPN 100 oil pressure (byte 4): 4 kPa/bit
    return [NA8, NA8, u8(oil_level_pct, 0.4), u8(oil_kpa, 4), NA8, NA8, NA8, NA8]


def lfe1(fuel_lph, throttle_pct):
    # SPN 183 fuel rate (bytes 1-2): 0.05 L/h; SPN 51 throttle (byte 7): 0.4 %/bit
    return [*le16(u16(fuel_lph, 0.05)), NA8, NA8, NA8, NA8, u8(throttle_pct, 0.4), NA8]


def dd1(fuel_level_pct):
    # SPN 96 fuel level (byte 2): 0.4 %/bit
    return [NA8, u8(fuel_level_pct, 0.4), NA8, NA8, NA8, NA8, NA8, NA8]


def vep1(battery_v, charging_v, key_v):
    # SPN 167 charging potential (bytes 3-4), SPN 168 battery (bytes 5-6), SPN 158 key switch (7-8): 0.05 V/bit
    return [NA8, NA8, *le16(u16(charging_v, 0.05)), *le16(u16(battery_v, 0.05)), *le16(u16(key_v, 0.05))]


def ccvs1(speed_kmh):
    # SPN 84 wheel-based speed (bytes 2-3): 1/256 km/h
    return [0xFF, *le16(u16(speed_kmh, 1 / 256)), NA8, NA8, NA8, NA8, NA8]


def amb(baro_kpa, ambient_c, intake_c):
    # SPN 108 baro (byte 1): 0.5 kPa; SPN 171 ambient (bytes 4-5): 0.03125 °C, -273; SPN 172 inlet (byte 6): 1 °C, -40
    return [u8(baro_kpa, 0.5), NA8, NA8, *le16(u16(ambient_c, 0.03125, -273)), u8(intake_c, 1, -40), NA8, NA8]


def vf(hyd_c):
    # SPN 1638 hydraulic temperature (byte 1): 1 °C/bit, -40 (matches firmware decoder)
    return [u8(hyd_c, 1, -40), NA8, NA8, NA8, NA8, NA8, NA8, NA8]


def hours(engine_h):
    # SPN 247 engine total hours (bytes 1-4): 0.05 h/bit; SPN 249 revolutions not sent
    return [*le32(u32(engine_h, 0.05)), NA8, NA8, NA8, NA8]


def oem_deck(blade_on, deck_mm):
    # Placeholder layout (same as firmware): byte 1 bits 0-1 blade state; byte 2 deck height 2 mm/bit
    return [0xFC | (1 if blade_on else 0), u8(deck_mm, 2), NA8, NA8, NA8, NA8, NA8, NA8]


def td(dt_utc):
    # PGN 65254 Time/Date: s 0.25/bit, min, hour, month, day 0.25/bit, year +1985, local offsets (+125)
    return [int(dt_utc.second * 4), dt_utc.minute, dt_utc.hour, dt_utc.month, int(dt_utc.day * 4),
            dt_utc.year - 1985, 125, 125]


def request(pgn):
    return [pgn & 0xFF, (pgn >> 8) & 0xFF, (pgn >> 16) & 0xFF]


def addr_claim(name):
    return [(name >> (8 * i)) & 0xFF for i in range(8)]


def dtc_bytes(spn, fmi, oc):
    return [spn & 0xFF, (spn >> 8) & 0xFF, ((spn >> 11) & 0xE0) | (fmi & 0x1F), oc & 0x7F]


def dm1_payload(dtcs, amber=False, red=False):
    """Full DM1 payload. dtcs: list of (spn, fmi, oc). Returns bytes (may exceed 8 -> needs BAM)."""
    lamps = (0x10 if red else 0) | (0x04 if amber else 0)
    body = [lamps, 0xFF]
    if not dtcs:
        body += [0, 0, 0, 0, 0xFF, 0xFF]
    for spn, fmi, oc in dtcs:
        body += dtc_bytes(spn, fmi, oc)
    while len(body) < 8:
        body.append(0xFF)
    return body


def bam_frames(pgn, payload):
    """Split a long payload into TP.CM BAM + TP.DT frames (list of (pgn, da, bytes))."""
    n = len(payload)
    npk = (n + 6) // 7
    out = [(PGN_TP_CM, GLOBAL, [32, n & 0xFF, n >> 8, npk, 0xFF, pgn & 0xFF, (pgn >> 8) & 0xFF, (pgn >> 16) & 0xFF])]
    for i in range(npk):
        chunk = payload[i * 7:(i + 1) * 7]
        chunk += [0xFF] * (7 - len(chunk))
        out.append((PGN_TP_DT, GLOBAL, [i + 1] + chunk))
    return out


# --- decoders used by validate.py -------------------------------------------

def _d8(b, res=1.0, off=0.0):
    return None if b > 0xFA else b * res + off


def _d16(lo, hi, res=1.0, off=0.0):
    v = lo | (hi << 8)
    return None if v > 0xFAFF else v * res + off


def decode(pgn, d):
    if pgn == PGN_EEC1:
        return {"rpm": _d16(d[3], d[4], 0.125)}
    if pgn == PGN_EEC2:
        return {"load_pct": _d8(d[2])}
    if pgn == PGN_ET1:
        return {"coolant_c": _d8(d[0], 1, -40), "oil_temp_c": _d16(d[2], d[3], 0.03125, -273)}
    if pgn == PGN_EFLP1:
        return {"oil_press_kpa": _d8(d[3], 4)}
    if pgn == PGN_LFE1:
        return {"fuel_rate_lph": _d16(d[0], d[1], 0.05)}
    if pgn == PGN_DD1:
        return {"fuel_level_pct": _d8(d[1], 0.4)}
    if pgn == PGN_VEP1:
        return {"battery_v": _d16(d[4], d[5], 0.05)}
    if pgn == PGN_CCVS1:
        return {"wheel_speed_kmh": _d16(d[1], d[2], 1 / 256)}
    if pgn == PGN_VF:
        return {"hyd_oil_c": _d8(d[0], 1, -40)}
    if pgn == PGN_AMB:
        return {"ambient_c": _d16(d[3], d[4], 0.03125, -273)}
    if pgn == PGN_OEM_DECK:
        return {"blade_engaged": d[0] & 0x03}
    return {}
