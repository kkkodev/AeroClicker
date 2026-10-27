#!/usr/bin/env python3
"""Bake the symbolic 16px icons into the compact masks used by gui_icons.cpp.

The UI paints icons in the current ink colour, so only the alpha channel is needed.  Baking
once keeps the build free of an SVG/PNG runtime dependency while the glyphs remain exactly the
shipped artwork.  There are no hand-drawn glyphs left: every icon in the enum has a mask.

Sources, in order of preference:

  oxygen  Oxygen icon theme, bundled under assets/icons/oxygen (LGPL-3.0-or-later).  Used
          wherever Oxygen ships a symbolic glyph that survives 16px.
  breeze  KDE Breeze icon theme, read from the system theme directory (LGPL-3.0-or-later).
          Oxygen has no symbolic home/settings/chart/clock/globe/terminal/refresh/add icon, so
          Breeze supplies those in the same flat symbolic language.

The Oxygen mouse has no separate left/right button artwork, so the two mouse masks are made
here by lighting up the matching top button of the shipped input-mouse glyph.  The fill follows
the glyph's own silhouette, which is what keeps the lit button inside the body at 16px.

See assets/icons/COPYING and assets/icons/NOTICE.

Usage:
    tools/gen_symbolic_icons.py [--theme /usr/share/icons/breeze]
                                [--oxygen assets/icons/oxygen]

Requires Inkscape for the SVG -> 16px raster step (development only).
"""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import tempfile

from PIL import Image

SIZE = 16

# OxIcon enum name -> (source, path inside that source).  Paths are relative to the theme root.
ICONS = {
    "Home": ("breeze", "actions/16/go-home-symbolic.svg"),
    "ArrowLeft": ("oxygen", "actions/16/arrow-left.svg"),
    "ArrowRight": ("oxygen", "actions/16/arrow-right.svg"),
    "List": ("oxygen", "actions/16/view-list-text.svg"),
    "Sliders": ("breeze", "actions/16/configure-symbolic.svg"),
    "Chart": ("breeze", "actions/16/view-statistics-symbolic.svg"),
    "Gauge": ("oxygen", "applets/22/speedometer.svg"),
    "Person": ("oxygen", "actions/16/im-user.svg"),
    "Clock": ("breeze", "actions/16/chronometer-symbolic.svg"),
    "Click": ("breeze", "actions/16/cursor-arrow-symbolic.svg"),
    "Pause": ("oxygen", "applets/22/media-playback-paused.svg"),
    "Globe": ("breeze", "actions/16/globe-symbolic.svg"),
    "Checklist": ("breeze", "actions/16/view-task-symbolic.svg"),
    "Play": ("oxygen", "applets/22/media-playback-playing.svg"),
    "Keyboard": ("breeze", "devices/16/input-keyboard-symbolic.svg"),
    "Grid": ("breeze", "actions/16/view-grid-symbolic.svg"),
    "Speaker": ("oxygen", "applets/22/audio-volume-high.svg"),
    "Folder": ("oxygen", "applets/16/document-open-folder.svg"),
    "Sun": ("oxygen", "applets/22/weather-clear.svg"),
    "Terminal": ("breeze", "apps/16/utilities-terminal-symbolic.svg"),
    "Pulse": ("breeze", "actions/16/office-chart-line-symbolic.svg"),
    "Record": ("breeze", "actions/16/media-record-symbolic.svg"),
    "Export": ("breeze", "actions/16/document-export-symbolic.svg"),
    "Save": ("oxygen", "actions/16/document-save.svg"),
    "Trash": ("oxygen", "applets/22/user-trash.svg"),
    "Refresh": ("breeze", "actions/16/view-refresh-symbolic.svg"),
    "Stop": ("oxygen", "applets/22/media-playback-stopped.svg"),
    "Plus": ("breeze", "actions/16/list-add-symbolic.svg"),
    "Close": ("oxygen", "actions/16/window-close.svg"),
}

# Mouse masks are derived from this glyph; the regions are the top button of each side,
# (x0, x1, y0, y1) inclusive.
MOUSE_BASE = ("oxygen", "applets/22/input-mouse.svg")
MOUSE_FILL = {
    "MouseLeft": [(4, 7, 1, 7)],
    "MouseRight": [(9, 12, 1, 7)],
}
# Pixels fainter than this are the outer antialiasing fringe; leaving it alone keeps the body's
# rounded silhouette instead of squaring off the filled corner.
MOUSE_FRINGE = 25

# The applets artwork is drawn on a 22px grid, so at 16px its 1.5px strokes cover two texels at
# half strength and bake as mid greys next to the crisp 16px glyphs.  Raising everything above
# the faint wash restores the weight of the native set without touching the icon's interior.
SOFT_ART = "applets/22/"
INK_GAIN = 1.7
WASH = 32


