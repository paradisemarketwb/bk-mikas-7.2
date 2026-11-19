// BK-HUNTER R2 gauge; Waveshare ESP32-S3-Touch-LCD-2.1 ORIGINAL (not2.1B).
// Dimensions taken from official structure.zip / mechanical PDF, 2024-07-11.
// Lens75±.15, active53.88±.15, overall module depth9.5, PCB width66,
// 4 M2 mounts on65 pitch circle: top x±29.5; bottom x±18.
// Thread depth and connector bend space require physical fit check.
$fn=160;
part="body"; // body | rear | spacer | assembled | exploded | dimensioned
lens_d=75; lens_tolerance=.15; active_d=53.88;
outer_d=88; cavity_d=76.4; aperture_d=55.2; body_depth=28;
front_lip=1.8; face_airgap=.6; rear_th=2.6;
module_front_z=front_lip+face_airgap;
mount_plane_z=module_front_z+5.5;
board_hole_r=32.5;
mounts=[[-29.5,sqrt(board_hole_r*board_hole_r-29.5*29.5)],
        [29.5,sqrt(board_hole_r*board_hole_r-29.5*29.5)],
        [-18,-sqrt(board_hole_r*board_hole_r-18*18)],
        [18,-sqrt(board_hole_r*board_hole_r-18*18)]];
case_screws=[for(a=[45:90:315])[40.3*cos(a),40.3*sin(a)]];
assert(cavity_d>lens_d+lens_tolerance+.5);
module body(){difference(){union(){difference(){cylinder(d=outer_d,h=body_depth);translate([0,0,front_lip])cylinder(d=cavity_d,h=body_depth+1);translate([0,0,-.1])cylinder(d=aperture_d,h=front_lip+.2);}for(h=case_screws)translate([h.x,h.y,14])cylinder(d=5.4,h=body_depth-14);}for(h=case_screws)translate([h.x,h.y,14.5])cylinder(d=2.1,h=body_depth);}}
module rear(){difference(){union(){cylinder(d=outer_d,h=rear_th);translate([0,0,rear_th-.1])difference(){cylinder(d=cavity_d-.6,h=2.5);cylinder(d=cavity_d-4.6,h=2.6);}for(h=mounts)translate([h.x,h.y,rear_th-.1])cylinder(d=5.6,h=body_depth-mount_plane_z+.1);}for(h=mounts)translate([h.x,h.y,-.1])cylinder(d=2.3,h=rear_th+body_depth);for(h=case_screws)translate([h.x,h.y,-.1])cylinder(d=2.8,h=rear_th+1);translate([0,-14,-.1])cylinder(d=12,h=rear_th+1);for(x=[-25,-18,18,25])translate([x,-4,-.1])hull(){cylinder(d=2.5,h=rear_th+1);translate([0,10,0])cylinder(d=2.5,h=rear_th+1);}}}
module spacer(){difference(){cylinder(d=lens_d+.3,h=.5);cylinder(d=69,h=.6);}}
module module_envelope(){color([.08,.08,.09])translate([0,0,module_front_z])cylinder(d=75,h=.7);color([.1,.4,.15])translate([0,0,mount_plane_z])difference(){cylinder(d=66,h=1.6);for(h=mounts)translate([h.x,h.y,-.1])cylinder(d=2,h=2);}color([.2,.5,.8])translate([0,0,module_front_z-.05])cylinder(d=53.88,h=.1);}
module draw_assembly(explode=0){color([.16,.18,.21,explode>0?.35:1])body();translate([0,0,explode])module_envelope();color([.24,.27,.30])translate([0,0,body_depth+rear_th+2*explode])rotate([180,0,0])rear();}
if(part=="body")body();else if(part=="rear")rear();else if(part=="spacer")spacer();else if(part=="assembled")draw_assembly();else if(part=="exploded")draw_assembly(30);else {body();translate([outer_d+10,0,0])rear();}
