from __future__ import annotations

from pathlib import Path
import re

from PIL import Image

SCRIPT_DIR = Path(__file__).resolve().parent
SOURCE_SETS_DIR = SCRIPT_DIR / "source" / "gif_sets"
CODE_DIR = SCRIPT_DIR / "code"
SET_SOURCE_PREFIX = "image_assets_"
REGISTRY_SOURCE_FILE = "image_gif_sets.c"
DEFAULT_FRAME_DELAY_MS = 50


def rgb888_to_rgb565(red, green, blue):
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def sanitize_identifier(value):
    identifier = re.sub(r"[^0-9A-Za-z]+", "_", value)
    identifier = re.sub(r"_+", "_", identifier).strip("_").lower()
    if not identifier:
        identifier = "image"
    if identifier[0].isdigit():
        identifier = f"img_{identifier}"
    return identifier


def natural_sort_key(value):
    parts = re.split(r"(\d+)", value.lower())
    key = []
    for part in parts:
        if part.isdigit():
            key.append((0, int(part)))
        else:
            key.append((1, part))
    return key


def strip_set_order_prefix(value):
    return re.sub(r"^\d+\s*[_-]*\s*", "", value)


def build_unique_set_key(set_dir_name, used_set_keys):
    base_key = sanitize_identifier(strip_set_order_prefix(set_dir_name))
    if base_key == "image":
        base_key = "gif_set"

    set_key = base_key
    next_index = 2
    while set_key in used_set_keys:
        set_key = f"{base_key}_{next_index}"
        next_index += 1

    used_set_keys.add(set_key)
    return set_key


def list_set_directories():
    if not SOURCE_SETS_DIR.exists():
        raise SystemExit(f"Missing GIF set directory: {SOURCE_SETS_DIR}")

    set_dirs = sorted(
        [path for path in SOURCE_SETS_DIR.iterdir() if path.is_dir()],
        key=lambda path: natural_sort_key(path.name),
    )
    if not set_dirs:
        raise SystemExit(f"No GIF set directories found in {SOURCE_SETS_DIR}")
    return set_dirs


def load_set_frames(set_dir):
    frames = sorted(
        set_dir.glob("*.png"),
        key=lambda path: natural_sort_key(path.name),
    )
    if not frames:
        raise SystemExit(f"No PNG frames found in {set_dir}")
    return frames


def build_asset(image_path, set_root, used_symbols):
    relative_stem = image_path.relative_to(set_root).with_suffix("")
    base_symbol = sanitize_identifier("_".join(relative_stem.parts))
    symbol = f"{base_symbol}_rgb565"
    next_index = 2
    while symbol in used_symbols:
        symbol = f"{base_symbol}_{next_index}_rgb565"
        next_index += 1
    used_symbols.add(symbol)

    with Image.open(image_path) as image:
        image = image.convert("RGB")
        width, height = image.size
        rgb_bytes = image.tobytes()
        values = [
            rgb888_to_rgb565(rgb_bytes[index], rgb_bytes[index + 1], rgb_bytes[index + 2])
            for index in range(0, len(rgb_bytes), 3)
        ]

    return {
        "name": relative_stem.as_posix(),
        "symbol": symbol,
        "width": width,
        "height": height,
        "values": values,
    }


def remove_legacy_generated_files():
    legacy_file = CODE_DIR / "image_assets.c"
    if legacy_file.exists():
        legacy_file.unlink()

    for path in CODE_DIR.glob(f"{SET_SOURCE_PREFIX}*.c"):
        path.unlink()

    for path in CODE_DIR.glob(f"{SET_SOURCE_PREFIX}*.h"):
        path.unlink()

    registry_file = CODE_DIR / REGISTRY_SOURCE_FILE
    if registry_file.exists():
        registry_file.unlink()


