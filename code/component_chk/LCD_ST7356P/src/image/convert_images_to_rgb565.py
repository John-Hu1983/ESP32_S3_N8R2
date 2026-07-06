from pathlib import Path
import re

from PIL import Image

SOURCE_DIR = Path(__file__).resolve().parent / "source"
CODE_DIR = Path(__file__).resolve().parent / "code"


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


def build_asset(image_path, used_symbols):
    relative_stem = image_path.relative_to(SOURCE_DIR).with_suffix("")
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
        values = [rgb888_to_rgb565(*pixel) for pixel in image.getdata()]

    return {
        "name": relative_stem.as_posix(),
        "symbol": symbol,
        "width": width,
        "height": height,
        "values": values,
    }


def remove_legacy_generated_files():
    for path in CODE_DIR.glob("image*.c"):
        if path.name != "image_assets.c":
            path.unlink()
    for path in CODE_DIR.glob("image*.h"):
        if path.name != "image_assets.h":
            path.unlink()


def write_assets_files(assets):
    assets_h = [
        "#ifndef IMAGE_ASSETS_H",
        "#define IMAGE_ASSETS_H",
        "",
        "#include <stdint.h>",
        "",
        "typedef struct {",
        "\tuint16_t width;",
        "\tuint16_t height;",
        "\tconst uint16_t *data;",
        "} image_rgb565_t;",
        "",
        f"#define IMAGE_ASSET_COUNT {len(assets)}",
        "",
        "extern const image_rgb565_t image_assets[IMAGE_ASSET_COUNT];",
        "",
        "#endif",
        "",
    ]
    (CODE_DIR / "image_assets.h").write_text("\n".join(assets_h), encoding="ascii")

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

    assets_c.append("const image_rgb565_t image_assets[IMAGE_ASSET_COUNT] = {")
    for asset in assets:
        assets_c.append(f"\t{{ {asset['width']}, {asset['height']}, {asset['symbol']} }},")
    assets_c.extend(["};", ""])
    (CODE_DIR / "image_assets.c").write_text("\n".join(assets_c), encoding="ascii")

def main():
    CODE_DIR.mkdir(parents=True, exist_ok=True)
    image_patterns = ("*.jpg", "*.jpeg", "*.png")
    images = []
    for pattern in image_patterns:
        images.extend(SOURCE_DIR.rglob(pattern))
    images = sorted(images, key=lambda path: path.relative_to(SOURCE_DIR).as_posix().lower())
    if not images:
        raise SystemExit(f"No image files found in {SOURCE_DIR}")

    used_symbols = set()
    assets = [build_asset(image_path, used_symbols) for image_path in images]

    remove_legacy_generated_files()
    write_assets_files(assets)

    print(f"Generated {len(assets)} RGB565 image assets in {CODE_DIR}")
    for asset in assets:
        print(f"{asset['name']}: {asset['width']}x{asset['height']} -> {asset['symbol']}")


if __name__ == "__main__":
    main()
