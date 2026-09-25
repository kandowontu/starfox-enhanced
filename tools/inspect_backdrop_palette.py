"""Read captured BG2 palette ownership and colour-response inputs (no mutation)."""
import argparse
import json
import struct
from pathlib import Path


def region_inks(path, rectangle):
    """Count exact BG2 inks in an authored atlas rectangle, before scrolling."""
    metadata = json.loads(path.read_text())
    stem = str(path).removesuffix('.ppu.json')
    vram = Path(stem + '.vram').read_bytes()
    live = struct.unpack('<256H', Path(stem + '.cgram').read_bytes()[:512])
    edge = 16 if metadata['bg2_tile16'] else 8
    pages = 2 if metadata['bg2_size'] & 1 else 1
    width = pages * 32 * edge
    height = (64 if metadata['bg2_size'] & 2 else 32) * edge
    left, top, right, bottom = rectangle
    if not (0 <= left < right <= width and 0 <= top < bottom <= height):
        raise ValueError(f'Region {rectangle} is outside the {width}x{height} atlas')
    def word(address):
        return vram[address & 65535] | vram[(address + 1) & 65535] << 8
    inks = {}
    for y in range(top, bottom):
        for x in range(left, right):
            tx, ty = x // edge, y // edge
            entry = (tx // 32 + ty // 32 * pages) * 1024 + ty % 32 * 32 + tx % 32
            tile = word(metadata['bg2_map'] * 2 + entry * 2)
            px = edge - 1 - x % edge if tile & 0x4000 else x % edge
            py = edge - 1 - y % edge if tile & 0x8000 else y % edge
            character = ((tile & 1023) + px // 8 + py // 8 * 16) & 1023
            address = metadata['bg2_char'] * 2 + character * 32 + py % 8 * 2
            bits, high, mask = word(address), word(address + 16), 128 >> (px % 8)
            ink = int(bool(bits & mask)) + 2 * bool(bits & (mask << 8))
            ink += 4 * bool(high & mask) + 8 * bool(high & (mask << 8))
            index = (tile >> 10 & 7) * 16 + ink if ink else 0
            item = inks.setdefault(index, {'count': 0, 'bounds': [x, y, x + 1, y + 1]})
            item['count'] += 1
            bounds = item['bounds']
            bounds[:] = [min(bounds[0], x), min(bounds[1], y),
                         max(bounds[2], x + 1), max(bounds[3], y + 1)]
    return {'rectangle': rectangle, 'inks': [
        {'index': index, 'rgb5': [(live[index] >> shift) & 31 for shift in (0, 5, 10)], **item}
        for index, item in sorted(inks.items())]}


def inspect(path, origin):
    metadata = json.loads(path.read_text())
    stem = str(path).removesuffix('.ppu.json')
    vram = Path(stem + '.vram').read_bytes()
    live = struct.unpack('<256H', Path(stem + '.cgram').read_bytes()[:512])
    reference = struct.unpack('<112H', Path(stem + '.palette-source').read_bytes()[:224])
    edge = 16 if metadata['bg2_tile16'] else 8
    width = (64 if metadata['bg2_size'] & 1 else 32) * edge
    height = (64 if metadata['bg2_size'] & 2 else 32) * edge
    pages = 2 if metadata['bg2_size'] & 1 else 1
    regions = [0] * 256
    def word(a):
        return vram[a & 65535] | vram[(a + 1) & 65535] << 8
    for y in range(origin, height):
        region = 2 if y < origin + 48 else 1 if y >= origin + 128 else 0
        if not region:
            continue
        counts = [0] * 256
        for x in range(0, width, 4):
            tx, ty = x // edge, y // edge
            entry = (tx // 32 + ty // 32 * pages) * 1024 + ty % 32 * 32 + tx % 32
            tile = word(metadata['bg2_map'] * 2 + entry * 2)
            px = edge - 1 - x % edge if tile & 0x4000 else x % edge
            py = edge - 1 - y % edge if tile & 0x8000 else y % edge
            character = ((tile & 1023) + px // 8 + py // 8 * 16) & 1023
            address = metadata['bg2_char'] * 2 + character * 32 + py % 8 * 2
            bits, high, mask = word(address), word(address + 16), 128 >> (px % 8)
            ink = int(bool(bits & mask)) + 2 * bool(bits & (mask << 8))
            ink += 4 * bool(high & mask) + 8 * bool(high & (mask << 8))
            if ink:
                counts[(tile >> 10 & 7) * 16 + ink] += 1
        threshold = width // 64 if region == 1 else (width * 95 + 399) // 400
        for index in range(1, 128):
            if counts[index] >= threshold:
                regions[index] |= region
    rgb = lambda c: [(c >> shift) & 31 for shift in (0, 5, 10)]
    return {'background': metadata['background'], 'source': metadata['palette_source'],
            'sky': [{'index': i, 'reference': rgb(reference[i]), 'live': rgb(live[i])}
                    for i in range(112) if i % 16 and regions[i] == 2],
            'ground': [{'index': i, 'reference': rgb(reference[i]), 'live': rgb(live[i])}
                       for i in range(112) if i % 16 and regions[i] == 1]}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('snapshot', type=Path)
    parser.add_argument('--origin', type=int)
    parser.add_argument('--region', type=int, nargs=4, action='append',
                        metavar=('LEFT', 'TOP', 'RIGHT', 'BOTTOM'),
                        help='Report exact ink counts and bounds inside an authored BG2 region')
    args = parser.parse_args()
    if args.origin is None and not args.region:
        parser.error('provide --origin or --region')
    result = inspect(args.snapshot, args.origin) if args.origin is not None else {}
    if args.region:
        result['regions'] = [region_inks(args.snapshot, rectangle) for rectangle in args.region]
    print(json.dumps(result, indent=2))
