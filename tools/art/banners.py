# Draws the marks of SSR on the scrolling boards of the pitch
# (Stadium_Banners.uis). Each box is the size of the original image. A
# ".patch" file changes only the pixels of the mark.
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from artlib import *

OUT = sys.argv[1]


def save(name, body, w, h):
    render(body, w, h, f'{OUT}/{name}.svg', f'{OUT}/{name}.png')
    os.remove(f'{OUT}/{name}.svg')


def rgb(v):
    return (v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff


def blend(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


# Banner01Text (250 x 55): the sponsor board. The star box and SSR, with a
# dark star at each end.
S = 40
tw = natural_width('SSR', S, track=0.03)
x0 = (250 - (S + 8 + tw)) / 2
body = ''
for cx in (12, 238):
    body += f'<path d="M{" L".join(f"{x:.1f},{y:.1f}" for x, y in star_points(cx, 28, 10, 4))}Z" fill="#2a2a2a" opacity="0.8"/>'
body += star_box(x0, 7, S)
d = text_path('SSR', x0 + S + 8, 7, tw, S, track=0.03)
body += glossy_text(d, 7, 7 + S, clip_id='g')
save('Banner01Text', body, 250, 55)

# Banner02Text (250 x 55): the name of the game. STAR SOCCER in grey and
# REBOOTED in white, narrow to fit the board.
th = 30
w1 = natural_width('STAR SOCCER', th, track=0.02)
w2 = natural_width('REBOOTED', th, track=0.02)
q = min(1.0, 236 / (w1 + w2 + 7))
x = (250 - (w1 + w2 + 7) * q) / 2
d1 = text_path('STAR SOCCER', x, 12, w1 * q, th, track=0.02)
d2 = text_path('REBOOTED', x + (w1 + 7) * q, 12, w2 * q, th, track=0.02)
body = (f'<g filter="url(#sh)"><path d="{d1}" fill="#8c8c8c"/>'
        f'<path d="{d2}" fill="url(#white)"/></g>')
save('Banner02Text', body, 250, 55)

# Banner04.patch (250 x 55): the boots board. The hashtag says SSR, the
# small logo of the brand is an SSR plate, and the mark on the side of the
# boot goes.
body = ''
# The purple stripes behind the hashtag go across at a slant. Each pixel
# of the hashtag gets the colour of its stripe again: the light stripe on
# the left, the middle stripe, then the dark stripe, each with the shade
# of its column.
LIGHT = 0xa254e4
MID = ((110, 0x5b18b1), (134, 0x6c1ed3))
DARK = ((122, 0x420aad), (155, 0x530dda))


def stripe(x, y):
    edge1 = 108.5 - (y - 34) * 0.31
    edge2 = 135.5 - (y - 33) * 0.88
    if x < edge1:
        return rgb(LIGHT)
    (xa, ca), (xb, cb) = MID if x < edge2 else DARK
    t = max(0.0, min(1.0, (x - xa) / (xb - xa)))
    return blend(rgb(ca), rgb(cb), t)


for y in range(35, 50):
    for x in range(95, 155):
        body += f'<rect x="{x}" y="{y}" width="1" height="1" fill="rgb{stripe(x, y)}"/>'
th = 11
tw = natural_width('#SSR-BOOTS', th, track=0.05)
d = text_path('#SSR-BOOTS', 0, 0, tw, th, track=0.05)
body += (f'<g transform="translate({125 - tw / 2:.1f} 37) skewX(-12)">'
         f'<path d="{d}" fill="#2a0870" transform="translate(1 1)"/><path d="{d}" fill="#ffffff"/></g>')
# The plate of the brand: blue, a white edge, SSR, and a white star.
body += ('<rect x="185" y="30" width="53" height="19" rx="2" fill="#2f84d8" stroke="#ffffff" stroke-width="1.2"/>'
         '<rect x="186" y="31" width="51" height="8" fill="#ffffff" opacity="0.18"/>')
tw = natural_width('SSR', 13, track=0.05)
d = text_path('SSR', 190, 33, tw, 13, track=0.05)
body += f'<path d="{d}" fill="#ffffff"/>'
body += f'<path d="M{" L".join(f"{x:.1f},{y:.1f}" for x, y in star_points(228, 39.5, 7, 2.8))}Z" fill="#ffffff"/>'
# The side of the boot: the purple of the boot, from the top to the
# bottom of the mark (x, top, bottom).
BOOT = [(48, 0x2c06a2, 0x3d16bc), (49, 0x3611ac, 0x3200be), (50, 0x2a00ac, 0x4002c5), (51, 0x2a00b7, 0x4102c7),
        (52, 0x3e00c7, 0x4000c5), (53, 0x4904d4, 0x4303c9), (54, 0x4903d4, 0x4301d0), (55, 0x4903d1, 0x4503cd),
        (56, 0x4803d0, 0x4302cd), (57, 0x4902d3, 0x4000ce), (58, 0x4d05d7, 0x4300cd), (59, 0x5108dd, 0x4603ca),
        (60, 0x580be6, 0x4903d0)]
for x, top, bottom in BOOT:
    for y in range(14, 22):
        c = blend(rgb(top), rgb(bottom), (y - 13) / 9)
        body += f'<rect x="{x}" y="{y}" width="1" height="1" fill="rgb{c}"/>'
save('Banner04.patch', body, 250, 55)
