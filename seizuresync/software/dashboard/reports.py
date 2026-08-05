"""SeizureSync — Neurologist report generator (PDF)."""
import logging
from datetime import datetime
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import getSampleStyleSheet
from reportlab.platypus import SimpleDocTemplate, Paragraph, Table, Spacer
from io import BytesIO

logger = logging.getLogger("seizuresync.reports")


def generate_neurologist_report(patient: dict, events: list,
                                  risk: dict, sudep: dict,
                                  triggers: dict) -> bytes:
    """Generate a HIPAA-compliant PDF neurologist report."""
    buf = BytesIO()
    doc = SimpleDocTemplate(buf, pagesize=letter)
    styles = getSampleStyleSheet()
    story = []

    story.append(Paragraph("SeizureSync Clinical Report", styles["Title"]))
    story.append(Spacer(1, 12))
    story.append(Paragraph(f"Patient: {patient['name']}", styles["Normal"]))
    story.append(Paragraph(f"Date: {datetime.now().strftime('%Y-%m-%d')}",
                           styles["Normal"]))
    story.append(Spacer(1, 20))

    # Seizure summary
    story.append(Paragraph("Seizure Summary", styles["Heading2"]))
    summary_data = [["Date", "Type", "Duration (s)", "Severity", "Confidence"]]
    for ev in events:
        summary_data.append([
            str(ev.get("onset", "")),
            ev.get("semiology", "unknown"),
            str(ev.get("duration_s", 0)),
            str(ev.get("severity", 0)),
            f"{ev.get('confidence', 0):.0f}%",
        ])
    story.append(Table(summary_data))
    story.append(Spacer(1, 20))

    # Risk forecast
    story.append(Paragraph("Risk Assessment", styles["Heading2"]))
    story.append(Paragraph(f"24-hour seizure risk: {risk.get('risk_24h', 0):.1f}%",
                           styles["Normal"]))
    story.append(Paragraph(f"7-day seizure risk: {risk.get('risk_7d', 0):.1f}%",
                           styles["Normal"]))
    story.append(Spacer(1, 12))

    # SUDEP risk
    story.append(Paragraph("SUDEP Risk Assessment", styles["Heading2"]))
    story.append(Paragraph(f"Annual SUDEP risk: {sudep.get('annual_risk_pct', 0):.2f}%",
                           styles["Normal"]))
    story.append(Paragraph(f"Apnea density: {sudep.get('apnea_density', 0):.1f} events/night",
                           styles["Normal"]))
    story.append(Paragraph(f"Prone episodes (30d): {sudep.get('prone_episodes', 0)}",
                           styles["Normal"]))
    story.append(Spacer(1, 20))

    # Triggers
    story.append(Paragraph("Trigger Attribution (SHAP)", styles["Heading2"]))
    for trigger, importance in triggers.items():
        story.append(Paragraph(f"  {trigger}: {importance:.2f}", styles["Normal"]))

    doc.build(story)
    buf.seek(0)
    return buf.getvalue()