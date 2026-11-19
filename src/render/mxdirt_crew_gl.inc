/* fork: the track crew (grader / water truck) drawn in the dirt pass, after the ground, with the game's depth:
   - models from plugins\mx_dirt_data\crew\ (mxdirt_crew_model.h): community\{dozer,truck} when
     [crew] community_models=1 in mx_dirt_render.ini and the files are there, else own\{grader,truck} (the mod's own);
   - set on the ground under it (the window's heights), tilted with it, wheels turning with the speed;
   - a soft contact shadow under it and, behind the water truck, a spray.
   Included by mxdirt_dirt_gl.c (uses its GL entry points). Loads lazily on the first frame a machine is out. */
#include "mxdirt_crew_model.h"
static const char crew_vs[]="#version 330 compatibility\n"
 "layout(location=0) in vec3 pos;layout(location=1) in vec3 nrm;layout(location=2) in vec2 uv;"
 "uniform mat4 view,projection,model;out vec3 wn;out vec2 tc;"
 "void main(){vec4 w=model*vec4(pos,1.);wn=mat3(model)*nrm;tc=uv;gl_Position=projection*view*w;}";
static const char crew_fs[]="#version 330 compatibility\n"
 "uniform sampler2D tex;uniform mat4 view;in vec3 wn;in vec2 tc;"
 "void main(){vec3 L=normalize(transpose(mat3(view))*gl_LightSource[0].position.xyz);"
 "vec3 sunc=gl_LightSource[0].diffuse.rgb;if(dot(sunc,vec3(1))<.05)sunc=vec3(1);sunc=min(sunc,vec3(1.5));"
 "vec3 n=normalize(wn);float d=max(dot(n,L),0.),sky=.5+.5*n.y;"
 "vec4 c=texture(tex,tc);gl_FragColor=vec4(c.rgb*(.30+.18*sky+.70*d*sunc),1.);}";
static const char fx_vs[]="#version 330 compatibility\n"
 "layout(location=0) in vec3 pos;layout(location=2) in vec2 uv;uniform mat4 view,projection;out vec2 tc;"
 "void main(){tc=uv;gl_Position=projection*view*vec4(pos,1.);}";
static const char fx_fs[]="#version 330 compatibility\n"
 "uniform int mode;uniform float time;in vec2 tc;"
 "float h(vec2 p){return fract(sin(dot(p,vec2(12.9898,78.233)))*43758.5453);}"
 "float nz(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);return mix(mix(h(i),h(i+vec2(1,0)),f.x),mix(h(i+vec2(0,1)),h(i+vec2(1,1)),f.x),f.y);}"
 "void main(){if(mode==0){float r=length(tc*2.-1.);gl_FragColor=vec4(0,0,0,.42*(1.-smoothstep(.35,1.,r)));return;}"
 /* spray: streaks falling from the bar (tc.y 0 at the bar, 1 on the ground), drifting back */
 "float s=nz(vec2(tc.x*26.,tc.y*5.-time*9.))*nz(vec2(tc.x*9.+3.,tc.y*2.-time*4.));"
 "float a=smoothstep(.25,.75,s)*(1.-smoothstep(.75,1.,tc.y))*smoothstep(0.,.08,tc.y)*(1.-pow(abs(tc.x*2.-1.),4.));"
 "gl_FragColor=vec4(vec3(.78,.83,.88),.55*a);}";
