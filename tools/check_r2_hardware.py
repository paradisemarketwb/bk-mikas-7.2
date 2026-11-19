#!/usr/bin/env python3
"""Compare exported KiCad schematic nets to real PCB pads; check fabrication files.
Run with KiCad's Python (Linux: /usr/bin/python3). Export netlist first using
kicad-cli sch export netlist --format kicadxml -o reports/netlist.xml BK-KLINE-R2.kicad_sch.
"""
from pathlib import Path
import xml.etree.ElementTree as ET
import pcbnew

root = Path(__file__).resolve().parents[1] / 'hardware/kicad/r2'
schematic = {}
for net in ET.parse(root / 'reports/netlist.xml').findall('.//nets/net'):
    if net.attrib['name'].startswith('unconnected-'):
        continue
    for node in net.findall('node'):
        ref = node.attrib['ref']
        if not ref.startswith('#'):
            schematic[(ref, node.attrib['pin'])] = net.attrib['name']
board = pcbnew.LoadBoard(str(root / 'BK-KLINE-R2.kicad_pcb'))
actual = {(f.GetReference(), pad.GetNumber()): pad.GetNetname()
          for f in board.GetFootprints() for pad in f.Pads() if pad.GetNetname()}
assert schematic == actual, f'Net mismatch: {schematic.items() ^ actual.items()}'
fab = root / 'fabrication-prototype'
for ext in ('gtl', 'gbl', 'gts', 'gbs', 'gto', 'gbo', 'gm1'):
    files = list(fab.glob('*.' + ext))
    assert len(files) == 1, (ext, files)
    data = files[0].read_text()
    assert '%FS' in data and 'M02*' in data, files[0]
drill = list(fab.glob('*.drl'))
assert drill and all('M48' in f.read_text() and 'M30' in f.read_text() for f in drill)
print(f'PASS: {len(actual)} connected pads match schematic; 7 Gerber layers + Excellon validated')
