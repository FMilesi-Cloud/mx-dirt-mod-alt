/* fork: in-game menu (F8) through MX Bikes' own HUD plugin API (DrawInit/Draw): no hooks. It replaces the
   PowerShell launcher: physics, look, track tools, presets, per-track profiles and Graphics Overhaul.
   Keyboard: F8 open/close, Tab / Shift+Tab tabs, Up/Down rows, Left/Right change (hold to repeat), Enter runs
   an action, Delete = default. Mouse: click a tab, a row, or the < > of a value (hold to repeat).
   Every change is written to the .ini at once and reaches the game through the plugins' live reload
   (~0.5 s); rows marked * apply on the next track load. Font and sprite: plugins\mx_dirt_data.
   Owner-thread work (reset, smooth, water) is only requested here and run by ra_menu_ops between steps. */
typedef struct {float pos[4][2];int sprite;unsigned long color;} MenuQuad;
typedef struct {char text[100];float pos[2];int font;float size;int justify;unsigned long color;} MenuString;
_Static_assert(sizeof(MenuQuad)==40&&sizeof(MenuString)==124,"MX Bikes draw ABI");
static int ra_settings_load(void);
enum {MK_VAL,MK_ACT,MK_INFO};
enum {A_RESET=1,A_SMOOTH=2,A_WATER=3,A_GRADER=5,A_TRUCK=6,A_PROFILE_SAVE=10,A_PROFILE_DEL,A_OH_RUTS_OFF=20,A_SAVE_AS=30,A_PRESET=31};
/* file 0 = mx_dirt.ini, 1 = mx_dirt_render.ini */
typedef struct {const char *label,*label_en;int kind,file;const char *section,*key;int lo,hi,step,def;const char *unit;int later,op;} MenuRow;
static const MenuRow menu_phys[]={
 {"Profundidad de surcos","Rut depth",MK_VAL,0,"soil","dirt_sim_depth_percent",10,300,5,60,"%",0,0},
 {"Firmeza de la pista","Track firmness",MK_VAL,0,"soil","dirt_sim_firmness_percent",0,100,5,55,"%",0,0},
 {"Dureza de la linea compactada","Packed line hardness",MK_VAL,0,"soil","packed_hardness_percent",25,200,5,100,"%",0,0},
 {"Humedad al regar","Moisture when watered",MK_VAL,0,"soil","dirt_sim_moisture_percent",0,100,5,35,"%",0,0},
 {"Velocidad de secado","Drying speed",MK_VAL,0,"soil","drying_percent",0,400,10,100,"%",0,0},
 {"Ancho de surco","Rut width",MK_VAL,0,"soil","rut_width_cm",0,80,2,36,"cm",0,0},
 {"Angulo max. de pared","Max wall angle",MK_VAL,0,"soil","wall_angle_deg",0,85,1,40,"deg",0,0},
 {"Erosion por inclinacion","Lean erosion",MK_VAL,0,"soil","lean_erosion_percent",0,200,10,100,"%",0,0},
 {"Huella suave sin traccion","Soft print without pull",MK_VAL,0,"soil","gentle_print_percent",0,100,10,70,"%",0,0},
 {"Marcas de botas (umbral)","Boot prints (threshold)",MK_VAL,0,"soil","boot_prints_threshold_percent",0,30,1,8,"%",0,0},
 {"Marcas de caidas","Crash marks",MK_VAL,0,"soil","crash_marks_percent",0,200,10,100,"%",0,0},
 {"Bermas que revientan","Berm blowouts",MK_VAL,0,"soil","berm_blowout_percent",0,200,10,100,"%",0,0},
 {"Base dura que aflora","Bare hard base",MK_VAL,0,"soil","exposed_base_percent",0,300,10,100,"%",0,0},
 {"Costra seca","Dry crust",MK_VAL,0,"soil","crust_percent",0,300,10,100,"%",0,0},
 {"Charcos","Puddles",MK_VAL,0,"soil","puddles_percent",0,300,10,100,"%",0,0},
 {"Erosion por lluvia","Rain erosion",MK_VAL,0,"soil","rain_erosion_percent",0,300,10,100,"%",0,0},
 {"Whoops de arena","Sand whoops",MK_VAL,0,"soil","sand_whoops_percent",0,300,10,100,"%",0,0},
 {"Labios y aterrizajes realistas","Realistic lips and landings",MK_VAL,0,"soil","lips_realistic",0,1,1,0,"",0,0},
 {"Cohesion de paredes","Wall cohesion",MK_VAL,0,"soil","wall_cohesion_percent",0,300,10,100,"%",0,0},
 {"Captura de surco","Rut capture",MK_VAL,0,"soil","rut_capture_percent",0,300,10,100,"%",0,0},
 {"Baches de frenado","Braking bumps",MK_VAL,0,"soil","braking_bumps_percent",0,200,10,100,"%",0,0},
 {"Baches rodando","Rolling bumps",MK_VAL,0,"soil","rolling_bumps_percent",0,100,5,60,"%",0,0},
 {"Terrones de roost","Roost clods",MK_VAL,0,"soil","roost_clods_percent",0,300,10,100,"%",0,0},
 {"Desgaste por vuelta","Wear per lap",MK_VAL,0,"soil","live_wear_percent",0,200,10,0,"%",0,0},
};
static const MenuRow menu_look[]={
 {"Shading de tierra","Dirt shading",MK_VAL,1,"renderer","dirt_material",0,1,1,1,"",0,0},
 {"Maquinas de la comunidad (si estan)","Community machines (if installed)",MK_VAL,1,"crew","community_models",0,1,1,1,"",1,0},
 {"Intensidad del shading","Shading strength",MK_VAL,1,"renderer","dirt_strength",0,200,10,135,"%",0,0},
 {"Relieve (oscurece lo hundido)","Relief (darkens dug ground)",MK_VAL,1,"renderer","dirt_relief",0,200,10,100,"%",0,0},
 {"Grano de tierra suelta","Loose dirt grain",MK_VAL,1,"renderer","dirt_loose",0,100,5,55,"%",0,0},
 {"Capa de tierra suelta","Loose dirt layer",MK_VAL,1,"renderer","soft_layer",0,1,1,1,"",0,0},
 {"Cubiertas hundidas en la capa","Tyres sink into the layer",MK_VAL,1,"renderer","tyre_sink",0,1,1,1,"",0,0},
 {"Distancia del detalle","Detail distance",MK_VAL,1,"renderer","dirt_fade_end",40,100,5,100,"m",0,0},
 {"Altura de la capa suelta sin tocar","Untouched layer height",MK_VAL,1,"renderer","soft_lift_percent",0,100,5,100,"%",0,0},
 {"Espesor de la capa suelta","Loose layer thickness",MK_VAL,0,"soil","soft_layer_mm",0,100,5,85,"mm",1,0},
};
static const MenuRow menu_track[]={
 {"Idioma / Language","Idioma / Language",MK_VAL,0,"menu","language",0,1,1,0,"",0,0},
 {"Resetear todo ahora","Reset everything now",MK_ACT,0,0,0,0,0,0,0,"",0,A_RESET},
 {"Alisar (surcos a la mitad)","Smooth (ruts halved)",MK_ACT,0,0,0,0,0,0,0,"",0,A_SMOOTH},
 {"Regar la pista","Water the track",MK_ACT,0,0,0,0,0,0,0,"",0,A_WATER},
 {"Saltar animaciones (todo al instante)","Skip animations (everything at once)",MK_VAL,0,"menu","skip_animations",0,1,1,0,"",0,0},
 {"Pasar la niveladora (por mi linea)","Run the grader (along my line)",MK_ACT,0,0,0,0,0,0,0,"",0,A_GRADER},
 {"Pasar el camion regador (por mi linea)","Run the water truck (along my line)",MK_ACT,0,0,0,0,0,0,0,"",0,A_TRUCK},
 {"Desgaste inicial (pilotos)","Starting wear (riders)",MK_VAL,0,"soil","prewear_riders",0,400,10,50,"",1,0},
 {"Dispersion de lineas","Line spread",MK_VAL,0,"soil","prewear_spread_cm",0,500,10,140,"cm",1,0},
 {"Mantener desgaste guardado","Keep saved wear",MK_VAL,0,"soil","prewear_fresh_only",0,1,1,1,"",1,0},
 {"Grabar mis vueltas","Record my laps",MK_VAL,0,"soil","record_ghosts",0,1,1,1,"",0,0},
 {"Humedad sigue el clima del juego","Moisture follows the game's weather",MK_VAL,0,"soil","follow_game_weather",0,1,1,1,"",0,0},
 {"Usar telemetria (frenos, caidas)","Use telemetry (brakes, crashes)",MK_VAL,0,"soil","use_telemetry",0,1,1,1,"",0,0},
 {"Cubiertas de la moto (ancho, tacos)","Bike's own tyres (width, knobs)",MK_VAL,0,"soil","use_bike_tyres",0,1,1,1,"",1,0},
 {"Seguir el slider Deformation","Follow the Deformation slider",MK_VAL,0,"soil","follow_game_deformation",0,1,1,1,"",1,0},
 {"Guardar perfil para esta pista","Save profile for this track",MK_ACT,0,0,0,0,0,0,0,"",0,A_PROFILE_SAVE},
 {"Borrar perfil de esta pista","Delete this track's profile",MK_ACT,0,0,0,0,0,0,0,"",0,A_PROFILE_DEL},
 {"Pedazos de pista por envio","Track tiles per hand-over",MK_VAL,0,"terrain","batch_tiles",1,4,1,4,"",0,0},
 {"Tiempo minimo entre envios","Min time between hand-overs",MK_VAL,0,"terrain","send_interval_ms",0,250,10,0,"ms",0,0},
 {"Mod activado (al reiniciar)","Mod enabled (on restart)",MK_VAL,0,"active","enabled",0,1,1,1,"",1,0},
};
static const MenuRow menu_oh[]={
 {"Humedad sigue la pista mojada de Overhaul","Moisture follows Overhaul's wet track",MK_VAL,0,"overhaul","sync_rain",0,1,1,1,"",0,0},
 {"Apagar surcos propios de Overhaul","Turn off Overhaul's own ruts",MK_ACT,0,0,0,0,0,0,0,"",0,A_OH_RUTS_OFF},
};
/* preset keys: everything a preset stores */
typedef struct {int file;const char *section,*key;} MenuKey;
static const MenuKey menu_pkeys[]={
 {0,"soil","dirt_sim_depth_percent"},{0,"soil","dirt_sim_firmness_percent"},{0,"soil","packed_hardness_percent"},{0,"soil","dirt_sim_moisture_percent"},
 {0,"soil","drying_percent"},{0,"soil","rut_width_cm"},{0,"soil","wall_angle_deg"},{0,"soil","wall_cohesion_percent"},{0,"soil","rut_capture_percent"},
 {0,"soil","braking_bumps_percent"},{0,"soil","rolling_bumps_percent"},{0,"soil","roost_clods_percent"},{0,"soil","live_wear_percent"},
 {0,"soil","prewear_riders"},{0,"soil","soft_layer_mm"},
 {1,"renderer","dirt_material"},{1,"renderer","dirt_strength"},{1,"renderer","dirt_relief"},{1,"renderer","dirt_loose"},
 {1,"renderer","soft_layer"},{1,"renderer","tyre_sink"},{1,"renderer","dirt_fade_end"},
 {0,"soil","lean_erosion_percent"},
};
enum {MENU_PK=sizeof(menu_pkeys)/sizeof(*menu_pkeys)};
/* built-in presets, same order as menu_pkeys; written once as editable files */
static const struct {const char *name;int v[MENU_PK];} menu_builtin[]={
 {"Realista",            {60,55,100,35,100,36,40,100,100,100,60,100,0,  50,85,  1,135,100,55,1,1,100,100}},
 {"Pro despues de manga",{70,50,100,35,100,36,42,110,120,120,70,120,60, 80,85,  1,135,110,55,1,1,100,110}},
 {"Arcade",              {80,55, 70,45,  0,44,32,150,150, 60,30,200,100,80,100, 1,160,100,55,1,1,100,70}},
 {"Barro",               {110,30,60,80, 40,40,45,120,130, 80,40,160,80, 50,100, 1,135,120,40,1,1,100,130}},
 {"Arena",               {120,15,50,25,100,50,30, 30,140, 70,70,150,60, 50,100, 1,135,100,70,1,1,100,120}},
 {"Rendimiento",         {60,55,100,35,100,36,40,100,100,100,60,100,0,  50,85,  0,135,100,55,0,0,60,100}},
};
enum {MENU_TABS=5,MENU_MAXROWS=24,MENU_MAXPRESETS=16};
static const char *const menu_tab_names[2][MENU_TABS]={{"Fisica","Visual","Pista","Presets","Overhaul"},{"Physics","Look","Track","Presets","Overhaul"}};
static char menu_dir[MAX_PATH],menu_ini[2][MAX_PATH],menu_oh_ini[MAX_PATH];
static int menu_lang; /* 0 espanol, 1 English ([menu] language) */
#define TR(es,en) (menu_lang?(en):(es))
static int menu_ready,menu_sprite,menu_open,menu_tab,menu_sel,menu_value[MENU_MAXROWS];
static char menu_presets[MENU_MAXPRESETS][48];static int menu_npresets;
static int menu_typing;static char menu_name[25];
static int menu_confirm_row=-1;static DWORD menu_confirm_tick,menu_hold_tick,menu_flush_tick,menu_oh_tick,menu_stat_tick;
static char menu_flash[96];static DWORD menu_flash_tick;
static unsigned char menu_down[256];
static double menu_gpu,menu_frames_seen;static int menu_render_state=-2,menu_render_drawing;
static float menu_oh_wet=-1.f;
static MenuQuad menu_quads[8];static MenuString menu_strings[96];
static int menu_nq,menu_ns,menu_cursor;static float menu_cx,menu_cy;
static void menu_say(const char *msg){snprintf(menu_flash,sizeof(menu_flash),"%s",msg);menu_flash_tick=GetTickCount();}
static int menu_paths(void){
 if(menu_dir[0])return 1;HMODULE self=NULL;
 if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)(uintptr_t)menu_paths,&self))return 0;
 char p[MAX_PATH];DWORD n=GetModuleFileNameA(self,p,MAX_PATH);if(!n||n>=MAX_PATH)return 0;
 char *slash=strrchr(p,'\\');if(!slash)return 0;slash[1]=0;
 if(snprintf(menu_ini[0],MAX_PATH,"%smx_dirt.ini",p)>=MAX_PATH||snprintf(menu_ini[1],MAX_PATH,"%smx_dirt_render.ini",p)>=MAX_PATH||
    snprintf(menu_oh_ini,MAX_PATH,"%soverhaul\\overhaul.ini",p)>=MAX_PATH)return 0;
 strcpy(menu_dir,p);return 1;
}
static const MenuRow *menu_rows(int tab,int *n){
 switch(tab){case 0:*n=(int)(sizeof(menu_phys)/sizeof(*menu_phys));return menu_phys;case 1:*n=(int)(sizeof(menu_look)/sizeof(*menu_look));return menu_look;
  case 2:*n=(int)(sizeof(menu_track)/sizeof(*menu_track));return menu_track;case 4:*n=(int)(sizeof(menu_oh)/sizeof(*menu_oh));return menu_oh;}
 *n=0;return NULL;
}
static int menu_row_count(void){int n;menu_rows(menu_tab,&n);return menu_tab==3?menu_npresets+1:n;}
static int menu_read(int file,const char *section,const char *key,int def){return (int)GetPrivateProfileIntA(section,key,def,menu_ini[file]);}
static void menu_write_int(const char *path,const char *section,const char *key,int v){char b[16];snprintf(b,sizeof(b),"%d",v);WritePrivateProfileStringA(section,key,b,path);}
static void menu_write(int file,const char *section,const char *key,int v){
 menu_write_int(menu_ini[file],section,key,v);
 if(file==1&&!strcmp(key,"dirt_fade_end"))menu_write_int(menu_ini[1],section,"dirt_fade_start",v-25);
}
static void menu_load(void){
 menu_lang=menu_read(0,"menu","language",0)==1;
 int n;const MenuRow *r=menu_rows(menu_tab,&n);
 for(int i=0;i<n&&i<MENU_MAXROWS;i++)if(r[i].kind==MK_VAL){int v=menu_read(r[i].file,r[i].section,r[i].key,r[i].def);menu_value[i]=v<r[i].lo?r[i].lo:v>r[i].hi?r[i].hi:v;}
}
/* ---- presets: plugins\mx_dirt_presets\<name>.ini, [soil] + [renderer] ---- */
static int menu_folder(char *out,const char *sub){if(snprintf(out,MAX_PATH,"%s%s",menu_dir,sub)>=MAX_PATH)return 0;CreateDirectoryA(out,NULL);return 1;}
static int menu_preset_path(char *out,const char *name){char d[MAX_PATH];return menu_folder(d,"mx_dirt_presets")&&snprintf(out,MAX_PATH,"%s\\%s.ini",d,name)<MAX_PATH;}
static void menu_preset_store(const char *path,const int *vals){
 for(int k=0;k<MENU_PK;k++){const MenuKey *q=menu_pkeys+k;
  menu_write_int(path,q->section,q->key,vals?vals[k]:menu_read(q->file,q->section,q->key,menu_builtin[0].v[k]));} /* a key missing from the ini = its default */
}
static int menu_preset_apply(const char *path){
 if(GetFileAttributesA(path)==INVALID_FILE_ATTRIBUTES)return 0;
 for(int k=0;k<MENU_PK;k++){const MenuKey *q=menu_pkeys+k;int v=(int)GetPrivateProfileIntA(q->section,q->key,-1,path);
  if(v>=0)menu_write(q->file,q->section,q->key,v);}
 return 1;
}
static void menu_presets_scan(void){
 char d[MAX_PATH],p[MAX_PATH];menu_npresets=0;if(!menu_folder(d,"mx_dirt_presets"))return;
 for(size_t i=0;i<sizeof(menu_builtin)/sizeof(*menu_builtin);i++)
  if(menu_preset_path(p,menu_builtin[i].name)&&GetFileAttributesA(p)==INVALID_FILE_ATTRIBUTES)menu_preset_store(p,menu_builtin[i].v);
 if(snprintf(p,MAX_PATH,"%s\\*.ini",d)>=MAX_PATH)return;
 WIN32_FIND_DATAA f;HANDLE h=FindFirstFileA(p,&f);if(h==INVALID_HANDLE_VALUE)return;
 do{size_t n=strlen(f.cFileName);if(n>4&&n-4<sizeof(menu_presets[0])&&menu_npresets<MENU_MAXPRESETS){
   memcpy(menu_presets[menu_npresets],f.cFileName,n-4);menu_presets[menu_npresets][n-4]=0;menu_npresets++;}}
 while(FindNextFileA(h,&f));FindClose(h);
}
/* ---- per-track profiles: plugins\mx_dirt_tracks\<track>.ini; _global.ini keeps the settings a profile replaced ---- */
static int menu_track_path(char *out,const char *track){
 char d[MAX_PATH],t[100];size_t j=0;if(!menu_folder(d,"mx_dirt_tracks"))return 0;
 for(size_t i=0;track[i]&&j<sizeof(t)-1;i++){char c=track[i];t[j++]=(isalnum((unsigned char)c)||c=='_'||c=='-')?c:'_';}t[j]=0;
 return j&&snprintf(out,MAX_PATH,"%s\\%s.ini",d,t)<MAX_PATH;
}
/* EventInit (owner): load this track's profile before its session is set up, or put back the global settings */
static void menu_profile_event(const char *track){
 if(!menu_paths()||!track||!track[0])return;
 char prof[MAX_PATH],glob[MAX_PATH];if(!menu_track_path(prof,track)||!menu_track_path(glob,"_global"))return;
 int has=GetFileAttributesA(prof)!=INVALID_FILE_ATTRIBUTES,saved=GetFileAttributesA(glob)!=INVALID_FILE_ATTRIBUTES;
 if(has){if(!saved)menu_preset_store(glob,NULL);menu_preset_apply(prof);}
 else if(saved){menu_preset_apply(glob);DeleteFileA(glob);}
 else return;
 ra_settings_load();
 if(ra_log){fprintf(ra_log,"{\"kind\":\"track_profile\",\"track\":\"%s\",\"profile\":%s}\n",track,has?"true":"false");fflush(ra_log);}
}
/* ---- Graphics Overhaul (read only, unless asked) ---- */
static float menu_oh_get(const char *section,const char *key,float def){
 char b[32];if(!GetPrivateProfileStringA(section,key,"",b,sizeof(b),menu_oh_ini)||!b[0])return def;return strtof(b,NULL);
}
static int menu_oh_present(void){return GetFileAttributesA(menu_oh_ini)!=INVALID_FILE_ATTRIBUTES;}
/* Wetness sources, read only and each optional: the game's own track conditions (0 dry .. 1 wet) and
   Graphics Overhaul's exported OverhaulTrackWetness() (float(void), its visual rain). The mod never needs either:
   "Regar la pista" waters the soil on its own. */
