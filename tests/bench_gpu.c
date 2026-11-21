/* fork: GPU cost of the dirt pass (the real mxdirt_gl_draw), 2560x1440 into an FBO, on this machine's default GL
   adapter. A synthetic scene: a camera 1.6 m up looking down a worn 8 m wide track (ruts, berms, packed and loose
   soil) across untouched ground. Built twice to compare the shader with and without the untouched-ground shortcut:
     zig cc -std=c11 -O2 -fms-extensions -I src/render -I src/shared tests/bench_gpu.c src/render/mxdirt_dirt_gl.c src/render/mxdirt_dirt.c -lopengl32 -lgdi32 -luser32 -o build/bench_gpu.exe
     ... -DMXDIRT_NO_QUIET=1 ... -o build/bench_gpu_noquiet.exe */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mxdirt_dirt.h"
#include "mxdirt_dirt_gl.h"
#include "mxdirt_camera.h"
#ifndef MXDIRT_NO_QUIET
#define MXDIRT_NO_QUIET 0
#endif
/* the game runs on the dedicated GPU of a hybrid laptop: ask the drivers for it here too */
__declspec(dllexport) DWORD NvOptimusEnablement=1;__declspec(dllexport) int AmdPowerXpressRequestHighPerformance=1;
typedef void(APIENTRY *GenFb)(GLsizei,GLuint*);typedef void(APIENTRY *BindFb)(GLenum,GLuint);typedef void(APIENTRY *FbTex)(GLenum,GLenum,GLenum,GLuint,GLint);
typedef void(APIENTRY *GenRb)(GLsizei,GLuint*);typedef void(APIENTRY *BindRb)(GLenum,GLuint);typedef void(APIENTRY *RbStore)(GLenum,GLenum,GLsizei,GLsizei);typedef void(APIENTRY *FbRb)(GLenum,GLenum,GLenum,GLuint);
static void perspective(float *m,float fovy,float aspect,float n,float f){float t=1.f/tanf(fovy*.5f);memset(m,0,64);m[0]=t/aspect;m[5]=t;m[10]=(f+n)/(n-f);m[11]=-1.f;m[14]=2.f*f*n/(n-f);}
static void look(float *m,const float e[3],const float c[3]){float f[3]={c[0]-e[0],c[1]-e[1],c[2]-e[2]},l=sqrtf(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]);for(int k=0;k<3;k++)f[k]/=l;
    float s[3]={f[1]*0-f[2]*1,f[2]*0-f[0]*0,f[0]*1-f[1]*0};l=sqrtf(s[0]*s[0]+s[1]*s[1]+s[2]*s[2]);for(int k=0;k<3;k++)s[k]/=l;
    float u[3]={s[1]*f[2]-s[2]*f[1],s[2]*f[0]-s[0]*f[2],s[0]*f[1]-s[1]*f[0]};
    memset(m,0,64);m[0]=s[0];m[4]=s[1];m[8]=s[2];m[1]=u[0];m[5]=u[1];m[9]=u[2];m[2]=-f[0];m[6]=-f[1];m[10]=-f[2];m[15]=1.f;
    m[12]=-(s[0]*e[0]+s[1]*e[1]+s[2]*e[2]);m[13]=-(u[0]*e[0]+u[1]*e[1]+u[2]*e[2]);m[14]=f[0]*e[0]+f[1]*e[1]+f[2]*e[2];}
