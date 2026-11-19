/* fork: check of the telemetry layout detection (src/core/mxdirt_bike.inc) with synthetic records.
   184-byte record, 188-byte record with an extra field early on (both must be found and used, brakes read right),
   and a 188-byte record whose extra field sits between the brakes and the materials (must be refused, never misread).
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
static float got_brake[2];
void mxdirt_session_set_inputs(MxdirtSession *s,const float brake[2]){(void)s;got_brake[0]=brake[0];got_brake[1]=brake[1];}
int mxdirt_session_set_tyres(MxdirtSession *s,const float w[2],float k,float sl){(void)s;(void)w;(void)k;(void)sl;return 1;}
static int ra_read(void *u,uint64_t a,void *o,size_t n){(void)u;(void)a;(void)o;(void)n;return 0;}
#include "mxdirt_bike.inc"
/* the assumed tail: ... crashed, steer, throttle_in, throttle, front_brake, rear_brake, clutch, ws0, ws1, mat0, mat1, steer_torque */
static int build(int32_t *rec,int extra_at,float fb,float rb,int frame){
 float f[48];int32_t *i=(int32_t*)f;int n=0;
 for(int k=0;k<34;k++)f[n++]=(float)(k*1.5+frame%7);  /* rpm .. suspension (any values) */
 i[0]=9000+frame;i[3]=2;                              /* rpm, gear: ints that change */
 i[n++]=0;                                            /* crashed */
 f[n++]=3.f+.1f*(float)(frame%5);                     /* steer (never 0 here) */
 f[n++]=.6f;f[n++]=.55f;f[n++]=fb;f[n++]=rb;f[n++]=0.f;f[n++]=12.f;f[n++]=12.3f;
 i[n++]=10;i[n++]=10;f[n++]=1.5f;                     /* materials (soft soil), steer torque */
 assert(n==46);
 if(extra_at>=0){memmove(&f[extra_at+1],&f[extra_at],(size_t)(n-extra_at)*4);f[extra_at]=.25f;n++;}
 memcpy(rec,f,(size_t)n*4);return n*4;
}
static int run(int extra_at){
 ra_telemetry_track();ra_log=NULL;
 for(int w=0;w<2;w++){memset(&ra_meta[w],0,sizeof(ra_meta[w]));ra_meta[w].material=10;ra_meta[w].availability.normal_contact=1;}
 int32_t rec[64];
 for(int frame=0;frame<1200&&ra_tel_state!=1&&ra_tel_state!=-1;frame++){
  const int bytes=build(rec,extra_at,frame>700?.8f:.0f,frame>700?.3f:.0f,frame);
  ra_telemetry_in(rec,bytes);ra_telemetry_frame();}
 if(ra_tel_state==1){int32_t r[64];const int b=build(r,extra_at,.8f,.3f,0);ra_telemetry_in(r,b);ra_telemetry_frame();}
 return ra_tel_state;
}
int main(void){
 assert(run(-1)==1&&fabsf(got_brake[0]-.8f)<1e-6f&&fabsf(got_brake[1]-.3f)<1e-6f); /* 184 bytes */
 assert(run(6)==1&&fabsf(got_brake[0]-.8f)<1e-6f&&fabsf(got_brake[1]-.3f)<1e-6f);  /* 188, extra field early */
 assert(run(40)==-1&&got_brake[0]==0.f&&got_brake[1]==0.f);                          /* 188, extra field in the tail: refused */
 (void)ra_tyres_lookup;(void)ra_tyre_from_memory;
 puts("test_telemetry ok");return 0;
}
