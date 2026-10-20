#!/usr/bin/env python3
"""SpaceSlicer logo set: writes SVGs (icon, wordmark) used to replace the OrcaSlicer artwork.
The wordmark is converted to plain paths (wxWidgets' SVG renderer has no <text> support).
Usage: make_logos.py <out_dir> [font.ttf]"""
import sys, os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.boundsPen import BoundsPen

OUT = sys.argv[1]
FONT = sys.argv[2] if len(sys.argv) > 2 else "/usr/share/fonts/truetype/google-fonts/Poppins-Bold.ttf"
os.makedirs(OUT, exist_ok=True)

PURPLE = ["#5b35c4", "#7b4fe0", "#9a6ff0", "#b892ff", "#d9c4ff"]

def cube(ox, oy, s, grey=False):
    """Sliced isometric cube in a 64-unit box, scaled by s and offset."""
    def P(pts): return " ".join("%.2f,%.2f" % (ox + x * s, oy + y * s) for x, y in pts)
    cols = PURPLE if not grey else ["#5a5a5a", "#6e6e6e", "#858585", "#a0a0a0", "#c4c4c4"]
    parts = []
    h, gap, layers = 4.2, 1.6, 4
    for i in reversed(range(layers)):           # bottom layer first
        y = 2.0 + i * (h + gap)
        left = [(14, 21 + y), (32, 30 + y), (32, 30 + y + h), (14, 21 + y + h)]
        right = [(32, 30 + y), (50, 21 + y), (50, 21 + y + h), (32, 30 + y + h)]
        shade = max(0, 3 - i)
        parts.append('<polygon points="%s" fill="%s"/>' % (P(left), cols[max(0, shade - 1)]))
        parts.append('<polygon points="%s" fill="%s"/>' % (P(right), cols[shade]))
    parts.append('<polygon points="%s" fill="%s"/>' % (P([(32, 12), (50, 21), (32, 30), (14, 21)]), cols[4]))
    return "\n".join(parts)

def bg(x, y, w, r, grey=False):
    g = ("#3a3a3a", "#1c1c1c") if grey else ("#2d1d58", "#110c22")
    gid = "bg%d" % int(x * 7 + y * 13 + w)
    return ('<defs><linearGradient id="%s" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="%s"/>'
            '<stop offset="1" stop-color="%s"/></linearGradient></defs>'
            '<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f" rx="%.2f" fill="url(#%s)"/>') % (gid, g[0], g[1], x, y, w, w, r, gid)

def icon_svg(size, grey=False, transparent=False, circle=False):
    s = size / 64.0
    body = ""
    if circle:
        body += '<circle cx="%.2f" cy="%.2f" r="%.2f" fill="%s"/>' % (size / 2, size / 2, size / 2, "#1a1233")
    elif not transparent:
        body += bg(0, 0, size, size * 0.22, grey)
    pad = 0.0 if transparent else size * 0.04
    inner = (size - 2 * pad) / 64.0
    body += cube(pad, pad + size * 0.02, inner, grey)
    return '<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">%s</svg>' % (size, size, size, size, body)

font = TTFont(FONT)
gs = font.getGlyphSet(); cmap = font.getBestCmap(); upm = font["head"].unitsPerEm
def text_paths(text, x, baseline, px, color_split):
    """Returns svg paths for text; first color_split chars in colors[0], rest colors[1]."""
    sc = px / upm
    out, cx = [], x
    for i, ch in enumerate(text):
        gname = cmap[ord(ch)]
        pen = SVGPathPen(gs)
        tp = TransformPen(pen, (sc, 0, 0, -sc, cx, baseline))
        gs[gname].draw(tp)
        d = pen.getCommands()
        if d:
            out.append((i < color_split, d))
        cx += gs[gname].width * sc
    return out, cx - x

def wordmark(x, baseline, px, c1, c2):
    paths, w = text_paths("SpaceSlicer", x, baseline, px, 5)
    return "".join('<path d="%s" fill="%s"/>' % (d, c1 if first else c2) for first, d in paths), w

def horizontal(W, H, c1, c2):
    ic = H * 0.86
    body = '<g transform="translate(%.2f,%.2f)">%s</g>' % (H * 0.07, H * 0.07, icon_svg(int(ic))[icon_svg(int(ic)).index(">") + 1:-6])
    px = H * 0.46
    wm, w = wordmark(H * 0.07 + ic + H * 0.14, H * 0.66, px, c1, c2)
    total = H * 0.07 + ic + H * 0.14 + w + H * 0.1
    sx = min(1.0, W / total)
    return '<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d"><g transform="scale(%.4f)">%s%s</g></svg>' % (W, H, W, H, sx, body, wm)

def splash(size, c1, c2):
    ic = size * 0.5
    inner = icon_svg(int(ic))
    body = '<g transform="translate(%.2f,%.2f)">%s</g>' % ((size - ic) / 2, size * 0.12, inner[inner.index(">") + 1:-6])
    px = size * 0.13
    _, w = text_paths("SpaceSlicer", 0, 0, px, 5)
    wm, _ = wordmark((size - w) / 2, size * 0.12 + ic + size * 0.18, px, c1, c2)
    return '<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">%s%s</svg>' % (size, size, size, size, body, wm)

def write(name, svg):
    open(os.path.join(OUT, name), "w").write(svg)

write("icon_1024.svg", icon_svg(1024))
write("icon_64.svg", icon_svg(64))
write("icon_grey_1024.svg", icon_svg(1024, grey=True))
write("icon_transparent_1024.svg", icon_svg(1024, transparent=True))
write("icon_circle_1280.svg", icon_svg(1280, circle=True))
write("horizontal_light.svg", horizontal(214, 80, "#1c1726", "#7b4fe0"))   # light background
write("horizontal_dark.svg", horizontal(214, 80, "#ece8f4", "#b280ff"))    # dark background
write("about_light.svg", horizontal(560, 125, "#1c1726", "#7b4fe0"))
write("about_dark.svg", horizontal(560, 125, "#ece8f4", "#b280ff"))
write("splash_light.svg", splash(480, "#1c1726", "#7b4fe0"))
write("splash_dark.svg", splash(480, "#ece8f4", "#b280ff"))
print("ok", OUT)
