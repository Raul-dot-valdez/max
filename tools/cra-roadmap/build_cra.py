import os
"""M1 dual CRA roadmap workbook."""
import math
from openpyxl import Workbook
from openpyxl.styles import Alignment, Border, Font, PatternFill, Side
from openpyxl.utils import get_column_letter
from openpyxl.worksheet.datavalidation import DataValidation
from openpyxl.formatting.rule import CellIsRule

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "docs", "cra", "M1_CRA_dual_roadmap.xlsx")

F = "Arial"
GRAPHITE = "0F1418"
AMBER = "FF7A1A"
SOFT = "F1F4F6"
INPUT = "FFF2CC"  # light yellow: cells to fill in
thin = Side(style="thin", color="CDD5DB")
BORDER = Border(left=thin, right=thin, top=thin, bottom=thin)
WRAP = Alignment(wrap_text=True, vertical="top")
HEAD_FONT = Font(name=F, bold=True, color="FFFFFF", size=10)
HEAD_FILL = PatternFill("solid", fgColor=GRAPHITE)
BODY = Font(name=F, size=10)
BOLD = Font(name=F, size=10, bold=True)

R1 = "Roadmap 1: Listen-only"
R2 = "Roadmap 2: Connected"

wb = Workbook()

# ---------------------------------------------------------------- helpers
def header(ws, row, cols, widths):
    for i, (c, w) in enumerate(zip(cols, widths), start=1):
        cell = ws.cell(row=row, column=i, value=c)
        cell.font, cell.fill, cell.alignment, cell.border = HEAD_FONT, HEAD_FILL, Alignment(wrap_text=True, vertical="center"), BORDER
        ws.column_dimensions[get_column_letter(i)].width = w
    ws.row_dimensions[row].height = 32

def fit_height(ws, row, widths, values, base=13.5, min_lines=1):
    lines = min_lines
    for v, w in zip(values, widths):
        if v is None:
            continue
        text = str(v)
        chars = max(1, int(w * 1.15))
        n = sum(max(1, math.ceil(len(part) / chars)) for part in text.split("\n"))
        lines = max(lines, n)
    ws.row_dimensions[row].height = min(409, lines * base + 4)

def title(ws, text, sub=None):
    ws["A1"] = text
    ws["A1"].font = Font(name=F, size=16, bold=True, color=GRAPHITE)
    if sub:
        ws["A2"] = sub
        ws["A2"].font = Font(name=F, size=10, italic=True, color="5B6873")

# ======================================================================
# Sheet 1: README
# ======================================================================
ws = wb.active
ws.title = "README"
title(ws, "M1 — EU Cyber Resilience Act (CRA) dual roadmap",
      "Engineering planning tool, not legal advice. Confirm interpretations with counsel or a notified body before signing an EU declaration of conformity.")
rows = [
    ("Purpose", "Plan CRA compliance for two M1 product roadmaps, requirement by requirement, with three alternative next steps for each."),
    ("Roadmap 1: Listen-only", "M1 receiving J1939 only. No Bluetooth, no CAN FD, no transmission to ECUs beyond what J1939 itself needs (address claim, requests if used). USB kept only for factory and service updates. Smallest attack surface."),
    ("Roadmap 2: Connected", "M1 with Bluetooth (phone app, configuration, logs, updates) and CAN FD, interacting with vehicle systems and ECUs (commands, parameter changes, diagnostics). Larger attack surface; extra regulations may apply."),
    ("Proposed CRA category", "Both roadmaps: default category (not 'important' or 'critical'). An instrument display is not listed in Annex III or Annex IV. Conformity by internal control (Module A, Annex VIII). Annex I and Annex II apply in full to both."),
    ("Alternative 1: Lean", "Minimum defensible measure, done in-house at lowest cost. Suitable where risk is low and evidence can be argued in the risk assessment."),
    ("Alternative 2: Recommended", "Balanced measure aligned with emerging standards and good practice (e.g. IEC 62443-4-1 / 4-2 style processes, signed firmware, documented testing). Default choice."),
    ("Alternative 3: Assured", "Highest confidence: third-party testing, external assessment or certification, or an outsourced service. Use where customers, insurers or market surveillance are likely to ask."),
    ("Status values", "Not started / In progress / Done / Not applicable. Edit the yellow cells in the 'CRA requirements' sheet: Owner, Status, Target date, Notes."),
    ("Priority", "P1 = needed for the first compliant release or already in force · P2 = needed before 11 Dec 2027 · P3 = improvement or roadmap 2 only."),
    ("Key dates", "11 Sep 2026: reporting obligations (Art. 14) apply, including to products already on the market. 11 Dec 2027: main obligations apply (Annex I, II, conformity assessment, CE marking). The 'Dashboard' sheet counts the days remaining."),
    ("VERIFY", "Rows or notes marked VERIFY rely on interpretation or on facts not yet confirmed (e.g. exact wording of support-period and retention rules, RED delegated act applicability, M1 MCU part number). Check before relying on them."),
    ("Sheets", "README · Dashboard (formula summary) · Variant comparison · CRA requirements (main table) · Annex map · Dual roadmap (quarterly plan) · Sources"),
    ("How to use", "Filter the 'CRA requirements' sheet by roadmap or priority. Pick one alternative per row, record it in Notes, set Owner, Status and Target date. The Dashboard updates automatically."),
]
r = 4
for k, v in rows:
    ws.cell(row=r, column=1, value=k).font = BOLD
    c = ws.cell(row=r, column=2, value=v)
    c.font, c.alignment = BODY, WRAP
    ws.cell(row=r, column=1).alignment = WRAP
    fit_height(ws, r, [24, 110], [k, v])
    r += 1
ws.column_dimensions["A"].width = 24
ws.column_dimensions["B"].width = 110

