#!/usr/bin/env python3
"""Generates the Wishcraft Mastering Limiter user manual as a PDF.

Source of truth for content lives here, not hand-edited into the PDF -- when a control's
range/default/behavior changes (check Source/PluginProcessor.cpp's createParameterLayout()
for current values), update the relevant section below and regenerate:

    pip3 install reportlab   # once
    python3 generate_manual.py

Requires: reportlab (pip3 install reportlab).
"""

from reportlab.lib.pagesizes import letter
from reportlab.lib.units import inch
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak,
    HRFlowable, KeepTogether
)
from reportlab.pdfgen import canvas

OUT_PATH = "Wishcraft_Mastering_Limiter_Manual.pdf"

# ---- Palette (echoes the plugin's own accent colors without literally being a dark
# theme -- a manual should be light, high-contrast, and printable) ----
NAVY = colors.HexColor("#132A3A")
BLUE = colors.HexColor("#1B6FA8")
BLUE_PALE = colors.HexColor("#EAF2F8")
AMBER = colors.HexColor("#B8790B")
INK = colors.HexColor("#232323")
MUTED = colors.HexColor("#666666")
RULE = colors.HexColor("#D5DCE1")
TABLE_HEAD_BG = colors.HexColor("#132A3A")
TABLE_ALT_BG = colors.HexColor("#F4F7F9")

styles = getSampleStyleSheet()

styles.add(ParagraphStyle(
    name="ManualTitle", fontName="Helvetica-Bold", fontSize=30, leading=34,
    textColor=NAVY, spaceAfter=6, alignment=TA_LEFT))
styles.add(ParagraphStyle(
    name="ManualSubtitle", fontName="Helvetica", fontSize=15, leading=20,
    textColor=BLUE, spaceAfter=4))
styles.add(ParagraphStyle(
    name="CoverMeta", fontName="Helvetica", fontSize=10.5, leading=15,
    textColor=MUTED))
styles.add(ParagraphStyle(
    name="H1", fontName="Helvetica-Bold", fontSize=17, leading=21,
    textColor=NAVY, spaceBefore=4, spaceAfter=8))
styles.add(ParagraphStyle(
    name="H2", fontName="Helvetica-Bold", fontSize=12.5, leading=16,
    textColor=BLUE, spaceBefore=14, spaceAfter=6))
styles.add(ParagraphStyle(
    name="Body", fontName="Helvetica", fontSize=10, leading=14.5,
    textColor=INK, spaceAfter=8, alignment=TA_LEFT))
styles.add(ParagraphStyle(
    name="BodyTight", fontName="Helvetica", fontSize=10, leading=13.5,
    textColor=INK, spaceAfter=3))
styles.add(ParagraphStyle(
    name="Numbered", fontName="Helvetica", fontSize=10, leading=14.5,
    textColor=INK, spaceAfter=6, leftIndent=14))
styles.add(ParagraphStyle(
    name="TipLabel", fontName="Helvetica-Bold", fontSize=10, leading=14,
    textColor=AMBER))
styles.add(ParagraphStyle(
    name="TableHead", fontName="Helvetica-Bold", fontSize=9, leading=11,
    textColor=colors.white))
styles.add(ParagraphStyle(
    name="TableCell", fontName="Helvetica", fontSize=9, leading=12.5,
    textColor=INK))
styles.add(ParagraphStyle(
    name="TableCellBold", fontName="Helvetica-Bold", fontSize=9, leading=12.5,
    textColor=NAVY))
styles.add(ParagraphStyle(
    name="Footer", fontName="Helvetica", fontSize=8, textColor=MUTED))
styles.add(ParagraphStyle(
    name="CaptionSmall", fontName="Helvetica-Oblique", fontSize=9, leading=13,
    textColor=MUTED, spaceAfter=8))


def rule(color=RULE, thickness=0.75, space_before=2, space_after=10):
    return HRFlowable(width="100%", thickness=thickness, color=color,
                       spaceBefore=space_before, spaceAfter=space_after)


