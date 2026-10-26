# Changelog

## 0.2.0 (2026-10-27)

- The app draws the main menu of the game from the layout files and the
  image atlases of your copy. It reads the layout language of the game:
  imports, macros, constants, conditions, origins, layers, clips, and the
  arithmetic of the layouts.
- The image atlases of the game work: the key files, the RLE pages, and
  the plain, sliced, and stripped images.
- Text uses the TTF font of the game. A text box wraps and fits its text.
  The text of the English language file fills the tags.
- The app reads the 2025 data of the game: 6138 clubs in 211 nations, and
  1060 competitions.
- The buttons of a screen play their sounds. The app decodes the OGG
  sounds of an APK and the MP3 sounds of an IPA.

## 0.1.0 (2026-10-26)

- The app imports the assets from your copy of New Star Soccer: an APK, an
  XAPK, or an IPA. Drop the file on the window, or give its path to the
  app.
- The app decodes the PNG and JPG images of the game and shows a title
  screen in portrait, as the game does.
- The macOS app holds SDL3, so it needs nothing more.
