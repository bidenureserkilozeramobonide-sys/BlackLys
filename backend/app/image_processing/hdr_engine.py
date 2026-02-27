from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Sequence

import cv2
import numpy as np
import rawpy

from ..config import OUTPUT_DIR
from ..schemas import HDRSettings


@dataclass
class HDRJobResult:
    output_path: Path
    merged_count: int
    used_method: str


def _load_raw_to_rgb(path: Path) -> np.ndarray:
    """
    Read a RAW file and return an RGB float32 image in range 0..1.
    Uses sRGB gamma for natural-looking output.
    """
    with rawpy.imread(str(path)) as raw:
        rgb = raw.postprocess(
            output_bps=16,
            no_auto_bright=False,  # Enable auto-bright for better exposure
            use_camera_wb=True,
            gamma=(2.222, 4.5),  # sRGB gamma for natural colors
        )
    return (rgb.astype(np.float32) / 65535.0).clip(0.0, 1.0)


def _align_brackets(images: Sequence[np.ndarray]) -> List[np.ndarray]:
    """
    Align hand-held brackets using ECC (Enhanced Correlation Coefficient).
    Preserves float32 precision by applying transforms to original images.
    """
    if len(images) <= 1:
        return list(images)

    aligned = [images[0].copy()]  # Reference image stays unchanged
    reference = images[0]
    
    # Convert reference to grayscale for alignment
    ref_gray = cv2.cvtColor((reference * 255).astype(np.uint8), cv2.COLOR_RGB2GRAY)
    
    # ECC parameters
    warp_mode = cv2.MOTION_EUCLIDEAN  # Translation + rotation
    warp_matrix = np.eye(2, 3, dtype=np.float32)
    criteria = (cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, 100, 1e-6)
    
    for img in images[1:]:
        img_gray = cv2.cvtColor((img * 255).astype(np.uint8), cv2.COLOR_RGB2GRAY)
        
        try:
            # Find the geometric transform
            _, warp_matrix = cv2.findTransformECC(
                ref_gray, img_gray, warp_matrix, warp_mode, criteria
            )
            
            # Apply transform to original float32 image (preserves quality)
            h, w = img.shape[:2]
            aligned_img = cv2.warpAffine(
                img, warp_matrix, (w, h),
                flags=cv2.INTER_LINEAR + cv2.WARP_INVERSE_MAP,
                borderMode=cv2.BORDER_REFLECT
            )
            aligned.append(aligned_img)
        except cv2.error:
            # If ECC fails, fall back to original image
            aligned.append(img.copy())
        
        # Reset warp matrix for next image
        warp_matrix = np.eye(2, 3, dtype=np.float32)
    
    return aligned


