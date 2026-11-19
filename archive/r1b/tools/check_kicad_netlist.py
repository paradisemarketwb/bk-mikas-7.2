#!/usr/bin/env python3
"""Check the real KiCad CLI-exported netlist against R1B pin-to-net requirements.

This is independent of KiCad's ERC (which checks electrical-rule conflicts).
It catches swapped connector pins, accidentally bridged nets, and omitted nets.
"""
from __future__ import annotations
from pathlib import Path
import re
import sys

EXPECTED = {
    "ACC_PROT": {"J1.1", "U1.7", "C2.1"},
    "GND": {"J1.2", "J2.2", "J3.2", "U1.5", "U2.3", "C1.2", "C2.2", "C3.2"},
    "K_LINE_IN": {"J1.3", "JP1.1"},
    "K_TO_CHIP": {"JP1.2", "U1.6"},
    "5V": {"J3.1", "J2.1"},
    "3V3": {"J2.3", "U1.3", "U2.5", "R3.1", "C1.1", "C3.1"},
    "ESP_TX": {"J2.4", "R1.1"},
    "U2_INPUT": {"R1.2", "U2.2"},
    "TX_GATE_ENABLE": {"J2.6", "U2.1", "R3.2"},
    "U1_TX": {"U2.4", "U1.4"},
    "U1_RX": {"U1.1", "R2.1"},
    "ESP_RX": {"R2.2", "J2.5"},
}

def parse_sexp(src: str):
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|[()]|[^\s()]+', src)
    pos = 0
    def parse():
        nonlocal pos
        if pos >= len(tokens):
            raise ValueError("Unexpected end of S-expression")
        tok = tokens[pos]
        pos += 1
        if tok == "(":
            result = []
            while pos < len(tokens) and tokens[pos] != ")":
                result.append(parse())
            if pos >= len(tokens):
                raise ValueError("Unclosed S-expression")
            pos += 1
            return result
        if tok == ")":
            raise ValueError("Unmatched closing parenthesis")
        if tok.startswith('"') and tok.endswith('"'):
            return bytes(tok[1:-1], "utf-8").decode("unicode_escape") if "\\\"" in tok else tok[1:-1]
        return tok
    result = parse()
    if pos != len(tokens):
        raise ValueError("Trailing tokens")
    return result

def children(obj, name):
    return [x for x in obj if isinstance(x,list) and x and x[0] == name]

def field(obj, name):
    f=children(obj, name)
    if not f or len(f[0]) < 2:
        raise ValueError(f"Missing {name} in {obj[:2]}")
    return f[0][1]

def analyze(path):
    tree=parse_sexp(Path(path).read_text(encoding="utf-8"))
    if tree[0] != "export":
        raise ValueError(f"Unexpected KiCad netlist format: {tree[0]}")
    blocks=children(tree,"nets")
    if len(blocks)!=1:
        raise ValueError("Missing nets section")
    actual={}
    seen={}
    for net in children(blocks[0],"net"):
        name=field(net,"name").lstrip("/")  # KiCad adds root sheet prefix to local labels
        pins=set()
        for node in children(net,"node"):
            contact=f"{field(node,'ref')}.{field(node,'pin')}"
            if contact in seen:
                raise ValueError(f"Duplicate {contact}: {seen[contact]} and {name}")
            seen[contact]=name
            pins.add(contact)
        actual[name]=pins
    for name, required in EXPECTED.items():
        if name not in actual:
            raise AssertionError(f"Missing electrical net {name}")
        missing=required-actual[name]
        if missing:
            raise AssertionError(f"{name}: missing {sorted(missing)}, got {sorted(actual[name])}")
        unexpected={x for x in actual[name] if not x.startswith("#FLG")}-required
        if unexpected:
            raise AssertionError(f"{name}: unexpected contacts {sorted(unexpected)}")
    if "J1.3" in actual["K_TO_CHIP"]:
        raise AssertionError("JP1 has been bypassed: original bus tap must be separated")
    print(f"PASS: validated {len(EXPECTED)} R1B electrical nets; {len(seen)} total pins")

if __name__ == "__main__":
    analyze(sys.argv[1] if len(sys.argv)>1 else "build/kicad/bk-kline-r1b.net")