def write_assets_header(set_infos):
    set_count = len(set_infos)

    assets_h = [
        "#ifndef IMAGE_ASSETS_H",
        "#define IMAGE_ASSETS_H",
        "",
        "#include <stdint.h>",
        "#include \"image_asset_config.h\"",
        "",
        "typedef struct {",
        "\tuint16_t width;",
        "\tuint16_t height;",
        "\tconst uint16_t *data;",
        "\tuint16_t delay;",
        "} image_rgb565_t;",
        "",
        "#ifndef IMAGE_ASSET_FRAME_DELAY_MS",
        f"#define IMAGE_ASSET_FRAME_DELAY_MS {DEFAULT_FRAME_DELAY_MS}",
        "#endif",
        "",
        f"#define IMAGE_GIF_SET_COUNT {set_count}",
        "",
    ]

    for set_info in set_infos:
        set_macro = set_info["set_macro"]
        assets_h.append(f"#define IMAGE_ASSET_COUNT_{set_macro} {set_info['asset_count']}")
        assets_h.append(f"#define IMAGE_GIF_SET_FOLDER_{set_macro} \"{set_info['set_dir_name']}\"")

    assets_h.append("")

    for set_info in set_infos:
        set_macro = set_info["set_macro"]
        assets_h.append(
            f"extern const image_rgb565_t {set_info['asset_symbol']}[IMAGE_ASSET_COUNT_{set_macro}];"
        )

    assets_h.extend(
        [
            "",
            "typedef struct {",
            "\tconst char *folder;",
            "\tuint16_t frame_count;",
            "\tconst image_rgb565_t *frames;",
            "} image_gif_set_t;",
            "",
            "extern const image_gif_set_t image_gif_sets[IMAGE_GIF_SET_COUNT];",
            "",
            "#endif",
            "",
        ]
    )
    (CODE_DIR / "image_assets.h").write_text("\n".join(assets_h), encoding="ascii")


def write_set_source(set_info):
    assets = set_info["assets"]
    set_index = set_info["index"]
    set_macro = set_info["set_macro"]
    asset_symbol = set_info["asset_symbol"]

    assets_c = [
        "#include \"image_assets.h\"",
        "",
    ]

    for asset in assets:
        assets_c.append(f"// {asset['name']}")
        assets_c.append(f"static const uint16_t {asset['symbol']}[{asset['width'] * asset['height']}] = {{")
        for index in range(0, len(asset["values"]), 12):
            chunk = asset["values"][index:index + 12]
            assets_c.append("\t" + ", ".join(f"0x{value:04X}" for value in chunk) + ",")
        assets_c.append("};")
        assets_c.append("")

    assets_c.append(f"const image_rgb565_t {asset_symbol}[IMAGE_ASSET_COUNT_{set_macro}] = {{")
    for asset in assets:
        assets_c.append(
            f"\t{{ {asset['width']}, {asset['height']}, {asset['symbol']}, IMAGE_ASSET_FRAME_DELAY_MS }},"
        )
    assets_c.extend(["};", ""])

    source_file_path = CODE_DIR / f"{SET_SOURCE_PREFIX}{set_info['index']}.c"
    source_file_path.write_text("\n".join(assets_c), encoding="ascii")
    return source_file_path


def write_registry_source(set_infos):
    registry_c = [
        "#include \"image_assets.h\"",
        "",
        "const image_gif_set_t image_gif_sets[IMAGE_GIF_SET_COUNT] = {",
    ]

    for set_info in set_infos:
        set_macro = set_info["set_macro"]
        asset_symbol = set_info["asset_symbol"]
        registry_c.append(f"\t// {set_info['set_dir_name']}")
        registry_c.append(
            f"\t{{ IMAGE_GIF_SET_FOLDER_{set_macro}, IMAGE_ASSET_COUNT_{set_macro}, {asset_symbol} }},"
        )

    registry_c.extend(["};", ""])
    registry_file_path = CODE_DIR / REGISTRY_SOURCE_FILE
    registry_file_path.write_text("\n".join(registry_c), encoding="ascii")
    return registry_file_path


def main():
    CODE_DIR.mkdir(parents=True, exist_ok=True)

    set_dirs = list_set_directories()
    set_infos = []
    used_set_keys = set()

    for index, set_dir in enumerate(set_dirs, start=1):
        frames = load_set_frames(set_dir)
        used_symbols = set()
        assets = [build_asset(frame_path, set_dir, used_symbols) for frame_path in frames]
        set_key = build_unique_set_key(set_dir.name, used_set_keys)

        set_infos.append(
            {
                "index": index,
                "set_dir_name": set_dir.name,
                "set_macro": set_key.upper(),
                "asset_symbol": f"image_assets_{set_key}",
                "assets": assets,
                "asset_count": len(assets),
            }
        )

    remove_legacy_generated_files()
    write_assets_header(set_infos)

    generated_source_files = []
    for set_info in set_infos:
        generated_source_files.append(write_set_source(set_info))
        print(
            f"Generated {generated_source_files[-1].name}: "
            f"set {set_info['index']} ({set_info['set_dir_name']}, frames={set_info['asset_count']})"
        )

    registry_file = write_registry_source(set_infos)
    generated_source_files.append(registry_file)
    print(f"Generated {registry_file.name}: set registry ({len(set_infos)} sets)")

    print(f"Generated assets header: {CODE_DIR / 'image_assets.h'}")


if __name__ == "__main__":
    main()
