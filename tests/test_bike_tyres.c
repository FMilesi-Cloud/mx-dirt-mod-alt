/* fork: check of the bike tyre lookup on a real install. Run with the MX Bikes folder and Documents\PiBoSo\MX Bikes\
   and a bike id; prints what the mod would use. With "450xf" (p_mx tyres) widths must be .150 / .110.
   Build: zig cc -std=c11 -I src/core -I src/shared tests/test_bike_tyres.c -o build/test_bike_tyres.exe */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mxdirt_wheel_sample.h"
#include "mxdirt_session.h"
static FILE *ra_log;static MxbWheelSampleMetadata ra_meta[2];static MxdirtSession *ra_session;
void mxdirt_session_set_inputs(MxdirtSession *s,const float brake[2],float t){(void)s;(void)brake;(void)t;}
int mxdirt_session_set_tyres(MxdirtSession *s,const float w[2],float k,float sl){(void)s;(void)w;(void)k;(void)sl;return 1;}
static int ra_read(void *u,uint64_t a,void *o,size_t n){(void)u;(void)a;(void)o;(void)n;return 0;}
#include "mxdirt_bike.inc"
int main(int argc,char **argv){
    if(argc<4){puts("usage: test_bike_tyres <Documents MX Bikes dir> <game dir> <bike id> [expect_front expect_rear]");return 2;}
    snprintf(ra_save_dir,sizeof(ra_save_dir),"%s",argv[1]);snprintf(ra_game_dir_override,sizeof(ra_game_dir_override),"%s",argv[2]);ra_log=stdout;
    ra_tyres_lookup(argv[3]);
    if(argc>=6){assert(fabsf(ra_tyre_width[0]-strtof(argv[4],NULL))<.002f&&fabsf(ra_tyre_width[1]-strtof(argv[5],NULL))<.002f);puts("test_bike_tyres ok");}
    (void)ra_tyres_lookup;(void)ra_telemetry_frame;(void)ra_telemetry_in;(void)ra_telemetry_track;(void)ra_tyre_from_memory;return 0;
}