int main(int argc,char **argv){const int flat=argc>1&&!strcmp(argv[1],"flat");
    const int W=2560,H=1440,S=MXDIRT_SIDE;
    WNDCLASSA wc={0};wc.lpfnWndProc=DefWindowProcA;wc.hInstance=GetModuleHandleA(NULL);wc.lpszClassName="mxdg";RegisterClassA(&wc);
    HWND hw=CreateWindowA("mxdg","",WS_OVERLAPPEDWINDOW,0,0,64,64,NULL,NULL,wc.hInstance,NULL);HDC dc=GetDC(hw);
    PIXELFORMATDESCRIPTOR pfd={sizeof(pfd),1,PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,PFD_TYPE_RGBA,32};pfd.cDepthBits=24;
    SetPixelFormat(dc,ChoosePixelFormat(dc,&pfd),&pfd);HGLRC rc=wglCreateContext(dc);wglMakeCurrent(dc,rc);
    fprintf(stderr,"GL %s | %s\n",(const char*)glGetString(GL_VERSION),(const char*)glGetString(GL_RENDERER));
    GenFb gf=(GenFb)wglGetProcAddress("glGenFramebuffers");BindFb bf=(BindFb)wglGetProcAddress("glBindFramebuffer");FbTex ft=(FbTex)wglGetProcAddress("glFramebufferTexture2D");
    GenRb gr=(GenRb)wglGetProcAddress("glGenRenderbuffers");BindRb br=(BindRb)wglGetProcAddress("glBindRenderbuffer");RbStore rs=(RbStore)wglGetProcAddress("glRenderbufferStorage");FbRb fr=(FbRb)wglGetProcAddress("glFramebufferRenderbuffer");
    GLuint fb,col,dep;gf(1,&fb);bf(0x8d40,fb);glGenTextures(1,&col);glBindTexture(GL_TEXTURE_2D,col);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,W,H,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);ft(0x8d40,0x8ce0,GL_TEXTURE_2D,col,0);gr(1,&dep);br(0x8d41,dep);rs(0x8d41,0x81a6,W,H);fr(0x8d40,0x8d00,0x8d41,dep);
    glBindTexture(GL_TEXTURE_2D,0);glDrawBuffer(0x8ce0);glReadBuffer(0x8ce0);glViewport(0,0,W,H);
    /* the scene: window texel (i,j) at x = i*.24, z = j*.24; a track along z at x 58..66 m */
    static uint16_t tex[MXDIRT_SIDE*MXDIRT_SIDE*2];static unsigned char st[MXDIRT_SIDE*MXDIRT_SIDE*4],st2[MXDIRT_SIDE*MXDIRT_SIDE*4];
    const float sp=.24f,hs=20.f;const uint16_t base=16384;
    for(int j=0;j<S;j++)for(int i=0;i<S;i++){const float x=(float)i*sp;size_t k=(size_t)j*S+i;float d=0.f;int worn=0;
        if(!flat&&x>58.f&&x<66.f){worn=1;const float u=fmodf(x-58.f,.6f);d=-.05f*expf(-(u-.3f)*(u-.3f)/.008f)+.02f*expf(-(u)*(u)/.004f);d+=.03f*sinf((float)j*sp*1.4f)*(x<62.f);}
        tex[2*k]=(uint16_t)lrintf((float)base+d/hs*65535.f);tex[2*k+1]=base;
        unsigned char *q=st+4*k;q[0]=worn?150:0;q[1]=worn?60:0;q[2]=90;q[3]=0;}
    MxDirtDirty wdirty={MXDIRT_RECTS+1},sdirty={MXDIRT_RECTS+1};
    MxDirtWindow w;memset(&w,0,sizeof(w));w.spacing_x=w.spacing_z=sp;w.height_scale=hs;w.texel0[0]=0;w.texel0[1]=0;w.texel0[2]=0;
    w.bounds[0]=0;w.bounds[1]=0;w.bounds[2]=(float)(S-1)*sp;w.bounds[3]=(float)(S-1)*sp;w.texels=tex;w.dirty=&wdirty;
    const float eye[3]={62.f,(float)base/65535.f*hs+1.6f,20.f},at[3]={62.f,(float)base/65535.f*hs-.5f,40.f};memcpy(w.camera,eye,12);
    MxDirtCameraScope sc;memset(&sc,0,sizeof(sc));sc.generation=1;sc.serial=1;sc.context=rc;sc.thread=GetCurrentThreadId();sc.complete=1;
    look(sc.modelview,eye,at);perspective(sc.projection,1.3f,(float)W/(float)H,.2f,2000.f);
    glMatrixMode(GL_PROJECTION);glLoadMatrixf(sc.projection);glMatrixMode(GL_MODELVIEW);glLoadMatrixf(sc.modelview);
    {float L[4]={.4f,.8f,.3f,0.f};glLightfv(GL_LIGHT0,GL_POSITION,L);}
    MxDirtLook lk={1.f,.55f,45.f,70.f,1.f,0.f};MxDirtState state={1,1,st,st2,&sdirty};
    MxDirtGl *g=NULL;if(!mxdirt_gl_create(&g,1)){int a,b,c;mxdirt_gl_last_failure(&a,&b,&c);fprintf(stderr,"create failed stage %d\n",a);return 1;}
    const float gy=(float)base/65535.f*hs;
    #define GROUND(on) do{glClearColor(.4f,.45f,.3f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);if(on){glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);        glColor3f(.5f,.42f,.3f);glBegin(GL_QUADS);glVertex3f(-50.f,gy,-50.f);glVertex3f(-50.f,gy,250.f);glVertex3f(250.f,gy,250.f);glVertex3f(250.f,gy,-50.f);glEnd();glDisable(GL_DEPTH_TEST);}}while(0)
    LARGE_INTEGER f0,f1,fq;QueryPerformanceFrequency(&fq);double best=1e9,sum=0;int n=0;static double all[1000];
    for(int it=0;it<620;it++){GROUND(1);glFinish();
        QueryPerformanceCounter(&f0);int r=mxdirt_gl_draw(g,&sc,&w,&lk,&state,NULL,NULL);glFinish();QueryPerformanceCounter(&f1);
        if(r!=1&&it>5){fprintf(stderr,"draw result %d\n",r);return 1;}
        if(it>=20){double ms=(double)(f1.QuadPart-f0.QuadPart)*1000./(double)fq.QuadPart;sum+=ms;all[n++]=ms;if(ms<best)best=ms;}}
    for(int i=1;i<n;i++){double v=all[i];int j=i;while(j>0&&all[j-1]>v){all[j]=all[j-1];j--;}all[j]=v;}
    printf("%s%s: dirt pass %.3f ms median, %.3f ms mean, %.3f ms best (2560x1440, %d frames)\n",flat?"untouched ground, ":"",MXDIRT_NO_QUIET?"without shortcut":"with shortcut",all[n/2],sum/n,best,n);
    /* the depth guard: with the game's ground under it the pass draws; over sky (nothing in depth) it draws nothing */
    static unsigned char a[2560*1440*4],b[2560*1440*4];long changed[2];
    for(int sky=0;sky<2;sky++){GROUND(!sky);glFinish();glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,a);
        mxdirt_gl_draw(g,&sc,&w,&lk,&state,NULL,NULL);glFinish();glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,b);
        long c=0;for(long i=0;i<(long)W*H*4;i+=4)c+=abs(a[i]-b[i])+abs(a[i+1]-b[i+1])+abs(a[i+2]-b[i+2])>6;changed[sky]=c;}
    printf("depth guard: pixels changed over the ground %ld, over the sky %ld\n",changed[0],changed[1]);
    return changed[0]>1000&&changed[1]==0?0:2;
}
