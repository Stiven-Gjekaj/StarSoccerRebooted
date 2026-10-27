# Draws the marks of SSR on the cards of the pairs game, on the strip
# behind the scroll windows, and on the banner of the classic match.
# A ".patch" file changes only the pixels of the mark. The other pixels of
# the original image stay. Each box is the size of the original image.
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from artlib import *

OUT = sys.argv[1]


def save(name, body, w, h):
    render(body, w, h, f'{OUT}/{name}.svg', f'{OUT}/{name}.png')
    os.remove(f'{OUT}/{name}.svg')


def mix(a, b, t):
    return a + (b - a) * t


# PShirt (144 x 164): the sponsor on the chest of the shirt. The patch
# fills the box of the old sponsor with the cloth of the shirt, then puts
# the star box and SSR on it. The cloth is grey: its shade comes from the
# edges of the box, so that the folds of the shirt go on.
X0, Y0, X1, Y1 = 41, 47, 89, 81
TOP = [0xfa, 0xfb, 0xfd, 0xfe, 0xfa, 0xfa, 0xfa, 0xf8, 0xf3, 0xed, 0xed, 0xdf, 0xcb]
BOTTOM = [0xe5, 0xe4, 0xe6, 0xe5, 0xe6, 0xe0, 0xdb, 0xcd, 0xc6, 0xb3, 0xaa, 0xa6, 0x89]
LEFT = [0xfa, 0xf8, 0xf4, 0xf0, 0xe9, 0xe6, 0xe7, 0xe9, 0xe6, 0xe5, 0xe1, 0xe4]
RIGHT = [0xcb, 0xbe, 0xb7, 0xb2, 0xab, 0x9e, 0x95, 0x95, 0xa3, 0x93, 0x87, 0x85]


def edge(values, t):
    t = min(max(t, 0.0), 1.0) * (len(values) - 1)
    i = min(int(t), len(values) - 2)
    return mix(values[i], values[i + 1], t - i)


body = ''
for y in range(Y0, Y1):
    for x in range(X0, X1):
        s = (x - X0) / (X1 - X0 - 1)
        t = (y - Y0) / (Y1 - Y0 - 1)
        # The blend of the four edges (Coons).
        v = ((1 - t) * edge(TOP, s) + t * edge(BOTTOM, s) + (1 - s) * edge(LEFT, t) + s * edge(RIGHT, t)
             - ((1 - s) * (1 - t) * TOP[0] + s * (1 - t) * TOP[-1] + (1 - s) * t * BOTTOM[0] + s * t * BOTTOM[-1]))
        v = int(min(max(v, 0), 255))
        body += f'<rect x="{x}" y="{y}" width="1" height="1" fill="#{v:02x}{v:02x}{v:02x}"/>'
S = 18
w = natural_width('SSR', S, track=0.03)
x0 = (X0 + X1 - (S + 3 + w)) / 2 - 2
y0 = 53
body += f'<g opacity="0.92">{star_box(x0, y0, S)}</g>'
d = text_path('SSR', x0 + S + 3, y0, w, S, track=0.03)
body += f'<path d="{d}" fill="#3e3e3e" opacity="0.92"/>'
body += f'<path d="{text_path("STAR SOCCER", x0, y0 + S + 3, S + 3 + w, 5, track=0.06)}" fill="#d0362a" opacity="0.92"/>'
save('PShirt.patch', body, 144, 164)

# PCardBackPattern1 (144 x 164): the back of the cards. Two dark plates
# with white type at a low alpha, as in the original. The box of the image
# starts at (15, 13).
body = ''
for top in (13 + 13, 13 + 111):
    x, y, w, h = 15 + 31, top, 53, 14
    body += (f'<linearGradient id="plate{top}" x1="0" y1="0" x2="0" y2="1">'
             f'<stop offset="0" stop-color="#262626"/><stop offset="1" stop-color="#4a4a4a"/></linearGradient>')
    tw = natural_width('SSR', 11, track=0.08)
    d = text_path('SSR', x + (w - tw) / 2, y + 1.5, tw, 11, track=0.08)
    body += (f'<path d="M{x},{y}h{w}v{h}h{-w}Z {d}" fill="url(#plate{top})" fill-rule="evenodd" fill-opacity="0.31"/>'
             f'<path d="{d}" fill="#fff" fill-opacity="0.31"/>')
