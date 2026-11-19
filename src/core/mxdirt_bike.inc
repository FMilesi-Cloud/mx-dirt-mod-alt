/* fork: what the mod learns about the rider's bike, through official data only.
   1. Its tyres: <bike>.cfg names a tyre set (tyres { id = X }); X.cfg names each wheel's .tyre file; the .tyre
      gives TorusRadius (width = 2x), KnobsFactor and SlipSinkage. Read from unpacked folders in the game's
      bikes\ / tyres\ and in Documents\...\mods\bikes\ / mods\tyres\. Packed (.pkz) bikes keep the defaults.
   2. Telemetry (RunTelemetry, the plugin API): brake inputs and the crashed flag. The layout is checked twice
      before use: the size the game passes, and its wheel materials against the ones the mod reads itself. */
static char ra_save_dir[MAX_PATH]; /* Documents\PiBoSo\MX Bikes\ (Startup's savepath) */
static char ra_game_dir_override[MAX_PATH]; /* tests only: the game folder when not running inside it */
static float ra_tyre_width[2],ra_tyre_knobs_rear,ra_tyre_slip;static char ra_tyre_source[16]="default";
/* key = value lookup inside [from,to): first line whose first word is key (case-insensitive) */
static int ra_cfg_value(const char *from,const char *to,const char *key,char *out,size_t cap){
 const size_t kn=strlen(key);
 for(const char *p=from;p&&p<to;){
  while(p<to&&(*p==' '||*p=='\t'||*p=='\r'||*p=='\n'))p++;
  const char *eol=memchr(p,'\n',(size_t)(to-p));if(!eol)eol=to;
  if((size_t)(eol-p)>kn&&!_strnicmp(p,key,kn)&&(p[kn]==' '||p[kn]=='\t'||p[kn]=='=')){
   const char *v=memchr(p,'=',(size_t)(eol-p));if(v){v++;while(v<eol&&(*v==' '||*v=='\t'))v++;
    size_t n=(size_t)(eol-v);while(n&&(v[n-1]=='\r'||v[n-1]==' '||v[n-1]=='\t'))n--;
    if(n>=cap)n=cap-1;memcpy(out,v,n);out[n]=0;return 1;}}
  p=eol<to?eol+1:to;
 }
 return 0;
}
/* the block that follows a line whose first word is name: [start of its '{', end of file) is enough here */
static const char *ra_cfg_block(const char *buf,const char *end,const char *name){
 const size_t n=strlen(name);
 for(const char *p=buf;p<end;){
  while(p<end&&(*p==' '||*p=='\t'||*p=='\r'||*p=='\n'))p++;
  if((size_t)(end-p)>n&&!_strnicmp(p,name,n)&&(p[n]=='\r'||p[n]=='\n'||p[n]==' '||p[n]=='\t'||p[n]=='{'))return p+n;
  const char *eol=memchr(p,'\n',(size_t)(end-p));p=eol?eol+1:end;
 }
 return NULL;
}
static char *ra_read_text(const char *path,size_t *n){
 FILE *f=fopen(path,"rb");if(!f)return NULL;char *b=malloc(65537);size_t got=b?fread(b,1,65536,f):0;fclose(f);
 if(!b||!got){free(b);return NULL;}b[got]=0;*n=got;return b;
}
/* roots: game folder and Documents\...\mods */
static int ra_bike_roots(char roots[2][MAX_PATH]){
 char exe[MAX_PATH];DWORD n=GetModuleFileNameA(NULL,exe,MAX_PATH);if(!n||n>=MAX_PATH)return 0;
 char *slash=strrchr(exe,'\\');if(!slash)return 0;slash[1]=0;snprintf(roots[0],MAX_PATH,"%s",ra_game_dir_override[0]?ra_game_dir_override:exe);
 if(ra_save_dir[0])snprintf(roots[1],MAX_PATH,"%smods\\",ra_save_dir);else roots[1][0]=0;
 return 1;
}
static int ra_tyre_file(const char *path,float *width,float *knobs,float *slip){
 size_t n;char *b=ra_read_text(path,&n);if(!b)return 0;char v[64];int ok=0;
 if(ra_cfg_value(b,b+n,"TorusRadius",v,sizeof(v))){float t=strtof(v,NULL);if(t>.02f&&t<.15f){*width=2.f*t;ok=1;}}
 if(ra_cfg_value(b,b+n,"KnobsFactor",v,sizeof(v)))*knobs=strtof(v,NULL);
 if(ra_cfg_value(b,b+n,"SlipSinkage",v,sizeof(v)))*slip=strtof(v,NULL);
 free(b);return ok;
}
/* EventInit: find the bike's tyres. Leaves the defaults (0) when anything is missing or packed. */
static void ra_tyres_lookup(const char *bike){
 ra_tyre_width[0]=ra_tyre_width[1]=ra_tyre_knobs_rear=ra_tyre_slip=0.f;snprintf(ra_tyre_source,sizeof(ra_tyre_source),"default");
 char roots[2][MAX_PATH];if(!bike||!bike[0]||strpbrk(bike,"\\/:*?\"<>|")||!ra_bike_roots(roots))return;
 char set[64]="";
 for(int r=0;r<2&&!set[0];r++){if(!roots[r][0])continue;
  char pat[MAX_PATH];if(snprintf(pat,MAX_PATH,"%sbikes\\%s\\*.cfg",roots[r],bike)>=MAX_PATH)continue;
  WIN32_FIND_DATAA f;HANDLE h=FindFirstFileA(pat,&f);if(h==INVALID_HANDLE_VALUE)continue;
  do{char p[MAX_PATH];size_t n;if(snprintf(p,MAX_PATH,"%sbikes\\%s\\%s",roots[r],bike,f.cFileName)>=MAX_PATH)continue;
   char *b=ra_read_text(p,&n);if(!b)continue;const char *t=ra_cfg_block(b,b+n,"tyres");
   if(t)ra_cfg_value(t,b+n,"id",set,sizeof(set));free(b);}
  while(!set[0]&&FindNextFileA(h,&f));FindClose(h);}
 const int set_ok=set[0]&&!strpbrk(set,"\\/:*?\"<>|"); /* no set found: the log line still says so */
 for(int r=0;r<2&&set_ok;r++){if(!roots[r][0])continue;
  char cfg[MAX_PATH];size_t n;if(snprintf(cfg,MAX_PATH,"%styres\\%s\\%s.cfg",roots[r],set,set)>=MAX_PATH)continue;
  char *b=ra_read_text(cfg,&n);if(!b)continue;
  float width[2]={0,0},knobs[2]={0,0},slip[2]={0,0};int found=0;
  for(int w=0;w<2;w++){const char *blk=ra_cfg_block(b,b+n,w?"wheel1":"wheel0");char file[96];
   if(!blk||!ra_cfg_value(blk,b+n,"params",file,sizeof(file))||strpbrk(file,"\\/:*?\"<>|"))continue;
   char tp[MAX_PATH];if(snprintf(tp,MAX_PATH,"%styres\\%s\\%s",roots[r],set,file)<MAX_PATH&&ra_tyre_file(tp,&width[w],&knobs[w],&slip[w]))found++;}
  free(b);
  if(found==2){ra_tyre_width[0]=width[0];ra_tyre_width[1]=width[1];ra_tyre_knobs_rear=knobs[1];ra_tyre_slip=.5f*(slip[0]+slip[1]);
   snprintf(ra_tyre_source,sizeof(ra_tyre_source),"%s",r?"mods":"game");break;}
 }
 if(ra_log){fprintf(ra_log,"{\"kind\":\"bike_tyres\",\"bike\":\"%s\",\"set\":\"%s\",\"source\":\"%s\",\"width_m\":[%.3f,%.3f],\"knobs_rear\":%.2f,\"slip_sinkage\":%.2f}\n",
  strchr(bike,'"')?"?":bike,strchr(set,'"')?"?":set,ra_tyre_source,(double)ra_tyre_width[0],(double)ra_tyre_width[1],(double)ra_tyre_knobs_rear,(double)ra_tyre_slip);fflush(ra_log);}
}
/* ---- telemetry (MX Bikes plugin API bike data) ----
   Seen in game (Beta 21, Steam build 23566986): 188 bytes = 47 slots. Its tail is crashed, steer, throttle, front
   brake, rear brake, clutch, wheel speed x2, wheel material x2, then 3 more fields; the materials count from 1
   (0 = no contact), one above the index the mod reads itself. The layout is still not taken for granted: the record
   is kept raw (4-byte slots) and the material pair is found by matching it against the mod's own materials, at
   offset 0 or +1 (90 % of 150 samples, no rival over half). Crash, throttle and brakes sit at fixed distances
   before it and are checked for 200 more samples (0..1, crash 0/1) before anything is used. Any doubt: off, and the
   log says why with every slot dumped. */
