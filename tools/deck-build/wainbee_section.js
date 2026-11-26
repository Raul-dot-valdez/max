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
    card(s, M, 1.4, 3.9, 4.6, "In the room", ["Show the Claude Code workflow live on a 280 project", "Hear their workflow: tools, OS, signal sources, review process", "Agree one pilot application and owners"], { kicker: "DURING", size: 14 });
    card(s, M + 4.2, 1.4, 3.9, 4.6, "Within 30 days", ["Pilot running on their bench with the agent pack", "First screen built from their signal list", "Two feedback sessions logged"], { kicker: "AFTER", size: 14 });
    card(s, M + 8.4, 1.4, 3.9, 4.6, "Measured", ["Hours from signal list to first screen", "Hours to add a proprietary PGN", "Defects caught by tests before hardware", "Wainbee decision: roll out to a customer project"], { kicker: "SUCCESS METRICS", size: 14, fill: "FFF1E6" });
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
  card(s, M, 1.4, 3.9, 4.9, "M1 280 SDK", ["STM32H743 · FreeRTOS", "Choice of GUI tool: TouchGFX or Qt for MCUs", "Data layer: screens read signals through GET_DL / SET_DL", "J1939 and CANopen stacks built in", "Starter, demo and hardware-check projects"], { kicker: "CODE", size: 14 });
  card(s, M + 4.2, 1.4, 3.9, 4.9, "M1", ["4.3\" 480×272, 1000 nit, camera input", "Two CAN, 5 inputs + 4–20 mA", "Bluetooth 4.2 on M1v / 430iv", "Configurator V2.3 today", "Qt for MCUs compatibility"], { kicker: "CONFIGURE", size: 14 });
  card(s, M + 8.4, 1.4, 3.9, 4.9, "Why it suits an AI agent", ["Plain C/C++ projects in STM32CubeIDE", "A clear data-layer boundary the agent can work against", "Text-based UI code (C++ and QML) an agent can read and edit", "Hardware checks already in the integration project"], { kicker: "FIT", size: 14, fill: "FFF1E6" });
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
    s.addShape(pres.shapes.RECTANGLE, { x, y: 2.25, w: bw, h: 2.6, fill: { color: i === 1 ? "FFF1E6" : C.background2 }, line: { color: HEX.rule, width: 0.75 } });
    s.addText([{ text: st[0], options: { bold: true, fontSize: 14, color: C.text1, breakLine: true } }, { text: st[1], options: { fontSize: 12, color: C.text2 } }],
      { x: x + 0.12, y: 2.35, w: bw - 0.24, h: 2.4, margin: 0, valign: "top", isTextBox: true });
    if (i < 5) s.addShape(pres.shapes.LINE, { x: x + bw + 0.02, y: 1.75, w: 0.21, h: 0, line: { color: C.accent1, width: 2, endArrowType: "triangle" } });
  });
  s.addText([{ text: "Available today: ", options: { bold: true, color: C.accent3 } }, { text: "steps 1, 5 and 6 with the current 280 SDK.  ", options: { color: C.text2 } },
             { text: "Built in the pilot: ", options: { bold: true, color: C.accent2 } }, { text: "the M1 agent pack (step 2), signal generation (step 3) and PC-side tests (step 4).", options: { color: C.text2 } }],
    { x: M, y: 5.15, w: CW, h: 0.8, fontSize: 14, margin: 0, valign: "top", isTextBox: true });
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
  card(s, M, 1.4, 3.9, 4.6, "maximatecc provides", ["M1 agent pack: project guide (CLAUDE.md), data-layer rules, memory budget checks", "Signal generator for J1939 and DBC", "PC test harness for the 280", "Early access to the M1 SDK", "Weekly engineering check-in"], { kicker: "MAXIMATECC", size: 14, fill: "FFF1E6" });
  card(s, M + 4.2, 1.4, 3.9, 4.6, "Wainbee provides", ["One real application on a 280 or 430", "Its signal list or DBC file", "Two engineers using Claude Code", "Bench hardware and CAN access", "Honest feedback after each sprint"], { kicker: "WAINBEE", size: 14 });
  card(s, M + 8.4, 1.4, 3.9, 4.6, "We measure", ["Hours from signal list to first screen", "Hours to add a proprietary PGN", "Defects caught on the PC before hardware", "Go / no-go for a customer project in week 6"], { kicker: "SUCCESS", size: 14 });
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
