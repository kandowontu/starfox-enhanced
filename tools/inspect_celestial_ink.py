"""Report connected authored celestial-ink bounds from a captured BG2 atlas."""
import argparse
import json
from pathlib import Path


def inspect(snapshot, first, last):
    m = json.loads(snapshot.read_text())
    vram = Path(str(snapshot).removesuffix('.ppu.json') + '.vram').read_bytes()
    edge = 16 if m['bg2_tile16'] else 8
    width = (64 if m['bg2_size'] & 1 else 32) * edge
    height = (64 if m['bg2_size'] & 2 else 32) * edge
    pages = 2 if m['bg2_size'] & 1 else 1
    word = lambda a: vram[a & 65535] | vram[(a + 1) & 65535] << 8
    points = set()
    for y in range(height):
        for x in range(width):
            tx, ty = x // edge, y // edge
            entry = (tx // 32 + ty // 32 * pages) * 1024 + ty % 32 * 32 + tx % 32
            tile = word(m['bg2_map'] * 2 + entry * 2)
            px = edge - 1 - x % edge if tile & 0x4000 else x % edge
            py = edge - 1 - y % edge if tile & 0x8000 else y % edge
            character = ((tile & 1023) + px // 8 + py // 8 * 16) & 1023
            address = m['bg2_char'] * 2 + character * 32 + py % 8 * 2
            lo, hi, mask = word(address), word(address + 16), 128 >> (px % 8)
            ink = int(bool(lo & mask)) + 2 * bool(lo & (mask << 8))
            ink += 4 * bool(hi & mask) + 8 * bool(hi & (mask << 8))
            if ink and first <= (tile >> 10 & 7) * 16 + ink <= last:
                points.add((x, y))
    groups = []
    while points:
        seed = points.pop()
        group, todo = [seed], [seed]
        while todo:
            x, y = todo.pop()
            for q in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)):
                if q in points:
                    points.remove(q); group.append(q); todo.append(q)
        if len(group) >= 8:
            xs, ys = zip(*group)
            groups.append({'pixels':len(group),'bounds':[min(xs),min(ys),max(xs)+1,max(ys)+1]})
    return {'background':m['background'],'size':[width,height],
            'components':sorted(groups,key=lambda g:-g['pixels'])}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('snapshot',type=Path)
    parser.add_argument('first',type=int)
    parser.add_argument('last',type=int)
    args = parser.parse_args()
    print(json.dumps(inspect(args.snapshot,args.first,args.last),indent=2))
