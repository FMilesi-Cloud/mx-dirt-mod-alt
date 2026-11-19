#ifndef MXB_SOIL_WEATHER_H
#define MXB_SOIL_WEATHER_H
/* fork, phases 3, 4 and 6: what time, rain and the track crew do to the worked soil (cells the tyres have
   touched: mat != 0). Runs between the wheel passes of a step (inside begin/end), visiting a few rows per step so
   it stays under ~0.3 ms: every worked cell is seen every few seconds, which is the pace of rain and drying.
   - crust (12): packed, dry soil that nobody rides grows a crust over ~10 min; water dissolves it.
   - water (4): rain falls on worked soil and runs to the lowest neighbour (track shape + wear + water); what
     reaches unworked ground soaks away. Standing water is a puddle: the cell is mud (mxb_soil_sim.h).
   - erosion (3): water running down a slope carries soil with it to the next cell (mass kept), so wet
     downhills cut channels along the ruts. Never below the hard base.
   Heights change only through mxb_ss_add (published like any wear); water and crust are display + physics. */
#include "mxb_soil_sim.h"
/* metres of water -> 0..255 (.2 mm units), stochastic rounding so light rain still adds up */
static inline uint8_t mxb_wx_units(MxbSoilSim *s,float m){float u=m/.0002f;if(!(u>0.f))return 0;if(u>=255.f)return 255;int i=(int)u;if(mxb_ss_rand(s)<u-(float)i)i++;return (uint8_t)(i>255?255:i);}
typedef struct {uint64_t visits,moved,eroded_cells;uint32_t sweeps;float sweep_acc,last_sweep;} MxbWeatherStats;
/* rain 0..1 (1 = heavy, ~10 mm/min); dt: seconds of this step; budget: worked cells to visit at most. */
static inline void mxb_soil_weather_step(MxbSoilSim *s,float dt,float rain,uint32_t budget,MxbWeatherStats *st){
    if(!s||!st||!s->water||!s->crust||!(dt>0.f)||dt>.5f)return;
    const MxbSoilSimParams *p=&s->p;const int S=(int)s->size;
    if(!isfinite(rain)||rain<0.f)rain=0.f;if(rain>1.f)rain=1.f;
    /* a row waits one whole sweep between visits: what happened to it meanwhile is applied at once */
    st->sweep_acc+=dt;const float since=st->last_sweep>0.f?fminf(30.f,st->last_sweep):1.5f;
    uint32_t done=0;int rows=0;const int max_rows=S/64>8?S/64:8; /* ~0.05 ms of row scanning per step */
    while(done<budget&&rows<max_rows){
        const int r=(int)s->wx_row;s->wx_row=(s->wx_row+1)%(uint32_t)S;rows++;
        if(!s->wx_row){st->sweeps++;st->last_sweep=st->sweep_acc;st->sweep_acc=0.f;}
        if(r<1||r>=S-1)continue;
        for(int q=1;q<S-1;q++){
            const size_t k=(size_t)r*S+q;if(!s->mat[k])continue;done++;
            const float mo=mxb_ss_moist(s,k),c=mxb_ss_c(s,k);
            /* crust */
            if(p->crust_gain>0.f){float dc=0.f; /* stochastic rounding: a visit adds a fraction of a step */
                if(mo<.30f&&s->loose[k]<.004f&&!s->water[k]&&c>p->compaction0+.05f)
                    dc=255.f*p->crust_gain*since/600.f*(1.f-mo/.30f)*fminf(1.f,(c-p->compaction0)/.25f);
                else if(s->crust[k]&&(mo>.48f||s->water[k]))dc=-255.f*since/60.f;
                if(dc!=0.f){int step=(int)dc;if(mxb_ss_rand(s)<fabsf(dc-(float)step))step+=dc>0.f?1:-1;
                    const int cr=s->crust[k]+step;s->crust[k]=(uint8_t)(cr<0?0:cr>255?255:cr);}}
            if(p->water_gain<=0.f){s->water[k]=0;continue;}
            /* water: rain in, soak away / dry out, run to the lowest neighbour */
            float wv=(float)s->water[k]*.0002f;
            wv+=rain*p->water_gain*.00017f*since;                               /* ~10 mm/min at rain 1 */
            wv-=since*(.00002f+.00004f*p->drying*(1.f-rain));                    /* soaks in, dries without rain */
            if(wv<=0.f){s->water[k]=0;continue;}
            const float bk=s->base?s->base[k]:0.f,H=bk+mxb_ss_m(s,k)+wv;
            static const int o[4][2]={{1,0},{-1,0},{0,1},{0,-1}};int best=-1;float bh=H;
            for(int j=0;j<4;j++){const size_t k2=(size_t)(r+o[j][1])*S+(q+o[j][0]);
                const float h2=(s->base?s->base[k2]:0.f)+mxb_ss_m(s,k2)+(float)s->water[k2]*.0002f;if(h2<bh){bh=h2;best=j;}}
            if(best>=0){
                const int q2=q+o[best][0],r2=r+o[best][1];const size_t k2=(size_t)r2*S+q2;
                const float mv=fminf(wv,.5f*(H-bh));wv-=mv;
                if(s->mat[k2])s->water[k2]=mxb_wx_units(s,(float)s->water[k2]*.0002f+mv);
                st->moved++;
                /* erosion: flowing water on a slope takes soil along, never below the hard base */
                const float slope=(H-bh)/s->cell_x;
                if(p->erosion_gain>0.f&&mv>.0005f&&slope>.04f&&mo>.40f){
                    const float pan=p->hardpan,room=mxb_ss_m(s,k)+pan;
                    float e=fminf(.0006f,p->erosion_gain*.2f*mv*fminf(1.f,slope));if(e>room)e=room;
                    if(e>0.f){mxb_ss_add(s,q,r,-e,0);mxb_ss_add(s,q2,r2,e,1);s->eroded_m3+=(double)e*s->cell_area;st->eroded_cells++;}}
            }
            s->water[k]=mxb_wx_units(s,wv);
            if(s->water[k]>10&&s->moist_off[k]<26)mxb_ss_moist_add(s,k,2); /* a puddle soaks its cell */
        }
    }
    st->visits+=done;
}
/* fork: rows [r0,r1) of a reset (factor 0) / smooth (factor .5) wave: the sim's wear there scaled toward the
   loaded surface, and for a reset the cell's whole history cleared. */