def control_table(rows, col_widths):
    """rows[0] is the header row (plain strings); the rest are data rows of any width --
    column 0 renders bold (a name/label column), the remaining columns render plain."""
    data = [[Paragraph(c, styles["TableHead"]) for c in rows[0]]]
    for r in rows[1:]:
        row_cells = [Paragraph(r[0], styles["TableCellBold"])]
        row_cells += [Paragraph(c, styles["TableCell"]) for c in r[1:]]
        data.append(row_cells)
    t = Table(data, colWidths=col_widths, repeatRows=1)
    style = [
        ("BACKGROUND", (0, 0), (-1, 0), TABLE_HEAD_BG),
        ("TOPPADDING", (0, 0), (-1, -1), 5),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
        ("LEFTPADDING", (0, 0), (-1, -1), 7),
        ("RIGHTPADDING", (0, 0), (-1, -1), 7),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LINEBELOW", (0, 0), (-1, 0), 0.75, TABLE_HEAD_BG),
        ("LINEBELOW", (0, 1), (-1, -1), 0.5, RULE),
    ]
    for i in range(1, len(data)):
        if i % 2 == 0:
            style.append(("BACKGROUND", (0, i), (-1, i), TABLE_ALT_BG))
    t.setStyle(TableStyle(style))
    return t


def draw_cover(c, doc):
    c.saveState()
    w, h = letter
    # Top navy band
    c.setFillColor(NAVY)
    c.rect(0, h - 2.6 * inch, w, 2.6 * inch, fill=1, stroke=0)
    c.setFillColor(colors.white)
    c.setFont("Helvetica-Bold", 30)
    c.drawString(0.85 * inch, h - 1.35 * inch, "WISHCRAFT")
    c.drawString(0.85 * inch, h - 1.85 * inch, "MASTERING LIMITER")
    c.setFont("Helvetica", 14)
    c.setFillColor(colors.HexColor("#8FC1E3"))
    c.drawString(0.85 * inch, h - 2.25 * inch, "User Manual")

    c.setFillColor(AMBER)
    c.rect(0.85 * inch, h - 2.75 * inch, 1.6 * inch, 0.035 * inch, fill=1, stroke=0)

    c.setFillColor(INK)
    c.setFont("Helvetica", 11)
    ty = h - 3.4 * inch
    lines = [
        "Version 1.0.0",
        "",
        "A selective-clipping mastering limiter with a genuine",
        "true-peak safety stage — built to take the place of a",
        "separate clipper plugin in front of your limiter.",
    ]
    for ln in lines:
        c.drawString(0.85 * inch, ty, ln)
        ty -= 0.24 * inch

    c.setFont("Helvetica", 10)
    c.setFillColor(MUTED)
    by = 1.1 * inch
    c.drawString(0.85 * inch, by, "Concept, design, and specification by Glenn Burgos")
    c.drawString(0.85 * inch, by - 0.2 * inch, "© 2026 Glenn Burgos")
    c.restoreState()


def footer(c, doc):
    c.saveState()
    c.setStrokeColor(RULE)
    c.setLineWidth(0.5)
    c.line(0.85 * inch, 0.65 * inch, letter[0] - 0.85 * inch, 0.65 * inch)
    c.setFont("Helvetica", 8)
    c.setFillColor(MUTED)
    c.drawString(0.85 * inch, 0.48 * inch, "Wishcraft Mastering Limiter — User Manual")
    c.drawRightString(letter[0] - 0.85 * inch, 0.48 * inch, f"Page {doc.page}")
    c.restoreState()


def first_page(c, doc):
    draw_cover(c, doc)


def later_pages(c, doc):
    footer(c, doc)


