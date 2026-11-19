#ifndef MXDIRT_CREW_MODEL_H
#define MXDIRT_CREW_MODEL_H
/* fork: track crew models (tools/crew_export.py writes them). Files only, no GL here.
   model.mxm, little endian:
     "MXM1", u32 parts, then per part: char tex[40] (a .tga next to the model, "" = none), u32 flags (1 = wheel),
     f32 pivot[3], f32 radius, u32 vertices, vertices x {f32 pos[3], nrm[3], uv[2]} (triangle list).
   Axes: metres, +Y up, +Z forward, +X right; origin = middle of the footprint, on the ground.
   Textures: uncompressed or RLE TGA, 24/32 bit (what Blender's TARGA_RAW writes). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define MXC_MAX_PARTS 32
#define MXC_MAX_VERTS 600000u
typedef struct {char tex[40];uint32_t flags,first,count;float pivot[3],radius;} MxcPart;
typedef struct {uint32_t parts,verts;MxcPart part[MXC_MAX_PARTS];float *v;float lo[3],hi[3];} MxcModel;
static inline void mxc_free(MxcModel *m){if(m){free(m->v);memset(m,0,sizeof(*m));}}
/* 1 = loaded; 0 = missing or malformed (nothing allocated) */
static inline int mxc_load(FILE *f,MxcModel *m){
    memset(m,0,sizeof(*m));if(!f)return 0;
    char magic[4];uint32_t n=0;if(fread(magic,1,4,f)!=4||memcmp(magic,"MXM1",4)||fread(&n,4,1,f)!=1||!n||n>MXC_MAX_PARTS)return 0;
    size_t cap=0;
    for(uint32_t i=0;i<n;i++){
        MxcPart *p=m->part+i;uint32_t hdr[6];
        if(fread(p->tex,1,40,f)!=40||fread(hdr,4,6,f)!=6)goto bad;
        p->tex[39]=0;p->flags=hdr[0];memcpy(p->pivot,hdr+1,12);memcpy(&p->radius,hdr+4,4);p->count=hdr[5];
        if(p->count%3||p->count>MXC_MAX_VERTS||m->verts+p->count>MXC_MAX_VERTS)goto bad;
        for(int k=0;k<3;k++)if(!isfinite(p->pivot[k]))goto bad;if(!isfinite(p->radius)||p->radius<0.f)goto bad;
        if(strstr(p->tex,"..")||strchr(p->tex,'/')||strchr(p->tex,'\\')||strchr(p->tex,':'))goto bad; /* a file next to the model, nothing else */
        p->first=m->verts;
        if(m->verts+p->count>cap){size_t c=cap?cap*2:4096;while(c<m->verts+p->count)c*=2;float *v=realloc(m->v,c*8*sizeof(float));if(!v)goto bad;m->v=v;cap=c;}
        if(fread(m->v+(size_t)m->verts*8,sizeof(float)*8,p->count,f)!=p->count)goto bad;
        m->verts+=p->count;m->parts++;
    }
    if(!m->verts)goto bad;
    for(int k=0;k<3;k++){m->lo[k]=1e9f;m->hi[k]=-1e9f;}
    for(uint32_t i=0;i<m->verts;i++){const float *q=m->v+(size_t)i*8;
        for(int k=0;k<8;k++)if(!isfinite(q[k]))goto bad;
        for(int k=0;k<3;k++){if(q[k]<m->lo[k])m->lo[k]=q[k];if(q[k]>m->hi[k])m->hi[k]=q[k];}}
    if(m->hi[0]-m->lo[0]>40.f||m->hi[1]-m->lo[1]>20.f||m->hi[2]-m->lo[2]>40.f)goto bad; /* a machine, not a building */
    return 1;
bad:mxc_free(m);return 0;
}
/* TGA -> RGBA8 rows bottom-up (GL order). 1 = ok; *rgba is malloc'd. */
static inline int mxc_tga(FILE *f,unsigned char **rgba,int *w,int *h){
    *rgba=NULL;unsigned char hd[18];if(!f||fread(hd,1,18,f)!=18)return 0;
    const int type=hd[2],bpp=hd[16],W=hd[12]|hd[13]<<8,H=hd[14]|hd[15]<<8,top=(hd[17]>>5)&1;
    if((type!=2&&type!=10)||(bpp!=24&&bpp!=32)||W<1||H<1||W>4096||H>4096||hd[1])return 0;
    if(hd[0]&&fseek(f,hd[0],SEEK_CUR))return 0;
    const int B=bpp/8;const size_t n=(size_t)W*H;unsigned char *px=malloc(n*4);if(!px)return 0;
    unsigned char c[4]={0,0,0,255};size_t i=0;
    while(i<n){
        int run=1,raw=1;
        if(type==10){int r=fgetc(f);if(r<0)goto bad;run=(r&127)+1;raw=!(r&128);}
        else run=(int)(n-i);
        for(int k=0;k<run&&i<n;k++,i++){
            if(raw||k==0){if(fread(c,1,(size_t)B,f)!=(size_t)B)goto bad;if(B==3)c[3]=255;}
            unsigned char *d=px+i*4;d[0]=c[2];d[1]=c[1];d[2]=c[0];d[3]=c[3];
        }
    }
    if(top)for(int y=0;y<H/2;y++)for(int x=0;x<W*4;x++){unsigned char t=px[(size_t)y*W*4+x];px[(size_t)y*W*4+x]=px[(size_t)(H-1-y)*W*4+x];px[(size_t)(H-1-y)*W*4+x]=t;}
    *rgba=px;*w=W;*h=H;return 1;
bad:free(px);return 0;
}
/* Where a crew machine sits: footprint corners on the ground give its tilt. ground(x,z) -> world y (NAN = unknown).
   Returns 0 when the ground under it is unknown. m: column-major 4x4 model->world. */
