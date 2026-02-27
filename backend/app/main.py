from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from .api.routes import galleries, health, hdr, invoices, missions, clients, quotes, topaz, stats
from .database import Base, engine
from . import models  # noqa: F401 - ensure models are registered

# Create tables on startup (lightweight for SQLite)
Base.metadata.create_all(bind=engine)

app = FastAPI(
    title="BlackLys – Real Estate Photo Suite",
    version="0.2.0",
    description="Backend API for real estate photography: clients, missions, HDR engine, galleries, invoices, stats.",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=[
        "http://localhost:5173",
        "http://localhost:5174",
        "http://127.0.0.1:5173",
        "http://127.0.0.1:5174",
        # Electron file:// protocol
        "file://",
        "null",  # file:// sends Origin: null
    ],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(health.router)
app.include_router(clients.router)
app.include_router(missions.router)
app.include_router(hdr.router)
app.include_router(galleries.router)
app.include_router(invoices.router)
app.include_router(quotes.router)
app.include_router(topaz.router)
app.include_router(stats.router)


@app.get("/")
def root():
    return {"message": "Real Estate Photo Suite API"}
