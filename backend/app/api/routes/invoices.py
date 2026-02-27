from typing import List

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import select
from sqlalchemy.orm import joinedload

from ... import crud, schemas
from ...database import get_db
from ...models import Invoice, Mission
from ...services.invoice import generate_invoice_pdf

router = APIRouter(prefix="/invoices", tags=["invoices"])


@router.get("/")
def list_invoices(db=Depends(get_db)):
    """List all invoices with mission and client info."""
    stmt = select(Invoice).options(
        joinedload(Invoice.mission).joinedload(Mission.client)
    ).order_by(Invoice.issued_at.desc())
    invoices = list(db.execute(stmt).scalars().unique())
    result = []
    for inv in invoices:
        mission = inv.mission
        client = mission.client if mission else None
        result.append({
            "id": inv.id,
            "mission_id": inv.mission_id,
            "mission_title": mission.title if mission else "",
            "client_name": client.name if client else "",
            "pack_name": inv.pack_name,
            "amount": inv.amount,
            "currency": inv.currency,
            "status": inv.status.value if inv.status else "draft",
            "issued_at": inv.issued_at.isoformat() if inv.issued_at else None,
            "due_date": inv.due_date.isoformat() if inv.due_date else None,
            "pdf_path": inv.pdf_path,
        })
    return result


def _mission_or_404(db, mission_id: int) -> Mission:
    mission = crud.get_mission(db, mission_id)
    if not mission:
        raise HTTPException(status_code=404, detail="Mission not found")
    return mission


def _invoice_or_404(db, invoice_id: int) -> Invoice:
    invoice = db.get(Invoice, invoice_id)
    if not invoice:
        raise HTTPException(status_code=404, detail="Invoice not found")
    return invoice


@router.post("/missions/{mission_id}", response_model=schemas.InvoiceOut)
def create_invoice_for_mission(
    mission_id: int, payload: schemas.InvoiceCreate, db=Depends(get_db)
):
    mission = _mission_or_404(db, mission_id)
    if mission.invoice:
        raise HTTPException(status_code=400, detail="Invoice already exists for mission")
    invoice = crud.create_invoice(db, mission, payload)
    pdf_path = generate_invoice_pdf(invoice, mission.client, mission)
    invoice.pdf_path = str(pdf_path)
    db.commit()
    db.refresh(invoice)
    return invoice


@router.get("/{invoice_id}", response_model=schemas.InvoiceOut)
def get_invoice(invoice_id: int, db=Depends(get_db)):
    return _invoice_or_404(db, invoice_id)


@router.post("/{invoice_id}/pay", response_model=schemas.InvoiceOut)
def mark_paid(invoice_id: int, db=Depends(get_db)):
    invoice = _invoice_or_404(db, invoice_id)
    return crud.mark_invoice_paid(db, invoice)

