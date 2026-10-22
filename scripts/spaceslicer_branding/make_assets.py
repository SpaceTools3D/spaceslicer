#!/usr/bin/env python3
"""Rasterises the SpaceSlicer SVGs (make_logos.py) and overwrites the OrcaSlicer artwork in resources/images.
File names are kept so no code paths change. Needs Python Playwright (Chromium) and Pillow.
Usage: make_assets.py <svg_dir> <repo_root>"""
import asyncio, os, shutil, sys
from io import BytesIO
from PIL import Image
from playwright.async_api import async_playwright

SVG, REPO = sys.argv[1], sys.argv[2]
IMG = os.path.join(REPO, "resources", "images")

async def raster(svg_name, px):
    async with async_playwright() as p:
        b = await p.chromium.launch()
        pg = await b.new_page(viewport={"width": px, "height": px})
        # Open the SVG itself: an about:blank page (set_content) may not load file:// images, which left every icon blank.
        await pg.goto("file://" + os.path.join(SVG, svg_name))
        await pg.wait_for_timeout(200)
        png = await pg.screenshot(omit_background=True, clip={"x": 0, "y": 0, "width": px, "height": px})
        await b.close()
    return Image.open(BytesIO(png)).convert("RGBA")

def mac_icon(full):
    """macOS draws icons on Apple's grid: an 824 px body centred in 1024 px with a soft shadow."""
    from PIL import ImageFilter
    body = full.resize((824, 824), Image.LANCZOS)
    out = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
    shadow = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
    shadow.paste((0, 0, 0, 90), (100, 112), body.getchannel("A"))
    out = Image.alpha_composite(out, shadow.filter(ImageFilter.GaussianBlur(14)))
    out.alpha_composite(body, (100, 100))
    return out

def save_png(im, name, size):
    im.resize((size, size), Image.LANCZOS).save(os.path.join(IMG, name))

async def main():
    full = await raster("icon_1024.svg", 1024)
    grey = await raster("icon_grey_1024.svg", 1024)
    trans = await raster("icon_transparent_1024.svg", 1024)
    for name, size in [("OrcaSlicer.png", 154), ("OrcaSlicer_128px.png", 128), ("OrcaSlicer_154.png", 154), ("OrcaSlicer_154_title.png", 184),
                       ("OrcaSlicer_192px.png", 192), ("OrcaSlicer_32px.png", 32), ("OrcaSlicer_64.png", 64), ("OrcaSlicer-mac_128px.png", 128),
                       ("OrcaSlicerTitle.png", 154)]:
        save_png(full, name, size)
    save_png(grey, "OrcaSlicer_192px_grayscale.png", 192)
    save_png(trans, "OrcaSlicer_192px_transparent.png", 192)
    full.save(os.path.join(IMG, "OrcaSlicer.ico"), sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    full.resize((256, 256), Image.LANCZOS).save(os.path.join(IMG, "OrcaSlicer-mac_256px.ico"), sizes=[(256, 256)])
    full.resize((154, 154), Image.LANCZOS).save(os.path.join(IMG, "OrcaSlicerTitle.ico"), sizes=[(154, 154)])
    mac = mac_icon(full)
    mac.save(os.path.join(IMG, "OrcaSlicer.icns"))
    mac.save(os.path.join(REPO, "resources", "Icon.icns"))           # the macOS bundle icon (CFBundleIconFile)
    for name, im in (("OrcaSlicer_192px.png", full), ("OrcaSlicer_32px.png", full)):
        if im.resize((32, 32)).getchannel("A").getextrema()[1] < 128:
            raise SystemExit("rasterising failed: %s is empty" % name)
    for src, dst in [("icon_64.svg", "OrcaSlicer.svg"), ("icon_1024.svg", "OrcaSlicer_gradient.svg"), ("icon_grey_1024.svg", "OrcaSlicer_gray.svg"),
                     ("icon_circle_1280.svg", "OrcaSlicer_gradient_circle.svg"), ("icon_transparent_1024.svg", "OrcaSlicer_gradient_narrow.svg"),
                     ("about_light.svg", "OrcaSlicer_about.svg"), ("about_dark.svg", "OrcaSlicer_about_dark.svg"),
                     ("horizontal_light.svg", "OrcaSlicer_horizontal_light.svg"), ("horizontal_dark.svg", "OrcaSlicer_horizontal_dark.svg"),
                     ("splash_light.svg", "splash_logo.svg"), ("splash_dark.svg", "splash_logo_dark.svg")]:
        shutil.copyfile(os.path.join(SVG, src), os.path.join(IMG, dst))
    print("ok")

asyncio.run(main())
