#!/usr/bin/env python3
"""Check supplied PNG pixels against compiled LVGL9 BGRA data, using stdlib only."""
from pathlib import Path
import hashlib
import json
import re
import struct
import zlib
ROOT=Path(__file__).resolve().parents[1]

def png_rgba(path):
    raw=path.read_bytes();assert raw[:8]==b'\x89PNG\r\n\x1a\n'
    at=8;compressed=bytearray();width=height=0
    while at<len(raw):
        length,kind=struct.unpack_from('>I4s',raw,at);payload=raw[at+8:at+8+length];at+=12+length
        if kind==b'IHDR':
            width,height,depth,color,_,_,interlace=struct.unpack('>IIBBBBB',payload)
            assert depth==8 and color==6 and interlace==0,(path,depth,color,interlace)
        elif kind==b'IDAT':compressed.extend(payload)
        elif kind==b'IEND':break
    scan=zlib.decompress(compressed);stride=width*4;previous=bytearray(stride);pixels=bytearray();offset=0
    def paeth(a,b,c):
        p=a+b-c;pa,pb,pc=abs(p-a),abs(p-b),abs(p-c)
        return a if pa<=pb and pa<=pc else b if pb<=pc else c
    for _ in range(height):
        method=scan[offset];offset+=1;row=bytearray(scan[offset:offset+stride]);offset+=stride
        for x in range(stride):
            left=row[x-4] if x>=4 else 0;up=previous[x];ul=previous[x-4] if x>=4 else 0
            add=(0,left,up,(left+up)//2,paeth(left,up,ul))[method]
            row[x]=(row[x]+add)&255
        pixels.extend(row);previous=row
    return width,height,pixels

manifest=json.loads((ROOT/'design/uaz21/PROVENANCE.json').read_text())
source=(ROOT/'firmware/components/ui_widgets/assets/uaz21/uaz_icons.c').read_bytes()
assert hashlib.sha256(source).hexdigest()==manifest['compiled_icons_sha256'],'Supplied arrays changed'
arrays=re.findall(r'static const uint8_t uaz_(\w+)_data\[\] = \{(.*?)\};',source.decode(),re.S)
assert len(arrays)==19
for name,numbers in arrays:
    width,height,pixels=png_rgba(ROOT/f'design/uaz21/icons/png/48/{name}.png')
    assert (width,height)==(48,48)
    bgra=bytearray()
    for at in range(0,len(pixels),4):
        r,g,b,a=pixels[at:at+4];bgra.extend((b,g,r,a))
    compiled=bytes(int(n) for n in re.findall(r'\d+',numbers))
    assert compiled==bgra and len(compiled)==9216,name
spec=json.loads((ROOT/'design/uaz21/layout/layout_480.json').read_text())
assert spec['display']['resolution_assumed']==[480,480] and spec['display']['center']==[240,240]
assert len(spec['screens'])==10
header=(ROOT/'firmware/components/ui_config/include/bk_uaz_layout.h').read_text()
for screen in spec['screens']:
    for box in screen['elements']:
        identifier='BK_UAZ_'+screen['id'][3:].upper()+'_'+box['id'].upper()
        expected=','.join(str(box[k]) for k in ('x','y','w','h','font_px'))
        assert f'{identifier} = {{{expected}}};' in header,identifier
print('UAZ21: all19 compiled icons match supplied PNG RGBA/BGRA pixels; geometry contract matches JSON: PASS')
