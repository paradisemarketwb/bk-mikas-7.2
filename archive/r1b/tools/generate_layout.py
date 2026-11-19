#!/usr/bin/env python3
"""BK-MIKAS original screen declarative compiler; no third-party code.
Edit ui/screens.json and ui/metrics.json, run this script from repo root.
Generates one JS data source and one C descriptor header; no ECU code touched.
"""
from __future__ import annotations
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[1]
SCREENS = ROOT / "ui/screens.json"
METRICS = ROOT / "ui/metrics.json"
OUTPUT_JS = ROOT / "ui/screens.generated.js"
OUTPUT_C = ROOT / "firmware/components/ui_layout/generated/ui_layout_defaults.h"
ID = re.compile(r"^[a-z][a-z0-9_]{0,30}$")
HEX = re.compile(r"^#[0-9a-fA-F]{6}$")
LAYOUTS = {"arc3", "dashboard", "trip", "diagnostics"}
def fail(why):
    raise ValueError("screen compiler: " + why)
def valid_id(value, what):
    if not isinstance(value, str) or not ID.fullmatch(value):
        fail(f"{what} must match [a-z][a-z0-9_]{{0,30}}")
def clean_str(value, what, maxlen=100):
    if not isinstance(value, str) or not 0 < len(value) <= maxlen:
        fail(f"{what} must be a non-empty string with <= {maxlen} chars")
    if any(ord(ch) < 32 for ch in value):
        fail(f"{what} has control chars")
    return value
def cstr(s):
    # UTF-8 string literal preserving non-ASCII screen titles
    return json.dumps(s, ensure_ascii=False)
def validate(data, metrics):
    if data.get("version") != 1 or metrics.get("version") != 1:
        fail("schema version must be 1")
    ms = metrics.get("metrics")
    screens = data.get("screens")
    if not isinstance(ms, dict) or not ms:
        fail("metrics is empty")
    if not isinstance(screens, list) or not 1 <= len(screens) <= 12:
        fail("screens count must be 1-12")
    for metric, desc in ms.items():
        valid_id(metric, "metric id")
        clean_str(desc.get("label"), metric + ".label")
        if not isinstance(desc.get("unit"), str):
            fail(metric + ".unit must be a string")
        if len(desc["unit"]) > 24:
            fail(metric + ".unit too long")
    ids=set()
    for s in screens:
        valid_id(s.get("id"), "screen id")
        if s["id"] in ids:
            fail("duplicate screen " + s["id"])
        ids.add(s["id"])
        clean_str(s.get("title"), s["id"] + ".title", 60)
        clean_str(s.get("caption"), s["id"] + ".caption", 120)
        if s.get("layout") not in LAYOUTS:
            fail(s["id"] + " unsupported layout")
        if not HEX.fullmatch(str(s.get("accent"))):
            fail(s["id"] + " invalid hex color")
        if not isinstance(s.get("enabled"), bool):
            fail(s["id"] + ".enabled must be bool")
        for slot in ["main","left","right"]:
            if s.get(slot) not in ms:
                fail(s["id"] + " unknown metric in " + slot)
    if data.get("start_screen") not in ids:
        fail("unknown start_screen")
    return screens
def main():
    data=json.loads(SCREENS.read_text(encoding="utf-8"))
    metricdata=json.loads(METRICS.read_text(encoding="utf-8"))
    screens=validate(data,metricdata)
    public={"version":1,"start_screen":data["start_screen"],"screens":screens,
            "metrics":metricdata["metrics"]}
    OUTPUT_JS.write_text("// GENERATED: python3 tools/generate_layout.py\n"
        "window.BK_LAYOUT = "+json.dumps(public,ensure_ascii=False,indent=2)+";\n",
        encoding="utf-8")
    rows=[]
    for s in screens:
        rows.append("    {"+", ".join([
            cstr(s["id"]),cstr(s["title"]),cstr(s["layout"]),
            cstr(s["accent"]),cstr(s["main"]),cstr(s["left"]),cstr(s["right"]),
            cstr(s["caption"]),"true" if s["enabled"] else "false"])+ "},")
    header = """/* GENERATED FROM ui/screens.json BY tools/generate_layout.py
   Data only; generic ESP32-S3 UI renderer reads these descriptors. */
#ifndef BK_UI_LAYOUT_DEFAULTS_H
#define BK_UI_LAYOUT_DEFAULTS_H
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    const char *id, *title, *layout, *accent;
    const char *main_metric, *left_metric, *right_metric, *caption;
    bool enabled;
} bk_screen_desc_t;
static const bk_screen_desc_t bk_default_screens[] = {
""" + "\n".join(rows) + """
};
static const size_t bk_default_screens_len =
    sizeof(bk_default_screens)/sizeof(bk_default_screens[0]);
#define BK_DEFAULT_START_SCREEN """+ cstr(data["start_screen"])+"""
#endif
"""
    OUTPUT_C.parent.mkdir(parents=True,exist_ok=True)
    OUTPUT_C.write_text(header,encoding="utf-8")
    print(f"Generated {len(screens)} screens, {len(metricdata['metrics'])} metrics")
if __name__=="__main__":
    main()
