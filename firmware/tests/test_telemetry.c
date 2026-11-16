#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "mikas_telemetry.h"
static int near(double a, double b) {return fabs(a-b)<0.00001;}
int main(void){
  uint8_t pair[]={0xF0,0x00};uint16_t raw=0;
  assert(mikas_le16(pair,2,&raw)&&raw==240);
  assert(!mikas_le16(pair,1,&raw)&&!mikas_le16(NULL,2,&raw));
  assert(near(mikas_rpm(20),800));
  assert(near(mikas_voltage(139),13.9));
  assert(near(mikas_celsius(128),88));
  assert(near(mikas_ignition_deg((uint8_t)0xFC),-2));
  assert(near(mikas_injection_ms(525),4.2));
  assert(near(mikas_fuel_lph(25),2.5));
  double out=0;assert(mikas_fuel_l100(7,50,&out)&&near(out,14));
  assert(!mikas_fuel_l100(7,0,&out));
  assert(!mikas_fuel_l100(7,2.9,&out));
  assert(!mikas_fuel_l100(NAN,50,&out));
  puts("mikas_telemetry: OK"); return 0;
}
