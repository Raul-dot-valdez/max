// M1 graphics framework strategy deck (structured pptxgenjs build)
const pptxgen = require("pptxgenjs");
const { applyTheme } = require(process.env.APPLY_THEME || "./apply_theme.js") /* theme helper from the pptx skill; not stored in this repo */;

const THEME = {
  name: "AI Terra Graphite",
  headFontFace: "Arial",
  bodyFontFace: "Calibri",
  colors: {
    dk1: "0F1418", lt1: "FFFFFF", dk2: "1E262D", lt2: "F1F4F6",
    accent1: "FF7A1A", accent2: "B8520A", accent3: "2F7D4F",
    accent4: "8E6200", accent5: "9A3B2C", accent6: "5B6873",
    hlink: "B8520A", folHlink: "8E6200",
  },
};
const HEX = { rule: "CDD5DB", soft: "F1F4F6", graphite: "0F1418", amber: "FF7A1A", slate: "5B6873" };

const pres = new pptxgen();
pres.layout = "LAYOUT_WIDE"; // 13.33 x 7.5
pres.title = "M1 SDK Review";
pres.author = "AI Terra";
pres.subject = "maximatecc M1 SDK deficiencies and opportunities";
pres.theme = { headFontFace: THEME.headFontFace, bodyFontFace: THEME.bodyFontFace };
const C = pres.SchemeColor;

const W = 13.333, M = 0.6, CW = W - 2 * M;
const WB = process.env.MODE === "wainbee";
const FOOT = WB ? "M1 × Wainbee · Claude Code working session" : "AI Terra · M1 SDK review · November 2026";

// ---------- layouts ----------
pres.defineSlideMaster({
  title: "TITLE_DARK",
  background: { color: C.text1 },
  objects: [
    { placeholder: { options: { name: "title", type: "title", x: M, y: 2.1, w: 11.2, h: 1.9, fontSize: 40, bold: true, color: C.background1, valign: "bottom", align: "left", margin: 0 }, text: "" } },
    { placeholder: { options: { name: "body", type: "body", x: M, y: 4.2, w: 10.5, h: 1.4, fontSize: 18, color: "B9C3CB", valign: "top", align: "left", margin: 0 }, text: "" } },
    { text: { text: WB ? "M1 × WAINBEE · WORKING SESSION" : "SDK REVIEW · EXPERT PANEL DEBATE", options: { x: M, y: 1.4, w: 10, h: 0.4, fontSize: 12, bold: true, color: C.accent1, charSpacing: 2, margin: 0 } } },
  ],
});
pres.defineSlideMaster({
  title: "SECTION_DARK",
  background: { color: C.text1 },
  objects: [
    { placeholder: { options: { name: "title", type: "title", x: M, y: 2.6, w: 11.5, h: 1.3, fontSize: 40, bold: true, color: C.background1, valign: "bottom", align: "left", margin: 0 }, text: "" } },
    { placeholder: { options: { name: "body", type: "body", x: M, y: 4.1, w: 10.5, h: 1.2, fontSize: 18, color: "B9C3CB", valign: "top", align: "left", margin: 0 }, text: "" } },
    { text: { text: FOOT, options: { x: M, y: 6.95, w: 9, h: 0.3, fontSize: 10, color: "8B98A3", margin: 0 } } },
  ],
  slideNumber: { x: 12.2, y: 6.95, w: 0.6, h: 0.3, fontSize: 10, color: "8B98A3", align: "right" },
});
pres.defineSlideMaster({
  title: "CONTENT",
  background: { color: C.background1 },
  objects: [
    { placeholder: { options: { name: "title", type: "title", x: M, y: 0.35, w: CW, h: 0.85, fontSize: 28, bold: true, color: C.text1, valign: "middle", align: "left", margin: 0 }, text: "" } },
    { text: { text: FOOT, options: { x: M, y: 7.0, w: 9, h: 0.3, fontSize: 10, color: C.accent6, margin: 0 } } },
  ],
  slideNumber: { x: 12.2, y: 7.0, w: 0.6, h: 0.3, fontSize: 10, color: C.accent6, align: "right" },
});

// ---------- helpers ----------
const TAGC = { EVIDENCE: C.accent3, JUDGMENT: C.accent4, VERIFY: C.accent5 };
let slideNo = 0;
function content(section, title) {
  const s = pres.addSlide({ masterName: "CONTENT", sectionTitle: section });
  s.addText(title, { placeholder: "title" });
  slideNo++;
  return s;
}
function lede(s, text, y = 1.25, h = 0.55) {
  s.addText(text, { x: M, y, w: CW, h, fontSize: 14, color: C.accent6, margin: 0, valign: "top", isTextBox: true });
}
function tag(s, kind, x, y) {
  s.addText(kind, { x, y, w: 1.15, h: 0.28, fontSize: 10, bold: true, color: TAGC[kind], align: "center", valign: "middle",
    line: { color: TAGC[kind], width: 1 }, margin: 0, isTextBox: true, charSpacing: 1 });
}
function bullets(items, size = 14) {
  return items.map((t, i) => {
    const runs = typeof t === "string" ? { text: t } : t;
    return { text: runs.text, options: { bullet: { indent: 14 }, breakLine: i < items.length - 1, paraSpaceAfter: 5, fontSize: size, bold: runs.bold, color: runs.color } };
  });
}
function card(s, x, y, w, h, head, body, opts = {}) {
  s.addShape(pres.shapes.RECTANGLE, { x, y, w, h, fill: { color: opts.fill || C.background2 }, line: { color: HEX.rule, width: 0.75 } });
  let ty = y + 0.15;
  if (opts.kicker) {
    s.addText(opts.kicker, { x: x + 0.2, y: ty, w: w - 0.4, h: 0.28, fontSize: 11, bold: true, color: opts.kickerColor || C.accent2, margin: 0, charSpacing: 1, isTextBox: true });
    ty += 0.32;
  }
  s.addText(head, { x: x + 0.2, y: ty, w: w - 0.4, h: opts.headH || 0.4, fontSize: opts.headSize || 16, bold: true, color: C.text1, margin: 0, valign: "top", isTextBox: true });
  ty += (opts.headH || 0.4) + 0.05;
  const arr = Array.isArray(body) ? bullets(body, opts.size || 13) : [{ text: body, options: { fontSize: opts.size || 13 } }];
  s.addText(arr, { x: x + 0.2, y: ty, w: w - 0.4, h: y + h - ty - 0.12, color: C.text2, margin: 0, valign: "top", isTextBox: true });
}
function table(s, header, rows, colW, y = 1.4, opts = {}) {
  const fs = opts.fontSize || 12;
  const head = header.map((h) => ({ text: h, options: { bold: true, color: C.background1, fill: { color: C.text1 }, fontSize: 11, charSpacing: 1 } }));
  const body = rows.map((r) => r.map((c, i) => {
    const o = typeof c === "object" ? c : { text: c };
    return { text: o.text, options: { fontSize: fs, color: o.color || C.text1, bold: o.bold !== undefined ? o.bold : i === 0, fill: { color: o.fill || C.background1 } } };
  }));
  s.addTable([head, ...body], { x: M, y, w: CW, colW, border: { type: "solid", color: HEX.rule, pt: 0.75 }, margin: [5, 7, 5, 7], valign: "top", autoPage: false });
}
function circleNum(s, n, x, y, d = 0.6, fill = C.accent1) {
  s.addShape(pres.shapes.OVAL, { x, y, w: d, h: d, fill: { color: fill }, line: { color: fill } });
  s.addText(String(n), { x, y, w: d, h: d, fontSize: d > 0.7 ? 26 : 18, bold: true, color: C.text1, align: "center", valign: "middle", margin: 0, isTextBox: true });
}

