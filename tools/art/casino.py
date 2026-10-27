# Draws the marks of SSR in the casino and on the race track. Each box is
# the size of the original image. A ".patch" file changes only the pixels
# of the mark.
import math
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from artlib import *

OUT = sys.argv[1]


def save(name, body, w, h):
    render(body, w, h, f'{OUT}/{name}.svg', f'{OUT}/{name}.png')
    os.remove(f'{OUT}/{name}.svg')


def board(name, w, rows, words, th, top, fill, shadow):
    """A grey board with slanted type. rows gives the grey of each row, from
    the top. The type has the height th, and its top is at top."""
    body = ''
    for y, v in enumerate(rows):
        body += f'<rect x="0" y="{y}" width="{w}" height="1" fill="#{v:02x}{v:02x}{v:02x}"/>'
    tw = natural_width(words, th, track=0.04)
    tw = min(tw, w - 30)
    x = (w - tw) / 2
    d = text_path(words, 0, 0, tw, th, track=0.04)
    body += (f'<g transform="translate({x:.1f} {top}) skewX(-14)">'
             f'<path d="{d}" fill="{shadow}" transform="translate(1 1)"/>'
             f'<path d="{d}" fill="{fill}"/></g>')
    save(name, body, w, len(rows))


def board_patch(name, w, h, x0, x1, y0, rows, words, th, top, fill, shadow):
    """The middle of a board with slanted ends: the rows from y0 get their
    grey again from x0 to x1, and the slanted type goes on them."""
    body = ''
    for i, v in enumerate(rows):
        body += f'<rect x="{x0}" y="{y0 + i}" width="{x1 - x0}" height="1" fill="#{v:02x}{v:02x}{v:02x}"/>'
    tw = min(natural_width(words, th, track=0.04), x1 - x0 - 16)
    d = text_path(words, 0, 0, tw, th, track=0.04)
    body += (f'<g transform="translate({(x0 + x1 - tw) / 2 + 3:.1f} {top}) skewX(-14)">'
             f'<path d="{d}" fill="{shadow}" transform="translate(1.5 1.5)"/>'
             f'<path d="{d}" fill="{fill}"/></g>')
    save(name, body, w, h)


# HorseBannerC.patch (256 x 30) and HorseBannerW.patch (259 x 30): the
# boards along the track of the race.
board_patch('HorseBannerC.patch', 256, 30, 24, 234, 2,
            [0xb4, 0xaf, 0xad, 0xac, 0xab, 0xaa, 0xa9, 0xa5, 0xa4, 0xa2, 0xa2, 0x9e, 0x9b, 0x97, 0x96, 0x92,
             0x8f, 0x8c, 0x8a, 0x88, 0x85, 0x82, 0x81, 0x7f, 0x7f],
            'STAR SOCCER REBOOTED', 20, 4, '#c4c4c4', '#7a7a7a')
board_patch('HorseBannerW.patch', 259, 30, 36, 234, 2,
            [0xcd, 0xc4, 0xc4, 0xc5, 0xc5, 0xc4, 0xc4, 0xc1, 0xbe, 0xbd, 0xbb, 0xb7, 0xb5, 0xb3, 0xb0, 0xae,
             0xab, 0xaa, 0xa9, 0xa5, 0xa3, 0xa1, 0xa0, 0x9e, 0x9f],
            'SSR LEAGUES', 20, 4, '#6e6e6e', '#c8c8c8')


# AdBoardColour (180 x 22): white type on a grey that gets dark to the
# bottom. The layout gives the board its colour.
board('AdBoardColour', 180,
      [0xff, 0xd1, 0xeb, 0xe8, 0xe8, 0xe7, 0xe3, 0xe1, 0xde, 0xd7, 0xd3, 0xcf, 0xc7, 0xc2, 0xbc, 0xb2,
       0xab, 0xa2, 0x9d, 0x9b, 0x6f, 0x39],
      'STAR SOCCER REBOOTED', 14, 3, '#ffffff', '#8a8a8a')

# AdBoardWhite (180 x 20): grey type on a light grey.
board('AdBoardWhite', 180,
      [0xd8, 0xd4, 0xd4, 0xd4, 0xd3, 0xd1, 0xce, 0xcb, 0xc7, 0xc6, 0xc3, 0xc1, 0xbe, 0xbc, 0xb8, 0xb7,
       0xb5, 0xba, 0x80, 0x1b],
      'SSR LEAGUES', 14, 2, '#868686', '#e0e0e0')


# The green of the star box and the silver of the type on the table.
TABLE = ('<linearGradient id="green" x1="0" y1="0" x2="0" y2="1">'
         '<stop offset="0" stop-color="#2a8a6c"/><stop offset="1" stop-color="#14533f"/></linearGradient>'
         '<linearGradient id="silver" x1="0" y1="0" x2="0" y2="1">'
         '<stop offset="0" stop-color="#ffffff"/><stop offset="0.6" stop-color="#f2f2f2"/>'
         '<stop offset="1" stop-color="#b8b8b8"/></linearGradient>')

# BlackJackLogo.patch (481 x 123): the mark above the BLACKJACK banner. The
# patch clears the old mark with pixels of alpha 1/255, then puts a green
# star box and SSR there, a little turned with the arc of the banner.
X0, X1 = 136, 338
# The top of the banner under the mark: (x, the first row of the banner).
EDGE = [(136, 47), (146, 48), (156, 50), (166, 52), (176, 53), (186, 54), (196, 55), (206, 56), (226, 57),
        (246, 57), (276, 56), (286, 55), (296, 54), (306, 53), (316, 51), (326, 49), (338, 48)]
clear = f'M{X0},0 ' + ' '.join(f'L{x},{y}' for x, y in EDGE) + f' L{X1},0Z'
body = f'<path d="{clear}" fill="#000" fill-opacity="0.004"/>' + TABLE
S = 38
th = 36
tw = natural_width('SSR', th, track=0.03)
x0 = (X0 + X1 - (S + 6 + tw)) / 2
body += f'<g transform="rotate(-3 {(X0 + X1) / 2} 24)">'
cx, cy = x0 + S / 2, 5 + S * 0.53
body += (f'<g filter="url(#sh)"><rect x="{x0}" y="5" width="{S}" height="{S}" fill="url(#green)"/>'
         f'{bevel_star(cx, cy, S * 0.43, S * 0.175)}</g>')
d = text_path('SSR', x0 + S + 6, 7, tw, th, track=0.03)
body += f'<g filter="url(#sh)"><path d="{d}" fill="url(#silver)"/></g></g>'
save('BlackJackLogo.patch', body, 481, 123)
