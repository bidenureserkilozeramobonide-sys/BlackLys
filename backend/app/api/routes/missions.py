from typing import Dict, List
from pathlib import Path

from fastapi import APIRouter, Depends, File, HTTPException, UploadFile, status
from fastapi.responses import FileResponse
from pydantic import BaseModel

from ... import crud, schemas
from ...database import get_db
from ...image_processing import organizer
from ...models import Mission, MissionStatus, Photo
from ...config import RAW_INPUT_DIR, GROUPED_DIR, OUTPUT_DIR

router = APIRouter(prefix="/missions", tags=["missions"])


class GroupingRequest(BaseModel):
    bracket_size: int = organizer.BRACKET_SIZE
    max_time_diff_seconds: float = organizer.MAX_TIME_DIFF_SECONDS


@router.get("/", response_model=List[schemas.MissionOut])
def list_missions(status: MissionStatus | None = None, db=Depends(get_db)):
    return crud.list_missions(db, status=status)


@router.post("/", response_model=schemas.MissionOut, status_code=status.HTTP_201_CREATED)
def create_mission(payload: schemas.MissionCreate, db=Depends(get_db)):
    if not crud.get_client(db, payload.client_id):
        raise HTTPException(status_code=400, detail="Client does not exist")
    return crud.create_mission(db, payload)


def _mission_or_404(db, mission_id: int) -> Mission:
    mission = crud.get_mission(db, mission_id)
    if not mission:
        raise HTTPException(status_code=404, detail="Mission not found")
    return mission


@router.get("/{mission_id}", response_model=schemas.MissionOut)
def get_mission(mission_id: int, db=Depends(get_db)):
    return _mission_or_404(db, mission_id)


@router.put("/{mission_id}", response_model=schemas.MissionOut)
def update_mission(mission_id: int, payload: schemas.MissionUpdate, db=Depends(get_db)):
    mission = _mission_or_404(db, mission_id)
    return crud.update_mission(db, mission, payload)


@router.delete("/{mission_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_mission(mission_id: int, db=Depends(get_db)):
    mission = _mission_or_404(db, mission_id)
    crud.delete_mission(db, mission)
    return None


@router.post("/{mission_id}/raw", status_code=status.HTTP_201_CREATED)
async def upload_raw(
    mission_id: int, files: List[UploadFile] = File(...), db=Depends(get_db)
):
    _mission_or_404(db, mission_id)
    target = RAW_INPUT_DIR / f"mission_{mission_id}"
    target.mkdir(parents=True, exist_ok=True)

    saved = []
    for up in files:
        # B9: Sanitize filename — prevent path traversal (e.g. "../../etc/passwd")
        safe_name = Path(up.filename).name
        if not safe_name or safe_name.startswith("."):
            continue  # skip hidden or empty filenames
        dest = target / safe_name
        content = await up.read()
        dest.write_bytes(content)
        photo = Photo(mission_id=mission_id, raw_path=str(dest), is_hdr=False)
        db.add(photo)
        saved.append(dest)
    db.commit()
    return {"saved": [str(p) for p in saved]}


@router.post("/{mission_id}/group")
def group_raw_brackets(
    mission_id: int, payload: GroupingRequest, db=Depends(get_db)
) -> Dict[str, List[str]]:
    _mission_or_404(db, mission_id)
    groups = organizer.group_brackets(
        mission_id=mission_id,
        bracket_size=payload.bracket_size,
        max_time_diff_seconds=payload.max_time_diff_seconds,
    )
    return {name: [str(p) for p in paths] for name, paths in groups.items()}


@router.get("/{mission_id}/groups")
def list_grouped_brackets(mission_id: int, db=Depends(get_db)) -> Dict[str, List[str]]:
    _mission_or_404(db, mission_id)
    base = GROUPED_DIR / f"mission_{mission_id}"
    if not base.exists():
        return {}
    groups: Dict[str, List[str]] = {}
    valid_ext = {ext.lower() for ext in organizer.VALID_EXT}
    for folder in sorted(base.iterdir()):
        if not folder.is_dir() or folder.name.startswith("_"):
            continue
        files = [
            p
            for p in sorted(folder.iterdir())
            if p.is_file() and p.suffix.lower() in valid_ext
        ]
        if len(files) > organizer.BRACKET_SIZE:
            files = files[: organizer.BRACKET_SIZE]
        if files:
            groups[folder.name] = [str(p) for p in files]
    return groups


@router.get("/{mission_id}/hdrs")
def list_hdr_outputs(mission_id: int, db=Depends(get_db)) -> List[Dict[str, str]]:
    _mission_or_404(db, mission_id)
    base = OUTPUT_DIR / f"mission_{mission_id}"
    if not base.exists():
        return []
    items = []
    for f in sorted(base.iterdir()):
        if f.is_file() and f.suffix.lower() in {".jpg", ".jpeg", ".png", ".tiff"}:
            items.append(
                {
                    "name": f.name,
                    "path": str(f),
                    "url": f"/missions/{mission_id}/hdrs/files/{f.name}",
                }
            )
    return items


@router.get("/{mission_id}/hdrs/files/{filename}")
def get_hdr_file(mission_id: int, filename: str, db=Depends(get_db)):
    _mission_or_404(db, mission_id)
    base = (OUTPUT_DIR / f"mission_{mission_id}").resolve()
    target = (base / filename).resolve()
    if base not in target.parents and base != target.parent:
        raise HTTPException(status_code=404, detail="File not found")
    if not target.exists() or target.is_dir():
        raise HTTPException(status_code=404, detail="File not found")
    return FileResponse(target, filename=target.name)
