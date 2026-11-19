import pathlib,uuid,json,csv
import pcbnew as p
root=pathlib.Path(__file__).resolve().parents[2]; out=root/'hardware/kicad/r2'; out.mkdir(parents=True,exist_ok=True)
u=lambda:str(uuid.uuid4())
# ref, value, footprint, [(name, net, type)], PCB x y
parts=[
('J1','VEHICLE_PROTECTED','TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-3-5.08_1x03_P5.08mm_Horizontal',[('VS','VS_PROT','passive'),('GND','GND','passive'),('K','K_BUS','passive')],5,8),
('JP1','REMOVE_FOR_SERVICE','Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical',[('BUS','K_BUS','passive'),('CHIP','K_TAP','passive')],23,8),
('U1','L9637D','Package_SO:SOIC-8_3.9x4.9mm_P1.27mm',[('RX','RX_CHIP','output'),('LO',None,'output'),('VCC','3V3','power_in'),('TX','TX_CHIP','input'),('GND','GND','power_in'),('K','K_CHIP','bidirectional'),('VS','VS_PROT','power_in'),('LI',None,'input')],30,20),
('U2','SN74LVC1G125QDBVRQ1','Package_TO_SOT_SMD:SOT-23-5',[('nOE','N_ENABLE','input'),('A','TX_BUF','input'),('GND','GND','power_in'),('Y','TX_CHIP','tri_state'),('VCC','3V3','power_in')],43,20),
('Q1','2N7002','Package_TO_SOT_SMD:SOT-23',[('G','ARM_GATE','input'),('S','GND','passive'),('D','N_ENABLE','passive')],44,31),
('R1','100R','Resistor_SMD:R_0805_2012Metric',[('1','ESP_TX','passive'),('2','TX_BUF','passive')],49,12),
('R2','1k','Resistor_SMD:R_0805_2012Metric',[('1','RX_CHIP','passive'),('2','ESP_RX','passive')],37,11),
('R3','10k','Resistor_SMD:R_0805_2012Metric',[('1','3V3','passive'),('2','N_ENABLE','passive')],37,28),
('R4','100k','Resistor_SMD:R_0805_2012Metric',[('1','ARM_GATE','passive'),('2','GND','passive')],49,37),
('R5','1k','Resistor_SMD:R_0805_2012Metric',[('1','TX_ARM','passive'),('2','ARM_GATE','passive')],54,30),
('R6','10k','Resistor_SMD:R_0805_2012Metric',[('1','3V3','passive'),('2','TX_CHIP','passive')],37,20),
('C1','100n 16V','Capacitor_SMD:C_0805_2012Metric',[('1','3V3','passive'),('2','GND','passive')],26,28),
('C2','100n 50V','Capacitor_SMD:C_0805_2012Metric',[('1','VS_PROT','passive'),('2','GND','passive')],19,20),
('C3','100n 16V','Capacitor_SMD:C_0805_2012Metric',[('1','3V3','passive'),('2','GND','passive')],43,12),
('J2','DISPLAY','Connector_JST:JST_XH_B6B-XH-A_1x06_P2.50mm_Vertical',[('5V','5V','passive'),('GND','GND','passive'),('3V3','3V3','passive'),('TX43','ESP_TX','passive'),('RX44','ESP_RX','passive'),('ARM19','TX_ARM','passive')],35,43),
('J3','REGULATED_5V','TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal',[('5V','5V','passive'),('GND','GND','passive')],5,35),
]
parts += [('R7','22R pulse 1W','Resistor_SMD:R_1206_3216Metric',[('1','K_TAP','passive'),('2','K_CHIP','passive')],24,14),('D1','SMAJ18A','Diode_SMD:D_SMA',[('K','K_CHIP','passive'),('A','GND','passive')],11,23)]
# generic rectangular symbols retain electrical pin types, datasheet pin numbering
libs=[]; instances=[]; labels=[]
for idx,(ref,val,fp,pins,x,y) in enumerate(parts):
 name=ref; n=len(pins); h=max(5,n*1.27+1.27); sx=35.56+(idx%4)*93.98; sy=35.56+(idx//4)*55.88; uid=u()
 props=f'(property "Reference" "{ref}" (at 0 {-h-3} 0) (effects (font (size 1.27 1.27)))) (property "Value" "{val}" (at 0 {h+3} 0) (effects (font (size 1.27 1.27)))) (property "Footprint" "{fp}" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))'
 pinstr=''
 for j,(pn,net,typ) in enumerate(pins):
  yy=round((n-1)*1.27-j*2.54,2)
  pinstr+=f'(pin {typ} line (at -12.7 {yy} 0) (length 5.08) (name "{pn}" (effects (font (size 1 1)))) (number "{j+1}" (effects (font (size 1 1)))))'
  px=sx-12.7; py=round(sy-yy,2)
  labels.append(f'(label "{net}" (at {px} {py} 0) (effects (font (size 1 1)) (justify right bottom)) (uuid "{u()}"))' if net else f'(no_connect (at {px} {py}) (uuid "{u()}"))')
 libs.append(f'(symbol "BK_R2:{name}" (pin_names (offset 0.5)) (in_bom yes) (on_board yes) {props} (symbol "{name}_0_1" (rectangle (start -7.62 {-h}) (end 7.62 {h}) (stroke (width .254) (type default)) (fill (type background)))) (symbol "{name}_1_1" {pinstr}))')
 instances.append(f'(symbol (lib_id "BK_R2:{name}") (at {sx} {sy} 0) (unit 1) (in_bom yes) (on_board yes) (dnp no) (uuid "{uid}") (property "Reference" "{ref}" (at {sx+12} {sy-3} 0) (effects (font (size 1.27 1.27)))) (property "Value" "{val}" (at {sx+12} {sy} 0) (effects (font (size 1 1)))) (property "Footprint" "{fp}" (at {sx} {sy} 0) (effects (font (size 1 1)) (hide yes))) (instances (project "BK-KLINE-R2" (path "/ROOTUUID" (reference "{ref}") (unit 1)))))')
uid=u(); sch=f'(kicad_sch (version 20231120) (generator "bk_r2") (uuid "{uid}") (paper "A3") (title_block (title "BK-KLINE-R2 - PROTECTED INTERFACE") (rev "R2") (comment 1 "USE BK-POWER-R2; BENCH QUALIFICATION REQUIRED")) (lib_symbols '+''.join(libs)+') '+''.join(labels)+''.join(instances).replace('ROOTUUID',uid)+')'
(out/'BK-KLINE-R2.kicad_sch').write_text(sch)
(out/'BK_R2.kicad_sym').write_text('(kicad_symbol_lib (version 20231120) (generator bk_r2) '+''.join(libs).replace('BK_R2:','')+')')
(out/'sym-lib-table').write_text('(sym_lib_table (lib (name BK_R2) (type KiCad) (uri ${KIPRJMOD}/BK_R2.kicad_sym) (options "") (descr "R2 original symbols")))')


# Explicit external supply declarations at interface connectors. Not a substitute for power design.
text=(out/'BK-KLINE-R2.kicad_sch').read_text()
flag='(symbol "BK_R2:PWR_FLAG" (power) (pin_names (offset 0)) (in_bom no) (on_board no) (property "Reference" "#FLG" (at 0 2.54 0) (effects (font (size 1 1)) (hide yes))) (property "Value" "PWR_FLAG" (at 0 -2.54 0) (effects (font (size 1 1)))) (symbol "PWR_FLAG_0_1" (polyline (pts (xy -1.27 0) (xy 0 1.27) (xy 1.27 0)) (stroke (width .2) (type default)) (fill (type none)))) (symbol "PWR_FLAG_1_1" (pin power_out line (at 0 0 90) (length 0) (name "pwr" (effects (font (size 1 1)))) (number "1" (effects (font (size 1 1)))))))'
text=text.replace('(lib_symbols ','(lib_symbols '+flag+' ',1)
for i,net in enumerate(['3V3','VS_PROT','GND']):
 x=35.56+i*25.4;y=264.16
 text=text[:-1]+f'(symbol (lib_id "BK_R2:PWR_FLAG") (at {x} {y} 0) (unit 1) (in_bom no) (on_board no) (uuid "{u()}") (property "Reference" "#FLG0{i+1}" (at {x} {y+2.54} 0) (effects (font (size 1 1)) (hide yes))) (property "Value" "PWR_FLAG" (at {x} {y-2.54} 0) (effects (font (size 1 1)))) (instances (project "BK-KLINE-R2" (path "/{uid}" (reference "#FLG0{i+1}") (unit 1))))) (label "{net}" (at {x} {y} 0) (effects (font (size 1 1)) (justify left bottom)) (uuid "{u()}")))'
(out/'BK-KLINE-R2.kicad_sch').write_text(text)
lib=(out/'BK_R2.kicad_sym').read_text();(out/'BK_R2.kicad_sym').write_text(lib[:-1]+flag.replace('BK_R2:','')+')')
