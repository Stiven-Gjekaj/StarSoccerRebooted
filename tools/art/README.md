# The art of the remake

These scripts draw the files of `art/`. A file of `art/` takes the place of
the atlas image with the same name. A file `art/<name>.patch.png` changes
only some pixels of the atlas image: each pixel of the patch with an alpha
above 0 takes the place of the pixel under it. The patch has the size of
the box of the image.

The scripts need Python 3, `fonttools`, `rsvg-convert`, and the font
Impact of macOS.

    python3 -m venv /tmp/artenv
    /tmp/artenv/bin/pip install fonttools
    /tmp/artenv/bin/python tools/art/title.py art
    /tmp/artenv/bin/python tools/art/marks.py art
    /tmp/artenv/bin/python tools/art/stadium.py art
    /tmp/artenv/bin/python tools/art/cards.py art
    /tmp/artenv/bin/python tools/art/casino.py art
    /tmp/artenv/bin/python tools/art/banners.py art
