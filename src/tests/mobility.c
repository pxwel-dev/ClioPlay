/* Navigation-independent recorder/model and exact-state diversion checks. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <stddef.h>
#include <string.h>

static u32 epoch;
static void sample(u32 ms,u32 when,int fix,s32 lat,s32 lon,int speed,int heading){
 mob_rmc(ms,when,fix,lat,lon,speed,heading);mob_sample(ms);
}
static void completed(u32 start,u32 depart,int commit){
 mob_session_begin(start,1,5497000,-161000);
 sample(start+1000,depart-60,1,5497000,-161000,0,0);
 sample(start+2000,depart-59,1,5497000,-161000,0,0);
 assert(!mob_started&&mob_live.depart==depart-59);
 for(unsigned i=0;i<=65;i++){
  sample(start+3000+i*1000,depart+i,1,5497000+(s32)i*10,-161000,250,0);
 }
 assert(mob_started&&mob_live.depart==depart&&mob_live.distance>=150&&mob_live.duration>=60);
 mob_stage_road(123,depart,3000);mob_stage_road(123,depart,3000);
 if(commit)assert(mob_finish(start+69000));else mob_stop_recording();
}
static void learner(void){
 mob_clear();epoch=mob_epoch(2010,1,4,7,0,0);
 assert(epoch!=MOB_UNKNOWN&&mob_day(epoch+86400u)==(mob_day(epoch)+1)%7);
 assert(mob_epoch(2010,2,29,0,0,0)==MOB_UNKNOWN);
 mob_current_epoch=MOB_UNKNOWN;mob_session_begin(1,1,5497000,-161000);
 assert(mob_time(1)==MOB_UNKNOWN);
 sample(1001,epoch,0,0,0,0,0);
 assert(mob_live.depart==epoch&&!mob_have_sample&&!mob_started);
 assert(!mob_finish(2001)&&mob.n==0);
 for(unsigned i=0;i<5;i++)completed(100000u+i*100000u,epoch+i*7u*86400u,1);
 assert(mob.n==5&&mob.serial==5);
 completed(700000,epoch+35u*86400u,0);assert(mob.n==5);
 mob_model_reset();mob_load();assert(mob.n==5&&mob.serial==5);
 mob_session_begin(800000,1,5497000,-161000);
 assert(mob_predict(800000)&&mob_suggest_point.lat==5497650);
 sample(801000,epoch+42u*86400u,0,0,0,0,0);
 assert(mob_predict(801000));
 sample(802000,epoch+42u*86400u+12u*3600u,0,0,0,0,0);
 assert(!mob_predict(802000));
 int known=0;
 assert(mob_road_cost(123,epoch,1000,&known)>1000&&known);
 assert(mob_road_cost(321,epoch,1000,&known)==1000&&!known);
 assert(mob_road_cost(123,epoch+36000u,1000,&known)==1000&&!known);
 assert(mob_road_cost(123,epoch+86400u,1000,&known)>1000&&known);
 pref.learn_history=pref.destination_predictions=1;trip.active=0;
 mob_suggest_valid=1;mob_suggest_point=mob.journey[0].dest;
 nfaves=nrecents=1;faves[0].lat=5497652;faves[0].lon=-161000;copystr(faves[0].name,sizeof(faves[0].name),"Work");
 recents[0].lat=5497650;recents[0].lon=-161000;copystr(recents[0].name,sizeof(recents[0].name),"Recent");
 struct SavedPlace out;assert(suggested_place(&out)&&streq(out.name,"Work"));
 nfaves=0;assert(suggested_place(&out)&&streq(out.name,"Recent"));
 nrecents=0;assert(suggested_place(&out)&&streq(out.name,"54.97650, -1.61000"));
 mob_clear();assert(!mob.n&&!mob_suggest_valid);
}

/* Four road states plus a distinct detour state. A blocked edge between
 * state 1 and state 2 is replaced by 1 -> 4 -> 2. Exact incoming lists
 * include only legal directed transitions (no reverse or invented turn). */
