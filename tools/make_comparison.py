"""Compose exact-size crops from rule_demo's synthetic frames; no webcam input."""
from pathlib import Path
import argparse
from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser()
parser.add_argument("source", type=Path)
parser.add_argument("destination", type=Path)
parser.add_argument("--font", default="DejaVuSans.ttf")
args = parser.parse_args()

def font(size):
    try:
        return ImageFont.truetype(args.font, size)
    except OSError:
        return ImageFont.load_default()

canvas = Image.new("RGB", (1240, 664), "#101720")
draw = ImageDraw.Draw(canvas)
draw.text((24, 18), "RM2027 BaseVision | Rule quality comparison", font=font(26), fill="#e6edf5")
draw.text((24, 58), "Same synthetic frame. AREA downsample -> LINEAR upscale. Crops shown at 1:1 pixels.",
          font=font(15), fill="#91a0b3")
for col, (health, coefficient, resolution) in enumerate([
        (10000, "1.00", "1920 x 1080"), (5000, "0.75", "1440 x 810"), (0, "0.50", "960 x 540")]):
    x = 104 + col * 368
    draw.text((x, 94), f"HP {health} | lambda {coefficient}", font=font(19), fill="#8fe2c6")
    draw.text((x, 122), f"Sampling: {resolution}", font=font(14), fill="#91a0b3")
    with Image.open(args.source / f"hp-{health}.png") as image:
        for row, cell in enumerate([6, 3, 1]):
            top = 152 + row * 152
            # Paste an unscaled crop: the composition adds no resampling.
            crop = image.convert("RGB").crop((180, 140 + row * 320, 532, 268 + row * 320))
            canvas.paste(crop, (x, top))
            if col == 0:
                draw.text((24, top + 50), f"{cell} px", font=font(18), fill="#e6edf5")
draw.text((24, 624), "At zero HP, a 1 px checkerboard becomes uniform gray through 2 x 2 area averaging.",
          font=font(14), fill="#91a0b3")
args.destination.parent.mkdir(parents=True, exist_ok=True)
canvas.save(args.destination, optimize=True)
print(args.destination)