# ======================================================================
# Sheet: Variant comparison
# ======================================================================
vc = wb.create_sheet("Variant comparison")
title(vc, "Roadmap 1 vs Roadmap 2 at a glance")
cols = ["Dimension", R1, R2, "Why it matters for the CRA"]
widths = [26, 46, 46, 50]
header(vc, 4, cols, widths)
data = [
    ("Interfaces", "2 × CAN (J1939, receive), USB (service only), keypad, analog inputs, camera input", "Adds Bluetooth 4.2+ and CAN FD; transmits to ECUs; phone app", "Every external interface is attack surface (Annex I Part I (2)(j))"),
    ("CAN behaviour", "Listens to J1939 broadcasts; sends only address claim and optional requests", "Sends commands and parameter writes, diagnostics, possibly J1939-22 (J1939 over CAN FD)", "Sending commands raises integrity and access-control requirements ((2)(d), (2)(f))"),
    ("CRA category", "Default", "Default (Bluetooth does not move a display into Annex III)", "Conformity route: internal control (Module A) for both"),
    ("Main threats", "Malformed or flooding CAN traffic causing freeze or crash; firmware tampering over USB or debug port", "All of Roadmap 1, plus wireless attacks, unauthorised commands to ECUs, spoofed app, data leakage from logs", "Drives the risk assessment required by Art. 13"),
    ("Personal or confidential data", "Minimal: machine signals, settings", "Logs, usage and location data, phone pairing keys, possibly operator identity", "Confidentiality and data minimisation ((2)(e), (2)(g), (2)(m))"),
    ("Update path", "Signed images over USB (service tool)", "Signed images over USB and Bluetooth; app-driven updates", "Security updates and secure distribution (Part I (2)(c), Part II (7)-(8))"),
    ("Other EU rules to check", "Machinery Regulation (EU) 2023/1230 if the display carries a safety function", "Also the Radio Equipment Directive delegated act (EU) 2022/30 and EN 18031 for Bluetooth; Machinery Regulation cybersecurity clauses if commands affect safety", "Overlapping requirements can share evidence (VERIFY applicability)"),
    ("Relative CRA effort", "Low to medium", "Medium to high", "Roadmap 1 can ship first and build the shared process"),
]
for i, row in enumerate(data, start=5):
    for j, v in enumerate(row, start=1):
        c = vc.cell(row=i, column=j, value=v)
        c.font, c.alignment, c.border = (BOLD if j == 1 else BODY), WRAP, BORDER
    fit_height(vc, i, widths, row)
vc.freeze_panes = "B5"

# ======================================================================
# Sheet: CRA requirements (main table)
# ======================================================================
rq = wb.create_sheet("CRA requirements")
title(rq, "CRA requirement matrix — M1, two roadmaps",
      "Requirement texts are paraphrased from Regulation (EU) 2024/2847. Yellow columns are for your team to fill in.")
COLS = ["ID", "Annex / Article", "Area", "Requirement (paraphrased)",
        "Applies to " + R1, "Applies to " + R2,
        R1 + ": approach", R2 + ": approach",
        "Evidence / artefact", "Suggested owner", "Priority", "CRA date",
        "Suggested next steps — Alternative 1 (Lean)",
        "Suggested next steps — Alternative 2 (Recommended)",
        "Suggested next steps — Alternative 3 (Assured)",
        "Owner (assigned)", "Status", "Target date", "Notes / VERIFY"]
WID = [7, 15, 18, 40, 11, 11, 38, 38, 28, 16, 9, 15, 38, 38, 38, 16, 14, 13, 30]
HROW = 4
header(rq, HROW, COLS, WID)

D27 = "11 Dec 2027"
D26 = "In force since 11 Sep 2026"

