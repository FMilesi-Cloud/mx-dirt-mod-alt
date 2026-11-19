/* fork: check of src/core/mxdirt_centerline.h. A closed track (straight, right turn, straight, left hairpin,
   straight, turns) written in each of the 8 heading conventions: each time the right one is found, the line closes
   (gaps ~0), points come every ~1 m and the walk length matches the track length.
   Build and run:  zig cc -std=c11 -I src/core tests/test_centerline.c -o build/test_centerline.exe && build/test_centerline.exe */
#include <assert.h>
#include <stdio.h>
#include "mxdirt_centerline.h"
int main(void){
    static float pts[2*4000];
    for(int c=0;c<8;c++){
        /* the layout as types, lengths, signed radii; starts and angles are generated in convention c */
        const int type[7]={0,1,0,1,0,1,0};const float len[7]={60.f,31.4159f,40.f,47.1239f,35.f,23.5619f,20.f},rad[7]={0.f,20.f,0.f,-15.f,0.f,15.f,0.f};
        MxcSeg seg[7];float x=12.f,z=-30.f,a=37.f;
        for(int i=0;i<7;i++){seg[i].type=type[i];seg[i].len=len[i];seg[i].radius=rad[i];seg[i].angle=a;seg[i].start[0]=x;seg[i].start[1]=z;
            float ex,ez;mxc_walk(c,seg+i,1.f,&ex,&ez,NULL,NULL,0);x=ex;z=ez;
            if(type[i]==1)a+=((c&4)?-1.f:1.f)*len[i]/rad[i]*57.29578f;}
        MxcFit fit;const uint32_t n=mxc_centerline(seg,7,pts,4000,1.f,&fit);
        float walked=0.f;for(uint32_t i=1;i<n;i++)walked+=hypotf(pts[2*i]-pts[2*i-2],pts[2*i+1]-pts[2*i-1]);
        fprintf(stderr,"convention %d: found %d, mean gap %.4f m, %u points, track %.1f m, walked %.1f m\n",c,fit.convention,fit.gap_m,n,fit.length_m,walked);
        assert(fit.gap_m<.05f&&fabsf((float)n-fit.length_m)<8.f&&fabsf(walked-fit.length_m)<3.f); /* a point per metre, one more per segment start */
        /* the found convention draws the same line as the true one (a mirror twin may tie; it must not differ) */
        static float ref[2*4000];uint32_t m=0;for(int i=0;i<7;i++){float ex,ez;mxc_walk(c,seg+i,1.f,&ex,&ez,ref,&m,4000);}
        float worst=0.f;for(uint32_t i=0;i<n&&i<m;i++)worst=fmaxf(worst,hypotf(pts[2*i]-ref[2*i],pts[2*i+1]-ref[2*i+1]));
        assert(m==n&&worst<.05f);
    }
    puts("test_centerline ok");return 0;
}