// ============ 1. Title ============
// ---------- extra helpers ----------
function divider(section, title, body) {
  pres.addSection({ title: section });
  const s = pres.addSlide({ masterName: "SECTION_DARK", sectionTitle: section });
  s.addText(title, { placeholder: "title" });
  s.addText(body, { placeholder: "body" });
  return s;
}
function pill(s, text, x, y, w, color) {
  s.addText(text, { x, y, w, h: 0.3, fontSize: 11, bold: true, color: C.background1, fill: { color }, align: "center", valign: "middle", margin: 0, isTextBox: true });
}
function layer(s, x, y, w, h, label, detail, fill, ink) {
  s.addShape(pres.shapes.RECTANGLE, { x, y, w, h, fill: { color: fill }, line: { color: HEX.rule, width: 0.75 } });
  s.addText([{ text: label, options: { bold: true, fontSize: 14, color: ink || C.text1, breakLine: true } }, { text: detail, options: { fontSize: 12, color: ink || C.text2 } }],
    { x: x + 0.15, y, w: w - 0.3, h, valign: "middle", margin: 0, isTextBox: true });
}
function debate(section, title, question, pos, verdict, cto) {
  const s = content(section, title);
  s.addText(question, { x: M, y: 1.2, w: CW, h: 0.45, fontSize: 15, italic: true, color: C.accent6, margin: 0, isTextBox: true });
  const cw = (CW - 2 * 0.3) / 3;
  pos.forEach((p, i) => {
    const x = M + i * (cw + 0.3), y = 1.8, h = 3.15;
    s.addShape(pres.shapes.RECTANGLE, { x, y, w: cw, h, fill: { color: i === 2 ? "FBF3E4" : C.background2 }, line: { color: HEX.rule, width: 0.75 } });
    s.addText(p[0], { x: x + 0.2, y: y + 0.15, w: cw - 0.4, h: 0.3, fontSize: 11, bold: true, color: i === 2 ? C.accent4 : C.accent2, charSpacing: 1, margin: 0, isTextBox: true });
    s.addText(p[1], { x: x + 0.2, y: y + 0.47, w: cw - 0.4, h: 0.4, fontSize: 15, bold: true, color: C.text1, margin: 0, isTextBox: true });
    s.addText(p[2], { x: x + 0.2, y: y + 0.9, w: cw - 0.4, h: h - 1.0, fontSize: 13, color: C.text2, margin: 0, valign: "top", isTextBox: true });
  });
  s.addShape(pres.shapes.RECTANGLE, { x: M, y: 5.15, w: CW, h: 1.65, fill: { color: C.text1 }, line: { color: C.text1 } });
  s.addText([{ text: "VERDICT   ", options: { bold: true, color: C.accent1, fontSize: 12 } }, { text: verdict, options: { color: C.background1, fontSize: 14, breakLine: true } },
             { text: "FOR THE CTO   ", options: { bold: true, color: C.accent1, fontSize: 12 } }, { text: cto, options: { color: "B9C3CB", fontSize: 14 } }],
    { x: M + 0.25, y: 5.2, w: CW - 0.5, h: 1.55, valign: "middle", margin: 0, paraSpaceAfter: 6, isTextBox: true });
  return s;
}