enum { RA_TEL_SLOTS=64, RA_TEL_MIN_BYTES=120, RA_TEL_NEED=150 };
static int32_t ra_tel_raw[RA_TEL_SLOTS];static volatile LONG ra_tel_size,ra_tel_count;
static unsigned ra_tel_score[RA_TEL_SLOTS][2],ra_tel_checked,ra_tel_frames,ra_tel_valid;static LONG ra_tel_seen;
static int ra_tel_state,ra_tel_slot=-1,ra_tel_off; /* state: 0 finding the layout, 2 validating it, 1 in use, -1 refused */
static unsigned ra_use_telemetry=1,ra_use_bike_tyres=1;
static int ra_tel_size_ok(LONG n){return n>=RA_TEL_MIN_BYTES&&n<=(LONG)sizeof(ra_tel_raw)&&!(n&3);}
static void ra_telemetry_in(const void *data,int size){
 InterlockedExchange(&ra_tel_size,size);
 if(data&&ra_tel_size_ok(size)){memcpy(ra_tel_raw,data,(size_t)size);InterlockedIncrement(&ra_tel_count);}
}
static float ra_tel_f(int slot){float f;memcpy(&f,&ra_tel_raw[slot],4);return f;}
static void ra_tel_log(const char *used,const char *reason){
 if(!ra_log)return;
 fprintf(ra_log,"{\"kind\":\"telemetry\",\"used\":%s,\"reason\":\"%s\",\"bytes\":%ld,\"calls\":%ld,\"checked\":%u,\"material_slot\":%d,\"material_offset\":%d,\"mod_material\":[%d,%d],\"slots\":[",
  used,reason,(long)ra_tel_size,(long)ra_tel_count,ra_tel_checked,ra_tel_slot,ra_tel_off,ra_meta[0].material,ra_meta[1].material);
 const int n=ra_tel_size_ok(ra_tel_size)?(int)(ra_tel_size/4):0;
 for(int k=0;k<n;k++)fprintf(ra_log,"%s[%d,%.4g]",k?",":"",ra_tel_raw[k],(double)ra_tel_f(k)); /* int and float view of every slot */
 fprintf(ra_log,"]}\n");
}
/* owner, every completed frame: find and check the layout once per track, then hand the rider's brakes and
   throttle to the session. Returns 1 when the bike is down (no tyre wear from a bike sliding on its side). */
