#!/usr/bin/env python3
"""Bake the app icon into the sizes an icon theme expects.

A compositor scales one large PNG down to the 16-24px the titlebar uses, which looks mushy;
a real downscale per size looks sharp. The output is committed, so the build needs no Pillow.

  tools/gen_app_icon.py                     # assets/aeroclicker.png -> assets/icons/app
  tools/gen_app_icon.py other.png out/      # explicit source and output directory

Writes <out>/<size>x<size>/aeroclicker.png for every size in SIZES. Requires Pillow.
"""

import pathlib
import sys

from PIL import Image

SIZES = (16, 24, 32, 48, 64, 128, 256, 512)


def main(argv):
    src = pathlib.Path(argv[1] if len(argv) > 1 else "assets/aeroclicker.png")
    out = pathlib.Path(argv[2] if len(argv) > 2 else "assets/icons/app")

    image = Image.open(src).convert("RGBA")
    # Sources are usually drawn on a padded canvas (assets/aeroclicker.png is 1024px with the
    # artwork only ~684px across). Keeping that padding would bake every size with a transparent
    # border, so the icon sits small and soft in its box; trim to the artwork so it is flush and
    # every pixel of the size goes to the icon.
    artwork = image.split()[3].getbbox()
    if artwork:
        image = image.crop(artwork)

    for size in SIZES:
        target = out / f"{size}x{size}" / (src.stem + ".png")
        target.parent.mkdir(parents=True, exist_ok=True)
        image.resize((size, size), Image.LANCZOS).save(target, optimize=True)
        print(f"  {target} ({target.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
