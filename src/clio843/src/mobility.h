/* Navigation-independent GPS history and contextual destination learner.
 * Fixed-capacity, integer-only; no dependency on map files, routes or UI time.
 * The caller is responsible for calling mob_finish only on Barcelona power-off.
 */
#ifndef CLIO_MOBILITY_H
#define CLIO_MOBILITY_H
#define MOB_JOURNEYS 512
#define MOB_TRACE 32
#define MOB_TRAFFIC 8192
#define MOB_STAGE 1024
#define MOB_RAW_LIMIT (64u*1024u*1024u)
#define MOB_UNKNOWN 0xffffffffu
#ifdef HOST_TEST
#define MOB_HISTORY CLIO_HOST_ROOT "history.dat"
#define MOB_PREV CLIO_HOST_ROOT "history.prev.dat"
#define MOB_MODEL CLIO_HOST_ROOT "mobility.dat"
#define MOB_TEMP CLIO_HOST_ROOT "mobility.tmp"
#else
#define MOB_HISTORY "./clio843/history.dat"
#define MOB_PREV "./clio843/history.prev.dat"
#define MOB_MODEL "./clio843/mobility.dat"
#define MOB_TEMP "./clio843/mobility.tmp"
#endif
struct MobPoint {s32 lat,lon;};
struct MobJourney {
 struct MobPoint origin,dest;
 u32 depart,serial,distance,duration;
 u32 trace_n;
 struct MobPoint trace[MOB_TRACE];
};
struct MobRoad {
 /* Direction-specific fingerprint of exact ordered geometry, independent of
  * pack-local state/arc numbers. Zero is not a valid fingerprint. */
 u32 key,stamp,mean_ds;
 u16 bucket,count;
};
struct MobModel {
 u32 magic,serial,n,head;
 struct MobJourney journey[MOB_JOURNEYS];
 struct MobRoad road[MOB_TRAFFIC];
};
struct MobRecord {
 u32 kind,epoch,elapsed;
 s32 lat,lon;
 u16 mph10,heading;
 u32 check;
};
enum {MOB_BEGIN=0x3142474du,MOB_SAMPLE=0x3153474du,MOB_END=0x3145474du};
static struct MobModel mob;
static struct MobJourney mob_live;
static struct MobRoad mob_stage[MOB_STAGE];
static unsigned mob_stage_n;
static struct MobRecord mob_raw[30];
static unsigned mob_raw_n,mob_raw_bytes;
static int mob_raw_fd=-1,mob_raw_ok,mob_recording,mob_origin_valid,mob_started,mob_have_sample;
static u32 mob_begin_ms,mob_depart_ms,mob_motion_ms,mob_motion_epoch,mob_last_ms,mob_last_epoch;
static struct MobPoint mob_last,mob_motion_origin;
static unsigned mob_motion_n;
static u32 mob_current_epoch=MOB_UNKNOWN,mob_clock_ms,mob_rmc_generation,mob_seen_generation;
static struct MobPoint mob_rmc_point;
static int mob_rmc_fix,mob_rmc_speed,mob_rmc_heading;
static u32 mob_rmc_ms,mob_rmc_epoch;
static int mob_suggest_valid;
static struct MobPoint mob_suggest_point;
static u32 mob_suggest_score,mob_predict_ms;
static u32 geo_seg_m(s32,s32,s32,s32);
static int mob_point_ok(struct MobPoint p){return p.lat>=-9000000&&p.lat<=9000000&&p.lon>=-18000000&&p.lon<=18000000;}
static u32 mob_dist(struct MobPoint a,struct MobPoint b){
 /* Reject remote points before using the local distance approximation. */
 if(iabs32(a.lat-b.lat)>1000000||iabs32(a.lon-b.lon)>1000000)return 1000000u;
 return geo_seg_m(a.lat,a.lon,b.lat,b.lon);
}
static u32 mob_hash(u32 h,const void*p,unsigned n){
 const u8*b=(const u8*)p;while(n--)h=(h^*b++)*16777619u;return h;
}
static int mob_write_all(int fd,const void*p,unsigned n){
 const u8*b=(const u8*)p;while(n){long k=s_write(fd,b,n);if(k<=0||(unsigned long)k>n)return 0;b+=k;n-=(unsigned)k;}return 1;
}
static int mob_read_all(int fd,void*p,unsigned n){
 u8*b=(u8*)p;while(n){long k=s_read(fd,b,n);if(k<=0||(unsigned long)k>n)return 0;b+=k;n-=(unsigned)k;}return 1;
}
/* An arbitrary but consistent date is sufficient. Use an epoch starting at
 * 2000, not clock_gps_epoch(), whose timezone-table range is intentionally
 * narrower. The weekly index is an unlabelled category, never weekday/weekend. */
