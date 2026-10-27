# Draws the marks of the stadiums: the roof panel (BannerTopS2) and the
# boards at the front of the stands (BannerFrontAbv). Each box is the size
# of the original image.
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from artlib import *

OUT = sys.argv[1]


def save(name, body, w, h):
    render(body, w, h, f'{OUT}/{name}.svg', f'{OUT}/{name}.png')
    os.remove(f'{OUT}/{name}.svg')


def slanted(s, x, top, h, fill, track=0.04, skew=-14, extra=''):
    w = natural_width(s, h, track=track)
    d = text_path(s, 0, 0, w, h, track=track)
    return f'<g transform="translate({x:.1f} {top:.1f}) skewX({skew})"><path d="{d}" fill="{fill}" {extra}/></g>', w


# The roof panel (720 x 52): grey metal with seams, and the name in
# pressed type between two pairs of stars.
W, H = 720, 52
body = ('<defs><linearGradient id="metal" x1="0" y1="0" x2="0" y2="1">'
        '<stop offset="0" stop-color="#d4d4d4"/><stop offset="0.5" stop-color="#b8b8b8"/>'
        '<stop offset="1" stop-color="#9c9c9c"/></linearGradient></defs>'
        f'<rect x="0" y="0" width="{W}" height="{H}" fill="url(#metal)"/>'
        f'<rect x="0" y="0" width="{W}" height="3" fill="#ececec"/>'
        f'<rect x="0" y="{H - 4}" width="{W}" height="4" fill="#7a7a7a"/>')
for x in range(11, W, 22):
    body += f'<rect x="{x}" y="3" width="1.5" height="{H - 7}" fill="#8e8e8e" opacity="0.7"/>'
    body += f'<rect x="{x + 1.5}" y="3" width="1" height="{H - 7}" fill="#e6e6e6" opacity="0.6"/>'
name = 'STAR SOCCER REBOOTED'
th = 30
w = natural_width(name, th, track=0.06)
tx = (W - w) / 2 + 4
light, _ = slanted(name, tx + 1.5, 12.5, th, '#e8e8e8', track=0.06)
dark, _ = slanted(name, tx, 11, th, '#7c7c7c', track=0.06)
body += light + dark
for cx in (tx - 70, tx - 42, tx + w + 30, tx + w + 58):
    pts = ' '.join(f'{a:.1f},{b:.1f}' for a, b in star_points(cx, 27, 16, 6.5))
    pts2 = ' '.join(f'{a + 1.5:.1f},{b + 1.5:.1f}' for a, b in star_points(cx, 27, 16, 6.5))
    body += f'<polygon points="{pts2}" fill="#e8e8e8"/><polygon points="{pts}" fill="#7c7c7c"/>'
save('BannerTopS2', body, W, H)

# The board (102 x 54): white slanted SSR. The layout gives it its colour.
h = 40
w = natural_width('SSR', h, track=0.04)
t, _ = slanted('SSR', (102 - w) / 2 + 5, 7, h, '#ffffff')
save('BannerFrontAbv', t, 102, 54)