struct TinyGraph {
 struct CprNode node[5];
 u32 edge[5];
 struct CprBase base[5];
 struct CprArc arc[5];
 struct CprPoint geom[10];
 u32 ways[10];
};
static struct TinyGraph tiny;
static void tiny_graph(void){
 memzero(&tiny,sizeof(tiny));memzero(&rh,sizeof(rh));
 struct CprPoint coords[5]={{5497000,-161000},{5497100,-161000},{5497500,-161000},{5497700,-161000},{5497300,-160800}};
 unsigned from[]={0,1,2,1,4},to[]={1,2,3,4,2};
 u32 costs[]={100,1000,100,250,250};
 for(int i=0;i<5;i++){
  tiny.node[i].lat=coords[i].lat;tiny.node[i].lon=coords[i].lon;tiny.node[i].comp=1;
  tiny.base[i].cost=costs[i];tiny.base[i].geom_first=2*i;tiny.base[i].geom_count=2;
  tiny.arc[i]=(struct CprArc){from[i],to[i],costs[i],(u32)i,ROUTE_NOARC,0,0,0};
  tiny.geom[2*i]=coords[from[i]];tiny.geom[2*i+1]=coords[to[i]];tiny.ways[2*i]=100+i;
 }
 tiny.node[1].first_edge=0;tiny.node[1].edge_count=1;tiny.edge[0]=0;
 tiny.node[2].first_edge=1;tiny.node[2].edge_count=2;tiny.edge[1]=1;tiny.edge[2]=4;
 tiny.node[3].first_edge=3;tiny.node[3].edge_count=1;tiny.edge[3]=2;
 tiny.node[4].first_edge=4;tiny.node[4].edge_count=1;tiny.edge[4]=3;
 FILE*f=tmpfile();assert(f&&fwrite(&tiny,sizeof(tiny),1,f)==1);assert(!fflush(f));routefd=fileno(f);
 routeok=1;rh.flags=2;rh.nodes=rh.route_edges=rh.base_edges=rh.ch_arcs=5;rh.geom_points=10;
 rh.node_off=offsetof(struct TinyGraph,node);rh.edge_off=offsetof(struct TinyGraph,edge);rh.base_off=offsetof(struct TinyGraph,base);rh.arc_off=offsetof(struct TinyGraph,arc);rh.geom_off=offsetof(struct TinyGraph,geom);
 route_geom_map_ok=0;cpr_bcache_count=0;memzero(cpr_acache_count,sizeof(cpr_acache_count));memzero(cpr_gcache_count,sizeof(cpr_gcache_count));
 pref.learn_history=pref.diversions=1;gps.fix=1;atlas_ready=atlas_probe=0;route_dest_comp=1;
 traffic_invalidate();route_status=R_READY;route_head=0;route_n=4;route_result_cost=1200;route_result_m=770;
 for(int i=0;i<4;i++){
  route_lat[i]=coords[i].lat;route_lon[i]=coords[i].lon;route_node[i]=i;
  route_seg_arc[i]=i?(u32)i-1:ROUTE_NOARC;route_seg_cost[i]=i?costs[i-1]:0;
  route_seg_m[i]=i?geo_seg_m(coords[i-1].lat,coords[i-1].lon,coords[i].lat,coords[i].lon):0;
  route_seg_phys[i]=i?2u*(i-1):ROUTE_NOARC;
 }
 mob_current_epoch=epoch;mob_clock_ms=100;
 u32 key=traffic_key_for_arc(1,0);assert(key);
 mob.serial=5;struct MobRoad*r=mob_road_find(key,mob_bucket(epoch),1);r->stamp=5;r->count=5;r->mean_ds=5000;
 traffic_warn=1;traffic_eta_ds=2000;traffic_eta_revision=traffic_route_revision;traffic_slow_end=2;
 assert(traffic_search_start(1,2,1,1000,100,100));
 for(unsigned i=0;i<30&&traffic_searching;i++)traffic_search_step(100+i*50);
 assert(traffic_offer_valid()&&traffic_offer_n==3&&traffic_path[1].node==4&&traffic_saving>=1200);
 /* Original route is unchanged before acceptance. */
 assert(route_n==4&&route_node[2]==2);
 assert(traffic_accept()&&route_n==5&&route_node[2]==4&&route_node[3]==2&&route_node[4]==3);
 assert(route_result_cost==700&&!traffic_offer_valid());
 fclose(f);routefd=-1;routeok=0;mob_clear();
}
int main(void){learner();tiny_graph();puts("Mobility recording, date, prediction, road profiles and exact-state diversion regression passed.");return 0;}