static u32 mob_epoch(int y,int m,int d,int h,int min,int sec){
 if(y<2000||y>2099||m<1||m>12||d<1||h<0||h>23||min<0||min>59||sec<0||sec>60)return MOB_UNKNOWN;
 static const u8 md[]={31,28,31,30,31,30,31,31,30,31,30,31};
 unsigned days=0;for(int yy=2000;yy<y;yy++)days+=365u+(yy%4==0);
 int lim=md[m-1]+(m==2&&y%4==0);if(d>lim)return MOB_UNKNOWN;
 for(int mm=1;mm<m;mm++)days+=md[mm-1]+(mm==2&&y%4==0);
 return (days+(unsigned)(d-1))*86400u+(u32)h*3600u+(u32)min*60u+(u32)(sec==60?59:sec);
}
static unsigned mob_day(u32 t){return (t/86400u)%7u;}
static unsigned mob_bucket(u32 t){return mob_day(t)*12u+(t%86400u)/7200u;}
static unsigned mob_hour_diff(u32 a,u32 b){
 unsigned x=(a%86400u)/60u,y=(b%86400u)/60u,d=x>y?x-y:y-x;return d>720u?1440u-d:d;
}
static void mob_model_reset(void){memzero(&mob,sizeof(mob));mob.magic=0x31424f4du;}
static void mob_load(void){
 mob_model_reset();long fd=s_open(MOB_MODEL,O_RDONLY_,0);if(fd<0)return;
 u32 check=0;int ok=mob_read_all((int)fd,&mob,sizeof(mob))&&mob_read_all((int)fd,&check,4);
 u8 extra;ok=ok&&s_read((int)fd,&extra,1)==0;s_close((int)fd);
 if(!ok||mob.magic!=0x31424f4du||mob.n>MOB_JOURNEYS||mob.head>=MOB_JOURNEYS||(mob.n<MOB_JOURNEYS&&mob.head!=mob.n)||check!=mob_hash(2166136261u,&mob,sizeof(mob))){mob_model_reset();return;}
 for(unsigned i=0;i<mob.n;i++){
  struct MobJourney*j=&mob.journey[i];
  if(!mob_point_ok(j->origin)||!mob_point_ok(j->dest)||j->trace_n>MOB_TRACE||!j->serial||j->serial>mob.serial){mob_model_reset();return;}
  for(unsigned k=0;k<j->trace_n;k++)if(!mob_point_ok(j->trace[k])){mob_model_reset();return;}
 }
 for(unsigned i=0;i<MOB_TRAFFIC;i++){struct MobRoad*r=&mob.road[i];if(r->key&&(!r->count||r->bucket>=84||!r->mean_ds||r->mean_ds>36000u||r->stamp>mob.serial)){mob_model_reset();return;}}
}
static int mob_save(void){
 u32 check=mob_hash(2166136261u,&mob,sizeof(mob));long fd=s_open(MOB_TEMP,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd<0)return 0;
 int ok=mob_write_all((int)fd,&mob,sizeof(mob))&&mob_write_all((int)fd,&check,4);s_close((int)fd);if(ok){s_sync();ok=s_rename(MOB_TEMP,MOB_MODEL)>=0;}if(!ok)s_unlink(MOB_TEMP);return ok;
}
static void mob_raw_close(void){if(mob_raw_fd>=0)s_close(mob_raw_fd);mob_raw_fd=-1;mob_raw_n=0;mob_raw_ok=0;}
static int mob_raw_flush(void){
 if(!mob_raw_n)return mob_raw_ok;
 unsigned bytes=mob_raw_n*(unsigned)sizeof(struct MobRecord);
 if(mob_raw_fd<0||mob_raw_bytes+bytes>MOB_RAW_LIMIT||!mob_write_all(mob_raw_fd,mob_raw,bytes)){mob_raw_close();return 0;}
 mob_raw_bytes+=bytes;mob_raw_n=0;return 1;
}
static void mob_raw_record(u32 kind,u32 epoch,u32 now,struct MobPoint p,int speed,int heading){
 if(!mob_raw_ok)return;
 struct MobRecord*r=&mob_raw[mob_raw_n++];r->kind=kind;r->epoch=epoch;r->elapsed=now-mob_begin_ms;r->lat=p.lat;r->lon=p.lon;r->mph10=(u16)speed;r->heading=(u16)heading;r->check=mob_hash(2166136261u,r,sizeof(*r)-4);
 if(mob_raw_n==30)mob_raw_flush();
}
static void mob_raw_open(void){
 mob_raw_close();long fd=s_open(MOB_HISTORY,O_RDWR_|O_CREAT_,0666);if(fd<0)return;
 long n=s_lseek((int)fd,0,SEEK_END_);
 /* Rotate only at a session boundary. An interrupted partial tail cannot
  * shift record boundaries: overwrite it at the last complete position. */
 if(n<0){s_close((int)fd);return;}
 if((unsigned long)n>=MOB_RAW_LIMIT/2u){
  s_close((int)fd);s_unlink(MOB_PREV);if(s_rename(MOB_HISTORY,MOB_PREV)<0)return;
  fd=s_open(MOB_HISTORY,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd<0)return;n=0;
 }
 n-=n%(long)sizeof(struct MobRecord);
 if(s_lseek((int)fd,n,SEEK_SET_)<0){s_close((int)fd);return;}
 mob_raw_fd=(int)fd;mob_raw_bytes=(unsigned)n;mob_raw_ok=1;
}
static void mob_session_begin(u32 now,int have,s32 lat,s32 lon){
 mob_raw_close();memzero(&mob_live,sizeof(mob_live));mob_live.depart=MOB_UNKNOWN;
 mob_stage_n=0;mob_begin_ms=now;mob_depart_ms=mob_motion_ms=mob_last_ms=0;
 mob_motion_epoch=mob_last_epoch=MOB_UNKNOWN;mob_motion_n=0;
 mob_started=mob_have_sample=mob_suggest_valid=0;mob_origin_valid=have;
 mob_live.origin=(struct MobPoint){lat,lon};mob_recording=1;mob_predict_ms=0;
 mob_seen_generation=mob_rmc_generation;mob_raw_open();
 mob_raw_record(MOB_BEGIN,MOB_UNKNOWN,now,mob_live.origin,0,0);
}
static void mob_stop_recording(void){mob_raw_close();mob_recording=mob_started=mob_origin_valid=mob_suggest_valid=0;mob_stage_n=0;}
static void mob_clear(void){mob_stop_recording();mob_model_reset();s_unlink(MOB_HISTORY);s_unlink(MOB_PREV);s_unlink(MOB_MODEL);s_unlink(MOB_TEMP);}
/* Called exactly once per RMC sentence, even if its position is invalid.
 * A valid RMC time may arrive before satellite position lock. */
