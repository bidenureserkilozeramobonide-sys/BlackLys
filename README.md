# Studio Immo HDR

Stack complet pour gérer la photo immobilière : clients, missions, HDR, galeries partageables et factures.

## Structure
- `backend/` : API FastAPI + pipeline RAW/HDR (rawpy + OpenCV), SQLite par défaut.
- `frontend/` : interface React + Vite avec pages Dashboard, Clients, Missions, HDR, Galeries, Facturation.
- `backend/storage/` : RAW importés, groupes, exports HDR, galeries, PDF.

## Backend (API FastAPI)
Prérequis : Python 3.10+, `rawpy`, `opencv-python-headless` (voir `backend/requirements.txt`).

```bash
cd backend
python -m venv .venv
.venv\Scripts\activate  # sur Windows
pip install -r requirements.txt
uvicorn app.main:app --reload --port 8000
```

Endpoints clés :
- `GET /health` : ping.
- `POST /clients`, `GET /clients`… : gestion clients.
- `POST /missions`… : missions (adresse, pack, prix, statut planned/processing/delivered/paid).
- `POST /missions/{id}/raw` : upload RAW pour une mission.
- `POST /missions/{id}/group` : regroupement en brackets via horodatage.
- `POST /hdr/process` : fusion HDR (rawpy + OpenCV, réglages expo/contraste/highlights/shadows/WB/saturation, output jpg/png/tiff).
- `POST /galleries/missions/{id}` : crée la galerie + zip téléchargeable + lien partageable.
- `POST /invoices/missions/{id}` : génère une facture PDF (pack choisi) ; `POST /invoices/{id}/pay` pour marquer payée.
- `GET /galleries/public/{token}` : accès public lecture (liste fichiers + zip).

Réglages et chemins : `backend/app/config.py` (`DATABASE_URL`, `RAW_INPUT_DIR`, `OUTPUT_DIR`, `GALLERIES_DIR`, `INVOICES_DIR`, etc.).

## Frontend (React + Vite)
Prérequis : Node 18+.

```bash
cd frontend
npm install
npm run dev  # http://localhost:5173
```

Pages incluses : Dashboard (stats), Clients, Missions, Studio HDR (groupes + réglages globaux), Galeries (liens partageables), Facturation/Stats.

API base par défaut : `http://127.0.0.1:8000`. Pour un autre hôte, définis `VITE_API_BASE` dans un `.env.local` (ex: `VITE_API_BASE=http://localhost:8000`).

## À continuer
- Brancher les appels API (axios) et relier les formulaires aux endpoints.
- Ajouter l’upload multiple côté front pour `POST /missions/{id}/raw`.
- Renforcer la génération HDR (exif/exposure times, files de processing), ajouter une file d’attente.
- Ajouter authentification et branding galerie (page publique, watermark, expiration).
- Couvrir par des tests API (pytest) et scripts de validation pipeline HDR.