def build():
    doc = SimpleDocTemplate(
        OUT_PATH, pagesize=letter,
        leftMargin=0.85 * inch, rightMargin=0.85 * inch,
        topMargin=0.85 * inch, bottomMargin=0.9 * inch,
        title="Wishcraft Mastering Limiter — User Manual",
        author="Glenn Burgos",
    )

    story = []

    # ---------------------------------------------------------------- cover
    story.append(Spacer(1, 0.1))
    story.append(PageBreak())

    # ---------------------------------------------------------------- what this is
    story.append(Paragraph("What This Is", styles["H1"]))
    story.append(rule())
    story.append(Paragraph(
        "Wishcraft Mastering Limiter eliminates the need for a separate clipper in front "
        "of your limiter. Its Selective Clipper can be pushed harder than a standard "
        "clipper without the usual audible cost, taking real load off the limiter before "
        "it ever sees the signal. A True Peak Limiter stage then guarantees the final "
        "output stays under a genuine true-peak target, using smooth gain reduction "
        "rather than a hard clip.", styles["Body"]))

    story.append(Paragraph("Signal Path", styles["H2"]))
    story.append(Paragraph(
        "Selective Clipper → Input Gain (Drive) → Limiter → True Peak Limiter "
        "→ Safety Clip → Output", styles["BodyTight"]))
    story.append(Spacer(1, 4))
    story.append(Paragraph(
        "Input Gain sits after the Selective Clipper and before the Limiter — it's "
        "the “drive into the limiter” control, not a plain input trim.",
        styles["CaptionSmall"]))

    story.append(Paragraph("Recommended Workflow", styles["H2"]))
    workflow = [
        "<b>Selective Clipper</b> — set Threshold and Selectivity first.",
        "<b>Gain</b> — use Input Gain to drive the already-clipped signal into the Limiter.",
        "<b>Limiter</b> — dial in Threshold, Release, Link, and Gain Match last.",
    ]
    for i, w in enumerate(workflow, 1):
        story.append(Paragraph(f"{i}. {w}", styles["Numbered"]))
    story.append(Paragraph(
        "This is the opposite order from most limiter plugins, where you'd normally "
        "start with the limiter itself — here, the Selective Clipper does a lot of "
        "the work before the signal ever reaches the Limiter.", styles["Body"]))

    story.append(Paragraph("Why It's Different", styles["H2"]))
    story.append(Paragraph(
        "The Selective Clipper only touches genuinely brief peaks — anything under "
        "about 5 ms. Sustained loud passages are left alone entirely and handled "
        "downstream by the Limiter's own gain reduction instead. That's why it can run "
        "more aggressively than a standard clipper without sounding like distortion: "
        "it's shaving isolated spikes, not flattening whole passages.", styles["Body"]))
    story.append(Paragraph(
        "Pushed to a similar loudness, the Selective Clipper typically preserves "
        "noticeably more dynamic range (LRA) than a standard clipper. The denser and "
        "more sustained the source material, the more effective it is — on already-"
        "loud, densely-produced material the gap can be substantial; on more dynamic, "
        "transient-heavy material it narrows, since there's less sustained content for "
        "the duration gate to exempt in the first place.", styles["Body"]))

    story.append(PageBreak())

    # ---------------------------------------------------------------- adjusting values
    story.append(Paragraph("Adjusting Values", styles["H1"]))
    story.append(rule())
    adj_rows = [
        ["Action", "Effect"],
        ["Click-drag", "Knobs respond to vertical movement (up increases, down decreases); "
         "sliders track the mouse directly."],
        ["Shift + drag", "Finer resolution — the same drag covers a smaller range of "
         "the control, for precise adjustments."],
        ["Ctrl/Cmd + click", "Opens a text box to type an exact value directly."],
        ["Double-click", "Resets the control to its default value."],
    ]
    story.append(control_table(adj_rows, [1.5 * inch, 4.65 * inch]))
    story.append(Spacer(1, 14))

    story.append(Paragraph("Installation (macOS)", styles["H2"]))
    story.append(Paragraph(
        "Copy the plugin bundles to the standard plugin folders and rescan plugins in "
        "your DAW:", styles["Body"]))
    install_rows = [
        ["Format", "Location"],
        ["VST3", "~/Library/Audio/Plug-Ins/VST3/  (or /Library/Audio/Plug-Ins/VST3/ for all users)"],
        ["Audio Unit (AU)", "~/Library/Audio/Plug-Ins/Components/  (or /Library/Audio/Plug-Ins/Components/ for all users)"],
    ]
    story.append(control_table(install_rows, [1.5 * inch, 4.65 * inch]))
    story.append(Spacer(1, 6))
    story.append(Paragraph(
        "A Standalone application is also included and needs no installation — just "
        "run it directly.", styles["CaptionSmall"]))

    story.append(PageBreak())

    # ---------------------------------------------------------------- control reference
    story.append(Paragraph("Control Reference", styles["H1"]))
    story.append(rule())

    col_widths = [1.35 * inch, 1.15 * inch, 0.75 * inch, 3.0 * inch]

    story.append(Paragraph("Selective Clipper", styles["H2"]))
    story.append(control_table([
        ["Control", "Range", "Default", "What it does"],
        ["Threshold", "−24 to 0 dB", "−3.0 dB", "Level above which the clipper can act. Lower "
         "values let it engage on quieter peaks."],
        ["Selectivity", "0–100%", "50%", "How much of the peaks above the threshold get "
         "clipped. 0% = Transparent, 100% = Aggressive."],
        ["Delta", "On/Off", "Off", "Solos what the Selective Clipper is removing, gained "
         "up so it's audible. For dialing in Threshold and Selectivity by ear — turn "
         "off before rendering."],
        ["Delta Trim", "−12 to +12 dB", "0.0 dB", "Only shown while Delta is on. Delta's "
         "boost can be startlingly loud with aggressive clipping — trim it down here. "
         "0.0 dB leaves Delta's boost unchanged."],
    ], col_widths))

    story.append(Paragraph("Gain", styles["H2"]))
    story.append(control_table([
        ["Control", "Range", "Default", "What it does"],
        ["Input Gain", "0–24 dB", "0.0 dB", "Drives the already-clipped signal into the "
         "Limiter. Sits after the Selective Clipper, before the Limiter — not a plain "
         "input trim."],
    ], col_widths))

    story.append(Paragraph("Shaping EQ", styles["H2"]))
    story.append(control_table([
        ["Control", "Range", "Default", "What it does"],
        ["Low Shelf", "−6 to +6 dB", "0.0 dB", "Shapes what triggers the Selective "
         "Clipper and the Limiter below ~300 Hz."],
        ["High Shelf", "−6 to +6 dB", "0.0 dB", "Same, above ~300 Hz."],
    ], col_widths))
    story.append(Paragraph(
        "Unlike a compressor's sidechain EQ, the Limiter's copy of these filters audibly "
        "colors the output too — treat both shelves as tone controls, not something "
        "silent happening behind the scenes.", styles["CaptionSmall"]))

    story.append(PageBreak())

    story.append(Paragraph("Limiter", styles["H2"]))
    story.append(control_table([
        ["Control", "Range", "Default", "What it does"],
        ["Threshold", "−18 to 0 dB", "−0.1 dB", "The Limiter's gain-reduction target. "
         "A smoothed, program-dependent value the signal can transiently overshoot — "
         "not a hard-enforced ceiling. True-peak safety is the True Peak Limiter's job."],
        ["Lookahead", "0.5–20 ms", "3.0 ms", "How far ahead the Limiter looks to smooth "
         "its gain reduction in before a peak arrives. Changing it triggers a brief "
         "engine reset, same as changing Oversampling."],
        ["Release", "0–100%", "30%", "Program-dependent release speed, blended between "
         "a fast and a slow time constant depending on recent gain-reduction history."],
        ["Link", "0–100%", "75%", "Stereo linking of the two channels' gain reduction, "
         "blended after smoothing using min-gain-wins logic."],
        ["TP Limit", "−3 to 0 dB", "−0.1 dB", "The True Peak Limiter's target ceiling "
         "— see “True Peak Accuracy” below for what this guarantee actually covers."],
        ["Gain Match", "On/Off", "Off", "Keeps output level roughly matched regardless of "
         "gain reduction, for A/B'ing settings without a loudness bias. Strictly cut-"
         "only — never boosts. Turn off before your final render/bounce."],
    ], col_widths))

    story.append(Paragraph("Utility", styles["H2"]))
    story.append(control_table([
        ["Control", "Range", "Default", "What it does"],
        ["Oversampling", "2x / 4x / 8x", "4x", "Higher factors improve true-peak detection "
         "accuracy at the cost of CPU. See “Choosing an Oversampling Factor” below."],
        ["Bypass", "On/Off", "Off", "Full latency-compensated bypass — compares the "
         "true, unprocessed input against your processed output."],
    ], col_widths))

    story.append(PageBreak())

    # ---------------------------------------------------------------- metering
    story.append(Paragraph("Metering", styles["H1"]))
    story.append(rule())
    story.append(control_table([
        ["Meter", "Shows"],
        ["Selective Clip", "How much the clipper is removing, per channel (held peak "
         "value, 1-second hold with 20 dB/sec fall)."],
        ["Gain Reduction", "How hard the Limiter is working, per channel."],
        ["Output", "Final true peak level, per channel, plus Dynamic Range (LRA) and "
         "short-term LUFS. The dot beside each channel turns yellow past your TP Limit, "
         "red past true 0 dBFS."],
    ], [1.5 * inch, 4.65 * inch]))

    story.append(Paragraph("Technical Notes", styles["H1"]))
    story.append(rule())

    story.append(Paragraph("Choosing an Oversampling Factor", styles["H2"]))
    story.append(Paragraph(
        "2x is the lightest setting; 4x (the default) is a good balance of accuracy and "
        "CPU use for most sessions. 8x gives the True Peak Limiter the finest-resolution "
        "peak estimate and is recommended for final masters on demanding material, "
        "particularly dense, loud, transient-heavy content — CPU headroom is ample on "
        "any reasonably current machine.", styles["Body"]))

    story.append(Paragraph("Latency", styles["H2"]))
    story.append(Paragraph(
        "The plugin reports its total processing latency to your host, which "
        "compensates for it automatically in any PDC-aware DAW. Latency depends on the "
        "Selective Clipper's internal budget, the Limiter's Lookahead setting, and the "
        "True Peak Limiter's own fixed lookahead — it does <b>not</b> depend on the "
        "Oversampling factor. Changing Lookahead updates the reported latency "
        "immediately.", styles["Body"]))

    story.append(Paragraph("True Peak Accuracy", styles["H2"]))
    story.append(Paragraph(
        "The True Peak Limiter uses smooth gain reduction, not a hard clip, specifically "
        "because hard-clipping in the oversampled domain has been measured to still "
        "overshoot on reconstruction — the same failure mode any hard-clip-based "
        "“true peak” limiter has. On realistic program material, TP Limit is held "
        "to within roughly 0.5 dB. As with any true-peak limiter, an extreme synthetic "
        "test signal (a sustained full-scale tone parked right at the edge of Nyquist) "
        "can defeat any oversampled true-peak estimator by a wider margin — this is a "
        "known, industry-wide limitation of the technique (ITU-R BS.1770's own true-"
        "peak specification documents a comparable-order-of-magnitude bound), not "
        "something unique to this plugin. For critical delivery on unusually dense or "
        "loud transient masters, verify the final render with a dedicated true-peak "
        "meter, the same practice recommended for any true-peak limiter.", styles["Body"]))

    story.append(Spacer(1, 10))

    # ---------------------------------------------------------------- credits
    credits_block = [
        Paragraph("Credits", styles["H1"]),
        rule(),
        Paragraph(
            "Wishcraft Mastering Limiter — concept, design, and specification by "
            "<b>Glenn Burgos</b>.", styles["Body"]),
        Paragraph(
            "Built with the <b>JUCE</b> framework.", styles["Body"]),
        Spacer(1, 10),
        Paragraph("© 2026 Glenn Burgos.", styles["BodyTight"]),
    ]
    story.append(KeepTogether(credits_block))

    doc.build(story, onFirstPage=first_page, onLaterPages=later_pages)
    print(f"Wrote {OUT_PATH}")


if __name__ == "__main__":
    build()
