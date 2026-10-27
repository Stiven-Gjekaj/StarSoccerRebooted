# Draws a simple crest for each club of a data pack. The crest is not the
# real crest of the club. It uses the colours of the club, one pattern, and
# the initials of the club.
#
#     python3 tools/art/crests.py community/season-2025-26
#
# The script reads world/Clubs.txt of the pack. A colour that the pack does
# not give comes from Clubs.txt of the data set of the game. The script
# writes crests/<club id>.png into the pack, 256 by 256 pixels.
import csv, hashlib, os, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from artlib import text_path, natural_width, render

SIZE = 256
DATASET = os.path.expanduser(
    '~/Library/Application Support/StivenGjekaj/StarSoccerRebooted/game/MyData/v2025b')
COLUMNS = ('shortname', 'homekitshirtcol1', 'homekitshirtcol2', 'homekitshortscol', 'awaykitshirtcol1')


def read_clubs(path):
    rows = {}
    if not os.path.exists(path):
        return rows
    with open(path, encoding='utf-8', newline='') as f:
        for row in csv.DictReader(f, delimiter='\t'):
            cid = (row.get('id') or '').strip()
            if cid.isdigit():
                rows[int(cid)] = {k: (v or '').strip() for k, v in row.items() if k}
    return rows


def rgb(c):
    c = c.lstrip('#')
    return tuple(int(c[i:i + 2], 16) for i in (0, 2, 4))


def luma(c):
    r, g, b = rgb(c)
    return 0.299 * r + 0.587 * g + 0.114 * b


def far(a, b):
    return sum(abs(x - y) for x, y in zip(rgb(a), rgb(b))) > 120


def initials(name):
    words = [w for w in name.replace('&', ' ').replace('.', ' ').split() if w]
    if len(words) == 1:
        w = words[0]
        return w if w.isupper() and len(w) <= 4 else w[0].upper() + 'FC'
    out = ''
    for w in words:
        out += w if w.isupper() and w.isalpha() else w[0].upper()
    return out[:4]


def pattern(kind, main, second):
    s = SIZE
    if kind == 0:  # vertical stripes
        return ''.join(f'<rect x="{x}" y="0" width="32" height="{s}" fill="{second}"/>' for x in range(16, s, 64))
    if kind == 1:  # one band across the middle
        return f'<rect x="0" y="{s * 0.36:.0f}" width="{s}" height="{s * 0.28:.0f}" fill="{second}"/>'
    if kind == 2:  # a sash from the top left to the bottom right
        return f'<polygon points="0,0 70,0 {s},{s - 70} {s},{s} {s - 70},{s} 0,70" fill="{second}"/>'
    if kind == 3:  # halves
        return f'<rect x="{s // 2}" y="0" width="{s // 2}" height="{s}" fill="{second}"/>'
    if kind == 4:  # a chevron
        return f'<polygon points="0,90 {s // 2},170 {s},90 {s},140 {s // 2},220 0,140" fill="{second}"/>'
    # quarters
    h = s // 2
    return f'<rect x="{h}" y="0" width="{h}" height="{h}" fill="{second}"/><rect x="0" y="{h}" width="{h}" height="{h}" fill="{second}"/>'


def crest(cid, club):
    main = club['homekitshirtcol1'] or '#FFFFFF'
    second = club['homekitshirtcol2'] or club['homekitshortscol'] or '#FFFFFF'
    if not far(main, second):
        for c in (club['homekitshortscol'], club['awaykitshirtcol1'], '#FFFFFF', '#000000'):
            if c and far(main, c):
                second = c
                break
    kind = int(hashlib.sha1(str(cid).encode()).hexdigest(), 16) % 6
    # The disc in the middle holds the initials.
    disc = '#FFFFFF' if luma(main) < 140 else '#1A1A1A'
    ink = main if far(main, disc) else second
    text = initials(club['shortname'])
    height = 58 if len(text) <= 3 else 46
    width = min(natural_width(text, height, track=0.04), 150)
    x = (SIZE - width) / 2
    d = text_path(text, x, (SIZE - height) / 2, width, height, track=0.04)
    c = SIZE / 2
    return (f'<rect width="{SIZE}" height="{SIZE}" fill="{main}"/>'
            f'{pattern(kind, main, second)}'
            f'<circle cx="{c}" cy="{c}" r="88" fill="{disc}" stroke="{second}" stroke-width="10"/>'
            f'<circle cx="{c}" cy="{c}" r="74" fill="none" stroke="{ink}" stroke-width="3"/>'
            f'<path d="{d}" fill="{ink}"/>')


def main():
    if len(sys.argv) < 2:
        sys.exit('usage: crests.py <pack folder> [data set folder]')
    pack = sys.argv[1]
    dataset = sys.argv[2] if len(sys.argv) > 2 else DATASET
    base = read_clubs(os.path.join(dataset, 'Clubs.txt'))
    clubs = read_clubs(os.path.join(pack, 'world', 'Clubs.txt'))
    if not clubs:
        sys.exit('the pack has no world/Clubs.txt')
    out = os.path.join(pack, 'crests')
    os.makedirs(out, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        for cid, row in sorted(clubs.items()):
            # An empty cell of the pack keeps the value of the data set.
            club = {k: row.get(k) or base.get(cid, {}).get(k, '') for k in COLUMNS}
            svg = os.path.join(tmp, f'{cid}.svg')
            render(crest(cid, club), SIZE, SIZE, svg, os.path.join(out, f'{cid}.png'))
    print(f'{len(clubs)} crests in {out}')


if __name__ == '__main__':
    main()