def _apply_global_settings(img: np.ndarray, settings: HDRSettings) -> np.ndarray:
    """
    Apply all adjustment parameters to the image.
    Professional-grade processing pipeline.
    """
    img = np.clip(img, 0.0, 1.0).astype(np.float32)
    
    # === LIGHT PANEL ===
    
    # 1. Base stretch to normalize dynamic range
    low = np.percentile(img, 0.5)
    high = np.percentile(img, 99.5)
    if high > low + 1e-5:
        img = (img - low) / (high - low)
    img = np.clip(img, 0.0, 1.0)
    
    # 2. Exposure
    img = img * settings.exposure
    
    # 3. Contrast
    img = 0.5 + (img - 0.5) * settings.contrast
    
    # 4. Highlights recovery
    if settings.highlights != 0:
        mask = np.clip((img - 0.6) / 0.4, 0, 1)  # Smooth transition
        img = img - settings.highlights * mask * (img - 0.6)
    
    # 5. Shadows lift
    if settings.shadows != 0:
        mask = np.clip((0.4 - img) / 0.4, 0, 1)  # Smooth transition
        img = img + settings.shadows * mask * (0.4 - img)
    
    # 6. Whites (push bright areas)
    if hasattr(settings, 'whites') and settings.whites != 0:
        mask = np.clip((img - 0.8) / 0.2, 0, 1)
        img = img + settings.whites * mask * 0.2
    
    # 7. Blacks (push dark areas)
    if hasattr(settings, 'blacks') and settings.blacks != 0:
        mask = np.clip((0.2 - img) / 0.2, 0, 1)
        img = img - settings.blacks * mask * 0.2
    
    img = np.clip(img, 0.0, 1.0)
    
    # === COLOR PANEL ===
    
    # 8. Temperature (warm/cool shift)
    if hasattr(settings, 'temperature') and settings.temperature != 0:
        temp = settings.temperature / 100.0  # Normalize to -1..1
        # Warm = add red/yellow, reduce blue
        # Cool = add blue, reduce red/yellow
        img[:, :, 0] = np.clip(img[:, :, 0] + temp * 0.1, 0, 1)  # Red
        img[:, :, 2] = np.clip(img[:, :, 2] - temp * 0.1, 0, 1)  # Blue
    
    # 9. Tint (green/magenta shift)
    if hasattr(settings, 'tint') and settings.tint != 0:
        tint = settings.tint / 100.0
        img[:, :, 1] = np.clip(img[:, :, 1] - tint * 0.05, 0, 1)  # Green
    
    # 10. Manual white balance gains
    if settings.white_balance and len(settings.white_balance) == 3:
        gains = np.array(settings.white_balance, dtype=np.float32)
        gains = gains / max(np.max(gains), 1e-3)
        img = img * gains.reshape(1, 1, 3)
    
    # 11. Vibrance (smart saturation - affects less saturated colors more)
    if hasattr(settings, 'vibrance') and settings.vibrance != 0:
        gray = cv2.cvtColor(img, cv2.COLOR_RGB2GRAY)[..., None]
        current_sat = np.abs(img - gray).max(axis=2, keepdims=True)
        # Less saturated pixels get more boost
        weight = 1.0 - current_sat
        vib_factor = 1.0 + settings.vibrance * weight
        img = gray + (img - gray) * vib_factor
    
    # 12. Saturation
    if settings.saturation != 1.0:
        gray = cv2.cvtColor(img.astype(np.float32), cv2.COLOR_RGB2GRAY)[..., None]
        img = gray + (img - gray) * settings.saturation
    
    img = np.clip(img, 0.0, 1.0)
    
    # === EFFECTS PANEL ===
    
    # 13. Dehaze (simple dark channel prior approximation)
    if hasattr(settings, 'dehaze') and settings.dehaze != 0:
        # Positive dehaze removes haze, negative adds haze
        dark_channel = img.min(axis=2, keepdims=True)
        atmospheric = np.percentile(dark_channel, 99)
        transmission = 1.0 - settings.dehaze * (dark_channel / (atmospheric + 1e-6))
        transmission = np.clip(transmission, 0.1, 1.0)
        img = (img - atmospheric * (1 - transmission)) / transmission
    
    # 14. Vignette
    if hasattr(settings, 'vignette') and settings.vignette != 0:
        h, w = img.shape[:2]
        Y, X = np.ogrid[:h, :w]
        center_y, center_x = h / 2, w / 2
        # Distance from center, normalized
        dist = np.sqrt((X - center_x)**2 + (Y - center_y)**2)
        max_dist = np.sqrt(center_x**2 + center_y**2)
        dist = dist / max_dist
        # Smooth vignette falloff
        vignette_mask = 1.0 - settings.vignette * (dist ** 2)
        vignette_mask = np.clip(vignette_mask, 0.0, 1.5)[..., None]
        img = img * vignette_mask
    
    img = np.clip(img, 0.0, 1.0)
    
    # === DETAIL PANEL ===
    
    # 15. Sharpening (unsharp mask)
    if hasattr(settings, 'sharpening') and settings.sharpening > 0:
        amount = settings.sharpening / 100.0
        blurred = cv2.GaussianBlur(img, (0, 0), sigmaX=1.5)
        img = cv2.addWeighted(img, 1.0 + amount, blurred, -amount, 0)
    
    # 16. Noise reduction (simple bilateral filter approximation)
    if hasattr(settings, 'noise_reduction') and settings.noise_reduction > 0:
        strength = int(settings.noise_reduction / 10) + 1
        img_u8 = (np.clip(img, 0, 1) * 255).astype(np.uint8)
        img_u8 = cv2.bilateralFilter(img_u8, d=strength, sigmaColor=75, sigmaSpace=75)
        img = img_u8.astype(np.float32) / 255.0
    
    img = np.clip(img, 0.0, 1.0)
    
    # === FINAL TONE MAPPING ===
    # S-curve for pleasing contrast (sigmoid-based)
    img = 1.0 / (1.0 + np.exp(-6.0 * (img - 0.5)))
    # Normalize back to 0-1 range
    img = (img - img.min()) / (img.max() - img.min() + 1e-8)
    
    return np.clip(img, 0.0, 1.0).astype(np.float32)


def _merge_mertens(images: Sequence[np.ndarray]) -> np.ndarray:
    """
    Mertens exposure fusion with tuned weights for richer output.
    """
    merger = cv2.createMergeMertens(
        contrast_weight=1.2,    # More local contrast
        saturation_weight=1.3,  # Richer colors
        exposure_weight=0.5     # Better mid-tone priority
    )
    return merger.process(images)


