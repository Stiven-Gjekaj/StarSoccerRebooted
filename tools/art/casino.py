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


# SlotsTitlePanel.patch (598 x 126): the word above SLOTS. The light rays
# of the panel come from (300, 64), in the wedges of these angles from the
# vertical. The patch draws the rays again where the old word was, then
# STAR SOCCER in yellow, low enough to cover the foot of the old word.
CX, CY = 300, 64
WEDGES = [(-52.9, -30.5), (-9.5, 9.5), (30.5, 53.1)]
# The dark red of each row from 7 to 32. A ray is lighter by (16, 14, 14).
DARK = [(91, 14, 14), (92, 12, 12), (96, 14, 14), (98, 14, 14), (101, 15, 15), (103, 15, 15), (104, 15, 15),
        (105, 16, 16), (104, 16, 15), (102, 15, 15), (102, 15, 15), (104, 16, 16), (105, 15, 15), (105, 16, 15),
        (105, 15, 15), (105, 15, 15), (104, 16, 16), (102, 15, 15), (101, 15, 15), (101, 15, 15), (101, 15, 15),
        (101, 16, 15), (100, 15, 15), (101, 15, 15), (103, 15, 15), (107, 14, 14)]
X0, X1, Y0 = 224, 374, 7
body = ''
for i, (r, g, b) in enumerate(DARK):
    y = Y0 + i
    x = X0
    while x < X1:
        a = math.degrees(math.atan2(x + 0.5 - CX, CY - (y + 0.5)))
        light = any(lo <= a <= hi for lo, hi in WEDGES)
        run = x
        while run < X1:
            b2 = math.degrees(math.atan2(run + 0.5 - CX, CY - (y + 0.5)))
            if any(lo <= b2 <= hi for lo, hi in WEDGES) != light:
                break
            run += 1
        c = (r + 16, g + 14, b + 14) if light else (r, g, b)
        body += f'<rect x="{x}" y="{y}" width="{run - x}" height="1" fill="rgb{c}"/>'
        x = run
body += ('<linearGradient id="gold" x1="0" y1="0" x2="0" y2="1">'
         '<stop offset="0" stop-color="#fff35a"/><stop offset="1" stop-color="#f7b800"/></linearGradient>')
th = 19
tw = natural_width('STAR SOCCER', th, track=0.05)
q = min(1.0, 134 / tw)
d = text_path('STAR SOCCER', 0, 0, tw * q, th, track=0.05)
body += (f'<g transform="translate({300 - tw * q / 2 + 3:.1f} 15) skewX(-14)">'
         f'<path d="{d}" fill="#3a0000" stroke="#3a0000" stroke-width="3" stroke-linejoin="round" transform="translate(1.5 2)"/>'
         f'<path d="{d}" fill="url(#gold)" stroke="#5a0a00" stroke-width="1.2" stroke-linejoin="round" paint-order="stroke"/></g>')
save('SlotsTitlePanel.patch', body, 598, 126)


# CardBack.patch (77 x 110): the band across the ball of the card back. The
# patch fills the band, then puts SSR there in slanted white type.
body = ('<linearGradient id="band" x1="0" y1="0" x2="1" y2="0">'
        '<stop offset="0" stop-color="#98c0f1"/><stop offset="1" stop-color="#91bcf0"/></linearGradient>'
        '<rect x="25" y="47" width="28" height="15" fill="url(#band)"/>')
tw = natural_width('SSR', 11, track=0.06)
d = text_path('SSR', 0, 0, tw, 11, track=0.06)
body += f'<g transform="translate({39 - tw / 2 + 1:.1f} 49) skewX(-14)"><path d="{d}" fill="#ffffff"/></g>'
save('CardBack.patch', body, 77, 110)


