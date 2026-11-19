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
/* Where a crew machine sits on the real ground (the published terrain: track shape, jumps, ruts). It rests on it
   like a vehicle: along its length on the highest ground under the front and under the rear (the upper hull of
   5 stations), across on the highest ground left and right, never through it - over a crest it bridges, on a
   face it pitches up, in a rut it sits on the edges. ground(x,z) -> world y (NAN = unknown). */
typedef float (*MxcGround)(void *user,float x,float z);
typedef struct {float y,pitch,roll;} MxcRest; /* centre height, dy per m forward, dy per m right */
/* the support line from above over points (u,h): the upper hull edge over u=0; returns its height at 0, slope in *k */
static inline float mxc_support(const float *u,const float *h,int n,float *k){
    /* on a vertex of the hull (a crest right under the middle) several lines fit: it balances between the
       steepest-down and steepest-up of them (any mix of lines through one hull point stays above all points) */
    float best=-1e30f,klo=0.f,khi=0.f,bk=0.f;int found=0;
    for(int i=0;i<n;i++)for(int j=0;j<n;j++){if(i==j||!(u[j]<=0.f&&u[i]>=0.f&&u[i]>u[j]))continue;
        const float kk=(h[i]-h[j])/(u[i]-u[j]),y0=h[j]-kk*u[j];int ok=1;
        for(int q=0;q<n&&ok;q++)ok=h[q]<=y0+kk*u[q]+1e-4f;
        if(!ok)continue;
        if(!found||y0>best+1e-4f){best=y0;klo=khi=kk;found=1;}
        else if(y0>best-1e-4f){if(kk<klo)klo=kk;if(kk>khi)khi=kk;}}
    bk=.5f*(klo+khi);
    if(!found){best=-1e30f;for(int q=0;q<n;q++)if(h[q]>best)best=h[q];bk=0.f;}
    *k=fmaxf(-1.2f,fminf(1.2f,bk));return best;
}
static inline int mxc_rest(MxcRest *r,float x,float z,float fx,float fz,float half_w,float half_l,MxcGround ground,void *user){
    float fl=hypotf(fx,fz);if(!(fl>1e-4f))return 0;fx/=fl;fz/=fl;const float rx=fz,rz=-fx; /* right = up x forward */
    enum{NS=5,NV=3};float g[NS][NV],u[NS],v[NV]={-half_w,0.f,half_w};
    for(int s=0;s<NS;s++){u[s]=half_l*((float)s/(NS-1)*2.f-1.f);
        for(int k=0;k<NV;k++){g[s][k]=ground(user,x+fx*u[s]+rx*v[k],z+fz*u[s]+rz*v[k]);if(!isfinite(g[s][k]))return 0;}}
    float h[NS];for(int s=0;s<NS;s++){h[s]=g[s][0];for(int k=1;k<NV;k++)if(g[s][k]>h[s])h[s]=g[s][k];}
    float kp;mxc_support(u,h,NS,&kp);
    float hv[NV];for(int k=0;k<NV;k++){hv[k]=-1e30f;for(int s=0;s<NS;s++){const float d=g[s][k]-kp*u[s];if(d>hv[k])hv[k]=d;}}
    float kr;const float y=mxc_support(v,hv,NV,&kr);
    r->y=y;r->pitch=kp;r->roll=kr;return isfinite(y);
}
/* column-major model->world from a rest pose */
static inline void mxc_pose(float m[16],float x,float z,float fx,float fz,const MxcRest *r){
    float fl=hypotf(fx,fz);fx/=fl;fz/=fl;const float rx=fz,rz=-fx;
    float F[3]={fx,r->pitch,fz},R[3]={rx,r->roll,rz};
    float U[3]={F[1]*R[2]-F[2]*R[1],F[2]*R[0]-F[0]*R[2],F[0]*R[1]-F[1]*R[0]}; /* forward x right = up */
    float ul=sqrtf(U[0]*U[0]+U[1]*U[1]+U[2]*U[2]),fn=sqrtf(F[0]*F[0]+F[1]*F[1]+F[2]*F[2]);
    for(int k=0;k<3;k++){U[k]/=ul;F[k]/=fn;}if(U[1]<0.f)for(int k=0;k<3;k++)U[k]=-U[k];
    float Rr[3]={U[1]*F[2]-U[2]*F[1],U[2]*F[0]-U[0]*F[2],U[0]*F[1]-U[1]*F[0]}; /* up x forward = right */
    m[0]=Rr[0];m[1]=Rr[1];m[2]=Rr[2];m[3]=0.f;m[4]=U[0];m[5]=U[1];m[6]=U[2];m[7]=0.f;
    m[8]=F[0];m[9]=F[1];m[10]=F[2];m[11]=0.f;m[12]=x;m[13]=r->y;m[14]=z;m[15]=1.f;
}
/* kept for callers that want the old one-call form */
static inline int mxc_place(float m[16],float x,float z,float fx,float fz,float half_w,float half_l,MxcGround ground,void *user){
    MxcRest r;if(!mxc_rest(&r,x,z,fx,fz,half_w,half_l,ground,user))return 0;mxc_pose(m,x,z,fx,fz,&r);return 1;
}
#endif
