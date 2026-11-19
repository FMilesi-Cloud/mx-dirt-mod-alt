/* fork: CPU benchmark of the dirt simulation, the same synthetic ride for the original MX Dirt Mod (1.7.1) and
   this fork, built twice from each tree's own headers:
     zig cc -std=c11 -O2 -I <tree>/src/core tests/bench_sim.c -o bench.exe            (original)
     zig cc -std=c11 -O2 -DFORK -I src/core tests/bench_sim.c -o bench_fork.exe      (fork)
   Workload: a 2048 grid over 600 m; a bike lapping a 520 m oval for 60 s at 50 Hz (braking into the turns, spinning
   out of them), both wheels cut the sim and stamp the soft layer; then a 50-rider pre-wear gate on that lap.
   Fork only: the weather pass (rain on / off), crash marks and berm checks on top of each step. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "mxb_soil_sim.h"
#include "mxb_soil_prewear.h"
#include "mxdirt_soft_layer.h"
#ifdef FORK
#include "mxb_soil_weather.h"
#include "mxdirt_events.h"
#endif
static double now_ms(void){LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return (double)t.QuadPart*1000./(double)f.QuadPart;}
static int cmp(const void *a,const void *b){double x=*(const double*)a,y=*(const double*)b;return x<y?-1:x>y;}
/* the oval: two 160 m straights, two R=32 m turns; s metres along it -> point, heading, curvature */
static const float L1=160.f,R=32.f;
static void oval(float s,float *x,float *z,float *fx,float *fz,float *k){
    const float per=2.f*L1+2.f*3.14159265f*R;s=fmodf(s,per);if(s<0.f)s+=per;
    if(s<L1){*x=-L1/2+s;*z=-R;*fx=1;*fz=0;*k=0;return;}s-=L1;
    if(s<3.14159265f*R){float a=s/R-1.5707963f;*x=L1/2+R*cosf(a);*z=R*sinf(a);*fx=-sinf(a);*fz=cosf(a);*k=1/R;return;}s-=3.14159265f*R;
    if(s<L1){*x=L1/2-s;*z=R;*fx=-1;*fz=0;*k=0;return;}s-=L1;
    {float a=s/R+1.5707963f;*x=-L1/2+R*cosf(a);*z=R*sinf(a);*fx=-sinf(a);*fz=cosf(a);*k=1/R;}
}
/* speed plan along the lap: ~22 m/s on the straights, ~11 m/s in the turns; drive = speeding up, brake = slowing */
static float speed_at(float s){float x,z,fx,fz,k,best=22.f;for(float d=0;d<40.f;d+=2.f){oval(s+d,&x,&z,&fx,&fz,&k);if(k>0.f)best=fminf(best,11.f+d*.28f);}
    for(float d=0;d<25.f;d+=2.f){oval(s-d,&x,&z,&fx,&fz,&k);if(k>0.f)best=fminf(best,11.f+d*.44f);}return best;}
