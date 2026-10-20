#!/usr/bin/env python3
"""SpaceSlicer theme: replaces OrcaSlicer's teal accent with the SpaceTools violet and gives the dark
mode a deep-violet, "night sky" tint. Idempotent: running it twice changes nothing the second time.
Usage: apply_theme.py <repo_root>"""
import os, re, sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."

# accent family (light value -> SpaceTools violet). Hex is matched case-insensitively.
HEX = {
    "009688": "7B4FE0",  # ORCA accent
    "26A69A": "9A6FF0",  # accent hover
    "00675B": "5B35C4",  # accent, dark mode
    "008172": "6A3FD6",  # accent hover, dark mode
    "00897B": "6A3FD6",
    "00796B": "5B35C4",
    "4DB6AC": "B892FF",
    "80CBC4": "CDB6FF",
    "B2DFDB": "E0D4FF",
    "E0F2F1": "F3EEFF",
    "BFE1DE": "DED2FB",  # accent 25% (checked dropdown item)
    "E5F0EE": "F1ECFD",  # accent 10% (focused combo)
    "223C3C": "2E2347",  # accent 25%, dark mode
    "283232": "2A2438",  # accent 10%, dark mode
    "3B4446": "1B1430",  # top tab bar (light mode) -> deep violet
}
RGB = {(0, 150, 136): (123, 79, 224), (0, 137, 123): (106, 63, 214), (38, 166, 154): (154, 111, 240), (0, 103, 91): (91, 53, 196)}

# dark-mode neutrals (StateColor.cpp right-hand values): grey -> violet-tinted night colours
DARK = {
    "#2D2D31": "#1C1826",  # window background
    "#36363C": "#241F30", "#36363B": "#231E2E", "#36363D": "#241F31",
    "#2D2D30": "#120E1C",  # top bar
    "#3E3E45": "#2E2740", "#4D4D54": "#3B3251", "#54545B": "#3A3249",
    "#4C4C55": "#352D47", "#242428": "#15111E", "#333337": "#26202F",
    "#4A4A51": "#3D3452", "#27272A": "#1E1928",
}

def sub_hex(text):
    def rep(m):
        v = HEX.get(m.group(2).upper())
        return m.group(1) + v if v else m.group(0)
    return re.sub(r"(#|0x)([0-9A-Fa-f]{6})(?![0-9A-Fa-f])", rep, text)

def sub_rgb(text):
    def rep(m):
        t = tuple(int(x) for x in m.group(2, 3, 4))
        if t not in RGB:
            return m.group(0)
        r, g, b = RGB[t]
        return "%s%d%s%d%s%d" % (m.group(1), r, m.group(5), g, m.group(6), b)
    return re.sub(r"((?:wxColour|rgba?|wxColor)\(\s*)(\d{1,3})(\s*,\s*)(\d{1,3})(\s*,\s*)(\d{1,3})", lambda m: _rgb_fix(m), text)

def _rgb_fix(m):
    t = (int(m.group(2)), int(m.group(4)), int(m.group(6)))
    if t not in RGB:
        return m.group(0)
    r, g, b = RGB[t]
    return "%s%d%s%d%s%d" % (m.group(1), r, m.group(3), g, m.group(5), b)

def sub_triplet(text):
    """Bare accent triples in ImGui/initialiser code: IM_COL32(0, 150, 136, a), ImColor(0, 150, 136, a), {0, 150, 136}."""
    for (a, b, c), (x, y, z) in RGB.items():
        text = re.sub(r"(?<![\d.])%d(\s*,\s*)%d(\s*,\s*)%d(?![\d.])" % (a, b, c), lambda m: "%d%s%d%s%d" % (x, m.group(1), y, m.group(2), z), text)
    return text

def sub_dark(text):
    """Dark-mode greys used directly (outside StateColor's map) -> the same violet-tinted values."""
    def rep(m):
        v = DARK.get("#" + m.group(2).upper())
        return m.group(1) + v[1:] if v else m.group(0)
    return re.sub(r"(#|0x)([0-9A-Fa-f]{6})(?![0-9A-Fa-f])", rep, text)

