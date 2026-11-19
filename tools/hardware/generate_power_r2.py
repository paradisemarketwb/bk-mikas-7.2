#!/usr/bin/python3
"""Generate editable R2 power schematic and initial PCB from exact component map.
Requires KiCad9 pcbnew. Existing routed PCB is NOT overwritten without --initial-pcb.
"""
from pathlib import Path
import uuid,json,csv,sys
import pcbnew as p
from power_parts import parts
root=Path(__file__).resolve().parents[2];out=root/'hardware/kicad/power-r2';out.mkdir(parents=True,exist_ok=True)
u=lambda:str(uuid.uuid4())
# TI DRR0012E recommended land pattern: 0.5 pitch; floating pad13 mandatory.
fp='(footprint "WSON-12_DRR_3x3_EP1.3x2.5" (version 20240108) (generator pcbnew) (layer "F.Cu") (attr smd) (fp_rect (start -1.5 -1.5) (end 1.5 1.5) (stroke (width .1) (type default)) (fill none) (layer "F.Fab")) (fp_rect (start -1.9 -1.75) (end 1.9 1.75) (stroke (width .05) (type default)) (fill none) (layer "F.CrtYd")) (fp_text reference "REF**" (at 0 -2.2) (layer "F.SilkS") (effects (font (size 1 1) (thickness .15))))'
for n in range(1,13):
 x=-1.425 if n<=6 else 1.425;y=(-1.25+(n-1)*.5) if n<=6 else (1.25-(n-7)*.5)
 fp+=f'(pad "{n}" smd rect (at {x} {y}) (size .85 .25) (layers "F.Cu" "F.Paste" "F.Mask"))'
