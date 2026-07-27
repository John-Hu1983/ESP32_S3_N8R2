from __future__ import annotations

import json
from pathlib import Path
import re
import shutil

from PIL import Image, ImageOps, ImageSequence

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent.parent
GIF_SOURCE_DIR = PROJECT_ROOT / "rsc" / "gif"
PNG_SETS_DIR = SCRIPT_DIR / "source" / "gif_sets"
MANIFEST_PATH = PNG_SETS_DIR / "manifest.json"
TARGET_SIZE = (480, 320)


def natural_sort_key(value):
    parts = re.split(r"(\d+)", value.lower())
    key = []
    for part in parts:
        if part.isdigit():
            key.append((0, int(part)))
        else:
            key.append((1, part))
    return key


def normalize_gif_stem(stem):
    stem = re.sub(r"\(\s*\d+\s*-\s*frame\s*\)", "", stem, flags=re.IGNORECASE)
    stem = re.sub(r"^\s*\d+\s*[._-]*\s*", "", stem)
    stem = re.sub(r"\s*[-_]+\s*$", "", stem)
    return stem.strip()


def sanitize_dir_name(value):
    normalized = normalize_gif_stem(value)
    normalized = re.sub(r"[^0-9A-Za-z]+", "_", normalized)
    normalized = re.sub(r"_+", "_", normalized).strip("_").lower()
    return normalized or "gif"


def fit_to_target_size(frame):
    if frame.size == TARGET_SIZE:
        return frame

    resized = ImageOps.contain(frame, TARGET_SIZE, Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", TARGET_SIZE, (0, 0, 0, 255))
    offset_x = (TARGET_SIZE[0] - resized.width) // 2
    offset_y = (TARGET_SIZE[1] - resized.height) // 2
    canvas.paste(resized, (offset_x, offset_y), resized)
    return canvas


def extract_single_gif(gif_path, output_dir):
    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    frame_count = 0
    with Image.open(gif_path) as gif:
        for frame_index, frame in enumerate(ImageSequence.Iterator(gif)):
            rgba = frame.convert("RGBA")
            output_image = fit_to_target_size(rgba).convert("RGB")
            output_path = output_dir / f"frame_{frame_index:03d}.png"
            output_image.save(output_path, format="PNG")
            frame_count += 1

    return frame_count


def main():
    if not GIF_SOURCE_DIR.exists():
        raise SystemExit(f"GIF source directory does not exist: {GIF_SOURCE_DIR}")

    gif_paths = sorted(GIF_SOURCE_DIR.glob("*.gif"), key=lambda path: natural_sort_key(path.name))
    if not gif_paths:
        raise SystemExit(f"No GIF files found in {GIF_SOURCE_DIR}")

    PNG_SETS_DIR.mkdir(parents=True, exist_ok=True)

    manifest = {
        "target_size": [TARGET_SIZE[0], TARGET_SIZE[1]],
        "gif_sets": [],
    }

    for index, gif_path in enumerate(gif_paths, start=1):
        set_name = sanitize_dir_name(gif_path.stem)
        set_dir_name = f"{index:02d}_{set_name}"
        output_dir = PNG_SETS_DIR / set_dir_name

        frame_count = extract_single_gif(gif_path, output_dir)

        manifest["gif_sets"].append(
            {
                "index": index,
                "set_dir": set_dir_name,
                "source_gif": gif_path.name,
                "frame_count": frame_count,
            }
        )

        print(f"[{index}] {gif_path.name} -> {set_dir_name} ({frame_count} frames)")

    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2), encoding="ascii")
    print(f"Wrote manifest: {MANIFEST_PATH}")


if __name__ == "__main__":
    main()