def sub_imvec(text):
    text = re.sub(r"ImVec4\(0\.00f, 0\.59f, 0\.53f,", "ImVec4(0.48f, 0.31f, 0.88f,", text)
    text = re.sub(r"ImVec4\(0, 0\.588, 0\.533,", "ImVec4(0.482, 0.310, 0.878,", text)
    return text

changed = {}
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
        changed[path] = True

for base, exts, fns in (("src", (".cpp", ".hpp", ".h"), (sub_hex, sub_rgb, sub_triplet, sub_imvec, sub_dark)),
                        ("resources/images", (".svg",), (sub_hex, sub_rgb, sub_dark)),
                        ("resources/web", (".html", ".css", ".js", ".svg"), (sub_hex, sub_rgb, sub_dark))):
    for dp, dn, fnames in os.walk(os.path.join(ROOT, base)):
        if "/node_modules" in dp:
            continue
        for fn in fnames:
            if fn.endswith(exts):
                process(os.path.join(dp, fn), fns)

# targeted edits
def edit(rel, pairs):
    p = os.path.join(ROOT, rel); s = open(p, encoding="utf-8").read(); n = s
    for a, b in pairs:
        n = n.replace(a, b)
    if n != s:
        open(p, "w", encoding="utf-8").write(n); changed[p] = True

sc = os.path.join(ROOT, "src/slic3r/GUI/Widgets/StateColor.cpp")
s = open(sc, encoding="utf-8").read()
def dark_rep(m):
    return m.group(1) + DARK.get(m.group(2).upper(), m.group(2)) + m.group(3)
n = re.sub(r'(\{"#[0-9A-Fa-f]{6}", ")(#[0-9A-Fa-f]{6})("\})', dark_rep, s)
if n != s:
    open(sc, "w", encoding="utf-8").write(n); changed[sc] = True

edit("src/libslic3r/Color.hpp", [
    ("static const ColorRGB ORCA()\t\t{ return {0.0f, 150.f / 255.0f, 136.0f / 255}; }",
     "static const ColorRGB ORCA()\t\t{ return {123.0f / 255.0f, 79.0f / 255.0f, 224.0f / 255.0f}; } // SpaceSlicer violet"),
    ("static const ColorRGBA ORCA()        { return {0.0f, 150.f / 255.0f, 136.0f / 255, 1.0f}; }",
     "static const ColorRGBA ORCA()        { return {123.0f / 255.0f, 79.0f / 255.0f, 224.0f / 255.0f, 1.0f}; } // SpaceSlicer violet"),
])
edit("src/slic3r/GUI/ImGuiWrapper.cpp", [
    ("const ImVec4 ImGuiWrapper::COL_ORCA_DARK         = { 0.f       , 103 / 255.f, 91  / 255.f, 1.f };",
     "const ImVec4 ImGuiWrapper::COL_ORCA_DARK         = { 91 / 255.f, 53 / 255.f, 196 / 255.f, 1.f };"),
    ("const ImVec4 ImGuiWrapper::COL_ORCA_HOVER        = { 38 / 255.f, 166 / 255.f, 154 / 255.f, 1.f };",
     "const ImVec4 ImGuiWrapper::COL_ORCA_HOVER        = { 154 / 255.f, 111 / 255.f, 240 / 255.f, 1.f };"),
    ("const ImVec4 ImGuiWrapper::COL_ORCA_HOVER_DARK   = { 0.f       , 129 / 255.f, 114  / 255.f, 1.f };",
     "const ImVec4 ImGuiWrapper::COL_ORCA_HOVER_DARK   = { 106 / 255.f, 63 / 255.f, 214 / 255.f, 1.f };"),
])
edit("src/slic3r/GUI/GLCanvas3D.cpp", [
    ("static Slic3r::ColorRGBA DEFAULT_BG_LIGHT_COLOR_DARK = { 0.329f, 0.329f, 0.353f, 1.0f };",
     "static Slic3r::ColorRGBA DEFAULT_BG_LIGHT_COLOR_DARK = { 0.180f, 0.161f, 0.235f, 1.0f }; // SpaceSlicer: deep violet night"),
])
print("changed files:", len(changed))