REQ = [
    # ---------- Annex I Part I
    ("I-1.1", "Annex I Part I (1)", "Design", "Design, develop and produce the product to ensure an appropriate level of cybersecurity based on the risks.",
     "Yes", "Yes",
     "Threat model limited to CAN receive, USB service port and debug port.",
     "Full threat model incl. Bluetooth, phone app, CAN FD and commands sent to ECUs.",
     "Risk assessment; threat model; security requirements list", "Product security lead", "P1", D27,
     "Write a short threat model (STRIDE) per interface in a shared document.",
     "Run a structured threat analysis and risk assessment following an IEC 62443-4-1 style secure development process; keep it versioned with each release.",
     "Commission an external security consultancy to review the threat model and secure development process.",
     "Shared foundation for both roadmaps."),
    ("I-2a", "Annex I Part I (2)(a)", "Vulnerabilities", "Place on the market without known exploitable vulnerabilities.",
     "Yes", "Yes",
     "Scan SBOM components (FreeRTOS, CAN stack, GUI library, HAL) for CVEs before each release.",
     "Same, plus Bluetooth stack, app and CAN FD stack; penetration test of wireless interface.",
     "CVE scan report per release; release checklist", "Firmware lead", "P1", D27,
     "Manual CVE check of the SBOM against public databases before release.",
     "Automated SBOM vulnerability scanning in CI; release blocked on unresolved high/critical findings.",
     "Third-party vulnerability assessment and penetration test before first release and yearly.",
     ""),
    ("I-2b", "Annex I Part I (2)(b)", "Secure by default", "Ship with a secure default configuration, with the possibility to reset to the original state.",
     "Yes", "Yes",
     "Service menus locked by default; debug port locked (STM32 read-out protection); factory reset of settings.",
     "Bluetooth off or non-discoverable by default; pairing required; ECU command functions disabled until configured by the OEM.",
     "Default configuration specification; reset procedure", "Firmware lead", "P1", D27,
     "Document default settings and add a settings-reset function.",
     "Hardening checklist verified in end-of-line test (RDP level, disabled interfaces, default credentials absent).",
     "External configuration review against a hardening baseline.",
     ""),
    ("I-2c", "Annex I Part I (2)(c)", "Security updates", "Allow vulnerabilities to be fixed through security updates, including automatic updates where applicable, with user notification and opt-out.",
     "Yes", "Yes",
     "Signed firmware update via USB service tool; automatic updates not applicable (no network).",
     "Signed updates via USB and Bluetooth/app; app notifies the user; opt-out and postpone options.",
     "Update procedure; signed-image format; user instructions", "Firmware lead", "P1", D27,
     "USB bootloader accepts only images signed with A99's key.",
     "Secure bootloader with signature check, version anti-rollback and A/B or recovery image (e.g. MCUboot or ST X-CUBE-SBSFU — VERIFY fit for the M1 MCU).",
     "Update mechanism reviewed externally; key management in a hardware security module (HSM) or managed signing service.",
     "VERIFY M1 MCU part number and bootloader capabilities."),
    ("I-2d", "Annex I Part I (2)(d)", "Access control", "Protect from unauthorised access with appropriate controls (authentication, identity or access management) and report possible unauthorised access.",
     "Limited", "Yes",
     "Service functions behind a key sequence and PIN; USB update requires signed image; debug locked.",
     "Bluetooth pairing with authentication (LE Secure Connections or equivalent); role-based access in the app (operator / technician / OEM); command whitelist towards ECUs.",
     "Access-control design; pairing specification", "Firmware lead + app lead", "P1", D27,
     "PIN-protect service menus; lock debug.",
     "Authenticated pairing, per-role permissions, and event log of failed access attempts.",
     "External test of authentication and pairing (e.g. against EN 18031-1 for radio equipment).",
     "Roadmap 1: CAN itself has no authentication; argue in risk assessment."),
    ("I-2e", "Annex I Part I (2)(e)", "Confidentiality", "Protect confidentiality of stored, transmitted or processed data, e.g. with state-of-the-art encryption.",
     "Limited", "Yes",
     "Little confidential data; protect configuration and calibration in flash from read-out (RDP).",
     "Encrypt Bluetooth link; encrypt stored logs and keys; protect pairing keys.",
     "Data inventory; encryption design", "Product security lead", "P2", D27,
     "Data inventory showing no personal data; RDP enabled.",
     "Encrypted Bluetooth (bonded, LE Secure Connections); keys in protected flash; logs encrypted at rest where they contain operator or location data.",
     "Use a secure element for key storage; external review of crypto design.",
     ""),
    ("I-2f", "Annex I Part I (2)(f)", "Integrity", "Protect integrity of data, commands, programs and configuration against unauthorised modification, and report corruption.",
     "Yes", "Yes",
     "Signed firmware; boot-time image verification (secure boot); CRC on configuration; plausibility checks on received J1939 values.",
     "All of Roadmap 1, plus integrity of commands sent to ECUs (authentication of commands where the ECU supports it, e.g. message authentication on CAN FD) and signed app packages.",
     "Secure boot design; integrity test results", "Firmware lead", "P1", D27,
     "Verify signature on update; CRC on settings.",
     "Secure boot: verify application signature at every start-up; refuse modified images; log integrity failures.",
     "Hardware root of trust (secure element or MCU with secure boot support) and external review.",
     "Secure boot is not named in the CRA but is the usual way to meet (2)(f)."),
    ("I-2g", "Annex I Part I (2)(g)", "Data minimisation", "Process only data that is adequate, relevant and limited to the intended purpose.",
     "Yes", "Yes",
     "Display only; store only settings and hour counters.",
     "Define what logs and fleet data are collected and why; retention limits; no location unless needed.",
     "Data inventory with purpose per item", "Product manager", "P2", D27,
     "One-page data inventory.",
     "Data inventory plus retention rules implemented in firmware and app.",
     "Privacy and data-protection review (GDPR overlap for operator or location data).",
     ""),
    ("I-2h", "Annex I Part I (2)(h)", "Availability", "Protect availability of essential functions, including resilience against denial-of-service.",
     "Yes", "Yes",
     "Display keeps working under CAN bus flooding and malformed frames; watchdog recovery; stale data shown as '--'.",
     "Same, plus Bluetooth flooding or pairing spam must not affect display or CAN functions; CAN FD error handling.",
     "Robustness test report (CAN fuzzing, bus-load test)", "Test lead", "P1", D27,
     "Bench test with high bus load and random frames.",
     "Automated CAN fuzzing in CI and on hardware (malformed transport-protocol and DM1 messages, address-claim conflicts, 100% bus load).",
     "Independent robustness testing by a test lab.",
     ""),
    ("I-2i", "Annex I Part I (2)(i)", "Impact on others", "Minimise negative impact on the availability of services provided by other devices or networks.",
     "Yes", "Yes",
     "Listen-only by design; limit own transmissions; bounded request rates.",
     "Rate-limit commands and requests to ECUs; safe defaults if the app disconnects; no bus flooding on error.",
     "Network behaviour specification; bus-load measurements", "Firmware lead", "P2", D27,
     "Document transmit behaviour and maximum rates.",
     "Enforce transmit budgets in firmware and test them; error-passive / bus-off handling verified.",
     "OEM-integrated network test on the target machine with the OEM's ECUs.",
     ""),
    ("I-2j", "Annex I Part I (2)(j)", "Attack surface", "Limit attack surfaces, including external interfaces.",
     "Yes", "Yes",
     "USB function limited to signed updates; debug port locked; unused peripherals disabled.",
     "Bluetooth profiles limited to what the app needs; CAN FD command set whitelisted; app surface reviewed.",
     "Interface inventory; hardening checklist", "Firmware lead", "P1", D27,
     "Interface inventory with justification for each port.",
     "Disable unused services in production builds; separate service build; checklist in end-of-line test.",
     "External attack-surface review.",
     ""),
    ("I-2k", "Annex I Part I (2)(k)", "Exploit mitigation", "Reduce the impact of incidents using exploitation mitigation mechanisms.",
     "Yes", "Yes",
     "MPU regions (no-execute for data/stack), stack overflow checks in FreeRTOS, watchdog.",
     "Same, plus isolation of Bluetooth stack from display/CAN tasks; least privilege in the app.",
     "Memory-protection configuration; code-review records", "Firmware lead", "P2", D27,
     "Enable FreeRTOS stack-overflow checking and the MPU.",
     "MPU configuration reviewed; compiler hardening flags; static analysis (e.g. MISRA-style rules) in CI.",
     "External code audit.",
     ""),
    ("I-2l", "Annex I Part I (2)(l)", "Security logging", "Record and monitor relevant internal activity (access, modification of data or functions), with user opt-out.",
     "Limited", "Yes",
     "Log update attempts, integrity failures and resets in non-volatile memory.",
     "Also log pairing, app commands and parameter changes sent to ECUs; export via app.",
     "Security event log specification", "Firmware lead", "P2", D27,
     "Minimal event log of updates and failures.",
     "Structured security log with timestamps, readable by service tool or app.",
     "Log export to OEM fleet or security monitoring service.",
     ""),
    ("I-2m", "Annex I Part I (2)(m)", "Data removal", "Let users securely and permanently remove all data and settings.",
     "Yes", "Yes",
     "Factory reset of settings.",
     "Factory reset including logs, pairing keys and app bindings; secure erase.",
     "Reset procedure; test record", "Firmware lead", "P2", D27,
     "Settings reset in the service menu.",
     "Secure erase of logs and keys, documented in user instructions.",
     "External verification that erased data cannot be recovered.",
     ""),
    # ---------- Annex I Part II
    ("II-1", "Annex I Part II (1)", "SBOM", "Identify and document vulnerabilities and components, including an SBOM in a machine-readable format (at least top-level dependencies).",
     "Yes", "Yes",
     "SBOM: FreeRTOS, HAL, CAN stack, GUI library (TouchGFX or Qt for MCUs), bootloader.",
     "Same, plus Bluetooth stack, crypto libraries, mobile app dependencies.",
     "SBOM per release (SPDX or CycloneDX)", "Firmware lead", "P1", D27,
     "Maintain the SBOM by hand in SPDX or CycloneDX format.",
     "Generate the SBOM automatically in the build pipeline and archive it per release.",
     "SBOM management platform with continuous monitoring.",
     ""),
    ("II-2", "Annex I Part II (2)", "Remediation", "Address and remediate vulnerabilities without delay, including through security updates; separate security updates from feature updates where feasible.",
     "Yes", "Yes",
     "Patch releases via USB service tool; distributor and OEM notification.",
     "Patch releases over Bluetooth/app as well; separate security-only releases.",
     "Patch policy with target timelines", "Product security lead", "P1", D27,
     "Written patch policy with response times by severity.",
     "Security-only release branch; regression tests that let a fix ship quickly.",
     "Retained external support for emergency fixes.",
     ""),
    ("II-3", "Annex I Part II (3)", "Security testing", "Apply effective and regular tests and reviews of security.",
     "Yes", "Yes",
     "CAN fuzzing, update-path tests, debug-lock verification.",
     "Same, plus Bluetooth and app penetration tests, command-path tests with ECUs.",
     "Test plan and reports", "Test lead", "P1", D27,
     "Security test checklist run before each release.",
     "Automated security tests in CI plus a yearly internal review.",
     "Yearly third-party penetration test.",
     ""),
    ("II-4", "Annex I Part II (4)", "Disclosure", "Once an update is available, publicly disclose fixed vulnerabilities (description, affected products, impact, severity, remediation).",
     "Yes", "Yes",
     "Security advisories on the A99 website.",
     "Same, plus in-app notification.",
     "Advisory template; advisory page", "Product security lead", "P2", D27,
     "Advisory page with a simple template.",
     "Machine-readable advisories (e.g. CSAF) and CVE assignment.",
     "Become a CVE Numbering Authority or use a coordinator.",
     ""),
    ("II-5", "Annex I Part II (5)", "CVD policy", "Put in place and enforce a coordinated vulnerability disclosure policy.",
     "Yes", "Yes",
     "Same for both roadmaps.", "Same for both roadmaps.",
     "Published CVD policy", "Product security lead", "P1", D26,
     "Publish a short CVD policy and security.txt.",
     "CVD policy plus internal triage process with response targets.",
     "Bug bounty or managed disclosure programme.",
     "Needed now to support Art. 14 reporting."),
    ("II-6", "Annex I Part II (6)", "Reporting contact", "Facilitate reporting of potential vulnerabilities, including a contact address.",
     "Yes", "Yes",
     "Same for both roadmaps.", "Same for both roadmaps.",
     "Security contact (email, web form)", "Product security lead", "P1", D26,
     "Dedicated security email address.",
     "Web form plus PGP key and acknowledgement SLA.",
     "Managed intake via a coordination service.",
     ""),
    ("II-7", "Annex I Part II (7)", "Secure distribution", "Provide mechanisms to distribute updates securely and in a timely (where applicable automatic) manner.",
     "Yes", "Yes",
     "Signed packages downloaded from an authenticated A99 portal; service tool checks signature.",
     "Signed packages delivered via app; app verifies before transfer; device verifies again.",
     "Distribution design; signing-key management", "Firmware lead + IT", "P1", D27,
     "Signed packages on an HTTPS download page.",
     "Signing in CI with keys in an HSM or managed signing service; download portal with integrity checks.",
     "External review of the update supply chain.",
     ""),
    ("II-8", "Annex I Part II (8)", "Free updates", "Disseminate security updates without delay and free of charge, with advisory messages.",
     "Yes", "Yes",
     "Free security updates via distributors and OEMs.",
     "Free security updates via app as well.",
     "Commercial terms; advisory process", "Product manager", "P2", D27,
     "State free security updates in terms and conditions.",
     "Distributor and OEM agreements include update dissemination duties.",
     "Update dissemination tracked per serial number.",
     ""),
    # ---------- Annex II
    ("A2-1", "Annex II (1)", "User information", "Manufacturer name, registered trade name or mark, postal and email address or other contact.",
     "Yes", "Yes", "Same for both.", "Same for both.", "User manual; label", "Technical writer", "P2", D27,
     "Add to the manual.", "Add to manual, label and website.", "Reviewed in documentation audit.", ""),
    ("A2-2", "Annex II (2)", "User information", "Single point of contact for reporting vulnerabilities and where the CVD policy can be found.",
     "Yes", "Yes", "Same for both.", "Same for both.", "User manual; website", "Technical writer", "P1", D27,
     "Add contact to the manual.", "Contact and CVD link in manual, website and app.", "Reviewed in documentation audit.", ""),
    ("A2-3", "Annex II (3)", "User information", "Product name, type and information enabling unique identification.",
     "Yes", "Yes", "Model and serial on label and service screen.", "Also shown in app.", "Label; service screen", "Firmware lead", "P2", D27,
     "Model and serial on label.", "Also on service screen with firmware version.", "Machine-readable identification (QR) on screen.", ""),
    ("A2-4", "Annex II (4)", "User information", "Intended purpose, security environment, essential functions and security properties.",
     "Yes", "Yes", "Read-only CAN display; intended in-cab installation.", "Connected display with app and ECU interaction.", "User manual; integration guide", "Product manager", "P2", D27,
     "Short security section in the manual.", "Dedicated security chapter in manual and integration guide.", "Reviewed by external assessor.", ""),
    ("A2-5", "Annex II (5)", "User information", "Known or foreseeable circumstances that may lead to significant cybersecurity risks.",
     "Yes", "Yes", "E.g. connecting an untrusted CAN tool; leaving USB port accessible.", "Also e.g. pairing with unknown phones; sharing app credentials.", "User manual", "Product security lead", "P2", D27,
     "List the top risks in the manual.", "Risks derived from the threat model and kept in sync.", "Reviewed externally.", ""),
    ("A2-6", "Annex II (6)", "User information", "Where applicable, internet address of the EU declaration of conformity.",
     "Yes", "Yes", "Same for both.", "Same for both.", "Website page with DoC", "Quality", "P2", D27,
     "Publish DoC PDF.", "Versioned DoC page per product variant.", "—", ""),
    ("A2-7", "Annex II (7)", "User information", "Type of technical security support offered and end date of the support period.",
     "Yes", "Yes", "Support period stated per variant.", "Same; may differ because of app and wireless components.", "Manual; product page", "Product manager", "P1", D27,
     "State end date in the manual.", "End date on label, manual, website and service screen.", "Contractual support commitments with OEMs.", "Links to Art. 13 support period row."),
    ("A2-8a", "Annex II (8)(a)", "User information", "Security measures at commissioning and throughout lifetime.",
     "Yes", "Yes", "Installation: protect USB port, set service PIN.", "Also: pairing procedure, app account set-up.", "Installation guide", "Technical writer", "P2", D27,
     "Checklist in installation guide.", "Step-by-step secure commissioning guide.", "—", ""),
    ("A2-8b", "Annex II (8)(b)", "User information", "How changes to the product can affect the security of data.",
     "Yes", "Yes", "Same for both.", "Same for both.", "Manual", "Technical writer", "P3", D27,
     "One paragraph in the manual.", "Guidance per configurable feature.", "—", ""),
    ("A2-8c", "Annex II (8)(c)", "User information", "How security-relevant updates can be installed.",
     "Yes", "Yes", "USB service-tool procedure.", "App and USB procedures.", "Update instructions", "Technical writer", "P1", D27,
     "Written USB update procedure.", "Procedures for each update path, with screenshots.", "—", ""),
    ("A2-8d", "Annex II (8)(d)", "User information", "Secure decommissioning, including removing user data.",
     "Yes", "Yes", "Factory reset procedure.", "Factory reset plus unpairing and app account removal.", "Decommissioning instructions", "Technical writer", "P2", D27,
     "Reset procedure in manual.", "Decommissioning checklist for OEMs and dealers.", "—", ""),
    ("A2-8e", "Annex II (8)(e)", "User information", "How to turn off automatic installation of security updates, where applicable.",
     "N/A", "Yes", "Not applicable: no automatic updates.", "App setting to postpone or disable automatic updates.", "App instructions", "App lead", "P3", D27,
     "—", "Document the app setting.", "—", ""),
    ("A2-8f", "Annex II (8)(f)", "User information", "For products integrated into other products: information the integrator needs to comply.",
     "Yes", "Yes", "Integration guide for OEMs: network placement, USB access, update duties.", "Also: command interface security, ECU whitelist configuration, app deployment.", "OEM integration guide", "Applications engineering", "P1", D27,
     "Security chapter in the OEM integration guide.", "Integration guide plus security checklist the OEM signs.", "Joint review with key OEMs.", "Important: the M1 is always integrated into an OEM machine."),
    ("A2-9", "Annex II (9)", "User information", "If the SBOM is made available to users, where it can be accessed.",
     "Optional", "Optional", "Provide on request to OEMs.", "Provide on request to OEMs.", "SBOM access process", "Product security lead", "P3", D27,
     "SBOM on request.", "SBOM portal for OEM customers.", "Public SBOM.", ""),
    # ---------- Articles and conformity
    ("Art13-RA", "Art. 13(2)-(3)", "Risk assessment", "Carry out, document and keep up to date a cybersecurity risk assessment; include it in the technical documentation.",
     "Yes", "Yes", "Shared method; smaller scope.", "Shared method; wider scope.", "Risk assessment document", "Product security lead", "P1", D27,
     "Template-based risk assessment.", "Risk assessment linked to threat model and test evidence, reviewed each release.", "External review of the assessment.", ""),
    ("Art13-DD", "Art. 13(5)", "Third-party components", "Exercise due diligence when integrating third-party components (incl. open source).",
     "Yes", "Yes", "FreeRTOS, HAL, CAN stack, GUI library.", "Also Bluetooth stack, crypto, app frameworks.", "Component register with due-diligence notes", "Firmware lead", "P1", D27,
     "Register of components with source and licence.", "Register plus maintenance status and CVE history per component.", "Supplier security questionnaires and audits.", ""),
    ("Art13-SP", "Art. 13(8)", "Support period", "Determine a support period reflecting expected use time (generally at least five years) and handle vulnerabilities during it.",
     "Yes", "Yes", "Propose support period matching machine lifetime expectations.", "Same; plan for app and Bluetooth stack maintenance.", "Support-period decision record", "Product manager", "P1", D27,
     "Set a minimum five-year support period.", "Set support period from OEM machine lifetimes (often 10+ years) and budget for it.", "Contractual long-term support with OEMs.", "VERIFY exact wording of the five-year rule and update retention periods."),
    ("Art14", "Art. 14", "Reporting", "Report actively exploited vulnerabilities and severe incidents via the single reporting platform: early warning within 24 h, notification within 72 h, final report later; inform users.",
     "Yes", "Yes", "Same process for both.", "Same process for both.", "Incident and vulnerability reporting procedure", "Product security lead", "P1", D26,
     "Named contact and a one-page reporting procedure.", "Documented PSIRT process with on-call rota and templates for each report stage.", "Retained external incident-response provider.", "Applies now, including to products already on the market."),
    ("Art31", "Art. 31 + Annex VII", "Technical documentation", "Draw up technical documentation (design, risk assessment, SBOM, test reports, update process) before placing on the market.",
     "Yes", "Yes", "Smaller documentation set.", "Larger set; includes wireless and app.", "Technical file", "Quality", "P1", D27,
     "Technical file assembled from existing documents.", "Technical file generated per release from the repository and CI.", "Pre-assessment of the technical file by a notified body.", "Retention period: VERIFY."),
    ("Art32", "Art. 32 + Annex VIII", "Conformity assessment", "Default category: internal control (Module A).",
     "Yes", "Yes", "Module A self-assessment.", "Module A self-assessment.", "Conformity assessment record", "Quality", "P1", D27,
     "Self-assessment against Annex I using the requirement matrix.", "Self-assessment using harmonised standards once published.", "Voluntary third-party assessment for customer confidence.", ""),
    ("Art28-30", "Art. 28-30 + Annex V", "DoC and CE", "Draw up the EU declaration of conformity and affix CE marking.",
     "Yes", "Yes", "Same for both.", "Same for both.", "EU declaration of conformity; CE label", "Quality", "P1", D27,
     "Extend existing CE documentation with CRA.", "Single combined DoC covering CRA, EMC and other applicable acts.", "Notified-body review of DoC package.", ""),
    # ---------- Other regulations
    ("RED-DA", "Delegated Reg. (EU) 2022/30 (RED)", "Other EU rules", "Cybersecurity requirements for radio equipment that connects to the internet directly or via other equipment (EN 18031 series).",
     "No", "VERIFY", "Not applicable (no radio).", "Likely applicable to Bluetooth variant if the phone links it to the internet.", "EN 18031 assessment", "Quality", "P1", "Since 1 Aug 2025",
     "Confirm applicability with notified body.", "Assess against EN 18031-1 (and -2 if personal data); reuse CRA evidence.", "Third-party EN 18031 assessment.", "VERIFY applicability to the Bluetooth models."),
    ("MR-2023", "Reg. (EU) 2023/1230 (Machinery)", "Other EU rules", "Protection against corruption for safety-relevant software and control systems.",
     "VERIFY", "VERIFY", "Only if the display carries a safety function.", "More likely if commands to ECUs affect safety functions.", "Machinery risk assessment link", "Safety lead", "P2", "From 20 Jan 2027",
     "Confirm whether any display function is safety-related.", "Align machinery and CRA risk assessments; reuse integrity evidence.", "Joint assessment with OEM safety team.", "VERIFY date and scope."),
    ("CAN-SEC", "Good practice", "CAN security", "Integrity of commands on CAN/CAN FD towards ECUs.",
     "N/A", "Yes", "Not applicable: listen-only.", "Message authentication for commands where ECUs support it (CAN FD payload allows MACs); whitelist and rate limits otherwise.", "CAN security concept", "Firmware lead", "P2", D27,
     "Command whitelist and rate limiting.", "Message authentication codes on safety- or security-relevant commands (SAE J1939-91 family guidance — VERIFY).", "Joint security concept with OEM and ECU suppliers.", ""),
]

