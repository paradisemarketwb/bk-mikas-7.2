#!/usr/bin/env python3
"""Export actual simulator PPM framebuffers to documentation PNGs (requires Pillow)."""
from pathlib import Path
import argparse
from PIL import Image, ImageDraw
p=argparse.ArgumentParser();p.add_argument('render_directory',type=Path)
p.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'docs/images/lvgl')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
for source in sorted(a.render_directory.glob('*.ppm')):
    if source.name.startswith('custom-'):
        continue
    Image.open(source).save(a.output/(source.stem+'.png'))
sheet=Image.new('RGB',(1200,1040),'#dddcd5');d=ImageDraw.Draw(sheet)
d.text((15,8),'LVGL 9.6.0 / SIMULATION DATA / 10 screens / light and dark',(0,0,0))
for theme in range(2):
    for n in range(10):
        name=f'screen-{n:02}-'+('dark' if theme else 'light')
        im=Image.open(a.output/(name+'.png'))
        sheet.paste(im.resize((240,240)),((n%5)*240,30+theme*500+(n//5)*250))
sheet.save(a.output/'all-screens.png')
widgets=Image.new('RGB',(960,780),'#dddcd5');d=ImageDraw.Draw(widgets)
d.text((15,8),'REAL LVGL / 9 configurable widget types + touchscreen editors / SIMULATION',(0,0,0))
names=[f'widget-{n:02}' for n in range(9)]+['layout-editor','widget-editor']
for i,name in enumerate(names):
    im=Image.open(a.output/(name+'.png'))
    widgets.paste(im.resize((240,240)),((i%4)*240,30+(i//4)*250))
widgets.save(a.output/'widgets-and-editor.png')
