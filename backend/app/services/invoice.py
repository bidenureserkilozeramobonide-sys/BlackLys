from datetime import datetime
from pathlib import Path

from fpdf import FPDF

from ..config import INVOICES_DIR
from ..models import Client, Invoice, Mission


def generate_invoice_pdf(invoice: Invoice, client: Client, mission: Mission) -> Path:
    """
    Generate a lightweight PDF invoice and return the path.
    """
    output_dir = INVOICES_DIR / f"mission_{mission.id}"
    output_dir.mkdir(parents=True, exist_ok=True)
    pdf_path = output_dir / f"invoice_{invoice.id}.pdf"

    pdf = FPDF()
    pdf.add_page()
    pdf.set_font("Helvetica", "B", 16)
    pdf.cell(0, 10, "Invoice", ln=True)
    pdf.set_font("Helvetica", "", 12)
    pdf.cell(0, 10, f"Invoice ID: {invoice.id}", ln=True)
    pdf.cell(0, 10, f"Date: {datetime.utcnow().date()}", ln=True)

    pdf.ln(4)
    pdf.set_font("Helvetica", "B", 12)
    pdf.cell(0, 8, "Bill To:", ln=True)
    pdf.set_font("Helvetica", "", 12)
    pdf.cell(0, 8, client.name, ln=True)
    if client.company:
        pdf.cell(0, 8, client.company, ln=True)
    if client.email:
        pdf.cell(0, 8, client.email, ln=True)
    if client.phone:
        pdf.cell(0, 8, client.phone, ln=True)

    pdf.ln(6)
    pdf.set_font("Helvetica", "B", 12)
    pdf.cell(0, 8, "Mission:", ln=True)
    pdf.set_font("Helvetica", "", 12)
    pdf.multi_cell(0, 8, f"{mission.title} - {mission.address}")
    pdf.cell(0, 8, f"Pack: {invoice.pack_name}", ln=True)

    pdf.ln(6)
    pdf.set_font("Helvetica", "B", 12)
    pdf.cell(0, 8, "Amount", ln=True)
    pdf.set_font("Helvetica", "", 12)
    pdf.cell(0, 8, f"{invoice.amount:.2f} {invoice.currency}", ln=True)

    if invoice.due_date:
        pdf.ln(4)
        pdf.cell(0, 8, f"Due date: {invoice.due_date.date()}", ln=True)

    pdf.output(str(pdf_path))
    return pdf_path

