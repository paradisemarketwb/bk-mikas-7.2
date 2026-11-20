# Regenerate with lv_font_conv 1.5.3; DejaVu license: ui_widgets/licenses/DejaVu-copyright.txt.
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'firmware/components/ui_widgets/assets/uaz21/fonts'
import argparse
p=argparse.ArgumentParser();p.add_argument('--converter',default='lv_font_conv');args=p.parse_args();conv=args.converter
fonts=[(146,'digits','/usr/share/fonts/truetype/dejavu/DejaVuSans-BoldOblique.ttf'),*[(size,'digits','/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf') for size in (74,72,60,58,54,52,46,38)],*[(size,'text','/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf') for size in (34,30,26)]]
for size,kind,path in fonts:
 name='bk_uaz_font_'+str(size)
 ranges='0x20-0x3A,0x43,0x56,0x45,0x46,0xB0,0xD7,0x3BB,0x412,0x431,0x433,0x438,0x43A-0x43E,0x440,0x441,0x447' if kind=='digits' else '0x20-0x7F,0xB0,0xD7,0x3BB,0x400-0x45F'
 path='/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf' if size in (26,38,60,72) else path
 subprocess.run([conv,'--font',path,'--size',str(size),'--bpp','4','--format','lvgl','--range',ranges,'--no-compress','-o',str(out/(name+'.c'))],check=True,capture_output=True)
 p=out/(name+'.c');s=p.read_text();s=s.replace('#ifdef LV_LVGL_H_INCLUDE_SIMPLE\n#include "lvgl.h"\n#else\n#include "lvgl/lvgl.h"\n#endif','#include "lvgl.h"');
 if kind=='digits':s=s.replace('#include "lvgl.h"','#include "lvgl.h"\n#include "../bk_uaz_fonts.h"').replace('.fallback = NULL,','.fallback = &bk_uaz_font_34,')
 p.write_text(s)
header=['#pragma once','#include "lvgl.h"',*[f'LV_FONT_DECLARE(bk_uaz_font_{n});' for n,_,_ in fonts]]
header+=['const lv_font_t *bk_uaz_font(unsigned px);']
(out.parent/'bk_uaz_fonts.h').write_text('\n'.join(header)+'\n')
(out.parent/'bk_uaz_fonts.c').write_text('#include "bk_uaz_fonts.h"\nconst lv_font_t *bk_uaz_font(unsigned px){switch(px){'+''.join(f'case {n}:return &bk_uaz_font_{n};' for n,_,_ in fonts)+'default:return &bk_uaz_font_30;}}\n')
(out.parent/'sources.cmake').write_text('set(BK_UAZ_ASSET_SOURCES\n "${CMAKE_CURRENT_LIST_DIR}/bk_uaz_widgets.c"\n "${CMAKE_CURRENT_LIST_DIR}/uaz_icons.c"\n "${CMAKE_CURRENT_LIST_DIR}/bk_uaz_fonts.c"\n'+''.join(f' "${{CMAKE_CURRENT_LIST_DIR}}/fonts/bk_uaz_font_{n}.c"\n' for n,_,_ in fonts)+')\n')
print('Generated',len(fonts),'fonts')