// ============ Opening ============
if (!WB) {
pres.addSection({ title: "Opening" });
{
  const s = pres.addSlide({ masterName: "TITLE_DARK", sectionTitle: "Opening" });
  s.addText("M1 SDK review: gaps, opportunities and the path to growth", { placeholder: "title" });
  s.addText("A senior expert panel debates maximatecc's M1 display SDKs, benchmarks them against CrossControl CCpilot, and proposes an SDK 2.0 for GUI firmware teams and an investment case for the CTO. Based on maximatecc.com as read on 24 November 2026.", { placeholder: "body" });
  s.addNotes("Labels used throughout: EVIDENCE = stated on maximatecc.com, crosscontrol.com or another cited source; JUDGMENT = panel assessment; VERIFY = check before deciding.");
}
{
  const s = content("Opening", "Executive summary");
  card(s, M, 1.35, 3.843, 3.85, "Strong hardware, thin SDK", "Rugged CAN displays from 1.36\" to 4.3\" with good environmental ratings. Only the M1 280 has a documented SDK; the M1, marketed as fully customisable, has a configurator and a \"Qt for MCU compatibility\" claim but no SDK package.", { kicker: "WHERE WE ARE", size: 14, headH: 0.45 });
  card(s, M + 4.143, 1.35, 3.843, 3.85, "Five gaps hold back growth", "Two incompatible GUI builds, a locked STM32 hardware layer, custom CAN only through sales, no simulator or CI, and no secure field update. Each slows customers and caps scale.", { kicker: "WHAT HOLDS IT BACK", size: 14, headH: 0.45 });
  card(s, M + 8.287, 1.35, 3.843, 3.85, "M1 SDK 2.0", "One signal model and data layer across the range, a GUI bridge so TouchGFX, Qt for MCUs and LVGL share app code, self-service J1939, a host simulator and CLI built for AI agents, and secure updates over USB and Bluetooth.", { kicker: "WHAT WE PROPOSE", size: 14, headH: 0.45, fill: "FFF1E6" });
  s.addText([{ text: "Correction to the earlier strategy deck: ", options: { bold: true, color: C.accent5 } },
    { text: "M1 is maximatecc's own MCU display line (130, 200, 280, M1), part of CentroMotion. CrossControl CCpilot is a separate company's Linux display range and is used here only as a benchmark.", options: { color: C.text2 } }],
    { x: M, y: 5.45, w: CW, h: 0.95, fontSize: 14, margin: 12, fill: { color: C.background2 }, valign: "middle", isTextBox: true });
}
{
  const s = content("Opening", "The panel: eight senior experts, two audiences");
  const P = [
    ["GUI firmware lead", "TouchGFX, Qt for MCUs, LVGL"], ["Embedded architect", "STM32H7, FreeRTOS, memory"],
    ["CAN / J1939 specialist", "Stacks, DBC, CANopen"], ["Safety and cyber lead", "ISO 25119, ISO 13849, CRA"],
    ["OEM HMI engineer", "Customer voice, mid-size OEM"], ["AI tooling lead", "Agents, CI, simulators"],
    ["Product manager", "Portfolio and pricing"], ["CTO office", "Investment and growth"],
  ];
  const cw = (CW - 3 * 0.3) / 4;
  P.forEach((p, i) => {
    const x = M + (i % 4) * (cw + 0.3), y = 1.4 + Math.floor(i / 4) * 1.55;
    circleNum(s, i + 1, x, y + 0.1, 0.55, i < 6 ? C.accent1 : C.accent6);
    s.addText([{ text: p[0], options: { bold: true, fontSize: 15, color: C.text1, breakLine: true } }, { text: p[1], options: { fontSize: 13, color: C.accent6 } }],
      { x: x + 0.7, y, w: cw - 0.7, h: 0.8, margin: 0, valign: "middle", isTextBox: true });
  });
  s.addText("Orange: technical lens for GUI firmware developers. Grey: business lens for the CTO. Each debate slide ends with a verdict and a one-line takeaway for the CTO.", { x: M, y: 4.6, w: 7, h: 0.9, fontSize: 14, color: C.text2, margin: 0, isTextBox: true });
  [["EVIDENCE", "Stated in a cited source"], ["JUDGMENT", "Panel assessment"], ["VERIFY", "Check before deciding"]].forEach((t, i) => {
    tag(s, t[0], 8.2, 4.6 + i * 0.5);
    s.addText(t[1], { x: 9.5, y: 4.6 + i * 0.5, w: 3.2, h: 0.28, fontSize: 13, color: C.text1, margin: 0, valign: "middle", isTextBox: true });
  });
}

// ============ Current state ============
divider("Current state", "Where the M1 SDKs stand today", "Facts from maximatecc.com: the display range, the software paths, and the M1 280 SDK in detail");
{
  const s = content("Current state", "The M1 display range");
  table(s, ["Model", "Processor", "Memory", "Display", "CAN", "Other I/O and options"], [
    ["M1 130", "Arm Cortex-M0", "16 KB RAM, 200 KB flash", "1.36\" FSTN, 106×56 mono", "1 × J1939 or CANopen, 250/500k", "4 analog inputs; 3 LEDs; optional 2 keys (130b)"],
    ["M1 200", "Arm Cortex-M3", "64 KB RAM, 512 KB flash, 64 KB EEPROM", "2.0\" LCD, 128×96 mono", "1 × J1939, 250/500k", "4 inputs, 2 outputs; up to 50 parameters"],
    ["M1 280 / 280b", "STM32H743II (Cortex-M7)", "1 MB RAM, 2 MB flash; 16 MB ext. flash", "2.8\" IPS, 240×320, 18-bit", "1 × J1939 (bit rate listed as \"5Mbps\")", "280b: 5 in / 4 out, Bluetooth for config"],
    ["M1 / v / i / iv", "Arm Cortex-M7 (part not stated)", "1 MB RAM, 2 MB flash; 2 MB SDRAM, 16 MB flash", "4.3\" 480×272, 24-bit, 1000 nit", "2 × J1939, 250/500k", "5 in + 4–20 mA, 1 out, camera input; BT 4.2 on v / iv"],
  ], [1.75, 2.0, 2.4, 2.0, 2.0, 1.98], 1.4, { fontSize: 12 });
  tag(s, "EVIDENCE", 11.58, 1.03);
  s.addText("All models: IP67 front, ISO 13766 emissions, SAE J1113-21 immunity, 9–32 V. The 280 and M1 run FreeRTOS. Largest screen in the range: 4.3\".",
    { x: M, y: 5.6, w: CW, h: 0.6, fontSize: 14, italic: true, color: C.accent2, margin: 0, isTextBox: true });
}
{
  const s = content("Current state", "Four software paths, unevenly available");
  table(s, ["Path", "What it is", "130", "200", "280", "M1"], [
    ["Configurator", "No-code tool: presets for machine data and layouts, custom icons", "V2.0.0.0", "On device", "2.0.0.0", "V2.3.1.2"],
    ["M1 STM SDK (ST-SDK)", "STM32CubeIDE project with TouchGFX GUI, FreeRTOS, CAN stack, data layer", "—", "—", { text: "v1.0 (2025)", bold: true }, { text: "Not documented", color: C.accent5 }],
    ["M1 Qt SDK (Qt-SDK)", "Qt for MCUs 2.11 project; needs a commercial Qt licence", "—", "—", { text: "v1.0 (2025)", bold: true }, { text: "\"Qt for MCU compatibility\" claimed, no package", color: C.accent5 }],
    ["M1 Design Studio", "Listed on downloads pages", "—", "—", "Contact sales", "Contact sales"],
    ["M1 Specialized", "Custom interface development by maximatecc engineers", "Quote", "Quote", "Quote", "Quote"],
  ], [2.4, 4.3, 1.1, 1.1, 1.4, 1.83], 1.4, { fontSize: 13 });
  tag(s, "EVIDENCE", 11.58, 1.03);
  s.addText("Implication: a customer who outgrows the configurator on a M1, the flagship, has no documented SDK to move to.",
    { x: M, y: 5.5, w: CW, h: 0.6, fontSize: 15, bold: true, color: C.text1, margin: 0, isTextBox: true });
}
{
  const s = content("Current state", "Inside the M1 280 SDK");
  const x = M, w = 6.6;
  layer(s, x, 1.4, w, 0.75, "Application layer", "Customer logic", "FFF1E6");
  layer(s, x, 2.22, w / 2 - 0.05, 0.85, "TouchGFX (ST-SDK)", "MVP pattern, generated C++", C.background2);
  layer(s, x + w / 2 + 0.05, 2.22, w / 2 - 0.05, 0.85, "Qt for MCUs (Qt-SDK)", "QML subset compiled to C++", C.background2);
  layer(s, x, 3.14, w, 0.75, "Data layer", "GET_DL / SET_DL by field id in database.h", "E8F1EA");
  layer(s, x, 3.96, w, 0.75, "Platform services", "FreeRTOS tasks; J1939 and CANopen (CiA 301) stacks", C.background2);
  layer(s, x, 4.78, w, 0.75, "MCAL", "Generated by STM32CubeMX: GPIO, timers, ADC, comms", C.background2);
  layer(s, x, 5.6, w, 0.6, "STM32H743II", "Cortex-M7 · 1 MB RAM · 2 MB flash", "0F1418", C.background1);
  card(s, 7.55, 1.4, 5.18, 2.35, "What works", ["The data layer cleanly separates the GUI from CAN and I/O", "Two GUI tools, both generating C/C++", "J1939 and CANopen defaults included"], { kicker: "EVIDENCE", kickerColor: C.accent3, size: 13 });
  card(s, 7.55, 3.9, 5.18, 2.3, "Stated limits", ["The MCAL \"should not be altered\": seek support", "ST-SDK and Qt-SDK are separate, incompatible builds", "Custom J1939 or CANopen sets need a tailored library from sales"], { kicker: "EVIDENCE", kickerColor: C.accent5, size: 13 });
}
{
  const s = content("Current state", "Toolchain and packages (M1 280 SDK v1.0)");
  table(s, ["Tool", "Version", "Used by"], [
    ["STM32CubeIDE", "1.18.1", "Both"], ["GNU Tools for STM32", "13.3.rel1", "ST-SDK"], ["Arm GNU Toolchain", "12.3.rel1", "Qt-SDK"],
    ["STM32CubeMX + H7 HAL", "6.14.1", "Both"], ["STM32CubeProgrammer / ST-LINK Utility", ">2.19.0 / 2.1.1-2", "Both"],
    ["TouchGFX", "4.25", "ST-SDK"], ["Qt for MCUs", "2.11.1 (commercial licence)", "Qt-SDK"],
  ], [3.6, 2.6, 1.6], 1.4, { fontSize: 13 });
  card(s, 8.85, 1.4, 3.88, 4.85, "Shipped packages", ["280_st_sdk_blank: TouchGFX starter", "280_st_sdk_demo: gauge and telltales over J1939 or CANopen", "280_st_sdk_integration: one screen per module for hardware checks", "280_qt_sdk: Qt for MCUs starter for Qt Creator", "All as 7z archives; Windows install steps; no CLI, no simulator stated"], { size: 13, kicker: "EVIDENCE", kickerColor: C.accent3 });
  s.addText("Two compilers and two GUI tools pinned to exact versions: every upgrade is a two-path regression job.", { x: M, y: 5.0, w: 7.9, h: 0.8, fontSize: 14, italic: true, color: C.accent2, margin: 0, isTextBox: true });
}
{
  const s = content("Current state", "What the panel credits today");
  const G = [
    ["Rugged, compliant hardware", "IP67, ISO 13766, SAE J1113-21, 9–32 V, glass cover, 1000-nit screens: OEM-ready out of the box."],
    ["A real data layer", "GET_DL / SET_DL decouples screens from CAN and I/O, the right foundation for everything proposed later."],
    ["Two respected GUI tools", "TouchGFX is free on STM32; Qt for MCUs serves teams that know QML. Both produce compiled C++."],
    ["Protocol coverage", "J1939 and CANopen built in, plus a no-code configurator for the many OEMs who never need code."],
  ];
  G.forEach((g, i) => {
    const y = 1.4 + i * 1.3;
    circleNum(s, "✓", M, y + 0.1, 0.6, C.accent3);
    s.addText([{ text: g[0], options: { bold: true, fontSize: 17, color: C.text1, breakLine: true } }, { text: g[1], options: { fontSize: 14, color: C.text2 } }],
      { x: M + 0.85, y, w: 11.2, h: 1.1, margin: 0, valign: "middle", isTextBox: true });
  });
  tag(s, "JUDGMENT", 11.58, 1.03);
}

// ============ Benchmark ============
divider("Benchmark", "Benchmark: M1 SDKs vs CrossControl CCpilot", "CCpilot is a separate company's Linux display range (7\"–12\"). It shows what a mature display SDK offers, and where the white space is.");
{
  const s = content("Benchmark", "Side by side 1/2: platform and runtime");
  table(s, ["Dimension", "maximatecc M1 SDKs", "CrossControl CCpilot SDK"], [
    ["Hardware class", "MCU instrument displays, 1.36\"–4.3\" (Cortex-M0/M3/M7)", "Linux display computers, 7\"–12\" (NXP i.MX 8; Intel on X900)"],
    ["Operating system", "FreeRTOS (280, M1); bare firmware on 130/200", "CCLinux, Yocto-based; LinX Core manages system integrity and settings"],
    ["GUI options", "TouchGFX (ST-SDK) or Qt for MCUs (Qt-SDK), separate builds; configurator", "Qt 6 (CCLinux 5.0 with Qt 6.8.3; 5.1 with Qt 6.12.0), UX Designer, CODESYS, HTML5"],
    ["Signal layer", "Data layer: GET_DL / SET_DL over database.h", "Data Engine, shared between Qt apps and CODESYS projects"],
    ["Fieldbus", "J1939 and CANopen; custom sets via a tailored library from sales", "Fieldbus Access in the LinX-Base package"],
    ["CODESYS", "Not offered", "CODESYS 3.5 supported"],
  ], [2.3, 4.9, 4.93], 1.4, { fontSize: 13 });
  tag(s, "EVIDENCE", 11.58, 1.03);
  s.addNotes("CCpilot facts from crosscontrol.com product pages (V1290, V710, V1200, X900), the CC Linux Programmer's Guide (2022), UX Designer online documentation (v5.0, 5.1, 6.0), Docker Hub crosscontrol images, and the CrossControl 2023 brochure. The Qt 6.12.0 version comes from a search summary of the V1290 page: verify.");
}
{
  const s = content("Benchmark", "Side by side 2/2: developer experience and lifecycle");
  table(s, ["Dimension", "maximatecc M1 SDKs", "CrossControl CCpilot SDK"], [
    ["Host and build", "Windows install steps; STM32CubeIDE; two pinned compilers", "Linux VM or native; Yocto SDK installers; Docker images (e.g. crosscontrol/v1x00-sdk:4.2.0.0_qt-6.8.3); Qt Creator integration"],
    ["Headless / CI build", "Not documented", "Yes: Docker command-line build documented"],
    ["Simulator", "Not stated", "Not confirmed in sources read"],
    ["Field update", "Rebuild and reprogram via USB bootloader or ST-LINK", "Not confirmed in sources read"],
    ["Licensing", "TouchGFX free on STM32; Qt for MCUs commercial only", "Qt runtime packages sold per image; Qt Commercial optional"],
    ["Documentation", "280 SDK manual v1.0 (2025); M1 SDK absent; spec inconsistencies", "Versioned online manuals (UX Designer 5.0 → 6.0); programmer's guide"],
  ], [2.3, 4.9, 4.93], 1.4, { fontSize: 13 });
  tag(s, "EVIDENCE", 11.58, 1.03);
}
{
  const s = content("Benchmark", "What the benchmark teaches");
  card(s, M, 1.4, 5.9, 3.0, "Lessons to adopt", ["Docker-based, scriptable SDK: the basis for CI and AI agents", "One signal engine shared by every UI technology (Data Engine)", "Versioned, public online documentation per release", "CODESYS as a first-class path for controls-engineer customers"], { kicker: "FROM CCPILOT", size: 14 });
  card(s, M + 6.2, 1.4, 5.9, 3.0, "Where M1 already wins", ["Price and power of an MCU display: no Linux to maintain", "Instant-on and simpler cyber surface than a full Linux image", "Configurator for no-code customers", "Small, sealed form factors for instrument panels"], { kicker: "M1 ADVANTAGE", size: 14 });
  s.addShape(pres.shapes.RECTANGLE, { x: M, y: 4.65, w: CW, h: 1.85, fill: { color: "FFF1E6" }, line: { color: HEX.rule, width: 0.75 } });
  s.addText([{ text: "WHITE SPACE   ", options: { bold: true, color: C.accent2, fontSize: 12 } },
    { text: "Nothing in M1 sits between a 4.3\" cluster and a 7\" Linux display computer. The same STM32H7 class already in the 280 can drive a 7\" 800×480 screen with external SDRAM (the AI Terra mower HMI reference design does exactly this). A 5\"–7\" MCU display with SDK 2.0 would compete on cost and boot time below CCpilot.", options: { fontSize: 15, color: C.text1 } }],
    { x: M + 0.25, y: 4.7, w: CW - 0.5, h: 1.75, margin: 0, valign: "middle", isTextBox: true });
  tag(s, "JUDGMENT", 11.58, 1.03);
}

// ============ Deficiencies ============
divider("Deficiencies", "Deficiencies: the panel debate", "A scorecard first, then five debates. Each ends with a verdict and a takeaway for the CTO.");
{
  const s = content("Deficiencies", "Deficiency scorecard");
  const SEV = { High: C.accent5, Medium: C.accent4, Low: C.accent3 };
  const rows = [
    ["High", "No SDK for the M1, the flagship marketed as fully customisable", "OEMs, sales"],
    ["High", "ST-SDK and Qt-SDK are incompatible builds; app code can't move between them", "Firmware devs"],
    ["High", "No simulator, host build or CI; every change means reflashing hardware", "Firmware devs"],
    ["High", "No secure, signed field update; no SBOM or vulnerability process documented", "OEMs, compliance"],
    ["Medium", "Locked MCAL on one STM32 part: no silicon second source", "Supply chain"],
    ["Medium", "Custom J1939/CANopen only via a tailored library from sales; no DBC import", "OEMs, sales"],
    ["Medium", "Windows-only manual setup; two compilers pinned to exact versions", "Firmware devs"],
    ["Medium", "Qt for MCUs licence left to the customer; no bundled option", "OEMs, procurement"],
    ["Medium", "No data logging or fleet path, although 280b/430v/430iv have Bluetooth", "Fleet managers"],
    ["Low", "Documentation gaps: Design Studio \"contact sales\", spec inconsistencies", "Everyone"],
  ];
  rows.forEach((r, i) => {
    const y = 1.35 + i * 0.53;
    pill(s, r[0].toUpperCase(), M, y + 0.08, 1.05, SEV[r[0]]);
    s.addText(r[1], { x: M + 1.25, y, w: 8.3, h: 0.46, fontSize: 14, color: C.text1, valign: "middle", margin: 0, isTextBox: true });
    s.addText(r[2], { x: 10.2, y, w: 2.55, h: 0.46, fontSize: 13, color: C.accent6, valign: "middle", margin: 0, isTextBox: true });
  });
  tag(s, "JUDGMENT", 11.58, 1.03);
}
debate("Deficiencies", "Debate A: one GUI path or two?", "Should M1 keep separate TouchGFX and Qt for MCUs SDKs, consolidate, or add LVGL?",
  [["GUI FIRMWARE LEAD", "Keep both, share the app", "Put a GUI bridge on top of the data layer: one display-ready model that every GUI front-end reads. TouchGFX and Qt for MCUs become thin views; customer logic is written once. The AI Terra reference firmware does this with one ui_model_t feeding both."],
   ["PRODUCT MANAGER", "Two paths sell", "TouchGFX is free on STM32 and wins price-led deals. Qt for MCUs wins teams that already use QML and Qt Design Studio. Dropping either loses customers."],
   ["DISSENT · EMBEDDED ARCHITECT", "Add LVGL as the neutral path", "TouchGFX ties us to ST; Qt for MCUs adds a licence. LVGL is MIT-licensed, runs on monochrome 130/200 screens and colour 280/430, and works on any future silicon."]],
  "One data layer plus GUI bridge; TouchGFX and Qt for MCUs as supported front-ends; prototype LVGL on the M1 and the monochrome models before committing.",
  "Customers keep their GUI tool; maximatecc maintains one application layer instead of two.");
debate("Deficiencies", "Debate B: silicon lock-in and the MCAL", "Is a CubeMX-generated MCAL that customers must not touch acceptable for a 10-year product line?",
  [["EMBEDDED ARCHITECT", "Add a platform layer now", "Wrap the MCAL behind a small platform API (CAN, UART, I2C, flash, display, GPIO). Everything above it stops including ST headers, so a second MCU vendor becomes a port, not a rewrite."],
   ["CTO OFFICE", "Supply risk is real", "The 2021–2023 chip shortage hit single-source display makers hardest. A second-source plan for the M7 tier protects revenue and OEM confidence."],
   ["DISSENT · PRODUCT MANAGER", "Don't port speculatively", "A second port doubles validation. Our customers buy a finished display, not a chip. Spend only when a customer or a shortage demands it."]],
  "Introduce the platform layer in SDK 2.0 (low cost, high option value); keep STM32H7 as the only validated port until a trigger appears.",
  "Cheap insurance: the abstraction costs weeks; a forced redesign in a shortage costs quarters.");
debate("Deficiencies", "Debate C: custom CAN through sales, or self-service?", "Every OEM has proprietary PGNs. Today a custom J1939 set or CANopen dictionary needs a tailored library from sales.",
  [["CAN / J1939 SPECIALIST", "Generate from DBC and J1939-DA", "Import the OEM's DBC or EDS, pick signals, and generate database.h fields, decoders and staleness timeouts. Standard PGNs (EEC1, ET1, DD1, DM1) should be one click."],
   ["OEM HMI ENGINEER", "The wait kills schedules", "A quote cycle for every signal change can cost weeks. We want to add a proprietary PGN on Friday and see it on screen on Monday."],
   ["DISSENT · PRODUCT MANAGER", "Tailored libraries are revenue", "Custom CAN work is billable and builds relationships. Self-service could cannibalise it."],
   ],
  "Self-service generation for standard J1939, CANopen and customer DBC/EDS files; paid services move up-value to validation, diagnostics (DM1/DM2 UI) and certification support.",
  "Faster design-ins win more of them; the service revenue moves rather than disappears.");
debate("Deficiencies", "Debate D: developer velocity and AI agents", "How fast can a customer go from a CAN signal list to a validated screen, and can AI coding agents help safely?",
  [["AI TOOLING LEAD", "Build for agents and CI", "Add a headless CLI build (Docker, Linux and Windows), a host simulator of the data layer and GUI, screenshot tests, and agent skills. QML and LVGL XML are text an agent can edit and a linter can check."],
   ["GUI FIRMWARE LEAD", "Proof exists", "The AI Terra mower HMI (same STM32H743, FreeRTOS, TouchGFX and Qt for MCUs combination) was built with Claude Code and is gated by 1,458 host tests that run without hardware."],
   ["DISSENT · OEM HMI ENGINEER", "Our teams live in CubeIDE", "Many OEM developers are Windows IDE users and won't adopt a CLI. AI-generated firmware also raises review and liability questions."]],
  "Keep the CubeIDE path; add CLI, simulator and tests underneath it; publish agent skills with guardrails (memory budget, frame time, licence checks).",
  "Measured by days from signal list to screen; this is the metric that wins design-ins.");
debate("Deficiencies", "Debate E: field updates, security and fleet data", "Displays now stay in service 10+ years and must be patchable. Today an update means a rebuild and a reflash.",
  [["SAFETY AND CYBER LEAD", "Secure update is mandatory", "Signed images, secure bootloader, rollback, SBOM and a vulnerability process. The EU Cyber Resilience Act applies to these products; main obligations start in December 2027."],
   ["PRODUCT MANAGER", "Bluetooth is already shipping", "The 280b, M1v and M1iv carry Bluetooth for configuration. The same link can deliver signed updates and pull logged fleet data to a phone app."],
   ["DISSENT · CTO OFFICE", "Security costs margin", "Secure boot and a vulnerability process need people and long-term budget for small, cheap displays."]],
  "Secure bootloader and signed USB/Bluetooth update in SDK 2.0; on-device log ring plus phone sync on Bluetooth models; SBOM per release.",
  "Not optional for the EU market, and the Bluetooth models turn it into a fleet-data feature customers pay for.");
{
  const s = content("Deficiencies", "Why QML and declarative UI matter here");
  card(s, M, 1.4, 5.9, 5.3, "QML in the M1 Qt-SDK", ["Qt for MCUs compiles a QML subset to C++ at build time: no JavaScript engine on the 280", "Bindings make screens reactive: a label bound to rpm updates when the data layer changes", "Text format: diffable, lintable, and well known to AI models", "Catch: commercial-only licence, and the QML can't be reused by the TouchGFX path"], { kicker: "TECHNICAL", size: 14 });
  card(s, M + 6.2, 1.4, 5.9, 5.3, "Implication for SDK 2.0", ["Treat a declarative screen description as the interchange format", "Generate QML for Qt for MCUs, TouchGFX code, or LVGL XML from one spec bound to the signal model", "Agents edit the spec; the simulator and tests gate it", "Non-safety screens only: warning telltales keep an independent, verified path (ISO 25119 / 13849 freedom from interference)"], { kicker: "STRATEGIC", size: 14, fill: "FFF1E6" });
}

// ============ Evolution ============
divider("Evolution", "Evolution: M1 SDK 2.0", "Target architecture, product tiers, priorities, metrics, growth case and roadmap");
{
  const s = content("Evolution", "Target architecture: M1 SDK 2.0");
  const x = M, w = 7.4;
  layer(s, x, 1.35, w, 0.62, "Customer application", "Written once, against the GUI bridge and data layer", "FFF1E6");
  layer(s, x, 2.03, 2.4, 0.78, "TouchGFX", "ST front-end", C.background2);
  layer(s, x + 2.5, 2.03, 2.4, 0.78, "Qt for MCUs", "QML front-end", C.background2);
  layer(s, x + 5.0, 2.03, 2.4, 0.78, "LVGL (pilot)", "Neutral front-end", C.background2);
  layer(s, x, 2.87, w, 0.62, "GUI bridge", "One display-ready model; front-ends only render", "E8F1EA");
  layer(s, x, 3.55, w, 0.62, "Signal model + data layer", "Generated from DBC / EDS / J1939-DA; staleness per signal", "E8F1EA");
  layer(s, x, 4.23, w, 0.62, "Services", "J1939 + CANopen, logging ring, secure update, Bluetooth link", C.background2);
  layer(s, x, 4.91, w, 0.62, "Platform layer (new)", "CAN, UART, I2C, flash, display, GPIO API; FreeRTOS via OS layer", "E8F1EA");
  layer(s, x, 5.59, w, 0.62, "Ports", "STM32H7 (validated) · second source on trigger", "0F1418", C.background1);
  card(s, 8.3, 1.35, 4.43, 4.86, "Tooling around it", ["CLI + Docker build (Linux and Windows), CubeIDE still supported", "Host simulator of data layer and screens", "Unit and screenshot tests in CI", "Agent skills and guardrails; MCP server for the signal model", "Signed release packages with SBOM"], { kicker: "DEVELOPER EXPERIENCE", size: 14 });
  s.addText("Green layers are new or extended in SDK 2.0.", { x: 8.3, y: 6.35, w: 4.4, h: 0.3, fontSize: 12, italic: true, color: C.accent6, margin: 0, isTextBox: true });
}
{
  const s = content("Evolution", "One SDK family, tiered across the range");
  const T = [
    ["No-code", "130 · 200 · all", "Configurator, extended with J1939 signal picker and templates", C.accent6],
    ["SDK Lite", "280 · 430", "Signal model, data layer, GUI bridge, simulator; TouchGFX or LVGL front-end; free download", C.accent3],
    ["SDK Pro", "280 · M1 · new 5\"–7\"", "Adds Qt for MCUs licence bundle, secure update, Bluetooth fleet sync, agent skills, priority support", C.accent2],
    ["Specialized", "Any", "maximatecc engineers: custom UI, CAN validation, diagnostics, certification support", C.text1],
  ];
  T.forEach((t, i) => {
    const y = 1.4 + i * 1.28, x = M + i * 0.35;
    s.addShape(pres.shapes.RECTANGLE, { x, y, w: CW - i * 0.35, h: 1.12, fill: { color: C.background2 }, line: { color: HEX.rule, width: 0.75 } });
    s.addShape(pres.shapes.OVAL, { x: x + 0.2, y: y + 0.26, w: 0.6, h: 0.6, fill: { color: t[3] }, line: { color: t[3] } });
    s.addText(String(i + 1), { x: x + 0.2, y: y + 0.26, w: 0.6, h: 0.6, fontSize: 18, bold: true, color: C.background1, align: "center", valign: "middle", margin: 0, isTextBox: true });
    s.addText([{ text: t[0], options: { bold: true, fontSize: 17, color: C.text1, breakLine: true } }, { text: t[1], options: { fontSize: 12, color: C.accent6 } }], { x: x + 1.0, y, w: 2.6, h: 1.12, valign: "middle", margin: 0, isTextBox: true });
    s.addText(t[2], { x: x + 3.7, y, w: CW - i * 0.35 - 3.9, h: 1.12, fontSize: 14, color: C.text2, valign: "middle", margin: 0, isTextBox: true });
  });
  tag(s, "JUDGMENT", 11.58, 1.03);
}
{
  const s = content("Evolution", "Priorities: value against effort");
  const gx = M + 0.6, gy = 1.45, gw = 7.4, gh = 5.0;
  s.addShape(pres.shapes.RECTANGLE, { x: gx, y: gy, w: gw, h: gh, fill: { color: C.background2 }, line: { color: HEX.rule, width: 0.75 } });
  s.addShape(pres.shapes.LINE, { x: gx + gw / 2, y: gy, w: 0, h: gh, line: { color: HEX.rule, width: 1, dashType: "dash" } });
  s.addShape(pres.shapes.LINE, { x: gx, y: gy + gh / 2, w: gw, h: 0, line: { color: HEX.rule, width: 1, dashType: "dash" } });
  s.addText("VALUE →", { x: M - 0.2, y: gy + gh / 2 - 0.2, w: 0.75, h: 0.4, fontSize: 11, bold: true, color: C.accent6, rotate: 270, margin: 0, isTextBox: true });
  s.addText("EFFORT →", { x: gx, y: gy + gh + 0.05, w: gw, h: 0.3, fontSize: 11, bold: true, color: C.accent6, align: "center", margin: 0, isTextBox: true });
  s.addText("Quick wins", { x: gx + 0.15, y: gy + 0.1, w: 3, h: 0.3, fontSize: 12, bold: true, color: C.accent3, margin: 0, isTextBox: true });
  s.addText("Strategic bets", { x: gx + gw / 2 + 0.15, y: gy + 0.1, w: 3, h: 0.3, fontSize: 12, bold: true, color: C.accent2, margin: 0, isTextBox: true });
  const pts = [
    ["1", 0.15, 0.85, "430 SDK on the 280 base"], ["2", 0.25, 0.75, "CLI + Docker build"], ["3", 0.35, 0.9, "DBC / J1939 signal generator"],
    ["4", 0.3, 0.6, "Docs and spec clean-up"], ["5", 0.6, 0.88, "Host simulator + tests"], ["6", 0.65, 0.8, "Secure update + SBOM"],
    ["7", 0.8, 0.72, "5\"–7\" MCU display"], ["8", 0.55, 0.62, "GUI bridge across front-ends"], ["9", 0.85, 0.45, "Second silicon port"], ["10", 0.45, 0.4, "LVGL pilot"],
  ];
  pts.forEach((p) => {
    const cx = gx + p[1] * gw - 0.22, cy = gy + (1 - p[2]) * gh - 0.22;
    circleNum(s, p[0], cx, cy, 0.44, p[2] > 0.7 ? C.accent1 : C.accent6);
  });
  s.addText(pts.map((p, i) => ({ text: `${p[0]}  ${p[3]}`, options: { fontSize: 14, color: C.text1, breakLine: i < pts.length - 1, paraSpaceAfter: 6 } })),
    { x: 8.85, y: 1.45, w: 3.9, h: 5.0, margin: 0, valign: "top", isTextBox: true });
  tag(s, "JUDGMENT", 11.58, 1.03);
}
{
  const s = content("Evolution", "How success is measured");
  table(s, ["Metric", "Today (from documentation)", "SDK 2.0 target (proposed)"], [
    ["Time from install to first screen on hardware", "Manual multi-tool setup on Windows", "Under 1 hour, one command"],
    ["Time to add a proprietary PGN to a screen", "Quote and tailored library from sales", "Same day, self-service"],
    ["Iteration loop (edit → see result)", "Rebuild and reflash", "Seconds, in the host simulator"],
    ["Automated tests per release", "None documented", "Unit + screenshot tests in CI on every change"],
    ["Display models with an SDK", "1 of 4 (280)", "280, M1 and the new 5\"–7\" model"],
    ["Field update", "USB reprogram", "Signed update over USB and Bluetooth, with rollback"],
  ], [4.4, 3.8, 3.93], 1.4, { fontSize: 14 });
  tag(s, "JUDGMENT", 11.58, 1.03);
}
{
  const s = content("Evolution", "The growth case for the CTO");
  const L = [
    ["Win more design-ins", "Faster time to first screen and self-service CAN shorten OEM evaluation, the stage where display suppliers win or lose."],
    ["Sell up the range", "An SDK on the M1 and a new 5\"–7\" MCU display lift average selling price and close the gap below Linux display computers."],
    ["Recurring revenue", "SDK Pro subscriptions, Qt licence bundles, secure-update and fleet-data services, specialised engineering."],
    ["Protect the base", "Secure updates and SBOMs keep EU sales open under the Cyber Resilience Act; a platform layer de-risks chip shortages."],
  ];
  const cw = (CW - 0.3) / 2;
  L.forEach((l, i) => card(s, M + (i % 2) * (cw + 0.3), 1.4 + Math.floor(i / 2) * 2.25, cw, 2.05, l[0], l[1], { size: 15, headSize: 18, headH: 0.45, fill: i === 0 ? "FFF1E6" : undefined }));
  s.addText("No revenue figures are given: they need maximatecc's own design-in, pricing and pipeline data. The first 90 days should produce that model.", { x: M, y: 6.0, w: CW, h: 0.6, fontSize: 13, italic: true, color: C.accent6, margin: 0, isTextBox: true });
  tag(s, "JUDGMENT", 11.58, 1.03);
}
{
  const s = content("Evolution", "Roadmap");
  const R = [
    ["0–6 months", "Foundation", ["430 SDK built on the 280 base", "CLI + Docker build; CubeIDE kept", "DBC / J1939-DA signal generator", "Docs and spec clean-up; public release notes", "Customer interviews and win/loss data"]],
    ["6–12 months", "SDK 2.0", ["GUI bridge; TouchGFX and Qt for MCUs on one app layer", "Host simulator, unit and screenshot tests", "Secure bootloader, signed USB and Bluetooth update, SBOM", "Agent skills and guardrails"]],
    ["12–24 months", "Expansion", ["5\"–7\" STM32H7 display with SDK Pro", "Fleet-data sync on Bluetooth models", "LVGL front-end if the pilot succeeds", "Second silicon port if a trigger appears"]],
  ];
  const cw = (CW - 2 * 0.3) / 3;
  s.addShape(pres.shapes.LINE, { x: M + 0.3, y: 1.62, w: CW - 0.6, h: 0, line: { color: C.accent1, width: 2 } });
  R.forEach((r, i) => {
    const x = M + i * (cw + 0.3);
    s.addShape(pres.shapes.OVAL, { x: x + 0.2, y: 1.47, w: 0.3, h: 0.3, fill: { color: C.accent1 }, line: { color: C.background1, width: 2 } });
    card(s, x, 2.0, cw, 4.7, r[0], r[2], { kicker: r[1].toUpperCase(), size: 14, headSize: 18, fill: i === 0 ? "FFF1E6" : undefined });
  });
}
{
  const s = content("Evolution", "Decisions requested from the CTO");
  const D = [
    ["Approve SDK 2.0 as a product", "Fund a dedicated SDK team, owning tooling, docs and releases, not just per-project engineering."],
    ["Close the M1 gap first", "Ship a documented M1 SDK on the 280 base within six months."],
    ["Open CAN self-service", "Move custom J1939/CANopen from quoted libraries to a signal generator; refocus services on validation and diagnostics."],
    ["Commit to secure update", "Signed updates and SBOM for all SDK models ahead of the Cyber Resilience Act deadlines."],
    ["Scope a 5\"–7\" MCU display", "Business case and prototype on STM32H7 to fill the gap below Linux display computers."],
  ];
  D.forEach((d, i) => {
    const y = 1.4 + i * 1.03;
    circleNum(s, i + 1, M, y + 0.12, 0.6);
    s.addText([{ text: d[0], options: { bold: true, fontSize: 17, color: C.text1, breakLine: true } }, { text: d[1], options: { fontSize: 14, color: C.text2 } }],
      { x: M + 0.85, y, w: 11.3, h: 0.9, margin: 0, valign: "middle", isTextBox: true });
  });
}

}
// ============ Wainbee meeting section ============
const WSEC = WB ? "Working session" : "Wainbee meeting";
function mark(s, kind) {
  if (WB) return;
  pill(s, kind === "show" ? "SHOW TO WAINBEE" : "INTERNAL ONLY", 10.98, 1.03, 1.75, kind === "show" ? C.accent3 : C.accent5);
}
if (WB) {
  pres.addSection({ title: WSEC });
  const s = pres.addSlide({ masterName: "TITLE_DARK", sectionTitle: WSEC });
  s.addText("Building M1 and 280 screens with Claude Code", { placeholder: "title" });
  s.addText("A working session with Wainbee: today's M1 SDK, how an AI coding agent fits the workflow, the guardrails, and a joint pilot proposal.", { placeholder: "body" });
} else {
  divider(WSEC, "Wainbee meeting: a Claude Code–ready M1", "Internal preparation, the panel's choice of slides, the slides to show, and the follow-up plan. Green-tagged slides are also exported as a separate customer deck.");
  {
    const s = content(WSEC, "Account brief: Wainbee");
    mark(s, "internal");
    card(s, M, 1.4, 5.9, 3.0, "Who they are", ["Large privately held Canadian distributor and integrator: 14 offices, 200+ employees, engineering staff coast to coast", "Mobile and industrial hydraulics, pneumatics, automation; acquired Brimotion (2024) and Hydraulics Plus (mining, forestry, construction)", "Also carries CrossControl controllers and Scanreco remote controls, plus WAGO and Parker"], { kicker: "EVIDENCE", kickerColor: C.accent3, size: 13 });
    card(s, M + 6.2, 1.4, 5.9, 3.0, "What they told us", ["They like the M1 and 280", "Their engineers want to work more with Claude Code", "Assumed: they build screens for several OEM customers, so faster development multiplies across projects"], { kicker: "FROM THE ACCOUNT · VERIFY THE ASSUMPTION", kickerColor: C.accent4, size: 13 });
    s.addShape(pres.shapes.RECTANGLE, { x: M, y: 4.65, w: CW, h: 1.9, fill: { color: "FFF1E6" }, line: { color: HEX.rule, width: 0.75 } });
    s.addText([{ text: "WHY IT MATTERS   ", options: { bold: true, color: C.accent2, fontSize: 12 } },
      { text: "Wainbee can sell CrossControl as well as M1. At the MCU display tier, M1 wins if it is the easiest display to build with an AI agent. A good pilot gives maximatecc a reference customer, real data for SDK 2.0 and a template for other integrators.", options: { fontSize: 15, color: C.text1 } }],
      { x: M + 0.25, y: 4.7, w: CW - 0.5, h: 1.8, margin: 0, valign: "middle", isTextBox: true });
  }
  {
    const s = content(WSEC, "Meeting objectives and what success looks like");
    mark(s, "internal");
    card(s, M, 1.4, 3.843, 4.6, "In the room", ["Show the Claude Code workflow live on a 280 project", "Hear their workflow: tools, OS, signal sources, review process", "Agree one pilot application and owners"], { kicker: "DURING", size: 14 });
    card(s, M + 4.143, 1.4, 3.843, 4.6, "Within 30 days", ["Pilot running on their bench with the agent pack", "First screen built from their signal list", "Two feedback sessions logged"], { kicker: "AFTER", size: 14 });
    card(s, M + 8.287, 1.4, 3.843, 4.6, "Measured", ["Hours from signal list to first screen", "Hours to add a proprietary PGN", "Defects caught by tests before hardware", "Wainbee decision: roll out to a customer project"], { kicker: "SUCCESS METRICS", size: 14, fill: "FFF1E6" });
  }
  debate(WSEC, "Panel debate: what to show Wainbee", "Which slides create the most value for Wainbee and maximatecc, and what stays internal?",
    [["AI TOOLING LEAD", "Lead with a live demo", "Ten minutes of Claude Code adding a real J1939 signal to a 280 screen beats any slide. Bring a prepared repo, a recorded fallback and a simulated CAN log so it works without a machine."],
     ["PRODUCT MANAGER", "Close on a pilot", "Show today's SDK honestly, then a concrete six-week pilot with named deliverables. The goal is a signed scope, not applause."],
     ["DISSENT · SAFETY AND CYBER LEAD", "Mind what we promise", "Keep the deficiency scorecard and competitor benchmark internal. Say clearly that agent-written code is for non-safety screens, reviewed by people, and that roadmap items are plans, not commitments."]],
    "Show seven slides plus a live demo: today's SDK, the agent workflow, an example session, guardrails, the pilot, the roadmap preview and next steps. Keep debates, scorecard and benchmark internal.",
    "A pilot with Wainbee is the cheapest way to prove SDK 2.0 demand with real numbers.");
  {
    const s = content(WSEC, "Slide selection: value against risk");
    mark(s, "internal");
    const G = C.accent3, A = C.accent4, R = C.accent5;
    table(s, ["Candidate slide", "Value to Wainbee", "Value to maximatecc", "Risk", "Decision"], [
      ["Today's M1 SDK (facts)", "High", "Medium", "Low", { text: "Show", bold: true, color: G }],
      ["Claude Code workflow on M1", "High", "High", "Low", { text: "Show", bold: true, color: G }],
      ["Example agent session (code)", "High", "High", "Low", { text: "Show + live demo", bold: true, color: G }],
      ["Guardrails: what the agent does and doesn't do", "High", "High", "Low", { text: "Show", bold: true, color: G }],
      ["Joint pilot proposal", "High", "High", "Medium: needs resources", { text: "Show", bold: true, color: G }],
      ["Roadmap preview", "Medium", "High", "Medium: read as commitments", { text: "Show, labelled as planned", bold: true, color: A }],
      ["Deficiency scorecard and debates", "Low", "Low", "High: undermines confidence", { text: "Internal", bold: true, color: R }],
      ["CCpilot benchmark", "Low", "Low", "High: Wainbee sells CrossControl too", { text: "Internal", bold: true, color: R }],
    ], [3.6, 1.9, 2.1, 2.6, 1.93], 1.4, { fontSize: 13 });
  }
  {
    const s = content(WSEC, "Run of show: 60 minutes");
    mark(s, "internal");
    const A = [
      ["0–5", "Introductions and goals", "Account lead"],
      ["5–15", "Their workflow: tools, OS, CAN sources, review, customers", "Wainbee engineers (we listen)"],
      ["15–20", "Today's M1 SDK", "Product manager"],
      ["20–35", "Live demo: Claude Code adds a J1939 signal to a 280 screen", "GUI firmware lead"],
      ["35–42", "Guardrails and where people stay in charge", "Safety and cyber lead"],
      ["42–52", "Pilot proposal and roadmap preview", "Product manager"],
      ["52–60", "Agree scope, owners and dates", "Account lead"],
    ];
    A.forEach((a, i) => {
      const y = 1.45 + i * 0.73;
      s.addText(a[0], { x: M, y, w: 1.1, h: 0.55, fontSize: 15, bold: true, color: C.accent2, valign: "middle", margin: 0, isTextBox: true });
      s.addShape(pres.shapes.RECTANGLE, { x: M + 1.2, y, w: 7.4, h: 0.55, fill: { color: i === 3 ? "FFF1E6" : C.background2 }, line: { color: HEX.rule, width: 0.75 } });
      s.addText(a[1], { x: M + 1.35, y, w: 7.1, h: 0.55, fontSize: 14, color: C.text1, valign: "middle", margin: 0, isTextBox: true });
      s.addText(a[2], { x: 9.4, y, w: 3.3, h: 0.55, fontSize: 13, color: C.accent6, valign: "middle", margin: 0, isTextBox: true });
    });
    s.addText("Demo kit: prepared 280 project, recorded fallback video, CAN log replay, a printed one-page agent guide to leave behind.", { x: M, y: 6.6, w: CW, h: 0.35, fontSize: 13, italic: true, color: C.accent6, margin: 0, isTextBox: true });
  }
}