# IBlackJackColour.patch (273 x 210): the icon of blackjack has three
# marks. Each part of the patch covers one.
body = TABLE
# The mark on the table, above the arc of the banner. Each column gets the
# felt again: a blend from the colour above the mark at the top to the
# colour above the banner at the bottom (x, the last row, top, bottom).
FELT = [(128, 170, 0x1f5546, 0x104c3c), (132, 171, 0x1f5747, 0x0f4c3c), (136, 169, 0x1f5748, 0x104e3d),
        (140, 169, 0x205949, 0x104f3d), (144, 170, 0x225a4a, 0x10503e), (148, 170, 0x225b4a, 0x10513e),
        (152, 170, 0x235c4c, 0x10523f), (156, 169, 0x235d4c, 0x105340), (160, 169, 0x245e4f, 0x105340),
        (164, 167, 0x245f4f, 0x105441), (168, 168, 0x256050, 0x105441), (172, 166, 0x256150, 0x115643),
        (176, 167, 0x256151, 0x105542), (180, 165, 0x276253, 0x115642), (184, 166, 0x276453, 0x115743),
        (188, 164, 0x276453, 0x125643), (192, 165, 0x286554, 0x115643), (196, 164, 0x286454, 0x125643),
        (200, 163, 0x296756, 0x125743), (204, 162, 0x296655, 0x125743), (208, 160, 0x2a6757, 0x135845),
        (212, 161, 0x2a6657, 0x135845), (216, 159, 0x2a6756, 0x145846), (220, 158, 0x2a6656, 0x145946),
        (224, 158, 0x2a6656, 0x145946), (228, 157, 0x2a6757, 0x155847), (232, 156, 0x2b6656, 0x185b47),
        (236, 155, 0x2b6756, 0x175b48), (240, 152, 0x2b6656, 0x175b49), (244, 151, 0x2c6757, 0x175b48),
        (248, 150, 0x2c6557, 0x185c49), (252, 148, 0x2d6656, 0x195c4a), (256, 147, 0x2e6657, 0x195c4a),
        (260, 145, 0x2e6657, 0x1a5c4b), (264, 144, 0x2f6557, 0x1a5b4b), (272, 142, 0x2f6557, 0x1a5b4b)]


def rgb(v):
    return (v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff


def blend(a, b, t):
    return tuple(int(round(p + (q - p) * t)) for p, q in zip(a, b))


for x in range(128, 271):
    k = min((x - 128) // 4, len(FELT) - 2)
    (xa, ba, ta, ua), (xb, bb, tb, ub) = FELT[k], FELT[k + 1]
    f = (x - xa) / (xb - xa)
    top = int(132 - 0.23 * (x - 133))
    last = int(round(ba + (bb - ba) * f))
    upper = blend(rgb(ta), rgb(tb), f)
    lower = blend(rgb(ua), rgb(ub), f)
    for y in range(top, last + 1):
        c = blend(upper, lower, (y - top) / max(1, last - top))
        body += f'<rect x="{x}" y="{y}" width="1" height="1" fill="rgb{c}"/>'
S = 30
th = 28
tw = natural_width('SSR', th, track=0.03)
body += '<g transform="translate(140 130) rotate(-7.4)">'
cx, cy = S / 2, S * 0.53
body += (f'<g filter="url(#sh)"><rect x="0" y="0" width="{S}" height="{S}" fill="url(#green)"/>'
         f'{bevel_star(cx, cy, S * 0.43, S * 0.175)}</g>')
d = text_path('SSR', S + 5, 1, tw, th, track=0.03)
body += f'<g filter="url(#sh)"><path d="{d}" fill="url(#silver)"/></g></g>'
# The top of the ace: NEW goes, so that the card says STAR and SOCCER
# round the spade. The card is white there.
body += '<path d="M85,41 H106 V54 L102,55 L101,59 H85 Z" fill="#ffffff"/>'
# The band of the card back: a white plate across the band, with SSR in
# the blue of the card.
body += ('<g transform="translate(64.5 93) rotate(-10.8)">'
         '<rect x="-22" y="-8" width="44" height="16" fill="#f5f8fe"/>')
tw = natural_width('SSR', 12, track=0.06)
d = text_path('SSR', -tw / 2, -6, tw, 12, track=0.06)
body += f'<path d="{d}" fill="#5493e8"/></g>'
save('IBlackJackColour.patch', body, 273, 210)
