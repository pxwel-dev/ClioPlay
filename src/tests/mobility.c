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

/* Controlled varied histories, with no private data. Each record represents a
 * separate completed session. This is a policy/algorithm regression, not a
 * measured forecast accuracy claim. */
static const struct MobPoint pred_origin={5497000,-161000},pred_north={5498800,-161000},pred_south={5495200,-161000};
static void prediction_fixture(u32 now,u32 e){
 mob_stop_recording();mob_model_reset();mob_current_epoch=MOB_UNKNOWN;
 mob_session_begin(now,1,pred_origin.lat,pred_origin.lon);
 if(e!=MOB_UNKNOWN)sample(now+1000,e,0,0,0,0,0);
}
static void prediction_visit(struct MobPoint dest,u32 e,int north){
 assert(mob.n<MOB_JOURNEYS);struct MobJourney*j=&mob.journey[mob.head];memzero(j,sizeof(*j));
 j->origin=pred_origin;j->dest=dest;j->depart=e;j->serial=++mob.serial;j->distance=2200;j->duration=360;
 j->trace_n=12;for(unsigned i=0;i<j->trace_n;i++)j->trace[i]=(struct MobPoint){pred_origin.lat+(north?1:-1)*(s32)i*100,pred_origin.lon};
 mob.n++;mob.head=(mob.head+1u)%MOB_JOURNEYS;
}
static void predictor_quality(void){
 u32 e=mob_epoch(2026,10,19,7,30,0),now=100000;
 prediction_fixture(now,MOB_UNKNOWN);
 for(int i=0;i<5;i++)prediction_visit(pred_north,e-(i+1)*7u*86400u,1);
 assert(!mob_predict(now)); /* no trustworthy clock or live origin */
 sample(now+1000,e,0,0,0,0,0);assert(mob_predict(now+1000)&&mob_suggest_point.lat==pred_north.lat);
 u32 serial=mob_suggest_serial;assert(mob_predict(now+1100)&&mob_suggest_serial==serial);
 sample(now+2000,e+1,1,pred_origin.lat+5000,pred_origin.lon,0,0);
 assert(mob_origin_confirmed&&mob_live.origin.lat==pred_origin.lat+5000&&!mob_suggest_valid&&!mob_predict(now+2000));
 /* A repeated but conflicting calendar routine disambiguates the destination. */
 prediction_fixture(now,e);
 for(int i=0;i<5;i++){prediction_visit(pred_north,e-(i+1)*7u*86400u,1);prediction_visit(pred_south,e-(i+1)*7u*86400u+86400u,0);}
 assert(mob_predict(now+1000)&&mob_suggest_point.lat==pred_north.lat);
 mob_reject_suggestion();assert(!mob_predict(now+1000)&&mob_declined_n==1);
 assert(mob.n==10&&mob.serial==10); /* rejection isn't a training record */
 sample(now+2000,e+86400u,0,0,0,0,0);
 assert(mob_predict(now+2000)&&mob_suggest_point.lat==pred_south.lat);
 /* Same-time ambiguity is neither a suggestion nor a forced choice. Early
  * ordered GPS geometry separates even otherwise identical contexts. */
 prediction_fixture(now,e);
 for(int i=0;i<4;i++){prediction_visit(pred_north,e-(i+1)*7u*86400u,1);prediction_visit(pred_south,e-(i+1)*7u*86400u,0);}
 assert(!mob_predict(now+1000));
 sample(now+2000,e+1,1,pred_origin.lat,pred_origin.lon,0,0);
 mob_started=1;mob_live.depart=e;mob_live.trace_n=7;mob_live.distance=700;
 for(unsigned i=0;i<7;i++)mob_live.trace[i]=(struct MobPoint){pred_origin.lat+(s32)i*100,pred_origin.lon};
 mob_last=mob_live.trace[6];assert(mob_predict(now+2000)&&mob_suggest_point.lat==pred_north.lat);
 /* GPS loss withdraws after a bounded stale interval. A physically reached
  * destination and a substantially longer different journey aren't offered. */
 assert(!mob_predict(now+18000));mob_last_ms=now+18000;
 mob_last=pred_north;assert(!mob_predict(now+18000));
 mob_last=mob_live.trace[6];mob_live.distance=5000;assert(!mob_predict(now+18000));
 /* Entry/retention hysteresis: a modest decrease in the leading evidence
  * doesn't flicker a displayed candidate. A fresh offer still needs 70%. */
 prediction_fixture(now,e);
 for(int i=0;i<6;i++)prediction_visit(pred_north,e-7u*86400u,1);
 for(int i=0;i<2;i++)prediction_visit(pred_south,e-7u*86400u,0);
 assert(mob_predict(now+1000)&&mob_suggest_point.lat==pred_north.lat);serial=mob_suggest_serial;
 for(int i=0;i<2;i++)prediction_visit(pred_south,e-7u*86400u,0);
 assert(mob_predict(now+1000)&&mob_suggest_serial==serial&&mob_suggest_score<70);
 mob_suggest_valid=0;assert(!mob_predict(now+1000));
 /* Missing time with a live fix requires stronger independent support. */
 prediction_fixture(now,MOB_UNKNOWN);sample(now+1000,MOB_UNKNOWN,1,pred_origin.lat,pred_origin.lon,0,0);
 for(int i=0;i<3;i++)prediction_visit(pred_north,MOB_UNKNOWN,1);
 assert(!mob_predict(now+1000));for(int i=0;i<2;i++)prediction_visit(pred_north,MOB_UNKNOWN,1);
 assert(mob_predict(now+1000));
 /* Rejecting a suggestion is scoped to the recording session. */
 mob_reject_suggestion();assert(!mob_predict(now+1000));mob_session_begin(now+2000,1,pred_origin.lat,pred_origin.lon);
 sample(now+3000,MOB_UNKNOWN,1,pred_origin.lat,pred_origin.lon,0,0);assert(mob_predict(now+3000));
 /* Ring wrap: newest actual endpoint anchors the deterministic cluster. */
 prediction_fixture(now,e);mob.n=MOB_JOURNEYS;mob.head=2;mob.serial=MOB_JOURNEYS;
 for(unsigned i=0;i<MOB_JOURNEYS;i++){
  struct MobJourney*j=&mob.journey[i];j->origin=(struct MobPoint){pred_origin.lat+5000,pred_origin.lon};j->dest=pred_south;
  j->depart=e-7u*86400u;j->serial=i+1;
 }
 unsigned indices[]={MOB_JOURNEYS-1u,0,1};
 for(unsigned i=0;i<3;i++){struct MobJourney*j=&mob.journey[indices[i]];j->origin=pred_origin;j->dest=pred_north;j->dest.lat+=(s32)i*3;j->serial=MOB_JOURNEYS-2u+i;}
 assert(mob_predict(now+1000)&&mob_suggest_point.lat==pred_north.lat+6);
 mob_clear();
}
static void prediction_draw(void){
 hud_capture_ms+=600u;hud_prepare(5,hud_now_ms(),1);hud_settle_for_capture(hud_now_ms());hud_draw_card(HC_STATUS,hud_now_ms());
}
static void prediction_ui(void){
 u32 e=mob_epoch(2026,10,19,7,30,0);hud_capture_clock=1;hud_capture_ms=300000;
 prediction_fixture(hud_capture_ms,e);
 for(int i=0;i<5;i++)prediction_visit(pred_north,e-7u*(i+1)*86400u,1);
 assert(mob_predict(hud_capture_ms+1000));
 pref.learn_history=pref.destination_predictions=1;trip.active=trip.count=0;route_status=R_IDLE;mapok=0;
 screen=SCR_MAP;modal=MOD_NONE;freecam=0;gps.fix=1;gps.lat=track_lat=pred_origin.lat;gps.lon=track_lon=pred_origin.lon;track_valid=1;
 nfaves=1;nrecents=0;faves[0].lat=pred_north.lat;faves[0].lon=pred_north.lon;copystr(faves[0].name,sizeof(faves[0].name),"Work");
 hw_bluetooth_enabled=btui.connected=1;set_band("CD");btui.playing=1;bt_control_mode=1;hud_reset_animations();
 assert(!hud_suggestion_shortcut_ready());prediction_draw();
 assert(streq(hud_tween[HC_STATUS].target.traffic_hint,"BACK to review"));assert(!hud_suggestion_shortcut_ready());
 key_handle(K_ENTER);assert(bt_transport_press.kind==BT_TRANSPORT_PLAY_PAUSE&&screen==SCR_MAP&&!trip.active);
 key_handle(K_BACK);assert(!bt_control_mode);prediction_draw();assert(hud_suggestion_shortcut_ready());
 key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==4&&sheet_focus==0&&menu_suggestion_current());
 key_handle(K_BACK);assert(screen==SCR_MAP&&mob_suggest_valid&&!mob_declined_n&&!trip.active);
 prediction_draw();assert(hud_suggestion_shortcut_ready());key_handle(K_ENTER);
 /* Renamed saved place or replacement candidate cannot be accepted from a
  * stale popup, even if the underlying raw endpoint did not change. */
 copystr(faves[0].name,sizeof(faves[0].name),"New work name");assert(!menu_suggestion_current());key_handle(K_ENTER);assert(!trip.active&&screen==SCR_MAP);
 prediction_draw();assert(hud_suggestion_shortcut_ready());key_handle(K_ENTER);key_handle(K_RIGHT);key_handle(K_ENTER);
 assert(!trip.active&&mob_declined_destination(pred_north)&&!mob_suggest_valid&&screen==SCR_MAP);
 assert(!mob_predict(hud_capture_ms));
 mob_declined_n=0;assert(mob_predict(hud_capture_ms));prediction_draw();
 menu_open(P_TRIP,0,hud_now_ms());assert(menu_count(P_TRIP)==1);key_handle(K_DOWN);assert(sheet_focus==0);key_handle(K_BACK);key_handle(K_MAP);assert(screen==SCR_MAP);
 prediction_draw();assert(hud_suggestion_shortcut_ready());key_handle(K_ENTER);mob_suggest_serial++;
 key_handle(K_ENTER);assert(!trip.active&&screen==SCR_MAP);prediction_draw();
 assert(hud_suggestion_shortcut_ready());key_handle(K_ENTER);assert(sheet_focus==0);key_handle(K_ENTER);
 assert(trip.active&&trip.count==1&&streq(trip.target[0].name,"New work name")&&trip.target[0].lat==pred_north.lat&&!mob_suggest_valid);
 assert(mob.n==5&&mob.serial==5);clear_trip();mob_clear();hud_reset_animations();
}
static void prediction_names(void){
 /* Test an actual CPM label record, including malformed-length rejection. */
 static struct Tile tile;memzero(&tile,sizeof(tile));tile.lvl=5;tile.tx=tile_div(pred_north.lon,5);tile.ty=tile_div(pred_north.lat,5);
 tile.data[2]=1;tile.data[8]=5;tile.data[14]=4;tile.len=20;
 unsigned span=span_for_level(5),lx=(unsigned)(pred_north.lon-tile.tx*(s32)span)*4095u/span,ly=(unsigned)(pred_north.lat-tile.ty*(s32)span)*4095u/span;
 tile.data[10]=(u8)lx;tile.data[11]=(u8)(lx>>8);tile.data[12]=(u8)ly;tile.data[13]=(u8)(ly>>8);
 copystr((char*)tile.data+16,5,"Town");char name[32]="";u32 best=1501;
 suggested_settlement_tile(&tile,pred_north,&best,name,sizeof(name));assert(streq(name,"Town")&&best<10);
 tile.len=19;best=1501;name[0]=0;suggested_settlement_tile(&tile,pred_north,&best,name,sizeof(name));assert(!name[0]);
}

