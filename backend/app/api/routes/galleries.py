from pathlib import Path
from typing import List

from fastapi import APIRouter, Depends, HTTPException
from fastapi.responses import FileResponse
from pydantic import BaseModel
from sqlalchemy import select
from sqlalchemy.orm import joinedload

from ... import crud, schemas
from ...database import get_db
from ...models import Mission, Gallery, Photo
from ...services.gallery import build_gallery_zip, delete_gallery_assets
from ...config import GALLERIES_DIR, OUTPUT_DIR

router = APIRouter(prefix="/galleries", tags=["galleries"])


@router.get("/")
def list_galleries(db=Depends(get_db)):
    """List all galleries with metadata."""
    stmt = select(Gallery).options(
        joinedload(Gallery.mission)
    ).order_by(Gallery.created_at.desc())
    galleries = list(db.execute(stmt).scalars().unique())
    result = []
    for g in galleries:
        # Count photos from gallery directory
        gallery_dir = GALLERIES_DIR / f"mission_{g.mission_id}"
        photo_count = 0
        if gallery_dir.exists():
            photo_count = sum(1 for f in gallery_dir.iterdir() if f.is_file() and not f.name.lower().endswith(".zip"))
        result.append({
            "id": g.id,
            "mission_id": g.mission_id,
            "title": g.title,
            "share_token": g.share_token,
            "share_url": crud.build_share_url(g.share_token),
            "is_public": g.is_public,
            "photo_count": photo_count,
            "created_at": g.created_at.isoformat() if g.created_at else None,
            "status": "active" if g.is_public else "expired",
        })
    return result


class GalleryRequest(BaseModel):
    title: str
    photo_paths: List[Path]
    is_public: bool = True


def _mission_or_404(db, mission_id: int) -> Mission:
    mission = crud.get_mission(db, mission_id)
    if not mission:
        raise HTTPException(status_code=404, detail="Mission not found")
    return mission


def _gallery_by_token(db, token: str) -> Gallery:
    gallery = db.execute(
        select(Gallery).where(Gallery.share_token == token)
    ).scalar_one_or_none()
    if not gallery:
        raise HTTPException(status_code=404, detail="Gallery not found")
    if not gallery.is_public:
        raise HTTPException(status_code=403, detail="Gallery is private")
    return gallery


@router.post("/missions/{mission_id}", response_model=schemas.GalleryOut)
def create_gallery(mission_id: int, payload: GalleryRequest, db=Depends(get_db)):
    mission = _mission_or_404(db, mission_id)
    if mission.gallery:
        raise HTTPException(status_code=400, detail="Gallery already exists for this mission")
    # B30: Validate photo_paths are within OUTPUT_DIR
    output_root = OUTPUT_DIR.resolve()
    for p in payload.photo_paths:
        resolved = Path(p).resolve()
        if not str(resolved).startswith(str(output_root)):
            raise HTTPException(status_code=400, detail=f"Path not allowed: {p}")
        if not resolved.exists():
            raise HTTPException(status_code=400, detail=f"File missing: {p}")
    zip_path = build_gallery_zip(mission.id, payload.photo_paths)
    title = payload.title or mission.title
    gallery = crud.create_gallery(
        db,
        mission=mission,
        payload=schemas.GalleryCreate(
            title=title,
            is_public=payload.is_public,
        ),
        zip_path=str(zip_path),
    )
    return schemas.GalleryOut(
        id=gallery.id,
        mission_id=mission_id,
        title=gallery.title,
        share_token=gallery.share_token,
        share_url=crud.build_share_url(gallery.share_token),
        zip_path=gallery.zip_path,
        is_public=gallery.is_public,
        created_at=gallery.created_at,
    )


@router.get("/missions/{mission_id}", response_model=schemas.GalleryOut)
def get_gallery(mission_id: int, db=Depends(get_db)):
    mission = _mission_or_404(db, mission_id)
    gallery = mission.gallery
    if not gallery:
        raise HTTPException(status_code=404, detail="Gallery not found")
    return schemas.GalleryOut(
        id=gallery.id,
        mission_id=mission_id,
        title=gallery.title,
        share_token=gallery.share_token,
        share_url=crud.build_share_url(gallery.share_token),
        zip_path=gallery.zip_path,
        is_public=gallery.is_public,
        created_at=gallery.created_at,
    )


@router.delete("/missions/{mission_id}", status_code=204)
def delete_gallery(mission_id: int, db=Depends(get_db)):
    mission = _mission_or_404(db, mission_id)
    if not mission.gallery:
        return None
    delete_gallery_assets(mission.id)
    db.delete(mission.gallery)
    db.commit()
    return None


@router.get("/public/{token}")
def get_public_gallery(token: str, db=Depends(get_db)):
    gallery = _gallery_by_token(db, token)
    gallery_dir = GALLERIES_DIR / f"mission_{gallery.mission_id}"
    files = []
    if gallery_dir.exists():
        for p in gallery_dir.iterdir():
            if p.is_file():
                # on évite de ré-afficher le zip comme fichier
                if p.name.lower().endswith(".zip"):
                    continue
                files.append(
                    {
                        "name": p.name,
                        "url": f"/galleries/public/{token}/files/{p.name}",
                    }
                )
    return {
        "title": gallery.title,
        "zip_path": gallery.zip_path,
        "zip_url": f"/galleries/public/{token}/zip",
        "files": files,
    }


@router.get("/public/{token}/zip")
def download_public_zip(token: str, db=Depends(get_db)):
    gallery = _gallery_by_token(db, token)
    if not gallery.zip_path or not Path(gallery.zip_path).exists():
        raise HTTPException(status_code=404, detail="Zip not found")
    return FileResponse(gallery.zip_path, filename=Path(gallery.zip_path).name, media_type="application/zip")


@router.get("/public/{token}/files/{filename}")
def download_public_file(token: str, filename: str, db=Depends(get_db)):
    gallery = _gallery_by_token(db, token)
    gallery_dir = GALLERIES_DIR / f"mission_{gallery.mission_id}"
    target = (gallery_dir / filename).resolve()
    if not target.exists() or target.is_dir() or gallery_dir.resolve() not in target.parents:
        raise HTTPException(status_code=404, detail="File not found")
    return FileResponse(target, filename=target.name)


@router.get("/missions/{mission_id}/zip")
def download_zip(mission_id: int, db=Depends(get_db)):
    mission = _mission_or_404(db, mission_id)
    gallery = mission.gallery
    if not gallery or not gallery.zip_path or not Path(gallery.zip_path).exists():
        raise HTTPException(status_code=404, detail="Zip not found")
    return FileResponse(gallery.zip_path, filename=Path(gallery.zip_path).name, media_type="application/zip")
