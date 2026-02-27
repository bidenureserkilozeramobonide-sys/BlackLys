from pathlib import Path
from typing import List

from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel

from ... import crud, schemas
from ...database import get_db
from ...config import OUTPUT_DIR, RAW_INPUT_DIR, GROUPED_DIR
from ...image_processing.hdr_engine import merge_bracket_group
from ...models import Mission

router = APIRouter(prefix="/hdr", tags=["hdr"])


class HDRRequest(BaseModel):
    mission_id: int
    bracket_paths: List[Path]
    output_name: str = "hdr_merge"
    settings: schemas.HDRSettings = schemas.HDRSettings()


class HDRAdjustRequest(BaseModel):
    mission_id: int
    input_filename: str
    output_name: str = "hdr_edit"
    settings: schemas.HDRSettings = schemas.HDRSettings()


def _mission_or_404(db, mission_id: int) -> Mission:
    mission = crud.get_mission(db, mission_id)
    if not mission:
        raise HTTPException(status_code=404, detail="Mission not found")
    return mission


# B26: Allowed root directories for bracket paths
_ALLOWED_ROOTS = [RAW_INPUT_DIR.resolve(), GROUPED_DIR.resolve()]


@router.post("/process")
def process_hdr(req: HDRRequest, db=Depends(get_db)):
    _mission_or_404(db, req.mission_id)

    for path in req.bracket_paths:
        resolved = Path(path).resolve()
        if not any(str(resolved).startswith(str(root)) for root in _ALLOWED_ROOTS):
            raise HTTPException(status_code=400, detail=f"Path not allowed: {path}")
        if not resolved.exists():
            raise HTTPException(status_code=400, detail=f"File missing: {path}")

    result = merge_bracket_group(
        bracket_paths=req.bracket_paths,
        settings=req.settings,
        mission_id=req.mission_id,
        output_name=req.output_name,
    )
    return {
        "output_path": str(result.output_path),
        "merged_count": result.merged_count,
        "method": result.used_method,
    }


@router.post("/adjust")
def adjust_hdr(req: HDRAdjustRequest, db=Depends(get_db)):
    _mission_or_404(db, req.mission_id)
    from ...config import OUTPUT_DIR
    from ...image_processing.hdr_engine import adjust_existing_image

    base = (OUTPUT_DIR / f"mission_{req.mission_id}").resolve()
    input_path = (base / req.input_filename).resolve()
    if base not in input_path.parents and base != input_path.parent:
        raise HTTPException(status_code=404, detail="File not found")
    if not input_path.exists() or input_path.is_dir():
        raise HTTPException(status_code=404, detail="File not found")

    result = adjust_existing_image(
        input_path=input_path,
        settings=req.settings,
        mission_id=req.mission_id,
        output_name=req.output_name,
    )
    return {
        "output_path": str(result.output_path),
        "merged_count": result.merged_count,
        "method": result.used_method,
    }