// ---- customer-facing slides (also in the review deck, tagged SHOW) ----
{
  const s = content(WSEC, "What the M1 SDK gives you today");
  mark(s, "show");
  card(s, M, 1.4, 3.843, 4.9, "M1 280 SDK", ["STM32H743 · FreeRTOS", "Choice of GUI tool: TouchGFX or Qt for MCUs", "Data layer: screens read signals through GET_DL / SET_DL", "J1939 and CANopen stacks built in", "Starter, demo and hardware-check projects"], { kicker: "CODE", size: 14 });
  card(s, M + 4.143, 1.4, 3.843, 4.9, "M1", ["4.3\" 480×272, 1000 nit, camera input", "Two CAN, 5 inputs + 4–20 mA", "Bluetooth 4.2 on M1v / 430iv", "Configurator V2.3 today", "Qt for MCUs compatibility"], { kicker: "CONFIGURE", size: 14 });
  card(s, M + 8.287, 1.4, 3.843, 4.9, "Why it suits an AI agent", ["Plain C/C++ projects in STM32CubeIDE", "A clear data-layer boundary the agent can work against", "Text-based UI code (C++ and QML) an agent can read and edit", "Hardware checks already in the integration project"], { kicker: "FIT", size: 14, fill: "FFF1E6" });
}
{
  const s = content(WSEC, "Claude Code in the M1 workflow");
  mark(s, "show");
  const steps = [
    ["Signal list", "DBC file or J1939 PGN/SPN list from the OEM"],
    ["Claude Code + M1 pack", "Agent reads the project guide, data-layer rules and memory budget"],
    ["Generated change", "Data-layer fields, J1939 decode, screen bindings"],
    ["Checks", "Build, unit and screenshot tests on the PC"],
    ["Engineer review", "Diff review and approval in Git"],
    ["Target", "STM32CubeIDE build, flash the 280 or M1, test on the bench"],
  ];
  const bw = (CW - 5 * 0.25) / 6;
  steps.forEach((st, i) => {
    const x = M + i * (bw + 0.25);
    circleNum(s, i + 1, x + bw / 2 - 0.3, 1.45, 0.6, i === 1 ? C.accent1 : C.accent6);
    s.addShape(pres.shapes.RECTANGLE, { x, y: 2.25, w: bw, h: 2.0, fill: { color: i === 1 ? "FFF1E6" : C.background2 }, line: { color: HEX.rule, width: 0.75 } });
    s.addText([{ text: st[0], options: { bold: true, fontSize: 14, color: C.text1, breakLine: true } }, { text: st[1], options: { fontSize: 12, color: C.text2 } }],
      { x: x + 0.12, y: 2.35, w: bw - 0.24, h: 1.8, margin: 0, valign: "top", isTextBox: true });
    if (i < 5) s.addShape(pres.shapes.LINE, { x: x + bw + 0.02, y: 1.75, w: 0.21, h: 0, line: { color: C.accent1, width: 2, endArrowType: "triangle" } });
  });
  s.addText([{ text: "Available today: ", options: { bold: true, color: C.accent3 } }, { text: "steps 1, 5 and 6 with the current 280 SDK.  ", options: { color: C.text2 } },
             { text: "Built in the pilot: ", options: { bold: true, color: C.accent2 } }, { text: "the M1 agent pack (step 2), signal generation (step 3) and PC-side tests (step 4).", options: { color: C.text2 } }],
    { x: M, y: 4.55, w: CW, h: 0.8, fontSize: 15, margin: 0, valign: "top", isTextBox: true });
}
{
  const s = content(WSEC, "Example: one request, end to end");
  mark(s, "show");
  s.addText("ILLUSTRATIVE · FIELD AND FUNCTION NAMES ARE EXAMPLES", { x: M, y: 1.2, w: 8, h: 0.3, fontSize: 11, bold: true, color: C.accent4, charSpacing: 1, margin: 0, isTextBox: true });
  s.addText("> Add coolant temperature (J1939 PGN 65262 ET1, SPN 110) to the main screen. Show it in °C, turn the value red above 105 °C, show -- if the signal is older than 3 s.",
    { x: M, y: 1.6, w: CW, h: 0.85, fontFace: "Courier New", fontSize: 14, color: C.background1, fill: { color: C.text1 }, margin: 12, valign: "middle", isTextBox: true });
  const code = [
    "// database.h        new field",
    "FIELD_COOLANT_TEMP_C,",
    "",
    "// j1939 decode      ET1, byte 1, 1 °C/bit, offset -40",
    "case PGN_ET1:",
    "  SET_DL(FIELD_COOLANT_TEMP_C, raw[0] - 40);",
    "",
    "// MainView.cpp      screen binding",
    "int t = GET_DL(FIELD_COOLANT_TEMP_C);",
    "coolant.setColor(t > 105 ? RED : WHITE);",
  ];
  s.addText(code.join("\n"), { x: M, y: 2.7, w: 7.3, h: 3.9, fontFace: "Courier New", fontSize: 13, color: C.text1, fill: { color: C.background2 }, margin: 12, valign: "top", isTextBox: true });
  card(s, 8.2, 2.7, 4.53, 3.9, "What the engineer gets", ["A small diff across three files", "A PC test that decodes a recorded ET1 frame and checks 104 / 106 °C and the stale case", "A note of RAM and flash impact", "Nothing outside the data layer and UI is touched"], { size: 14, kicker: "RESULT" });
}
{
  const s = content(WSEC, "Guardrails: where people stay in charge");
  mark(s, "show");
  card(s, M, 1.4, 5.9, 4.4, "The agent does", ["Screens, layouts and data-layer bindings", "Decoders for standard J1939 PGNs and the OEM's DBC signals", "Unit and screenshot tests, documentation", "Checks against the RAM and flash budget (1 MB / 2 MB internal on the 280)"], { kicker: "IN SCOPE", kickerColor: C.accent3, size: 15 });
  card(s, M + 6.2, 1.4, 5.9, 4.4, "The agent does not", ["Change the MCAL or low-level drivers", "Own safety-relevant warnings: those stay on a reviewed, separately tested path", "Merge without an engineer's review and approval", "Change licensed components or licence terms"], { kicker: "OUT OF SCOPE", kickerColor: C.accent5, size: 15 });
  s.addText("Your code and signal lists stay in your repository. Teams choose their own Claude plan and data settings.", { x: M, y: 6.0, w: CW, h: 0.5, fontSize: 14, italic: true, color: C.accent6, margin: 0, isTextBox: true });
}
{
  const s = content(WSEC, "Proposed joint pilot: six weeks");
  mark(s, "show");
  card(s, M, 1.4, 3.843, 4.6, "maximatecc provides", ["M1 agent pack: project guide (CLAUDE.md), data-layer rules, memory budget checks", "Signal generator for J1939 and DBC", "PC test harness for the 280", "Early access to the M1 SDK", "Weekly engineering check-in"], { kicker: "MAXIMATECC", size: 14, fill: "FFF1E6" });
  card(s, M + 4.143, 1.4, 3.843, 4.6, "Wainbee provides", ["One real application on a 280 or 430", "Its signal list or DBC file", "Two engineers using Claude Code", "Bench hardware and CAN access", "Honest feedback after each sprint"], { kicker: "WAINBEE", size: 14 });
  card(s, M + 8.287, 1.4, 3.843, 4.6, "We measure", ["Hours from signal list to first screen", "Hours to add a proprietary PGN", "Defects caught on the PC before hardware", "Go / no-go for a customer project in week 6"], { kicker: "SUCCESS", size: 14 });
}
{
  const s = content(WSEC, "Where the M1 SDK is heading");
  mark(s, "show");
  s.addText("PLANNED · SUBJECT TO CONFIRMATION · NOT A COMMITMENT", { x: M, y: 1.2, w: 9, h: 0.3, fontSize: 11, bold: true, color: C.accent4, charSpacing: 1, margin: 0, isTextBox: true });
  const R = [
    ["Next", ["M1 SDK on the 280 base", "Command-line and Docker build next to CubeIDE", "Signal generator from DBC and J1939"]],
    ["Then", ["One application layer for TouchGFX and Qt for MCUs", "PC simulator with screenshot tests", "Signed updates over USB and Bluetooth"]],
    ["Later", ["Larger 5\"–7\" M1 display", "Fleet data from Bluetooth models", "Additional GUI options"]],
  ];
  const cw = (CW - 2 * 0.3) / 3;
  R.forEach((r, i) => card(s, M + i * (cw + 0.3), 1.65, cw, 4.6, r[0], r[1], { size: 15, headSize: 20, headH: 0.5, fill: i === 0 ? "FFF1E6" : undefined }));
  s.addText("Pilot feedback from Wainbee directly shapes the order.", { x: M, y: 6.4, w: CW, h: 0.4, fontSize: 14, italic: true, color: C.accent6, margin: 0, isTextBox: true });
}
{
  const s = content(WSEC, "Next steps");
  mark(s, "show");
  const N = [
    ["This meeting", "Agree the pilot application, the two engineers and the bench setup"],
    ["Week 1", "maximatecc delivers the agent pack and PC test harness; kick-off session"],
    ["Weeks 2–5", "Wainbee builds screens with Claude Code; weekly check-ins; metrics logged"],
    ["Week 6", "Joint review of results; decision on a customer project and M1 SDK early adoption"],
  ];
  N.forEach((n, i) => {
    const y = 1.5 + i * 1.2;
    circleNum(s, i + 1, M, y + 0.1, 0.65);
    s.addText([{ text: n[0], options: { bold: true, fontSize: 18, color: C.text1, breakLine: true } }, { text: n[1], options: { fontSize: 15, color: C.text2 } }],
      { x: M + 0.95, y, w: 11.2, h: 0.95, margin: 0, valign: "middle", isTextBox: true });
  });
}

