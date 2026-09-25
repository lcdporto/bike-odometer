"""Render app branding from docs/assets/logo.svg.

Run with Python 3, Pillow, and rsvg-convert (librsvg) installed.
"""

from io import BytesIO
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw


APP = Path(__file__).resolve().parents[1]
SOURCE = APP.parent / "docs/assets/logo.svg"
RES = APP / "android/app/src/main/res"
NAVY = "#102b36"
NS = "{http://www.w3.org/2000/svg}"


def render(svg, size):
    result = subprocess.run(
        ["rsvg-convert", "--width", str(size), "--height", str(size)],
        input=svg, capture_output=True, check=True,
    )
    return Image.open(BytesIO(result.stdout)).convert("RGBA")


source = SOURCE.read_bytes()
tree = ET.fromstring(source)
# The emblem is shared by the launcher, splash screens, and in-app SVG.
tree.remove(tree.find(f"{NS}rect"))
emblem = ET.tostring(tree)


def centered(size, fraction, background=NAVY):
    canvas = Image.new("RGBA", size, background)
    edge = round(min(size) * fraction)
    mark = render(emblem, edge)
    canvas.alpha_composite(mark, ((size[0] - edge) // 2, (size[1] - edge) // 2))
    return canvas


(APP / "src/lib/assets/favicon.svg").write_bytes(source)
assets = APP / "assets"
render(source, 1024).save(assets / "icon-only.png")
Image.new("RGB", (1024, 1024), NAVY).save(assets / "icon-background.png")
centered((1024, 1024), 0.60, (0, 0, 0, 0)).save(assets / "icon-foreground.png")
for name in ("splash.png", "splash-dark.png"):
    centered((2732, 2732), 0.35).save(assets / name)

for path in (APP / "icons").glob("icon-*.webp"):
    edge = int(path.stem.split("-")[1])
    render(source, edge).save(path, lossless=True)

# Adaptive layers use a 108dp canvas; the emblem fits inside its safe centre.
for density, scale in {"ldpi": 0.75, "mdpi": 1, "hdpi": 1.5,
                       "xhdpi": 2, "xxhdpi": 3, "xxxhdpi": 4}.items():
    folder = RES / f"mipmap-{density}"
    edge = round(48 * scale)
    render(source, edge).save(folder / "ic_launcher.png")
    rounded = centered((edge * 4, edge * 4), 0.85)
    mask = Image.new("L", rounded.size)
    ImageDraw.Draw(mask).ellipse((0, 0, rounded.width - 1, rounded.height - 1), fill=255)
    rounded.putalpha(mask)
    rounded.resize((edge, edge), Image.Resampling.LANCZOS).save(folder / "ic_launcher_round.png")
    layer_edge = round(108 * scale)
    centered((layer_edge, layer_edge), 0.60, (0, 0, 0, 0)).save(folder / "ic_launcher_foreground.png")
    Image.new("RGB", (layer_edge, layer_edge), NAVY).save(folder / "ic_launcher_background.png")

for path in RES.glob("drawable*/splash.png"):
    with Image.open(path) as previous:
        size = previous.size
    centered(size, 0.45).save(path)

print("Updated favicon, source artwork, web icons, Android launcher icons, and splash screens.")
