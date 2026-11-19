/* fork: compiles and links every shader of the render plugin on this machine's OpenGL driver (a hidden window,
   a legacy = compatibility context, as the game's), so a GLSL error shows here and not as a silent "dirt_disabled"
   in the game. Also loads the crew models in a folder, if given, and checks placing one on a slope.
   Build and run:
     zig cc -std=c11 -I src/render -I src/core tests/test_shaders.c -lopengl32 -lgdi32 -luser32 -o build/test_shaders.exe
     build/test_shaders.exe [plugins\mx_dirt_data\crew] */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <assert.h>
#include <stdio.h>
#include "mxdirt_dirt.h"
#include "mxdirt_dirt_shader.h"
#include "mxdirt_crew_model.h"
#define FN(ret,name,args) static ret(APIENTRY *name)args
FN(GLuint,glCreateShader_,(GLenum));FN(void,glShaderSource_,(GLuint,GLsizei,const char*const*,const GLint*));FN(void,glCompileShader_,(GLuint));
FN(void,glGetShaderiv_,(GLuint,GLenum,GLint*));FN(void,glGetShaderInfoLog_,(GLuint,GLsizei,GLsizei*,char*));FN(GLuint,glCreateProgram_,(void));
FN(void,glAttachShader_,(GLuint,GLuint));FN(void,glLinkProgram_,(GLuint));FN(void,glGetProgramiv_,(GLuint,GLenum,GLint*));FN(void,glGetProgramInfoLog_,(GLuint,GLsizei,GLsizei*,char*));
FN(GLint,glGetUniformLocation_,(GLuint,const char*));
static GLuint compile(GLenum type,int n,const char *const *src,const char *what){
    GLuint s=glCreateShader_(type);glShaderSource_(s,n,src,NULL);glCompileShader_(s);GLint ok=0;glGetShaderiv_(s,0x8b81,&ok);
    if(!ok){static char log[8192];glGetShaderInfoLog_(s,sizeof(log),NULL,log);fprintf(stderr,"%s: compile FAILED\n%s\n",what,log);return 0;}return s;
}
static GLuint link(GLuint a,GLuint b,const char *what){
    GLuint p=glCreateProgram_();glAttachShader_(p,a);glAttachShader_(p,b);glLinkProgram_(p);GLint ok=0;glGetProgramiv_(p,0x8b82,&ok);
    if(!ok){static char log[8192];glGetProgramInfoLog_(p,sizeof(log),NULL,log);fprintf(stderr,"%s: link FAILED\n%s\n",what,log);return 0;}
    fprintf(stderr,"%s: ok\n",what);return p;
}
/* the crew shaders are in mxdirt_crew_gl.inc as static strings: pull them in without the GL code */
#define MXDIRT_CREW_SHADERS_ONLY
static const char *crew_src(const char *name);
static float flat(void *u,float x,float z){(void)u;(void)z;return .2f*x;} /* a 20 % slope up +x */
int main(int argc,char **argv){
    WNDCLASSA wc={0};wc.lpfnWndProc=DefWindowProcA;wc.hInstance=GetModuleHandleA(NULL);wc.lpszClassName="mxdt";RegisterClassA(&wc);
    HWND h=CreateWindowA("mxdt","",WS_OVERLAPPEDWINDOW,0,0,64,64,NULL,NULL,wc.hInstance,NULL);HDC dc=GetDC(h);
    PIXELFORMATDESCRIPTOR pfd={sizeof(pfd),1,PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,PFD_TYPE_RGBA,32};pfd.cDepthBits=24;
    SetPixelFormat(dc,ChoosePixelFormat(dc,&pfd),&pfd);HGLRC rc=wglCreateContext(dc);assert(rc&&wglMakeCurrent(dc,rc));
    fprintf(stderr,"GL %s | %s\n",(const char*)glGetString(GL_VERSION),(const char*)glGetString(GL_RENDERER));
#define L(n) *(PROC*)&n##_=wglGetProcAddress(#n);assert(n##_)
    L(glCreateShader);L(glShaderSource);L(glCompileShader);L(glGetShaderiv);L(glGetShaderInfoLog);L(glCreateProgram);L(glAttachShader);L(glLinkProgram);L(glGetProgramiv);L(glGetProgramInfoLog);L(glGetUniformLocation);
    int bad=0;
    {const char *v[1]={mxdirt_vertex_shader},*f0[3]={mxdirt_fragment_version,"\n",mxdirt_fragment_body},*f1[3]={mxdirt_fragment_version,mxdirt_fragment_sink,mxdirt_fragment_body};
     GLuint vs=compile(0x8b31,1,v,"terrain vertex"),fs=compile(0x8b30,3,f0,"terrain colour"),ss=compile(0x8b30,3,f1,"terrain sink");
     GLuint p0=vs&&fs?link(vs,fs,"terrain colour program"):0,p1=vs&&ss?link(vs,ss,"terrain sink program"):0;bad+=!p0+!p1;
     if(p0){static const char *const need[]={"state2","state","heights","clods","soft"};
      for(unsigned k=0;k<sizeof(need)/sizeof(*need);k++)if(glGetUniformLocation_(p0,need[k])<0){fprintf(stderr,"uniform %s optimised away\n",need[k]);bad++;}}}
    static const char *const pairs[2][3]={{"crew_vs","crew_fs","crew model program"},{"fx_vs","fx_fs","crew shadow/spray program"}};
    for(int k=0;k<2;k++){const char *v[1]={crew_src(pairs[k][0])},*f[1]={crew_src(pairs[k][1])};
        GLuint a=compile(0x8b31,1,v,pairs[k][0]),b=compile(0x8b30,1,f,pairs[k][1]);bad+=!(a&&b&&link(a,b,pairs[k][2]));}
    /* placing: on a 20 % slope up +x, heading +x: the machine pitches nose-up by ~11 degrees */
    float m[16];assert(mxc_place(m,10.f,5.f,1.f,0.f,1.f,2.f,flat,NULL));
    fprintf(stderr,"place: up (%.3f %.3f %.3f) forward (%.3f %.3f %.3f) at y %.2f\n",m[4],m[5],m[6],m[8],m[9],m[10],m[13]);
    assert(fabsf(m[13]-2.f)<1e-3f&&m[9]>.18f&&m[9]<.21f&&m[4]<0.f&&fabsf(m[0]*m[8]+m[1]*m[9]+m[2]*m[10])<1e-4f);
    if(argc>1){static const char *const sub[4]={"own\\grader","own\\truck","community\\dozer","community\\truck"};
        for(int k=0;k<4;k++){char p[1024];snprintf(p,sizeof(p),"%s\\%s\\model.mxm",argv[1],sub[k]);FILE *f=fopen(p,"rb");if(!f){fprintf(stderr,"%s: not there\n",sub[k]);continue;}
            MxcModel md;const int ok=mxc_load(f,&md);fclose(f);
            if(!ok){fprintf(stderr,"%s: BAD model\n",sub[k]);bad++;continue;}
            int wheels=0,tex=0;for(uint32_t i=0;i<md.parts;i++){wheels+=(md.part[i].flags&1)!=0;
                if(md.part[i].tex[0]){snprintf(p,sizeof(p),"%s\\%s\\%s",argv[1],sub[k],md.part[i].tex);FILE *t=fopen(p,"rb");unsigned char *px;int w,hh;
                    if(t&&mxc_tga(t,&px,&w,&hh)){tex++;free(px);}else{fprintf(stderr,"  texture %s BAD\n",md.part[i].tex);bad++;}if(t)fclose(t);}}
            fprintf(stderr,"%s: %u parts (%d wheels), %u triangles, %.1f x %.1f x %.1f m, %d textures ok\n",sub[k],md.parts,wheels,md.verts/3,md.hi[0]-md.lo[0],md.hi[1]-md.lo[1],md.hi[2]-md.lo[2],tex);
            assert(md.lo[1]>-.01f&&md.lo[1]<.01f);mxc_free(&md);}}
    if(bad){fprintf(stderr,"%d FAILED\n",bad);return 1;}
    puts("test_shaders ok");return 0;
}
/* the crew shader strings, cut from the .inc at run time (they live there, next to the GL code that uses them) */
static const char *crew_src(const char *name){
    static char buf[4][8192];static int used;FILE *f=fopen("src/render/mxdirt_crew_gl.inc","rb");if(!f)return "";
    static char all[65536];size_t n=fread(all,1,sizeof(all)-1,f);fclose(f);all[n]=0;
    char key[64];snprintf(key,sizeof(key),"static const char %s[]=",name);char *s=strstr(all,key);if(!s)return "";s+=strlen(key);
    char *o=buf[used++&3];size_t k=0;
    while(*s&&*s!=';'){ /* concatenate the C string literals up to the closing ; (a ; inside a literal is skipped over) */
        if(*s=='"'){s++;while(*s&&*s!='"'){if(*s=='\\'&&s[1]){s++;o[k++]=*s=='n'?'\n':*s;s++;continue;}o[k++]=*s++;}s++;}
        else if(s[0]=='/'&&s[1]=='*'){s=strstr(s,"*/");if(!s)break;s+=2;}else s++;
    }
    o[k]=0;return o;
}
