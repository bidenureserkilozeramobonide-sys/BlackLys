from pathlib import Path
import os

# Paths
BASE_DIR = Path(__file__).resolve().parent.parent
STORAGE_ROOT = Path(os.getenv("STORAGE_ROOT", BASE_DIR / "storage"))
STORAGE_ROOT.mkdir(parents=True, exist_ok=True)

DATABASE_URL = os.getenv(
    "DATABASE_URL",
    f"sqlite:///{(STORAGE_ROOT / 'app.db').as_posix()}",
)

# Image pipeline settings
RAW_INPUT_DIR = Path(os.getenv("RAW_INPUT_DIR", STORAGE_ROOT / "raw"))
GROUPED_DIR = Path(os.getenv("GROUPED_DIR", STORAGE_ROOT / "grouped"))
OUTPUT_DIR = Path(os.getenv("OUTPUT_DIR", STORAGE_ROOT / "output"))
GALLERIES_DIR = Path(os.getenv("GALLERIES_DIR", STORAGE_ROOT / "galleries"))
INVOICES_DIR = Path(os.getenv("INVOICES_DIR", STORAGE_ROOT / "invoices"))

for folder in [RAW_INPUT_DIR, GROUPED_DIR, OUTPUT_DIR, GALLERIES_DIR, INVOICES_DIR]:
    folder.mkdir(parents=True, exist_ok=True)

# Security / liens publics
# Par défaut on pointe vers le front local pour les liens de galerie.
SHARE_LINK_BASE = os.getenv("SHARE_LINK_BASE", "http://localhost:5174")
