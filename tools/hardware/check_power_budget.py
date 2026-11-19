#!/usr/bin/env python3
"""Reproducible worst-case DC/defined-pulse arithmetic, not EMC certification."""
import json,math
from pathlib import Path
r1min=10000*.99;vzmin=62*.98;vin=200;duration=.350
ov_nom=1.231*(1+100000/7500);uv_nom=1.231*(1+100000/18000)
ilim_max=22980/((22.1*.99)**.94)/1000
r={'assumed_envelope':{'vehicle_nominal_V':12,'regulated_input_V':[9,16],'positive_pulse_V':200,'positive_pulse_s':duration,'minimum_pulse_interval_s':60,'maximum_ambient_C':70,'continuous_5V_load_A':1.0,'usb2_host_budget_A':.5,'usb3_host_budget_A':.9},
'ov_nominal_V':ov_nom,'ov_tolerance_range_V':[1.195*(1+99000/7575),1.267*(1+101000/7425)],'uv_rising_nominal_V':uv_nom,'uv_rising_tolerance_range_V':[1.195*(1+99000/18180),1.267*(1+101000/17820)],'uv_falling_nominal_V':1.132*(1+100000/18000),
'R1_worst_200V_pulse_W':(vin-vzmin)**2/r1min,'R1_worst_pulse_J':((vin-vzmin)**2/r1min)*duration,'R1_pulse_average_at_60s_interval_W':((vin-vzmin)**2/r1min)*duration/60,
'R2_200V_W':(200-200*7500/107500)**2/100000,'R4_200V_W':(200-200*18000/118000)**2/100000,'D1_62V_nominal_pulse_W':62*(200-62)/10000,
'converter_input_A_at_8V_85pct_efficiency':5/(8*.85),'converter_input_A_at_9V_85pct_efficiency':5/(9*.85),'Q1_loss_at_9V_with_hot_Rds_1ohm_W':(5/(9*.85))**2,
'current_limit_max_including_1pct_R_A':ilim_max,'current_limit_nominal_A':23950/(22.1**.977)/1000,'current_limit_min_including_1pct_R_A':25230/((22.1*1.01)**1.016)/1000,'LM66100_continuous_rating_A':1.5}
assert r['R1_worst_200V_pulse_W']<2.0
assert r['R2_200V_W']<.75 and r['R4_200V_W']<.75
assert 1<r['current_limit_min_including_1pct_R_A']<ilim_max<1.5
assert r['ov_tolerance_range_V'][1]<32 and r['converter_input_A_at_9V_85pct_efficiency']<2
p=Path(__file__).resolve().parents[2]/'hardware/kicad/power-r2/reports/power-budget.json';p.parent.mkdir(exist_ok=True,parents=True);p.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
