"""Makes the fallback fonts of the remake.

Android draws a character that the font of the game does not have with the
font of the system (FontNative.GetTextPaint). Roboto has the Latin and the
Cyrillic characters. Noto Sans CJK has the Japanese, the Korean, and the
Chinese characters. This script keeps only the characters of the glyph
tables of the import that the font of the game does not have.

    python fallback.py <import>/Fonts <Roboto.ttf> <NotoSansCJK.ttc> fonts
"""

import io
import os
import struct
import sys

from fontTools import subset
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont, newTable
from fontTools.varLib import instancer

# The face of the collection for each language, as the fonts.xml of Android
# gives them.
CJK_FACES = {"ja": 0, "ko": 1, "ch": 2}


def table_characters(path):
    """The characters of a glyph table (GlyphMetricsLibrary)."""
    data = open(path, "rb").read()
    count = struct.unpack(">i", data[:4])[0]
    characters = set()
    for i in range(count):
        _, _, start, end = struct.unpack(">4i", data[4 + 16 * i : 20 + 16 * i])
        for offset in range(start, end, 14):
            characters.add(struct.unpack(">i", data[offset : offset + 4])[0])
    return characters


def reloaded(font):
    """The font, written and read again, so that each table agrees."""
    data = io.BytesIO()
    font.save(data)
    data.seek(0)
    return TTFont(data)


def static_font(font):
    """The weight 400 of a variable font, with TrueType outlines. Android
    draws the normal text at the weight 400. stb_truetype cannot read CFF2,
    so the outlines become quadratic curves (the otf2ttf of fontTools)."""
    if "fvar" in font:
        font = reloaded(instancer.instantiateVariableFont(reloaded(font), {"wght": 400}))
    tag = "CFF2" if "CFF2" in font else "CFF " if "CFF " in font else None
    if not tag:
        return font
    order = font.getGlyphOrder()
    glyphs = font.getGlyphSet()
    font["loca"] = newTable("loca")
    glyf = font["glyf"] = newTable("glyf")
    glyf.glyphOrder = order
    glyf.glyphs = {}
    for name in order:
        pen = TTGlyphPen(glyphs)
        glyphs[name].draw(Cu2QuPen(pen, 1.0, reverse_direction=True))
        glyf.glyphs[name] = pen.glyph()
    del font[tag]
    for unused in ("VORG", "vhea", "vmtx", "HVAR", "VVAR", "BASE"):
        if unused in font:
            del font[unused]
    font["head"].glyphDataFormat = 0
    maxp = font["maxp"]
    maxp.tableVersion = 0x00010000
    for field in ("maxZones", "maxTwilightPoints", "maxStorage", "maxFunctionDefs", "maxInstructionDefs",
                  "maxStackElements", "maxSizeOfInstructions", "maxComponentElements"):
        setattr(maxp, field, 0)
    maxp.maxZones = 1
    font["post"].formatType = 2.0
    font["post"].extraNames = []
    font["post"].mapping = {}
    font.sfntVersion = "\x00\x01\x00\x00"
    return reloaded(font)


def write_subset(font, characters, path):
    options = subset.Options()
    options.name_IDs = ["*"]
    options.name_languages = ["*"]
    options.notdef_outline = True
    options.hinting = True
    # The remake draws one glyph for each character, with no shaping.
    options.layout_features = []
    subsetter = subset.Subsetter(options)
    subsetter.populate(unicodes=sorted(characters))
    subsetter.subset(font)
    # The subset first, because the full font takes a long time to change.
    font = static_font(font)
    font.save(path)
    print(f"{path}: {len(characters)} characters, {os.path.getsize(path)} bytes")


def main():
    fonts, roboto_path, cjk_path, out = sys.argv[1:5]
    os.makedirs(out, exist_ok=True)
    game = set(TTFont(os.path.join(fonts, "Veriox-Rg.ttf")).getBestCmap())
    game |= set(TTFont(os.path.join(fonts, "TCB.ttf")).getBestCmap())
    glyphs = os.path.join(fonts, "Glyphs")
    tables = {}
    for name in sorted(os.listdir(glyphs)):
        if name.endswith("_glyphs.bin"):
            language = name.split("_")[1]
            tables.setdefault(language, set()).update(table_characters(os.path.join(glyphs, name)))
    roboto = TTFont(roboto_path)
    robotoMap = set(roboto.getBestCmap())
    latin = set()
    for language, characters in tables.items():
        if language not in CJK_FACES:
            latin |= (characters - game) & robotoMap
    write_subset(roboto, latin, os.path.join(out, "Fallback.ttf"))
    for language, face in CJK_FACES.items():
        cjk = TTFont(cjk_path, fontNumber=face)
        characters = (tables[language] - game) & set(cjk.getBestCmap())
        write_subset(cjk, characters, os.path.join(out, f"Fallback-{language}.ttf"))


if __name__ == "__main__":
    main()
