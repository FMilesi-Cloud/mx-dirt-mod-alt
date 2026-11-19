/* fork: checks of phase 1 (src/core/mxb_soil_sim.h, src/core/mxdirt_soft_layer.h).
   1. A slow wheel with no pull (pushed / paddled) cuts much less than the same wheel pulling; at riding speed the
      two cut the same.  2. A boot print: toe deeper than heel, soil pushed back behind the heel, deeper in damp
      loose soil than on dry packed ground.
   Build and run:  zig cc -std=c11 -I src/core tests/test_footprint.c -o build/test_footprint.exe && build/test_footprint.exe */
#include <assert.h>
#include <stdio.h>
#include "mxb_soil_sim.h"
#include "mxdirt_soft_layer.h"
static float rut(float speed,float drive){
    MxbSoilSim *s=mxb_soil_sim_create(256,40.f,40.f,.001f,.5f,1.f);assert(s);
    MxbSoilSimWheel w;memset(&w,0,sizeof(w));
    w.fwd[1]=1.f;w.lat[0]=1.f;w.speed=speed;w.load=1300.f;w.width=.15f;w.radius=.33f;w.gain=1.f;w.dt=1.f/60.f;w.valid=1;w.material=7;w.drive=drive;
    for(int i=0;i<240;i++){w.z=-6.f+speed*w.dt*(float)i;mxb_soil_sim_begin(s);mxb_soil_sim_wheel(s,1,&w);mxb_soil_sim_end(s);}
    float d=0.f;for(float x=-.04f;x<=.04f;x+=.02f)d+=mxb_ss_sample_m(s,x,-6.f+speed*w.dt*120.f);
    mxb_soil_sim_destroy(s);return d/5.f;
}
static int depth(const MxdSoft *s,float X,float Z){
    const uint8_t *p=mxd_soft_peek(s,(int)floorf((X+.5f*s->span_x)/s->cell),(int)floorf((Z+.5f*s->span_z)/s->cell));return p?(int)p[0]-128:0;
}
static float boot_depth_t(float thick,float moisture,float packed,int *toe,int *heel,int *behind){
    MxdSoft *s=mxd_soft_create(40.f,40.f,thick);assert(s);
    const float fwd[2]={0.f,1.f};const MxdSoftSoil soil={1.f,moisture,packed};
    mxd_soft_boot(s,0.f,0.f,fwd,300.f,&soil);
    *toe=depth(s,0.f,.10f);*heel=depth(s,0.f,-.10f);*behind=depth(s,0.f,-.175f);
    const int mid=depth(s,0.f,0.f);mxd_soft_destroy(s);return (float)mid;
}
static float boot_depth(float m,float p,int *t,int *h,int *b){return boot_depth_t(.085f,m,p,t,h,b);}
int main(void){
    const float pushed=rut(1.5f,0.f),pulled=rut(1.5f,1.f),ride0=rut(6.f,0.f),ride1=rut(6.f,1.f);
    fprintf(stderr,"1.5 m/s: no pull %.4f m, pulling %.4f m | 6 m/s: no pull %.4f m, pulling %.4f m\n",pushed,pulled,ride0,ride1);
    assert(pulled<0.f&&pushed<0.f);
    assert(pushed>pulled*.6f);                         /* pushed: clearly shallower (depths are negative) */
    assert(ride0<0.f&&fabsf(ride0-ride1)<.0015f);       /* riding speed: the same cut */
    int toe,heel,behind;
    const float wet=boot_depth(.6f,0.f,&toe,&heel,&behind);
    fprintf(stderr,"boot (damp, loose): mid %d toe %d heel %d behind heel %+d mm\n",(int)wet,toe,heel,behind);
    assert(toe<heel&&heel<0&&wet>-40);                /* toe deeper than heel; ~2-3 cm, not a crater */
    assert(behind>0&&behind<20);                       /* a little soil pushed back behind the heel */
    int t2,h2,b2;const float dry=boot_depth(.12f,.9f,&t2,&h2,&b2);
    fprintf(stderr,"boot (dry, packed): mid %d toe %d heel %d mm\n",(int)dry,t2,h2);
    assert(dry>wet);                                    /* dry packed ground: a much shallower print */
    int t3,h3,b3;const float thin=boot_depth_t(.010f,.6f,0.f,&t3,&h3,&b3);
    fprintf(stderr,"boot (damp, 10 mm layer): mid %d toe %d heel %d mm\n",(int)thin,t3,h3);
    assert(thin<=-12&&t3>=-19);                        /* a thin layer still shows the print, within what the renderer draws */
    puts("test_footprint ok");return 0;
}