if (!WB) {
  const s = content(WSEC, "After the meeting: follow-up plan");
  mark(s, "internal");
  table(s, ["When", "Action", "Owner"], [
    ["Within 24 hours", "Send the customer deck, demo recording and meeting notes; confirm pilot scope in writing", "Account lead"],
    ["Within 1 week", "Deliver the agent pack (CLAUDE.md, data-layer rules, memory checks), signal generator and PC test harness; kick-off", "GUI firmware lead"],
    ["Weekly", "30-minute check-in; log the three pilot metrics; capture friction points as SDK 2.0 backlog items", "Product manager"],
    ["Week 6", "Joint results review; go / no-go for a Wainbee customer project", "Account lead + product manager"],
    ["Week 8", "Internal readout to the CTO: pilot data feeds the SDK 2.0 investment decision", "CTO office"],
    ["Ongoing", "With Wainbee's consent, turn the pilot into a reference case for other integrators", "Marketing"],
  ], [2.2, 7.6, 2.33], 1.4, { fontSize: 14 });
}

if (!WB) {
// ============ Appendix ============
divider("Appendix", "Appendix", "Open questions and sources");
{
  const s = content("Appendix", "Open questions to verify with maximatecc");
  table(s, ["Item", "What the sources say", "Why it matters"], [
    ["M1 280 CAN bit rate", "Product page says \"5Mbps\"; J1939 runs at 250 or 500 kbit/s", "Likely a typo; confusing for buyers"],
    ["M1 output current", "Spec: high-side 0.5 A; FAQ: 350 mA", "Load design on the customer side"],
    ["M1 processor", "\"Arm Cortex-M7\"; part number not stated", "Decides whether the 280 SDK ports directly"],
    ["M1 Design Studio", "\"Contact maximatecc\" on downloads pages", "May already cover part of the gap"],
    ["280 SDK pages not read", "Project start (c2), platform services (c4), release notes", "May document flashing and task details"],
    ["CCpilot Qt 6.12.0", "From a search summary of the V1290 page", "Benchmark accuracy"],
    ["EU CRA dates", "Main obligations December 2027", "Timing of the secure-update work"],
  ], [3.0, 5.0, 4.13], 1.4, { fontSize: 13 });
  tag(s, "VERIFY", 11.58, 1.03);
}
const SRC2 = [
  ["maximatecc home", "https://www.maximatecc.com/"],
  ["M1 software solutions", "https://www.maximatecc.com/maxai-software-solutions/"],
  ["maximatecc downloads", "https://www.maximatecc.com/downloads/"],
  ["M1 CAN bus displays", "https://www.maximatecc.com/products/maxai-can-bus-displays/"],
  ["M1", "https://www.maximatecc.com/maxai-430/"],
  ["M1 280", "https://www.maximatecc.com/maxai-280/"],
  ["M1 130", "https://www.maximatecc.com/maxai-130/"],
  ["M1 200", "https://www.maximatecc.com/maxai-200"],
  ["M1 downloads", "https://www.maximatecc.com/maxai-430-downloads/"],
  ["M1 280 downloads", "https://www.maximatecc.com/maxai-280-downloads/"],
  ["M1 130 downloads", "https://www.maximatecc.com/maxai-130-downloads/"],
  ["280 SDK manual", "https://maximatecc.com/280-instrument-displays-SDK-manual/"],
  ["280 SDK: tools setup", "https://maximatecc.com/280-instrument-displays-SDK-manual/content/b1_280_sdk_tools_setup.html"],
  ["280 SDK: packages", "https://maximatecc.com/280-instrument-displays-SDK-manual/content/c1_280_sdk_packages.html"],
  ["280 SDK: architecture", "https://maximatecc.com/280-instrument-displays-SDK-manual/content/c3_280_sdk_architecture.html"],
  ["280 SDK: GUI design", "https://maximatecc.com/280-instrument-displays-SDK-manual/content/c5_280_sdk_GUI_design.html"],
  ["280 SDK: CAN communication", "https://maximatecc.com/280-instrument-displays-SDK-manual/content/c6_280_sdk_CAN_comm.html"],
  ["maximatecc knowledge center", "https://www.maximatecc.com/knowledge-center/"],
  ["About maximatecc", "https://www.maximatecc.com/about-maximatecc/"],
  ["CCpilot V1290", "https://crosscontrol.com/displays-computers/ccpilot-v1290/"],
  ["CCpilot V710", "https://crosscontrol.com/displays-computers/ccpilot-v710/"],
  ["CCpilot V1200", "https://crosscontrol.com/displays-computers/ccpilot-v1200/"],
  ["CC Linux Programmer's Guide", "https://crosscontrol.com/media/u4adsjjf/cc-linux-programmers-guide.pdf"],
  ["UX Designer 6.0: Docker", "https://crosscontrol.com/manual/UX-Designer-V6.0-Online-Documentation/content/getting_started_docker.html"],
  ["Docker Hub: crosscontrol SDK", "https://hub.docker.com/r/crosscontrol/x1200-sdk"],
  ["CrossControl brochure 2023", "https://cdn.asp.events/CLIENT_CloserSt_D86EA381_5056_B739_5482D50A1A831DDD/sites/ivt-expo-europe-2025/media/libraries/exhibitor-brochures/815590-crosscontrol-company-broschure-2023.pdf"],
  ["CrossControl KB: CODESYS and Data Engine", "https://crosscontrol.zohodesk.eu/portal/en/kb/articles/device-description-files-and-libraries-for-codesys"],
  ["Wainbee: CrossFire SX controller", "https://www.frasersdirectory.com/redwire/crossfire-sx-controller-from-wainbee-built-for-the-harshest-environments/"],
  ["Wainbee: Scanreco brand page", "https://wainbee.com/en/brands/scanreco"],
  ["Wainbee acquires Brimotion", "https://wainbee.com/en/blog/post/wainbee-strengthens-its-automation-focus-with-the-strategic-acquisition-of-brimotion-automation"],
  ["Wainbee: Hydraulics Plus (MNP)", "https://www.mnp.ca/fr/services/financement-dentreprises/transactions/wainbee-limited-hydraulics-plus"],
];
[[0, 16], [16, SRC2.length]].forEach(([a, b], k) => {
  const s = content("Appendix", `Sources ${k + 1}/2`);
  const items = SRC2.slice(a, b);
  const half = Math.ceil(items.length / 2);
  [items.slice(0, half), items.slice(half)].forEach((col, c) => {
    const runs = col.map((it, i) => ({ text: it[0], options: { hyperlink: { url: it[1] }, color: C.accent2, fontSize: 14, breakLine: i < col.length - 1, paraSpaceAfter: 8 } }));
    s.addText(runs, { x: M + c * 6.2, y: 1.4, w: 5.9, h: 5.3, margin: 0, valign: "top", isTextBox: true });
  });
});

}
(async () => {
  const out = require("path").join(__dirname, "../../docs", WB ? "wainbee/M1-Wainbee-Claude-Code-session.pptx" : "strategy/M1-SDK-review.pptx");
  await pres.writeFile({ fileName: out });
  await applyTheme(out, THEME);
  console.log("wrote", out);
})();
