#!/usr/bin/env python3
"""Export real OpenSCAD meshes and optional desktop renders. --images needs DISPLAY."""
import argparse,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];cad=root/'hardware/case/r2';images=root/'docs/images/hardware';images.mkdir(parents=True,exist_ok=True)
a=argparse.ArgumentParser();a.add_argument('--images',action='store_true');args=a.parse_args()
for model,parts in [('gauge-r2',['body','rear','spacer']),('adapter-r2',['base','lid']),('adapter-power-stack-r2',['base','lid'])]:
 for part in parts:
  subprocess.run(['openscad','-o',str(cad/'stl'/f'{model}-{part}.stl'),'-D',f'part="{part}"',str(cad/f'{model}.scad')],check=True)
if args.images:
 for model,prefix in [('gauge-r2','gauge'),('adapter-power-stack-r2','adapter')]:
  for part in ['assembled','exploded']:
   cmd=['openscad','--preview','--imgsize=1200,900','--autocenter','--viewall','--colorscheme=Tomorrow Night','-D',f'part="{part}"','-o',str(images/f'{prefix}-{part}.png'),str(cad/f'{model}.scad')]
   if prefix=='gauge' and part=='assembled':cmd += ['--camera=100,-100,-100,0,0,14']
   subprocess.run(cmd,check=True)
