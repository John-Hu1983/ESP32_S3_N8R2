from pathlib import Path

from PIL import Image

SOURCE_DIR = Path(__file__).resolve().parent / "source"
CODE_DIR = Path(__file__).resolve().parent / "code"


def rgb888_to_rgb565(red, green, blue):
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def write_image_files(image_path):
    stem = image_path.stem
    symbol = f"{stem}_rgb565"
    macro = stem.upper()

    with Image.open(image_path) as image:
        image = image.convert("RGB")
        width, height = image.size
        values = [rgb888_to_rgb565(*pixel) for pixel in image.getdata()]

    header_path = CODE_DIR / f"{stem}.h"
    source_path = CODE_DIR / f"{stem}.c"

    header_path.write_text(
        f"#ifndef {macro}_H\n"
        f"#define {macro}_H\n\n"
        f"#include <stdint.h>\n\n"
        f"#define {macro}_WIDTH  {width}\n"
        f"#define {macro}_HEIGHT {height}\n\n"
        f"extern const uint16_t {symbol}[{width * height}];\n\n"
        f"#endif\n",
        encoding="ascii",
    )

    source_lines = [
        f"#include \"{stem}.h\"",
        "",
        f"const uint16_t {symbol}[{width * height}] = {{",
    ]
    for index in range(0, len(values), 12):
        chunk = values[index:index + 12]
        source_lines.append("\t" + ", ".join(f"0x{value:04X}" for value in chunk) + ",")
    source_lines.extend(["};", ""])
    source_path.write_text("\n".join(source_lines), encoding="ascii")

    return stem, symbol, width, height


def main():
    CODE_DIR.mkdir(parents=True, exist_ok=True)
    images = sorted(SOURCE_DIR.glob("*.jpg")) + sorted(SOURCE_DIR.glob("*.jpeg"))
    if not images:
        raise SystemExit(f"No JPG files found in {SOURCE_DIR}")

    assets = [write_image_files(image_path) for image_path in images]

    include_lines = [f"#include \"{stem}.h\"" for stem, _, _, _ in assets]
    assets_h = [
        "#ifndef IMAGE_ASSETS_H",
        "#define IMAGE_ASSETS_H",
        "",
        "#include <stdint.h>",
        "",
        *include_lines,
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
        "const image_rgb565_t image_assets[IMAGE_ASSET_COUNT] = {",
    ]
    for stem, symbol, _, _ in assets:
        macro = stem.upper()
        assets_c.append(f"\t{{ {macro}_WIDTH, {macro}_HEIGHT, {symbol} }},")
    assets_c.extend(["};", ""])
    (CODE_DIR / "image_assets.c").write_text("\n".join(assets_c), encoding="ascii")

    print(f"Generated {len(assets)} RGB565 image assets in {CODE_DIR}")
    for stem, symbol, width, height in assets:
        print(f"{stem}: {width}x{height} -> {symbol}")


if __name__ == "__main__":
    main()
