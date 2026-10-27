# Draws the name plate of a team on the match screen (SSRPlate). The plate
# is white, so that the colour of the team tints it. It has a soft shade
# from the top to the bottom and a thin dark edge, and no bright rim.
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from artlib import *

OUT = sys.argv[1]
W, H = 249, 68
body = ('<defs><linearGradient id="shade" x1="0" y1="0" x2="0" y2="1">'
        '<stop offset="0" stop-color="#ffffff"/><stop offset="0.5" stop-color="#f2f2f2"/>'
        '<stop offset="1" stop-color="#d0d0d0"/></linearGradient></defs>'
        f'<rect x="1" y="1" width="{W - 2}" height="{H - 2}" rx="9" fill="#000000" opacity="0.35"/>'
        f'<rect x="2.5" y="2.5" width="{W - 5}" height="{H - 5}" rx="8" fill="url(#shade)"/>')
render(body, W, H, f'{OUT}/SSRPlate.svg', f'{OUT}/SSRPlate.png', scale=2)
os.remove(f'{OUT}/SSRPlate.svg')