enum { CREW_MODELS=4 }; /* own grader, own truck, community dozer, community truck */
typedef struct {int state;MxcModel m;GLuint vao,vbo,tex[MXC_MAX_PARTS];float spin[MXC_MAX_PARTS];} CrewModel;
typedef struct CrewGl {
 int state;GLuint prog,fx,fx_vao,fx_vbo;GLint u_view,u_proj,u_model,u_tex,f_view,f_proj,f_mode,f_time;
 CrewModel model[CREW_MODELS];uint64_t last;double time;int community,ini_read;
} CrewGl;
static GLuint crew_program(MxDirtGl *p,const char *vs,const char *fs){
 GLuint s[2]={p->shader(X_VERTEX_SHADER),p->shader(X_FRAGMENT_SHADER)},prog=0;const char *src[2]={vs,fs};int ok=s[0]&&s[1];
 for(int k=0;k<2&&ok;k++){p->source(s[k],1,src+k,NULL);p->compile(s[k]);GLint st=0;p->shader_iv(s[k],X_COMPILE_STATUS,&st);ok=st;}
 if(ok){prog=p->program_create();if(prog){p->attach(prog,s[0]);p->attach(prog,s[1]);p->link(prog);GLint st=0;p->program_iv(prog,X_LINK_STATUS,&st);if(!st){p->delete_program(prog);prog=0;}}}
 for(int k=0;k<2;k++)if(s[k])p->delete_shader(s[k]);return prog;
}
static FILE *crew_open(const wchar_t *dir,const char *name){
 wchar_t path[1024];if(swprintf(path,1024,L"%ls\\%hs",dir,name)<0)return NULL;return _wfopen(path,L"rb");
}
/* 1 loaded, 0 not there / bad (state -1, never retried this session) */
static int crew_load(MxDirtGl *p,CrewModel *c,const wchar_t *dir){
 if(c->state)return c->state>0;c->state=-1;
 FILE *f=crew_open(dir,"model.mxm");const int ok=mxc_load(f,&c->m);if(f)fclose(f);if(!ok)return 0;
 GLint vao=0,buffer=0;glGetIntegerv(X_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(X_ARRAY_BUFFER_BINDING,&buffer);
 p->gen_vao(1,&c->vao);p->gen_buffers(1,&c->vbo);if(!c->vao||!c->vbo){mxc_free(&c->m);return 0;}
 p->bind_vao(c->vao);p->bind_buffer(X_ARRAY_BUFFER,c->vbo);p->buffer_data(X_ARRAY_BUFFER,(intptr_t)((size_t)c->m.verts*32),c->m.v,X_STATIC_DRAW);
 p->attribute(0,3,GL_FLOAT,GL_FALSE,32,(void*)0);p->enable_attribute(0);p->attribute(1,3,GL_FLOAT,GL_FALSE,32,(void*)12);p->enable_attribute(1);
 p->attribute(2,2,GL_FLOAT,GL_FALSE,32,(void*)24);p->enable_attribute(2);
 p->bind_vao((GLuint)vao);p->bind_buffer(X_ARRAY_BUFFER,(GLuint)buffer);
 free(c->m.v);c->m.v=NULL; /* on the GPU now */
 for(uint32_t i=0;i<c->m.parts;i++){const char *t=c->m.part[i].tex;if(!t[0])continue;
  for(uint32_t j=0;j<i;j++)if(!strcmp(c->m.part[j].tex,t)){c->tex[i]=c->tex[j];break;}if(c->tex[i])continue;
  unsigned char *rgba=NULL;int w=0,h=0;FILE *tf=crew_open(dir,t);const int good=mxc_tga(tf,&rgba,&w,&h);if(tf)fclose(tf);if(!good)continue;
  glGenTextures(1,c->tex+i);glBindTexture(GL_TEXTURE_2D,c->tex[i]);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);p->generate_mipmap(GL_TEXTURE_2D);free(rgba);}
 c->state=1;return 1;
}
static float crew_ground(void *user,float x,float z){
 const MxDirtWindow *w=user;const float tx=(x-w->texel0[0])/w->spacing_x,tz=(z-w->texel0[2])/w->spacing_z;
 const int i=(int)floorf(tx),j=(int)floorf(tz);if(i<0||j<0||i>MXDIRT_SIDE-2||j>MXDIRT_SIDE-2)return NAN;
 const float fx=tx-(float)i,fz=tz-(float)j;float v[4];
 for(int k=0;k<4;k++){const uint32_t a=((uint32_t)(i+(k&1))+w->shift_x)%MXDIRT_SIDE,b=((uint32_t)(j+(k>>1))+w->shift_z)%MXDIRT_SIDE;
  v[k]=(float)w->texels[2*((size_t)b*MXDIRT_SIDE+a)]/65535.f;}
 return w->texel0[1]+w->height_scale*((v[0]+(v[1]-v[0])*fx)*(1.f-fz)+(v[2]+(v[3]-v[2])*fx)*fz);
}
static void mul4(float o[16],const float a[16],const float b[16]){
 for(int c=0;c<4;c++)for(int r=0;r<4;r++){float s=0.f;for(int k=0;k<4;k++)s+=a[k*4+r]*b[c*4+k];o[c*4+r]=s;}}
