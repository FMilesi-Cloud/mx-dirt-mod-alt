/* fork: checks of phases 3, 4 and 6 (src/core/mxb_soil_weather.h, src/core/mxb_soil_sim.h).
   1. Puddles: rain on worked soil fills a rut, not the ground beside it; without rain they dry out.
   2. Erosion: rain on a worked slope moves soil downhill, mass kept.
   3. Crust: packed dry soil grows a crust; a tyre breaks it into loose soil and lumps.
   4. Bare base: ruts that reach the hard base mark it bare.
   5. Grader: a worn band is levelled to ~25 %.
   6. Realistic lips: a tyre spinning up a jump face throws roost out of it (a kicker forming) only with lips_realistic on.
   Build and run:  zig cc -std=c11 -I src/core tests/test_phases.c -o build/test_phases.exe && build/test_phases.exe */
#include <assert.h>
#include <stdio.h>
#include "mxb_soil_weather.h"
static MxbSoilSim *sim(void){MxbSoilSim *s=mxb_soil_sim_create(256,40.f,40.f,.001f,.5f,1.f);assert(s);return s;}
static void work(MxbSoilSim *s,float x0,float x1,float z0,float z1){ /* mark a patch as ridden soil */
    for(int r=0;r<(int)s->size;r++)for(int c=0;c<(int)s->size;c++){float X,Z;mxb_ss_world(s,c,r,&X,&Z);
        if(X>=x0&&X<=x1&&Z>=z0&&Z<=z1)s->mat[(size_t)r*s->size+c]=1;}
}
static size_t cell(const MxbSoilSim *s,float X,float Z){float c,r;mxb_ss_texel(s,X,Z,&c,&r);return (size_t)lrintf(r)*s->size+(size_t)lrintf(c);}
static void weather(MxbSoilSim *s,MxbWeatherStats *st,float rain,float seconds){
    for(float t=0.f;t<seconds;t+=.02f){mxb_soil_sim_begin(s);mxb_soil_weather_step(s,.02f,rain,20000u,st);mxb_soil_sim_end(s);}
}
static MxbSoilSimWheel wheel(float x,float z,float v,float slip){
    MxbSoilSimWheel w;memset(&w,0,sizeof(w));w.x=x;w.z=z;w.fwd[0]=1.f;w.lat[1]=1.f;w.speed=v;w.slip_long=slip;w.load=1500.f;w.width=.15f;w.radius=.33f;
    w.gain=1.f;w.dt=.02f;w.valid=1;w.material=7;w.drive=1.f;return w;
}
int main(void){
    /* 1. puddles: a 6 cm rut along x in a worked patch */
    MxbSoilSim *s=sim();MxbWeatherStats st;memset(&st,0,sizeof(st));work(s,-4.f,4.f,-2.f,2.f);
    for(int r=0;r<(int)s->size;r++)for(int c=0;c<(int)s->size;c++){float X,Z;mxb_ss_world(s,c,r,&X,&Z);if(fabsf(X)<3.f&&fabsf(Z)<.25f)mxb_ss_add(s,c,r,-.06f,0);}
    weather(s,&st,1.f,240.f);
    const int in_rut=s->water[cell(s,0.f,0.f)],beside=s->water[cell(s,0.f,1.2f)];
    weather(s,&st,0.f,1200.f);const int dried=s->water[cell(s,0.f,0.f)];
    fprintf(stderr,"puddle: rut %.1f mm, beside %.1f mm, 20 min later %.1f mm (sweep %.2f s)\n",in_rut*.2f,beside*.2f,dried*.2f,st.last_sweep);
    assert(in_rut>25&&beside<in_rut/3&&dried<in_rut/4);
    mxb_soil_sim_destroy(s);
    /* 2. erosion: a 15 % slope down +x */
    s=sim();memset(&st,0,sizeof(st));s->p.moisture=.6f;work(s,-6.f,6.f,-1.f,1.f);
    s->base=malloc((size_t)s->size*s->size*sizeof(float));assert(s->base);
    for(int r=0;r<(int)s->size;r++)for(int c=0;c<(int)s->size;c++){float X,Z;mxb_ss_world(s,c,r,&X,&Z);s->base[(size_t)r*s->size+c]=-.15f*X;}
    const double v0=s->volume_m3;weather(s,&st,1.f,300.f);
    float up=0.f,down=0.f;for(float z=-.8f;z<=.8f;z+=.2f){up+=mxb_ss_sample_m(s,-4.f,z);down+=mxb_ss_sample_m(s,5.5f,z);}
    fprintf(stderr,"erosion: %.4f m3 moved, upslope %+.1f mm, foot %+.1f mm, volume %+.2e m3\n",s->eroded_m3,up/9.f*1e3f,down/9.f*1e3f,s->volume_m3-v0);
    assert(s->eroded_m3>1e-4&&up<0.f&&fabs(s->volume_m3-v0)<1e-5);
    mxb_soil_sim_destroy(s);
    /* 3. crust: packed dry patch, one loose cell */
    s=sim();memset(&st,0,sizeof(st));s->p.moisture=.15f;work(s,-2.f,2.f,-2.f,2.f);
    for(int r=0;r<(int)s->size;r++)for(int c=0;c<(int)s->size;c++){size_t k=(size_t)r*s->size+c;if(s->mat[k])mxb_ss_setc(s,k,s->p.compaction0+.3f);}
    s->loose[cell(s,1.f,1.f)]=.02f;
    weather(s,&st,0.f,900.f);
    const int crusted=s->crust[cell(s,0.f,0.f)],loose_cell=s->crust[cell(s,1.f,1.f)];
    float loose0=s->loose[cell(s,0.f,0.f)];
    for(int i=0;i<40;i++){MxbSoilSimWheel w=wheel(-1.5f+.075f*(float)i,0.f,4.f,0.f);mxb_soil_sim_begin(s);mxb_soil_sim_wheel(s,1,&w);mxb_soil_sim_end(s);}
    fprintf(stderr,"crust: %d/255 after 15 min dry (loose cell %d); after a tyre %d, loose %.1f -> %.1f mm, breaks %llu\n",crusted,loose_cell,s->crust[cell(s,0.f,0.f)],loose0*1e3f,s->loose[cell(s,0.f,0.f)]*1e3f,(unsigned long long)s->crust_breaks);
    assert(crusted>150&&loose_cell<20&&s->crust[cell(s,0.f,0.f)]<crusted/2&&s->crust_breaks>0);
    mxb_soil_sim_destroy(s);
    /* 4. bare base: a shallow hard base and a spinning tyre digging */
    s=sim();s->p.hardpan=.02f;
    for(int i=0;i<300;i++){MxbSoilSimWheel w=wheel(-1.f+.01f*(float)(i%60),0.f,3.f,-6.f);mxb_soil_sim_begin(s);mxb_soil_sim_wheel(s,1,&w);mxb_soil_sim_end(s);}
    fprintf(stderr,"bare base: %llu cells, under the tyre %d\n",(unsigned long long)s->exposed_cells,s->exposed[cell(s,-.6f,0.f)]);
    assert(s->exposed_cells>0);
    mxb_soil_sim_destroy(s);
    /* 5. grader along a worn band */
    s=sim();for(int r=0;r<(int)s->size;r++)for(int c=0;c<(int)s->size;c++){float X,Z;mxb_ss_world(s,c,r,&X,&Z);if(fabsf(X)<4.f&&fabsf(Z)<.3f){mxb_ss_add(s,c,r,-.08f,0);s->mat[(size_t)r*s->size+c]=1;}}
    mxb_soil_sim_begin(s);mxb_soil_service_band(s,-5.f,0.f,5.f,0.f,1.7f,1);mxb_soil_sim_end(s);
    fprintf(stderr,"grader: rut -80 mm -> %.1f mm\n",mxb_ss_sample_m(s,0.f,0.f)*1e3f);
    assert(mxb_ss_sample_m(s,0.f,0.f)>-.03f);
    mxb_soil_sim_destroy(s);
    /* 6. realistic lips: a jump face (slope 1:1 up to a lip at x=1, then a 1 m drop) */
    double cut[2];
    for(int real=0;real<2;real++){
        s=sim();s->p.lip_real=(float)real;s->base=malloc((size_t)s->size*s->size*sizeof(float));assert(s->base);
        for(int r=0;r<(int)s->size;r++)for(int c=0;c<(int)s->size;c++){float X,Z;mxb_ss_world(s,c,r,&X,&Z);s->base[(size_t)r*s->size+c]=X<-1.f?0.f:X<1.f?X+1.f:X<1.6f?2.f-(X-1.f)/.6f:1.f;}
        for(int i=0;i<60;i++){MxbSoilSimWheel w=wheel(-.6f+.02f*(float)i,0.f,4.f,-5.f);mxb_soil_sim_begin(s);mxb_soil_sim_wheel(s,1,&w);mxb_soil_sim_end(s);}
        cut[real]=s->roost_m3;mxb_soil_sim_destroy(s);}
    fprintf(stderr,"lips: roost dug out of the face %.5f m3 crew-kept, %.5f m3 realistic\n",cut[0],cut[1]);
    assert(cut[0]<1e-5&&cut[1]>.002);
    /* 7. reset / smooth wave rows: smooth halves the wear in its rows only, reset clears it and the cell's history */
    s=sim();for(int r=0;r<(int)s->size;r++)for(int c=0;c<(int)s->size;c++){mxb_ss_add(s,c,r,-.04f,0);s->mat[(size_t)r*s->size+c]=1;s->crust[(size_t)r*s->size+c]=200;}
    mxb_soil_wave_rows(s,0,128,.5f,0);mxb_soil_wave_rows(s,128,256,0.f,1);
    fprintf(stderr,"wave: smoothed rows %.1f mm, reset rows %.1f mm (worked %d, crust %d)\n",mxb_ss_m(s,10*256+10)*1e3f,mxb_ss_m(s,200*256+10)*1e3f,s->mat[200*256+10],s->crust[10*256+10]);
    assert(fabsf(mxb_ss_m(s,10*256+10)+.02f)<1e-4f&&mxb_ss_m(s,200*256+10)==0.f&&!s->mat[200*256+10]&&s->mat[10*256+10]&&!s->crust[10*256+10]);
    mxb_soil_sim_destroy(s);
    puts("test_phases ok");return 0;
}
