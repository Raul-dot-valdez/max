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
pres.title = "M1 Graphics Framework Strategy";
pres.author = "AI Terra";
pres.subject = "Off-highway HMI graphics strategy for A99";
pres.theme = { headFontFace: THEME.headFontFace, bodyFontFace: THEME.bodyFontFace };
const C = pres.SchemeColor;

const W = 13.333, M = 0.6, CW = W - 2 * M;
const FOOT = "AI Terra · M1 graphics framework strategy · November 2026";

// ---------- layouts ----------
pres.defineSlideMaster({
  title: "TITLE_DARK",
  background: { color: C.text1 },
  objects: [
    { placeholder: { options: { name: "title", type: "title", x: M, y: 2.1, w: 11.2, h: 1.9, fontSize: 40, bold: true, color: C.background1, valign: "bottom", align: "left", margin: 0 }, text: "" } },
    { placeholder: { options: { name: "body", type: "body", x: M, y: 4.2, w: 10.5, h: 1.4, fontSize: 18, color: "B9C3CB", valign: "top", align: "left", margin: 0 }, text: "" } },
    { text: { text: "STRATEGY BRIEF · OFF-HIGHWAY COCKPIT HMI", options: { x: M, y: 1.4, w: 10, h: 0.4, fontSize: 12, bold: true, color: C.accent1, charSpacing: 2, margin: 0 } } },
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
pres.addSection({ title: "Opening" });
{
  const s = pres.addSlide({ masterName: "TITLE_DARK", sectionTitle: "Opening" });
  s.addText("Graphics framework strategy for the A99 M1 roadmap", { placeholder: "title" });
  s.addText("Market evidence, jobs to be done, OEM-tier segmentation, SDK SWOT, an expert-panel debate and five scenarios for hardware and firmware planning. Prepared November 2026 from public sources.", { placeholder: "body" });
  s.addNotes("Pre-read deck. Every claim carries a label: EVIDENCE (stated in a cited public source), JUDGMENT (expert-panel assessment, not measured), VERIFY (check before it drives a decision). Sources are listed at the end of the deck.");
}

// ============ 2. Bottom line ============
{
  const s = content("Opening", "Bottom line: frameworks coexist; value moves around them");
  card(s, M, 1.35, 5.9, 5.3, "What is happening",
    ["The framework war is ending in coexistence: Qt, CODESYS and LVGL each keep a segment; MCU-vendor tools and Slint sit at the edges",
     "AI coding tools such as Claude Code make the framework itself less of a moat",
     "OEMs will pay for what surrounds the framework: machine signals ready to bind, safety-certified rendering, fast simulators, update and cyber compliance, and an AI-ready toolchain"],
    { kicker: "MARKET", size: 15 });
  card(s, M + 6.2, 1.35, 5.9, 5.3, "What A99 should do",
    ["Position M1 as a framework-neutral HMI platform",
     "One signal layer, one safety island, one build/simulation/OTA pipeline",
     "Three supported runtimes: Qt 6, LVGL 9, CODESYS Visualization",
     "An agent interface so Claude Code and similar tools build on it safely",
     "Tiered hardware: the same stack from an MCU smart display to a multi-screen cockpit computer",
     { text: "Play primarily in the mid-to-high OEM band", bold: true }],
    { kicker: "RECOMMENDATION", size: 15, fill: "FFF1E6" });
  s.addNotes("The recommendation combines the framework analysis (first brief) with the OEM-tier segmentation (follow-up). The mid-to-high band means upper mid-size OEMs and roughly the second tier of large OEMs, global ranks 10 to 50.");
}

// ============ 3. Evidence legend + agenda ============
{
  const s = content("Opening", "How to read this deck");
  const items = [
    ["EVIDENCE", "Stated in a cited public source"],
    ["JUDGMENT", "Expert-panel assessment, not measured"],
    ["VERIFY", "Check before it drives a decision"],
  ];
  items.forEach((it, i) => {
    tag(s, it[0], M, 1.6 + i * 0.6);
    s.addText(it[1], { x: M + 1.35, y: 1.6 + i * 0.6, w: 4, h: 0.28, fontSize: 14, color: C.text1, margin: 0, valign: "middle", isTextBox: true });
  });
  s.addText("Market share by graphics framework for off-highway displays is not published. Shifts shown here come from vendor filings, partner programmes and product releases; get real share numbers from VDC Research or similar, and from a win/loss review of A99 design-ins.",
    { x: M, y: 3.6, w: 5.4, h: 1.6, fontSize: 13, color: C.text2, margin: 10, fill: { color: "FFF1E6" }, isTextBox: true, valign: "top" });
  const agenda = ["Market evidence", "Customer: jobs to be done and design thinking", "Expert panel debate", "Five scenarios for 2027–2032", "Where to play: large, mid-size, small OEMs", "SDK strategy SWOT", "Roadmap, risks and sources"];
  agenda.forEach((a, i) => {
    circleNum(s, i + 1, 6.9, 1.45 + i * 0.73, 0.5);
    s.addText(a, { x: 7.55, y: 1.45 + i * 0.73, w: 5.2, h: 0.5, fontSize: 16, color: C.text1, valign: "middle", margin: 0, isTextBox: true });
  });
}

// ============ Section: Market evidence ============
pres.addSection({ title: "Market evidence" });
{
  const s = pres.addSlide({ masterName: "SECTION_DARK", sectionTitle: "Market evidence" });
  s.addText("Market evidence", { placeholder: "title" });
  s.addText("Framework landscape, market size, A99 today, and the influence of AI coding tools", { placeholder: "body" });
}
{
  const s = content("Market evidence", "Framework landscape 1/2: the three runtimes in demand");
  table(s, ["Option", "Where it stands in 2026", "Strength off-highway", "Weakness off-highway"], [
    ["Qt 6 / Qt for MCUs", "Reference for rich Linux HMIs. Qt Group sales flat in 2025; distribution licence revenue expected flat or slightly down in 2026. Bought IAR (2025); MCU deals with Infineon and Nuvoton. Design Studio has an MCP-based agentic AI assistant (Claude, Gemini or OpenAI keys).", "Mature designer workflow. QML is text, so AI tools edit it well. Qt Safe Renderer certification basis includes ISO 25119. Existing CCpilot base.", "Per-developer and per-device cost more visible as development cost falls. Qt 5 end of support forces migrations. Complex licence terms."],
    ["CODESYS Visualization", "TargetVisu, WebVisu and CODESYS HMI share most functions. CODESYS calls itself the number one development system for mobile-machine ECUs, SIL2 certified for safety applications.", "Controls engineers already own the logic in IEC 61131-3. One tool for logic and screens. WebVisu gives browser access for service.", "Modest visual ceiling; modern UX patterns are hard. Ties HMI to PLC release cycles."],
    ["LVGL 9 + LVGL Pro", "MIT licence. Partner programmes with Renesas (LVGL Pro kits, Nov 2025), ST, TI and NXP (GUI Guider generates LVGL). LVGL Pro editor: XML, live preview, Figma sync, CLI.", "No runtime royalty. Small footprint on MCUs and MPUs. XML + CLI suits CI and AI agents.", "Thin safety-certification story. Smaller designer pool. Large multi-screen Linux apps less proven."],
  ], [2.0, 4.4, 2.85, 2.85], 1.4, { fontSize: 12 });
  tag(s, "EVIDENCE", 11.58, 1.03);
}
{
  const s = content("Market evidence", "Framework landscape 2/2: edge options and suites");
  table(s, ["Option", "Where it stands in 2026", "Strength off-highway", "Weakness off-highway"], [
    ["MCU vendor tools", "ST TouchGFX (free, 4.26 in Sept 2025), NXP GUI Guider (LVGL-based), Microchip Harmony Graphics. Free on each vendor's own chips.", "Zero licence cost. Tuned to each chip's 2D accelerators.", "Lock-in to one silicon vendor, the opposite of a multi-vendor display range."],
    ["Slint", "Rust declarative UI; runtime under 300 KiB RAM; Linux, QNX and MCUs; used in two-wheeler clusters; GPLv3 or commercial; courting Qt 5 migrations.", "Memory safety fits the Cyber Resilience Act direction. .slint markup suits AI tools.", "No off-highway references found. Small vendor."],
    ["Commercial HMI suites", "Crank Storyboard (now AMETEK), Kanzi (Rightware / ThunderSoft), Altia, Embedded Wizard. Strong in automotive and appliances.", "Designer-led workflows and code generation.", "Niche in off-highway. Ownership changes add roadmap risk."],
  ], [2.0, 4.4, 2.85, 2.85], 1.4, { fontSize: 13 });
  tag(s, "EVIDENCE", 11.58, 1.03);
  s.addText("Pattern: vendor-tied tools keep the GUI close to one silicon family; Qt, LVGL and Slint are positioned as portable across vendors.",
    { x: M, y: 5.75, w: CW, h: 0.6, fontSize: 14, italic: true, color: C.accent2, margin: 0, isTextBox: true });
}
{
  const s = content("Market evidence", "Tooling is the fast-growing slice of the HMI software market");
  s.addChart(pres.charts.BAR, [{ name: "US$ million", labels: ["2024 (GIA)", "2030 (GIA)", "2030 (MRFR)"], values: [240.5, 883.1, 1012.4] }], {
    x: M, y: 1.4, w: 7.2, h: 5.2, barDir: "col",
    showTitle: true, title: "Embedded GUI development software, US$ M", titleFontSize: 14, titleColor: "0F1418", titleFontFace: "+mn-lt",
    chartColors: ["FF7A1A"], showValue: true, dataLabelPosition: "outEnd", dataLabelFontSize: 12, dataLabelColor: "0F1418", dataLabelFontFace: "+mn-lt", dataLabelFormatCode: "#,##0",
    catAxisLabelColor: "5B6873", valAxisLabelColor: "5B6873", catAxisLabelFontSize: 12, valAxisLabelFontSize: 11, catAxisLabelFontFace: "+mn-lt", valAxisLabelFontFace: "+mn-lt",
    valGridLine: { color: "E3E8EC", size: 0.5 }, catGridLine: { style: "none" }, showLegend: false, valAxisMinVal: 0,
  });
  s.addText([{ text: "24–26 %", options: { fontSize: 44, bold: true, color: C.accent2, breakLine: true } }, { text: "CAGR forecast for embedded GUI development software, 2024–2030", options: { fontSize: 14, color: C.text2 } }],
    { x: 8.3, y: 1.6, w: 4.4, h: 1.7, margin: 0, valign: "top", isTextBox: true });
  s.addText([{ text: "US$ 7–13 B", options: { fontSize: 36, bold: true, color: C.text1, breakLine: true } }, { text: "broader HMI software and solutions market by 2030, at 5–10 % CAGR", options: { fontSize: 14, color: C.text2 } }],
    { x: 8.3, y: 3.5, w: 4.4, h: 1.6, margin: 0, valign: "top", isTextBox: true });
  s.addText("Runtime royalties are under pressure; tooling grows. Analyst scopes differ, so treat these as directional.", { x: 8.3, y: 5.3, w: 4.4, h: 0.9, fontSize: 13, italic: true, color: C.accent6, margin: 0, isTextBox: true });
  tag(s, "VERIFY", 8.3, 6.3);
  s.addNotes("Sources: Global Industry Analysts via GII/Yahoo Finance (US$240.5M in 2024 to US$883.1M by 2030, 24.2% CAGR); Market Research Future (US$1,012.4M by 2030, 25.9% CAGR 2022-2030); broader HMI estimates from Grand View, GIA, MRFR, Global Info Research.");
}
{
  const s = content("Market evidence", "A99 and CCpilot today");
  table(s, ["CCpilot model", "Processor", "Screen", "Frameworks"], [
    ["V1200", "NXP i.MX 8QuadXPlus", "12.1\"", "Qt 5.15+ open source, Qt 6 support; CODESYS 3.5"],
    ["X900", "Intel Atom x5-E3940", "—", "Qt 5.12+; CODESYS"],
    ["i.MX 8 family", "NXP i.MX 8", "7\" to 12\"", "Qt and CODESYS toolchains"],
    ["VI", "—", "3.5\"", "Compact display"],
  ], [2.2, 2.6, 1.4, 5.9], 1.4, { fontSize: 13 });
  tag(s, "EVIDENCE", 11.58, 1.03);
  card(s, M, 4.15, 5.9, 2.6, "Data Engine: seed of a neutral signal layer", "The Data Engine already exchanges signals between a CODESYS project and a Qt application. Extending it to LVGL and to a typed, exportable signal model is the core of the M1 SDK.", { size: 14, kicker: "ASSET", headH: 0.45 });
  card(s, M + 6.2, 4.15, 5.9, 2.6, "Open questions", ["Maxima, Turotest and CrossControl merged as A99 in 2012; CrossControl later moved to the Scanreco group (public dates conflict)", "maximatecc.com could not be read from this session; details come from crosscontrol.com and resellers", "No public reference to \"M1\": treated as your internal roadmap name"], { size: 13, kicker: "VERIFY", kickerColor: C.accent5, headH: 0.4 });
}
{
  const s = content("Market evidence", "Is Claude Code changing embedded graphics? Yes, early");
  card(s, M, 1.35, 5.9, 5.35, "What the evidence shows", [
    "LVGL community published an agent skill letting Claude Code or Cursor generate SquareLine Studio projects as LVGL widget trees, with a validator",
    "Qt Design Studio's assistant is agentic over MCP and turns Figma selections into QML",
    "LVGL's editor exposes code generation, Figma sync, validation and tests through a CLI",
    "Practitioners: AI-written LVGL code works but needs review for memory use; general assistants can invent hardware register details",
  ], { kicker: "EVIDENCE", kickerColor: C.accent3, size: 14 });
  card(s, M + 6.2, 1.35, 5.9, 5.35, "What it means", [
    "When an agent writes screens in hours, nobody pays for framework API familiarity",
    "Value moves to what agents cannot invent: correct machine signals, a simulator and tests, safety and cyber evidence, clear licensing",
    "Text UI formats (QML, LVGL XML, .slint) gain; binary project files lose",
    "Per-device royalties become the largest remaining HMI cost and come under pressure",
    "Proof point: the AI Terra mower HMI firmware was built with Claude Code behind interchangeable TouchGFX / Qt for MCUs backends, gated by 1,458 host tests",
  ], { kicker: "JUDGMENT", kickerColor: C.accent4, size: 14 });
}

// ============ Section: Customer ============
pres.addSection({ title: "Customer" });
{
  const s = pres.addSlide({ masterName: "SECTION_DARK", sectionTitle: "Customer" });
  s.addText("Jobs to be done and design thinking", { placeholder: "title" });
  s.addText("Eight customer roles; the innovation column is where A99 can win regardless of the customer's framework choice", { placeholder: "body" });
}
const JTBD = [
  ["OEM HMI engineer", "When a new variant is defined, I want its screens on the real display within days, so I can test with operators before design freeze.", "Board bring-up, porting and signal wiring eat weeks before the first screen.", "Pre-integrated runtimes; desktop simulator matching the target; agent skills from spec or Figma; signals bound from the DBC."],
  ["OEM controls engineer", "When I change machine logic in CODESYS, I want the display to follow, so I avoid two code bases in sync.", "Hand-maintained interfaces between PLC variables and the GUI.", "One typed signal model exported to CODESYS, Qt and LVGL (Data Engine, extended)."],
  ["Product manager", "When I plan a 10-year platform, I want no lock-in to one framework or chip, so I protect margin and supply.", "Royalties, end-of-life notices, single-source silicon.", "Framework-neutral app layer; tiered hardware on one SDK; licences bundled in the display price."],
  ["Safety assessor", "When the display shows warnings, I want evidence they render correctly, so I can sign off under ISO 25119 or ISO 13849.", "GUI stack too big to certify; telltales duplicated in separate hardware.", "Safety island that draws and CRC-checks telltales independently of the main UI framework."],
  ["Operator", "When I work a 12-hour shift in dust, sun and vibration, I want critical information at a glance and glove-friendly controls.", "Four displays per cab, glare, touch failing with gloves, clutter.", "Cockpit-level UX across displays; sunlight and glove-tuned hardware; context-aware screens via on-device AI."],
  ["Dealer technician", "When a machine is down, I want faults explained and fixes guided, so I fix it on the first visit.", "Raw fault codes, laptop tools, no history.", "On-display diagnostics assistant; WebVisu or phone access; fault history that survives power-off."],
  ["Fleet manager", "When I run a mixed fleet, I want usage, health and location data, so I plan service and bill jobs.", "Data stuck on each machine; separate telematics boxes.", "Display as edge gateway: logging, OTA, cloud connector, clear data ownership."],
  ["OEM compliance lead", "When the EU Machinery Regulation and CRA apply, I want SBOMs, vulnerability handling and secure updates, so I keep selling in Europe.", "Open-source stacks without a maintained SBOM; no secure update path.", "Maintained Yocto/Zephyr base with SBOM, CVE monitoring, secure boot and signed OTA as a service."],
];
[[0, 4], [4, 8]].forEach(([a, b], i) => {
  const s = content("Customer", `Jobs to be done ${i + 1}/2`);
  table(s, ["Who", "Job: when … I want … so I can …", "Pain today", "Innovation that does the job"], JTBD.slice(a, b), [1.9, 4.0, 2.7, 3.5], 1.4, { fontSize: 13 });
});
{
  const s = content("Customer", "Design thinking: empathy to a 48-hour HMI prototype");
  const steps = [
    ["Empathize", ["Ride-alongs with operators in 3 industries: in-cab video, glove and glare tests", "Shadow OEM HMI and controls teams through one variant release", "Interview dealers on the last 10 field failures"]],
    ["Define", ["OEMs do not buy a framework; they buy time to a validated screen on a certified machine", "The deciding owner is often the controls team, not the HMI team"]],
    ["Ideate", ["How might we show the first screen on real hardware on day one?", "…let CODESYS, Qt and LVGL teams share one signal model?", "…certify telltales without certifying the framework?", "…let an agent build screens within memory and safety budgets?"]],
    ["Prototype & test", ["48-hour HMI kit: DBC + Figma frame + agent skill → running screen on CCpilot, scored by OEM engineers", "A/B cockpit layout with operators: task time and errors", "Safety-island demo with corrupted GUI output"]],
  ];
  const cw = (CW - 3 * 0.3) / 4;
  s.addShape(pres.shapes.LINE, { x: M + 0.3, y: 1.675, w: 3 * (cw + 0.3), h: 0, line: { color: C.accent1, width: 2 } });
  steps.forEach((st, i) => {
    const x = M + i * (cw + 0.3);
    circleNum(s, i + 1, x, 1.4, 0.55);
    card(s, x, 2.1, cw, 4.6, st[0], st[1], { size: 13, headH: 0.4 });
  });
}

// ============ Section: Expert debate ============
pres.addSection({ title: "Expert debate" });
{
  const s = pres.addSlide({ masterName: "SECTION_DARK", sectionTitle: "Expert debate" });
  s.addText("Expert panel debate", { placeholder: "title" });
  s.addText("Six cross-functional panels: position, pros and cons, and the strongest dissent raised against each", { placeholder: "body" });
}
const PANELS = [
  ["Cockpit & vehicle architecture", "Machine architects, zonal E/E, 48 V", "Zonal layouts with central compute are coming; excavators carry up to four displays, consolidation will follow.", "Fewer boxes; one UX; lower harness cost", "Mixed-criticality software needs a hypervisor and safety partitioning", "Mid-size OEMs keep separate displays for a decade; the ECU-by-ECU market stays large."],
  ["Graphics & HMI frameworks", "Qt, LVGL, CODESYS, Slint specialists", "No single winner. Qt premium Linux, LVGL MCU and cost-down, CODESYS PLC-centric OEMs. One framework loses deals.", "Multi-runtime matches how customers buy", "Three runtimes triple validation and support", "Make LVGL the default and Qt a premium add-on, to escape royalties."],
  ["Functional safety & compliance", "ISO 25119, ISO 13849, IEC 61508, cyber", "Nobody certifies a full GUI framework. Put safety rendering on an independent island with output checks. Machinery Regulation (Jan 2027) and CRA make SBOM and secure updates mandatory.", "One safety concept for every framework", "Extra hardware cost; longer certification lead time", "Qt Safe Renderer already covers this; our own island duplicates it."],
  ["Silicon & hardware platform", "SoC/MCU selection, thermal, supply", "Three tiers on one SDK: MCU smart display, Linux display with NPU, cockpit computer with GPU and safety domain. Two silicon sources per tier.", "Covers $150–$3,000 price points with shared software", "MCU vendors push free GUI tools that lock customers to one chip", "An MCU tier competes with cheap commodity displays where A99 has little edge."],
  ["Business, licensing & channel", "Pricing, partners, procurement", "Royalty runtimes become the biggest visible HMI cost. Bundle licences in hardware; charge for SDK support, OTA and fleet services, safety evidence packs.", "Recurring revenue; one price for procurement", "Margin pressure if Qt royalties are absorbed", "Qt partner status opens doors at large OEMs; don't look like leaving Qt."],
  ["AI tooling & developer experience", "Agentic coding, CI, simulation", "Design the SDK for agents first: text UI formats, machine-readable signal model, simulator, CLI, screenshot tests, MCP server on the device.", "Days-to-first-screen becomes a sellable metric", "Generated code needs review against memory, timing, safety; liability", "OEM IT may block cloud AI; local or on-premise models must also work."],
];
[[0, 3], [3, 6]].forEach(([a, b], k) => {
  const s = content("Expert debate", `Panel positions ${k + 1}/2`);
  const cw = (CW - 2 * 0.3) / 3;
  PANELS.slice(a, b).forEach((p, i) => {
    const x = M + i * (cw + 0.3), y = 1.35, h = 5.4;
    s.addShape(pres.shapes.RECTANGLE, { x, y, w: cw, h, fill: { color: C.background2 }, line: { color: HEX.rule, width: 0.75 } });
    s.addText(p[0], { x: x + 0.2, y: y + 0.15, w: cw - 0.4, h: 0.45, fontSize: 16, bold: true, color: C.text1, margin: 0, isTextBox: true });
    s.addText(p[1], { x: x + 0.2, y: y + 0.6, w: cw - 0.4, h: 0.3, fontSize: 11, color: C.accent6, margin: 0, isTextBox: true });
    s.addText(p[2], { x: x + 0.2, y: y + 0.95, w: cw - 0.4, h: 1.65, fontSize: 13, color: C.text2, margin: 0, valign: "top", isTextBox: true });
    s.addText([{ text: "PRO  ", options: { bold: true, color: C.accent3, fontSize: 11 } }, { text: p[3], options: { fontSize: 13, color: C.text1 } }], { x: x + 0.2, y: y + 2.65, w: cw - 0.4, h: 0.8, margin: 0, valign: "top", isTextBox: true });
    s.addText([{ text: "CON  ", options: { bold: true, color: C.accent5, fontSize: 11 } }, { text: p[4], options: { fontSize: 13, color: C.text1 } }], { x: x + 0.2, y: y + 3.45, w: cw - 0.4, h: 0.8, margin: 0, valign: "top", isTextBox: true });
    s.addShape(pres.shapes.LINE, { x: x + 0.2, y: y + 4.3, w: cw - 0.4, h: 0, line: { color: HEX.rule, width: 0.75, dashType: "dash" } });
    s.addText([{ text: "DISSENT  ", options: { bold: true, color: C.accent4, fontSize: 11 } }, { text: p[5], options: { fontSize: 13, italic: true, color: C.accent6 } }], { x: x + 0.2, y: y + 4.38, w: cw - 0.4, h: 0.95, margin: 0, valign: "top", isTextBox: true });
  });
});

// ============ Section: Scenarios ============
pres.addSection({ title: "Scenarios" });
{
  const s = pres.addSlide({ masterName: "SECTION_DARK", sectionTitle: "Scenarios" });
  s.addText("Five scenarios for 2027–2032", { placeholder: "title" });
  s.addText("Not mutually exclusive: several will hold in different segments. Likelihood and impact are the panels' joint judgment.", { placeholder: "body" });
}
const SCEN = [
  ["Polyglot runtimes, one platform", 0.8, "High", 0.75, "High",
    "Qt, LVGL and CODESYS each keep a stable share in different segments. OEMs pick the display vendor whose platform runs their framework with the least integration work.",
    ["RFQs list \"Qt or LVGL\" instead of one framework", "Qt distribution revenue keeps flattening", "More silicon vendors sign LVGL partnerships"],
    ["Same i.MX 8M Plus / i.MX 93 boards qualified for all three runtimes", "GPU or 2D engine sized for Qt; LVGL runs on the same parts"],
    ["Yocto BSP with Qt 6 LTS, LVGL 9 and CODESYS TargetVisu as layers", "Data Engine v-next: one typed signal model exported to all three", "Common simulator and screenshot-test harness"]],
  ["Open source takes the mid-tier", 0.65, "Medium-high", 0.7, "High",
    "LVGL, and to a lesser degree Slint, become the default for 3.5\"–10\" displays and cost-down programmes. Qt keeps the premium tier. Cost pressure and Qt 5 end of support are the triggers.",
    ["Qt 5 migrations choose LVGL or Slint over Qt 6", "LVGL Pro adds a safety or LTS offering", "Price-led RFQs for instrument and secondary displays"],
    ["MCU smart-display tier: i.MX RT1170, STM32H7/U5 or RA8D1 class, no Linux", "MPU entry tier: i.MX 93 class with NPU"],
    ["Commercial LVGL support: LTS branch, CVE patches, SBOM", "Join LVGL Pro partner programme; contribute off-highway widgets (gauges, telltales, soft keys)", "Qt 5 → LVGL or Qt 6 migration service"]],
  ["AI-native HMI engineering", 0.75, "High", 0.9, "Very high",
    "Agents such as Claude Code write most HMI code from specs, Figma and signal databases. The framework matters less; the platform's guardrails, simulator, tests and machine-readable models decide the vendor. Biggest chance for M1 to stand out.",
    ["OEM job ads ask for AI-assisted HMI development", "Framework vendors ship MCP servers and agent skills (Qt has started)", "RFQs ask for days-to-first-screen"],
    ["Debug and agent port on every unit: MCP server on device exposing signals, screenshots, logs, safe test hooks", "RAM and flash headroom for on-device test agents"],
    ["M1 SDK: CLI, desktop simulator, golden-image tests, DBC → widget binding", "Agent skills and guardrails: memory budget, frame time, safety-zone rules, licence checks", "Works with cloud and on-premise models"]],
  ["Controls engineers own the screen", 0.5, "Medium", 0.55, "Medium",
    "In mid-size OEMs without HMI teams, CODESYS Visualization becomes good enough. The display is bought as a CODESYS target with I/O; WebVisu covers phones and service laptops.",
    ["CODESYS HMI and Visu releases close the UX gap", "More requests for displays that also run machine control", "Combined display-and-controller ECUs win tenders"],
    ["Display computers with more CAN, I/O and a safety PLC co-processor", "SIL2 / PL d certifiable variants"],
    ["First-class CODESYS device package with Visu, safety and the signal layer", "Visu widget library in A99 style", "Agent skills generating CODESYS Visu and IEC 61131-3 from the same spec"]],
  ["Software-defined machine cockpit", 0.55, "Medium (large OEMs high)", 0.85, "Very high",
    "Large OEMs move to zonal E/E, a central cockpit computer driving several displays, and OTA features. Regulation forces SBOM, secure updates and cyber processes. Display suppliers become cockpit-computer suppliers or commodity glass.",
    ["Top-10 OEMs tender a single cockpit domain controller", "Machinery Regulation and CRA audits start", "Autonomy and operator-assist need camera, radar and AI overlays"],
    ["Cockpit computer: i.MX 95-class SoC with safety domain and NPU, GMSL cameras, 2–4 display outputs", "Thin smart-glass displays over a video link"],
    ["Hypervisor or partitioned Linux + RTOS; framework apps in containers", "Independent safety island for telltales", "Signed OTA, SBOM and CVE service; fleet data and AI-assist pipeline"]],
];
{
  const s = content("Scenarios", "Scenario overview: likelihood and impact");
  const lx = M + 4.6, bw = 3.2;
  s.addText("LIKELIHOOD", { x: lx, y: 1.35, w: bw, h: 0.3, fontSize: 11, bold: true, color: C.accent6, margin: 0, charSpacing: 1, isTextBox: true });
  s.addText("IMPACT ON A99", { x: lx + bw + 0.5, y: 1.35, w: bw, h: 0.3, fontSize: 11, bold: true, color: C.accent6, margin: 0, charSpacing: 1, isTextBox: true });
  SCEN.forEach((sc, i) => {
    const y = 1.85 + i * 0.95;
    circleNum(s, i + 1, M, y, 0.6);
    s.addText(sc[0], { x: M + 0.8, y, w: 3.7, h: 0.6, fontSize: 16, bold: true, color: C.text1, valign: "middle", margin: 0, isTextBox: true });
    [[sc[1], sc[2], lx], [sc[3], sc[4], lx + bw + 0.5]].forEach(([v, label, x]) => {
      s.addShape(pres.shapes.RECTANGLE, { x, y: y + 0.1, w: bw, h: 0.16, fill: { color: "E3E8EC" }, line: { color: "E3E8EC" } });
      s.addShape(pres.shapes.RECTANGLE, { x, y: y + 0.1, w: bw * v, h: 0.16, fill: { color: C.accent1 }, line: { color: C.accent1 } });
      s.addText(label, { x, y: y + 0.3, w: bw, h: 0.3, fontSize: 12, color: C.text2, margin: 0, isTextBox: true });
    });
  });
  tag(s, "JUDGMENT", 11.58, 1.03);
}
SCEN.forEach((sc, i) => {
  const s = content("Scenarios", `Scenario ${i + 1}: ${sc[0]}`);
  circleNum(s, i + 1, M, 1.35, 0.9);
  s.addText(sc[5], { x: M + 1.15, y: 1.3, w: 8.0, h: 1.05, fontSize: 15, color: C.text1, margin: 0, valign: "middle", isTextBox: true });
  s.addText([{ text: "LIKELIHOOD", options: { bold: true, color: C.accent6, fontSize: 10, breakLine: true } }, { text: sc[2], options: { bold: true, color: C.text1, fontSize: 14, breakLine: true } }, { text: "IMPACT", options: { bold: true, color: C.accent6, fontSize: 10, breakLine: true } }, { text: sc[4], options: { bold: true, color: C.accent2, fontSize: 14 } }],
    { x: 10.0, y: 1.25, w: 2.75, h: 1.2, margin: 8, valign: "middle", fill: { color: C.background2 }, isTextBox: true });
  const cw = (CW - 2 * 0.3) / 3;
  [["Signposts to watch", sc[6]], ["Hardware implications", sc[7]], ["Firmware and M1 moves", sc[8]]].forEach(([h, b], k) => {
    card(s, M + k * (cw + 0.3), 2.75, cw, 3.6, h, b, { size: 14, fill: k === 2 ? "FFF1E6" : undefined });
  });
});

// ============ Section: Where to play ============
pres.addSection({ title: "Where to play" });
{
  const s = pres.addSlide({ masterName: "SECTION_DARK", sectionTitle: "Where to play" });
  s.addText("Where to play: large, mid-size and small OEMs", { placeholder: "title" });
  s.addText("The software-defined cockpit does not arrive at the same depth in every OEM tier", { placeholder: "body" });
}
{
  const s = content("Where to play", "The top of the market builds its own software");
  const stats = [["~61 %", "of top-50 construction-equipment sales held by the top 10 OEMs (KHL Yellow Table)"], ["~25 %", "held by Caterpillar and Komatsu alone"], ["US$ 247 B", "top-50 OEM sales in 2025, a record"]];
  stats.forEach((st, i) => {
    const x = M + i * 4.1;
    s.addText([{ text: st[0], options: { fontSize: 44, bold: true, color: C.accent2, breakLine: true } }, { text: st[1], options: { fontSize: 14, color: C.text2 } }],
      { x, y: 1.4, w: 3.8, h: 1.9, margin: 0, valign: "top", isTextBox: true });
  });
  card(s, M, 3.65, 5.9, 3.05, "Large OEMs insource cockpit software", "John Deere ships its own G5 display software on its G5 CommandCenter and Universal displays, licenses it to other displays, and updates it over the air. At this level the software is in-house; the supplier role shrinks to hardware, BSP and qualification.", { kicker: "EVIDENCE", kickerColor: C.accent3, size: 14, headH: 0.45 });
  card(s, M + 6.2, 3.65, 5.9, 3.05, "Mid and small OEMs buy CODESYS-ready displays", "Vendors aimed at mid-size and small OEMs compete on CODESYS readiness: Helios / Enovation OpenView S50 and S70, ifm ecomatDisplay, Intercontrol digsy HMI, TTControl. Trade press reports OEMs need more supplier help with application software.", { kicker: "EVIDENCE", kickerColor: C.accent3, size: 14, headH: 0.45 });
  s.addNotes("Yellow Table figures vary slightly between outlets (Caterpillar 15.7 to 16.8% share depending on edition and denominator). The 2026 edition covers 2025 results: top-50 sales about US$246.6B; XCMG third globally at about 5.8% share.");
}
{
  const s = content("Where to play", "Software-defined cockpit depth by OEM tier");
  table(s, ["Dimension", "Large OEM (global top ~10)", "Mid-size OEM", "Small / specialty OEM"], [
    ["Cockpit depth", "Full: central compute, multiple displays, OTA, app ecosystems, autonomy overlays", "Partial (SDV-lite): 1–2 smart displays, OTA, telematics, some apps", "Minimal: configurable display, CODESYS screens, optional telematics"],
    ["Who writes the HMI", "In-house software org, hundreds of engineers", "Small HMI team plus controls engineers; outsources peaks", "Controls engineer or distributor; rarely an HMI specialist"],
    ["Preferred stack", "Own platform on Qt, Android/Linux or web; Qt Safe Renderer or own safety island", "Qt or CODESYS, moving toward LVGL on cost-down programmes", "CODESYS Visualization; vendor templates"],
    ["Buys from a display supplier", "Hardware to spec (ODM), BSP, safety and environmental qualification", "Hardware + platform + SDK + support: a finished base", "Ready product with screens configured, via distributors"],
    ["Volume per platform", "High (10k–100k+ / year)", "Medium (1k–10k)", "Low (<1k)"],
    ["Price sensitivity / cycle", "High on hardware; 2–4 years, global sourcing", "Medium, values time-to-market; 12–24 months", "High, values simplicity; 3–12 months"],
    [{ text: "Fit for a M1 SDK", bold: true }, "Low as SDK, medium as hardware/BSP partner", { text: "High", bold: true, color: C.accent2, fill: "FFF1E6" }, "Medium, if packaged as templates and agent-built screens"],
  ], [2.3, 3.3, 3.3, 3.2], 1.35, { fontSize: 12 });
  tag(s, "JUDGMENT", 11.58, 1.03);
}
{
  const s = content("Where to play", "Panel debate on where to play");
  const D = [
    ["\"Go high-end\"", "Cockpit architecture panel", "The full cockpit, and the money, sits with large OEMs. A cockpit computer with a safety domain is the only way to avoid commodity glass.", "Large OEMs insource the software (Deere G5). What's left is ODM hardware at large-OEM purchasing margins, against Bosch, Continental and Danfoss."],
    ["\"Own the mid-to-high band\"", "Business and channel panel", "Mid and upper-mid OEMs want large-OEM features (OTA, multi-display, telematics, AI assist) but can't build the platform. Here an SDK is the product.", "Mid-size OEMs buy cautiously and volumes per design-in are modest, so sales cost per unit is high."],
    ["\"Don't ignore the long tail\"", "AI tooling panel", "AI agents give small OEMs custom screens without an HMI team. A CODESYS-ready display, templates and agent skills could reach hundreds of OEMs via distributors.", "CODESYS-first rivals (ifm, Intercontrol, OpenView, TTControl) already price for this tier. Enter only if the channel sells."],
    ["\"Watch China\"", "Silicon and hardware panel", "Fast-growing Chinese OEMs (XCMG ranks third globally) adopt software-defined cockpits fast and source locally, resetting price expectations.", "Western mid-size OEMs facing EU cyber and machinery rules may prefer a European supplier with compliance evidence."],
  ];
  const cw = (CW - 0.3) / 2, ch = 2.6;
  D.forEach((d, i) => {
    const x = M + (i % 2) * (cw + 0.3), y = 1.35 + Math.floor(i / 2) * (ch + 0.2);
    s.addShape(pres.shapes.RECTANGLE, { x, y, w: cw, h: ch, fill: { color: C.background2 }, line: { color: HEX.rule, width: 0.75 } });
    s.addText([{ text: d[0], options: { fontSize: 16, bold: true, color: C.text1 } }, { text: "   " + d[1], options: { fontSize: 11, color: C.accent6 } }], { x: x + 0.2, y: y + 0.12, w: cw - 0.4, h: 0.4, margin: 0, isTextBox: true });
    s.addText(d[2], { x: x + 0.2, y: y + 0.55, w: cw - 0.4, h: 1.05, fontSize: 13, color: C.text2, margin: 0, valign: "top", isTextBox: true });
    s.addText([{ text: "COUNTER  ", options: { bold: true, color: C.accent5, fontSize: 11 } }, { text: d[3], options: { fontSize: 13, italic: true, color: C.accent6 } }], { x: x + 0.2, y: y + 1.62, w: cw - 0.4, h: 0.9, margin: 0, valign: "top", isTextBox: true });
  });
}
{
  const s = content("Where to play", "Verdict: primary in the mid-to-high band");
  const V = [
    ["PRIMARY", "Mid-to-high", "Upper-mid OEMs and the second tier of large OEMs (global ranks ~10–50). Sell M1 as an SDV-lite platform: 1–3 displays, OTA, telematics, safety island, multi-framework SDK.", "FFF1E6"],
    ["SELECTIVE", "High end", "Bid for large-OEM programmes as hardware, BSP and safety-island partner where the OEM brings its own software. No full cockpit computer until Scenario 5 signposts move.", undefined],
    ["CHANNEL", "Low end", "CODESYS-ready display with templates and agent-built screens, sold by distributors. No custom engineering.", undefined],
  ];
  const cw = (CW - 2 * 0.3) / 3;
  V.forEach((v, i) => card(s, M + i * (cw + 0.3), 1.4, cw, 3.7, v[1], v[2], { kicker: v[0], size: 15, fill: v[3], headSize: 22, headH: 0.5 }));
  s.addText("Before this drives investment, build a sized model: OEMs per tier and segment, display units per year, and which tiers are open to an outside platform. Start from KHL Yellow Table data for construction and equivalent rankings for agriculture and material handling.",
    { x: M, y: 5.4, w: CW, h: 1.2, fontSize: 14, color: C.text2, margin: 12, fill: { color: C.background2 }, valign: "middle", isTextBox: true });
}

// ============ Section: SDK SWOT ============
pres.addSection({ title: "SDK SWOT" });
{
  const s = pres.addSlide({ masterName: "SECTION_DARK", sectionTitle: "SDK SWOT" });
  s.addText("SWOT: A99 SDK strategy", { placeholder: "title" });
  s.addText("The recommended framework-neutral M1 SDK, four alternative strategies, and moves that combine the quadrants", { placeholder: "body" });
}
{
  const s = content("SDK SWOT", "SWOT of the framework-neutral M1 SDK");
  const Q = [
    ["S", "Strengths", C.accent3, ["Installed CCpilot base with Qt and CODESYS already supported", "Data Engine already bridges CODESYS and Qt", "Rugged off-highway hardware know-how, long lifecycles", "i.MX 8 family from 7\" to 12\" shares one software base"]],
    ["W", "Weaknesses", C.accent4, ["Small software org vs Qt Group, CODESYS, Tier-1s", "Three runtimes multiply validation and support", "No published safety-island / certified rendering offer", "Unclear brand: A99, CrossControl, Scanreco", "Thin cloud, telematics and AI tooling today"]],
    ["O", "Opportunities", C.accent2, ["Qt 5 end of support forces every Qt 5 customer to re-decide", "Mid-size OEMs want SDV-lite without building a platform", "AI-agent workflows reward simulators and signal models", "Machinery Regulation and CRA: compliance as a service", "LVGL partner programmes; NPUs on newer i.MX parts"]],
    ["T", "Threats", C.accent5, ["Large OEMs insource cockpit software (Deere G5 model)", "Free MCU-vendor GUI tools tied to their chips", "CODESYS-first rivals: ifm, Intercontrol, OpenView, TTControl", "Qt moving into tooling and MCUs; could bypass display makers", "Lower-cost Asian displays; AI lowers entry barriers; liability for AI code"]],
  ];
  const cw = (CW - 0.25) / 2, ch = 2.62;
  Q.forEach((q, i) => {
    const x = M + (i % 2) * (cw + 0.25), y = 1.35 + Math.floor(i / 2) * (ch + 0.2);
    s.addShape(pres.shapes.RECTANGLE, { x, y, w: cw, h: ch, fill: { color: C.background2 }, line: { color: HEX.rule, width: 0.75 } });
    s.addShape(pres.shapes.OVAL, { x: x + 0.2, y: y + 0.18, w: 0.5, h: 0.5, fill: { color: q[2] }, line: { color: q[2] } });
    s.addText(q[0], { x: x + 0.2, y: y + 0.18, w: 0.5, h: 0.5, fontSize: 18, bold: true, color: C.background1, align: "center", valign: "middle", margin: 0, isTextBox: true });
    s.addText(q[1], { x: x + 0.85, y: y + 0.18, w: cw - 1.1, h: 0.5, fontSize: 18, bold: true, color: C.text1, valign: "middle", margin: 0, isTextBox: true });
    s.addText(bullets(q[3], 13), { x: x + 0.25, y: y + 0.8, w: cw - 0.45, h: ch - 0.9, color: C.text2, margin: 0, valign: "top", isTextBox: true });
  });
}
{
  const s = content("SDK SWOT", "Four SDK strategies compared");
  table(s, ["Strategy", "Strength", "Weakness", "Opportunity", "Threat"], [
    ["A · Qt-centric SDK", "Continuity for today's customers; premium UX; Qt Safe Renderer path", "Royalty cost stays in the BOM; depends on Qt pricing", "Qt 5 → Qt 6 migrations; Qt partner co-selling", "Customers moving to LVGL or Slint on cost; Qt selling direct"],
    [{ text: "B · Framework-neutral M1 SDK (recommended)", bold: true, fill: "FFF1E6" }, { text: "Wins whatever the customer picks; value in signal model, safety, tooling", fill: "FFF1E6" }, { text: "Highest engineering and support load", fill: "FFF1E6" }, { text: "AI-native engineering; mid-tier SDV-lite demand", fill: "FFF1E6" }, { text: "Spread too thin; partner frictions", fill: "FFF1E6" }],
    ["C · CODESYS-first SDK", "Matches how small and mid OEMs work; one tool for logic and screens", "Weak at premium UX; hard to stand out", "Display-plus-controller products; SIL2 path", "Crowded: ifm, Intercontrol, OpenView, TTControl"],
    ["D · LVGL open-core SDK", "No runtime royalty; MCU-to-MPU portability; community", "Thin safety story; fewer HMI designers", "Cost-down programmes; LTS, CVE, SBOM as paid service", "MCU vendors bundle LVGL free (GUI Guider)"],
  ], [2.5, 2.45, 2.3, 2.45, 2.4], 1.4, { fontSize: 13 });
}
{
  const s = content("SDK SWOT", "Combining the quadrants into moves");
  const T = [
    ["Strengths × Opportunities", ["Package the Qt 5 exit as a programme: assess, migrate to Qt 6 or LVGL on one signal layer, deliver on CCpilot hardware", "Grow Data Engine into the M1 signal model with DBC import and agent skills"]],
    ["Weaknesses × Opportunities", ["Partner rather than build: Qt Safe Renderer or a silicon safety domain for telltales; LVGL Pro for the editor", "Use AI agents internally to multiply a small team, gated by the same tests sold to customers"]],
    ["Strengths × Threats", ["Against insourcing, sell large OEMs rugged hardware, BSP and safety qualification they don't want to build", "Against CODESYS-first rivals, offer CODESYS parity plus a Qt or LVGL upgrade path on the same hardware"]],
    ["Weaknesses × Threats", ["Limit validated combinations (3 runtimes × 2 SoC families, LTS only)", "Publish rules for AI-generated code: review gates, test evidence, liability terms", "Settle the brand story before launching M1 publicly"]],
  ];
  const cw = (CW - 0.25) / 2, ch = 2.62;
  T.forEach((t, i) => card(s, M + (i % 2) * (cw + 0.25), 1.35 + Math.floor(i / 2) * (ch + 0.2), cw, ch, t[0], t[1], { size: 14 }));
}

// ============ Section: Roadmap ============
pres.addSection({ title: "Roadmap" });
{
  const s = pres.addSlide({ masterName: "SECTION_DARK", sectionTitle: "Roadmap" });
  s.addText("M1 roadmap moves", { placeholder: "title" });
  s.addText("No-regret moves that pay off in every scenario come first; scenario bets follow once signposts move", { placeholder: "body" });
}
{
  const s = content("Roadmap", "Roadmap: from validation to platform");
  const R = [
    ["Next 90 days", ["Win/loss review of the last 30 design-ins by framework", "Buy independent framework share data", "15 jobs-to-be-done interviews: HMI, controls, safety, dealer", "Build the 48-hour HMI prototype on CCpilot"]],
    ["12 months", ["M1 SDK 1.0: signal model, simulator, CLI, screenshot tests", "Qt 6 LTS and LVGL 9 validated on i.MX 8/9", "Published agent skills and guardrails", "SBOM, CVE and signed OTA service"]],
    ["24 months", ["Safety island with telltale rendering and CRC checking", "MCU smart-display tier on the same SDK", "MCP server on the device", "CODESYS device package at parity with Qt"]],
    ["36 months +", ["Cockpit computer for multiple displays, if Scenario 5 signposts move", "Edge-AI assist (diagnostics, operator guidance) on the NPU", "Fleet data products with OEM revenue share"]],
  ];
  const cw = (CW - 3 * 0.3) / 4;
  s.addShape(pres.shapes.LINE, { x: M + 0.3, y: 1.72, w: CW - 0.6, h: 0, line: { color: C.accent1, width: 2 } });
  R.forEach((r, i) => {
    const x = M + i * (cw + 0.3);
    s.addShape(pres.shapes.OVAL, { x: x + 0.2, y: 1.57, w: 0.3, h: 0.3, fill: { color: C.accent1 }, line: { color: C.background1, width: 2 } });
    card(s, x, 2.1, cw, 4.6, r[0], r[1], { size: 14, headSize: 18, fill: i === 0 ? "FFF1E6" : undefined });
  });
}
{
  const s = content("Roadmap", "Main risks and mitigations");
  table(s, ["Risk", "Why it matters", "Mitigation"], [
    ["Validation capacity", "Three runtimes × several SoCs multiply test and support work", "One shared test harness; limit validated combinations; LTS releases only"],
    ["Qt partner relations", "Promoting LVGL can look like leaving Qt at large OEMs", "Position as customer choice, not replacement; keep Qt premium tier"],
    ["Liability for AI-generated code", "Defects in agent-written HMI code reach machines in the field", "Guardrails, review gates, test evidence, clear contract terms"],
    ["Silicon allocation", "Shortages hit single-source tiers first", "Dual-source each hardware tier where possible"],
    ["Large-OEM insourcing", "Software value at the top is captured in-house", "Play hardware/BSP/safety there; focus SDK on mid-to-high band"],
  ], [2.6, 4.5, 5.0], 1.4, { fontSize: 14 });
}

// ============ Sources ============
pres.addSection({ title: "Sources" });
const SRC = [
  ["CrossControl: CCpilot V1200", "https://crosscontrol.com/displays-computers/ccpilot-v1200/"],
  ["CrossControl: CCpilot X900", "https://crosscontrol.com/displays-computers/ccpilot-x900/"],
  ["CrossControl: i.MX 8 displays 7\"–12\"", "https://crosscontrol.com/newsroom/news/imx8-based-displays-from-7-12/"],
  ["CrossControl KB: CODESYS libraries and Data Engine", "https://crosscontrol.zohodesk.eu/portal/en/kb/articles/device-description-files-and-libraries-for-codesys"],
  ["OEM Off-Highway: A99 formed", "https://www.oemoffhighway.com/electronics/press-release/10875917/maxima-technologies-turotest-and-crosscontrol-form-vehicle-electronics-provider-maximatecc"],
  ["KHL: Scanreco to acquire CrossControl", "https://www.khl.com/news/scanreco-set-to-acquire-crosscontrol/8121115.article"],
  ["Qt Group annual report 2025", "https://www.qt.io/hubfs/_website/QtV2/QtV2%20Investors/2025/Qt_AnnualReport_2025-EN.pdf"],
  ["Qt Group Q4 2025 earnings summary", "https://quartr.com/events/qt-group-qtcom-q4-2025_3YEx7gfe"],
  ["Qt Design Studio AI assistant", "https://www.qt.io/blog/ai-assisted-design-in-qt-design-studio"],
  ["Qt Design Studio Figma MCP tutorial", "https://doc.qt.io/qtdesignstudio/figma-mcp-tutorial.html"],
  ["Qt functional safety", "https://www.qt.io/functional-safety-and-qt"],
  ["Slint: Qt 5 end of support", "https://slint.dev/qt-5-end-of-support"],
  ["Slint use cases", "https://slint.dev/use-cases"],
  ["Renesas + LVGL", "https://www.renesas.com/en/blogs/renesas-joins-forces-lvgl-empower-embedded-ui-innovation-open-source"],
  ["LVGL editor", "https://lvgl.io/editor"],
  ["LVGL forum: AI skill for Claude Code", "https://forum.lvgl.io/t/tool-ai-skill-that-generates-complete-squareline-studio-projects-to-export-as-lvgl-widget-trees/24322"],
  ["AI coding in embedded: pitfalls", "https://industrialmonitordirect.com/blogs/knowledgebase/ai-coding-in-embedded-development-practical-use-cases-and-pitfalls"],
  ["ST: TouchGFX 4.26", "https://blog.st.com/?p=25706"],
  ["NXP: GUI Guider", "https://www.nxp.com/company/about-nxp/smarter-world-blog/BL-HOW-GUI-GUIDER-IMPROVED-EMBEDDED-INTERFACE-DEV"],
  ["CODESYS Visualization", "https://www.codesys.com/products/visualization/"],
  ["CODESYS mobile automation", "https://codesys.com/industries/mobile-automation/page/2.html"],
  ["AMETEK acquires Crank Software", "https://www.cranksoftware.com/resources/blogs/AMETEK-acquires-Crank-Software"],
  ["Embedded GUI software market (GIA)", "https://finance.yahoo.com/news/embedded-graphical-user-interface-gui-150000555.html"],
  ["Market Research Future: embedded GUI", "https://marketresearchfuture.com/reports/embedded-graphical-user-interface-gui-development-software-market/market-size"],
  ["Yellow Table 2025 in 4 charts", "https://www.constructionbriefing.com/news/yellow-table-2025-in-4-charts-market-share-cagr-and-regional-power-shift/8062551.article"],
  ["Equipment World: largest OEMs 2025", "https://www.equipmentworld.com/market-pulse/article/15827092/worlds-largest-construction-equipment-manufacturers-in-2025"],
  ["XCMG in 2026 Yellow Table", "https://www.aap.com.au/aapreleases/cision20260503ae49764/"],
  ["Deere G5 CommandCenter", "https://www.deere.com/en/technology-products/precision-ag-technology/guidance/g5-commandcenter/"],
  ["Deere display updates", "https://www.deere.com/en/technology-products/precision-ag-technology/guidance/display-updates"],
  ["OpenView S50/S70 CODESYS", "https://enovationcontrols.com/codesys-is-now-available-on-openview-s50-s70/"],
  ["ifm ecomatDisplay", "https://www.ifm.com/it/en/shared/productnews/2022/ecomatdisplay-powerful-robust-hmis-for-mobile-machines"],
  ["Intercontrol graphics creation", "https://www.intercontrol.de/en/off-highway-electronics/products/software/software/graphics-creation"],
  ["KHL: HMI and telematics trends", "https://www.khl.com/news/key-trends-in-hmi-and-telematics-for-off-highway-equipment/8038667.article"],
  ["Avnet: zonal off-highway architectures", "https://www.avnet.com/americas/resources/article/zonal-architectures-in-off-highway-vehicles/"],
];
[[0, 17], [17, SRC.length]].forEach(([a, b], k) => {
  const s = content("Sources", `Sources ${k + 1}/2`);
  const items = SRC.slice(a, b);
  const half = Math.ceil(items.length / 2);
  [items.slice(0, half), items.slice(half)].forEach((col, c) => {
    const runs = col.map((it, i) => ({ text: it[0], options: { hyperlink: { url: it[1] }, color: C.accent2, fontSize: 13, breakLine: i < col.length - 1, paraSpaceAfter: 6 } }));
    s.addText(runs, { x: M + c * 6.2, y: 1.4, w: 5.9, h: 5.3, margin: 0, valign: "top", isTextBox: true });
  });
});

(async () => {
  const out = require("path").join(__dirname, "../../docs/strategy/M1-graphics-strategy.pptx");
  await pres.writeFile({ fileName: out });
  await applyTheme(out, THEME);
  console.log("wrote", out);
})();