def _merge_debevec(images: Sequence[np.ndarray]) -> np.ndarray:
    # B31: CalibrateRobertson expects uint8 arrays, not float32
    images_u8 = [(np.clip(img, 0, 1) * 255).astype(np.uint8) for img in images]
    # Debevec expects exposure times; we fallback to a neutral ramp.
    times = np.linspace(1.0, 1.0 + 0.1 * (len(images) - 1), num=len(images)).astype(
        np.float32
    )
    calibrate = cv2.createCalibrateRobertson()
    response = calibrate.process(images_u8, times)
    merge = cv2.createMergeDebevec()
    hdr = merge.process(images_u8, times=times, response=response)
    tonemap = cv2.createTonemapReinhard(gamma=1.2, intensity=0.0, light_adapt=0.8)
    return tonemap.process(hdr)


def merge_bracket_group(
    bracket_paths: Iterable[Path],
    settings: HDRSettings,
    mission_id: int,
    output_name: str,
) -> HDRJobResult:
    paths = [Path(p) for p in bracket_paths]
    if not paths:
        raise ValueError("No bracket paths provided")

    # B32: Sanitize output_name to prevent path traversal
    safe_name = Path(output_name).name
    if not safe_name:
        raise ValueError("Invalid output name")

    rgb_images = [_load_raw_to_rgb(p) for p in paths]
    aligned = _align_brackets(rgb_images)

    method = settings.merge_method.lower()
    if method == "debevec":
        merged = _merge_debevec(aligned)
        method_used = "debevec"
    else:
        merged = _merge_mertens(aligned)
        method_used = "mertens"

    merged = _apply_global_settings(merged, settings)

    output_dir = OUTPUT_DIR / f"mission_{mission_id}"
    output_dir.mkdir(parents=True, exist_ok=True)

    suffix = settings.output_format.lower()
    if suffix not in {"jpg", "jpeg", "png", "tiff"}:
        suffix = "jpg"
    output_path = output_dir / f"{safe_name}.{suffix}"

    bgr = cv2.cvtColor(merged, cv2.COLOR_RGB2BGR)
    bgr_8 = np.clip(bgr * 255.0, 0, 255).astype("uint8")

    if suffix in {"jpg", "jpeg"}:
        cv2.imwrite(str(output_path), bgr_8, [int(cv2.IMWRITE_JPEG_QUALITY), 92])
    elif suffix == "png":
        cv2.imwrite(str(output_path), bgr_8, [int(cv2.IMWRITE_PNG_COMPRESSION), 3])
    else:
        cv2.imwrite(str(output_path), (bgr * 65535.0).astype("uint16"))

    return HDRJobResult(
        output_path=output_path,
        merged_count=len(paths),
        used_method=method_used,
    )


def adjust_existing_image(
    input_path: Path, settings: HDRSettings, mission_id: int, output_name: str
) -> HDRJobResult:
    if not input_path.exists() or input_path.is_dir():
        raise ValueError("Input HDR not found")

    # B32: Sanitize output_name to prevent path traversal
    safe_name = Path(output_name).name
    if not safe_name:
        raise ValueError("Invalid output name")

    # Read existing HDR (8-bit or 16-bit) to float RGB 0..1
    bgr = cv2.imread(str(input_path), cv2.IMREAD_UNCHANGED)
    if bgr is None:
        raise ValueError("Could not read input HDR")
    if bgr.dtype == np.uint16:
        img = bgr.astype(np.float32) / 65535.0
    else:
        img = bgr.astype(np.float32) / 255.0
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)

    # Apply global settings
    adjusted = _apply_global_settings(img, settings)

    output_dir = OUTPUT_DIR / f"mission_{mission_id}"
    output_dir.mkdir(parents=True, exist_ok=True)

    suffix = settings.output_format.lower()
    if suffix not in {"jpg", "jpeg", "png", "tiff"}:
        suffix = "jpg"
    output_path = output_dir / f"{safe_name}.{suffix}"

    bgr_adj = cv2.cvtColor(adjusted, cv2.COLOR_RGB2BGR)
    if suffix in {"jpg", "jpeg"}:
        cv2.imwrite(str(output_path), (bgr_adj * 255.0).clip(0, 255).astype("uint8"), [int(cv2.IMWRITE_JPEG_QUALITY), 92])
    elif suffix == "png":
        cv2.imwrite(str(output_path), (bgr_adj * 255.0).clip(0, 255).astype("uint8"), [int(cv2.IMWRITE_PNG_COMPRESSION), 3])
    else:
        cv2.imwrite(str(output_path), (bgr_adj * 65535.0).clip(0, 65535).astype("uint16"))

    return HDRJobResult(
        output_path=output_path,
        merged_count=1,
        used_method="adjust",
    )