static void mob_rmc(u32 now,u32 epoch,int fix,s32 lat,s32 lon,int speed,int heading){
 mob_rmc_generation++;mob_rmc_ms=now;mob_rmc_epoch=epoch;
 if(epoch!=MOB_UNKNOWN){mob_current_epoch=epoch;mob_clock_ms=now;}
 mob_rmc_point=(struct MobPoint){lat,lon};mob_rmc_fix=fix&&mob_point_ok(mob_rmc_point);
 mob_rmc_speed=speed>=0&&speed<2000?speed:0;mob_rmc_heading=heading>=0&&heading<360?heading:0;
}
static u32 mob_time(u32 now){
 if(mob_current_epoch==MOB_UNKNOWN||now-mob_clock_ms>120000u)return MOB_UNKNOWN;
 return mob_current_epoch+(now-mob_clock_ms)/1000u;
}
static void mob_trace_add(struct MobPoint p){
 unsigned n=mob_live.trace_n;if(n>=MOB_TRACE)return;
 if(!n||mob_dist(mob_live.trace[n-1],p)>=75u)mob_live.trace[mob_live.trace_n++]=p;
}
/* Returns 1 only for a new, fresh 1 Hz sample, suitable for traffic matching. */
static int mob_sample(u32 now){
 if(!mob_recording)return 0;
 if(!mob_started)mob_live.depart=mob_time(now);
 if(mob_seen_generation==mob_rmc_generation)return 0;
 mob_seen_generation=mob_rmc_generation;
 if(!mob_rmc_fix||now-mob_rmc_ms>2500u){mob_motion_n=0;return 0;}
 if(mob_have_sample&&mob_rmc_ms-mob_last_ms<850u)return 0;
 struct MobPoint p=mob_rmc_point;
 if(!mob_origin_valid){mob_live.origin=p;mob_origin_valid=1;}
 u32 gap=mob_have_sample?mob_rmc_ms-mob_last_ms:0,dist=mob_have_sample?mob_dist(mob_last,p):0;
 /* Missing fixes may not contribute to travelled distance or road time. */
 int continuous=mob_have_sample&&gap<=3000u&&dist<=200u;
 if(!mob_started){
  if(mob_rmc_speed>=32){
   if(!mob_motion_n||!continuous){mob_motion_ms=mob_rmc_ms;mob_motion_epoch=mob_rmc_epoch;mob_motion_origin=mob_have_sample?mob_last:p;mob_motion_n=1;}
   else if(mob_motion_n<100)mob_motion_n++;
  }else mob_motion_n=0;
  if(mob_motion_n>=5&&mob_rmc_ms-mob_motion_ms>=5000u&&mob_dist(mob_live.origin,p)>=50u){
   mob_started=1;mob_depart_ms=mob_motion_ms;mob_live.depart=mob_motion_epoch;
   if(mob_live.depart==MOB_UNKNOWN&&mob_rmc_epoch!=MOB_UNKNOWN&&mob_rmc_epoch>=(mob_rmc_ms-mob_depart_ms)/1000u)mob_live.depart=mob_rmc_epoch-(mob_rmc_ms-mob_depart_ms)/1000u;
   mob_live.distance=mob_dist(mob_live.origin,p);mob_trace_add(mob_live.origin);mob_trace_add(mob_motion_origin);
  }
 }else{
  if(continuous&&mob_rmc_speed>=10)mob_live.distance=sat_add32(mob_live.distance,dist);
 }
 if(mob_started){if(mob_live.depart==MOB_UNKNOWN&&mob_rmc_epoch!=MOB_UNKNOWN&&mob_rmc_epoch>=(mob_rmc_ms-mob_depart_ms)/1000u)mob_live.depart=mob_rmc_epoch-(mob_rmc_ms-mob_depart_ms)/1000u;mob_trace_add(p);mob_live.dest=p;mob_live.duration=(mob_rmc_ms-mob_depart_ms)/1000u;}
 mob_last=p;mob_last_ms=mob_rmc_ms;mob_last_epoch=mob_rmc_epoch;mob_have_sample=1;
 mob_raw_record(MOB_SAMPLE,mob_rmc_epoch,mob_rmc_ms,p,mob_rmc_speed,mob_rmc_heading);
 return 1;
}
/* Sparse bounded hash table. A single session contributes at most one
 * independent observation per directed road/time bucket (repeated passes
 * in that session are averaged before commit). */
