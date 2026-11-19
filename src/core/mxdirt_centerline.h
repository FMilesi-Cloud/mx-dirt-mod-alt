#ifndef MXDIRT_CENTERLINE_H
#define MXDIRT_CENTERLINE_H
/* fork: a track's centre line (the game's TrackCenterline segments) -> a polyline, a point every `step` metres,
   in the segments' own coordinates (world x, z). The game documents "start angle in degrees" but not which way
   0 points or which way it turns, so every convention is tried: integrate each segment from its start and see how
   far its end lands from the next segment's start; the convention with the smallest gaps is the right one. */
#include <math.h>
#include <stdint.h>
typedef struct {int32_t type;float len,radius,angle,start[2];} MxcSeg;
typedef struct {int convention;float gap_m,length_m;} MxcFit;
/* convention c: bits 0-1 where 0 degrees points and which way angles grow, bit 2 which way a positive radius turns */
static inline void mxc_dir(int c,float deg,float *dx,float *dz){
    const float a=deg*.017453293f;
    switch(c&3){case 0:*dx=sinf(a);*dz=cosf(a);break;   /* 0 = +z, clockwise (compass) */
    case 1:*dx=-sinf(a);*dz=cosf(a);break;               /* 0 = +z, counter-clockwise */
    case 2:*dx=cosf(a);*dz=sinf(a);break;                /* 0 = +x, toward +z */
    default:*dx=cosf(a);*dz=-sinf(a);break;}             /* 0 = +x, toward -z */
}
/* walks one segment from its start; out (if not NULL) gets a point every step m (n counts them); returns the end */
static inline void mxc_walk(int c,const MxcSeg *s,float step,float *ex,float *ez,float *out,uint32_t *n,uint32_t cap){
    float x=s->start[0],z=s->start[1],a=s->angle;const float sg=(c&4)?-1.f:1.f;
    const int curve=s->type==1&&fabsf(s->radius)>.5f;const float ds=.25f;float done=0.f,next=0.f;
    while(done<s->len){const float d=fminf(ds,s->len-done);float dx,dz;
        if(out&&done>=next&&*n<cap){out[2*(*n)]=x;out[2*(*n)+1]=z;(*n)++;next+=step;}
        if(curve){const float da=sg*d/s->radius*57.29578f;mxc_dir(c,a+.5f*da,&dx,&dz);a+=da;}else mxc_dir(c,a,&dx,&dz);
        x+=dx*d;z+=dz*d;done+=d;}
    *ex=x;*ez=z;
}
/* writes up to cap points (x,z pairs) into out; returns how many; fit says which convention and how well it closes */
static inline uint32_t mxc_centerline(const MxcSeg *seg,int count,float *out,uint32_t cap,float step,MxcFit *fit){
    int best=0;float best_gap=1e30f,len=0.f;
    for(int i=0;i<count;i++)len+=seg[i].len;
    for(int c=0;c<8;c++){float gap=0.f;
        for(int i=0;i+1<count;i++){float ex,ez;mxc_walk(c,seg+i,step,&ex,&ez,(float*)0,(uint32_t*)0,0);gap+=hypotf(ex-seg[i+1].start[0],ez-seg[i+1].start[1]);}
        if(gap<best_gap){best_gap=gap;best=c;}}
    uint32_t n=0;for(int i=0;i<count;i++){float ex,ez;mxc_walk(best,seg+i,step,&ex,&ez,out,&n,cap);}
    if(fit){fit->convention=best;fit->gap_m=count>1?best_gap/(float)(count-1):0.f;fit->length_m=len;}
    return n;
}
#endif
