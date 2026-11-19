#!/usr/bin/env python3
"""Refresh both real KiCad fabrication sets, validation reports and source hashes."""
import subprocess,pathlib,os,json,hashlib,csv,sys,xml.etree.ElementTree as ET
root=pathlib.Path(__file__).resolve().parents[2];env=os.environ.copy();env['KICAD9_3DMODEL_DIR']=str(root/'hardware/kicad/models3d')
for sub,name in [('r2','BK-KLINE-R2'),('power-r2','BK-POWER-R2')]:
 if '--board' in sys.argv and sub!=sys.argv[sys.argv.index('--board')+1]:continue
 p=root/'hardware/kicad'/sub;pcb=str(p/(name+'.kicad_pcb'));sch=str(p/(name+'.kicad_sch'));fab=p/'fabrication-prototype';ex=p/'exports';rep=p/'reports'
 for d in [fab,ex,rep]:d.mkdir(exist_ok=True)
 # Avoid leaving stale merged drill files next to the new PTH / NPTH split set.
 for f in fab.glob('*'):
  if f.is_file():f.unlink()
 cmds=[['sch','erc',sch,'--exit-code-violations','-o',str(rep/'ERC.txt')],['pcb','drc',pcb,'--exit-code-violations','-o',str(rep/'DRC.txt')],['sch','export','netlist',sch,'--format','kicadxml','-o',str(rep/'netlist.xml')],['pcb','export','gerbers',pcb,'-o',str(fab),'-l','F.Cu,B.Cu,F.Mask,B.Mask,F.Silkscreen,B.Silkscreen,Edge.Cuts','--subtract-soldermask'],['pcb','export','drill',pcb,'-o',str(fab),'--excellon-separate-th','--generate-map','--map-format','svg'],['pcb','export','pos',pcb,'-o',str(fab/'positions.csv'),'--format','csv','--units','mm'],['pcb','export','svg',pcb,'-o',str(ex/'pcb-top.svg'),'-l','F.Cu,F.Silkscreen,Edge.Cuts','--fit-page-to-board','--exclude-drawing-sheet'],['pcb','export','svg',pcb,'-o',str(ex/'pcb-bottom.svg'),'-l','B.Cu,B.Silkscreen,Edge.Cuts','--mirror','--fit-page-to-board','--exclude-drawing-sheet'],['sch','export','svg',sch,'-o',str(ex)],['sch','export','pdf',sch,'-o',str(ex/(name+'.pdf'))],['pcb','export','step',pcb,'-o',str(ex/(name+'.step')),'--force','--subst-models'],['pcb','render',pcb,'-o',str(ex/'pcb-3d.png'),'--width','1200','--height','900','--rotate','30,0,30','--background','opaque']]
 for cmd in cmds:
  r=subprocess.run(['kicad-cli']+cmd,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  if r.returncode:raise SystemExit(f'{name} {cmd[:3]}: {r.stdout}')
  print(name,' '.join(cmd[:3]),'PASS',flush=True)
 tree=ET.parse(rep/'netlist.xml');nets=[(n.attrib['name'],'|'.join(x.attrib['ref']+'.'+x.attrib['pin'] for x in n.findall('node') if not x.attrib['ref'].startswith('#'))) for n in tree.findall('.//nets/net') if not n.attrib['name'].startswith('unconnected-')]
 with (p/'NETLIST.csv').open('w') as f:
  w=csv.writer(f);w.writerow(['net','pads']);w.writerows(nets)
 if sub=='r2':
  old=list(csv.DictReader((p/'BOM.csv').open()));mpns={r['Reference']:r.get('Manufacturer_PN',r.get('Manufacturer part / requirement','')) for r in old};mpns.update({'D1':'SMAJ18A-TR','R7':'CRCW120622R0FKEAHP'})
  with (p/'BOM.csv').open('w') as f:
   w=csv.writer(f);w.writerow(['Reference','Value','Footprint','Manufacturer_PN'])
   for c in tree.findall('.//components/comp'):
    ref=c.attrib['ref']
    if not ref.startswith('#'):w.writerow([ref,c.findtext('value'),c.findtext('footprint'),mpns.get(ref,'')])
 files=list(p.glob('*.kicad_sym'))+list(p.glob('*.pretty/*.kicad_mod'))+([p/'fp-lib-table'] if (p/'fp-lib-table').exists() else [])+[p/(name+s) for s in ['.kicad_pcb','.kicad_sch','.kicad_pro']]+list(p.glob('*.kicad_dru'))+[p/'BOM.csv',p/'NETLIST.csv',rep/'netlist.xml',rep/'ERC.txt',rep/'DRC.txt']+sorted(fab.glob('*'))+sorted(ex.glob('*'))
 (rep/'SHA256.json').write_text(json.dumps({str(f.relative_to(p)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files if f.is_file()},indent=2)+'\n')
