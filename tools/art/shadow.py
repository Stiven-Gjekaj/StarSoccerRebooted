# Draws the shadow of the ball of a chance. The import has no BallShadow
# image, so the remake draws its own: a soft black ellipse.
#   SSRBallShadow (40 x 28): the shadow on the pitch under the ball.
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from artlib import *

OUT = sys.argv[1]

body = ('<defs><radialGradient id="soft" cx="0.5" cy="0.5" r="0.5">'
        '<stop offset="0" stop-color="#000000" stop-opacity="0.6"/>'
        '<stop offset="0.6" stop-color="#000000" stop-opacity="0.35"/>'
        '<stop offset="1" stop-color="#000000" stop-opacity="0"/></radialGradient></defs>'
        '<ellipse cx="20" cy="14" rx="20" ry="14" fill="url(#soft)"/>')
render(body, 40, 28, f'{OUT}/SSRBallShadow.svg', f'{OUT}/SSRBallShadow.png', scale=2)
os.remove(f'{OUT}/SSRBallShadow.svg')
