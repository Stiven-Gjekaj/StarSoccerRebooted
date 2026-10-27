# Draws art/NewStarLogo.png, the logo of the title above the SOCCER letters
# of the game, in a box of 555 x 134, the size of the original. It also
# draws art/SSRRebooted.png, the word under the SOCCER letters
# (layouts/ssr_rebooted.uis).
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from artlib import *

OUT = sys.argv[1]
S = 126
y0 = 4
w = natural_width('STAR', S, track=0.02)
x0 = (555 - (S + 12 + w)) / 2
b = star_box(x0, y0, S)
tx = x0 + S + 12
b += glossy_text(text_path('STAR', tx, y0, w, S, track=0.02), y0, y0 + S, clip_id='a')
render(b, 555, 134, OUT + '/NewStarLogo.svg', OUT + '/NewStarLogo.png')
os.remove(OUT + '/NewStarLogo.svg')

# REBOOTED in the red type of the logo, with the letter height of STAR on
# the screen (the layout draws the logo at 434 of 555), at two times the
# drawn size.
h = round(2 * S * 434 / 555)
w = natural_width('REBOOTED', h, track=0.02)
pad = 8
W, H = round(w + 2 * pad), h + 2 * pad
b = glossy_text(text_path('REBOOTED', pad, pad, w, h, track=0.02), pad, pad + h, sw=5, clip_id='r')
render(b, W, H, OUT + '/SSRRebooted.svg', OUT + '/SSRRebooted.png')
os.remove(OUT + '/SSRRebooted.svg')
print('SSRRebooted', W, H)
