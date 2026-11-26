"""Worst-case graphics + AI resource budget for a M1-class display (STM32H743 path).

First-principles model, not a measurement. Every input is in PLATFORM / WORKLOADS below and is
tagged with its source or with VERIFY. Change a number, rerun, and the tables follow:

  python3 budget.py              # markdown tables
  python3 budget.py --json out.json
"""
import argparse, json

MB = 1e6

PLATFORM = {
    "cpu_hz_nominal": 480e6,          # STM32H743 rev V, VOS0
    "cpu_hz_plan": 400e6,             # planning clock: 480 MHz is limited to a lower junction temp (VERIFY for the grade used)
    "sdram_bytes": 2 * 1024 * 1024,   # M1 datasheet: 2 MB SDRAM (ISSI, 16-bit class)
    "sdram_bus_bytes": 2,             # 16-bit data bus (VERIFY on the M1 board)
    "sdram_clk_hz": 100e6,            # FMC SDCLK = HCLK/2 with HCLK 200 MHz
    "sdram_efficiency": 0.55,         # mixed masters, row misses, refresh: 50-65 % of peak is typical
    "qspi_peak_bps": 50 * MB,         # quad SDR at 100 MHz
    "qspi_efficiency": 0.70,
    "panel_w": 480, "panel_h": 272, "refresh_hz": 60,   # M1 datasheet: 4.3 in, 480x272, 24-bit
    "internal_ram_kb": {"DTCM": 128, "AXI_SRAM": 512, "SRAM1-3": 288, "SRAM4": 64, "ITCM": 64},
    "can_frames_per_s_100pct_250k": 1613,  # 29-bit ID, 8 data bytes, worst-case bit stuffing
    "can_isr_us": 3.0,                # ISR + J1939 decode + vd_put at 400 MHz (estimate)
}

FB = {"RGB888": 3, "RGB565": 2}

# AI workloads: MACs per inference, cycles per MAC range, period (s).
AI = {
    "phase1_cnn_int8":  {"macs": 43_664,     "cpm": (3.0, 6.0), "period_s": 30,  "ram_kb": 16,  "rom_kb": 9,
                         "note": "deployed model; small layers carry high fixed overhead"},
    "phase1_lstm":      {"macs": 190_528,    "cpm": (4.0, 8.0), "period_s": 30,  "ram_kb": 24,  "rom_kb": 24,
                         "note": "candidate; float or int8 via ST Edge AI Core"},
    "nanoedge_anomaly": {"macs": 20_000,     "cpm": (4.0, 8.0), "period_s": 1,   "ram_kb": 8,   "rom_kb": 16,
                         "note": "trend / dull blades with on-device learning; size from the generated lib (VERIFY)"},
    "camera_detector":  {"macs": 60_000_000, "cpm": (1.5, 2.5), "period_s": None, "ram_kb": 450, "rom_kb": 900,
                         "note": "person/obstacle detector, 192x192 int8 class model; order of magnitude (VERIFY in ST model zoo benchmarks)"},
}


def screen_bytes(fmt):
    return PLATFORM["panel_w"] * PLATFORM["panel_h"] * FB[fmt]


def sdram_usable():
    p = PLATFORM
    return p["sdram_clk_hz"] * p["sdram_bus_bytes"] * p["sdram_efficiency"]


