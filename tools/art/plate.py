# Draws the plates of the match screen. Each plate is white, so that a
# colour tints it. It has a soft shade from the top to the bottom and a
# thin dark edge, and no bright rim.
#   SSRPlate (249 x 68): the name plate of a team.
#   SSRClock (60 x 73): the face of the clock of the score bar.
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from artlib import *

OUT = sys.argv[1]


def plate(name, w, h, radius):
    body = ('<defs><linearGradient id="shade" x1="0" y1="0" x2="0" y2="1">'
            '<stop offset="0" stop-color="#ffffff"/><stop offset="0.5" stop-color="#f2f2f2"/>'
            '<stop offset="1" stop-color="#d0d0d0"/></linearGradient></defs>'
            f'<rect x="1" y="1" width="{w - 2}" height="{h - 2}" rx="{radius + 1}" fill="#000000" opacity="0.35"/>'
            f'<rect x="2.5" y="2.5" width="{w - 5}" height="{h - 5}" rx="{radius}" fill="url(#shade)"/>')
    render(body, w, h, f'{OUT}/{name}.svg', f'{OUT}/{name}.png', scale=2)
    os.remove(f'{OUT}/{name}.svg')


plate('SSRPlate', 249, 68, 8)
plate('SSRClock', 60, 73, 8)
