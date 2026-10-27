# Helpers for the art of the remake: text as paths, the star box.
import math, subprocess
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.boundsPen import BoundsPen

IMPACT = '/System/Library/Fonts/Supplemental/Impact.ttf'
_fonts = {}

def _font(path):
    if path not in _fonts: _fonts[path] = TTFont(path)
    return _fonts[path]

def text_path(s, x, top, width, height, font=IMPACT, track=0.0):
    """Gives the text as one SVG path. The ink of the text fills the box."""
    f = _font(font); gs = f.getGlyphSet(); cmap = f.getBestCmap()
    names = [cmap[ord(c)] for c in s]
    # The bounds are in the units of the font.
    adv = 0; xs = []; minx = miny = 1e9; maxx = maxy = -1e9
    upm = f['head'].unitsPerEm
    for n in names:
        bp = BoundsPen(gs); gs[n].draw(bp)
        if bp.bounds:
            a, b, c, d = bp.bounds
            minx = min(minx, adv + a); maxx = max(maxx, adv + c); miny = min(miny, b); maxy = max(maxy, d)
        xs.append(adv); adv += gs[n].width + track * upm
    sx = width / (maxx - minx); sy = height / (maxy - miny)
    pen = SVGPathPen(gs)
    for n, ox in zip(names, xs):
        t = TransformPen(pen, (sx, 0, 0, -sy, x + (ox - minx) * sx, top + maxy * sy))
        gs[n].draw(t)
    return pen.getCommands()

def star_points(cx, cy, R, r, rot=0):
    pts = []
    for i in range(10):
        a = -math.pi / 2 + rot + i * math.pi / 5
        rr = R if i % 2 == 0 else r
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    return pts

def bevel_star(cx, cy, R, r, light='#ffffff', dark='#cfcfcf', rot=0):
    """Gives a star. Each point has a light side and a dark side."""
    p = star_points(cx, cy, R, r, rot); out = ''
    for k in range(5):
        tip = p[2 * k]; prev = p[(2 * k - 1) % 10]; nxt = p[2 * k + 1]
        out += f'<path d="M{cx:.2f},{cy:.2f}L{prev[0]:.2f},{prev[1]:.2f}L{tip[0]:.2f},{tip[1]:.2f}Z" fill="{dark if k in (2,3) else light}"/>'
        out += f'<path d="M{cx:.2f},{cy:.2f}L{tip[0]:.2f},{tip[1]:.2f}L{nxt[0]:.2f},{nxt[1]:.2f}Z" fill="{light if k in (4,) else dark}"/>'
    return out

DEFS = '''<linearGradient id="red" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ff7656"/><stop offset="0.45" stop-color="#f4412b"/><stop offset="1" stop-color="#cf1a12"/></linearGradient>
<linearGradient id="white" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffffff"/><stop offset="1" stop-color="#dedede"/></linearGradient>
<linearGradient id="gloss" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity="0.35"/><stop offset="1" stop-color="#fff" stop-opacity="0.05"/></linearGradient>
<filter id="sh" x="-10%" y="-10%" width="130%" height="140%"><feDropShadow dx="1.5" dy="2.5" stdDeviation="1.5" flood-color="#3a0000" flood-opacity="0.45"/></filter>'''

def star_box(x, y, s):
    cx, cy = x + s / 2, y + s * 0.53
    return (f'<g filter="url(#sh)"><rect x="{x}" y="{y}" width="{s}" height="{s}" fill="url(#red)"/>'
            f'<rect x="{x+1}" y="{y+1}" width="{s-2}" height="{s-2}" fill="none" stroke="#ff9a80" stroke-width="2" opacity="0.7"/>'
            f'<g filter="url(#sh)">{bevel_star(cx, cy, s*0.43, s*0.175)}</g>'
            f'<path d="M{x},{y}h{s}v{s*0.47}C{x+s*0.66},{y+s*0.40} {x+s*0.33},{y+s*0.56} {x},{y+s*0.5}Z" fill="#fff" opacity="0.16"/></g>')

def glossy_text(d, top, bottom, fill='url(#red)', stroke='#ffffff', sw=2.5, clip_id='c'):
    mid = top + (bottom - top) * 0.48
    return (f'<g filter="url(#sh)"><path d="{d}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}" stroke-linejoin="round" paint-order="stroke"/></g>'
            f'<clipPath id="{clip_id}"><rect x="0" y="0" width="4000" height="{mid:.1f}"/></clipPath>'
            f'<path d="{d}" fill="#fff" opacity="0.14" clip-path="url(#{clip_id})"/>')

def render(svg_body, w, h, out_svg, out_png, scale=1):
    svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}"><defs>{DEFS}</defs>{svg_body}</svg>'
    open(out_svg, 'w').write(svg)
    subprocess.run(['rsvg-convert', '-z', str(scale), '-o', out_png, out_svg], check=True)

# The width of the text at the height, with no stretch.
def natural_width(s, height, font=IMPACT, track=0.0):
    f = _font(font); gs = f.getGlyphSet(); cmap = f.getBestCmap(); upm = f['head'].unitsPerEm
    adv = 0; minx = miny = 1e9; maxx = maxy = -1e9
    for c in s:
        n = cmap[ord(c)]; bp = BoundsPen(gs); gs[n].draw(bp)
        if bp.bounds:
            a, b, cc, d = bp.bounds
            minx = min(minx, adv + a); maxx = max(maxx, adv + cc); miny = min(miny, b); maxy = max(maxy, d)
        adv += gs[n].width + track * upm
    return (maxx - minx) * height / (maxy - miny)