static MxbSoilSimWheel wheel_at(float s,int rear,float v,float acc,float t){
    float x,z,fx,fz,k;oval(s-(rear?1.45f:0.f),&x,&z,&fx,&fz,&k);
    MxbSoilSimWheel w;memset(&w,0,sizeof(w));w.x=x;w.z=z;w.fwd[0]=fx;w.fwd[1]=fz;w.lat[0]=-fz;w.lat[1]=fx;w.speed=v;
    w.slip_long=rear&&acc>.5f?-1.5f*acc:acc<-.5f?.6f*-acc:0.f;w.slip_lat=k*v*.15f;
    w.load=(rear?1400.f:1100.f)*(1.f+.25f*sinf(t*6.2831853f*3.f+(float)rear));w.width=rear?.15f:.11f;w.radius=rear?.33f:.35f;
    w.gain=1.f+.15f*sinf(t*6.2831853f*4.f);w.dt=.02f;w.valid=1;w.material=7;w.turn=k*R;w.brake=acc<-.5f?fminf(1.f,-acc/6.f):0.f;
#ifdef FORK
    w.drive=rear&&acc>0.f?fminf(1.f,acc/3.f):0.f;w.lean=-fminf(.8f,k*v*v/9.81f*.6f);
#endif
    return w;
}
int main(void){
    const float span=600.f;const int steps=3000;
    MxbSoilSim *s=mxb_soil_sim_create(2048,span,span,.001f,.5f,1.f);MxdSoft *soft=mxd_soft_create(span,span,.085f);
    if(!s||!soft){puts("alloc failed");return 1;}
#ifdef FORK
    MxbWeatherStats wx;memset(&wx,0,sizeof(wx));MxdEvents ev;memset(&ev,0,sizeof(ev));ev.crash_gain=ev.berm_gain=1.f;
    double *tw=malloc(sizeof(double)*steps),*te=malloc(sizeof(double)*steps);
#endif
    double *ts=malloc(sizeof(double)*steps),*tf=malloc(sizeof(double)*steps);
    float pos=0.f,v=speed_at(0.f);
    for(int i=0;i<steps;i++){const float t=(float)i*.02f,target=speed_at(pos),acc=fmaxf(-8.f,fminf(4.f,(target-v)/.4f));v+=acc*.02f;pos+=v*.02f;
        MxbSoilSimWheel w[2]={wheel_at(pos,0,v,acc,t),wheel_at(pos,1,v,acc,t)};
        double a=now_ms();
        mxb_soil_sim_begin(s);mxb_soil_sim_wheel(s,0,w);mxb_soil_sim_wheel(s,1,w+1);
        double b=now_ms();
        const MxdSoftSoil soil={1.f,.35f,.3f};
        for(int k=0;k<2;k++){const float spin=w[k].slip_long<0.f?fminf(1.f,-w[k].slip_long/fmaxf(v,1.f)):0.f,skid=w[k].slip_long>0.f?fminf(1.f,w[k].slip_long/fmaxf(v,1.f)):0.f;
#ifdef FORK
            mxd_soft_stamp(soft,k,w[k].x,w[k].z,w[k].fwd,w[k].lat,v,.02f,w[k].load,spin,skid,w[k].slip_lat,7,0,&soil,0.f);
#else
            mxd_soft_stamp(soft,k,w[k].x,w[k].z,w[k].fwd,w[k].lat,v,.02f,w[k].load,spin,skid,w[k].slip_lat,7,0,&soil);
#endif
        }
        double c=now_ms();
#ifdef FORK
        {MxdirtContact in[2];memset(in,0,sizeof(in));for(int k=0;k<2;k++){in[k].point[0]=w[k].x;in[k].point[2]=w[k].z;in[k].forward[0]=w[k].fwd[0];in[k].forward[2]=w[k].fwd[1];
            in[k].lateral[0]=w[k].lat[0];in[k].lateral[2]=w[k].lat[1];in[k].center_velocity[0]=v*w[k].fwd[0];in[k].center_velocity[2]=v*w[k].fwd[1];in[k].lean=w[k].lean;
            in[k].load_n=w[k].load;in[k].contact=in[k].deformation=1;in[k].dt=.02f;}
         mxd_crash_step(&ev,s,in,1.f);mxd_berm_step(&ev,s,in,1.f);}
        double d=now_ms();mxb_soil_weather_step(s,.02f,i<steps/2?1.f:0.f,8000u,&wx);double e=now_ms();te[i]=d-c;tw[i]=e-d;c=e;
#endif
        mxb_soil_sim_end(s);double f=now_ms();ts[i]=(b-a)+(f-c);tf[i]=c-b-0;
#ifdef FORK
        tf[i]=c-b-(te[i]+tw[i]);
#endif
    }
    qsort(ts,steps,sizeof(double),cmp);qsort(tf,steps,sizeof(double),cmp);
    double ms=0,mf=0;for(int i=0;i<steps;i++){ms+=ts[i];mf+=tf[i];}
#ifdef FORK
    const char *who="fork";
#else
    const char *who="original";
#endif
    printf("%s ride: sim mean %.4f ms p99 %.4f max %.3f | soft layer mean %.4f ms p99 %.4f | cut %.3f m3\n",who,ms/steps,ts[steps*99/100],ts[steps-1],mf/steps,tf[steps*99/100],s->cut_m3);
#ifdef FORK
    qsort(tw,steps,sizeof(double),cmp);qsort(te,steps,sizeof(double),cmp);double mw=0,me=0;for(int i=0;i<steps;i++){mw+=tw[i];me+=te[i];}
    printf("fork extras: weather mean %.4f ms p99 %.4f max %.3f (rain the first 30 s) | crash+berm mean %.4f ms p99 %.4f\n",mw/steps,tw[steps*99/100],tw[steps-1],me/steps,te[steps*99/100]);
#endif
    /* pre-wear: 50 riders on the recorded lap (60 Hz, both wheels) */
    const int n=2*60*26;MxbDirtGhostLap lap;memset(&lap,0,sizeof(lap));lap.s=calloc((size_t)n,sizeof(MxbDirtGhostSample));lap.h.samples=(uint32_t)n;
    {float p2=0.f,v2=speed_at(0.f);for(int i=0;i<n;i+=2){const float t=(float)i/120.f,tg=speed_at(p2),acc=fmaxf(-8.f,fminf(4.f,(tg-v2)/.4f));v2+=acc/60.f;p2+=v2/60.f;
        for(int k=0;k<2;k++){MxbSoilSimWheel w=wheel_at(p2,k,v2,acc,t);MxbDirtGhostSample *g=lap.s+i+k;
            g->t=t;g->x=w.x;g->z=w.z;g->fwd[0]=w.fwd[0];g->fwd[1]=w.fwd[1];g->lat[0]=w.lat[0];g->lat[1]=w.lat[1];g->speed=w.speed;g->slip_long=w.slip_long;
            g->slip_lat=w.slip_lat;g->load=w.load;g->width=w.width;g->radius=w.radius;g->gain=1.f;g->dt=1.f/60.f;g->wheel=(uint8_t)k;g->material=7;}}}
    MxbSoilSim *s2=mxb_soil_sim_create(2048,span,span,.001f,.5f,1.f);unsigned char *uni=calloc((size_t)s2->tiles_x*s2->tiles_y,1);int box[4];
    double a=now_ms();
#ifdef FORK
    const float mix[3]={.2f,.8f,0.f};uint64_t passes=mxb_soil_prewear(s2,&lap,1,50,1.4f,4.f,.13f,0x9e3779b9u,uni,box,mix);
#else
    uint64_t passes=mxb_soil_prewear(s2,&lap,1,50,1.4f,4.f,.13f,0x9e3779b9u,uni,box);
#endif
    printf("%s pre-wear: 50 riders x 1 lap, %llu passes in %.0f ms\n",who,(unsigned long long)passes,now_ms()-a);
    return 0;
}
