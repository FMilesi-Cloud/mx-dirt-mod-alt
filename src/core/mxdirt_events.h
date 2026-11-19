#ifndef MXDIRT_EVENTS_H
#define MXDIRT_EVENTS_H
/* fork (phase 2): crash marks and berm blowouts, on the dirt sim's grid (metres, terrain-centred). Mass is kept:
   what a groove or a slumping crest takes is put down beside it. nm: metres per native unit of the contacts. */
#include "mxb_soil_prewear.h"
#include "mxdirt_contact.h"
typedef struct {
    int crashed,crash_have;float crash_gain,crash_x,crash_z,crash_m;uint64_t crash_frames,crash_cuts;
    float berm_gain,berm_x,berm_z;uint64_t berm_blows,berm_frames[4];
    float turn_f[2][2],turn_acc[2],turn_side[2];int turn_have[2]; /* per wheel: last heading, lateral accel (m/s2), turn side */
} MxdEvents;
/* fork: ground height (m) under a point: the track shape plus the wear */
static inline float mxd_ground_m(const MxbSoilSim *m,float X,float Z){return mxb_ss_sample_m(m,X,Z)+mxb_pw_base(m,X,Z);}
/* fork: dig a band (centre X,Z, unit d, a..b m along d, half-width hw) up to depth m with a rounded floor, never below
   -floor_m of wear. Returns the volume taken (m^3), for the caller to put down again: mass is kept. */
static inline float mxd_band_cut(MxbSoilSim *m,float X,float Z,const float d[2],float a,float b,float hw,float depth,float floor_m){
    if(!(depth>0.f)||!(hw>0.f)||!(b>a))return 0.f;
    const float n[2]={-d[1],d[0]},ext=fmaxf(fabsf(a),fabsf(b))+hw+m->cell_x;float c0,r0;mxb_ss_texel(m,X,Z,&c0,&r0);
    const int sc=(int)ceilf(ext/m->cell_x)+1,sr=(int)ceilf(ext/m->cell_z)+1;if(sc>48||sr>48)return 0.f;double vol=0.;
    for(int r=(int)r0-sr;r<=(int)r0+sr;r++)for(int c=(int)c0-sc;c<=(int)c0+sc;c++){
        if(c<0||r<0||c>=(int)m->size||r>=(int)m->size)continue;
        float cx,cz;mxb_ss_world(m,c,r,&cx,&cz);const float px=cx-X,pz=cz-Z,u=px*d[0]+pz*d[1],v=(px*n[0]+pz*n[1])/hw;
        if(u<a||u>b||fabsf(v)>=1.f)continue;
        const size_t k=(size_t)r*m->size+(size_t)c;const float cut=fminf(depth*(1.f-v*v),mxb_ss_m(m,k)+floor_m);
        if(cut>0.f){mxb_ss_add(m,c,r,-cut,0);vol+=(double)cut*m->cell_area;}
    }
    return (float)vol;
}
/* soil at a point: packed 0..1 (above the start compaction), moisture 0..1, loose metres */
static inline void mxd_soil_at(const MxbSoilSim *m,float X,float Z,float *packed,float *moist,float *loose){
    float col,row;mxb_ss_texel(m,X,Z,&col,&row);const int q=(int)lrintf(col),r=(int)lrintf(row);
    *packed=0.f;*moist=m->p.moisture;*loose=0.f;if(q<0||r<0||q>=(int)m->size||r>=(int)m->size)return;
    const size_t k=(size_t)r*m->size+(size_t)q;*packed=fminf(1.f,fmaxf(0.f,(mxb_ss_c(m,k)-m->p.compaction0)/.35f));*moist=mxb_ss_moist(m,k);*loose=m->loose[k];
}
/* fork: crash marks (idea 2). While the game says crashed (or, without telemetry, both wheels lie almost flat) and
   the bike slides on deformable ground, it ploughs a groove along its slide: ~.5 m wide at speed, narrowing as it
   stops, a few cm deep (more in loose damp soil, little on packed ground). The soil goes to the sides and piles up
   where it stops. */
static inline void mxd_crash_step(MxdEvents *s,MxbSoilSim *m,const MxdirtContact in[2],float nm){
    if(!m||!(s->crash_gain>0.f))return;
    const int lying=fabsf(in[0].lean)>.8f&&fabsf(in[1].lean)>.8f;
    if(!s->crashed&&!lying){s->crash_have=0;return;}
    s->crash_frames++;
    if(!(in[0].contact&&in[0].deformation)&&!(in[1].contact&&in[1].deformation)){s->crash_have=0;return;}
    const float X=.5f*(in[0].point[0]+in[1].point[0])*nm,Z=.5f*(in[0].point[2]+in[1].point[2])*nm;
    const float vx=.5f*(in[0].center_velocity[0]+in[1].center_velocity[0])*nm,vz=.5f*(in[0].center_velocity[2]+in[1].center_velocity[2])*nm,v=hypotf(vx,vz);
    if(!isfinite(X)||!isfinite(Z)||!isfinite(v)||v<.6f){s->crash_have=0;return;}
    if(!s->crash_have){s->crash_have=1;s->crash_x=X;s->crash_z=Z;return;}
    const float dx=X-s->crash_x,dz=Z-s->crash_z,len=hypotf(dx,dz);
    if(len>3.f){s->crash_x=X;s->crash_z=Z;return;} /* a respawn, not a slide */
    if(len<.06f)return;
    const float d[2]={dx/len,dz/len},n[2]={-d[1],d[0]},hw=.09f+.16f*fminf(1.f,v/8.f);
    float pk,mo,lo;mxd_soil_at(m,X,Z,&pk,&mo,&lo);
    const float soft=(1.f-.75f*pk)*(.6f+.8f*fminf(1.f,mo/.5f))*(1.f+fminf(1.f,lo/.03f));
    const float depth=fminf(.07f,s->crash_gain*.03f*fminf(1.f,v/6.f)*soft);
    m->cur_on=1;
    const float vol=mxd_band_cut(m,s->crash_x,s->crash_z,d,0.f,len,hw,depth,m->p.hardpan);
    if(vol>0.f){
        for(int j=-1;j<=1;j+=2)mxb_ss_deposit(m,s->crash_x+n[0]*(float)j*(hw+.07f),s->crash_z+n[1]*(float)j*(hw+.07f),d,0.f,len,.07f,.3f*vol);
        mxb_ss_deposit(m,X,Z,d,0.f,.3f,hw,.4f*vol);}
    mxb_ss_rect_push(m);
    s->crash_x=X;s->crash_z=Z;s->crash_m+=len;s->crash_cuts++;
}
/* fork: berms that blow out (idea 13). The push a wheel puts on the ground outside it in a turn is load x lateral
   g, the lateral acceleration taken from speed x turn rate (not the lean: on a berm face the tyre stands square to
   the ground and its lean to it is ~0). Only loose soil can blow: the spoil ruts and roost piled up. A berm of the
   track itself (or packed spoil) holds; loose spoil on it holds better than a loose ridge alone. When the push beats
   the hold, the loose crest slumps and that soil goes over the outside. */
