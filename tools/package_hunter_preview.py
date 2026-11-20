#!/usr/bin/env python3
"""Package verified CI outputs; never compile, flash, sign or upload firmware."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--windows', type=Path, required=True)
p.add_argument('--firmware', type=Path, required=True)
p.add_argument('--source-sha', required=True)
p.add_argument('--windows-run', required=True)
p.add_argument('--firmware-run', required=True)
p.add_argument('--output', type=Path, default=ROOT / 'downloads')
a = p.parse_args()
if len(a.source_sha) != 40 or any(c not in '0123456789abcdef' for c in a.source_sha):
    raise SystemExit('Exact lowercase source commit SHA required')
a.output.mkdir(parents=True, exist_ok=True)

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)

def check_pe(path):
    data = path.read_bytes()
    assert data[:2] == b'MZ', path
    off, = struct.unpack_from('<I', data, 0x3c)
    assert data[off:off+4] == b'PE\0\0', path
    assert struct.unpack_from('<H', data, off+4)[0] == 0x8664, path

def partitions(path):
    result = {}
    raw = path.read_bytes()
    for offset in range(0, len(raw)-31, 32):
        magic, kind, subtype, address, size, label, flags = struct.unpack_from('<HBBII16sI', raw, offset)
        if magic != 0x50aa:
            break
        result[label.split(b'\0',1)[0].decode()] = dict(type=kind, subtype=subtype, offset=address, size=size, flags=flags)
    required = {'nvs':(0x9000,0x5000),'otadata':(0xe000,0x2000),'ota_0':(0x10000,0x300000),'ota_1':(0x310000,0x300000),'ui_assets':(0x610000,0x100000),'ui_profiles':(0x710000,0x10000)}
    for name, (address, size) in required.items():
        assert (result[name]['offset'],result[name]['size']) == (address,size), name
    return result

metadata = dict(source_commit=a.source_sha, version='2.2.1-uaz21-preview', branch='feat/hunter-reference-ui', pr='https://github.com/paradisemarketwb/bk-mikas-7.2/pull/3', physical_tested=False, windows10_manual_tested=False, windows_ci_platform='Windows Server 2025 / Visual Studio 2026 / x64', windows_ci_run=a.windows_run, firmware_ci_run=a.firmware_run)

def finish(directory, name, extra):
    (directory/'BUILD-METADATA.json').write_text(json.dumps(metadata|extra,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    files = sorted(f for f in directory.rglob('*') if f.is_file())
    (directory/'SHA256SUMS.txt').write_text(''.join(digest(f)+'  '+f.relative_to(directory).as_posix()+'\n' for f in files),encoding='utf-8')
    target=a.output/name
    with zipfile.ZipFile(target,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for f in sorted(directory.rglob('*')):
            if f.is_file(): z.write(f,f.relative_to(directory).as_posix())
    target.with_name(target.name+'.sha256').write_text(digest(target)+'  '+target.name+'\n',encoding='utf-8')
    print(f'{target.name}: {target.stat().st_size} bytes, sha256 {digest(target)}')

with tempfile.TemporaryDirectory(prefix='hunter-package-') as temp:
    temp=Path(temp)
    win=temp/'windows';shutil.copytree(a.windows,win)
    check_pe(win/'bk_sim.exe');check_pe(win/'SDL2.dll')
    for name in ('Classic.cmd','Reference.cmd','WINDOWS-BUNDLE-RU.md'):
        assert (win/name).is_file(), name
    shutil.copytree(ROOT/'docs/images/hunter-reference',win/'screenshots')
    finish(win,'BK-MIKAS-Hunter-Windows-x64-preview.zip',dict(kind='windows-simulator',simulation_only=True))
    fw=temp/'firmware';fw.mkdir()
    all_partitions={}
    for mode in ('safe','live','usb-kkl'):
        src=a.firmware/f'BK-R2-ESP32S3-{mode}-experimental'
        assert (src/'bk_mikas_r2.bin').read_bytes()[0]==0xe9, src
        table=partitions(src/'partition_table/partition-table.bin')
        all_partitions[mode]=table
        shutil.copytree(src,fw/mode)
    assert all_partitions['safe']==all_partitions['live']==all_partitions['usb-kkl']
    (fw/'PARTITIONS-VERIFIED.json').write_text(json.dumps(all_partitions['safe'],indent=2)+'\n',encoding='utf-8')
    for rel in ('docs/hunter-background-migration.md','docs/background-storage.md','docs/sources-calibration.md','docs/wiring/USB_KKL.md','docs/firmware/USB_KKL.md','docs/firmware/R2.md','docs/WIRING.md','docs/lvgl/HUNTER_STATUS.md','docs/lvgl/IMPLEMENTATION.md','docs/lvgl/SCREEN_CUSTOMIZATION.md'):
        copy(ROOT/rel,fw/rel)
    for pattern in ('*usb*','waveshare*'):
        for image in (ROOT/'docs/images/hardware').glob(pattern):
            if image.is_file(): copy(image,fw/'docs/images/hardware'/image.name)
    shutil.copytree(ROOT/'firmware/components/ui_widgets/licenses',fw/'licenses/fonts')
    with zipfile.ZipFile(ROOT/'downloads/BK-MIKAS-USB-KKL-FT232RL-experimental.zip') as prior:
        for entry in prior.namelist():
            if entry.startswith('licenses/') and not entry.endswith('/'):
                target=fw/entry;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(prior.read(entry))
    copy(ROOT/'simulator/profiles/default.json',fw/'profiles/classic-v3.json')
    (fw/'READ-BEFORE-FLASH-RU.md').write_text('''# Hunter preview: ESP32-S3

В одной прошивке доступны Classic R2 и Hunter Reference.

safe/ — диагностический обмен выключен.
live/ — UART/L9637D, только существующий R2-жгут.
usb-kkl/ — USB Host / FT232RL KKL, отдельное USB-подключение.
Не объединяйте UART/ARM-жгут с USB-KKL проводкой; используйте инструкции своей конфигурации.
KKL не является загрузчиком ESP32: сервисная прошивка дисплея идёт через его CH343/J2.

Первое включение: Classic. Настройки → Стиль Reference → «Reference: заводской вид»
даёт новую композицию; отдельные экраны сохраняют свой стиль.

Для обновления работающего прибора сначала экспортируйте JSON и сохраните полный backup Flash.
Предпочтительное OTA — только bk_mikas_r2.bin выбранного режима. OTA сохраняет старую таблицу:
custom-фоны и большие профили потребуют отдельной миграции из docs/hunter-background-migration.md.
Не стирайте Flash, NVS, OTA data или текущие профили для включения нового UI.

Каждая папка содержит app, bootloader, partition_table, ota_data_initial и flasher_args.json.
Адреса для полного первичного программирования находятся в flash_files этого JSON.
Существующий прибор обновляйте по инструкции миграции, а не полным factory-flash поверх NVS.

Образы экспериментальные; физическая проверка питания/сенсора/ЭБУ/кабеля не выполнена.
Build source и CI указаны в BUILD-METADATA.json, контрольные суммы — SHA256SUMS.txt.
''',encoding='utf-8')
    finish(fw,'BK-MIKAS-Hunter-ESP32S3-preview.zip',dict(kind='esp32s3-firmware',modes=['safe','live','usb-kkl']))