r0 = HROW + 1
for i, row in enumerate(REQ):
    r = r0 + i
    vals = list(row[:16 - 1])  # up to Alternative 3
    notes = row[15]
    full = vals + ["", "Not started", None, notes]
    for j, v in enumerate(full, start=1):
        c = rq.cell(row=r, column=j, value=v)
        c.font = BOLD if j in (1, 11) else BODY
        c.alignment = WRAP
        c.border = BORDER
        if j in (16, 17, 18, 19):
            c.fill = PatternFill("solid", fgColor=INPUT)
        if j == 18:
            c.number_format = "yyyy-mm-dd"
    fit_height(rq, r, WID, full)
last = r0 + len(REQ) - 1

dv_status = DataValidation(type="list", formula1='"Not started,In progress,Done,Not applicable"', allow_blank=True)
dv_prio = DataValidation(type="list", formula1='"P1,P2,P3"', allow_blank=True)
rq.add_data_validation(dv_status); rq.add_data_validation(dv_prio)
dv_status.add(f"Q{r0}:Q{last}")
dv_prio.add(f"K{r0}:K{last}")
red = PatternFill("solid", fgColor="F8D7D3"); amb = PatternFill("solid", fgColor="FBE7C6"); grn = PatternFill("solid", fgColor="D8EEDF")
rq.conditional_formatting.add(f"K{r0}:K{last}", CellIsRule(operator="equal", formula=['"P1"'], fill=red))
rq.conditional_formatting.add(f"K{r0}:K{last}", CellIsRule(operator="equal", formula=['"P2"'], fill=amb))
rq.conditional_formatting.add(f"Q{r0}:Q{last}", CellIsRule(operator="equal", formula=['"Done"'], fill=grn))
rq.freeze_panes = rq.cell(row=r0, column=5)
rq.auto_filter.ref = f"A{HROW}:S{last}"
rq.cell(row=last + 2, column=1, value="Example of a filled row: set Owner (assigned) = 'J. Tremblay', Status = 'In progress', Target date = 2027-03-31, Notes = 'Chose Alternative 2'. Columns P-S (yellow) are for your team; other columns are the proposal.").font = Font(name=F, size=9, italic=True, color="5B6873")

