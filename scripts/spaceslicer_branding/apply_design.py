#!/usr/bin/env python3
"""SpaceSlicer design language, step 2 (run after apply_theme.py): moves the dark palette onto the
tokens of the SpaceSlicer home page (resources/web/homepage/css/dark.css) so every tab, dialog, web
page and the 3D canvas share one set of colours, and gives the top tab bar one colour in both modes.
Idempotent. Usage: apply_design.py <repo_root>"""
import os, re, sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."

# Dark-only neutrals written by apply_theme.py -> home page tokens. These values exist only as
# dark-mode colours, so they are replaced wherever they appear.
NEUTRALS = {
    "1C1826": "17121F",  # window background            -> --hp-side
    "241F30": "1E1928",  # raised panel / title bar     -> --hp-card
    "231E2E": "1E1928",
    "241F31": "1E1928",
    "15111E": "130F1B",  # deepest background           -> --hp-bg
    "26202F": "221C2C",  # disabled background
    "2E2740": "27222F",  # button background            -> --hp-sunken
    "3B3251": "322B3F",  # button hover
    "352D47": "2C2638",  # separator / title line       -> --hp-border
    "2A2438": "231A34",  # accent 10 % on the window background
    "2E2347": "342653",  # accent 25 % on the window background
    "120E1C": "1A1424",  # top tab bar, dark mode
    "1B1430": "1A1424",  # top tab bar, light mode: the bar is the same deep violet in both modes
}

def sub_neutrals(text):
    def rep(m):
        v = NEUTRALS.get(m.group(2).upper())
        return m.group(1) + v if v else m.group(0)
    return re.sub(r"(#|0x)([0-9A-Fa-f]{6})(?![0-9A-Fa-f])", rep, text)

changed = []
def process(path, fns):
    try:
        s = open(path, encoding="utf-8").read()
    except (UnicodeDecodeError, OSError):
        return
    n = s
    for f in fns:
        n = f(n)
    if n != s:
        open(path, "w", encoding="utf-8").write(n)
        changed.append(path)

for base, ok in (("src", lambda f: f.endswith((".cpp", ".hpp", ".h"))),
                 ("resources/web", lambda f: f.endswith((".html", ".css", ".js", ".svg")) and not f.endswith(".min.js")),
                 ("resources/images", lambda f: f.endswith(".svg") and "dark" in f)):
    for dp, dn, fnames in os.walk(os.path.join(ROOT, base)):
        if "node_modules" in dp or "/three" in dp:
            continue
        for fn in fnames:
            if ok(fn):
                process(os.path.join(dp, fn), (sub_neutrals,))

def edit(rel, pairs):
    p = os.path.join(ROOT, rel)
    s = open(p, encoding="utf-8").read()
    n = s
    for a, b in pairs:
        if a in n:
            n = n.replace(a, b)
        elif b not in n:
            raise SystemExit("%s: neither %r nor its replacement found" % (rel, a))
    if n != s:
        open(p, "w", encoding="utf-8").write(n)
        changed.append(p)

# accent and text: the dark column only, so the light values that share these hex codes stay
edit("src/slic3r/GUI/Widgets/StateColor.cpp", [
    ('{"#7B4FE0", "#5B35C4"}', '{"#7B4FE0", "#8B63F0"}'),   # accent       -> --hp-violet
    ('{"#9A6FF0", "#6A3FD6"}', '{"#9A6FF0", "#9D7AF3"}'),   # accent hover
    ('{"#262E30", "#EFEFF0"}', '{"#262E30", "#F1EEF6"}'),   # text         -> --hp-text
    ('{"#6B6B6A", "#B3B3B5"}', '{"#6B6B6A", "#A39CB1"}'),   # side text    -> --hp-muted
    ('{"#363636", "#B2B3B5"}', '{"#363636", "#C4BED1"}'),   # sidebar labels
    ('{"#D9D9D9", "#1E1928"}', '{"#D9D9D9", "#2C2638"}'),   # toggle track, off the new card colour
])
edit("src/slic3r/GUI/BitmapCache.cpp", [
    ('replaces["\\"#7B4FE0\\""] = "\\"#5B35C4\\"";', 'replaces["\\"#7B4FE0\\""] = "\\"#8B63F0\\"";'),
    ('replaces["#7B4FE0"] = "#5B35C4";', 'replaces["#7B4FE0"] = "#8B63F0";'),
    ('replaces["\\"#262E30\\""] = "\\"#EFEFF0\\"";', 'replaces["\\"#262E30\\""] = "\\"#F1EEF6\\"";'),
    ('replaces["#262E30"] = "#EFEFF0"; // ORCA', 'replaces["#262E30"] = "#F1EEF6"; // ORCA'),
])
edit("src/slic3r/GUI/ImGuiWrapper.cpp", [
    ('decode_color_to_float_array("#5B35C4")));\n', 'decode_color_to_float_array("#8B63F0")));\n'),
    ('decode_color_to_float_array("#6A3FD6")));', 'decode_color_to_float_array("#9D7AF3")));'),
    ('ImGuiCol_ButtonActive,  to_ImVec4(decode_color_to_float_array("#5B35C4")));', 'ImGuiCol_ButtonActive,  to_ImVec4(decode_color_to_float_array("#8B63F0")));'),
    ("COL_SEPARATOR_DARK    = { 0.24f, 0.24f, 0.27f, 1.0f };", "COL_SEPARATOR_DARK    = { 44 / 255.f, 38 / 255.f, 56 / 255.f, 1.0f }; // SpaceSlicer --hp-border"),
    ("COL_WINDOW_BG_DARK    = { 45 / 255.f, 45 / 255.f, 49 / 255.f, 1.f };", "COL_WINDOW_BG_DARK    = { 30 / 255.f, 25 / 255.f, 40 / 255.f, 1.f }; // SpaceSlicer --hp-card"),
    ("COL_TOOLBAR_BG_DARK   = { 57  / 255.f, 60  / 255.f, 66  / 255.f, 1.f }; // ORCA color matches with toolbar_background_dark.png",
     "COL_TOOLBAR_BG_DARK   = { 30  / 255.f, 25  / 255.f, 40  / 255.f, 1.f }; // SpaceSlicer --hp-card, matches toolbar_background_dark.png"),
    ("COL_ORCA_DARK         = { 91 / 255.f, 53 / 255.f, 196 / 255.f, 1.f };", "COL_ORCA_DARK         = { 139 / 255.f, 99 / 255.f, 240 / 255.f, 1.f };"),
    ("COL_ORCA_HOVER_DARK   = { 106 / 255.f, 63 / 255.f, 214 / 255.f, 1.f };", "COL_ORCA_HOVER_DARK   = { 157 / 255.f, 122 / 255.f, 243 / 255.f, 1.f };"),
])
edit("src/slic3r/GUI/GLCanvas3D.cpp", [
    ("DEFAULT_BG_LIGHT_COLOR_DARK = { 0.180f, 0.161f, 0.235f, 1.0f }; // SpaceSlicer: deep violet night",
     "DEFAULT_BG_LIGHT_COLOR_DARK = { 0.075f, 0.059f, 0.106f, 1.0f }; // SpaceSlicer --hp-bg #130F1B"),
])
edit("resources/web/include/global.css", [
    ("--main-color\t\t\t: #5B35C4;", "--main-color\t\t\t: #8B63F0;"),
    ("--main-color-hover\t\t: #6A3FD6;", "--main-color-hover\t\t: #9D7AF3;"),
])

