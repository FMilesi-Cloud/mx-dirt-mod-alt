/* fork: check for the lean erosion. A tyre leaning toward +lat (the inside of the turn) must cut deeper on that
   side than on the other, and push more spoil into the outside (-lat) berm than an upright tyre does.
   Build and run:  zig cc -std=c11 -I src/core tests/test_lean.c -o build/test_lean.exe && build/test_lean.exe */
#include <assert.h>
#include <stdio.h>
#include "mxb_soil_sim.h"
typedef struct {float in_floor,out_floor,in_berm,out_berm;} Ride;
static float band(const MxbSoilSim *s,float x0,float x1,float z){double a=0;int n=0;for(float x=x0;x<=x1;x+=.02f){a+=mxb_ss_sample_m(s,x,z);n++;}return (float)(a/n);}
static Ride ride(float lean){
    MxbSoilSim *s=mxb_soil_sim_create(256,40.f,40.f,.001f,.5f,1.f);assert(s);
    MxbSoilSimWheel w;memset(&w,0,sizeof(w));
    w.fwd[1]=1.f;w.lat[0]=1.f;w.speed=6.f;w.load=1500.f;w.width=.15f;w.radius=.33f;w.gain=1.f;w.dt=1.f/60.f;w.valid=1;w.material=7;w.lean=lean;
    for(int i=0;i<180;i++){w.z=-6.f+w.speed*w.dt*(float)i;mxb_soil_sim_begin(s);mxb_soil_sim_wheel(s,0,&w);mxb_soil_sim_end(s);}
    Ride r={band(s,.02f,.10f,0.f),band(s,-.10f,-.02f,0.f),band(s,.25f,.8f,0.f),band(s,-.8f,-.25f,0.f)};
    mxb_soil_sim_destroy(s);return r;
}
int main(void){
    Ride up=ride(0.f),lean=ride(.6f);
    printf("upright: floor in %.4f out %.4f berm in %.4f out %.4f\n",up.in_floor,up.out_floor,up.in_berm,up.out_berm);
    printf("leaned : floor in %.4f out %.4f berm in %.4f out %.4f\n",lean.in_floor,lean.out_floor,lean.in_berm,lean.out_berm);
    assert(up.in_floor<0.f&&up.out_floor<0.f);                        /* an upright tyre cuts a rut */
    assert(lean.in_floor<lean.out_floor);                             /* leaned: deeper on the inside */
    assert(lean.out_berm>up.out_berm&&lean.out_berm>lean.in_berm);    /* spoil goes to the outside berm */
    puts("test_lean ok");return 0;
}
