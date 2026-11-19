# ref, MPN/value, footprint, pins (name, net, electrical type), x, y
R='Resistor_SMD:R_0805_2012Metric';C='Capacitor_SMD:C_0805_2012Metric';SMA='Diode_SMD:D_SMA'; SMB='Diode_SMD:D_SMB';SOT='Package_TO_SOT_SMD:SOT-23';P='passive';I='input';O='output'
parts=[]
def add(ref,val,fp,pins,x,y):parts.append((ref,val,fp,pins,x,y))
def pair(ref,val,fp,a,b,x,y):add(ref,val,fp,[('1',a,P),('2',b,P)],x,y)
add('J1','ACC_IN_FUSED_2A','TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal',[('ACC','ACC_RAW',P),('GND','GND',P)],8,10)
# External inline 2A automotive fuse close to ignition source, not replaceable onboard.
add('Q1','IPD50R380CE','Package_TO_SOT_SMD:TO-252-2',[('G','HGATE',I),('D','ACC_RAW',P),('S','COMMON_SOURCE',P)],30,12)
add('Q2','BUK7Y4R8-60E','Package_TO_SOT_SMD:LFPAK56',[('S','COMMON_SOURCE',P),('S','COMMON_SOURCE',P),('S','COMMON_SOURCE',P),('G','DGATE',I),('D','VS_PROT',P)],49,12)
# Q2 LFPAK has repeated source pads; corrected explicit mapping in build script.
add('U1','LM74800QDRRRQ1','BK_POWER:WSON-12_DRR_3x3_EP1.3x2.5',[('DGATE','DGATE',O),('A','COMMON_SOURCE',I),('VSNS','CTRL_VS',I),('SW','CTRL_VS',P),('OV','OV_SENSE',I),('EN_UVLO','UV_SENSE',I),('GND','GND','power_in'),('HGATE','HGATE',O),('OUT','COMMON_SOURCE',I),('VS','CTRL_VS','power_in'),('CAP','CAP',O),('C','VS_PROT',I),('RTN',None,P)],41,31)
pair('R1','10k 1210 0.5W 200V','Resistor_SMD:R_1210_3225Metric','ACC_RAW','CTRL_VS',20,27)
pair('D1','BZG03C56G','Diode_SMD:D_SMA','CTRL_VS','GND',24,35) # pad1 cathode
pair('D2','BAS21H,115','Diode_SMD:D_SOD-123F','CTRL_VS','COMMON_SOURCE',31,24)
pair('D3','SMBJ150A',SMB,'ACC_RAW','TVS_MID',9,22)
pair('D4','SMBJ33CA',SMB,'GND','TVS_MID',9,29)
pair('C1','100n 250V','Capacitor_SMD:C_1210_3225Metric','ACC_RAW','GND',18,18)
pair('C2','1u 100V X7R','Capacitor_SMD:C_1210_3225Metric','CTRL_VS','GND',31,35)
pair('C3','100n 25V',C,'CTRL_VS','CAP',46,34)
pair('C4','1u 50V',C,'COMMON_SOURCE','GND',39,18)
pair('R2','100k 1% 200V','Resistor_SMD:R_1206_3216Metric','ACC_RAW','OV_SENSE',22,43)
pair('R3','7k5 1%',R,'OV_SENSE','GND',33,43)
pair('R4','100k 1% 200V','Resistor_SMD:R_1206_3216Metric','ACC_RAW','UV_SENSE',23,49)
pair('R5','18k 1%',R,'UV_SENSE','GND',34,49)
pair('D5','SMBJ18A',SMB,'VS_PROT','GND',58,22)
pair('C5','10u 50V X7R','Capacitor_SMD:C_1210_3225Metric','VS_PROT','GND',65,17)
add('U2','R-78B5.0-2.0','Converter_DCDC:Converter_DCDC_RECOM_R-78B-2.0_THT',[('VIN','VS_PROT','power_in'),('GND','GND','power_in'),('5V','5V_CAR','power_out')],66,30)
pair('C6','10u 16V X7R','Capacitor_SMD:C_1206_3216Metric','5V_CAR','GND',65,42)
add('U3','LM66100DCKR','Package_TO_SOT_SMD:SOT-363_SC-70-6',[('VIN','5V_CAR','power_in'),('GND','GND','power_in'),('nCE','5V_OUT',I),('NC',None,P),('nST',None,'open_collector'),('VOUT','5V_OUT',P)],74,45)
pair('C7','100n 16V',C,'5V_CAR','GND',69,49)
pair('C8','10u 16V X7R','Capacitor_SMD:C_1206_3216Metric','5V_OUT','GND',82,46)
add('J2','VS_AND_5V_OUT','Connector_JST:JST_XH_B4B-XH-A_1x04_P2.50mm_Vertical',[('VS_PROT','VS_PROT',P),('GND','GND',P),('5V_OUT','5V_OUT',P),('GND','GND',P)],68,61)
# USB service interposer: host and display VBUS separated electrically.
add('J3','USB_HOST_5V_GND','Connector_JST:JST_XH_B2B-XH-A_1x02_P2.50mm_Vertical',[('VBUS','USB_5V',P),('GND','GND',P)],48,60)
add('U4','LM66100DCKR','Package_TO_SOT_SMD:SOT-363_SC-70-6',[('VIN','USB_5V','power_in'),('GND','GND','power_in'),('nCE','5V_OUT',I),('NC',None,P),('nST',None,'open_collector'),('VOUT','5V_OUT',P)],54,48)
pair('C9','100n 16V',C,'USB_5V','GND',48,54)
# Analog output monitor and test points use plated 1mm testpads, no fake connectors.
for i,(net,x,y) in enumerate([('ACC_RAW',18,8),('VS_PROT',59,8),('5V_CAR',69,37),('5V_OUT',82,54),('GND',14,60)]):add('TP'+str(i+1),net,'TestPoint:TestPoint_Pad_D1.5mm',[(net,net,P)],x,y)