static unsigned mob_road_slot(u32 key,unsigned bucket){return (key^(bucket*2654435761u))&(MOB_TRAFFIC-1u);}
static struct MobRoad*mob_road_find(u32 key,unsigned bucket,int create){
 unsigned at=mob_road_slot(key,bucket);struct MobRoad*old=0;
 for(unsigned n=0;n<32;n++){struct MobRoad*r=&mob.road[(at+n)&(MOB_TRAFFIC-1u)];
  if(r->key==key&&r->bucket==bucket)return r;
  if(!r->key){if(!create)return 0;old=r;break;}
  if(!old||r->stamp<old->stamp)old=r;
 }
 if(!create||!old)return 0;
 memzero(old,sizeof(*old));old->key=key;old->bucket=(u16)bucket;return old;
}
static void mob_stage_road(u32 key,u32 epoch,u32 ds){
 if(!mob_recording||!mob_started||!key||epoch==MOB_UNKNOWN||ds<10||ds>36000u)return;
 unsigned b=mob_bucket(epoch);for(unsigned i=0;i<mob_stage_n;i++){struct MobRoad*r=&mob_stage[i];if(r->key==key&&r->bucket==b){if(r->count<100){r->mean_ds=(r->mean_ds*r->count+ds)/(r->count+1u);r->count++;}return;}}
 if(mob_stage_n>=MOB_STAGE)return;
 mob_stage[mob_stage_n++]=(struct MobRoad){key,0,ds,(u16)b,1};
}
static void mob_road_commit(void){
 for(unsigned i=0;i<mob_stage_n;i++){struct MobRoad*s=&mob_stage[i],*r=mob_road_find(s->key,s->bucket,1);if(!r)continue;
  unsigned n=r->count;if(n>20)n=20;
  /* Bounded exponential-like recency; an obsolete observation cannot own
   * the profile forever. */
  if(r->stamp&&mob.serial-r->stamp>128u)n=n>4?4:n;
  r->mean_ds=(r->mean_ds*n+s->mean_ds)/(n+1u);r->count=(u16)(n+1u);r->stamp=mob.serial;
 }
}
/* Pooled same-hour fallback with no hardcoded weekday/weekend assumption.
 * Exact-day statistics take precedence when enough independent journeys exist. */
