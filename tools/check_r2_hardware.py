#!/usr/bin/env python3
"""Check both current KiCad netlists, fabrication payloads and exact source hashes."""
from pathlib import Path
import xml.etree.ElementTree as ET
import json,hashlib
import pcbnew

base=Path(__file__).resolve().parents[1]/'hardware/kicad'
for sub,name in [('r2','BK-KLINE-R2'),('power-r2','BK-POWER-R2')]:
    root=base/sub
    required=[root/(name+'.kicad_sch'),root/(name+'.kicad_pcb'),root/'reports/netlist.xml',root/'reports/SHA256.json',root/'BOM.csv']
    assert all(p.is_file() and p.stat().st_size for p in required),f'{name}: missing required sources/exports'
    schematic={}
    for net in ET.parse(root/'reports/netlist.xml').findall('.//nets/net'):
        if net.attrib['name'].startswith('unconnected-'):continue
        for node in net.findall('node'):
            ref=node.attrib['ref']
            if not ref.startswith('#'):schematic[(ref,node.attrib['pin'])]=net.attrib['name']
    board=pcbnew.LoadBoard(str(root/(name+'.kicad_pcb')))
    actual={(f.GetReference(),pad.GetNumber()):pad.GetNetname() for f in board.GetFootprints() for pad in f.Pads() if pad.GetNetname()}
    assert schematic and schematic==actual,f'{name}: net mismatch {schematic.items()^actual.items()}'
    fab=root/'fabrication-prototype'
    for ext in ('gtl','gbl','gts','gbs','gto','gbo','gm1'):
        files=list(fab.glob('*.'+ext));assert len(files)==1,(name,ext,files)
        data=files[0].read_text();assert '%FS' in data and 'M02*' in data,files[0]
    drills=list(fab.glob('*.drl'))
    assert len(drills)==2 and any('NPTH' in f.name for f in drills) and any('PTH' in f.name and 'NPTH' not in f.name for f in drills),(name,'missing separated plated/nonplated drill files')
    assert all('M48' in f.read_text() and 'M30' in f.read_text() for f in drills),name
    manifest=json.loads((root/'reports/SHA256.json').read_text())
    for rel in [name+'.kicad_pcb',name+'.kicad_sch','reports/netlist.xml','BOM.csv']+[str(f.relative_to(root)) for f in fab.glob('*') if f.is_file()]:
        assert rel in manifest,f'{name}: missing source/artifact hash {rel}'
    for rel,digest in manifest.items():
        f=root/rel;assert f.is_file() and hashlib.sha256(f.read_bytes()).hexdigest()==digest,f'{name}: source/export changed without regeneration: {rel}'
    for rel in ['exports/pcb-top.svg','exports/pcb-bottom.svg','exports/pcb-3d.png','exports/'+name+'.step','exports/'+name+'.pdf']:
        assert (root/rel).is_file() and (root/rel).stat().st_size>100,rel
    print(f'PASS {name}: {len(actual)} connected pads match schematic; 7 Gerber + PTH/NPTH Excellon; all source/export SHA256 match')