static int ra_telemetry_frame(void){
 float brake[2]={0,0},throttle=-1.f;int crashed=0;
 if(!ra_use_telemetry||ra_tel_state<0){if(ra_session)mxdirt_session_set_inputs(ra_session,brake,throttle);return 0;}
 const LONG count=ra_tel_count,size=ra_tel_size;const int fresh=count!=ra_tel_seen;ra_tel_seen=count;
 const int n=ra_tel_size_ok(size)?(int)(size/4):0;
 /* give up only after plenty of riding: ~2000 records (about 3 minutes at 10 per second) or no data at all */
 if(ra_tel_state==0&&(++ra_tel_frames>=60000u||(count>=2000&&ra_tel_checked<RA_TEL_NEED))){ra_tel_state=-1;ra_tel_log("false",!count?(size?"size":"no_data"):"materials");}
 if(ra_tel_state==0&&fresh&&n&&ra_meta[0].availability.normal_contact&&ra_meta[1].availability.normal_contact&&ra_meta[0].material>0&&ra_meta[1].material>0){
  ra_tel_checked++;
  for(int k=8;k+1<n;k++)for(int o=0;o<2;o++)if(ra_tel_raw[k]==ra_meta[0].material+o&&ra_tel_raw[k+1]==ra_meta[1].material+o)ra_tel_score[k][o]++;
  if(ra_tel_checked>=RA_TEL_NEED){int best=-1,bo=0;unsigned b1=0,b2=0;
   for(int k=0;k<n;k++)for(int o=0;o<2;o++){const unsigned v=ra_tel_score[k][o];if(v>b1){b2=b1;b1=v;best=k;bo=o;}else if(v>b2)b2=v;}
   if(best>=8&&b1*10>=ra_tel_checked*9&&b2*2<ra_tel_checked){ra_tel_slot=best;ra_tel_off=bo;ra_tel_state=2;}
   else{ra_tel_state=-1;ra_tel_log("false","materials");}}
 }
 const int m=ra_tel_slot;
 if(ra_tel_state>=1&&fresh&&m>=8&&m+1<n){
  const float th=ra_tel_f(m-6),fb=ra_tel_f(m-5),rb=ra_tel_f(m-4);const int32_t cr=ra_tel_raw[m-8];
  #define RA_TEL_UNIT(x) (isfinite(x)&&(x)>=-.001f&&(x)<=1.001f)
  const int plausible=RA_TEL_UNIT(th)&&RA_TEL_UNIT(fb)&&RA_TEL_UNIT(rb)&&(cr==0||cr==1);
  #undef RA_TEL_UNIT
  if(!plausible){ra_tel_state=-1;ra_tel_log("false","layout");}
  else if(ra_tel_state==2&&++ra_tel_valid>=200){ra_tel_state=1;ra_tel_log("true","ok");}
 }
 if(ra_tel_state==1&&m>=8&&m+1<n){
  throttle=fminf(1.f,fmaxf(0.f,ra_tel_f(m-6)));brake[0]=fminf(1.f,fmaxf(0.f,ra_tel_f(m-5)));brake[1]=fminf(1.f,fmaxf(0.f,ra_tel_f(m-4)));crashed=ra_tel_raw[m-8]==1;}
 if(ra_session)mxdirt_session_set_inputs(ra_session,brake,throttle);
 return crashed;
}
static int ra_tyre_mem_done;
static void ra_telemetry_track(void){ra_tel_checked=ra_tel_frames=ra_tel_valid=0;ra_tel_state=0;ra_tel_seen=0;ra_tel_slot=-1;ra_tel_off=0;memset(ra_tel_score,0,sizeof(ra_tel_score));ra_tyre_mem_done=0;}
/* ---- tyre width from the game's own wheel definition (packed bikes, e.g. the OEM ones) ----
   The mod already reads the wheel radius at definition+0xa0 (double). A .tyre lists Radius, TorusRadius, Width,
   so the profile most likely follows: torus (double, m) at +0xa8, width (double, mm) at +0xb0. Used only when both
   fit that pattern; the raw doubles around them are logged once per track ("wheel_definition") to confirm it. */
