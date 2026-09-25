"""Arrange existing runtime captures for visual QA; never alter source images."""
import argparse
import math
from pathlib import Path
from PIL import Image, ImageDraw


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('captures', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--choices', type=int, nargs='+', default=list(range(37)) + [99])
    args = parser.parse_args()
    frames = []
    for choice in args.choices:
        path = args.captures / f'choice-{choice}-final.bmp'
        with Image.open(path) as source:
            frame = source.convert('RGB')
        frame.thumbnail((400, 224), Image.Resampling.LANCZOS)
        frames.append((choice, frame))
    cell_height = max(frame.height for _, frame in frames) + 24
    sheet = Image.new('RGB', (1600, math.ceil(len(frames) / 4) * cell_height), '#181818')
    labels = ImageDraw.Draw(sheet)
    for index, (choice, frame) in enumerate(frames):
        x, y = (index % 4) * 400, (index // 4) * cell_height
        labels.text((x + 8, y + 5), f'BG {choice}', fill='white')
        sheet.paste(frame, (x, y + 24))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)
    print(f'{len(frames)} runtime captures; QA sheet: {args.output}')


if __name__ == '__main__':
    main()
