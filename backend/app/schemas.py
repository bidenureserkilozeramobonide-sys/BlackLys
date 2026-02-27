from datetime import datetime
from typing import List, Optional

from pydantic import BaseModel, EmailStr, Field

from .models import InvoiceStatus, MissionStatus, QuoteStatus


class ClientBase(BaseModel):
    name: str
    company: Optional[str] = None
    email: Optional[EmailStr] = None
    phone: Optional[str] = None
    notes: Optional[str] = None


class ClientCreate(ClientBase):
    pass


class ClientUpdate(BaseModel):
    name: Optional[str] = None
    company: Optional[str] = None
    email: Optional[EmailStr] = None
    phone: Optional[str] = None
    notes: Optional[str] = None


class ClientOut(ClientBase):
    id: int
    created_at: datetime
    updated_at: datetime

    class Config:
        orm_mode = True


class MissionBase(BaseModel):
    client_id: int
    title: str
    address: str
    pack_name: str
    price: float
    currency: str = "EUR"
    shoot_date: Optional[datetime] = None
    status: MissionStatus = MissionStatus.planned
    notes: Optional[str] = None


class MissionCreate(MissionBase):
    pass


class MissionUpdate(BaseModel):
    title: Optional[str] = None
    address: Optional[str] = None
    pack_name: Optional[str] = None
    price: Optional[float] = None
    currency: Optional[str] = None
    shoot_date: Optional[datetime] = None
    status: Optional[MissionStatus] = None
    notes: Optional[str] = None


class MissionOut(MissionBase):
    id: int
    created_at: datetime
    updated_at: datetime
    client: Optional[ClientOut] = None

    class Config:
        orm_mode = True


class PhotoOut(BaseModel):
    id: int
    bracket_group: Optional[str] = None
    raw_path: str
    output_path: Optional[str] = None
    is_hdr: bool
    created_at: datetime

    class Config:
        orm_mode = True


class GalleryCreate(BaseModel):
    title: str
    is_public: bool = True


class GalleryOut(BaseModel):
    id: int
    mission_id: int
    title: str
    share_token: str
    share_url: str
    zip_path: Optional[str] = None
    is_public: bool
    created_at: datetime

    class Config:
        orm_mode = True


class InvoiceCreate(BaseModel):
    pack_name: str
    amount: float
    currency: str = "EUR"
    due_date: Optional[datetime] = None


class InvoiceOut(BaseModel):
    id: int
    mission_id: int
    pack_name: str
    amount: float
    currency: str
    pdf_path: Optional[str]
    status: InvoiceStatus
    due_date: Optional[datetime]
    issued_at: datetime

    class Config:
        orm_mode = True


# ── Quotes ──

class QuoteItemCreate(BaseModel):
    description: str
    quantity: float = 1.0
    unit_price: float = 0.0


class QuoteItemOut(BaseModel):
    id: int
    quote_id: int
    description: str
    quantity: float
    unit_price: float
    total: float

    class Config:
        orm_mode = True


class QuoteCreate(BaseModel):
    client_id: Optional[int] = None
    mission_id: Optional[int] = None
    date: str
    valid_until: Optional[str] = None
    status: QuoteStatus = QuoteStatus.draft
    tax_rate: float = 20.0
    notes: Optional[str] = None
    items: List[QuoteItemCreate] = []


class QuoteUpdate(BaseModel):
    client_id: Optional[int] = None
    mission_id: Optional[int] = None
    date: Optional[str] = None
    valid_until: Optional[str] = None
    status: Optional[QuoteStatus] = None
    tax_rate: Optional[float] = None
    notes: Optional[str] = None


class QuoteOut(BaseModel):
    id: int
    client_id: Optional[int]
    mission_id: Optional[int]
    number: str
    date: str
    valid_until: Optional[str]
    status: QuoteStatus
    subtotal: float
    tax_rate: float
    tax_amount: float
    total: float
    notes: Optional[str]
    client_name: Optional[str] = None
    items: List[QuoteItemOut] = []
    created_at: datetime
    updated_at: datetime

    class Config:
        orm_mode = True


class HDRSettings(BaseModel):
    # Light panel
    exposure: float = Field(1.0, description="Global exposure multiplier")
    contrast: float = Field(1.0, description="Contrast multiplier")
    highlights: float = Field(0.0, description="Recover highlights (-1..1)")
    shadows: float = Field(0.0, description="Lift shadows (-1..1)")
    whites: float = Field(0.0, description="White point adjustment (-1..1)")
    blacks: float = Field(0.0, description="Black point adjustment (-1..1)")
    
    # Color panel
    temperature: float = Field(0.0, description="Color temperature shift (-100..100, neg=cool, pos=warm)")
    tint: float = Field(0.0, description="Green/Magenta tint shift (-100..100)")
    vibrance: float = Field(0.0, description="Vibrance boost (-1..1)")
    saturation: float = Field(1.0, description="Saturation multiplier")
    white_balance: Optional[List[float]] = Field(
        None, description="RGB gains, e.g. [1.0, 1.0, 1.0]"
    )
    
    # Detail panel
    sharpening: float = Field(0.0, description="Sharpening amount (0..100)")
    noise_reduction: float = Field(0.0, description="Noise reduction (0..100)")
    
    # Effects panel
    dehaze: float = Field(0.0, description="Dehaze amount (-1..1)")
    vignette: float = Field(0.0, description="Vignette amount (-1..1, neg=darken edges)")
    grain: float = Field(0.0, description="Film grain amount (0..100)")
    
    # Processing
    merge_method: str = Field("mertens", description="mertens|debevec")
    output_format: str = Field("jpg", description="jpg|png|tiff")

