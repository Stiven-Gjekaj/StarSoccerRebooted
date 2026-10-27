# Layouts

This folder holds the screen layouts of Star Soccer Rebooted. They use the
layout language of New Star Soccer. docs/formats.md describes it.

- The app looks for a layout here first, then in the import of the player.
  A layout here with the name of a layout of the game takes its place.
- A layout names the images of the atlases and imports the macros of the
  game, for example `Import:(ButtonMacros.uis)`. The import of the player
  gives them when the app runs.
- A file `<layout>.patch` removes lines of a layout of the game. Each line
  that starts with `-` gives the start of the lines to remove, for example
  `- Text:(TM,`. The other lines are comments.
- Do not copy text from the layout files of the game into this folder.