# ======================================================================
# Sheet: Annex map
# ======================================================================
am = wb.create_sheet("Annex map")
title(am, "Which CRA annexes apply to the M1")
cols = ["Annex", "Content", R1, R2, "Comment"]
widths = [12, 46, 18, 18, 52]
header(am, 4, cols, widths)
amap = [
    ("Annex I", "Essential cybersecurity requirements: Part I product properties, Part II vulnerability handling", "Applies in full", "Applies in full", "Same for every category; see 'CRA requirements' rows I-* and II-*"),
    ("Annex II", "Information and instructions to the user", "Applies in full", "Applies in full", "Rows A2-*; (8)(f) integrator information is key for an OEM-integrated display"),
    ("Annex III", "Important products, Class I and Class II", "Not listed", "Not listed", "Only decides the category; an instrument display is not listed"),
    ("Annex IV", "Critical products", "Not listed", "Not listed", "Not relevant"),
    ("Annex V", "EU declaration of conformity", "Applies", "Applies", "Row Art28-30"),
    ("Annex VI", "Simplified EU declaration of conformity", "Optional", "Optional", "Format option for the DoC reference"),
    ("Annex VII", "Content of technical documentation", "Applies", "Applies", "Row Art31"),
    ("Annex VIII", "Conformity assessment procedures", "Module A (internal control)", "Module A (internal control)", "Default category route; row Art32"),
]
for i, row in enumerate(amap, start=5):
    for j, v in enumerate(row, start=1):
        c = am.cell(row=i, column=j, value=v)
        c.font, c.alignment, c.border = (BOLD if j == 1 else BODY), WRAP, BORDER
    fit_height(am, i, widths, row)