save('PCardBackPattern1.patch', body, 144, 164)

# LogoStrip (477 x 35): four plates at a low alpha behind the scroll
# windows. The layout gives the strip its colour.
A = 0.38
body = ''
plates = [(9, 'light'), (127, 'star'), (247, 'light'), (365, 'dark')]
for x, kind in plates:
    w, y, h = 106, 4, 30
    if kind == 'star':
        s = 26
        body += f'<rect x="{x}" y="{y + 2}" width="{s}" height="{s}" fill="#fff" fill-opacity="{A}"/>'
        pts = ' '.join(f'{a:.1f},{b:.1f}' for a, b in star_points(x + s / 2, y + 2 + s * 0.54, s * 0.42, s * 0.17))
        body += f'<polygon points="{pts}" fill="#000" fill-opacity="{A}"/>'
        tw = w - s - 4
        body += f'<path d="{text_path("SOCCER", x + s + 4, y + 2, tw, s, track=0.02)}" fill="#fff" fill-opacity="{A}"/>'
        continue
    tw = natural_width('SSR', 22, track=0.04)
    d = text_path('SSR', x + (w - tw) / 2, y + 4, tw, 22, track=0.04)
    plate = f'M{x + 6},{y}h{w - 12}a6,6 0 0 1 6,6v{h - 12}a6,6 0 0 1 -6,6h{-(w - 12)}a6,6 0 0 1 -6,-6v{-(h - 12)}a6,6 0 0 1 6,-6Z'
    back, front = ('#fff', '#000') if kind == 'light' else ('#000', '#fff')
    body += (f'<path d="{plate} {d}" fill="{back}" fill-rule="evenodd" fill-opacity="{A}"/>'
             f'<path d="{d}" fill="{front}" fill-opacity="{A}"/>')
save('LogoStrip', body, 477, 35)

# CLNewStar (615 x 62): the banner of the classic match. Grey slanted type
# on grey, with a ball at the end.
W, H = 615, 62
body = ('<linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">'
        '<stop offset="0" stop-color="#989898"/><stop offset="1" stop-color="#6a6a6a"/></linearGradient>'
        '<linearGradient id="type" x1="0" y1="0" x2="0" y2="1">'
        '<stop offset="0" stop-color="#c2c2c2"/><stop offset="1" stop-color="#a6a6a6"/></linearGradient>'
        f'<rect width="{W}" height="{H}" fill="url(#bg)"/>')
name = 'STAR SOCCER REBOOTED'
th = 44
w = natural_width(name, th, track=0.04)
q = min(1.0, 540 / w)
d = text_path(name, 0, 0, w * q, th, track=0.04)
body += (f'<g transform="translate(10 11) skewX(-14)"><path d="{d}" fill="#5e5e5e" transform="translate(2 2)"/>'
         f'<path d="{d}" fill="url(#type)"/></g>')
cx, cy, r = 588, 31, 24
body += (f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="url(#type)" stroke="#5e5e5e" stroke-width="2"/>'
         f'<polygon points="{" ".join(f"{cx + 9 * math.cos(a):.1f},{cy + 9 * math.sin(a):.1f}" for a in [-math.pi / 2 + k * 2 * math.pi / 5 for k in range(5)])}" fill="#6a6a6a"/>')
for k in range(5):
    a = -math.pi / 2 + k * 2 * math.pi / 5
    body += (f'<line x1="{cx + 9 * math.cos(a):.1f}" y1="{cy + 9 * math.sin(a):.1f}" '
             f'x2="{cx + r * math.cos(a):.1f}" y2="{cy + r * math.sin(a):.1f}" stroke="#6a6a6a" stroke-width="2"/>')
save('CLNewStar', body, W, H)
