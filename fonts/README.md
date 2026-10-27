# The fallback fonts

The font of the game has no Cyrillic, Japanese, Korean, or Chinese
characters. Android draws these characters with the font of the system
(FontNative.GetTextPaint). The remake draws them with these fonts:

- `Fallback.ttf` is Roboto, from the Android system image. The license is
  in `LICENSE-Roboto.txt` (Apache License 2.0).
- `Fallback-ja.otf`, `Fallback-ko.otf`, and `Fallback-ch.otf` are the
  Japanese, the Korean, and the Simplified Chinese faces of Noto Sans CJK,
  from the Android system image. The license is in
  `LICENSE-NotoSansCJK.txt` (SIL Open Font License 1.1).

Each file keeps only the characters of the glyph tables of 4.32
(`Fonts/Glyphs` of the import) that the font of the game does not have.
`tools/fonts/fallback.py` makes the files:

    adb pull /system/fonts/RobotoStatic-Regular.ttf
    adb pull /system/fonts/NotoSansCJK-Regular.ttc
    python tools/fonts/fallback.py <import>/Fonts RobotoStatic-Regular.ttf NotoSansCJK-Regular.ttc fonts

The script needs `fonttools`.
