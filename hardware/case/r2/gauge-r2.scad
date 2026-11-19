/*
  BK-MIKAS 7.2  — ORIGINAL ROUND GAUGE ENCLOSURE, REV R2 / FIT-CHECK ONLY
  Source: original project. Design intended for Waveshare ESP32-S3-Touch-LCD-2.1.
  Inherited concept geometry only; do not print an installation part until actual module
  and 12-pin flex/USB ports are measured. No brackets: fitment handled later.
  Units: mm.  OpenSCAD -> change part -> F6 -> Export STL.
*/
$fn=180;
part="body"; // "body" | "rear" | "spacer" | "layout"

// TO MEASURE ON RECEIVED UNIT (these are nominal draft values)
glass_outer_d = 75.0;        // outside of display glass, NOT active 2.1-inch field
front_aperture_d = 54.0;     // keep entire active display visible, confirm on real unit
outer_d = 85.0;              // total cylinder diameter in dash
radial_clearance = 0.85;     // clearance for display + pads + printer tolerance
length = 34.0;               // rear enclosure depth; test access to connectors
front_lip = 1.9;             // front metal-look holding lip
rear_plate_thickness = 2.5;
rear_insert_length = 4.5;
rear_insert_slop = 0.45;
rear_cable_hole_d = 14.0;   // strain relief must be designed after cable check
spacer_th = 1.8;
spacer_inner_d = 61.0;

cavity_d=glass_outer_d + 2*radial_clearance;
wall=(outer_d-cavity_d)/2;
assert(wall >= 2.0, "Increase OD or reduce clearance; unsafe thin wall.");
assert(front_aperture_d < glass_outer_d - 1.0, "Front aperture too wide.");
assert(spacer_inner_d < cavity_d - 2.0, "Spacer needs supporting ring.");

module gauge_body(){
    difference(){
        cylinder(d=outer_d,h=length);
        // Front glass holding rim
        translate([0,0,-0.1])
            cylinder(d=front_aperture_d,h=front_lip+0.25);
        // Inner cavity for glass and Waveshare PCB
        translate([0,0,front_lip])
            cylinder(d=cavity_d,h=length-front_lip+0.3);
        // Rear relief cutout; separate cap covers it
        translate([0,0,length-6])
            cylinder(d=rear_cable_hole_d,h=6.2);
    }
}
module rear_cap(){
    difference(){
        union(){
            cylinder(d=outer_d,h=rear_plate_thickness);
            translate([0,0,rear_plate_thickness])
                cylinder(d=cavity_d-2*rear_insert_slop,h=rear_insert_length);
        }
        translate([0,0,-0.2])
            cylinder(d=rear_cable_hole_d,h=rear_plate_thickness+rear_insert_length+0.5);
        // Shallow relief on inside to avoid sharp pressure points
        translate([0,0,rear_plate_thickness+0.8])
            cylinder(d=rear_cable_hole_d+8,h=rear_insert_length);
    }
}
module foam_spacer_template(){
    difference(){
        cylinder(d=cavity_d-0.9,h=spacer_th);
        translate([0,0,-0.2])
            cylinder(d=spacer_inner_d,h=spacer_th+0.4);
    }
}

if(part=="body") gauge_body();
else if(part=="rear") rear_cap();
else if(part=="spacer") foam_spacer_template();
else if(part=="layout"){
    gauge_body();
    translate([outer_d+15,0,0]) rear_cap();
    translate([0,outer_d+15,0]) foam_spacer_template();
}
