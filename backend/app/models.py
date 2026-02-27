from datetime import datetime
import enum
from sqlalchemy import (
    Boolean,
    Column,
    DateTime,
    Enum,
    Float,
    ForeignKey,
    Integer,
    String,
    Text,
)
from sqlalchemy.orm import relationship

from .database import Base


class MissionStatus(str, enum.Enum):
    planned = "planned"
    processing = "processing"
    delivered = "delivered"
    paid = "paid"


class InvoiceStatus(str, enum.Enum):
    draft = "draft"
    sent = "sent"
    paid = "paid"


class Client(Base):
    __tablename__ = "clients"

    id = Column(Integer, primary_key=True, index=True)
    name = Column(String(255), nullable=False)
    company = Column(String(255), nullable=True)
    email = Column(String(255), nullable=True, index=True)
    phone = Column(String(64), nullable=True)
    notes = Column(Text, nullable=True)
    created_at = Column(DateTime, default=datetime.utcnow, nullable=False)
    updated_at = Column(
        DateTime, default=datetime.utcnow, onupdate=datetime.utcnow, nullable=False
    )

    missions = relationship("Mission", back_populates="client", cascade="all,delete")


class Mission(Base):
    __tablename__ = "missions"

    id = Column(Integer, primary_key=True, index=True)
    client_id = Column(Integer, ForeignKey("clients.id"), nullable=False)
    title = Column(String(255), nullable=False)
    address = Column(String(255), nullable=False)
    pack_name = Column(String(128), nullable=False)
    price = Column(Float, nullable=False)
    currency = Column(String(8), default="EUR", nullable=False)
    shoot_date = Column(DateTime, nullable=True)
    status = Column(Enum(MissionStatus), default=MissionStatus.planned, nullable=False)
    notes = Column(Text, nullable=True)
    created_at = Column(DateTime, default=datetime.utcnow, nullable=False)
    updated_at = Column(
        DateTime, default=datetime.utcnow, onupdate=datetime.utcnow, nullable=False
    )

    client = relationship("Client", back_populates="missions")
    photos = relationship("Photo", back_populates="mission", cascade="all,delete")
    gallery = relationship(
        "Gallery", back_populates="mission", uselist=False, cascade="all,delete"
    )
    invoice = relationship(
        "Invoice", back_populates="mission", uselist=False, cascade="all,delete"
    )


class Photo(Base):
    __tablename__ = "photos"

    id = Column(Integer, primary_key=True)
    mission_id = Column(Integer, ForeignKey("missions.id"), nullable=False)
    bracket_group = Column(String(64), nullable=True)
    raw_path = Column(String(500), nullable=False)
    output_path = Column(String(500), nullable=True)
    is_hdr = Column(Boolean, default=False, nullable=False)
    created_at = Column(DateTime, default=datetime.utcnow, nullable=False)

    mission = relationship("Mission", back_populates="photos")


class Gallery(Base):
    __tablename__ = "galleries"

    id = Column(Integer, primary_key=True)
    mission_id = Column(Integer, ForeignKey("missions.id"), nullable=False, unique=True)
    title = Column(String(255), nullable=False)
    share_token = Column(String(64), unique=True, index=True, nullable=False)
    zip_path = Column(String(500), nullable=True)
    is_public = Column(Boolean, default=True, nullable=False)
    created_at = Column(DateTime, default=datetime.utcnow, nullable=False)

    mission = relationship("Mission", back_populates="gallery")


class Invoice(Base):
    __tablename__ = "invoices"

    id = Column(Integer, primary_key=True)
    mission_id = Column(Integer, ForeignKey("missions.id"), nullable=False, unique=True)
    pack_name = Column(String(128), nullable=False)
    amount = Column(Float, nullable=False)
    currency = Column(String(8), default="EUR", nullable=False)
    pdf_path = Column(String(500), nullable=True)
    status = Column(Enum(InvoiceStatus), default=InvoiceStatus.draft, nullable=False)
    due_date = Column(DateTime, nullable=True)
    issued_at = Column(DateTime, default=datetime.utcnow, nullable=False)

    mission = relationship("Mission", back_populates="invoice")


class QuoteStatus(str, enum.Enum):
    draft = "draft"
    sent = "sent"
    accepted = "accepted"
    rejected = "rejected"


class Quote(Base):
    __tablename__ = "quotes"

    id = Column(Integer, primary_key=True, index=True)
    client_id = Column(Integer, ForeignKey("clients.id"), nullable=True)
    mission_id = Column(Integer, ForeignKey("missions.id"), nullable=True)
    number = Column(String(32), unique=True, nullable=False)
    date = Column(String(10), nullable=False)
    valid_until = Column(String(10), nullable=True)
    status = Column(
        Enum(QuoteStatus), default=QuoteStatus.draft, nullable=False
    )
    subtotal = Column(Float, default=0.0, nullable=False)
    tax_rate = Column(Float, default=20.0, nullable=False)
    tax_amount = Column(Float, default=0.0, nullable=False)
    total = Column(Float, default=0.0, nullable=False)
    notes = Column(Text, nullable=True)
    created_at = Column(DateTime, default=datetime.utcnow, nullable=False)
    updated_at = Column(
        DateTime, default=datetime.utcnow, onupdate=datetime.utcnow, nullable=False
    )

    client = relationship("Client")
    mission = relationship("Mission")
    items = relationship("QuoteItem", back_populates="quote", cascade="all,delete")


class QuoteItem(Base):
    __tablename__ = "quote_items"

    id = Column(Integer, primary_key=True, index=True)
    quote_id = Column(Integer, ForeignKey("quotes.id"), nullable=False)
    description = Column(String(500), nullable=False)
    quantity = Column(Float, default=1.0, nullable=False)
    unit_price = Column(Float, default=0.0, nullable=False)
    total = Column(Float, default=0.0, nullable=False)

    quote = relationship("Quote", back_populates="items")

