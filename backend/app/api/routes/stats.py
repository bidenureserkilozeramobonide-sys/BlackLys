from datetime import datetime, timedelta

from fastapi import APIRouter, Depends
from sqlalchemy import case, func, select

from ...database import get_db
from ...models import Client, Gallery, Invoice, InvoiceStatus, Mission, MissionStatus

router = APIRouter(prefix="/stats", tags=["stats"])


@router.get("/dashboard")
def dashboard_stats(db=Depends(get_db)):
    """Aggregated statistics for the dashboard."""
    # ── 1 query: all mission counts by status ──
    mission_row = db.execute(
        select(
            func.count(Mission.id).label("total"),
            func.count(case((Mission.status == MissionStatus.planned, 1))).label("planned"),
            func.count(case((Mission.status == MissionStatus.processing, 1))).label("processing"),
            func.count(case((Mission.status == MissionStatus.delivered, 1))).label("delivered"),
        )
    ).one()

    # ── 1 query: 30-day revenue (paid invoices only) ──
    thirty_days_ago = datetime.utcnow() - timedelta(days=30)
    revenue_30d = db.execute(
        select(func.coalesce(func.sum(Invoice.amount), 0)).where(
            Invoice.status == InvoiceStatus.paid,
            Invoice.issued_at >= thirty_days_ago,
        )
    ).scalar() or 0

    # ── 1 query: pending invoice aggregates (all-time) ──
    pending_row = db.execute(
        select(
            func.coalesce(
                func.sum(case((Invoice.status != InvoiceStatus.paid, Invoice.amount))), 0
            ).label("pending_amount"),
            func.count(case((Invoice.status != InvoiceStatus.paid, 1))).label("pending_count"),
        )
    ).one()

    # ── 1 query: galleries + clients (fast counts) ──
    active_galleries = db.execute(
        select(func.count(Gallery.id)).where(Gallery.is_public.is_(True))
    ).scalar() or 0

    total_clients = db.execute(select(func.count(Client.id))).scalar() or 0

    return {
        "total_missions": mission_row.total,
        "planned": mission_row.planned,
        "processing": mission_row.processing,
        "delivered": mission_row.delivered,
        "active_galleries": active_galleries,
        "revenue_30d": revenue_30d,
        "total_clients": total_clients,
        "pending_amount": pending_row.pending_amount,
        "pending_count": pending_row.pending_count,
    }