static void xform(const float m[16],float x,float y,float z,float *o){
 o[0]=m[0]*x+m[4]*y+m[8]*z+m[12];o[1]=m[1]*x+m[5]*y+m[9]*z+m[13];o[2]=m[2]*x+m[6]*y+m[10]*z+m[14];}
static void crew_draw(MxDirtGl *p,const MxDirtCameraScope *scope,const MxDirtWindow *w,const MxDirtCrewInput *in){
 if(!in||in->kind<1||in->kind>2||!isfinite(in->x)||!isfinite(in->z))return;
 CrewGl *g=p->crew;
 if(!g){g=p->crew=calloc(1,sizeof(*g));if(!g)return;}
 if(g->state<0)return;
 if(!g->state){g->prog=crew_program(p,crew_vs,crew_fs);g->fx=crew_program(p,fx_vs,fx_fs);
  if(!g->prog||!g->fx){g->state=-1;return;}
  g->u_view=p->location(g->prog,"view");g->u_proj=p->location(g->prog,"projection");g->u_model=p->location(g->prog,"model");g->u_tex=p->location(g->prog,"tex");
  g->f_view=p->location(g->fx,"view");g->f_proj=p->location(g->fx,"projection");g->f_mode=p->location(g->fx,"mode");g->f_time=p->location(g->fx,"time");
  GLint vao=0,buffer=0;glGetIntegerv(X_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(X_ARRAY_BUFFER_BINDING,&buffer);
  p->gen_vao(1,&g->fx_vao);p->gen_buffers(1,&g->fx_vbo);if(!g->fx_vao||!g->fx_vbo){g->state=-1;return;}
  p->bind_vao(g->fx_vao);p->bind_buffer(X_ARRAY_BUFFER,g->fx_vbo);p->buffer_data(X_ARRAY_BUFFER,5*4*12*4,NULL,0x88e8 /* DYNAMIC_DRAW */);
  p->attribute(0,3,GL_FLOAT,GL_FALSE,20,(void*)0);p->enable_attribute(0);p->attribute(2,2,GL_FLOAT,GL_FALSE,20,(void*)12);p->enable_attribute(2);
  p->bind_vao((GLuint)vao);p->bind_buffer(X_ARRAY_BUFFER,(GLuint)buffer);g->state=1;}
 if(!g->ini_read){g->ini_read=1;g->community=1; /* [crew] community_models (default on when installed) */
  wchar_t exe[1024];DWORD n=GetModuleFileNameW(NULL,exe,1024);if(n&&n<1024){wchar_t *s=wcsrchr(exe,L'\\');if(s){s[1]=0;
   wchar_t ini[1100];swprintf(ini,1100,L"%lsplugins\\mx_dirt_render.ini",exe);g->community=GetPrivateProfileIntW(L"crew",L"community_models",1,ini)!=0;}}}
 /* which model: community first if wanted and present, the mod's own otherwise */
 static const char *const sub[CREW_MODELS]={"own\\grader","own\\truck","community\\dozer","community\\truck"};
 CrewModel *c=NULL;
 for(int pass=0;pass<2&&!c;pass++){const int k=pass==0?(g->community?2+in->kind-1:-1):in->kind-1;if(k<0)continue;
  CrewModel *m=g->model+k;
  if(!m->state){wchar_t dir[1024];DWORD n=GetModuleFileNameW(NULL,dir,1024);if(!n||n>=1024)return;wchar_t *s=wcsrchr(dir,L'\\');if(!s)return;s[1]=0;
   wchar_t full[1100];swprintf(full,1100,L"%lsplugins\\mx_dirt_data\\crew\\%hs",dir,sub[k]);crew_load(p,m,full);}
  if(m->state>0)c=m;}
 if(!c)return;
 /* time for the wheels and the spray */
 LARGE_INTEGER now,fq;QueryPerformanceCounter(&now);QueryPerformanceFrequency(&fq);
 const double t=(double)now.QuadPart/(double)fq.QuadPart;float dt=g->last?(float)(t-(double)g->last/(double)fq.QuadPart):0.f;g->last=(uint64_t)now.QuadPart;
 if(!(dt>0.f)||dt>.2f)dt=0.f;g->time+=dt;
 const float half_w=.5f*(c->m.hi[0]-c->m.lo[0]),half_l=.5f*(c->m.hi[2]-c->m.lo[2]);
 float M[16];if(!mxc_place(M,in->x,in->z,in->fwd[0],in->fwd[1],.8f*half_w,.8f*half_l,crew_ground,(void*)w))return; /* outside the height window: too far to see */
 /* the machine */
 p->use_program(g->prog);p->matrix(g->u_view,1,GL_FALSE,scope->modelview);p->matrix(g->u_proj,1,GL_FALSE,scope->projection);p->ui(g->u_tex,0);
 p->active_texture(X_TEXTURE0);p->bind_sampler(0,p->mipped);
 p->disable_indexed(GL_BLEND,0);p->color(0,1,1,1,0);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glDisable(GL_POLYGON_OFFSET_FILL);
 p->bind_vao(c->vao);
 for(uint32_t i=0;i<c->m.parts;i++){const MxcPart *q=c->m.part+i;float W[16];
  if(q->flags&1u&&q->radius>.05f){c->spin[i]=fmodf(c->spin[i]+in->speed*dt/q->radius,6.2831853f);
   const float a=c->spin[i],cs=cosf(a),sn=sinf(a),*v=q->pivot;
   /* T(pivot) * Rx(a) * T(-pivot) */
   const float R[16]={1,0,0,0, 0,cs,sn,0, 0,-sn,cs,0, 0,v[1]-cs*v[1]+sn*v[2],v[2]-sn*v[1]-cs*v[2],1};mul4(W,M,R);}
  else memcpy(W,M,sizeof(W));
  p->matrix(g->u_model,1,GL_FALSE,W);glBindTexture(GL_TEXTURE_2D,c->tex[i]?c->tex[i]:p->clods);
  glDrawArrays(GL_TRIANGLES,(GLint)q->first,(GLsizei)q->count);}
 /* contact shadow, then (water truck) the spray: blended, no depth writes */
 float fxv[2*4*5];int quads=0;
 {const float sw=1.15f*half_w,sl=1.1f*half_l,c4[4][2]={{-sw,-sl},{sw,-sl},{sw,sl},{-sw,sl}},uv4[4][2]={{0,0},{1,0},{1,1},{0,1}};
  for(int k=0;k<4;k++){float o[3];xform(M,c4[k][0],.05f,c4[k][1],o);float *d=fxv+k*5;d[0]=o[0];d[1]=o[1];d[2]=o[2];d[3]=uv4[k][0];d[4]=uv4[k][1];}quads=1;}
 if(in->kind==2){const float z0=c->m.lo[2]+.05f,bw=.95f*half_w;
  const float P[4][3]={{-bw,.65f,z0},{bw,.65f,z0},{1.25f*bw,.02f,z0-2.4f},{-1.25f*bw,.02f,z0-2.4f}},UV[4][2]={{0,0},{1,0},{1,1},{0,1}};
  for(int k=0;k<4;k++){float o[3];xform(M,P[k][0],P[k][1],P[k][2],o);float *d=fxv+20+k*5;d[0]=o[0];d[1]=o[1];d[2]=o[2];d[3]=UV[k][0];d[4]=UV[k][1];}quads=2;}
 p->use_program(g->fx);p->matrix(g->f_view,1,GL_FALSE,scope->modelview);p->matrix(g->f_proj,1,GL_FALSE,scope->projection);p->u1f(g->f_time,(float)fmod(g->time,1000.));
 p->bind_vao(g->fx_vao);p->bind_buffer(X_ARRAY_BUFFER,g->fx_vbo);p->buffer_data(X_ARRAY_BUFFER,(intptr_t)sizeof(fxv),fxv,0x88e8);
 p->enable_indexed(GL_BLEND,0);p->blend(0,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ONE);p->equation(0,0x8006,0x8006);glDepthMask(GL_FALSE);
 glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(-2.f,-2.f);
 for(int k=0;k<quads;k++){p->ui(g->f_mode,k);if(k==1)glDisable(GL_POLYGON_OFFSET_FILL);glDrawArrays(GL_TRIANGLE_FAN,4*k,4);}
}
