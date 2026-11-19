/*
  BK-KLINE-R1 original PCB protection case, REV R1, NOT tested.
  PCB outer size, connector placement and standoff positions MUST be
  checked after KiCad layout. Intended for small U1/R1/R2/C1/C2 board
  with protected automobile 5V DC/DC positioned OUTSIDE this case.
  Unit mm. Change part and export an STL separately.
*/
$fn=48;
part="base"; // "base" | "lid" | "layout"

// provisional PCB size — not a promise that this PCB has already been routed
pcb_x=36;
pcb_y=26;
pcb_z=1.6;
free_edge=2.0;
wall=2.4;
floor_h=2.4;
inside_h=18.0;
lid_th=2.0;
lid_lip_h=2.4;
lid_slop=0.4;
support_rail_w=2.0;
support_rail_h=3.5;
cable_slot_w=9.0;
cable_slot_h=8.5;

inner_x=pcb_x+2*free_edge;
inner_y=pcb_y+2*free_edge;
box_x=inner_x+2*wall;
box_y=inner_y+2*wall;

module base(){
    union(){
        difference(){
            cube([box_x,box_y,inside_h+floor_h]);
            translate([wall,wall,floor_h])
                cube([inner_x,inner_y,inside_h+0.1]);
            // Grommet openings: confirm actual wire direction before printing
            translate([-0.1,(box_y-cable_slot_w)/2,floor_h+4])
                cube([wall+0.2,cable_slot_w,cable_slot_h]);
            translate([box_x-wall-0.1,(box_y-cable_slot_w)/2,floor_h+4])
                cube([wall+0.2,cable_slot_w,cable_slot_h]);
        }
        // PCB edge rails, no guessed hole locations
        translate([wall+free_edge,wall+free_edge,floor_h])
            cube([support_rail_w,pcb_y,support_rail_h]);
        translate([wall+free_edge+pcb_x-support_rail_w,wall+free_edge,floor_h])
            cube([support_rail_w,pcb_y,support_rail_h]);
    }
}
module lid(){
    union(){
        cube([box_x,box_y,lid_th]);
        translate([wall+lid_slop,wall+lid_slop,lid_th])
            difference(){
                cube([inner_x-2*lid_slop,inner_y-2*lid_slop,lid_lip_h]);
                translate([2,2,-0.1])
                    cube([inner_x-2*lid_slop-4,inner_y-2*lid_slop-4,lid_lip_h+0.2]);
            }
    }
}
if(part=="base") base();
else if(part=="lid") lid();
else if(part=="layout"){
    base();
    translate([box_x+8,0,0]) lid();
}
