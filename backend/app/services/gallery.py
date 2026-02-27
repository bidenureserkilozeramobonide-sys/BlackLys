import zipfile
import shutil
from pathlib import Path
from typing import Iterable, List

from ..config import GALLERIES_DIR


def build_gallery_zip(mission_id: int, photo_paths: Iterable[Path]) -> Path:
    output_dir = GALLERIES_DIR / f"mission_{mission_id}"
    output_dir.mkdir(parents=True, exist_ok=True)
    zip_path = output_dir / "gallery.zip"

    paths = [Path(p) for p in photo_paths]
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for path in paths:
            if not path.exists():
                continue
            zf.write(path, arcname=path.name)

    # On copie aussi les fichiers dans le dossier public de la galerie pour l'accès HTTP direct
    for path in paths:
        if not path.exists():
            continue
        dest = output_dir / path.name
        if not dest.exists():
            shutil.copy2(path, dest)
    return zip_path


def list_gallery_items(mission_id: int) -> List[Path]:
    dir_path = GALLERIES_DIR / f"mission_{mission_id}"
    if not dir_path.exists():
        return []
    return [p for p in dir_path.iterdir() if p.is_file()]


def delete_gallery_assets(mission_id: int) -> None:
    dir_path = GALLERIES_DIR / f"mission_{mission_id}"
    if dir_path.exists():
        for p in dir_path.iterdir():
            p.unlink(missing_ok=True)
        dir_path.rmdir()
