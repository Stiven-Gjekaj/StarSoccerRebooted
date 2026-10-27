# Draws art/NewStarLogo.png: the logo of the title, above the SOCCER
# letters of the game. The box is 555 x 134, the size of the original.
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from artlib import *

OUT = sys.argv[1]
S = 126
y0 = 4
w = natural_width('STAR', S, track=0.02)
x0 = 40
b = star_box(x0, y0, S)
tx = x0 + S + 12
b += glossy_text(text_path('STAR', tx, y0, w, S, track=0.02), y0, y0 + S, clip_id='a')
# The REBOOTED sticker is on the right end. The layout clips the bottom of
# the logo, and the SOCCER letters cover it, so the sticker stays high.
sw, sh = 190, 44
sx, sy = 555 - sw - 8, 58
hw = sw - 16
b += (f'<g transform="rotate(-8 {sx + sw/2:.1f} {sy + sh/2:.1f})" filter="url(#sh)">'
      f'<rect x="{sx}" y="{sy}" width="{sw}" height="{sh}" rx="4" fill="url(#white)" stroke="#c41810" stroke-width="3"/>'
      f'<path d="{text_path("REBOOTED", sx + 8, sy + 9, hw, sh - 18, track=0.05)}" fill="#d42116"/></g>')
render(b, 555, 134, OUT + '/NewStarLogo.svg', OUT + '/NewStarLogo.png')
os.remove(OUT + '/NewStarLogo.svg')
