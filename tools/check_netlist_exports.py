#!/usr/bin/env python3
"""Compare electrical connectivity of two KiCad XML exports, ignoring timestamps."""
from pathlib import Path
import argparse
import xml.etree.ElementTree as ET

def connections(path):
    return {(node.attrib['ref'], node.attrib['pin']): net.attrib['name']
            for net in ET.parse(path).findall('.//nets/net')
            if not net.attrib['name'].startswith('unconnected-')
            for node in net.findall('node')
            if not node.attrib['ref'].startswith('#')}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('current', type=Path)
    parser.add_argument('recorded', type=Path)
    args = parser.parse_args()
    current, recorded = connections(args.current), connections(args.recorded)
    assert current and current == recorded, f'Schematic connectivity mismatch: {current.items() ^ recorded.items()}'
    print(f'PASS: fresh schematic export matches recorded {len(current)} connected pins')
