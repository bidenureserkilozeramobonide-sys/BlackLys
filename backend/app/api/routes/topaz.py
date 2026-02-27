from fastapi import APIRouter, HTTPException, BackgroundTasks
from pydantic import BaseModel
from typing import Optional
from app.services.topaz_service import topaz_service
from app.config import OUTPUT_DIR
from pathlib import Path
import os

router = APIRouter(prefix="/topaz", tags=["topaz"])

class TopazEnhanceRequest(BaseModel):
    file_path: str
    model: str = "denoise"  # denoise, sharpen, enhance
    output_name: str = None
    strength: Optional[float] = None
    scale_factor: Optional[str] = None
    face_enhancement: Optional[bool] = None
    fix_compression: Optional[bool] = None

@router.post("/enhance")
def enhance_image(req: TopazEnhanceRequest):
    """
    Enhance an image using Topaz Labs API.
    Supports denoise, sharpen, enhance (upscale/face/fix) operations.
    """
    # B14: Validate file_path is within OUTPUT_DIR — prevent arbitrary file read
    resolved = Path(req.file_path).resolve()
    if not str(resolved).startswith(str(OUTPUT_DIR.resolve())):
        raise HTTPException(status_code=400, detail="File path must be within output directory")
    if not resolved.exists():
        raise HTTPException(status_code=404, detail="File not found")
        
    try:
        # Construct output path if not provided
        if req.output_name:
            # B34: Sanitize output_name — prevent path traversal
            safe_name = Path(req.output_name).name
            if not safe_name:
                raise HTTPException(status_code=400, detail="Invalid output name")
            dir_name = os.path.dirname(req.file_path)
            output_path = os.path.join(dir_name, safe_name)
        else:
            output_path = None
        
        # Build extra params for the service
        extra_params = {}
        if req.strength is not None:
            extra_params["strength"] = req.strength
        if req.scale_factor is not None:
            extra_params["scale_factor"] = req.scale_factor
        if req.face_enhancement is not None:
            extra_params["face_enhancement"] = req.face_enhancement
        if req.fix_compression is not None:
            extra_params["fix_compression"] = req.fix_compression
            
        final_path = topaz_service.process_image(
            req.file_path, req.model, output_path, **extra_params
        )
        
        return {
            "status": "success",
            "original_path": req.file_path,
            "output_path": final_path,
            "message": f"Image enhanced with {req.model}"
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

