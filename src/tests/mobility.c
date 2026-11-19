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
 /* Original route is unchanged before acceptance. A settled suffix
  * remains settled even if the accepted splice inserts points. */
 assert(route_n==4&&route_node[2]==2);traffic_set_declined(3,1);
 assert(traffic_accept()&&route_n==5&&route_node[2]==4&&route_node[3]==2&&route_node[4]==3);
 assert(route_result_cost==700&&!traffic_offer_valid()&&traffic_segment_declined(4));
 fclose(f);routefd=-1;routeok=0;mob_clear();
}

/* Two non-overlapping slow stretches, each with a genuinely legal bypass. */
struct TwoGraph {
 struct CprNode node[9];u32 edge[10];struct CprBase base[10];
 struct CprArc arc[10];struct CprPoint geom[20];u32 ways[20];
};
static void two_diversions(void){
 struct TwoGraph g={0};
 const struct CprPoint p[9]={{5497000,-161000},{5497100,-161000},{5497500,-161000},{5497750,-161000},
  {5497300,-160800},{5498000,-161000},{5498400,-161000},{5498650,-161000},{5498200,-160800}};
 const u32 from[]={0,1,2,1,4,3,5,6,5,8},to[]={1,2,3,4,2,5,6,7,8,6};
 const u32 cost[]={100,1600,100,250,250,100,1200,100,250,250};
 const u32 path[]={0,1,2,3,5,6,7},arcs[]={0,1,2,5,6,7};
 for(int i=0;i<9;i++){g.node[i].lat=p[i].lat;g.node[i].lon=p[i].lon;g.node[i].comp=1;}
 for(int i=0;i<10;i++){
  g.base[i].cost=cost[i];g.base[i].geom_first=2*i;g.base[i].geom_count=2;
  g.arc[i]=(struct CprArc){from[i],to[i],cost[i],(u32)i,ROUTE_NOARC,0,0,0};
  g.geom[2*i]=p[from[i]];g.geom[2*i+1]=p[to[i]];g.ways[2*i]=100+i;
 }
 int n=0;for(int v=0;v<9;v++){g.node[v].first_edge=n;for(int i=0;i<10;i++)if(to[i]==(u32)v)g.edge[n++]=i;g.node[v].edge_count=n-g.node[v].first_edge;}
 FILE*f=tmpfile();assert(f&&fwrite(&g,sizeof(g),1,f)==1&&!fflush(f));routefd=fileno(f);
 memzero(&rh,sizeof(rh));routeok=1;rh.flags=2;rh.nodes=9;rh.route_edges=rh.base_edges=rh.ch_arcs=10;rh.geom_points=20;
 rh.node_off=offsetof(struct TwoGraph,node);rh.edge_off=offsetof(struct TwoGraph,edge);rh.base_off=offsetof(struct TwoGraph,base);rh.arc_off=offsetof(struct TwoGraph,arc);rh.geom_off=offsetof(struct TwoGraph,geom);
 route_geom_map_ok=0;cpr_bcache_count=0;memzero(cpr_acache_count,sizeof(cpr_acache_count));memzero(cpr_gcache_count,sizeof(cpr_gcache_count));
 pref.learn_history=pref.diversions=1;gps.fix=1;atlas_ready=atlas_probe=0;route_dest_comp=1;
 traffic_invalidate();route_status=R_READY;route_head=0;route_n=7;route_result_cost=route_result_m=0;
 for(int i=0;i<7;i++){
  u32 v=path[i];route_lat[i]=p[v].lat;route_lon[i]=p[v].lon;route_node[i]=v;
  route_seg_arc[i]=i?arcs[i-1]:ROUTE_NOARC;route_seg_cost[i]=i?cost[arcs[i-1]]:0;
  route_seg_m[i]=i?geo_seg_m(p[path[i-1]].lat,p[path[i-1]].lon,p[v].lat,p[v].lon):0;
  route_seg_phys[i]=i?2*arcs[i-1]:ROUTE_NOARC;
  route_result_cost+=route_seg_cost[i];route_result_m+=route_seg_m[i];
 }
 mob_model_reset();mob.serial=20;mob_current_epoch=epoch;mob_clock_ms=100;
 for(int i=1;i<=6;i+=5){u32 k=traffic_key_for_arc(i,0);assert(k);struct MobRoad*r=mob_road_find(k,mob_bucket(epoch),1);assert(r);r->count=20;r->stamp=20;r->mean_ds=cost[i]*5u;}
 traffic_scan(100);assert(traffic_searching&&traffic_block_arc==1&&traffic_begin_idx==1&&traffic_join_idx==3);
 for(int i=0;i<100&&traffic_searching;i++)traffic_search_step(100+i*10);
 assert(traffic_offer_valid());u32 eta=traffic_remaining_cost();assert(eta>route_result_cost);
 trip.active=trip.count=1;trip.active_idx=0;hud_capture_clock=1;hud_capture_ms=1000;
 menu_reset(hud_now_ms());menu_open(P_TRIP,trip.count+2,hud_now_ms());key_handle(K_ENTER);
 assert(sheet_page==P_CONFIRM&&confirm_kind==3&&sheet_focus==0);
 key_handle(K_RIGHT);assert(sheet_focus==1);key_handle(K_ENTER);
 assert(sheet_page==P_TRIP&&!traffic_offer_valid()&&route_n==7&&route_node[2]==2);
 assert(traffic_segment_declined(2)&&traffic_segment_declined(3)&&!traffic_segment_declined(5));
 assert(traffic_warn&&traffic_remaining_cost()==eta&&!traffic_scan_ms);
 traffic_scan(1100);assert(traffic_searching&&traffic_block_arc==6&&traffic_begin_idx==4&&traffic_join_idx==6);
 for(int i=0;i<100&&traffic_searching;i++)traffic_search_step(1100+i*10);
 assert(traffic_offer_valid()&&traffic_path[1].node==8);
 menu_open(P_TRIP,trip.count+2,hud_now_ms());key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&sheet_focus==0);
 /* Simulate an obsolete dialog identity; declining it must not clear the current offer. */
 confirm_diversion_revision--;key_handle(K_RIGHT);key_handle(K_ENTER);
 assert(sheet_page==P_TRIP&&traffic_offer_valid()&&!traffic_segment_declined(5));
 menu_open(P_TRIP,trip.count+2,hud_now_ms());key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&sheet_focus==0);
 key_handle(K_ENTER);assert(!traffic_offer_valid()&&route_n==8&&route_node[5]==8);
 assert(traffic_segment_declined(2)&&traffic_segment_declined(3)&&traffic_segment_declined(5));
 traffic_scan(1200);assert(!traffic_searching&&!traffic_offer_valid());
 traffic_invalidate();assert(!traffic_segment_declined(2)&&!traffic_segment_declined(5));
 fclose(f);routefd=-1;routeok=0;mob_clear();
}
int main(void){learner();tiny_graph();two_diversions();puts("Mobility recording, date, prediction, road profiles, exact-state diversion, section-local dismissal and default-Yes regression passed.");return 0;}
