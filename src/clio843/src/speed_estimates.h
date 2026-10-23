/* Bounded, offline fallback for untagged urban streets. Every result carries ?.
 * No routing speeds, arbitrary nearest-road copying, rural/motorway guesses,
 * chained estimates or street/country-specific coordinates are used here.
 * CPM5 style alone cannot identify lanes, living streets or carriageway type. */
#include "urban_speed_defaults.h"
#define SPEED_NEAR_D2 72900u /* 270 E5 local units, approximately 300 metres */
#define SPEED_NEAR_NAMES 64
struct SpeedNeighbour {u32 name;u8 local,value,flags,conflict;};
static struct {int valid,speed,flags;char iso[3];s32 lat,lon;u32 time;} speed_estimate_cache;
static int speed_urban_default(const char*iso,int*flags){
 for(unsigned i=0;i<sizeof(urban_defaults)/sizeof(urban_defaults[0]);i++)
  if(urban_defaults[i].iso[0]==iso[0]&&urban_defaults[i].iso[1]==iso[1]){
   *flags=urban_defaults[i].flags;return urban_defaults[i].speed;
  }
 return 0;
}
static int speed_local_vote(const struct RoadCtx*r,int mph,int*flags){
 int a=r->speed,b=r->reverse_speed,af=r->speed_flags,bf=r->reverse_flags;
 /* Only original numeric evidence votes. Unknown opposite direction on a
  * one-way is allowed; conflicting directional/conditional values are not. */
 if((af|bf)&(1|4|8))return 0;
 if(a&&b&&(a!=b||(af&2)!=(bf&2)))return 0;
 if(!a){a=b;af=bf;}if(!a||!!(af&2)!=mph)return 0;
 *flags=af&2;return a;
}
static int speed_nearby_tile(struct Tile*t,s32 lat,s32 lon,int mph,struct SpeedNeighbour*v,int*count){
 if(!t||t->lvl!=6||t->len<8)return 1;
 const u8*p=t->data,*end=p+t->len;int ng=rd16(p),cq=coslat_q15(lat);p+=8;
 for(int g=0;g<ng&&p+4<=end;g++){
  int layer=p[0],cls=p[1],np=p[2],mi=p[3];p+=4;
  if(p+(unsigned)np*4>end)return 0;
  if(!layer&&cls>=3&&cls<=6&&mi!=255&&np>=2){
   int near=0;s32 la,lo;road_raw_point(t,p,&la,&lo);
   s32 ax=lon_scale(lo-lon,cq),ay=la-lat;
   for(int j=1;j<np;j++){road_raw_point(t,p+j*4,&la,&lo);s32 bx=lon_scale(lo-lon,cq),by=la-lat;
    if(seg_dist2(ax,ay,bx,by)<=SPEED_NEAR_D2)near=1;ax=bx;ay=by;}
   struct RoadCtx r;memzero(&r,sizeof(r));
   if(near&&read_road_meta(t,mi,&r)&&r.street[0]){
    u32 hash=2166136261u;for(int j=0;r.street[j];j++)hash=(hash^(u8)r.street[j])*16777619u;
    int k=0;while(k<*count&&v[k].name!=hash)k++;
    if(k==*count){if(k==SPEED_NEAR_NAMES)return 0;memzero(v+k,sizeof(v[k]));v[k].name=hash;(*count)++;}
    if(cls==6)v[k].local=1;
    int flags=0,value=speed_local_vote(&r,mph,&flags);
    if(value){if(v[k].value&&(v[k].value!=value||v[k].flags!=flags))v[k].conflict=1;
     else{v[k].value=(u8)value;v[k].flags=(u8)flags;}}
   }
  }p+=(unsigned)np*4;
 }return 1;
}
static int speed_estimate_choice(const struct SpeedNeighbour*v,int n,const char*iso,int*flags){
 int local=0,known=0,best=0,value=0,unit=0;
 for(int i=0;i<n;i++){local+=v[i].local!=0;if(v[i].conflict)known++;else if(v[i].value){known++;int votes=0;
  for(int j=0;j<n;j++)if(!v[j].conflict&&v[j].value==v[i].value&&v[j].flags==v[i].flags)votes++;
  if(votes>best){best=votes;value=v[i].value;unit=v[i].flags;}}}
 /* Five distinct local street names establish an urban network. Segmented
  * ways, duplicate tiles and repeatedly named carriageways count only once. */
 if(local<5)return 0;
 if(best>=3&&best*4>=known*3&&value<=((unit&2)?40:60)){*flags=unit|1;return value;}
 int df=0,d=speed_urban_default(iso,&df);if(!d)return 0;
 /* Conflicting local signs veto the country fallback, even if only one
  * mapped street supplies that evidence. Never replace a local 20 zone by 50. */
 for(int i=0;i<n;i++)if(v[i].conflict||(v[i].value&&(v[i].value!=d||(v[i].flags&2)!=(df&2))))return 0;
 *flags=df|1;return d;
}
static int speed_estimate_compute(s32 lat,s32 lon,int*flags){
 if(!region_iso[0]||regionfd<0)return 0;
 char iso[3];copystr(iso,sizeof(iso),region_iso);int saved_flags=region_flags;
 int cq=coslat_q15(lat);if(cq<=0)return 0;
 s32 dx=(s32)udiv32(270u*32768u,(u32)cq);int same=1;
 /* Do not mix evidence across country boundaries. Coarse polygon data still
  * has its documented border uncertainty; the display remains an estimate. */
 for(int a=-1;a<=1;a+=2)for(int b=-1;b<=1;b+=2){region_resolve(lat+a*270,lon+b*dx);if(!streq(region_iso,iso))same=0;}
 copystr(region_iso,sizeof(region_iso),iso);region_flags=saved_flags;if(!same)return 0;
 struct SpeedNeighbour v[SPEED_NEAR_NAMES];int n=0;
 int tx0=tile_div(lon-dx,6),tx1=tile_div(lon+dx,6),ty0=tile_div(lat-270,6),ty1=tile_div(lat+270,6);
 for(int y=ty0;y<=ty1;y++)for(int x=tx0;x<=tx1;x++)
  if(!speed_nearby_tile(get_tile(6,x,y),lat,lon,(region_flags&1)!=0,v,&n))return 0;
 return speed_estimate_choice(v,n,iso,flags);
}
static void road_estimate_missing(const struct RoadGeomRef*r){
 if(mapfmt!=5||!r||r->cls<3||r->cls>6||!roadctx.valid||!roadctx.street[0])return;
 /* Existing evidence in either direction always wins, including VAR/INF.
  * Bit 4 explicitly blocks inference on unsupported tags/special roads. */
 if(roadctx.speed||roadctx.reverse_speed||((roadctx.speed_flags|roadctx.reverse_flags)&(1|4|8)))return;
 if((roadctx.speed_flags&16)&&(roadctx.reverse_flags&16))return;
 region_resolve(gps.lat,gps.lon);
 u32 now=s_now_ms();int stale=!speed_estimate_cache.valid||!streq(speed_estimate_cache.iso,region_iso)||
  now-speed_estimate_cache.time>=5000u||road_join_d2(gps.lat,gps.lon,speed_estimate_cache.lat,speed_estimate_cache.lon)>3600u;
 if(stale){int flags=0,value=speed_estimate_compute(gps.lat,gps.lon,&flags);
  speed_estimate_cache.valid=1;speed_estimate_cache.speed=value;speed_estimate_cache.flags=flags;
  speed_estimate_cache.lat=gps.lat;speed_estimate_cache.lon=gps.lon;speed_estimate_cache.time=now;
  copystr(speed_estimate_cache.iso,sizeof(speed_estimate_cache.iso),region_iso);}
 if(!speed_estimate_cache.speed)return;
 if(!(roadctx.speed_flags&16)){roadctx.speed=speed_estimate_cache.speed;roadctx.speed_flags=(u8)speed_estimate_cache.flags;}
 if(!(roadctx.reverse_flags&16)){roadctx.reverse_speed=speed_estimate_cache.speed;roadctx.reverse_flags=(u8)speed_estimate_cache.flags;}
}