typedef float (*MxcGround)(void *user,float x,float z);
static inline int mxc_place(float m[16],float x,float z,float fx,float fz,float half_w,float half_l,MxcGround ground,void *user){
    float fl=hypotf(fx,fz);if(!(fl>1e-4f))return 0;fx/=fl;fz/=fl;const float rx=fz,rz=-fx; /* right = up x forward */
    const float yf=ground(user,x+fx*half_l,z+fz*half_l),yb=ground(user,x-fx*half_l,z-fz*half_l);
    const float yr=ground(user,x+rx*half_w,z+rz*half_w),yl=ground(user,x-rx*half_w,z-rz*half_w);
    if(!isfinite(yf)||!isfinite(yb)||!isfinite(yr)||!isfinite(yl))return 0;
    /* forward and right along the ground, up = right x forward... kept orthonormal */
    float F[3]={fx*2.f*half_l,yf-yb,fz*2.f*half_l},R[3]={rx*2.f*half_w,yr-yl,rz*2.f*half_w};
    float U[3]={F[1]*R[2]-F[2]*R[1],F[2]*R[0]-F[0]*R[2],F[0]*R[1]-F[1]*R[0]}; /* forward x right = up */
    float ul=sqrtf(U[0]*U[0]+U[1]*U[1]+U[2]*U[2]),flen=sqrtf(F[0]*F[0]+F[1]*F[1]+F[2]*F[2]);if(!(ul>1e-6f)||!(flen>1e-6f))return 0;
    for(int k=0;k<3;k++){U[k]/=ul;F[k]/=flen;}
    if(U[1]<0.f)for(int k=0;k<3;k++)U[k]=-U[k];
    float Rr[3]={U[1]*F[2]-U[2]*F[1],U[2]*F[0]-U[0]*F[2],U[0]*F[1]-U[1]*F[0]}; /* up x forward = right */
    const float y=.25f*(yf+yb+yr+yl);
    m[0]=Rr[0];m[1]=Rr[1];m[2]=Rr[2];m[3]=0.f;m[4]=U[0];m[5]=U[1];m[6]=U[2];m[7]=0.f;
    m[8]=F[0];m[9]=F[1];m[10]=F[2];m[11]=0.f;m[12]=x;m[13]=y;m[14]=z;m[15]=1.f;return 1;
}
#endif