static void frequent_fixture(u32 now,u32 epoch,int distinct){
 mob_clear();mob_session_begin(now,1,pred_north.lat,pred_north.lon);frequent_arm_startup();
 for(unsigned i=0;i<5;i++)prediction_visit(pred_north,epoch-(distinct?i+1:1)*86400u,1);
 pref.learn_history=pref.destination_predictions=1;screen=SCR_MAP;modal=MOD_NONE;freecam=bt_control_mode=0;
 mapok=0;gps.fix=1;trip.active=trip.count=0;nfaves=nrecents=0;route_status=R_IDLE;hud_capture_clock=1;hud_capture_ms=now;hud_reset_animations();
}
static void frequent_tick(u32 now,u32 epoch,int speed){
 hud_capture_ms=now;mob_rmc(now,epoch,1,pred_north.lat,pred_north.lon,speed,0);
 int fresh=mob_sample(now);frequent_startup_service(now,fresh);
}
static void frequent_wait(u32 now,u32 e){for(unsigned i=1;i<=5;i++)frequent_tick(now+i*1000u,e+i,0);}
static void frequent_places(void){
 u32 e=mob_epoch(2026,10,19,8,0,0),now=600000u;struct MobPoint out;
 frequent_fixture(now,e,0);assert(!mob_frequent_destination(e,pred_north,&out));frequent_wait(now,e);assert(sheet_page==P_CLOSED);
 frequent_fixture(now,e,1);assert(mob_frequent_destination(e,pred_north,&out)&&out.lat==pred_north.lat);
 assert(!mob_frequent_destination(e,pred_origin,&out));assert(!mob_frequent_destination(e+91u*86400u,pred_north,&out));
 assert(!mob_frequent_destination(MOB_UNKNOWN,pred_north,&out));
 mob.journey[0].depart=MOB_UNKNOWN;assert(!mob_frequent_destination(e,pred_north,&out));mob.journey[0].depart=e-86400u;
 for(unsigned i=1;i<=5;i++)frequent_tick(now+i*1000,e+i,50);assert(sheet_page==P_CLOSED&&!mob_fave_prompted(pred_north));
 frequent_fixture(now,e,1);nfaves=1;faves[0].lat=pred_north.lat+10;faves[0].lon=pred_north.lon;
 frequent_wait(now,e);assert(sheet_page==P_CLOSED&&!mob_fave_prompted(pred_north));
 frequent_fixture(now,e,1);bt_control_mode=1;frequent_wait(now,e);assert(sheet_page==P_CLOSED&&!mob_fave_prompted(pred_north));
 bt_control_mode=0;frequent_tick(now+6000,e+6,0);assert(sheet_page==P_CONFIRM&&confirm_kind==5&&sheet_focus==1);
 key_handle(K_ENTER);assert(screen==SCR_MAP&&nfaves==0&&mob_fave_prompted(pred_north)&&mob.n==5&&mob.serial==5&&!trip.active);
 mob_fave_reset();assert(!mob_fave_prompted(pred_north));mob_fave_load();assert(mob_fave_prompted(pred_north));
 mob_session_begin(now+10000,1,pred_north.lat,pred_north.lon);frequent_arm_startup();frequent_wait(now+10000,e+10);assert(sheet_page==P_CLOSED);
 frequent_fixture(now,e,1);nrecents=1;recents[0].lat=pred_north.lat+20;recents[0].lon=pred_north.lon;copystr(recents[0].name,sizeof(recents[0].name),"Work");
 frequent_wait(now,e);assert(sheet_page==P_CONFIRM&&confirm_kind==5&&sheet_focus==1);
 /* A name is resolved only after consent; show the latest name, not a stale draft. */
 copystr(recents[0].name,sizeof(recents[0].name),"WORKS");
 key_handle(K_LEFT);key_handle(K_ENTER);assert(sheet_page==P_NAME&&keyboard_origin==KBD_NAME_FREQUENT&&streq(kbd_text,"WORKS")&&nfaves==0);
 key_handle(K_ESC);assert(streq(kbd_text,"WORK"));pk_focus=PK_SAVE;key_handle(K_ENTER);
 assert(screen==SCR_MAP&&nfaves==1&&streq(faves[0].name,"WORK")&&faves[0].lat==pred_north.lat&&faves[0].lon==pred_north.lon&&!trip.active&&mob.n==5);
 frequent_fixture(now,e,1);frequent_wait(now,e);assert(sheet_page==P_CONFIRM);
 key_handle(K_LEFT);key_handle(K_ENTER);assert(sheet_page==P_NAME&&kbd_place_lat==pred_north.lat&&nfaves==0&&!kbd_text[0]);
 pk_focus=PK_SAVE;key_handle(K_ENTER);assert(sheet_page==P_NAME&&nfaves==0);
 key_handle(K_BACK);assert(screen==SCR_MAP&&nfaves==0&&mob_fave_prompted(pred_north));
 frequent_fixture(now,e,1);frequent_wait(now,e);assert(sheet_page==P_CONFIRM);
 frequent_tick(now+6000,e+6,50);menu_sync(hud_now_ms());assert(sheet_page==P_CLOSED&&nfaves==0);
 frequent_fixture(now,e,1);frequent_wait(now,e);key_handle(K_LEFT);key_handle(K_ENTER);assert(sheet_page==P_NAME);
 frequent_tick(now+6000,e+6,50);menu_sync(hud_now_ms());assert(sheet_page==P_CLOSED&&nfaves==0);
 mob_clear();mob_fave_load();assert(!mob_fave_prompted(pred_north));hud_reset_animations();frequent_boot_pending=0;gps.mph10=0;
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
 assert(traffic_offer_valid()&&traffic_begin_idx==1&&traffic_join_idx==2);u32 eta=traffic_remaining_cost();assert(eta>route_result_cost);
 trip.active=trip.count=1;trip.active_idx=0;hud_capture_clock=1;hud_capture_ms=1000;
 menu_reset(hud_now_ms());menu_open(P_TRIP,trip.count+2,hud_now_ms());key_handle(K_ENTER);
 assert(sheet_page==P_CONFIRM&&confirm_kind==3&&sheet_focus==0);
 key_handle(K_RIGHT);assert(sheet_focus==1);key_handle(K_ENTER);
 assert(sheet_page==P_TRIP&&!traffic_offer_valid()&&route_n==7&&route_node[2]==2);
 assert(traffic_segment_declined(2)&&!traffic_segment_declined(3)&&!traffic_segment_declined(5));
 assert(traffic_warn&&traffic_remaining_cost()==eta&&!traffic_scan_ms);
 traffic_scan(1100);assert(traffic_searching&&traffic_block_arc==6&&traffic_begin_idx==4&&traffic_join_idx==6);
 for(int i=0;i<100&&traffic_searching;i++)traffic_search_step(1100+i*10);
 assert(traffic_offer_valid()&&traffic_path[1].node==8&&traffic_join_idx==5);
 menu_open(P_TRIP,trip.count+2,hud_now_ms());key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&sheet_focus==0);
 /* Simulate an obsolete dialog identity; declining it must not clear the current offer. */
 confirm_diversion_revision--;key_handle(K_RIGHT);key_handle(K_ENTER);
 assert(sheet_page==P_TRIP&&traffic_offer_valid()&&!traffic_segment_declined(5));
 /* Map shortcut respects BT CTRL, Explore and console Info. */
 menu_close(hud_now_ms());freecam=0;modal=MOD_NONE;screen=SCR_MAP;
 gps.lat=track_lat=route_lat[0];gps.lon=track_lon=route_lon[0];track_valid=1;
 hw_bluetooth_enabled=btui.connected=1;set_band("CD");btui.playing=1;
 gps.mph10=240;
 /* A covered map, no unrelated notification, and normal route progress. */
 mapok=1;mh.flags=0;mh.minlat=mh.minlon=-90000000;mh.maxlat=mh.maxlon=90000000;
 hud_observed_route=R_READY;hud_reset_animations();
 assert(bt_controls_available());bt_control_mode=1;hud_capture_ms+=500;
 hud_prepare(5,hud_now_ms(),1);hud_settle_for_capture(hud_now_ms());hud_draw_card(HC_STATUS,hud_now_ms());
 assert(streq(hud_tween[HC_STATUS].target.traffic_hint,"BACK to review"));
 assert(hud_tween[HC_STATUS].target.flags&HUD_KEYS);assert(!hud_diversion_shortcut_ready());
 /* The one-line invitation does not grow, collapse or replay after time. */
 struct HudCard initial=hud_tween[HC_STATUS].target;
 assert(streq(initial.traffic_title,"Faster route available")&&!initial.traffic_detail[0]);
 hud_capture_ms+=60000u;hud_prepare(5,hud_now_ms(),1);
 assert(hud_tween[HC_STATUS].target.box.h==initial.box.h);
 assert(streq(hud_tween[HC_STATUS].target.traffic_hint,"BACK to review"));
 hud_settle_for_capture(hud_now_ms());hud_draw_card(HC_STATUS,hud_now_ms());
 assert(!hud_diversion_shortcut_ready());
 key_handle(K_ENTER);assert(screen==SCR_MAP&&bt_control_mode&&traffic_offer_valid()&&bt_transport_press.kind==BT_TRANSPORT_PLAY_PAUSE);
 key_handle(K_BACK);assert(!bt_control_mode&&screen==SCR_MAP);
 hud_capture_ms+=500;hud_prepare(5,hud_now_ms(),1);hud_settle_for_capture(hud_now_ms());hud_draw_card(HC_STATUS,hud_now_ms());
 assert(streq(hud_tween[HC_STATUS].target.traffic_hint,"OK to review"));
 assert(!hud_tween[HC_STATUS].target.traffic_detail[0]);
 assert(hud_diversion_shortcut_ready());
 key_handle(K_CONSOLE_INFO);assert(screen==SCR_MAP&&hud_navigation.kind==HUD_JOURNEY);
 pref.autozoom=1;zoom_override=5;
 key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&sheet_focus==0&&confirm_diversion_map);
 key_handle(K_BACK);assert(screen==SCR_MAP&&traffic_offer_valid()&&zoom_override==5);
 hud_capture_ms+=500;hud_prepare(5,hud_now_ms(),1);hud_settle_for_capture(hud_now_ms());hud_draw_card(HC_STATUS,hud_now_ms());
 assert(hud_diversion_shortcut_ready());key_handle(K_ENTER);assert(sheet_page==P_CONFIRM);
 key_handle(K_BACK);assert(screen==SCR_MAP&&traffic_offer_valid());
 /* Approaching the decision point does not change invitation content. */
 hud_capture_ms+=500;hud_prepare(5,hud_now_ms(),1);
 assert(streq(hud_tween[HC_STATUS].target.traffic_hint,"OK to review"));
 route_head=3;gps.lat=track_lat=route_lat[3];gps.lon=track_lon=route_lon[3];gps.mph10=400;
 hud_capture_ms+=60000;hud_prepare(5,hud_now_ms(),1);
 assert(streq(hud_tween[HC_STATUS].target.traffic_hint,"OK to review")&&!hud_tween[HC_STATUS].target.traffic_detail[0]);
 /* Crossing the decision point removes the invitation. */
 route_head=traffic_begin_idx;hud_capture_ms+=500;hud_prepare(5,hud_now_ms(),1);
 assert(!hud_diversion_shortcut_ready()&&!hud_tween[HC_STATUS].target.traffic_serial);
 /* Restore the controlled car position for testing acceptance and stale identity. */
 route_head=3;hud_capture_ms+=500;hud_prepare(5,hud_now_ms(),1);hud_settle_for_capture(hud_now_ms());hud_draw_card(HC_STATUS,hud_now_ms());
 assert(hud_diversion_shortcut_ready());key_handle(K_ENTER);assert(sheet_page==P_CONFIRM);
 /* A new offer with the same endpoints is still a different identity. */
 traffic_offer_serial++;key_handle(K_ENTER);assert(screen==SCR_MAP&&traffic_offer_valid()&&route_n==7);
 hud_capture_ms+=500;hud_prepare(5,hud_now_ms(),1);hud_settle_for_capture(hud_now_ms());hud_draw_card(HC_STATUS,hud_now_ms());
 assert(hud_diversion_shortcut_ready());key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&sheet_focus==0);
 key_handle(K_ENTER);assert(screen==SCR_MAP&&zoom_override==5);assert(!traffic_offer_valid()&&route_n==8&&route_node[5]==8);
 assert(traffic_segment_declined(2)&&!traffic_segment_declined(3)&&traffic_segment_declined(5));
 traffic_scan(1200);assert(!traffic_searching&&!traffic_offer_valid());
 traffic_invalidate();assert(!traffic_segment_declined(2)&&!traffic_segment_declined(5));
 fclose(f);routefd=-1;routeok=0;mob_clear();
}
static void shared_geometry(void){
 traffic_invalidate();route_head=0;route_n=7;route_status=R_READY;gps.fix=pref.learn_history=pref.diversions=1;
 for(int i=0;i<7;i++){route_lat[i]=5497000+i*100;route_lon[i]=-161000;route_node[i]=i;route_seg_arc[i]=i;route_seg_phys[i]=2*i;route_seg_m[i]=110;route_seg_cost[i]=100;}
 traffic_begin_idx=1;traffic_join_idx=6;traffic_offer_n=6;traffic_region=atlas_route_region;traffic_offer_revision=traffic_route_revision;
 for(int i=0;i<6;i++){int j=i+1;traffic_path[i]=(struct TrafficPoint){route_lat[j],route_lon[j],route_node[j],100,route_seg_arc[j],route_seg_phys[j],110};}
 /* Distinct bypass: common prefix 1->2, 2->4 off the original, then common suffix 4->5->6. */
 traffic_path[2].lon+=100;traffic_path[2].node=99;traffic_path[2].arc=99;traffic_path[3].arc=100;
 assert(traffic_prepare_offer()&&traffic_offer_valid());
 assert(traffic_begin_idx==2&&traffic_join_idx==4&&traffic_offer_n==3);
 assert(route_overlay_slot(2,1)==0&&route_overlay_slot(3,1)==1&&route_overlay_slot(4,1)==1&&route_overlay_slot(5,1)==0);
 route_head=1;assert(traffic_offer_valid());route_head=2;assert(!traffic_offer_valid());
 traffic_invalidate();route_head=0;
}
int main(void){(void)load_font();learner();predictor_quality();prediction_names();prediction_ui();frequent_places();shared_geometry();tiny_graph();two_diversions();puts("Mobility recording, contextual predictor, session dismissal, startup and trajectory evidence, map confirmation, offline names, frequent-place prompts and persistence, road profiles and diversion regressions passed.");return 0;}