# ======================================================================
# Sheet: Dual roadmap
# ======================================================================
rm = wb.create_sheet("Dual roadmap")
title(rm, "Quarterly plan — both roadmaps share one security process",
      "Proposal. Adjust to engineering capacity. Roadmap 1 ships first and builds the process Roadmap 2 reuses.")
QS = ["Q1 2027", "Q2 2027", "Q3 2027", "Q4 2027", "Q1 2028", "Q2 2028", "Q3 2028", "Q4 2028"]
cols = ["Workstream", "Roadmap"] + QS
widths = [26, 18] + [26] * 8
header(rm, 4, cols, widths)
plan = [
    ("Reporting and PSIRT (in force)", "Both", "PSIRT process, contact and CVD policy live", "Reporting drill", "", "", "Yearly drill", "", "", ""),
    ("Risk assessment and threat model", "Roadmap 1", "Threat model and risk assessment", "Review with test results", "Freeze for release", "", "Yearly review", "", "", ""),
    ("Risk assessment and threat model", "Roadmap 2", "", "Threat model incl. Bluetooth, app, CAN FD", "Risk assessment", "Review with pen-test results", "Freeze for release", "", "Yearly review", ""),
    ("Secure boot and signed updates", "Roadmap 1", "Bootloader choice (MCUboot / SBSFU) — VERIFY MCU", "Signed USB updates, anti-rollback", "Secure boot at start-up; debug lock in production", "", "", "", "", ""),
    ("Secure boot and signed updates", "Roadmap 2", "", "", "Reuse Roadmap 1 bootloader", "Signed updates over Bluetooth via app", "Update notification and opt-out in app", "", "", ""),
    ("SBOM and vulnerability scanning", "Both", "SBOM format chosen; first SBOM", "Automated SBOM in build", "CVE scanning in CI", "", "", "", "", ""),
    ("Robustness and security testing", "Roadmap 1", "CAN fuzzing set-up", "Fuzzing in CI and on hardware", "Pre-release security test", "", "Yearly test", "", "", ""),
    ("Robustness and security testing", "Roadmap 2", "", "", "Bluetooth and app test plan", "Penetration test (wireless and app)", "Command-path tests with OEM ECUs", "", "Yearly pen test", ""),
    ("Access control and pairing", "Roadmap 2", "", "Pairing and role design", "Implement authenticated pairing and roles", "Command whitelist and rate limits", "Message authentication pilot with an OEM", "", "", ""),
    ("Documentation and Annex II", "Roadmap 1", "Manual security chapter", "OEM integration guide", "Technical file", "", "", "", "", ""),
    ("Documentation and Annex II", "Roadmap 2", "", "", "Manual and app instructions", "OEM integration guide", "Technical file", "", "", ""),
    ("Conformity and CE", "Roadmap 1", "", "", "Module A self-assessment", "DoC and CE under CRA before 11 Dec 2027", "", "", "", ""),
    ("Conformity and CE", "Roadmap 2", "", "", "RED / EN 18031 applicability decision", "", "EN 18031 assessment", "Module A, DoC, CE", "", ""),
]
for i, row in enumerate(plan, start=5):
    for j, v in enumerate(row, start=1):
        c = rm.cell(row=i, column=j, value=v)
        c.font = BOLD if j <= 2 else BODY
        c.alignment, c.border = WRAP, BORDER
        if j > 2 and v:
            c.fill = PatternFill("solid", fgColor="E8F1EA" if row[1] == "Roadmap 1" else ("FFF1E6" if row[1] == "Roadmap 2" else SOFT))
    fit_height(rm, i, widths, row)