# the 3D toolbar's background texture is a flat swatch of COL_TOOLBAR_BG_DARK
try:
    from PIL import Image
    p = os.path.join(ROOT, "resources/images/toolbar_background_dark.png")
    im = Image.open(p).convert("RGBA")
    if im.getpixel((0, 0))[:3] != (30, 25, 40):
        Image.new("RGBA", im.size, (30, 25, 40, 255)).save(p)
        changed.append(p)
except ImportError:
    print("Pillow missing: recolour resources/images/toolbar_background_dark.png to #1E1928 by hand")

print("%d files changed" % len(changed))
for p in changed:
    print("  " + os.path.relpath(p, ROOT))

# ---- raster assets: the upstream logo on the setup wizard, and teal left in UI bitmaps ----
try:
    import colorsys, shutil
    from PIL import Image

    def recolor_teal(path, dark_greys=None):
        im = Image.open(path).convert("RGBA")
        px = im.load()
        done = False
        for y in range(im.size[1]):
            for x in range(im.size[0]):
                r, g, b, a = px[x, y]
                if a == 0:
                    continue
                h, l, s = colorsys.rgb_to_hls(r / 255, g / 255, b / 255)
                if s > 0.3 and 0.42 < h < 0.55:
                    # teal (#009688 family) -> SpaceSlicer violet (#7B4FE0 family), same lightness
                    nr, ng, nb = colorsys.hls_to_rgb(0.718, min(0.85, l * 1.15), min(1.0, s * 0.75))
                    px[x, y] = (round(nr * 255), round(ng * 255), round(nb * 255), a)
                    done = True
                elif dark_greys and (r, g, b) in dark_greys:
                    px[x, y] = dark_greys[(r, g, b)] + (a,)
                    done = True
        if done:
            im.save(path)
            changed.append(path)

    for rel in ("resources/web/model/img/null.png", "resources/web/guide/11/earth.png", "resources/web/guide/11/tip.png",
                "resources/images/texture_displacement/select_all.png",
                "resources/images/texture_displacement/smart_fill_angle_high.png",
                "resources/images/texture_displacement/smart_fill_angle_low.png"):
        recolor_teal(os.path.join(ROOT, rel))
    # the dark-mode empty-state picture also sits on the card colour
    recolor_teal(os.path.join(ROOT, "resources/web/model/img/null2.png"),
                 {(45, 45, 48): (30, 25, 40), (54, 54, 60): (39, 34, 47)})

    logo_src = os.path.join(ROOT, "resources/images/OrcaSlicer_154.png")   # the SpaceSlicer logo since make_logos.py
    for rel, size in (("resources/web/image/logo.png", 154), ("resources/web/image/logo2.png", 339)):
        dst = os.path.join(ROOT, rel)
        img = Image.open(logo_src).convert("RGBA").resize((size, size), Image.LANCZOS)
        if Image.open(dst).convert("RGBA").tobytes() != img.tobytes():
            img.save(dst)
            changed.append(dst)
except ImportError:
    pass

print("%d raster files changed" % sum(1 for p in changed if p.endswith(".png")))
