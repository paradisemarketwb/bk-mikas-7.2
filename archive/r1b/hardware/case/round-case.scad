/* BK MIKAS 7.2 — PARAMETRIC ROUND HOUSING / geometric draft only.
   This is NOT a tested Waveshare mount. Measure the purchased 2.1inch module
   and cables before printing, especially glass/PCB dimensions and USB ports.
   Units: millimeters. Open in OpenSCAD; F6 then Export STL.
*/
$fn = 192;
part = "body";         // "body", "cap", "exploded"
module_od = 75.0;     // nominal outer face size, verify physically
clearance = 0.7;      // radial allowance, revise after first test
outer_d = 82.0;       // face ring OD, subject to dashboard clearance
face_aperture = 55.0;// visible screen area; verify active circle and bezel
body_depth = 25.0;
lip_thickness = 1.6;
back_opening = 22.0;  // cable pass-through; NOT USB service access
cap_height = 3.5;
fit_slop = 0.35;
inner_d = module_od + 2*clearance;
wall = (outer_d-inner_d)/2;
assert(wall > 1.0, "Wall too thin; increase outer_d or reduce clearance.");
assert(face_aperture < inner_d-2, "Front aperture cannot exceed cavity.");

module body() {
  difference(){
    cylinder(d=outer_d,h=body_depth);
    // Rear accessible cavity starts behind face ring.
    translate([0,0,lip_thickness]) cylinder(d=inner_d,h=body_depth+0.2);
    // Front window only, preserving a lip to hold panel from rear.
    translate([0,0,-0.2]) cylinder(d=face_aperture,h=lip_thickness+0.5);
    // Rear exit for cable; keep open in this draft.
    translate([0,0,body_depth-4]) cylinder(d=back_opening,h=5);
  }
}
module back_cap() {
  difference(){
    cylinder(d=outer_d,h=cap_height);
    translate([0,0,-0.1]) cylinder(d=back_opening,h=cap_height+0.2);
    // For a future version, add actual locking screw bosses based on measured PCB
  }
}
if(part=="body") body();
else if(part=="cap") back_cap();
else {body();translate([outer_d+8,0,0])back_cap();}
