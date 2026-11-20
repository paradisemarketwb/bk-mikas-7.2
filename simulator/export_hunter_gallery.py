#!/usr/bin/env python3
"""PNG exports of actual LVGL RGB565 framebuffers; no generated/mock UI artwork."""
import argparse
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument("render_directory", type=Path)
parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "docs/images/hunter-reference")
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
for style, prefix, count in (("classic", "", 5), ("reference", "reference-", 10)):
    for theme in ("light", "dark"):
        for index in range(count):
            source = args.render_directory / f"{prefix}screen-{index:02}-{theme}.ppm"
            with Image.open(source) as frame:
                frame.save(args.output / f"{style}-{index:02}-{theme}.png")
for theme in ("light", "dark"):
    sheet = Image.new("RGB", (1200, 480), "#f2f1ed")
    for index in range(10):
        with Image.open(args.output / f"reference-{index:02}-{theme}.png") as frame:
            sheet.paste(frame.resize((240, 240)), ((index % 5) * 240, (index // 5) * 240))
    sheet.save(args.output / ("reference-contact.png" if theme == "light" else "reference-contact-dark.png"))
with Image.open(args.render_directory / "layout-editor.ppm") as frame:
    frame.save(args.output / "drag-editor.png")
comparison = Image.new("RGB", (960, 480), "#f2f1ed")
for index, name in enumerate(("classic-01-light.png", "reference-01-light.png")):
    with Image.open(args.output / name) as frame:
        comparison.paste(frame, (index * 480, 0))
comparison.save(args.output / "styles-engine.png")
print(f"Exported actual LVGL frames to {args.output}")