static u32 mob_road_cost(u32 key,u32 epoch,u32 base,int*known){
 if(known)*known=0;
 if(!key||epoch==MOB_UNKNOWN||!base||base>36000u)return base;
 unsigned b=mob_bucket(epoch),n=0;u32 mean=0;
 struct MobRoad*r=mob_road_find(key,b,0);
 if(r&&r->count>=5&&mob.serial-r->stamp<=512u){n=r->count;mean=r->mean_ds;}
 else{
  for(unsigned day=0;day<7;day++){r=mob_road_find(key,day*12u+b%12u,0);if(r&&mob.serial-r->stamp<=512u){mean+=r->mean_ds*r->count;n+=r->count;}}
  if(n<5)return base;
  mean/=n;
 }
 if(n>20)n=20;
 if(mean>base*5u)mean=base*5u;
 /* Do not use learned speeds to encourage illegal speeds: only add delay
  * relative to the legal static routing estimate. */
 if(mean<base)mean=base;
 if(known)*known=1;
 return (base*5u+mean*n)/(5u+n);
}
static int mob_finish(u32 now){
 if(!mob_recording)return 0;
 int ok=mob_started&&mob_have_sample&&mob_live.distance>=150u&&mob_live.duration>=60u&&now-mob_last_ms<=120000u;
 if(ok){
  mob_raw_record(MOB_END,mob_last_epoch,now,mob_live.dest,0,0);
  if(!mob_raw_flush())ok=0;
 }
 if(ok){
  mob.serial++;mob_live.serial=mob.serial;mob.journey[mob.head]=mob_live;
  mob.head=(mob.head+1u)%MOB_JOURNEYS;if(mob.n<MOB_JOURNEYS)mob.n++;
  mob_road_commit();ok=mob_save();
 }
 mob_stop_recording();return ok;
}
struct MobCandidate {struct MobPoint p;u32 weight,visits;};
static struct MobCandidate mob_candidates[MOB_JOURNEYS];
static u32 mob_trace_weight(const struct MobJourney*j){
 unsigned n=mob_live.trace_n;if(n<3||j->trace_n<3)return 100;
 unsigned match=0,last=0,shown=0,first=n>6?n-6:1;
 for(unsigned i=first;i<n;i++){u32 best=140;unsigned at=last;
  for(unsigned k=last;k<j->trace_n;k++){u32 d=mob_dist(mob_live.trace[i],j->trace[k]);if(d<best){best=d;at=k;}}
  shown++;if(best<140){match++;last=at;}
 }
 return shown?25u+75u*match/shown:100u;
}
static int mob_predict(u32 now){
 mob_suggest_valid=0;
 if(!mob_origin_valid||mob.n<3)return 0;
 unsigned nc=0;u32 total=0,epoch=mob_started?mob_live.depart:mob_time(now);
 for(unsigned i=0;i<mob.n;i++){
  const struct MobJourney*j=&mob.journey[i];u32 d=mob_dist(mob_live.origin,j->origin);
  if(d>300u||mob_dist(mob_live.origin,j->dest)<100u)continue;
  if(mob_started&&mob_dist(mob_last,j->dest)<100u)continue;
  u32 w=(301u-d)*10u;
  if(epoch!=MOB_UNKNOWN&&j->depart!=MOB_UNKNOWN){
   unsigned delta=mob_hour_diff(epoch,j->depart);if(delta>240u)continue;
   w=w*(241u-delta)/241u;
   if(mob_day(epoch)!=mob_day(j->depart))w=(w+1u)/3u;
  }
  unsigned age=mob.serial-j->serial;w=w*128u/(128u+(age>1024u?1024u:age));
  if(mob_started)w=w*mob_trace_weight(j)/100u;
  if(!w)continue;
  unsigned k=0;for(;k<nc;k++)if(mob_dist(mob_candidates[k].p,j->dest)<=100u)break;
  if(k==nc){mob_candidates[nc++]=(struct MobCandidate){j->dest,0,0};}
  struct MobCandidate*c=&mob_candidates[k];c->weight+=w;c->visits++;total+=w;
 }
 unsigned best=0;for(unsigned i=1;i<nc;i++)if(mob_candidates[i].weight>mob_candidates[best].weight)best=i;
 if(!nc||!total||mob_candidates[best].visits<3||mob_candidates[best].weight<1800u||mob_candidates[best].weight*100u/total<65u)return 0;
 mob_suggest_point=mob_candidates[best].p;mob_suggest_score=mob_candidates[best].weight*100u/total;mob_suggest_valid=1;return 1;
}
#endif
