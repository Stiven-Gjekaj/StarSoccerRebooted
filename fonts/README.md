# The fallback fonts

The font of the game has no Cyrillic, Japanese, Korean, or Chinese
characters. Android draws these characters with the font of the system
(FontNative.GetTextPaint). The remake draws them with these fonts:

- `Fallback.ttf` is Roboto, from the Android system image. The license is
  in `LICENSE-Roboto.txt` (Apache License 2.0).
- `Fallback-ja.ttf`, `Fallback-ko.ttf`, and `Fallback-ch.ttf` are the
  Japanese, the Korean, and the Simplified Chinese faces of Noto Sans CJK,
  from the Android system image. The license is in
  `LICENSE-NotoSansCJK.txt` (SIL Open Font License 1.1).

Each file keeps only the characters of the glyph tables of 4.32
(`Fonts/Glyphs` of the import) that the font of the game does not have.
`tools/fonts/fallback.py` makes the files:

    adb pull /system/fonts/RobotoStatic-Regular.ttf
    adb pull /system/fonts/NotoSansCJK-Regular.ttc
    python tools/fonts/fallback.py <import>/Fonts RobotoStatic-Regular.ttf NotoSansCJK-Regular.ttc fonts

The Noto Sans CJK of Android is a variable font with CFF2 outlines. The
script keeps its weight 400, which Android uses for normal text, and
changes the outlines to TrueType curves, because stb_truetype cannot read
CFF2. The script needs `fonttools`.