static inline void mxd_berm_step(MxdEvents *s,MxbSoilSim *m,const MxdirtContact in[2],float nm){
    if(!m)return;
    for(int w=0;w<2;w++){
        const MxdirtContact *i=in+w;const float fl=hypotf(i->forward[0],i->forward[2]),ll=hypotf(i->lateral[0],i->lateral[2]);
        if(!i->contact||!(fl>1e-4f)||!(ll>1e-4f)||!(i->dt>0.f)||s->crashed){s->turn_have[w]=0;continue;}
        const float f[2]={i->forward[0]/fl,i->forward[2]/fl},la[2]={i->lateral[0]/ll,i->lateral[2]/ll};
        const float v=(i->center_velocity[0]*f[0]+i->center_velocity[2]*f[1])*nm;
        /* the turn from the way the wheel actually travels (its velocity), not where it points: the front wheel
           steers, and every steering correction read as a hard turn (2133 blowouts in one ride) */
        const float vx=i->center_velocity[0],vz=i->center_velocity[2],vl=hypotf(vx,vz);
        if(!(vl>1e-4f)){s->turn_have[w]=0;continue;}
        const float d[2]={vx/vl,vz/vl};float acc=0.f,side=0.f;
        if(s->turn_have[w]){const float df0=d[0]-s->turn_f[w][0],df1=d[1]-s->turn_f[w][1];side=df0*la[0]+df1*la[1]; /* toward the turn centre */
            const float rate=hypotf(df0,df1)/i->dt;acc=fabsf(v)*rate;if(!isfinite(acc)||rate>3.f)acc=0.f;}
        s->turn_f[w][0]=d[0];s->turn_f[w][1]=d[1];s->turn_have[w]=1;
        s->turn_acc[w]+=(acc-s->turn_acc[w])*fminf(1.f,i->dt/.2f); /* ~0.2 s: no jolt from one bumpy frame */
        if(fabsf(side)>1e-6f)s->turn_side[w]=side>0.f?1.f:-1.f;
        if(!(s->berm_gain>0.f)||!i->deformation||!(v>5.f)||!(s->turn_acc[w]>4.f)||!s->turn_side[w]||!isfinite(i->load_n))continue;
        const float o[2]={-s->turn_side[w]*la[0],-s->turn_side[w]*la[1]}; /* away from the turn centre */
        const float X=i->point[0]*nm,Z=i->point[2]*nm,h0=mxd_ground_m(m,X,Z);
        float rise=0.f,at=0.f;for(int k=0;k<5;k++){const float u=.25f+.15f*(float)k,r=mxd_ground_m(m,X+o[0]*u,Z+o[1]*u)-h0;if(r>rise){rise=r;at=u;}}
        if(!(rise>.07f))continue;
        const float cx=X+o[0]*at,cz=Z+o[1]*at;float pk,mo,lo;mxd_soil_at(m,cx,cz,&pk,&mo,&lo);
        lo=fminf(lo,rise);if(!(lo>.03f))continue; /* a solid berm: nothing loose to blow */
        const float push=i->load_n*fminf(3.f,s->turn_acc[w]/9.81f);
        const float wet=1.f-1.2f*fabsf(mo-.4f),solid=fmaxf(0.f,rise-lo)/rise;
        const float hold=3800.f*(.35f+pk)*fmaxf(.3f,wet)*fminf(1.4f,.12f/lo)*(1.f+2.f*solid)/s->berm_gain;
        const float ratio=push/fmaxf(1.f,hold);s->berm_frames[ratio<.5f?0:ratio<.8f?1:ratio<1.f?2:3]++;
        if(ratio<1.f||(s->berm_blows&&hypotf(cx-s->berm_x,cz-s->berm_z)<1.5f))continue; /* not the same crest twice */
        const float drop=fminf(.08f,fminf(.7f*lo,.45f*rise*fminf(1.5f,ratio-.6f)));
        m->cur_on=1;const float vol=mxd_band_cut(m,cx,cz,f,-.45f,.45f,.16f,drop,m->p.hardpan);
        if(vol>0.f)mxb_ss_deposit(m,cx+o[0]*.38f,cz+o[1]*.38f,f,-.55f,.55f,.22f,vol);
        mxb_ss_rect_push(m);s->berm_x=cx;s->berm_z=cz;s->berm_blows++;
    }
}
#endif
