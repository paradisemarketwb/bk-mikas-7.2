// Combined BK-POWER-R2 (90x70) and BK-KLINE-R2 (65x52) serviceable enclosure.
// Separate power and UART boards, upper interface fixed to removable lid.
// mm. Cables use strain-relief ties; ports do not expose a direct unprotected USB.
$fn=64;part="base"; // base | lid | assembled | exploded
wall=2.4;clearance=3;floor=2.4;height=50;lid_th=2.6;
pcb=[90,70,1.6];inner=[96,76];outer=[100.8,80.8];off=wall+clearance;
power_holes=[[3,42],[87,3],[3,67],[87,67]];
interface_offset=[10,8];interface_holes=[[3,24],[62,3],[3,49],[62,49]];
case_holes=[[3.2,3.2],[outer.x-3.2,3.2],[3.2,outer.y-3.2],[outer.x-3.2,outer.y-3.2]];
module base(){difference(){union(){difference(){cube([outer.x,outer.y,height]);translate([wall,wall,floor])cube([inner.x,inner.y,height]);}for(h=power_holes)translate([off+h.x,off+h.y,floor-.1])cylinder(d=6,h=5.1);for(h=case_holes)translate([h.x,h.y,15])cylinder(d=5.6,h=height-15);for(x=[8,outer.x-12])translate([x,outer.y/2-10,floor-.1])cube([4,20,4]);}for(h=power_holes)translate([off+h.x,off+h.y,floor+1])cylinder(d=2.0,h=6);for(h=case_holes)translate([h.x,h.y,16])cylinder(d=2.5,h=height);for(x=[-1,outer.x-wall-1])translate([x,outer.y/2-8,12])cube([wall+2,16,height]);for(x=[7,outer.x-13])translate([x,outer.y/2-7,floor+1])cube([6,14,1.6]);}}
module lid(){difference(){union(){cube([outer.x,outer.y,lid_th]);translate([wall+.35,wall+.35,lid_th-.1])difference(){cube([inner.x-.7,inner.y-.7,2.5]);translate([2,2,-.1])cube([inner.x-4.7,inner.y-4.7,2.7]);}for(h=interface_holes)translate([off+interface_offset.x+h.x,off+interface_offset.y+h.y,lid_th-.1])cylinder(d=6,h=13.1);}for(h=case_holes)translate([h.x,h.y,-.1])cylinder(d=3.2,h=lid_th+1);for(h=interface_holes)translate([off+interface_offset.x+h.x,off+interface_offset.y+h.y,lid_th+1])cylinder(d=2.0,h=14);for(x=[25:7:75])translate([x,8,-.1])cube([2,10,lid_th+.2]);}}
module boards(explode=0){color([.1,.42,.22])translate([off,off,floor+5])difference(){cube(pcb);for(h=power_holes)translate([h.x,h.y,-.1])cylinder(d=2.7,h=2);}color([.12,.15,.19])translate([off+62.8,off+27.9,floor+6.6])cube([11.5,8.5,17.5]);color([.1,.42,.22])translate([off+10,off+8,height-13-1.6+explode])difference(){cube([65,52,1.6]);for(h=interface_holes)translate([h.x,h.y,-.1])cylinder(d=2.7,h=2);}}
if(part=="base")base();else if(part=="lid")lid();else{color([.18,.2,.23,.3])base();boards(part=="exploded"?25:0);color([.25,.28,.32])translate([0,outer.y,height+lid_th+(part=="exploded"?45:0)])rotate([180,0,0])lid();}