def render(root: pathlib.Path, rel: str) -> Image.Image:
    """Rasterise one SVG to a 16px alpha mask."""
    with tempfile.TemporaryDirectory() as tmp:
        out = pathlib.Path(tmp) / "icon.png"
        subprocess.run(
            [
                "inkscape",
                str(root / rel),
                "--export-type=png",
                f"--export-filename={out}",
                f"--export-width={SIZE}",
                f"--export-height={SIZE}",
            ],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        im = Image.open(out).convert("RGBA")
    if im.size != (SIZE, SIZE):
        im = im.resize((SIZE, SIZE), Image.LANCZOS)
    # Symbolic icons are flat ink; the alpha channel is the complete mask.
    return im.getchannel("A")


def fill_button(mask: Image.Image, regions: list[tuple[int, int, int, int]]) -> Image.Image:
    """Light up the matching top button, following the glyph's own rounded silhouette.

    The outer antialiasing fringe (alpha < 25) is left alone, so the filled cap keeps the
    body's curve instead of squaring off the corner and looking like it spills outside.
    """
    out = mask.copy()
    px = out.load()
    for x0, x1, y0, y1 in regions:
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if px[x, y] >= MOUSE_FRINGE:
                    px[x, y] = 255
    return out


def sharpen(mask: Image.Image) -> Image.Image:
    """Bring downscaled 22px strokes back up to the weight of the native 16px glyphs."""
    src = mask.load()
    out = mask.copy()
    px = out.load()
    for y in range(SIZE):
        for x in range(SIZE):
            a = src[x, y]
            if a > WASH:
                px[x, y] = min(255, int(WASH + (a - WASH) * INK_GAIN))
    return out


def alpha_values(mask: Image.Image) -> list[int]:
    data = mask.get_flattened_data() if hasattr(mask, "get_flattened_data") else mask.getdata()
    return list(data)


def emit(out: pathlib.Path, theme: pathlib.Path, oxygen: pathlib.Path, with_mouse: bool) -> None:
    roots = {"breeze": theme, "oxygen": oxygen}
    lines = [
        "// Generated by tools/gen_symbolic_icons.py -- do not edit by hand.",
        "// 16px symbolic masks. Oxygen artwork is bundled in assets/icons/oxygen; the glyphs",
        "// Oxygen does not ship symbolically come from KDE Breeze. Both are LGPL-3.0-or-later.",
        "// See assets/icons/COPYING for the licence and assets/icons/NOTICE for attribution.",
        "",
    ]

    def entry(name: str, data: list[int], note: str) -> None:
        lines.append(f"// {note}")
        lines.append(f"static const unsigned char kOxMask_{name}[{SIZE * SIZE}] = {{")
        for i in range(0, len(data), SIZE):
            lines.append("    " + ", ".join(f"{v:3d}" for v in data[i:i + SIZE]) + ",")
        lines.append("};")
        lines.append("")

    for name, (source, rel) in ICONS.items():
        mask = render(roots[source], rel)
        note = f"{source}: {rel}"
        if SOFT_ART in rel:
            mask = sharpen(mask)
            note += " (ink restored)"
        entry(name, alpha_values(mask), note)

    if with_mouse:
        base = render(roots[MOUSE_BASE[0]], MOUSE_BASE[1])
        if SOFT_ART in MOUSE_BASE[1]:
            base = sharpen(base)
        for name, regions in MOUSE_FILL.items():
            entry(name, alpha_values(fill_button(base, regions)),
                  f"{MOUSE_BASE[0]}: {MOUSE_BASE[1]} (top button filled)")

    lines.append("struct OxMaskEntry { OxIcon icon; const unsigned char* px; };")
    lines.append("static const OxMaskEntry kOxMasks[] = {")
    for name in [*ICONS, *(MOUSE_FILL if with_mouse else ())]:
        lines.append(f"    {{ OxIcon::{name}, kOxMask_{name} }},")
    lines.append("};")
    lines.append("")
    out.write_text("\n".join(lines))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--theme", default="/usr/share/icons/breeze")
    ap.add_argument("--oxygen", default="assets/icons/oxygen")
    ap.add_argument("--out", default="src/gui_icons_symbolic.inc")
    args = ap.parse_args()
    emit(pathlib.Path(args.out), pathlib.Path(args.theme), pathlib.Path(args.oxygen), True)
    print(f"wrote {args.out} ({len(ICONS) + len(MOUSE_FILL)} icons)")


if __name__ == "__main__":
    main()
