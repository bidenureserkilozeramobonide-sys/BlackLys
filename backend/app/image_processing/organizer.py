import os
import shutil
from pathlib import Path
from typing import Dict, List, Sequence

from ..config import GROUPED_DIR, RAW_INPUT_DIR

# Default values can be overridden when calling the helpers
BRACKET_SIZE = 5
MAX_TIME_DIFF_SECONDS = 3.0
VALID_EXT = {
    ".arw",
    ".cr2",
    ".cr3",
    ".nef",
    ".raf",
    ".dng",
    ".orf",
    ".rw2",
    ".jpg",
    ".jpeg",
}


def get_file_time(path: Path) -> float:
    return path.stat().st_mtime


def import_raw_from_folder(source: Path, mission_id: int) -> Path:
    """
    Copy RAW files from a folder (or SD card mount) to the mission RAW directory.
    """
    target = RAW_INPUT_DIR / f"mission_{mission_id}"
    target.mkdir(parents=True, exist_ok=True)

    count = 0
    for root, _, files in os.walk(source):
        for name in files:
            src = Path(root) / name
            if src.suffix.lower() not in VALID_EXT:
                continue
            dest = target / src.name
            shutil.copy2(src, dest)
            count += 1

    print(f"Imported {count} files to {target}")
    return target


def group_brackets(
    mission_id: int,
    bracket_size: int = BRACKET_SIZE,
    max_time_diff_seconds: float = MAX_TIME_DIFF_SECONDS,
) -> Dict[str, List[Path]]:
    """
    Group RAWs into exposure brackets based on capture time.
    Returns a dict {group_name: [paths]} and moves files into GROUPED_DIR/mission_xxx/scene_xxx.
    """
    raw_dir = RAW_INPUT_DIR / f"mission_{mission_id}"
    if not raw_dir.exists():
        raise FileNotFoundError(f"RAW folder not found for mission {mission_id}: {raw_dir}")

    grouped_dir = GROUPED_DIR / f"mission_{mission_id}"
    grouped_dir.mkdir(parents=True, exist_ok=True)

    files = [
        f for f in raw_dir.iterdir() if f.is_file() and f.suffix.lower() in VALID_EXT
    ]
    if not files:
        raise FileNotFoundError(f"No RAW files found in {raw_dir}")

    files.sort(key=get_file_time)
    scenes: List[List[Path]] = []
    current: List[Path] = [files[0]]

    for prev, curr in zip(files, files[1:]):
        dt = get_file_time(curr) - get_file_time(prev)
        if dt <= max_time_diff_seconds and len(current) < bracket_size:
            current.append(curr)
        else:
            scenes.append(current)
            current = [curr]
    if current:
        scenes.append(current)

    valid = [s for s in scenes if len(s) == bracket_size]
    leftovers = [s for s in scenes if len(s) != bracket_size]

    groups: Dict[str, List[Path]] = {}
    for idx, group in enumerate(valid, start=1):
        scene_dir = grouped_dir / f"scene_{idx:03d}"
        scene_dir.mkdir(parents=True, exist_ok=True)
        moved: List[Path] = []
        for f in group:
            dest = scene_dir / f.name
            shutil.move(str(f), str(dest))
            moved.append(dest)
        groups[scene_dir.name] = moved

    if leftovers:
        misc = grouped_dir / "_incomplete"
        misc.mkdir(parents=True, exist_ok=True)
        for group in leftovers:
            for f in group:
                if f.exists():
                    shutil.move(str(f), str(misc / f.name))

    return groups

