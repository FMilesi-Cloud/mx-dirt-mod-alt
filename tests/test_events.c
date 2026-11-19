/* fork: checks of phase 2 (src/core/mxdirt_events.h).
   1. Crash: a bike sliding on its side from 8 m/s to a stop ploughs a groove, wider and deeper where it was fast,
      soil kept (put beside it); an upright bike that is not crashed leaves nothing.
   2. Berm: a wheel turning hard (1.5 g) beside a tall loose ridge blows it out (crest lower, soil outside it); the
      same ridge as solid ground (a berm of the track itself) holds; with berm blowouts at 0 nothing happens.
   Build and run:  zig cc -std=c11 -I src/core tests/test_events.c -o build/test_events.exe && build/test_events.exe */
#include <assert.h>
#include <stdio.h>
#include "mxdirt_events.h"
static MxbSoilSim *sim(void){MxbSoilSim *s=mxb_soil_sim_create(512,40.f,40.f,.001f,.5f,1.f);assert(s);return s;}
static float at(const MxbSoilSim *s,float x,float z){return mxb_ss_sample_m(s,x,z);}
static void contact(MxdirtContact *c,float x,float z,float v,float lean,float lat_z){
    memset(c,0,sizeof(*c));c->point[0]=x;c->point[2]=z;c->forward[0]=1.f;c->lateral[2]=lat_z;c->center_velocity[0]=v;
    c->lean=lean;c->load_n=1500.f;c->contact=1;c->deformation=1;c->dt=1.f/50.f;
}
static void slide(MxbSoilSim *s,MxdEvents *e,int crashed,float lean){
    float x=-10.f,v=8.f;const float dt=1.f/50.f;e->crashed=crashed;
    for(int i=0;i<400&&v>0.f;i++){MxdirtContact in[2];contact(in,x+.6f,0.f,v,lean,1.f);contact(in+1,x-.6f,0.f,v,lean,1.f);
        mxd_crash_step(e,s,in,1.f);x+=v*dt;v-=3.f*dt;}
}
int main(void){
    /* 1. crash */
    MxbSoilSim *s=sim();MxdEvents e;memset(&e,0,sizeof(e));e.crash_gain=1.f;e.berm_gain=1.f;
    slide(s,&e,1,.95f);
    const float fast=at(s,-8.f,0.f),slow=at(s,.0f,0.f),side_fast=at(s,-8.f,.32f);
    float wf=0.f,ws=0.f;for(float z=-.4f;z<=.4f;z+=.02f){if(at(s,-8.f,z)<-.004f)wf+=.02f;if(at(s,0.f,z)<-.004f)ws+=.02f;}
    fprintf(stderr,"crash: groove %.1f m, depth fast %.1f mm (width %.2f m) slow %.1f mm (width %.2f m), beside %+.1f mm, volume %+.6f m3\n",
        e.crash_m,fast*1e3f,wf,slow*1e3f,ws,side_fast*1e3f,s->volume_m3);
    assert(e.crash_m>8.f&&fast<-.015f&&fast<slow&&wf>ws&&side_fast>0.f);
    assert(fabs(s->volume_m3)<1e-4); /* soil kept */
    mxb_soil_sim_destroy(s);
    s=sim();memset(&e,0,sizeof(e));e.crash_gain=1.f;slide(s,&e,0,.1f);assert(e.crash_m==0.f&&at(s,-8.f,0.f)==0.f);mxb_soil_sim_destroy(s);
    /* 2. berm: a 15 cm ridge 0.4-0.6 m outside a wheel leaned into the turn (lean + = toward +lateral = inside;
       lateral = +z, so the outside is -z) */
    for(int firm=0;firm<3;firm++){
        s=sim();memset(&e,0,sizeof(e));e.crash_gain=1.f;e.berm_gain=firm==2?0.f:1.f;
        for(int r=0;r<(int)s->size;r++)for(int c=0;c<(int)s->size;c++){float X,Z;mxb_ss_world(s,c,r,&X,&Z);
            if(fabsf(X)<3.f&&Z<-.38f&&Z>-.62f){mxb_ss_add(s,c,r,.15f,!firm);if(firm==1)mxb_ss_setc(s,(size_t)r*s->size+c,s->p.compaction0+.35f);}}
        const float crest0=at(s,0.f,-.5f),out0=at(s,0.f,-.95f);
        for(int k=0;k<30;k++){const float h=12.f/10.f*(float)k/50.f,fx=cosf(h),fz=sinf(h); /* turning toward +z (inside), 1.47 g */
            MxdirtContact in[2];contact(in,0.f,0.f,12.f,0.f,1.f);contact(in+1,-1.4f,0.f,12.f,0.f,1.f);
            for(int w=0;w<2;w++){in[w].forward[0]=fx;in[w].forward[2]=fz;in[w].lateral[0]=-fz;in[w].lateral[2]=fx;in[w].center_velocity[0]=12.f*fx;in[w].center_velocity[2]=12.f*fz;}
            mxd_berm_step(&e,s,in,1.f);}
        const float crest=at(s,0.f,-.5f),out=at(s,0.f,-.95f);
        fprintf(stderr,"berm (%s): blowouts %llu, crest %.0f -> %.0f mm, outside %.0f -> %.0f mm, push/hold frames %llu %llu %llu %llu\n",
            firm==0?"loose":firm==1?"packed":"off",(unsigned long long)e.berm_blows,crest0*1e3f,crest*1e3f,out0*1e3f,out*1e3f,
            (unsigned long long)e.berm_frames[0],(unsigned long long)e.berm_frames[1],(unsigned long long)e.berm_frames[2],(unsigned long long)e.berm_frames[3]);
        if(firm==0)assert(e.berm_blows>=1&&crest<crest0-.02f&&out>out0+.005f);
        else assert(e.berm_blows==0&&crest==crest0);
        mxb_soil_sim_destroy(s);
    }
    /* 3. steering corrections on a straight past a loose berm: the front wheel points left and right, the bike goes
       straight - no turn, no blowout (the first ride had 2133 from this) */
    {MxbSoilSim *s=sim();MxdEvents e;memset(&e,0,sizeof(e));e.berm_gain=1.f;
     for(int r=0;r<(int)s->size;r++)for(int c=0;c<(int)s->size;c++){float X,Z;mxb_ss_world(s,c,r,&X,&Z);if(fabsf(X)<6.f&&Z<-.38f&&Z>-.62f)mxb_ss_add(s,c,r,.15f,1);}
     for(int k=0;k<200;k++){const float a=.35f*sinf((float)k*.9f),fx=cosf(a),fz=sinf(a);MxdirtContact in[2];
      contact(in,-5.f+.24f*(float)k*.2f,0.f,12.f,0.f,1.f);contact(in+1,-6.4f+.24f*(float)k*.2f,0.f,12.f,0.f,1.f);
      in[0].forward[0]=fx;in[0].forward[2]=fz;in[0].lateral[0]=-fz;in[0].lateral[2]=fx; /* the front wheel steers; velocity stays +x */
      mxd_berm_step(&e,s,in,1.f);}
     fprintf(stderr,"berm (steering on a straight): blowouts %llu\n",(unsigned long long)e.berm_blows);assert(e.berm_blows==0);mxb_soil_sim_destroy(s);}
    puts("test_events ok");return 0;
}
