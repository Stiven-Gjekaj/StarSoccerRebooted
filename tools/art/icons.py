# Draws the icons of the remake for the buttons. Each icon is white with a
# soft shadow, as the icons of the game.
#   SSRShirt (96 x 96): the kit of a club (the editor on the squad screen).
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from artlib import *

OUT = sys.argv[1]

shirt = ('M30,10 L40,10 Q48,20 56,10 L66,10 L90,30 L78,46 L70,40 L70,88 L26,88 L26,40 '
         'L18,46 L6,30 Z')
body = (f'<path d="{shirt}" fill="#000000" opacity="0.35" transform="translate(3 4)"/>'
        f'<path d="{shirt}" fill="#ffffff"/>')
render(body, 96, 96, f'{OUT}/SSRShirt.svg', f'{OUT}/SSRShirt.png', scale=2)
os.remove(f'{OUT}/SSRShirt.svg')