typedef float (*MenuWetFn)(void);
static float menu_game_wet=-1.f,menu_oh_track=-1.f,menu_soil=-1.f;static int menu_game_weather=-1;
static float menu_wet_overhaul(void){
 HMODULE h=GetModuleHandleA("overhaul64.dlo");MenuWetFn f=h?(MenuWetFn)(uintptr_t)GetProcAddress(h,"OverhaulTrackWetness"):NULL;
 if(!f)return -1.f;const float v=f();return isfinite(v)?fminf(1.f,fmaxf(0.f,v)):-1.f;
}
static void menu_oh_poll(void){
 DWORD now=GetTickCount();if(now-menu_oh_tick<2000)return;menu_oh_tick=now;
 float t=-1.f;int w=-1;if(ra_game_weather(&w,&t)){menu_game_weather=w;menu_game_wet=t>=0.f&&t<=1.f?t:-1.f;}
 menu_oh_track=menu_wet_overhaul();
 {float m;menu_soil=ra_session&&mxdirt_session_dirt_moisture_now(ra_session,&m)?m:-1.f;}
 float wet=0.f;
 if(menu_read(0,"soil","follow_game_weather",1)&&menu_game_wet>0.f)wet=menu_game_wet;
 if(menu_read(0,"overhaul","sync_rain",1)&&menu_oh_track>wet)wet=menu_oh_track;
 /* only a wetter track waters the soil; when it dries the soil dries by itself (drying speed) */
 if(wet>menu_oh_wet+.05f&&wet>0.f&&ra_session){int base=menu_read(0,"soil","dirt_sim_moisture_percent",35);
  InterlockedExchange(&ra_menu_moisture,(LONG)(base+(90-base)*wet));InterlockedExchange(&ra_menu_request,4);}
 menu_oh_wet=wet;
 InterlockedExchange(&ra_rain_permille,(LONG)lrintf(1000.f*(wet>.15f?wet:0.f))); /* fork (3, 4): rain on the soil */
}
/* ---- actions ---- */
static void menu_action(int op){
 char p[MAX_PATH];
 switch(op){
 case A_GRADER:case A_TRUCK:
  if(!ra_session){menu_say(TR("Primero entra a la pista","Get on the track first"));break;}
  {MxdirtPhaseStats q;if(!mxdirt_session_phase_stats(ra_session,&q)||q.path_points<50){menu_say(TR("Hace falta una vuelta grabada en esta pista","Needs a recorded lap on this track"));break;}}
  InterlockedExchange(&ra_menu_request,op);menu_say(op==A_GRADER?TR("La niveladora sale por tu linea","The grader is out on your line"):TR("El regador sale por tu linea","The water truck is out on your line"));break;
 case A_RESET:case A_SMOOTH:case A_WATER:
  if(!ra_session){menu_say(TR("Primero entra a la pista","Get on the track first"));break;}
  InterlockedExchange(&ra_menu_request,op);menu_say(op==A_RESET?TR("Pista reseteada","Track reset"):op==A_SMOOTH?TR("Pista alisada","Track smoothed"):TR("Pista regada","Track watered"));break;
 case A_PROFILE_SAVE:
  if(!ra_event_track[0]||!menu_track_path(p,ra_event_track)){menu_say(TR("Sin pista cargada","No track loaded"));break;}
  menu_preset_store(p,NULL);menu_say(TR("Perfil guardado para esta pista","Profile saved for this track"));break;
 case A_PROFILE_DEL:
  if(ra_event_track[0]&&menu_track_path(p,ra_event_track)&&DeleteFileA(p))menu_say(TR("Perfil borrado (vale desde la proxima carga)","Profile deleted (from the next load)"));else menu_say(TR("Esta pista no tiene perfil","This track has no profile"));break;
 case A_OH_RUTS_OFF:
  if(!menu_oh_present()){menu_say(TR("Overhaul no esta instalado","Overhaul is not installed"));break;}
  WritePrivateProfileStringA("rendering","ruts","0",menu_oh_ini);WritePrivateProfileStringA("rendering","physical","0",menu_oh_ini);
  menu_say(TR("Hecho. Cerra el panel F5 de Overhaul sin guardar, o lo pisa","Done. Close Overhaul's F5 panel without saving, or it overwrites this"));break;
 }
}
/* ---- input ---- */
static int menu_edge(int vk){int d=(GetAsyncKeyState(vk)&0x8000)!=0,e=d&&!menu_down[vk];menu_down[vk]=(unsigned char)d;return e;}
static int menu_held(int vk){return (GetAsyncKeyState(vk)&0x8000)!=0;}
static void menu_change(int i,int dir){
 int n;const MenuRow *r=menu_rows(menu_tab,&n);if(!r||i<0||i>=n||r[i].kind!=MK_VAL)return;
 int v=menu_value[i]+dir*r[i].step;v=v<r[i].lo?r[i].lo:v>r[i].hi?r[i].hi:v;
 if(v!=menu_value[i]){menu_value[i]=v;menu_write(r[i].file,r[i].section,r[i].key,v);if(!strcmp(r[i].key,"language"))menu_lang=v==1;}
}
static void menu_activate(int i){
 if(menu_tab==3){
  if(i==menu_npresets){menu_typing=1;menu_name[0]=0;memset(menu_down,1,sizeof(menu_down));return;}
  char p[MAX_PATH];if(i>=0&&i<menu_npresets&&menu_preset_path(p,menu_presets[i])&&menu_preset_apply(p)){char m[96];snprintf(m,sizeof(m),TR("Preset aplicado: %s","Preset applied: %s"),menu_presets[i]);menu_say(m);}
  return;}
 int n;const MenuRow *r=menu_rows(menu_tab,&n);if(!r||i<0||i>=n)return;
 if(r[i].kind==MK_VAL){if(r[i].hi==1)menu_change(i,menu_value[i]?-1:1);return;}
 if(r[i].kind!=MK_ACT)return;
 /* destructive actions ask twice */
 if(r[i].op==A_RESET||r[i].op==A_SMOOTH||r[i].op==A_GRADER||r[i].op==A_PROFILE_DEL){
  if(menu_confirm_row!=i||GetTickCount()-menu_confirm_tick>3000){menu_confirm_row=i;menu_confirm_tick=GetTickCount();return;}}
 menu_confirm_row=-1;menu_action(r[i].op);
}
static void menu_type(void){
 size_t n=strlen(menu_name);int shift=menu_held(VK_SHIFT);
 for(int vk='A';vk<='Z';vk++)if(menu_edge(vk)&&n<sizeof(menu_name)-1){menu_name[n++]=(char)(shift?vk:vk+32);menu_name[n]=0;}
 for(int vk='0';vk<='9';vk++)if(menu_edge(vk)&&n<sizeof(menu_name)-1){menu_name[n++]=(char)vk;menu_name[n]=0;}
 for(int vk=VK_NUMPAD0;vk<=VK_NUMPAD9;vk++)if(menu_edge(vk)&&n<sizeof(menu_name)-1){menu_name[n++]=(char)('0'+vk-VK_NUMPAD0);menu_name[n]=0;}
 if(menu_edge(VK_SPACE)&&n&&n<sizeof(menu_name)-1){menu_name[n++]=' ';menu_name[n]=0;}
 if((menu_edge(VK_OEM_MINUS)||menu_edge(VK_SUBTRACT))&&n<sizeof(menu_name)-1){menu_name[n++]='-';menu_name[n]=0;}
 if(menu_edge(VK_BACK)&&n)menu_name[--n]=0;
 if(menu_edge(VK_ESCAPE)){menu_typing=0;return;}
 if(menu_edge(VK_RETURN)){
  while(n&&menu_name[n-1]==' ')menu_name[--n]=0;
  char p[MAX_PATH];if(n&&menu_preset_path(p,menu_name)){menu_preset_store(p,NULL);menu_presets_scan();char m[96];snprintf(m,sizeof(m),TR("Preset guardado: %s","Preset saved: %s"),menu_name);menu_say(m);}
  menu_typing=0;}
}
/* layout (screen 0..1) */
static const float MX0=.03f,MX1=.47f,MY0=.07f,MROW=.026f,MFONT=.020f;
static float menu_row_y(int i){return MY0+MROW*(3.4f+(float)i);}
static void menu_mouse(int *hover){
 *hover=-1;HWND w=GetForegroundWindow();POINT c;RECT r;
 if(!w||!GetCursorPos(&c)||!ScreenToClient(w,&c)||!GetClientRect(w,&r)||r.right<=0||r.bottom<=0)return;
 const float mx=(float)c.x/(float)r.right,my=(float)c.y/(float)r.bottom;
 menu_cursor=1;menu_cx=mx;menu_cy=my; /* drawn last */
 if(mx<MX0||mx>MX1)return;
 const int click=menu_edge(VK_LBUTTON),hold=menu_held(VK_LBUTTON);DWORD now=GetTickCount();
 /* tab bar */
 const float ty=MY0+MROW*2.1f;
 if(my>=ty&&my<ty+MROW&&click){int t=(int)((mx-MX0)/((MX1-MX0)/MENU_TABS));if(t>=0&&t<MENU_TABS){menu_tab=t;menu_sel=0;menu_load();if(t==3)menu_presets_scan();}return;}
 const int rows=menu_row_count();
 for(int i=0;i<rows;i++){const float y=menu_row_y(i);if(my<y||my>=y+MROW)continue;*hover=i;
  int n;const MenuRow *rr=menu_rows(menu_tab,&n);const int is_val=menu_tab!=3&&rr&&i<n&&rr[i].kind==MK_VAL&&rr[i].hi>1;
  if(is_val&&mx>MX1-.13f){const int dir=mx<MX1-.065f?-1:1;
   if(click){menu_sel=i;menu_change(i,dir);menu_hold_tick=now+350;}
   else if(hold&&(LONG)(now-menu_hold_tick)>=0&&menu_sel==i){menu_change(i,dir);menu_hold_tick=now+60;}}
  else if(click){menu_sel=i;menu_activate(i);}
  return;}
}
static void menu_keys(void){
 const int rows=menu_row_count();DWORD now=GetTickCount();
 if(menu_edge(VK_TAB)){menu_tab=(menu_tab+(menu_held(VK_SHIFT)?MENU_TABS-1:1))%MENU_TABS;menu_sel=0;menu_load();if(menu_tab==3)menu_presets_scan();return;}
 if(rows){if(menu_edge(VK_UP))menu_sel=(menu_sel+rows-1)%rows;if(menu_edge(VK_DOWN))menu_sel=(menu_sel+1)%rows;}
 if(menu_sel>=rows)menu_sel=rows?rows-1:0;
 if(menu_edge(VK_RETURN))menu_activate(menu_sel);
 int n;const MenuRow *r=menu_rows(menu_tab,&n);
 if(menu_tab!=3&&r&&menu_sel<n&&r[menu_sel].kind==MK_VAL){
  if(menu_edge(VK_DELETE)&&menu_value[menu_sel]!=r[menu_sel].def){menu_value[menu_sel]=r[menu_sel].def;menu_write(r[menu_sel].file,r[menu_sel].section,r[menu_sel].key,r[menu_sel].def);}
  const int left=menu_held(VK_LEFT),right=menu_held(VK_RIGHT);
  if(left!=right){const int vk=left?VK_LEFT:VK_RIGHT;if(menu_edge(vk)){menu_change(menu_sel,right?1:-1);menu_hold_tick=now+350;}
   else if((LONG)(now-menu_hold_tick)>=0){menu_change(menu_sel,right?1:-1);menu_hold_tick=now+60;}}
  else{menu_down[VK_LEFT]=(unsigned char)left;menu_down[VK_RIGHT]=(unsigned char)right;}}
}
/* ---- drawing ---- */
static void menu_quad(float x0,float y0,float x1,float y1,unsigned long abgr){
 if(menu_nq>=7)return;MenuQuad *q=&menu_quads[menu_nq++];
 q->pos[0][0]=x0;q->pos[0][1]=y0;q->pos[1][0]=x0;q->pos[1][1]=y1;q->pos[2][0]=x1;q->pos[2][1]=y1;q->pos[3][0]=x1;q->pos[3][1]=y0;q->sprite=1;q->color=abgr;
}
static MenuString *menu_text(float x,float y,int justify,unsigned long abgr){
 static MenuString spare;const int cap=(int)(sizeof(menu_strings)/sizeof(*menu_strings));
 MenuString *s=menu_ns<cap?menu_strings+menu_ns++:&spare; /* never write past the array */
 memset(s,0,sizeof(*s));s->pos[0]=x;s->pos[1]=y;s->font=1;s->size=MFONT;s->justify=justify;s->color=abgr;return s;
}
static void menu_status(void){
 DWORD now=GetTickCount();if(now-menu_stat_tick<1000)return;menu_stat_tick=now;
 typedef int (*StatsFn)(double out[3]);HMODULE r=GetModuleHandleA("mx_dirt_render.dlo");
 StatsFn fn=r?(StatsFn)(uintptr_t)GetProcAddress(r,"MxDirtRenderStats"):NULL;double st[3];
 if(fn&&fn(st)){menu_gpu=st[0];menu_render_drawing=st[1]>menu_frames_seen;menu_frames_seen=st[1];menu_render_state=(int)st[2];}
 else menu_render_state=-2;
}
static void menu_draw(int hover){
 const unsigned long WHITE=0xFFFFFFFFul,GREY=0xFFB4B4B4ul,ACCENT=0xFF40C8FFul,BAD=0xFF4040FFul,GOOD=0xFF60E060ul;
 const int rows=menu_row_count();
 menu_quad(MX0,MY0,MX1,menu_row_y(rows)+MROW*2.2f,0xD8100C0Aul);
 /* title + status */
 snprintf(menu_text(MX0+.008f,MY0+.004f,0,ACCENT)->text,100,"MX DIRT");
 {const char *core=!ra_enabled?TR("mod apagado","mod off"):ra_fatal?TR("DESACTIVADO por un error","DISABLED by an error"):!ra_session?TR("esperando pista","waiting for a track"):TR("simulando","simulating");
  const char *rnd=menu_render_state==-2?TR("render ausente","no render"):menu_render_state==-1?TR("render BLOQUEADO por otro plugin","render BLOCKED by another plugin"):menu_render_drawing?TR("render dibujando","render drawing"):TR("render en pausa","render paused");
  MenuString *s=menu_text(MX1-.008f,MY0+.004f,2,(ra_fatal||menu_render_state==-1)?BAD:GREY);
  snprintf(s->text,100,"%s | %s | GPU %.2f ms | CPU %.2f ms",core,rnd,menu_gpu,ra_last_solver_ms);}
 if(ra_fatal)snprintf(menu_text(MX0+.008f,MY0+MROW*1.05f,0,BAD)->text,100,TR("Codigo %d - mira plugins\\mx_dirt.jsonl (first_fatal)","Code %d - see plugins\\mx_dirt.jsonl (first_fatal)"),ra_first_fault.reason);
 else{const char *src=menu_game_wet>0.f&&menu_oh_track>0.f?TR("juego + Overhaul","game + Overhaul"):menu_game_wet>0.f?TR("clima del juego","game weather"):menu_oh_track>0.f?"Overhaul":TR("ninguna","none");
  char g[16],o[16],m[16];
  if(menu_game_wet>=0.f)snprintf(g,sizeof(g),"%.0f%%",100.f*menu_game_wet);else snprintf(g,sizeof(g),"?");
  if(menu_oh_track>=0.f)snprintf(o,sizeof(o),"%.0f%%",100.f*menu_oh_track);else snprintf(o,sizeof(o),"-");
  if(menu_soil>=0.f)snprintf(m,sizeof(m),"%.0f%%",100.f*menu_soil);else snprintf(m,sizeof(m),"-");
  snprintf(menu_text(MX0+.008f,MY0+MROW*1.05f,0,GREY)->text,100,TR("Humedad suelo %s | juego: pista %s | Overhaul: %s | lluvia: %s","Soil moisture %s | game: track %s | Overhaul: %s | rain: %s"),m,g,o,src);}
 /* tabs */
 const float ty=MY0+MROW*2.1f,tw=(MX1-MX0)/MENU_TABS;
 menu_quad(MX0+tw*(float)menu_tab,ty,MX0+tw*(float)(menu_tab+1),ty+MROW,0xFF1E78D2ul);
 for(int t=0;t<MENU_TABS;t++)snprintf(menu_text(MX0+tw*((float)t+.5f),ty+.003f,1,t==menu_tab?WHITE:GREY)->text,100,"%s",menu_tab_names[menu_lang][t]);
 /* rows */
 if(rows)menu_quad(MX0,menu_row_y(menu_sel),MX1,menu_row_y(menu_sel)+MROW,0xFF6A4A1Eul);
 if(hover>=0&&hover!=menu_sel)menu_quad(MX0,menu_row_y(hover),MX1,menu_row_y(hover)+MROW,0x60FFFFFFul);
 int n;const MenuRow *r=menu_rows(menu_tab,&n);
 for(int i=0;i<rows;i++){const float y=menu_row_y(i)+.003f;const unsigned long c=i==menu_sel?WHITE:GREY;
  if(menu_tab==3){
   if(i<menu_npresets)snprintf(menu_text(MX0+.008f,y,0,c)->text,100,"%s",menu_presets[i]);
   else{snprintf(menu_text(MX0+.008f,y,0,ACCENT)->text,100,menu_typing?TR("Nombre: %s_","Name: %s_"):TR("+ Guardar la configuracion actual como...","+ Save the current settings as..."),menu_name);}
   if(i<menu_npresets)snprintf(menu_text(MX1-.008f,y,2,GREY)->text,100,"%s",TR("Enter / clic = aplicar","Enter / click = apply"));
   continue;}
  const MenuRow *q=r+i;
  snprintf(menu_text(MX0+.008f,y,0,q->kind==MK_ACT?ACCENT:c)->text,100,"%s%s",TR(q->label,q->label_en),q->later?" *":"");
  MenuString *v=menu_text(MX1-.008f,y,2,c);
  if(q->kind==MK_ACT){snprintf(v->text,100,"%s",menu_confirm_row==i&&GetTickCount()-menu_confirm_tick<=3000?TR("Seguro? de nuevo","Sure? again"):"[ Enter ]");continue;}
  const int x=menu_value[i];
  if(!strcmp(q->key,"language"))snprintf(v->text,100,"<  %s  >",x?"English":"Espanol");
  else if(q->hi==1)snprintf(v->text,100,"%s",x?TR("SI","YES"):"NO");
  else if(!strcmp(q->key,"rut_width_cm")&&!x)snprintf(v->text,100,"<  original  >");
  else if((!strcmp(q->key,"crash_marks_percent")||!strcmp(q->key,"berm_blowout_percent")||!strcmp(q->key,"exposed_base_percent")||!strcmp(q->key,"crust_percent")||
           !strcmp(q->key,"puddles_percent")||!strcmp(q->key,"rain_erosion_percent")||!strcmp(q->key,"sand_whoops_percent"))&&!x)snprintf(v->text,100,"%s",TR("<  apagado  >","<  off  >"));
  else if(!strcmp(q->key,"boot_prints_threshold_percent")&&!x)snprintf(v->text,100,"%s",TR("<  apagado  >","<  off  >"));
  else if(!strcmp(q->key,"wall_angle_deg")&&!x)snprintf(v->text,100,"%s",TR("<  sin tope  >","<  no cap  >"));
  else snprintf(v->text,100,"<  %d %s  >",x,q->unit);}
 if(menu_tab==4){const float y=menu_row_y(rows)+.003f;
  if(!menu_oh_present())snprintf(menu_text(MX0+.008f,y,0,GREY)->text,100,"%s",TR("Graphics Overhaul no esta instalado","Graphics Overhaul is not installed"));
  else{int ruts=(int)menu_oh_get("rendering","ruts",0.f),phys=(int)menu_oh_get("rendering","physical",0.f);
   snprintf(menu_text(MX0+.008f,y,0,(ruts||phys)?BAD:GOOD)->text,100,(ruts||phys)?TR("Overhaul tambien cava el terreno: apagalo arriba","Overhaul also digs the terrain: turn it off above"):TR("Overhaul: surcos propios apagados (ok)","Overhaul: its own ruts are off (ok)"));}}
 /* footer */
 {const float y=menu_row_y(rows)+MROW*1.1f;
  if(menu_flash[0]&&GetTickCount()-menu_flash_tick<4000)snprintf(menu_text(MX0+.008f,y,0,GOOD)->text,100,"%s",menu_flash);
  else snprintf(menu_text(MX0+.008f,y,0,GREY)->text,100,menu_typing?TR("Escribi el nombre - Enter guarda, Esc cancela","Type the name - Enter saves, Esc cancels"):TR("F8 cerrar | Tab pestanas | flechas o mouse | Supr = default | * = proxima carga","F8 close | Tab tabs | arrows or mouse | Del = default | * = next load"));}
}
API int DrawInit(int *sprites,char **sprite_names,int *fonts,char **font_names){
 static char sprite_name[]="mx_dirt_data\\white.tga",font_name[MAX_PATH];
 *sprites=0;*fonts=0;menu_ready=menu_sprite=0;
 if(!menu_paths())return 0;char f[MAX_PATH];
 /* The mod ships no font (a .fnt is someone's conversion of a typeface). First one found, paths relative to plugins:
    mx_dirt_data\menu.fnt (yours), the Inter font of the MXB-MRP3 HUD plugin if installed, any .fnt in mx_dirt_data. */
 {static const char *const tries[]={"mx_dirt_data\\menu.fnt","mxbmrp3_data\\fonts\\Inter-Regular.fnt"};font_name[0]=0;
  for(size_t i=0;i<sizeof(tries)/sizeof(*tries)&&!font_name[0];i++)
   if(snprintf(f,MAX_PATH,"%s%s",menu_dir,tries[i])<MAX_PATH&&GetFileAttributesA(f)!=INVALID_FILE_ATTRIBUTES)snprintf(font_name,MAX_PATH,"%s",tries[i]);
  if(!font_name[0]&&snprintf(f,MAX_PATH,"%smx_dirt_data\\*.fnt",menu_dir)<MAX_PATH){WIN32_FIND_DATAA d;HANDLE h=FindFirstFileA(f,&d);
   if(h!=INVALID_HANDLE_VALUE){snprintf(font_name,MAX_PATH,"mx_dirt_data\\%s",d.cFileName);FindClose(h);}}}
 if(!font_name[0])return 0; /* no font: no menu, the mod itself runs normally */
 *fonts=1;*font_names=font_name;
 if(snprintf(f,MAX_PATH,"%smx_dirt_data\\white.tga",menu_dir)<MAX_PATH&&GetFileAttributesA(f)!=INVALID_FILE_ATTRIBUTES){*sprites=1;*sprite_names=sprite_name;menu_sprite=1;}
 menu_ready=1;return 0;
}
API void Draw(int state,int *quads,void **quad_data,int *strings,void **string_data){
 (void)state;*quads=0;*strings=0;
 if(!menu_ready)return;
 {DWORD now=GetTickCount();if(now-menu_flush_tick>10000){menu_flush_tick=now;ra_flush();}} /* the log survives a crash (owner, between frames) */
 menu_oh_poll();
 DWORD pid=0;HWND front=GetForegroundWindow();const int ours=front&&GetWindowThreadProcessId(front,&pid)&&pid==GetCurrentProcessId();
 if(!ours){memset(menu_down,1,sizeof(menu_down));return;} /* keys held while away never count as presses */
 if(menu_edge(VK_F8)){menu_open=!menu_open;menu_typing=0;if(menu_open){menu_load();menu_presets_scan();memset(menu_down,1,sizeof(menu_down));menu_down[VK_F8]=1;}}
 if(!menu_open){ /* fork: a small line while a wave / crew job / soaking runs, so nothing changes out of nowhere */
  const LONG kind=ra_job_kind,pct=ra_job_pct;if(!kind||!ra_session)return;menu_nq=menu_ns=0;
  static const char *const es[6]={"","Reseteando la pista","Alisando la pista","Niveladora en tu linea","Regador en tu linea","La tierra se esta mojando"};
  static const char *const en[6]={"","Resetting the track","Smoothing the track","Grader on your line","Water truck on your line","The soil is soaking up water"};
  MenuString *m=menu_text(.03f,.93f,0,0xFFE8E8E8ul);
  if(kind==5)snprintf(m->text,100,"%s...",menu_lang?en[5]:es[5]);else snprintf(m->text,100,"%s  %ld %%",menu_lang?en[kind<6?kind:0]:es[kind<6?kind:0],(long)pct);
  *quads=0;*quad_data=menu_quads;*strings=menu_ns;*string_data=menu_strings;return;}
 menu_status();
 int hover=-1;menu_nq=menu_ns=0;menu_cursor=0;
 if(menu_typing)menu_type();else{menu_keys();menu_mouse(&hover);}
 menu_draw(hover);
 /* cursor: a small square at the mouse */
 if(menu_cursor&&menu_nq<8){const float x=menu_cx,y=menu_cy;MenuQuad *q=&menu_quads[menu_nq++];
  q->pos[0][0]=x;q->pos[0][1]=y;q->pos[1][0]=x;q->pos[1][1]=y+.018f;q->pos[2][0]=x+.010f;q->pos[2][1]=y+.012f;q->pos[3][0]=x+.004f;q->pos[3][1]=y+.004f;q->sprite=1;q->color=0xFFFFFFFFul;}
 *quads=menu_sprite?menu_nq:0;*quad_data=menu_quads;*strings=menu_ns;*string_data=menu_strings;
}
