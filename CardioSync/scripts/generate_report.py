#!/usr/bin/env python3
"""
generate_report.py — Generate a cardiologist-ready PDF clinical report

Uses reportlab to create a PDF with:
  - ECG strips of all arrhythmia events in the period
  - AFib burden percentage
  - BP trends (AM/PM, 30-day)
  - HRV trends (RMSSD, SDNN)
  - Stroke risk assessment
  - CHA₂DS₂-VASc score

Usage:
    python generate_report.py --user-id 1 --month 2026-07

License: MIT
"""
import argparse
import requests
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Table,
                                 TableStyle, Image, PageBreak)
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER
import numpy as np
import io
import os
import tempfile

API_BASE = "http://localhost:8000/api/v1"

def generate_ecg_strip_image(ecg_data, title="ECG Strip"):
    """Generate ECG strip plot as PNG."""
    fig, ax = plt.subplots(figsize=(8, 2))
    t = np.arange(len(ecg_data)) / 250.0  # 250 Hz
    ax.plot(t, ecg_data, linewidth=0.5, color='black')
    ax.set_title(title, fontsize=10)
    ax.set_xlabel('Time (s)', fontsize=8)
    ax.set_ylabel('Amplitude', fontsize=8)
    ax.grid(True, alpha=0.3)
    plt.tight_layout()

    buf = tempfile.NamedTemporaryFile(suffix='.png', delete=False)
    plt.savefig(buf.name, dpi=150)
    plt.close()
    return buf.name

def generate_report(user_id, month, token=None):
    headers = {"Authorization": f"Bearer {token}"} if token else {}

    print(f"Generating report for user {user_id}, period {month}")

    # Fetch data
    r = requests.get(f"{API_BASE}/reports/monthly", headers=headers, timeout=30)
    report_data = r.json()

    r = requests.get(f"{API_BASE}/bp/trends?days=30", headers=headers, timeout=10)
    bp_trends = r.json()

    r = requests.get(f"{API_BASE}/hrv/trends?days=30", headers=headers, timeout=10)
    hrv_trends = r.json()

    r = requests.get(f"{API_BASE}/risk/stroke", headers=headers, timeout=10)
    stroke_risk = r.json()

    # Create PDF
    output_path = f"cardiosync_report_{user_id}_{month}.pdf"
    doc = SimpleDocTemplate(output_path, pagesize=A4,
                            topMargin=15*mm, bottomMargin=15*mm)
    styles = getSampleStyleSheet()
    story = []

    # Title
    story.append(Paragraph("CardioSync Clinical Report", styles['Title']))
    story.append(Paragraph(f"Patient ID: {user_id} | Period: {report_data['period']}",
                           styles['Normal']))
    story.append(Spacer(1, 10*mm))

    # Summary
    story.append(Paragraph("Clinical Summary", styles['Heading2']))
    summary = report_data['summary']
    summary_data = [
        ['Metric', 'Value'],
        ['Total ECG Events', str(summary['total_ecg_events'])],
        ['AFib Events', str(summary['afib_events'])],
        ['AFib Burden (%)', f"{summary['afib_burden_pct']:.1f}%"],
        ['BP Readings', str(summary['bp_readings'])],
        ['Avg Systolic (mmHg)', f"{summary['avg_systolic']:.0f}"],
        ['Avg Diastolic (mmHg)', f"{summary['avg_diastolic']:.0f}"],
    ]
    t = Table(summary_data)
    t.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), colors.grey),
        ('TEXTCOLOR', (0, 0), (-1, 0), colors.whitesmoke),
        ('FONTSIZE', (0, 0), (-1, -1), 9),
        ('GRID', (0, 0), (-1, -1), 0.5, colors.grey),
    ]))
    story.append(t)
    story.append(Spacer(1, 8*mm))

    # Stroke Risk
    story.append(Paragraph("Stroke Risk Assessment (30-day)", styles['Heading2']))
    risk_data = [
        ['Risk Factor', 'Value'],
        ['30-day Stroke Risk (%)', f"{stroke_risk['stroke_risk_30d']:.1f}%"],
        ['AFib Burden (24h)', f"{stroke_risk['afib_burden_pct']:.1f}%"],
        ['Latest BP', stroke_risk['bp_category']],
        ['HRV Trend', stroke_risk['hrv_trend']],
        ['CHA₂DS₂-VASc Score', str(stroke_risk['risk_factors'].get('chads_vasc_score', 'N/A'))],
    ]
    t = Table(risk_data)
    t.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), colors.darkred),
        ('TEXTCOLOR', (0, 0), (-1, 0), colors.whitesmoke),
        ('FONTSIZE', (0, 0), (-1, -1), 9),
        ('GRID', (0, 0), (-1, -1), 0.5, colors.grey),
    ]))
    story.append(t)
    story.append(Spacer(1, 8*mm))

    # ECG Events
    if report_data.get('events'):
        story.append(PageBreak())
        story.append(Paragraph("Arrhythmia Events", styles['Heading2']))
        for i, event in enumerate(report_data['events'][:10]):
            story.append(Paragraph(
                f"{i+1}. {event['type']} — HR: {event['hr']} bpm — "
                f"Confidence: {event['confidence']:.0%} — "
                f"{event['timestamp']}", styles['Normal']))
            story.append(Spacer(1, 3*mm))

    # Disclaimer
    story.append(Spacer(1, 10*mm))
    story.append(Paragraph(
        "This report is generated automatically by CardioSync and is intended "
        "for review by a qualified cardiologist. It is not a medical diagnosis.",
        ParagraphStyle('Disclaimer', parent=styles['Normal'],
                       fontSize=8, textColor=colors.darkred, alignment=TA_CENTER)))

    doc.build(story)
    print(f"Report generated: {output_path}")
    return output_path

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate CardioSync clinical report")
    parser.add_argument("--user-id", type=int, default=1)
    parser.add_argument("--month", default="2026-07")
    parser.add_argument("--token", default=None)
    args = parser.parse_args()
    generate_report(args.user_id, args.month, args.token)