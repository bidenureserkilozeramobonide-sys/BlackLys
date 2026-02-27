from secrets import token_urlsafe
from typing import Iterable, List, Optional

from sqlalchemy import select
from sqlalchemy.orm import Session, joinedload

from . import schemas
from .config import SHARE_LINK_BASE
from .models import Client, Gallery, Invoice, InvoiceStatus, Mission, MissionStatus, Photo


# Clients
def create_client(db: Session, payload: schemas.ClientCreate) -> Client:
    client = Client(**payload.dict())
    db.add(client)
    db.commit()
    db.refresh(client)
    return client


def list_clients(db: Session) -> List[Client]:
    return list(db.execute(select(Client)).scalars())


def get_client(db: Session, client_id: int) -> Optional[Client]:
    return db.get(Client, client_id)


def update_client(db: Session, client: Client, payload: schemas.ClientUpdate) -> Client:
    for field, value in payload.dict(exclude_unset=True).items():
        setattr(client, field, value)
    db.commit()
    db.refresh(client)
    return client


def delete_client(db: Session, client: Client) -> None:
    db.delete(client)
    db.commit()


# Missions
def create_mission(db: Session, payload: schemas.MissionCreate) -> Mission:
    mission = Mission(**payload.dict())
    db.add(mission)
    db.commit()
    db.refresh(mission)
    return mission


def list_missions(db: Session, status: Optional[MissionStatus] = None) -> List[Mission]:
    stmt = select(Mission).options(joinedload(Mission.client))
    if status:
        stmt = stmt.where(Mission.status == status)
    return list(db.execute(stmt).scalars().unique())


def get_mission(db: Session, mission_id: int) -> Optional[Mission]:
    return db.get(Mission, mission_id)


def update_mission(db: Session, mission: Mission, payload: schemas.MissionUpdate) -> Mission:
    for field, value in payload.dict(exclude_unset=True).items():
        setattr(mission, field, value)
    db.commit()
    db.refresh(mission)
    return mission


def delete_mission(db: Session, mission: Mission) -> None:
    db.delete(mission)
    db.commit()


# Photos
def add_photos(db: Session, mission: Mission, files: Iterable[schemas.PhotoOut]) -> List[Photo]:
    photos = []
    for f in files:
        photo = Photo(
            mission_id=mission.id,
            bracket_group=getattr(f, "bracket_group", None),
            raw_path=f.raw_path,
            output_path=getattr(f, "output_path", None),
            is_hdr=getattr(f, "is_hdr", False),
        )
        db.add(photo)
        photos.append(photo)
    db.commit()
    for photo in photos:
        db.refresh(photo)
    return photos


# Galleries
def _generate_share_token() -> str:
    return token_urlsafe(12)


def create_gallery(
    db: Session, mission: Mission, payload: schemas.GalleryCreate, zip_path: Optional[str] = None
) -> Gallery:
    token = _generate_share_token()
    gallery = Gallery(
        mission_id=mission.id,
        title=payload.title,
        share_token=token,
        zip_path=zip_path,
        is_public=payload.is_public,
    )
    db.add(gallery)
    db.commit()
    db.refresh(gallery)
    return gallery


def build_share_url(token: str) -> str:
    return f"{SHARE_LINK_BASE}/g/{token}"


# Invoices
def create_invoice(db: Session, mission: Mission, payload: schemas.InvoiceCreate) -> Invoice:
    invoice = Invoice(
        mission_id=mission.id,
        pack_name=payload.pack_name,
        amount=payload.amount,
        currency=payload.currency,
        due_date=payload.due_date,
        status=InvoiceStatus.draft,
    )
    db.add(invoice)
    db.commit()
    db.refresh(invoice)
    return invoice


def mark_invoice_paid(db: Session, invoice: Invoice) -> Invoice:
    invoice.status = InvoiceStatus.paid
    db.commit()
    db.refresh(invoice)
    return invoice