def scenarios(fmt):
    """Each scenario: SDRAM MB/s by source, CPU % by source (at plan clock), QSPI MB/s, notes."""
    s = screen_bytes(fmt)
    hz = PLATFORM["cpu_hz_plan"]
    scan = s * PLATFORM["refresh_hz"] / MB
    can = lambda load, buses, k=1: buses * load * PLATFORM["can_frames_per_s_100pct_250k"] * k * PLATFORM["can_isr_us"] / 1e4
    cnn_burst_ms = AI["phase1_cnn_int8"]["macs"] * AI["phase1_cnn_int8"]["cpm"][1] / hz * 1e3
    lstm_burst_ms = AI["phase1_lstm"]["macs"] * AI["phase1_lstm"]["cpm"][1] / hz * 1e3
    px = PLATFORM["panel_w"] * PLATFORM["panel_h"]
    out = []

    # S1: normal mowing dashboard, 20 % of the screen redrawn at 30 fps, 2 rotating needles
    needle_cyc = 2 * 120 * 120 * 30            # texture-mapped bounding boxes, ~30 cycles/px
    out.append(dict(
        id="S1", name="Mowing dashboard, steady",
        what="Gauges, needles and numbers update at 30 fps (about 20 % of the screen per frame). CNN runs every 30 s.",
        sdram={"LTDC scan-out": scan, "Render (write + blend read)": 2 * 0.2 * s * 30 / MB},
        qspi=0.2 * s * 30 / MB,
        cpu={"RTOS + app services": 3.0, "CAN ISR (2 buses, 40 % load)": can(0.4, 2),
             "TouchGFX framework": 1.0e-3 * 30 * 100 / 1, "Needles (texture mapper)": needle_cyc * 30 / hz * 100},
        ai_burst_ms=cnn_burst_ms,
        notes="Comfortable. The AI is invisible at this level."))

    # S2: full-screen slide transition at 60 fps exactly when the AI fires
    out.append(dict(
        id="S2", name="Screen transition + AI inference",
        what="Operator swipes to the trip page: full-screen slide at 60 fps (snapshot read + new screen write every frame). The CNN fires mid-animation.",
        sdram={"LTDC scan-out": scan, "Slide (snapshot read + write)": 2 * s * 60 / MB},
        qspi=0.5 * s * 60 / MB,
        cpu={"RTOS + app services": 3.0, "CAN ISR (2 buses, 40 % load)": can(0.4, 2),
             "TouchGFX framework": 1.5e-3 * 60 * 100, "Widgets on new screen": 8.0},
        ai_burst_ms=cnn_burst_ms,
        notes="SDRAM is the limit, not the CPU. One 0.66 ms AI burst costs at most part of one frame if AI runs below the GUI priority."))

    # S3: rear camera (M1i / M1iv) full screen with guide lines while reversing
    cam_w, cam_h, cam_fps = 360, 240, 30       # DCMI 2:1 decimation of a 720x480 decoder stream
    conv_cyc = px * cam_fps * 14               # YUV422 -> panel format + scale on the CPU, ~14 cycles/px
    out.append(dict(
        id="S3", name="Reversing camera + overlays",
        what="ZTR reversing: camera at 30 fps full screen, guide-line overlay, warning banner. CNN keeps running.",
        sdram={"LTDC scan-out": scan, "Camera DCMI writes": cam_w * cam_h * 2 * cam_fps / MB,
               "Convert: read camera": cam_w * cam_h * 2 * cam_fps / MB, "Convert: write frame": s * cam_fps / MB,
               "Overlay blend (15 %)": 2 * 0.15 * s * cam_fps / MB},
        qspi=0.05 * s * cam_fps / MB,
        cpu={"RTOS + app services": 3.0, "CAN ISR (2 buses, 40 % load)": can(0.4, 2),
             "TouchGFX framework": 1.0e-3 * 30 * 100, "YUV to RGB + scale (CPU)": conv_cyc / hz * 100},
        ai_burst_ms=cnn_burst_ms,
        notes="The colour conversion is the hidden CPU cost: the H743 DMA2D does not take raster YUV422 (VERIFY against the M1i camera path)."))

    # S4: fault storm
    out.append(dict(
        id="S4", name="Fault storm",
        what="Engine fault: DM1 with 20 DTCs over TP on both buses at 70 % load, alarm pop-up fading in over a dimmed screen, events logged (flash sector erase starts), AI alert at the same moment.",
        sdram={"LTDC scan-out": scan, "Fade + dim (full-screen blend, 60 fps)": 2 * s * 60 / MB},
        qspi=0.3 * s * 60 / MB,
        cpu={"RTOS + app services": 4.0, "CAN ISR (2 buses, 70 % load)": can(0.7, 2),
             "TouchGFX framework": 1.5e-3 * 60 * 100, "Alarm widgets": 4.0},
        ai_burst_ms=cnn_burst_ms,
        notes="Bandwidth is fine. The real risk is the flash: a W25Q sector erase blocks memory-mapped reads for 45 ms typical, 400 ms max. If GUI assets live on the same chip, the alarm screen freezes."))

    # S5: everything at once
    out.append(dict(
        id="S5", name="Worst of worst",
        what="Camera view + alarm fade + fault storm + LSTM instead of CNN + Bluetooth log download (M1v / M1iv) + hot cabin (400 MHz plan clock).",
        sdram={"LTDC scan-out": scan, "Camera DCMI writes": cam_w * cam_h * 2 * cam_fps / MB,
               "Convert: read camera": cam_w * cam_h * 2 * cam_fps / MB, "Convert: write frame": s * cam_fps / MB,
               "Fade + dim (60 fps)": 2 * s * 60 / MB},
        qspi=0.3 * s * 60 / MB + 0.1,
        cpu={"RTOS + app services": 4.0, "CAN ISR (2 buses, 70 % load)": can(0.7, 2),
             "TouchGFX framework": 1.5e-3 * 60 * 100, "YUV to RGB + scale (CPU)": conv_cyc / hz * 100,
             "Alarm widgets": 4.0, "BLE dump (CRC + software AES)": 100e3 * 30 / hz * 100},
        ai_burst_ms=lstm_burst_ms,
        notes="Only reachable with design rules: no 60 fps fades while the camera is shown, RGB565 frame buffers, LTDC given bus priority."))

    # S6: roadmap - camera AI on the H743
    det = AI["camera_detector"]
    det_cyc_lo, det_cyc_hi = det["macs"] * det["cpm"][0], det["macs"] * det["cpm"][1]
    out.append(dict(
        id="S6", name="Camera AI on the H743 (roadmap)",
        what="Person / obstacle detection on the camera feed while the camera view and dashboard run.",
        sdram={"LTDC scan-out": scan, "Camera DCMI writes": cam_w * cam_h * 2 * cam_fps / MB,
               "Convert: read camera": cam_w * cam_h * 2 * cam_fps / MB, "Convert: write frame": s * cam_fps / MB,
               "Detector input crop (192x192)": 192 * 192 * 3 * 5 / MB},
        qspi=0.05 * s * cam_fps / MB,
        cpu={"RTOS + app services": 3.0, "CAN ISR (2 buses, 40 % load)": can(0.4, 2),
             "TouchGFX framework": 1.0e-3 * 30 * 100, "YUV to RGB + scale (CPU)": conv_cyc / hz * 100},
        ai_burst_ms=det_cyc_hi / hz * 1e3,
        detector_ms=(det_cyc_lo / hz * 1e3, det_cyc_hi / hz * 1e3),
        notes="One detection takes 225-375 ms of CPU. With the spare CPU left, the detector reaches about 2-3.5 fps. Too slow and not a safety function. This is the case for the STM32N6 NPU."))
    for sc in out:
        sc["sdram_total"] = sum(sc["sdram"].values())
        sc["sdram_pct"] = 100 * sc["sdram_total"] * MB / sdram_usable()
        sc["cpu_total"] = sum(sc["cpu"].values())
        sc["qspi_pct"] = 100 * sc["qspi"] * MB / (PLATFORM["qspi_peak_bps"] * PLATFORM["qspi_efficiency"])
        sc["frame_ms_lost_to_ai"] = sc["ai_burst_ms"]
        spare = max(0.0, 100 - sc["cpu_total"]) / 100
        if "detector_ms" in sc:
            sc["detector_fps"] = (spare * 1e3 / sc["detector_ms"][1], spare * 1e3 / sc["detector_ms"][0])
        sc["status"] = "red" if sc["sdram_pct"] > 70 or sc["cpu_total"] > 70 or "detector_ms" in sc else \
                       "amber" if sc["sdram_pct"] > 50 or sc["cpu_total"] > 50 else "green"
    return out


