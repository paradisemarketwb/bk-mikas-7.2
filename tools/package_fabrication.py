#!/usr/bin/env python3
"""Package real KiCad exports and printable case parts, or verify stored ZIPs.
Run only after ERC/DRC/export/mesh checks. This is not vehicle qualification.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile
ROOT = Path(__file__).resolve().parents[1]

def archive(path, files, note, check):
    contents = {name: file.read_bytes() for name, file in sorted(files.items())}
    contents['README.txt'] = note.encode('utf-8')
    contents['SHA256.json'] = (json.dumps({name: hashlib.sha256(data).hexdigest()
        for name, data in contents.items()}, indent=2, sort_keys=True) + '\n').encode()
    if check:
        with zipfile.ZipFile(path) as z:
            assert z.testzip() is None
            assert set(z.namelist()) == set(contents), path
            for name, data in contents.items():
                assert z.read(name) == data, (path, name)
        print('PASS archive matches current source exports:', path.relative_to(ROOT))
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, data in sorted(contents.items()):
            info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            z.writestr(info, data)
    print('Wrote', path.relative_to(ROOT))

def main(check):
    files = {}
    for directory, stem in [('r2', 'BK-KLINE-R2'), ('power-r2', 'BK-POWER-R2')]:
        board = ROOT / 'hardware/kicad' / directory
        report = (board / 'reports/DRC.txt').read_text()
        assert re.search(r'Found 0 DRC violations', report), stem
        assert re.search(r'Found 0 unconnected pads', report), stem
        for suffix in ['gtl', 'gbl', 'gts', 'gbs', 'gto', 'gbo', 'gm1', 'drl']:
            assert list((board / 'fabrication-prototype').glob('*.' + suffix)), (stem, suffix)
        for source in (board / 'fabrication-prototype').iterdir():
            if source.is_file():
                files[stem + '/fabrication/' + source.name] = source
        for name in ['BOM.csv', 'README.md']:
            files[stem + '/' + name] = board / name
    archive(ROOT / 'hardware/fabrication/BK-R2-prototype-fabrication.zip', files,
        'BK R2 engineering prototype: separate BK-KLINE-R2 and BK-POWER-R2 boards.\n'
        'Gerber/Excellon exported from real KiCad PCB files. BOM and positions included.\n'
        'Check each board README before ordering. Electrical/thermal/vehicle qualification\n'
        'and physical footprint confirmation remain required. Not a vehicle certification.\n', check)
    case = ROOT / 'hardware/case/r2'
    stls = sorted((case / 'stl').glob('*.stl'))
    assert len(stls) >= 7, 'Missing gauge/interface/power case parts'
    files = {'stl/' + p.name: p for p in stls}
    files.update({p.name: p for p in case.glob('*.scad')})
    for name in ['README.md', 'SOURCES.json']:
        files[name] = case / name
    archive(ROOT / 'hardware/fabrication/BK-R2-printable-cases.zip', files,
        'BK R2 case sources and separate printable parts.\n'
        'Use CAD README and verify dimensions on the actual Waveshare and assembled PCBs.\n'
        'Closed STL mesh is not proof of physical fit, heat resistance or vibration strength.\n', check)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    main(parser.parse_args().check)
