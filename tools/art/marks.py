# Draws the marks of SSR that take the place of the marks of New Star
# Games: the TV of the match, the sponsor of the match, and the newspaper.
# Each box is the size of the original image.
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from artlib import *

OUT = sys.argv[1]


def save(name, body, w, h):
    render(body, w, h, f'{OUT}/{name}.svg', f'{OUT}/{name}.png')
    os.remove(f'{OUT}/{name}.svg')


def text(s, x, top, h, track=0.03, squeeze=1.0):
    w = natural_width(s, h, track=track) * squeeze
    return text_path(s, x, top, w, h, track=track), w


# MapNSGTVLogo (144 x 43): white type on the TV.
d, w = text('SSR TV', 0, 0, 34, track=0.04)
x = (144 - w) / 2
d, w = text('SSR TV', x, 4, 34, track=0.04)
save('MapNSGTVLogo', f'<g filter="url(#sh)"><path d="{d}" fill="url(#white)"/></g>', 144, 43)

# NSGamesLogo (400 x 93): the sponsor of the match.
S = 85
_, w = text('SSR', 0, 0, S, track=0.03)
x0 = (400 - (S + 10 + w)) / 2
body = star_box(x0, 4, S)
d, w = text('SSR', x0 + S + 10, 4, S, track=0.03)
body += glossy_text(d, 4, 4 + S, fill='url(#white)', stroke='#b01510', sw=2.5, clip_id='g')
save('NSGamesLogo', body, 400, 93)

# NSNewsLogo (354 x 93): the newspaper. SSR in red, NEWS in white.
S = 85
_, w1 = text('SSR', 0, 0, S)
_, w2 = text('NEWS', 0, 0, S)
# The type gets narrow to fit the box.
q = (354 - 8 - (S + 8 + 6)) / (w1 + w2)
x0 = 4
body = star_box(x0, 4, S)
d, w1 = text('SSR', x0 + S + 8, 4, S, squeeze=q)
body += glossy_text(d, 4, 4 + S, clip_id='n1')
d, w2 = text('NEWS', x0 + S + 8 + w1 + 6, 4, S, squeeze=q)
body += glossy_text(d, 4, 4 + S, fill='url(#white)', stroke='#9a9a9a', sw=1.5, clip_id='n2')
save('NSNewsLogo', body, 354, 93)

# CLHeaderNewsLogo (256 x 64): the sport page of the classic newspaper. A
# blue part with "ssr", and a red part with "SPORT" in slanted type.
body = ('<defs><linearGradient id="blue" x1="0" y1="0" x2="0" y2="1">'
        '<stop offset="0" stop-color="#4a6fe0"/><stop offset="1" stop-color="#1b2f9a"/></linearGradient></defs>'
        '<rect x="2" y="6" width="252" height="52" rx="8" fill="url(#red)" stroke="#fff" stroke-width="3"/>'
        '<path d="M5,9 h82 l-14,46 h-62 a5,5 0 0 1 -6,-5 v-36 a5,5 0 0 1 5,-5Z" fill="url(#blue)"/>'
        '<rect x="4" y="8" width="248" height="22" rx="6" fill="#fff" opacity="0.18"/>')
d, w = text('ssr', 0, 0, 26, track=0.02)
body += f'<g transform="translate(14 20) skewX(-12)"><path d="{text_path("ssr", 0, 0, w, 26, track=0.02)}" fill="#fff"/></g>'
_, w = text('SPORT', 0, 0, 38, track=0.03)
body += (f'<g transform="translate({90 + (160 - w) / 2:.1f} 13) skewX(-14)" filter="url(#sh)">'
         f'<path d="{text_path("SPORT", 0, 0, w, 38, track=0.03)}" fill="#fff"/></g>')
save('CLHeaderNewsLogo', body, 256, 64)

# WebAddressNS (224 x 13): the address bar of the news page. Dark grey
# bold type on white. The address has no real domain.
ARIAL_BOLD = '/System/Library/Fonts/Supplemental/Arial Bold.ttf'
ADDRESS = 'http://news.ssr'
w = natural_width(ADDRESS, 10, font=ARIAL_BOLD, track=0.02)
d = text_path(ADDRESS, (224 - w) / 2, 1.5, w, 10, font=ARIAL_BOLD, track=0.02)
save('WebAddressNS', f'<rect width="224" height="13" fill="#ffffff"/><path d="{d}" fill="#5a5a5a"/>', 224, 13)

# CLNSS (82 x 34): the block of the classic stadium. Light grey slanted
# type on grey.
ARIAL_BOLD_ITALIC = '/System/Library/Fonts/Supplemental/Arial Bold Italic.ttf'
w = natural_width('SSR', 26, font=ARIAL_BOLD_ITALIC, track=0.08)
d = text_path('SSR', (82 - w) / 2, 4, w, 26, font=ARIAL_BOLD_ITALIC, track=0.08)
body = ('<linearGradient id="block" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#979797"/>'
        '<stop offset="1" stop-color="#828282"/></linearGradient>'
        f'<rect width="82" height="34" fill="url(#block)"/><path d="{d}" fill="#c1c1c1"/>')
save('CLNSS', body, 82, 34)
