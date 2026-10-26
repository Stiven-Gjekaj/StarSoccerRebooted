# The file formats of New Star Soccer

This file describes the files that the app reads from the import. The
reference is the Android release 4.32. The import keeps the tree under
`assets/monkey/` of the APK.

## Data files (`MyData/`)

- Each file is text with tabs between the columns. The first row holds the
  names of the columns. A file can start with a UTF-8 byte order mark. The
  lines end with CRLF.
- `MyData/<set>/` holds one data set: `Continents.txt`, `Nations.txt`,
  `Clubs.txt`, `Competitions.txt`, and `PromotionPlaces.txt`. The newest set
  is `v2025b`. The game picks it in `TPlayer`.
- A club has a `leagueid`, a `nationid`, a `strength`, the place of its
  stadium, and the colours of its kits as `#RGB` or `#RRGGBB`.
- `MyData/Languages/<code>.txt` holds the text of one language. Each row
  has a tag, the text, and the text for a female player. A text can hold
  values such as `$teamname`.

## Fonts (`Fonts/`)

- `Fonts.txt` lists the font sets. The 2x screen uses the `#ANDROID` set
  `new`: `Veriox-Rg.ttf` at 31, 44, 60, and 18 pixels.
- An `Inject:` line puts an image in place of a character, for example the
  star (U+2605) from `newstarbuck48.png`.

## Image atlases (`NewGUI/Atlases/`)

The 2x screen loads `Key.key` and then `CharactersHiRes.key`. The pages of
both files share one list.

### The key files

All values are little-endian. An int or a float starts at a multiple of 4
from the start of the file, and a short at a multiple of 2. A string is a
signed length byte and its bytes. The next value after a string starts at a
multiple of 4.

```
int pageCount
each page:
    string name          // the file is name + ".png" or name + "-quant.bin"
    int width, height
    int imageCount
    each image:
        int flags
        string name
        flags & 2: sliced    flags & 8: stripped
        flags & 4: packed    else: plain
then the cel chains: string base, then strings up to an empty one, up to an
empty base.
```

- An unpack record is `short sourcePage, sourceFrame, x, y` and
  `int keepAlpha`. A plain or sliced image with `flags & 1` has one before
  its sizes.
- Plain: `short originalWidth, originalHeight, trimWidth, trimHeight`, then
  8 floats `x0, y0, u0, v0, x1, y1, u1, v1`. x and y are parts of the drawn
  size. u and v are parts of the page size, from the top left.
- Sliced: the 4 sizes, `short fixedWidth, fixedHeight`, then quads of 12
  floats up to a float of -1. A corner is at `o + s * (drawn - fixed)`.
- Stripped: with `flags & 1`, `short count, sourcePage`, then `keepAlpha` as
  a short when count is odd and as an int when it is even, then count times
  `short sourceFrame, x, y, width, height`. Then `short originalWidth,
  originalHeight` and triangles of 12 floats up to a float of -1.
- Packed: the 4 sizes only. A packed image is a frame of a RLE page.

### The RLE pages (`-quant.bin`)

```
int paletteSize
paletteSize colours of 4 bytes: red, green, blue, alpha
int frameCount
int frameStart[frameCount + 1]   // in 32-bit words
u32 data[]
```

A byte under `paletteSize` is a colour. It goes `count` times, and then
count is 1 again. A byte above `paletteSize` sets count to the difference.
A byte equal to `paletteSize` ends the row. Each row ends with it.

A PNG page gets each unpack frame at its `x, y`. The edge pixels of the
frame also go one pixel out.

## Screen layouts (`NewGUI/Screens/`)

- `.uis` and `.uih` files hold the screens. A line `Name:(a,b) ref:R` is an
  element. A line `Name(a,b)` is a modifier of the element before it.
- `{ }` holds the children of an element, and `< >` holds the children of
  a modifier.
- `Import:`, `Macro:` with `EndM`, `Const:`, `Define:`, and `If:` with
  `Else` and `EndIf` work when the file is read. A `?NAME?` takes the value
  of a macro parameter or a constant.
- An argument can be arithmetic: `[@B-960]*0.7+744`, `Max[a:b]`, and
  `twk(Group,Name)`. `@L`, `@T`, `@W`, `@H`, and `@B` are the edges and the
  size of the screen.
- `$HIDE` after an element hides it at the start.