static inline void mxb_soil_wave_rows(MxbSoilSim *s,int r0,int r1,float factor,int reset){
    const int S=(int)s->size;if(r0<0)r0=0;if(r1>S)r1=S;
    for(int r=r0;r<r1;r++){int any=0;
        for(int q=0;q<S;q++){const size_t k=(size_t)r*S+q;const float d=s->h[k]-s->ref;
            if(d!=0.f){s->h[k]=s->ref+d*factor;s->volume_m3-=(double)(d*(1.f-factor)*s->per_unit)*s->cell_area;any=1;}
            s->loose[k]*=factor;s->clods[k]=0;s->print[k]=0;s->crust[k]=0;s->exposed[k]=0;
            if(reset){s->comp[k]=0;s->mat[k]=0;s->moist_off[k]=0;s->water[k]=0;}}
        if(any)for(int q=0;q<S;q+=128)s->tiles[(r/32)*s->tiles_x+q/128]=1;}
}
/* Track crew along a polyline segment (metres, terrain-centred), half-width hw:
   kind 1 grader: wear levelled to ~25 % (ruts filled, berms cut), soil re-packed, prints/clods/crust/bare base gone;
   kind 2 water truck: soil soaked (+.10 moisture), a film of water, crust dissolved. */
static inline void mxb_soil_service_band(MxbSoilSim *s,float x0,float z0,float x1,float z1,float hw,int kind){
    if(!s||!(hw>0.f))return;const float dx=x1-x0,dz=z1-z0,len=hypotf(dx,dz);if(!(len>1e-3f))return;
    const float d0=dx/len,d1=dz/len,n0=-d1,n1=d0,ext=len+hw+s->cell_x;
    float c0,r0;mxb_ss_texel(s,.5f*(x0+x1),.5f*(z0+z1),&c0,&r0);
    const int sc=(int)ceilf(ext/s->cell_x)+1,sr=(int)ceilf(ext/s->cell_z)+1;const int S=(int)s->size;
    s->cur_on=1;
    for(int r=(int)r0-sr;r<=(int)r0+sr;r++)for(int q=(int)c0-sc;q<=(int)c0+sc;q++){
        if(q<0||r<0||q>=S||r>=S)continue;
        float cx,cz;mxb_ss_world(s,q,r,&cx,&cz);const float px=cx-x0,pz=cz-z0,u=px*d0+pz*d1,v=px*n0+pz*n1;
        if(u<0.f||u>len||fabsf(v)>hw)continue;
        const size_t k=(size_t)r*S+q;const float edge=fminf(1.f,(hw-fabsf(v))/.4f); /* soft edge over 40 cm */
        if(kind==1){
            const float h=mxb_ss_m(s,k),t=h*.75f*edge;
            if(fabsf(t)>1e-5f)mxb_ss_add(s,q,r,-t,0);
            s->loose[k]=s->loose[k]*(1.f-edge);if(s->mat[k])mxb_ss_setc(s,k,s->p.compaction0+.12f*edge);
            s->clods[k]=0;s->print[k]=0;s->crust[k]=0;s->exposed[k]=0;
        }else if(kind==2){
            const int add=(int)(26.f*edge);if(s->moist_off[k]<add)s->moist_off[k]=(int8_t)add;
            if(s->mat[k]){int w=s->water[k]+(int)(12.f*edge);s->water[k]=(uint8_t)(w>255?255:w);}
            s->crust[k]=0;
        }
    }
    mxb_ss_rect_push(s);
}
#endif
