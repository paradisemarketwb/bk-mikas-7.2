/* Optional protective case for third-party 74.5 x 38 mm K-Line PCB.
   NOT a verified production housing; measure parts including tall modules,
   connectors and required strain relief. For OpenSCAD: select part and F6.
*/
$fn=64;
part="base"; // "base", "lid", "layout"
board_x=74.5;
board_y=38.0;
board_margin=1.2;
wall=2.2;
floor_th=2.3;
inner_x=board_x+board_margin*2;
inner_y=board_y+board_margin*2;
outer_x=inner_x+2*wall;
outer_y=inner_y+2*wall;
height=29;            // adjust to installed components
boss_h=5.0;           // board floor clearance
boss_d=7.0;
pilot_d=2.5;          // M3 self-tap test hole, adjust screw choice
lid_th=2.4;
lid_lip_h=3.2;
lid_clearance=0.35;
hole_inset=3.5;      // reference Gerber indicates unplated M3 holes at 3.5 from corners
slot_w=12;
slot_h=8;
module base(){
    difference(){
        union(){
            difference(){
                cube([outer_x,outer_y,height]);
                translate([wall,wall,floor_th])
                    cube([inner_x,inner_y,height-floor_th+0.1]);
                // wire entrance opposite sides; customize by actual connector orientation
                translate([outer_x-wall-0.1,(outer_y-slot_w)/2,6])
                    cube([wall+0.2,slot_w,slot_h]);
                translate([-0.1,(outer_y-slot_w)/2,6])
                    cube([wall+0.2,slot_w,slot_h]);
            }
            for(x=[hole_inset,board_x-hole_inset],y=[hole_inset,board_y-hole_inset])
                translate([wall+board_margin+x,wall+board_margin+y,floor_th])
                    cylinder(d=boss_d,h=boss_h);
        }
        for(x=[hole_inset,board_x-hole_inset],y=[hole_inset,board_y-hole_inset])
            translate([wall+board_margin+x,wall+board_margin+y,floor_th-0.01])
                cylinder(d=pilot_d,h=boss_h+0.3);
    }
}
module lid(){
    union(){
        cube([outer_x,outer_y,lid_th]);
        translate([wall+lid_clearance,wall+lid_clearance,lid_th])
            difference(){
                cube([inner_x-2*lid_clearance,inner_y-2*lid_clearance,lid_lip_h]);
                translate([1.8,1.8,-0.1])
                    cube([inner_x-2*lid_clearance-3.6,inner_y-2*lid_clearance-3.6,lid_lip_h+0.2]);
            }
    }
}
if(part=="base") base();
else if(part=="lid") lid();
else {base();translate([outer_x+10,0,0])lid();}