mk = 5 + len(plan) + 1
rm.cell(row=mk, column=1, value="Key dates").font = BOLD
rm.cell(row=mk + 1, column=1, value="11 Sep 2026: Art. 14 reporting applies (already in force)").font = BODY
rm.cell(row=mk + 2, column=1, value="11 Dec 2027: main CRA obligations apply — falls in Q4 2027").font = BODY
rm.freeze_panes = "C5"

# ======================================================================
# Sheet: Dashboard (formulas)
# ======================================================================
db = wb.create_sheet("Dashboard", 1)
title(db, "Dashboard", "All figures are formulas over the 'CRA requirements' sheet.")
rng = lambda col: f"'CRA requirements'!${col}${r0}:${col}${last}"
db["A4"] = "Main CRA obligations apply on"; db["B4"] = "=DATE(2027,12,11)"; db["B4"].number_format = "yyyy-mm-dd"
db["A5"] = "Days remaining"; db["B5"] = "=MAX(0,B4-TODAY())"; db["B5"].number_format = "0"
db["A6"] = "Reporting obligations (Art. 14) apply since"; db["B6"] = "=DATE(2026,9,11)"; db["B6"].number_format = "yyyy-mm-dd"
for rr in range(4, 7):
    db.cell(row=rr, column=1).font = BOLD; db.cell(row=rr, column=2).font = BODY
