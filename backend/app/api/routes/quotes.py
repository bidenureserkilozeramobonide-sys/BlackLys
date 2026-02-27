from typing import List, Optional

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import select, func
from sqlalchemy.orm import joinedload

from ... import schemas
from ...database import get_db
from ...models import Client, Quote, QuoteItem, QuoteStatus

router = APIRouter(prefix="/quotes", tags=["quotes"])


def _next_number(db) -> str:
    """Generate the next quote number DEV-XXXX."""
    result = db.execute(
        select(func.max(Quote.id))
    ).scalar()
    next_id = (result or 0) + 1
    return f"DEV-{next_id:04d}"


def _recalculate(db, quote: Quote):
    """Recalculate subtotal, tax_amount, total from items."""
    items = list(db.execute(
        select(QuoteItem).where(QuoteItem.quote_id == quote.id)
    ).scalars())
    subtotal = sum(item.total for item in items)
    quote.subtotal = subtotal
    quote.tax_amount = subtotal * quote.tax_rate / 100.0
    quote.total = subtotal + quote.tax_amount
    db.commit()
    db.refresh(quote)


def _quote_or_404(db, quote_id: int) -> Quote:
    quote = db.get(Quote, quote_id)
    if not quote:
        raise HTTPException(status_code=404, detail="Quote not found")
    return quote


# ── List all quotes ──
@router.get("/")
def list_quotes(status: Optional[str] = None, db=Depends(get_db)):
    stmt = select(Quote).options(
        joinedload(Quote.client),
        joinedload(Quote.items),
    ).order_by(Quote.date.desc())

    if status:
        try:
            qs = QuoteStatus(status)
            stmt = stmt.where(Quote.status == qs)
        except ValueError:
            pass

    quotes = list(db.execute(stmt).scalars().unique())
    result = []
    for q in quotes:
        result.append({
            "id": q.id,
            "client_id": q.client_id,
            "mission_id": q.mission_id,
            "number": q.number,
            "date": q.date,
            "valid_until": q.valid_until,
            "status": q.status.value if q.status else "draft",
            "subtotal": q.subtotal,
            "tax_rate": q.tax_rate,
            "tax_amount": q.tax_amount,
            "total": q.total,
            "notes": q.notes,
            "client_name": q.client.name if q.client else None,
            "items": [
                {
                    "id": item.id,
                    "quote_id": item.quote_id,
                    "description": item.description,
                    "quantity": item.quantity,
                    "unit_price": item.unit_price,
                    "total": item.total,
                }
                for item in q.items
            ],
            "created_at": q.created_at.isoformat() if q.created_at else None,
            "updated_at": q.updated_at.isoformat() if q.updated_at else None,
        })
    return result


# ── Get single quote ──
@router.get("/{quote_id}")
def get_quote(quote_id: int, db=Depends(get_db)):
    q = _quote_or_404(db, quote_id)
    db.refresh(q)
    items = list(db.execute(
        select(QuoteItem).where(QuoteItem.quote_id == q.id)
    ).scalars())
    return {
        "id": q.id,
        "client_id": q.client_id,
        "mission_id": q.mission_id,
        "number": q.number,
        "date": q.date,
        "valid_until": q.valid_until,
        "status": q.status.value if q.status else "draft",
        "subtotal": q.subtotal,
        "tax_rate": q.tax_rate,
        "tax_amount": q.tax_amount,
        "total": q.total,
        "notes": q.notes,
        "client_name": q.client.name if q.client else None,
        "items": [
            {
                "id": item.id,
                "quote_id": item.quote_id,
                "description": item.description,
                "quantity": item.quantity,
                "unit_price": item.unit_price,
                "total": item.total,
            }
            for item in items
        ],
        "created_at": q.created_at.isoformat() if q.created_at else None,
        "updated_at": q.updated_at.isoformat() if q.updated_at else None,
    }


# ── Create quote ──
@router.post("/", status_code=201)
def create_quote(payload: schemas.QuoteCreate, db=Depends(get_db)):
    number = _next_number(db)
    quote = Quote(
        client_id=payload.client_id,
        mission_id=payload.mission_id,
        number=number,
        date=payload.date,
        valid_until=payload.valid_until,
        status=payload.status,
        tax_rate=payload.tax_rate,
        notes=payload.notes,
    )
    db.add(quote)
    db.commit()
    db.refresh(quote)

    # Add items
    for item_data in payload.items:
        total = item_data.quantity * item_data.unit_price
        item = QuoteItem(
            quote_id=quote.id,
            description=item_data.description,
            quantity=item_data.quantity,
            unit_price=item_data.unit_price,
            total=total,
        )
        db.add(item)
    db.commit()

    _recalculate(db, quote)
    return get_quote(quote.id, db)


# ── Update quote ──
@router.put("/{quote_id}")
def update_quote(quote_id: int, payload: schemas.QuoteUpdate, db=Depends(get_db)):
    quote = _quote_or_404(db, quote_id)
    for field, value in payload.dict(exclude_unset=True).items():
        if field == "status" and value is not None:
            setattr(quote, field, QuoteStatus(value))
        else:
            setattr(quote, field, value)
    db.commit()
    db.refresh(quote)
    _recalculate(db, quote)
    return get_quote(quote.id, db)


# ── Delete quote ──
@router.delete("/{quote_id}", status_code=204)
def delete_quote(quote_id: int, db=Depends(get_db)):
    quote = _quote_or_404(db, quote_id)
    db.delete(quote)
    db.commit()
    return None


# ── Add item to quote ──
@router.post("/{quote_id}/items", status_code=201)
def add_item(quote_id: int, payload: schemas.QuoteItemCreate, db=Depends(get_db)):
    quote = _quote_or_404(db, quote_id)
    total = payload.quantity * payload.unit_price
    item = QuoteItem(
        quote_id=quote.id,
        description=payload.description,
        quantity=payload.quantity,
        unit_price=payload.unit_price,
        total=total,
    )
    db.add(item)
    db.commit()
    db.refresh(item)
    _recalculate(db, quote)
    return {
        "id": item.id,
        "quote_id": item.quote_id,
        "description": item.description,
        "quantity": item.quantity,
        "unit_price": item.unit_price,
        "total": item.total,
    }


# ── Remove item ──
@router.delete("/{quote_id}/items/{item_id}", status_code=204)
def remove_item(quote_id: int, item_id: int, db=Depends(get_db)):
    quote = _quote_or_404(db, quote_id)
    item = db.get(QuoteItem, item_id)
    if not item or item.quote_id != quote.id:
        raise HTTPException(status_code=404, detail="Item not found")
    db.delete(item)
    db.commit()
    _recalculate(db, quote)
    return None


# ── Convert quote to invoice ──
@router.post("/{quote_id}/convert", status_code=201)
def convert_to_invoice(quote_id: int, db=Depends(get_db)):
    from ...models import Invoice, InvoiceStatus
    from datetime import datetime

    quote = _quote_or_404(db, quote_id)

    invoice = Invoice(
        mission_id=quote.mission_id,
        pack_name=f"Devis {quote.number}",
        amount=quote.total,
        currency="EUR",
        status=InvoiceStatus.draft,
        due_date=datetime.utcnow(),
    )
    db.add(invoice)

    # Mark quote as accepted
    quote.status = QuoteStatus.accepted
    db.commit()
    db.refresh(invoice)

    return {
        "invoice_id": invoice.id,
        "quote_id": quote.id,
        "message": f"Quote {quote.number} converted to invoice #{invoice.id}",
    }
