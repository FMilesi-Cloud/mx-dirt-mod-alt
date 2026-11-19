/* fork: check of the telemetry layout detection (src/core/mxdirt_bike.inc) with synthetic records shaped like the
   one the game sends (Beta 21, seen in a real log): 47 slots, tail crashed, steer, throttle, front brake, rear brake,
   clutch, wheel speed x2, wheel material x2 (counted from 1: the mod's index + 1), 3 more fields.
   1. that record: found, throttle and brakes read right.  2. materials at the mod's own index (offset 0): found.
   3. an extra field between the brakes and the materials: refused, never misread.
   Build and run:  zig cc -std=c11 -I src/core -I src/shared tests/test_telemetry.c -o build/test_telemetry.exe && build/test_telemetry.exe */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mxdirt_wheel_sample.h"
#include "mxdirt_session.h"
static FILE *ra_log;static MxbWheelSampleMetadata ra_meta[2];static MxdirtSession *ra_session=(MxdirtSession*)1;
static float got_brake[2],got_throttle=-2.f;
void mxdirt_session_set_inputs(MxdirtSession *s,const float brake[2],float throttle){(void)s;got_brake[0]=brake[0];got_brake[1]=brake[1];got_throttle=throttle;}
int mxdirt_session_set_tyres(MxdirtSession *s,const float w[2],float k,float sl){(void)s;(void)w;(void)k;(void)sl;return 1;}
static int ra_read(void *u,uint64_t a,void *o,size_t n){(void)u;(void)a;(void)o;(void)n;return 0;}
#include "mxdirt_bike.inc"
enum { MOD_MAT=10 };
static int build(int32_t *rec,int offset,int shifted,float th,float fb,float rb,int frame){
 float f[64];int32_t *i=(int32_t*)f;int n=0;
 for(int k=0;k<34;k++)f[n++]=(float)(k*1.5+frame%7);
 i[0]=9000+frame;i[3]=2;
 i[n++]=0;                                   /* 34 crashed */
 f[n++]=3.f+.1f*(float)(frame%5);            /* 35 steer (never 0 here) */
 f[n++]=th;f[n++]=fb;f[n++]=rb;              /* 36 throttle, 37 front brake, 38 rear brake */
 if(shifted)f[n++]=.25f;                     /* an extra field in the tail */
 f[n++]=0.f;f[n++]=12.f;f[n++]=12.3f;        /* clutch, wheel speeds */
 i[n++]=MOD_MAT+offset;i[n++]=MOD_MAT+offset; /* materials */
 f[n++]=1.5f;f[n++]=3.26f;f[n++]=.22f;       /* 3 more fields */
 if(shifted)n--;                             /* keep 188 bytes */
 assert(n==47);memcpy(rec,f,(size_t)n*4);return n*4;
}
static int run(int offset,int shifted){
 ra_telemetry_track();ra_log=NULL;got_throttle=-2.f;
 for(int w=0;w<2;w++){memset(&ra_meta[w],0,sizeof(ra_meta[w]));ra_meta[w].material=MOD_MAT;ra_meta[w].availability.normal_contact=1;}
 int32_t rec[64];
 for(int frame=0;frame<2000&&ra_tel_state!=1&&ra_tel_state!=-1;frame++){
  const int late=frame>300;const int b=build(rec,offset,shifted,late?.9f:.1f,late?.8f:0.f,late?.3f:0.f,frame);
  ra_telemetry_in(rec,b);ra_telemetry_frame();}
 if(ra_tel_state==1){int32_t r[64];const int b=build(r,offset,shifted,.9f,.8f,.3f,0);ra_telemetry_in(r,b);ra_telemetry_frame();}
 return ra_tel_state;
}
int main(void){
 assert(run(1,0)==1&&fabsf(got_throttle-.9f)<1e-6f&&fabsf(got_brake[0]-.8f)<1e-6f&&fabsf(got_brake[1]-.3f)<1e-6f); /* the game's record */
 assert(run(0,0)==1&&fabsf(got_throttle-.9f)<1e-6f&&fabsf(got_brake[1]-.3f)<1e-6f);                                /* offset 0 */
 assert(run(1,1)==-1&&got_brake[0]==0.f&&got_brake[1]==0.f&&got_throttle<0.f);                                    /* shifted tail: refused */
 (void)ra_tyres_lookup;(void)ra_tyre_from_memory;
 puts("test_telemetry ok");return 0;
}
