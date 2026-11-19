#!/usr/bin/env python3
"""Render harness/architecture SVG from documented connector data, not PCB artwork.
When --check is supplied, verify every PCB endpoint against fresh KiCad XML.
"""
import argparse
import json
from html import escape
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
DATA = json.loads((ROOT / 'docs/wiring/harness.json').read_text())
OUT = ROOT / 'docs/images/system'
OUT.mkdir(parents=True, exist_ok=True)

def text(x, y, words, size=18, color='#173b2c'):
    return f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}">{escape(words)}</text>'

def box(x, y, w, h, title, lines, color='#e6f3eb'):
    s = f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="18" fill="{color}" stroke="#348363" stroke-width="2"/>'
    s += text(x + 16, y + 32, title, 23)
    for i, line in enumerate(lines):
        s += text(x + 16, y + 64 + i * 28, line)
    return s

def arrow(x1, y1, x2, y2, color='#348363'):
    return f'<path d="M{x1} {y1} L{x2} {y2}" fill="none" stroke="{color}" stroke-width="3" marker-end="url(#arrow)"/>'

def svg(body, width=1440, height=900):
    return f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}"><defs><marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0 0 L10 5 L0 10z" fill="#348363"/></marker></defs><rect width="100%" height="100%" fill="#f6f8f3"/><g font-family="DejaVu Sans,Arial,sans-serif">{body}</g></svg>'

def check():
    boards = {}
    for key, filename in DATA['boards'].items():
        boards[key] = {(node.attrib['ref'], node.attrib['pin']): net.attrib['name'].lstrip('/')
                       for net in ET.parse(ROOT / filename).findall('.//nets/net')
                       for node in net.findall('node')}
    def endpoint(ep):
        board, ref, pin = ep
        assert (ref, pin) in boards[board], f'Unknown harness endpoint {ep}'
        return boards[board][ref, pin]
    for link in DATA['links']:
        a, b = endpoint(link['from']), endpoint(link['to'])
        expected = link['signal']
        allowed = {'5V_OUT': {'5V', '+5V', '5V_OUT'}}.get(expected, {expected})
        assert a in allowed and b in allowed, (link, a, b)
    for ep in DATA['external']:
        endpoint([ep['board'], ep['ref'], ep['pin']])
    for ep in DATA['display']:
        endpoint(['kline', 'J2', ep['pin']])
    print('PASS: harness endpoints and four inter-board links match KiCad XML')

def draw():
    s = text(35, 45, 'HUNTER R2 · архитектура', 32)
    s += box(35, 95, 310, 160, 'Автомобиль', ['Микас 7.2 / monitor 0B', 'K-Line 9600, 8N1', 'ACC после зажигания', 'Штатная диагностика сохранена'])
    s += box(405, 95, 460, 160, 'Собственные платы R2', ['BK-POWER: reverse / OV / UV / 5V', 'USB ideal-diode ORing', 'BK-KLINE: L9637D / clamp / JP1', 'TX_ARM GPIO19, UART GPIO43/44'])
    s += box(925, 95, 470, 160, 'Waveshare ESP32-S3', ['ST7701 / CST820 / 480×480', 'RGB565 / PSRAM / PWM', 'Единая LVGL-задача', 'Native USB с ARM запрещён'])
    s += arrow(345, 165, 405, 165) + arrow(865, 165, 925, 165)
    s += box(35, 355, 310, 145, 'Протокол', ['Один worker UART', 'Эхо / checksum / таймаут', 'Только чтение / reconnect'])
    s += box(405, 355, 460, 145, 'Телеметрия и история', ['Снимки / состояния / возраст', '180 точек RAM, 64 DTC-события', 'Редкая запись событий в NVS'])
    s += box(925, 355, 470, 145, 'Общий renderer', ['8 приборов × 8 виджетов', 'Профили JSON / NVS', 'Native simulator и ESP32'])
    s += arrow(345, 420, 405, 420) + arrow(865, 420, 925, 420)
    s += box(405, 610, 460, 155, 'Телефон без интернета', ['WPA2 + HTTP Authentication', 'Конструктор / import / export', 'OTA: hash + ESP image / project', 'ЭБУ не принимает команды из web'])
    s += arrow(635, 500, 635, 610) + arrow(925, 490, 865, 610)
    s += text(35, 850, 'Архитектура программных и электрических связей. Не заменяет схемы KiCad и аппаратные испытания.', 18)
    (OUT / 'architecture.svg').write_text(svg(s), encoding='utf-8')
    s = text(35, 45, 'HUNTER R2 · межплатная проводка', 32)
    s += box(35, 95, 410, 245, 'Входы BK-POWER-R2', [ep['name'] for ep in DATA['external'] if ep['board'] == 'power'])
    s += box(35, 420, 410, 180, 'BK-POWER J2 → BK-KLINE', [f"{v['from'][1]}.{v['from'][2]} → {v['to'][1]}.{v['to'][2]} : {v['signal']}" for v in DATA['links']])
    s += box(515, 95, 390, 245, 'BK-KLINE-R2', ['J1.1 VS_PROT / J1.2 GND', 'J1.3 ответвление K-Line', 'J3.1 5V / J3.2 GND', 'JP1 снят при прошивке', 'Не перерезать штатную K-Line'])
    s += box(965, 95, 430, 280, 'J2 интерфейса → дисплей', [f"{v['pin']}: {v['label']}" for v in DATA['display']])
    s += arrow(445, 210, 515, 210) + arrow(905, 210, 965, 210) + arrow(445, 490, 515, 300)
    s += box(515, 450, 880, 195, 'Waveshare original2.1: два порта, U10 переключает UART автоматически', ['Работа: питание вJ9.2 USB_5V, обаType-C свободны.', 'Сервис: JP1 снят, PC → UARTType-CJ2/CH343; после снятькабель.', 'Не подавать постоянные5V вJ2: VBUS отключает внешнийUART.', 'NativeJ3.VBUS = J9.2: PC при внешних5V туда не подключать.'], '#fff1db')
    s += text(35, 820, 'Сигналы/номера берутся из docs/wiring/harness.json; --check сверяет выводы со схемными netlist.', 18)
    s += text(35, 855, 'Pin 1 наших JST не означает pin 1 разъёма Waveshare. Полные схемы и номиналы — только в KiCad.', 18)
    (OUT / 'wiring.svg').write_text(svg(s), encoding='utf-8')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if args.check:
        check()
    draw()