static void ra_tyre_from_memory(void){
 if(ra_tyre_mem_done||!ra_session||!ra_use_bike_tyres||strcmp(ra_tyre_source,"default"))return;
 if(!ra_meta[0].definition||!ra_meta[1].definition)return;
 ra_tyre_mem_done=1;double d[2][12];float width[2]={0,0};int fit=0;
 for(int w=0;w<2;w++){
  if(!ra_read(NULL,ra_meta[w].definition+0x90,d[w],sizeof(d[w]))){memset(d[w],0,sizeof(d[w]));continue;}
  const double radius=d[w][2],torus=d[w][3],mm=d[w][4]; /* +0xa0, +0xa8, +0xb0 */
  if(radius>.2&&radius<.6&&torus>.03&&torus<.12&&torus<.4*radius&&mm>=40.&&mm<=120.){width[w]=(float)(2.*torus);fit++;}
 }
 if(fit==2&&mxdirt_session_set_tyres(ra_session,width,0.f,0.f))snprintf(ra_tyre_source,sizeof(ra_tyre_source),"memory");
 if(ra_log){fprintf(ra_log,"{\"kind\":\"wheel_definition\",\"from_offset\":\"0x90\",\"front\":[");
  for(int k=0;k<12;k++)fprintf(ra_log,"%s%.6g",k?",":"",d[0][k]);fprintf(ra_log,"],\"rear\":[");
  for(int k=0;k<12;k++)fprintf(ra_log,"%s%.6g",k?",":"",d[1][k]);
  fprintf(ra_log,"],\"used\":%s,\"width_m\":[%.3f,%.3f]}\n",fit==2?"true":"false",(double)width[0],(double)width[1]);}
}