def memory_map(fmt, camera=True):
    s = screen_bytes(fmt)
    sd = {"Frame buffers (2)": 2 * s, "Transition snapshot": s,
          "Camera buffers (2, 360x240 YUV422)": 2 * 360 * 240 * 2 if camera else 0}
    sd["Asset cache (alarm screens + fonts)"] = 300 * 1024
    sd["Free"] = PLATFORM["sdram_bytes"] - sum(sd.values())
    return sd


def ai_table():
    hz = PLATFORM["cpu_hz_plan"]
    rows = []
    for k, a in AI.items():
        lo, hi = a["macs"] * a["cpm"][0] / hz * 1e3, a["macs"] * a["cpm"][1] / hz * 1e3
        avg = None if not a["period_s"] else 100 * hi / 1e3 / a["period_s"]
        rows.append(dict(model=k, macs=a["macs"], ms=(lo, hi), avg_cpu_pct=avg, ram_kb=a["ram_kb"], rom_kb=a["rom_kb"], note=a["note"]))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json")
    a = ap.parse_args()
    res = {"platform": PLATFORM, "sdram_usable_MBps": sdram_usable() / MB, "ai": ai_table(),
           "scenarios": {f: scenarios(f) for f in FB}, "sdram_map": {f: memory_map(f) for f in FB}}
    for f in FB:
        print(f"\n## Frame buffers {f}  (usable SDRAM {sdram_usable()/MB:.0f} MB/s at plan clock)\n")
        print("| Scenario | SDRAM MB/s | SDRAM % | CPU % | QSPI % | AI burst ms | Status |")
        print("|---|---|---|---|---|---|---|")
        for s in res["scenarios"][f]:
            print(f"| {s['id']} {s['name']} | {s['sdram_total']:.1f} | {s['sdram_pct']:.0f} | {s['cpu_total']:.1f} | "
                  f"{s['qspi_pct']:.0f} | {s['ai_burst_ms']:.2f} | {s['status']} |")
        print("\nSDRAM map:", {k: f"{v/1024:.0f} KB" for k, v in res["sdram_map"][f].items()})
    print("\n## AI workloads at plan clock\n")
    for r in res["ai"]:
        avg = "-" if r["avg_cpu_pct"] is None else f"{r['avg_cpu_pct']:.4f} %"
        print(f"- {r['model']}: {r['macs']:,} MACs, {r['ms'][0]:.2f}-{r['ms'][1]:.2f} ms, average CPU {avg}")
    if a.json:
        json.dump(res, open(a.json, "w"), indent=1, default=list)


if __name__ == "__main__":
    main()
