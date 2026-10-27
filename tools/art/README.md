# The art of the remake

These scripts draw the files of `art/`. A file of `art/` takes the place of
the atlas image with the same name.

The scripts need Python 3, `fonttools`, `rsvg-convert`, and the font
Impact of macOS.

    python3 -m venv /tmp/artenv
    /tmp/artenv/bin/pip install fonttools
    /tmp/artenv/bin/python tools/art/title.py art
