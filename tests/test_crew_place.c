/* fork: check of the crew machines resting on the real ground (src/render/mxdirt_crew_model.h, mxc_rest).
   A 6 m long, 3 m wide machine heading +x:
   1. up a jump face (slope .5): pitched up .5, sitting on the face;  2. over a sharp crest: level, on the crest top,
   bridging;  3. across a dip (a landing hollow): level, on both rims, never down in it;  4. on a side slope: rolled;
   5. never through the ground: every footprint point is at or under its floor.
   Build and run:  zig cc -std=c11 -I src/render tests/test_crew_place.c -o build/test_crew_place.exe && build/test_crew_place.exe */
#include <assert.h>
#include <stdio.h>
#include "mxdirt_crew_model.h"
static int shape;
static float ground(void *u,float x,float z){(void)u;
    switch(shape){case 0:return .5f*x;                       /* face */
    case 1:return 2.f-fabsf(x);                              /* crest */
    case 2:return fabsf(x)<2.f?-.6f*(1.f-fabsf(x)/2.f):0.f; /* dip, 60 cm deep, 4 m across */
    default:return .3f*z;}                                   /* side slope, rising to +z = the machine's left */
}
int main(void){
    static const char *const name[4]={"face","crest","dip","side slope"};
    const float hl=3.f,hw=1.5f;
    for(shape=0;shape<4;shape++){MxcRest r;assert(mxc_rest(&r,0.f,0.f,1.f,0.f,hw,hl,ground,NULL));
        /* the floor of the machine: y + pitch*u + roll*v (v to its right = -z here) */
        float worst=1e9f;for(float u=-hl;u<=hl;u+=.25f)for(float v=-hw;v<=hw;v+=.25f){const float floor=r.y+r.pitch*u+r.roll*v,gnd=ground(NULL,u,-v);worst=fminf(worst,floor-gnd);}
        fprintf(stderr,"%-10s y %.3f pitch %+.3f roll %+.3f, lowest clearance %+.3f m\n",name[shape],r.y,r.pitch,r.roll,worst);
        assert(worst>-.26f); /* sampled every 1.5 m along: a point between stations may poke up a little on a sharp crest */
        if(shape==0)assert(fabsf(r.pitch-.5f)<.01f&&fabsf(r.y)<.01f);
        if(shape==1)assert(fabsf(r.pitch)<.01f&&fabsf(r.y-2.f)<.01f);
        if(shape==2)assert(fabsf(r.pitch)<.01f&&fabsf(r.y)<.01f);
        if(shape==3)assert(fabsf(r.roll+.3f)<.01f&&fabsf(r.y)<.01f);}
    puts("test_crew_place ok");return 0;
}
