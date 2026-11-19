// BK-KLINE-R2, 65x52 PCB; enclosure engineering prototype, mm.
// Exact mounting holes follow PCB; connector/wire bend clearances need physical check.
$fn=64;
part="base"; // base | lid | exploded
pcb=[65,52,1.6]; wall=2.4; clearance=3; floor=2.4; lift=5; height=23;
inner=[pcb.x+2*clearance,pcb.y+2*clearance];
outer=[inner.x+2*wall,inner.y+2*wall];
holes=[[3,24],[62,3],[3,49],[62,49]];
offset=wall+clearance;
module base(){
 difference(){
  union(){
   difference(){cube([outer.x,outer.y,height]);translate([wall,wall,floor])cube([inner.x,inner.y,height]);}
   for(h=holes)translate([offset+h.x,offset+h.y,floor])cylinder(d=6,h=lift);
   // Tie-down bridge slots allow cable ties near each cable outlet.
   for(x=[5,outer.x-9])translate([x,outer.y/2-8,floor])cube([4,16,3]);
  }
  for(h=holes)translate([offset+h.x,offset+h.y,floor+1])cylinder(d=2.0,h=lift+1); // pilot M2.5 self-tap, confirm screw
  for(x=[-1,outer.x-wall-1])translate([x,outer.y/2-7,floor+lift+1])cube([wall+2,14,height]);
  for(x=[4,outer.x-10])translate([x,outer.y/2-5,floor+.8])cube([8,10,1.5]);
 }
}
module lid(){difference(){union(){cube([outer.x,outer.y,2.4]);translate([wall+.35,wall+.35,2.3])difference(){cube([inner.x-.7,inner.y-.7,2.5]);translate([1.8,1.8,-.1])cube([inner.x-4.3,inner.y-4.3,3]);}}for(x=[6:6:outer.x-6])translate([x,8,-1])cube([2,10,4]);}}
if(part=="base")base();else if(part=="lid")lid();else {base();translate([0,0,height+12])rotate([180,0,0])translate([0,-outer.y,0])lid();}