fp+='(pad "13" smd rect (at 0 0) (size 1.3 2.5) (layers "F.Cu" "F.Paste" "F.Mask")))'
(out/'BK_POWER.pretty/WSON-12_DRR_3x3_EP1.3x2.5.kicad_mod').write_text(fp)
(out/'fp-lib-table').write_text('(fp_lib_table (lib (name BK_POWER) (type KiCad) (uri ${KIPRJMOD}/BK_POWER.pretty) (options "") (descr "TI DRR0012E")))')
lib=[];instances=[];labels=[];rootuid=u();sources=[('ACC_RAW',14),('CTRL_VS',18),('COMMON_SOURCE',22),('VS_PROT',26),('5V_OUT',34),('USB_5V',38),('GND',42)]
allparts=parts+[(f'#FLG{i}',net,'',[(net,net,'power_out')],0,0) for i,(net,_) in enumerate(sources)]
for idx,(ref,val,foot,pins,x,y) in enumerate(allparts):
 name=ref.replace('#','');sx=35.56+(idx%4)*195.58;sy=35.56+(idx//4)*50.8;h=max(5,len(pins)*1.27+1.27);isflag=ref.startswith('#');props=f'(property "Reference" "{ref}" (at 0 {-h-3} 0) (effects (font (size 1.27 1.27)))) (property "Value" "{val}" (at 0 {h+3} 0) (effects (font (size 1 1)))) (property "Footprint" "{foot}" (at 0 0 0) (effects (font (size 1 1)) (hide yes)))'
 pinstr=''
 for j,(pn,net,typ) in enumerate(pins):
  yy=round((len(pins)-1)*1.27-j*2.54,2);pinstr+=f'(pin {typ} line (at -12.7 {yy} 0) (length 5.08) (name "{pn}" (effects (font (size 1 1)))) (number "{j+1}" (effects (font (size 1 1)))))';px=sx-12.7;py=round(sy-yy,2)
  labels.append(f'(label "{net}" (at {px} {py} 0) (effects (font (size 1 1)) (justify right bottom)) (uuid "{u()}"))' if net else f'(no_connect (at {px} {py}) (uuid "{u()}"))')
 lib.append(f'(symbol "BK_POWER:{name}" (pin_names (offset .5)) (in_bom {"no" if isflag else "yes"}) (on_board {"no" if isflag else "yes"}) {props} (symbol "{name}_0_1" (rectangle (start -7.62 {-h}) (end 7.62 {h}) (stroke (width .254) (type default)) (fill (type background)))) (symbol "{name}_1_1" {pinstr}))')
 instances.append(f'(symbol (lib_id "BK_POWER:{name}") (at {sx} {sy} 0) (unit 1) (in_bom {"no" if isflag else "yes"}) (on_board {"no" if isflag else "yes"}) (dnp no) (uuid "{u()}") (property "Reference" "{ref}" (at {sx+12} {sy-3} 0) (effects (font (size 1.27 1.27)))) (property "Value" "{val}" (at {sx+12} {sy} 0) (effects (font (size 1 1)))) (property "Footprint" "{foot}" (at {sx} {sy} 0) (effects (font (size 1 1)) (hide yes))) (instances (project "BK-POWER-R2" (path "/{rootuid}" (reference "{ref}") (unit 1)))))')
sch=f'(kicad_sch (version 20231120) (generator "bk_power_r2") (uuid "{rootuid}") (paper "A1") (title_block (title "BK-POWER-R2 - LM74800 protected power + USB ideal diode interposer") (rev "R2") (comment 1 "Engineering design: bench/load-dump qualification required")) (lib_symbols '+''.join(lib)+') '+''.join(labels)+''.join(instances)+')'
(out/'BK-POWER-R2.kicad_sch').write_text(sch)
(out/'BK_POWER.kicad_sym').write_text('(kicad_symbol_lib (version 20231120) (generator bk_power_r2) '+''.join(lib).replace('(symbol "BK_POWER:','(symbol "')+')')
(out/'sym-lib-table').write_text('(sym_lib_table (lib (name BK_POWER) (type KiCad) (uri ${KIPRJMOD}/BK_POWER.kicad_sym) (options "") (descr "Original power symbols")))')
if not (out/'BK-POWER-R2.kicad_pro').exists():(out/'BK-POWER-R2.kicad_pro').write_text(json.dumps({'board':{'design_settings':{'rules':{'min_clearance':.15,'min_track_width':.15,'min_through_hole_diameter':.25}}}},indent=2))
with (out/'BOM.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['Reference','Value/MPN','Footprint']);w.writerows((c[0],c[1],c[2]) for c in parts)
if '--initial-pcb' not in sys.argv:sys.exit(0)
b=p.BOARD();nets={}
for net in sorted({pi[1] for c in parts for pi in c[3] if pi[1]}):ni=p.NETINFO_ITEM(b,'/'+net);b.Add(ni);nets[net]=ni
for ref,val,fp,pins,x,y in parts:
 lib,fn=fp.split(':');folder=str(out/'BK_POWER.pretty') if lib=='BK_POWER' else '/usr/share/kicad/footprints/'+lib+'.pretty';f=p.FootprintLoad(folder,fn)
 if not f:raise ValueError(fp)
 f.SetReference(ref);f.SetValue(val);f.SetPosition(p.VECTOR2I(p.FromMM(x),p.FromMM(y)));b.Add(f)
 for pad in f.Pads():
  n=pad.GetNumber()
  if n.isdigit() and 0<int(n)<=len(pins) and pins[int(n)-1][1]:pad.SetNet(nets[pins[int(n)-1][1]])
for a,c in [((0,0),(90,0)),((90,0),(90,70)),((90,70),(0,70)),((0,70),(0,0))]:
 e=p.PCB_SHAPE();e.SetShape(p.SHAPE_T_SEGMENT);e.SetStart(p.VECTOR2I(p.FromMM(a[0]),p.FromMM(a[1])));e.SetEnd(p.VECTOR2I(p.FromMM(c[0]),p.FromMM(c[1])));e.SetLayer(p.Edge_Cuts);e.SetWidth(p.FromMM(.05));b.Add(e)
for i,(x,y) in enumerate([(3,3),(87,3),(3,67),(87,67)]):
 f=p.FootprintLoad('/usr/share/kicad/footprints/MountingHole.pretty','MountingHole_2.7mm_M2.5');f.SetReference('H'+str(i+1));f.SetPosition(p.VECTOR2I(p.FromMM(x),p.FromMM(y)));b.Add(f)
p.SaveBoard(str(out/'BK-POWER-R2.kicad_pcb'),b)