header(db, 8, ["Measure", R1, R2], [44, 24, 24])
db.column_dimensions["A"].width = 44
lines = [
    ("Requirements that apply (Yes / Limited / VERIFY / Optional)",
     f'=COUNTIF({rng("E")},"Yes")+COUNTIF({rng("E")},"Limited")+COUNTIF({rng("E")},"VERIFY")+COUNTIF({rng("E")},"Optional")',
     f'=COUNTIF({rng("F")},"Yes")+COUNTIF({rng("F")},"Limited")+COUNTIF({rng("F")},"VERIFY")+COUNTIF({rng("F")},"Optional")'),
    ("…of which P1", f'=COUNTIFS({rng("K")},"P1",{rng("E")},"<>N/A",{rng("E")},"<>No")', f'=COUNTIFS({rng("K")},"P1",{rng("F")},"<>N/A",{rng("F")},"<>No")'),
    ("…of which P2", f'=COUNTIFS({rng("K")},"P2",{rng("E")},"<>N/A",{rng("E")},"<>No")', f'=COUNTIFS({rng("K")},"P2",{rng("F")},"<>N/A",{rng("F")},"<>No")'),
    ("…of which P3", f'=COUNTIFS({rng("K")},"P3",{rng("E")},"<>N/A",{rng("E")},"<>No")', f'=COUNTIFS({rng("K")},"P3",{rng("F")},"<>N/A",{rng("F")},"<>No")'),
    ("Requirements marked VERIFY for this roadmap", f'=COUNTIF({rng("E")},"VERIFY")', f'=COUNTIF({rng("F")},"VERIFY")'),
]
for i, (lab, a, b) in enumerate(lines, start=9):
    db.cell(row=i, column=1, value=lab).font = BODY
    for j, f in ((2, a), (3, b)):
        c = db.cell(row=i, column=j, value=f); c.font = BODY; c.number_format = "0"; c.border = BORDER
    db.cell(row=i, column=1).border = BORDER
header(db, 15, ["Status (all rows)", "Count", "Share"], [44, 24, 24])
total_ref = f"COUNTA({rng('A')})"
for i, st in enumerate(["Not started", "In progress", "Done", "Not applicable"], start=16):
    db.cell(row=i, column=1, value=st).font = BODY
    c = db.cell(row=i, column=2, value=f'=COUNTIF({rng("Q")},"{st}")'); c.font = BODY; c.number_format = "0"
    s = db.cell(row=i, column=3, value=f"=IFERROR(B{i}/{total_ref},0)"); s.font = BODY; s.number_format = "0.0%"
    for j in (1, 2, 3): db.cell(row=i, column=j).border = BORDER
db.cell(row=20, column=1, value="Total requirement rows").font = BOLD
c = db.cell(row=20, column=2, value=f"={total_ref}"); c.font = BOLD; c.number_format = "0"

# ======================================================================
# Sheet: Sources
# ======================================================================
so = wb.create_sheet("Sources")
title(so, "Sources")
src = [
    ("Regulation (EU) 2024/2847 (Cyber Resilience Act), full text", "https://eur-lex.europa.eu/eli/reg/2024/2847/oj/eng"),
    ("CRA Article 2 (scope and exclusions)", "https://eur-lex.europa.eu/eli/reg/2024/2847/art_2/oj/eng"),
    ("Commission Implementing Regulation (EU) 2025/2392 (important and critical product descriptions)", "https://eur-lex.europa.eu/eli/reg_impl/2025/2392/oj/eng"),
    ("European Commission: CRA reporting", "https://digital-strategy.ec.europa.eu/en/policies/cra-reporting"),
    ("CMS: CRA scope and product classification", "https://cms.law/en/int/legal-updates/does-the-eu-cyber-resilience-act-apply-to-your-product-scope-definitions-and-the-product-classification-system-explained"),
    ("CRA Annex III text (Prighter)", "https://prighter.com/resources/laws/cyber-resilience-act/articles/annex-iii.md"),
    ("Irish NCSC: EU Cyber Resilience Act", "https://ncsc.gov.ie/cra/"),
    ("M1 product page (interfaces, Bluetooth variants)", "https://www.maximatecc.com/maxai-430/"),
    ("Note", "Annex I and II requirement texts in this workbook are paraphrased from the regulation; check the Official Journal text for exact wording."),
]
for i, (a, b) in enumerate(src, start=3):
    so.cell(row=i, column=1, value=a).font = BODY
    c = so.cell(row=i, column=2, value=b); c.font = Font(name=F, size=10, color="0563C1", underline="single") if b.startswith("http") else BODY
    if b.startswith("http"): c.hyperlink = b
so.column_dimensions["A"].width = 80; so.column_dimensions["B"].width = 100

# sheet order
order = ["README", "Dashboard", "Variant comparison", "CRA requirements", "Annex map", "Dual roadmap", "Sources"]
wb._sheets = [wb[n] for n in order]
for s in wb.worksheets:
    s.sheet_view.zoomScale = 90
wb.active = 0
wb.save(OUT)
print("saved", OUT, "rows", len(REQ))
