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
 /* Weekday vs weekend regime weighting: 2026-10-19 is Monday (mob_day==2),
  * 2026-10-21 is Wednesday (mob_day==4), 2026-10-24 is Saturday (mob_day==0). */
 u32 mon=mob_epoch(2026,10,19,8,0,0),wed=mob_epoch(2026,10,21,8,0,0),sat=mob_epoch(2026,10,24,8,0,0);
 assert(!mob_is_weekend(mon)&&!mob_is_weekend(wed)&&mob_is_weekend(sat));
 /* Sparse Wednesday: Mon/Tue weekday commutes (75%) beat Sat weekend trips (25%). */
 prediction_fixture(now,wed);
 for(int i=0;i<2;i++){
  prediction_visit(pred_north,mon-(u32)i*7u*86400u,1);
  prediction_visit(pred_north,mon+86400u-(u32)i*7u*86400u,1);
  prediction_visit(pred_south,sat-(u32)(i+1)*7u*86400u,0);
 }
 assert(mob_predict(now+1000)&&mob_suggest_point.lat==pred_north.lat);
 /* Saturday with 3 Saturday visits beats 5 weekday commutes (20% cross-regime weight). */
 prediction_fixture(now,sat);
 for(int i=0;i<5;i++)prediction_visit(pred_north,mon+(u32)i*86400u,1);
 for(int i=0;i<3;i++)prediction_visit(pred_south,sat-(u32)(i+1)*7u*86400u,0);
 assert(mob_predict(now+1000)&&mob_suggest_point.lat==pred_south.lat);
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
 assert(streq(menu_title(P_NAME),"Rename"));
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

/* A spur returns to the same junction with a different turn state. Only the
 * legal alternative via state 6 can remove it in the restricted case. */
static void route_spur(int same_state,int have_alternative,int separate_roads){
 struct SpurGraph {
  struct CprNode node[7];u32 edge[7];struct CprBase base[7];
  struct CprArc arc[7];struct CprPoint geom[14];
 } g={0};
 const struct CprPoint coords[]={{5497000,-161000},{5497300,-161000},{5497300,-160900},
  {5497300,separate_roads?-160990:-161000},{5497600,-161000},{5497900,-161000},{5497400,-161150}};
 u32 from[]={0,1,2,same_state?1u:3u,4,0,6};
 u32 to[]={1,2,same_state?1u:3u,4,5,6,4};
 const u32 cost[]={100,50,50,100,100,140,180};
 for(int i=0;i<7;i++){
  g.node[i].lat=coords[i].lat;g.node[i].lon=coords[i].lon;g.node[i].comp=1;
  g.base[i].geom_first=i==2&&!separate_roads?2u:2u*i;g.base[i].geom_count=2;
  if(i==2&&!separate_roads)g.base[i].flags=CPR_ARC_GEOM_REVERSED;
  g.geom[2*i]=coords[from[i]];g.geom[2*i+1]=coords[to[i]];
  g.arc[i]=(struct CprArc){from[i],to[i],cost[i],(u32)i,ROUTE_NOARC,0,0,0};
 }
 u32 n=0;
 for(u32 node=0;node<7;node++){
  g.node[node].first_edge=n;
  for(u32 arc=0;arc<(have_alternative?7u:5u);arc++)if(to[arc]==node){g.edge[n++]=arc;g.node[node].edge_count++;}
 }
 FILE*f=tmpfile();assert(f&&fwrite(&g,sizeof(g),1,f)==1);assert(!fflush(f));routefd=fileno(f);
 memzero(&rh,sizeof(rh));rh.nodes=7;rh.route_edges=n;rh.base_edges=rh.ch_arcs=7;rh.geom_points=14;
 rh.node_off=offsetof(struct SpurGraph,node);rh.edge_off=offsetof(struct SpurGraph,edge);
 rh.base_off=offsetof(struct SpurGraph,base);rh.arc_off=offsetof(struct SpurGraph,arc);rh.geom_off=offsetof(struct SpurGraph,geom);
 cpr_bcache_count=0;memzero(cpr_acache_count,sizeof(cpr_acache_count));memzero(cpr_gcache_count,sizeof(cpr_gcache_count));
 routeok=1;route_geom_map_ok=0;route_dest_comp=1;qadjmode=0;atlas_ready=atlas_probe=0;
 pref.learn_history=pref.diversions=0;gps.mph10=0;route_exact_from=0;
 traffic_invalidate();route_status=R_CALC;route_build_phase=RB_DONE;route_head=0;route_n=6;route_result_cost=400;route_result_m=0;
 for(int i=0;i<6;i++){
  route_lat[i]=coords[i].lat;route_lon[i]=coords[i].lon;route_node[i]=i==3&&same_state?1u:(u32)i;
  route_seg_arc[i]=i?(u32)i-1u:ROUTE_NOARC;route_seg_phys[i]=i?g.base[i-1].geom_first:ROUTE_NOARC;
  route_seg_cost[i]=i?cost[i-1]:0;route_seg_m[i]=i?geo_seg_m(coords[i-1].lat,coords[i-1].lon,coords[i].lat,coords[i].lon):0;
  route_result_m+=route_seg_m[i];
 }
 for(int i=0;route_status==R_CALC&&i<200;i++)regional_service_until(s_now_ms()+10u);
 assert(route_status==R_READY&&route_node[route_n-1]==5&&route_result_m==route_compute_m());
 if(same_state){assert(route_n==4&&route_node[1]==1&&route_node[2]==4&&route_result_cost==300);}
 else if(have_alternative){assert(route_n==4&&route_node[1]==6&&route_node[2]==4&&route_result_cost==420);}
 else{assert(route_n==6&&route_node[1]==1&&route_node[2]==2&&route_node[3]==3&&route_result_cost==400);}
 assert(!traffic_offer_valid());fclose(f);routefd=-1;routeok=0;route_exact_from=ROUTE_NOARC;
}

/* Both routes reach the same directed rejoin, then use the same final road.
 * The zigzag is fastest. The branch via state 6 has fewer turns. */
static void route_fewer_turns(u32 alternate_cost,int legal,int long_detour,int expected){
 struct TurnGraph {
  struct CprNode node[7];u32 edge[7];struct CprBase base[7];
  struct CprArc arc[7];struct CprPoint geom[14];
 } g={0};
 const struct CprPoint coords[]={{5497000,-161000},{5497110,-161000},{5497110,-160840},
  {5497220,-160840},{5497220,-160640},{5497220,-160450},
  {long_detour?5497000:5497110,long_detour?-160200:-160820}};
 const u32 from[]={0,1,2,3,4,0,6},to[]={1,2,3,4,5,6,4};
 const u32 cost[]={140,160,140,160,100,alternate_cost/2u,alternate_cost-alternate_cost/2u};
 for(int i=0;i<7;i++){
  g.node[i].lat=coords[i].lat;g.node[i].lon=coords[i].lon;g.node[i].comp=1;
  g.base[i].geom_first=2u*i;g.base[i].geom_count=2;
  g.geom[2*i]=coords[from[i]];g.geom[2*i+1]=coords[to[i]];
  g.arc[i]=(struct CprArc){from[i],to[i],cost[i],(u32)i,ROUTE_NOARC,0,0,0};
 }
 u32 n=0;
 for(u32 node=0;node<7;node++){
  g.node[node].first_edge=n;
  for(u32 arc=0;arc<(legal?7u:6u);arc++)if(to[arc]==node){g.edge[n++]=arc;g.node[node].edge_count++;}
 }
 FILE*f=tmpfile();assert(f&&fwrite(&g,sizeof(g),1,f)==1);assert(!fflush(f));routefd=fileno(f);
 memzero(&rh,sizeof(rh));rh.nodes=7;rh.route_edges=n;rh.base_edges=rh.ch_arcs=7;rh.geom_points=14;
 rh.node_off=offsetof(struct TurnGraph,node);rh.edge_off=offsetof(struct TurnGraph,edge);
 rh.base_off=offsetof(struct TurnGraph,base);rh.arc_off=offsetof(struct TurnGraph,arc);rh.geom_off=offsetof(struct TurnGraph,geom);
 cpr_bcache_count=0;memzero(cpr_acache_count,sizeof(cpr_acache_count));memzero(cpr_gcache_count,sizeof(cpr_gcache_count));
 routeok=1;route_geom_map_ok=0;route_dest_comp=1;qadjmode=0;atlas_ready=atlas_probe=0;
 pref.learn_history=pref.diversions=0;gps.mph10=0;route_exact_from=0;
 traffic_invalidate();route_status=R_CALC;route_build_phase=RB_DONE;route_head=0;route_n=6;route_result_cost=700;route_result_m=0;
 for(int i=0;i<6;i++){
  route_lat[i]=coords[i].lat;route_lon[i]=coords[i].lon;route_node[i]=(u32)i;
  route_seg_arc[i]=i?(u32)i-1u:ROUTE_NOARC;route_seg_phys[i]=i?g.base[i-1].geom_first:ROUTE_NOARC;
  route_seg_cost[i]=i?cost[i-1]:0;route_seg_m[i]=i?geo_seg_m(coords[i-1].lat,coords[i-1].lon,coords[i].lat,coords[i].lon):0;
  route_result_m+=route_seg_m[i];
 }
 for(int i=0;route_status==R_CALC&&i<200;i++)regional_service_until(s_now_ms()+10u);
 assert(route_status==R_READY&&route_node[route_n-1]==5&&route_result_m==route_compute_m());
 if(expected){assert(route_n==4&&route_node[1]==6&&route_node[2]==4&&route_result_cost==alternate_cost+100u);}
 else{assert(route_n==6&&route_node[1]==1&&route_result_cost==700);}
 assert(!traffic_offer_valid());fclose(f);routefd=-1;routeok=0;route_exact_from=ROUTE_NOARC;
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
 assert(streq(initial.traffic_title,"Traffic diversion available")&&!initial.traffic_detail[0]);
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
 key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&sheet_focus==0&&confirm_diversion_map&&confirm_diversion_kind==0);
 assert(streq(menu_title(P_CONFIRM),"Take traffic diversion?"));
 assert(menu_count(P_CONFIRM)==3&&menu_box(P_CONFIRM).h==122);
 /* D-pad Down jumps from Yes/No to View diversion and Up restores the top row;
  * console rotary cycles all three choices. */
 key_handle(K_RIGHT);assert(sheet_focus==1);
 key_handle(K_DOWN);assert(sheet_focus==2);
 key_handle(K_UP);assert(sheet_focus==1);
 key_handle(K_ROTCW);assert(sheet_focus==2);
 key_handle(K_ROTCW);assert(sheet_focus==0);
 key_handle(K_ROTCCW);assert(sheet_focus==2);
 /* Selecting View diversion closes the popup and frames the diversion tightly in Explore. */
 key_handle(K_ENTER);
 assert(screen==SCR_MAP&&sheet_page==P_CLOSED&&freecam&&diversion_preview_active);
 assert(cam_lat>=route_lat[traffic_begin_idx]&&cam_lat<=route_lat[traffic_join_idx]);
 assert(cam_lon>=route_lon[traffic_begin_idx]&&cam_lon<=traffic_path[1].lon);
 assert(freecam_zoom>=5);
 for(int k=0;k<traffic_offer_n;k++){
  int sx,sy;world_to_screen(traffic_path[k].lat,traffic_path[k].lon,freecam_zoom,&sx,&sy);
  assert(sx>=14&&sx<=W-14&&sy>=14&&sy<=H-14);
 }
 /* Panning and zooming while inspecting keeps preview active; MAP returns and re-opens. */
 key_handle(K_UP);key_handle(K_ESC);assert(freecam&&diversion_preview_active);
 key_handle(K_MAP);
 assert(!freecam&&!diversion_preview_active&&sheet_page==P_CONFIRM&&confirm_kind==3&&sheet_focus==0&&confirm_diversion_map);
 /* Opening from Trip, viewing, then pressing MAP preserves Trip as the Back target. */
 key_handle(K_BACK);assert(screen==SCR_MAP);
 menu_open(P_TRIP,trip.count+2,hud_now_ms());key_handle(K_ENTER);
 assert(sheet_page==P_CONFIRM&&confirm_kind==3&&!confirm_diversion_map);
 key_handle(K_DOWN);assert(sheet_focus==2);key_handle(K_ENTER);
 assert(screen==SCR_MAP&&freecam&&diversion_preview_active&&!diversion_preview_from_map);
 key_handle(K_MAP);
 assert(!freecam&&sheet_page==P_CONFIRM&&confirm_kind==3&&!confirm_diversion_map);
 key_handle(K_BACK);assert(sheet_page==P_TRIP);
 /* If the offer expires during preview, MAP returns to driving map without reopening. */
 menu_close(hud_now_ms());hud_capture_ms+=500;hud_prepare(5,hud_now_ms(),1);hud_settle_for_capture(hud_now_ms());hud_draw_card(HC_STATUS,hud_now_ms());
 key_handle(K_ENTER);assert(sheet_page==P_CONFIRM);
 key_handle(K_DOWN);key_handle(K_ENTER);assert(freecam&&diversion_preview_active);
 traffic_offer_serial++;
 key_handle(K_MAP);assert(!freecam&&!diversion_preview_active&&screen==SCR_MAP&&sheet_page==P_CLOSED);
 /* Restore zoom_override check on ordinary Back without previewing. */
 zoom_override=5;hud_capture_ms+=500;hud_prepare(5,hud_now_ms(),1);hud_settle_for_capture(hud_now_ms());hud_draw_card(HC_STATUS,hud_now_ms());
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
 key_handle(K_ENTER);assert(screen==SCR_MAP&&zoom_override==5&&streq(notice_text,"Diversion selected"));assert(!traffic_offer_valid()&&route_n==8&&route_node[5]==8);
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
static void ml_features(void){
 mob_clear();ml_reset();
 pref.learn_history=pref.destination_predictions=pref.diversions=1;
 u32 ep=mob_epoch(2026,1,12,8,30,0);
 mob_current_epoch=ep;mob_clock_ms=100;
 /* 1 & 7: Per-road-class ETA calibration + cold-weather multiplier */
 car.tempok=1;car.temp=18;assert(!ml_is_cold());
 for(int i=0;i<5;i++){
  mob_recording=mob_started=1;
  ml_stage_class(3,0,1000,1250,0);
  ml_commit_classes();ml_session_reset();
 }
 u32 warm_q8=ml_class_factor_q8(3,0,0);assert(warm_q8>270u&&warm_q8<340u);
 car.temp=1;assert(ml_is_cold());
 for(int i=0;i<4;i++){
  mob_recording=mob_started=1;
  ml_stage_class(3,0,1000,1450,1);
  ml_commit_classes();ml_session_reset();
 }
 u32 cold_q8=ml_class_factor_q8(3,0,1);assert(cold_q8>warm_q8);
 pref.learn_history=0;assert(ml_class_factor_q8(3,0,1)==256u);pref.learn_history=1;
 /* 2: Dwell-time & multi-leg trip chains + recurring intermediate stops */
 struct MobPoint home={5497000,-161000},shop={5497800,-161000},work={5499200,-161000};
 mob.serial=10;
 for(int i=0;i<4;i++)ml_record_chain(home,shop,work,ml_dwell_bucket(1200u));
 mob_live.origin=shop;mob_ml.last_origin=home;mob_ml.last_dest=shop;
 mob_ml.last_arrival=ep-1200u;mob_ml.last_valid=1;
 assert(ml_chain_factor(shop,work,ep)>=140u);
 mob_ml.last_arrival=ep-25000u; /* wrong dwell bucket (>5h) */
 assert(ml_chain_factor(shop,work,ep)<=100u);
 /* Recurring intermediate stop habit (optional extension to #2) */
 for(int i=0;i<3;i++)ml_record_stop_habit(home,shop,work,ep,"COFFEE SHOP");
 struct MobStopHabit sh;assert(ml_usual_stop_for(home,work,ep,&sh)&&streq(sh.name,"COFFEE SHOP"));
 mob_origin_valid=1;mob_live.origin=home;gps.fix=1;gps.lat=track_lat=home.lat;gps.lon=track_lon=home.lon;track_valid=1;
 hud_capture_clock=1;hud_capture_ms=100;
 set_destination("WORK",work.lat,work.lon);
 route_status=R_READY;screen=SCR_MAP;freecam=0;modal=MOD_NONE;bt_control_mode=0;
 assert(ml_stop_valid&&!ml_park_valid);
 hud_reset_animations();hud_prepare(5,hud_now_ms(),1);hud_settle_for_capture(hud_now_ms());hud_draw_card(HC_STATUS,hud_now_ms());
 assert(streq(hud_tween[HC_STATUS].target.title,"Usual stop available")&&hud_stop_shortcut_ready());
 key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==7&&sheet_focus==0);
 key_handle(K_ENTER);assert(screen==SCR_MAP&&trip.count==2&&streq(trip.target[0].name,"COFFEE SHOP"));
 /* Declining a usual stop suppresses it for the rest of the session. */
 clear_trip();set_destination("WORK",work.lat,work.lon);route_status=R_READY;
 assert(ml_stop_valid);hud_reset_animations();hud_prepare(5,hud_now_ms(),1);hud_settle_for_capture(hud_now_ms());hud_draw_card(HC_STATUS,hud_now_ms());
 key_handle(K_ENTER);key_handle(K_RIGHT);key_handle(K_ENTER);assert(!ml_stop_valid&&trip.count==1);
 set_destination("WORK",work.lat,work.lon);assert(!ml_stop_valid);
 clear_trip();
 /* 4: Online MAD variance & ETA range formatting */
 unsigned bk=mob_bucket(ep);
 ml_commit_road_variance(777u,bk,3000u,5400u,1);
 ml_commit_road_variance(777u,bk,3600u,5800u,1);
 ml_commit_road_variance(777u,bk,4200u,6600u,1);
 assert(ml_road_mad(777u,ep)>=2000u);
 route_result_cost=6000u;route_result_m=3218u;atlas_future_cost=0;atlas_future_m=0;
 ml_route_var_ds=2400u;traffic_eta_revision=0xffffffffu;
 char jbuf[96];hud_journey_text(jbuf,sizeof(jbuf));
 assert(strstr(jbuf,"-")!=0&&strstr(jbuf,"min")!=0);
 pref.learn_history=0;hud_journey_text(jbuf,sizeof(jbuf));assert(strstr(jbuf,"-")==0);pref.learn_history=1;
 ml_route_var_ds=0;
 /* 5: Adaptive auto-zoom (aligned bands + hysteresis + sustained override) & Day/Night brightness */
 menu_reset(hud_now_ms());bt_control_mode=0;mh.minlvl=0;mh.maxlvl=6;
 pref.autozoom=1;screen=SCR_MAP;freecam=0;modal=MOD_NONE;zoom_override=-1;
 assert(ml_zoom_band(240)==0&&ml_zoom_band(250)==1&&ml_zoom_band(440)==1&&ml_zoom_band(450)==2);
 assert(ml_zoom_band_for_level(9)==0&&ml_zoom_band_for_level(7)==0&&ml_zoom_band_for_level(6)==1&&ml_zoom_band_for_level(5)==2&&ml_zoom_band_for_level(4)==2);
 for(int i=0;i<4;i++){memzero(ml_zoom_nudged,sizeof(ml_zoom_nudged));ml_note_zoom_override(120,1);ml_note_zoom_override(500,-1);}
 assert(ml_autozoom_offset(120)==1&&ml_autozoom_offset(240)==1&&ml_autozoom_offset(250)==0&&ml_autozoom_offset(440)==0&&ml_autozoom_offset(450)==-1);
 gps.mph10=700;auto_zoom_level=4;assert(speed_auto_zoom()==4); /* clamped at 4 */
 gps.mph10=0;auto_zoom_level=9;assert(speed_auto_zoom()==9); /* clamped at 9 */
 gps.mph10=120;auto_zoom_level=7;assert(speed_auto_zoom()==8); /* learned +1 shifts Z7->Z8 */
 /* Hysteresis inheritance: at 26 mph while base Z7 holds (below 27 mph out-threshold), band 0 stays active */
 gps.mph10=260;assert(speed_auto_zoom()==8&&auto_zoom_level==7);
 gps.mph10=270;assert(speed_auto_zoom()==6&&auto_zoom_level==6); /* cleanly enters band 1 (Z6) */
 gps.mph10=470;assert(speed_auto_zoom()==6&&auto_zoom_level==6); /* below 48 mph out-threshold, stays in band 1 */
 gps.mph10=480;assert(speed_auto_zoom()==4&&auto_zoom_level==5); /* cleanly enters band 2 (Z5-1=Z4) */
 /* Sustained override guard in band 1 (30 mph, base Z6): misclick undo & quick peek must not bias ML */
 gps.mph10=300;auto_zoom_level=6;zoom_override=-1;memzero(ml_zoom_nudged,sizeof(ml_zoom_nudged));ml_zoom_cancel_override();
 /* Misclick undo: zoom out 6->5 then back 5->6; even after settle time, zero vote */
 key_handle(K_ESC);assert(zoom_override==5);
 hud_capture_ms+=1000u;key_handle(K_SR);assert(zoom_override==6);
 hud_capture_ms+=ML_ZOOM_SETTLE_MS;ml_poll_zoom_override(hud_now_ms());
 assert(mob_ml.zoom_samples[1]==0&&mob_ml.zoom_bias_q4[1]==0);
 /* Quick peek: zoom out 6->5 for 10 s then MAP returns to Auto zoom -> zero vote */
 key_handle(K_MAP);assert(zoom_override<0);
 key_handle(K_ESC);assert(zoom_override==5);
 hud_capture_ms+=10000u;ml_poll_zoom_override(hud_now_ms());
 key_handle(K_MAP);assert(zoom_override<0);
 hud_capture_ms+=ML_ZOOM_SETTLE_MS;ml_poll_zoom_override(hud_now_ms());
 assert(mob_ml.zoom_samples[1]==0&&mob_ml.zoom_bias_q4[1]==0);
 /* Direction change 6->5->6->7 then held >= 20 s -> commits +1 (not -1), timer restarts on last tap */
 key_handle(K_ESC);assert(zoom_override==5);
 hud_capture_ms+=15000u;key_handle(K_SR);key_handle(K_SR);assert(zoom_override==7);
 hud_capture_ms+=10000u;ml_poll_zoom_override(hud_now_ms());
 assert(mob_ml.zoom_samples[1]==0&&mob_ml.zoom_bias_q4[1]==0); /* timer reset on last tap */
 hud_capture_ms+=10000u;ml_poll_zoom_override(hud_now_ms());
 assert(mob_ml.zoom_samples[1]==1&&mob_ml.zoom_bias_q4[1]==4);
 /* Three more sustained sessions reach +10 and activate +1; one sustained opposite session cancels it */
 for(int i=0;i<3;i++){memzero(ml_zoom_nudged,sizeof(ml_zoom_nudged));ml_note_zoom_override(300,1);}
 key_handle(K_MAP);assert(zoom_override<0&&speed_auto_zoom()==7);
 memzero(ml_zoom_nudged,sizeof(ml_zoom_nudged));
 key_handle(K_ESC);assert(zoom_override==6); /* effective base was 7, target 6 -> delta -1 */
 hud_capture_ms+=ML_ZOOM_SETTLE_MS;ml_poll_zoom_override(hud_now_ms());
 key_handle(K_MAP);assert(zoom_override<0&&mob_ml.zoom_bias_q4[1]==4&&speed_auto_zoom()==6);
 /* Reset band 1 back to neutral for downstream checks */
 mob_ml.zoom_bias_q4[1]=0;mob_ml.zoom_samples[1]=0;
 hud_navigation.kind=HUD_NONE;hud_capture_ms+=2000u;
 gps.mph10=300;auto_zoom_level=6;assert(speed_auto_zoom()==6);
 ml_note_brightness(0,90);ml_note_brightness(0,90);
 ml_note_brightness(1,30);ml_note_brightness(1,30);
 assert(ml_learned_brightness(0)==90&&ml_learned_brightness(1)==30);
 pref.theme=THEME_AUTO;theme_resolved=0;pref.bright=80;
 gps.hour=23;gps.min=0;pref.day_start=420;pref.night_start=1200;pref.hoff=0;pref.moff=0;
 refresh_theme();assert(theme_resolved==1&&pref.bright==30);
 gps.hour=12;refresh_theme();assert(theme_resolved==0&&pref.bright==90);
 /* 6: Nearby POI affinity & Usual Parking spot */
 ml_record_poi_use(6,5497200,-161000,"SHELL");
 ml_record_poi_use(6,5497200,-161000,"SHELL");
 assert(ml_poi_affinity(6,5497200,-161000,"SHELL")>=2u);
 struct NearbyPoi p_shell={5497200,-161000,1400u,6,1,"SHELL"};
 struct NearbyPoi p_bp={5497100,-161000,1000u,6,0,"BP"};
 struct NearbyPoi p_esso={5497050,-161000,500u,6,0,"ESSO"};
 assert(nearby_before(&p_shell,&p_bp)&&nearby_before(&p_esso,&p_shell));
 struct MobPoint park={5499450,-161000}; /* ~275 m from work */
 for(int i=0;i<3;i++)ml_record_parking(work,park);
 set_destination("WORK",work.lat,work.lon);route_status=R_READY;
 assert(ml_park_valid);
 hud_reset_animations();hud_prepare(5,hud_now_ms(),1);hud_settle_for_capture(hud_now_ms());hud_draw_card(HC_STATUS,hud_now_ms());
 assert(streq(hud_tween[HC_STATUS].target.title,"Usual parking available")&&hud_parking_shortcut_ready());
 key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==6&&sheet_focus==0);
 key_handle(K_ENTER);assert(screen==SCR_MAP&&trip.target[0].lat==park.lat&&trip.target[0].lon==park.lon);
 clear_trip();
 /* 8: Repeat hard-braking / slowdown detection (warm & cold) */
 car.tempok=1;car.temp=15;
 for(int i=0;i<3;i++){
  mob_recording=mob_started=1;ml_prev_speed=-1;ml_prev_speed_ms=0;
  ml_observe_decel(1000,280,991u,5497500,-161000);
  ml_observe_decel(2000,140,991u,5497550,-161000);
  ml_commit_hazards();ml_session_reset();
 }
 assert(ml_hazard_level(991u,0)==1);
 car.temp=1;
 for(int i=0;i<2;i++){
  mob_recording=mob_started=1;ml_prev_speed=-1;ml_prev_speed_ms=0;
  ml_observe_decel(1000,260,992u,5497600,-161000);
  ml_observe_decel(2000,120,992u,5497650,-161000);
  ml_commit_hazards();ml_session_reset();
 }
 assert(ml_hazard_level(992u,0)==0&&ml_hazard_level(992u,1)==2);
 /* 8b: Dynamic hazard capping, anti-flicker smooth-pass decay, season-safe cold decay & age expiry */
 {
  u32 saved_ser=mob.serial;
  /* Cap check: 8 hard brakes cap count at 6 and cold_count at 5 */
  car.temp=1;
  for(int i=0;i<8;i++){
   mob_recording=mob_started=1;ml_prev_speed=-1;ml_prev_speed_ms=0;
   ml_observe_decel(1000,280,993u,5497700,-161000);
   ml_observe_decel(2000,140,993u,5497750,-161000);
   ml_commit_hazards();ml_session_reset();
  }
  struct MobHazard*hc=ml_hazard_find(993u,0);
  assert(hc&&hc->count==6u&&hc->cold_count==5u);
  /* Smooth pass < 4 journeys later must NOT decay (anti-flicker) */
  mob.serial+=3u;car.temp=15;mob_recording=mob_started=1;
  ml_stage_pref_arc(993u,10);ml_commit_hazards();ml_session_reset();
  assert(hc->count==6u&&hc->cold_count==5u);
  /* Warm smooth pass >= 4 journeys later decays count (6->5) but preserves cold_count (5) */
  mob.serial+=1u;car.temp=15;mob_recording=mob_started=1;
  ml_stage_pref_arc(993u,10);ml_commit_hazards();ml_session_reset();
  assert(hc->count==5u&&hc->cold_count==5u);
  /* Cold smooth pass >= 4 journeys later decays both count (5->4) and cold_count (5->4) */
  mob.serial+=4u;car.temp=1;mob_recording=mob_started=1;
  ml_stage_pref_arc(993u,10);ml_commit_hazards();ml_session_reset();
  assert(hc->count==4u&&hc->cold_count==4u);
  /* 4 more cold smooth passes spaced >= 4 journeys apart clear the slot completely */
  for(int i=0;i<4;i++){mob.serial+=4u;car.temp=1;mob_recording=mob_started=1;ml_stage_pref_arc(993u,10);ml_commit_hazards();ml_session_reset();}
  assert(ml_hazard_find(993u,0)==0&&ml_hazard_level(993u,1)==0);
  /* Age expiry: warm expires after >256 journeys; cold survives 300 journeys, expires after >512 */
  mob.serial=saved_ser+300u;
  assert(ml_hazard_level(991u,0)==0&&ml_hazard_level(992u,1)==2);
  mob.serial=saved_ser+520u;
  assert(ml_hazard_level(992u,1)==0);
  mob.serial=saved_ser;
  assert(ml_hazard_level(991u,0)==1&&ml_hazard_level(992u,1)==2);
 }
 /* 3: Preferred-route ("Usual route available") diversion offer on small graph */
 {
  struct PrefGraph {
   struct CprNode node[5];u32 edge[5];struct CprBase base[5];struct CprArc arc[5];struct CprPoint geom[10];u32 way[5][2];
  } g;
  memzero(&g,sizeof(g));
  struct CprPoint pts[5]={{5497000,-161000},{5497200,-161000},{5497600,-161000},{5497900,-161000},{5497400,-160700}};
  for(int i=0;i<5;i++){g.node[i].lat=pts[i].lat;g.node[i].lon=pts[i].lon;g.node[i].comp=1;}
  u32 from[5]={0,1,2,1,4},to[5]={1,2,3,4,2},cost[5]={400,1000,400,600,600};
  for(int i=0;i<5;i++){
   g.geom[i*2]=pts[from[i]];g.geom[i*2+1]=pts[to[i]];
   g.base[i]=(struct CprBase){cost[i],(u32)(i*2),0,to[i],2,0,3,0,0};
   g.arc[i]=(struct CprArc){from[i],to[i],cost[i],(u32)i,0,3,0,0};
   g.way[i][0]=300u+(u32)i;g.way[i][1]=0;
  }
  int en=0;for(int v=0;v<5;v++){g.node[v].first_edge=(u32)en;for(int i=0;i<5;i++)if(to[i]==(u32)v)g.edge[en++]=(u32)i;g.node[v].edge_count=(u16)((u32)en-g.node[v].first_edge);}
  FILE*f=tmpfile();assert(f&&fwrite(&g,sizeof(g),1,f)==1&&!fflush(f));routefd=fileno(f);
  memzero(&rh,sizeof(rh));routeok=1;rh.flags=2;rh.nodes=5;rh.route_edges=rh.base_edges=rh.ch_arcs=5;rh.geom_points=10;
  rh.node_off=offsetof(struct PrefGraph,node);rh.edge_off=offsetof(struct PrefGraph,edge);
  rh.base_off=offsetof(struct PrefGraph,base);rh.arc_off=offsetof(struct PrefGraph,arc);rh.geom_off=offsetof(struct PrefGraph,geom);
  route_geom_map_ok=0;cpr_bcache_count=0;memzero(cpr_acache_count,sizeof(cpr_acache_count));memzero(cpr_gcache_count,sizeof(cpr_gcache_count));
  traffic_invalidate();route_status=R_READY;route_head=0;route_n=4;route_dest_comp=1;route_result_cost=1800;route_result_m=990;
  for(int i=0;i<4;i++){
   route_lat[i]=pts[i].lat;route_lon[i]=pts[i].lon;route_node[i]=(u32)i;
   route_seg_arc[i]=i?(u32)(i-1):ROUTE_NOARC;route_seg_cost[i]=i?cost[i-1]:0;
   route_seg_m[i]=i?geo_seg_m(pts[i-1].lat,pts[i-1].lon,pts[i].lat,pts[i].lon):0;
   route_seg_phys[i]=i?2u*(u32)(i-1):ROUTE_NOARC;
  }
  u32 k_avoid=traffic_key_for_arc(1,0),k_pref1=traffic_key_for_arc(3,0),k_pref2=traffic_key_for_arc(4,0);
  assert(k_avoid&&k_pref1&&k_pref2);
  for(int i=0;i<3;i++){
   mob_recording=mob_started=1;
   ml_stage_pref_arc(k_avoid,-10);ml_stage_pref_arc(k_pref1,10);ml_stage_pref_arc(k_pref2,10);
   ml_commit_pref_arcs();ml_session_reset();
  }
  assert(ml_pref_score(k_avoid)<=-30&&ml_pref_score(k_pref1)>=30);
  /* Off-route detours during an active traffic warning or known-slow arc must NOT pollute pref_arcs. */
  mob_recording=mob_started=1;ml_session_reset();
  gps.lat=route_lat[0];gps.lon=route_lon[0];gps.mph10=200;
  gps.course=track_course=display_course=nav_heading10(route_lat[0],route_lon[0],route_lat[1],route_lon[1]);display_course_valid=1;
  traffic_warn=1;traffic_slow_end=3;
  traffic_note_offroute_avoidance();assert(ml_stage_pref_n==0);
  traffic_warn=0;traffic_slow_end=-1;
  traffic_note_offroute_avoidance();assert(ml_stage_pref_n>0&&ml_stage_pref[0].key==traffic_key_for_arc(0,0)&&ml_stage_pref[0].delta==-10);
  ml_session_reset();
  /* Usual-route offer uses dedicated non-ambiguous wording throughout popup, preview, and toasts. */
  traffic_scan(100);assert(traffic_searching&&traffic_search_kind==1);
  for(int i=0;i<50&&traffic_searching;i++)traffic_search_step(100u+(u32)i*10u);
  assert(traffic_offer_valid()&&ml_offer_kind==1);
  /* Usual-route proposals keep the active route in normal colour (no yellow traffic slot). */
  for(int i=1;i<route_n;i++)assert(route_overlay_slot(i,1)==0);
  ml_offer_kind=0;assert(route_overlay_slot(traffic_begin_idx+1,1)==1);ml_offer_kind=1;
  hud_reset_animations();hud_prepare(5,hud_now_ms(),1);hud_settle_for_capture(hud_now_ms());hud_draw_card(HC_STATUS,hud_now_ms());
  assert(streq(hud_tween[HC_STATUS].target.title,"Usual route available")&&hud_diversion_shortcut_ready());
  key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==3&&confirm_diversion_kind==1&&streq(menu_title(P_CONFIRM),"Use usual route?"));
  key_handle(K_DOWN);assert(sheet_focus==2);key_handle(K_ENTER);
  assert(freecam&&diversion_preview_active&&diversion_preview_kind==1);
  traffic_offer_serial++;key_handle(K_MAP);
  assert(!freecam&&!diversion_preview_active&&streq(notice_text,"Route unavailable"));
  hud_reset_animations();hud_prepare(5,hud_now_ms(),1);hud_settle_for_capture(hud_now_ms());hud_draw_card(HC_STATUS,hud_now_ms());
  key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==3&&confirm_diversion_kind==1);
  key_handle(K_ENTER);assert(screen==SCR_MAP&&streq(notice_text,"Usual route selected"));
  /* 3b: Unguided-learned Usual Route proposal (pure positive familiarity >= +40 on incoming shortcut, zero guided avoidance). */
  memzero(mob_ml.pref_arcs,sizeof(mob_ml.pref_arcs));
  traffic_invalidate();route_status=R_READY;route_head=0;route_n=4;route_dest_comp=1;route_result_cost=1800;route_result_m=990;
  for(int i=0;i<4;i++){
   route_lat[i]=pts[i].lat;route_lon[i]=pts[i].lon;route_node[i]=(u32)i;
   route_seg_arc[i]=i?(u32)(i-1):ROUTE_NOARC;route_seg_cost[i]=i?cost[i-1]:0;
   route_seg_m[i]=i?geo_seg_m(pts[i-1].lat,pts[i-1].lon,pts[i].lat,pts[i].lon):0;
   route_seg_phys[i]=i?2u*(u32)(i-1):ROUTE_NOARC;
  }
  for(int i=0;i<4;i++){
   mob_recording=mob_started=1;
   ml_stage_pref_arc(k_pref1,10);ml_stage_pref_arc(k_pref2,10);
   ml_commit_pref_arcs();ml_session_reset();
  }
  assert(ml_pref_score(k_avoid)==0&&ml_pref_score(k_pref1)>=40&&ml_pref_score(k_pref2)>=40);
  traffic_scan(200);assert(traffic_searching&&traffic_search_kind==1&&traffic_search_min_pref==40);
  for(int i=0;i<50&&traffic_searching;i++)traffic_search_step(200u+(u32)i*10u);
  assert(traffic_offer_valid()&&ml_offer_kind==1);
  /* When main route arc is also familiar (+40), do NOT offer a redundant unguided usual route. */
  traffic_invalidate();
  for(int i=0;i<4;i++){mob_recording=mob_started=1;ml_stage_pref_arc(k_avoid,10);ml_commit_pref_arcs();ml_session_reset();}
  traffic_scan(300);assert(!traffic_searching&&!traffic_offer_valid());
  /* 3c: Unguided historical congestion warning ("Likely traffic ahead") without an active route. */
  {
   u32 saved_n=mob.n;if(mob.n<5u)mob.n=5u;
   u32 tnow=hud_now_ms(),t_ep=mob_time(tnow);assert(t_ep!=MOB_UNKNOWN);
   unsigned tb=mob_bucket(t_ep);
   struct MobRoad*mr=mob_road_find(k_avoid,tb,1);assert(mr);
   mr->count=8u;mr->mean_ds=cost[1]+1600u;mr->stamp=mob.serial;
   route_status=R_IDLE;pref.diversions=1;gps.fix=1;track_valid=0;gps.mph10=200;freecam=0;screen=SCR_MAP;
   gps.lat=pts[1].lat;gps.lon=pts[1].lon;gps.course=0;
   tl_arc=1;tl_key=k_avoid;tl_region=atlas_route_region;tl_last_ms=tnow;
   ml_congestion_seen_n=0;traffic_un_cong_ms=0;hud_navigation.kind=HUD_NONE;
   /* Respects pref.diversions == 0 */
   pref.diversions=0;traffic_service(tnow);assert(hud_navigation.kind==HUD_NONE);
   /* Fires amber "Likely traffic ahead" when pref.diversions == 1 */
   pref.diversions=1;traffic_un_cong_ms=0;traffic_service(tnow);
   assert(hud_navigation.kind==HUD_CONGESTION);
   hud_reset_animations();hud_prepare(5,tnow,1);hud_settle_for_capture(tnow);
   assert(streq(hud_tween[HC_STATUS].target.title,"Likely traffic ahead"));
   /* Deduplicated on same trip */
   hud_navigation.kind=HUD_NONE;traffic_un_cong_ms=0;traffic_service(tnow+4000u);
   assert(hud_navigation.kind==HUD_NONE);
   mr->count=0;mr->key=0;tl_arc=ROUTE_NOARC;mob.n=saved_n;
  }
  fclose(f);routefd=-1;routeok=0;traffic_invalidate();
 }
 /* 9: Unguided slowdown alerts (warm & cold), forward-cone filtering & once-per-trip dedup */
 {
  route_status=R_IDLE;routeok=0;gps.fix=1;track_valid=0;gps.mph10=200;freecam=0;screen=SCR_MAP;
  ml_slowdown_seen_n=0;hud_navigation.kind=HUD_NONE;
  /* Driving South away from hazard 991 at (5497550,-161000) -> silent */
  gps.lat=5497400;gps.lon=-161000;gps.course=180;car.tempok=1;car.temp=15;
  traffic_service(hud_now_ms());assert(hud_navigation.kind==HUD_NONE);
  /* Driving North toward hazard 991 (~167 m ahead, bearing 0) -> warm Frequent slowdown ahead */
  gps.course=0;traffic_service(hud_now_ms());
  assert(hud_navigation.kind==HUD_SLOWDOWN&&!ml_slowdown_cold&&ml_slowdown_arc==991u);
  /* Deduplicated on same trip */
  hud_navigation.kind=HUD_NONE;traffic_service(hud_now_ms());assert(hud_navigation.kind==HUD_NONE);
  /* Cold-only hazard 992 at (5497650,-161000): silent at 15 C, alerts at 1 C */
  gps.lat=5497500;gps.lon=-161000;gps.course=0;car.temp=15;
  traffic_service(hud_now_ms());assert(hud_navigation.kind==HUD_NONE);
  car.temp=1;traffic_service(hud_now_ms());
  assert(hud_navigation.kind==HUD_SLOWDOWN&&ml_slowdown_cold&&ml_slowdown_arc==992u);
  hud_navigation.kind=HUD_NONE;
 }
 /* 10: Unguided Nearby-POI affinity, Usual Parking & auto-named Recurring Stops from map tiles */
 {
  struct MobPoint p_fuel={5495900,-162000},p_park={5496100,-162000},p_work={5496350,-162000},p_home={5495200,-162000};
  int tx=tile_div(p_fuel.lon,5),ty=tile_div(p_fuel.lat,5),span=span_for_level(5);
  assert(tile_div(p_park.lon,5)==tx&&tile_div(p_park.lat,5)==ty);
  /* Build a tiny valid CPM5 Z5 tile in tc[0] with one Fuel POI ("ESSO") and one Parking POI ("TOWN PARK"). */
  memzero(tc,sizeof(tc));
  u8*d=tc[0].data;u32 pos=8;
  d[0]=0;d[1]=0;d[2]=2;d[3]=0;d[4]=d[5]=d[6]=d[7]=0;
  struct {u8 k;s32 lat,lon;const char*name;} pois[2]={{6,p_fuel.lat,p_fuel.lon,"ESSO"},{7,p_park.lat,p_park.lon,"TOWN PARK"}};
  for(int i=0;i<2;i++){
   u16 lx=(u16)(((u32)(pois[i].lon-tx*span)*4095u)/(u32)span);
   u16 ly=(u16)(((u32)(pois[i].lat-ty*span)*4095u)/(u32)span);
   u8 tl=(u8)slen(pois[i].name);
   d[pos++]=pois[i].k;d[pos++]=0;d[pos++]=(u8)(lx&255u);d[pos++]=(u8)(lx>>8);
   d[pos++]=(u8)(ly&255u);d[pos++]=(u8)(ly>>8);d[pos++]=tl;d[pos++]=0;
   for(u8 j=0;j<tl;j++)d[pos++]=(u8)pois[i].name[j];
  }
  tc[0].valid=1;tc[0].lvl=5;tc[0].page=0;tc[0].tx=tx;tc[0].ty=ty;tc[0].len=pos;tc[0].age=++agec;
  mapok=1;mh.minlvl=0;mh.maxlvl=6;mh.count=0;
  /* Two unguided arrivals at ESSO -> automatically learns Preferred Fuel POI without Trip -> Nearby */
  ml_guided_target_valid=0;
  for(int i=0;i<2;i++){
   mob_live.origin=p_home;mob_live.dest=p_fuel;mob_live.depart=ep+(u32)i*86400u;mob_live.duration=300u;
   ml_commit_session();
  }
  assert(ml_poi_affinity(6,p_fuel.lat,p_fuel.lon,"ESSO")>=2u);
  /* Explicit Trip -> Nearby pick + arrival in same session must NOT double-count */
  u8 before_cnt=0;
  for(int i=0;i<MOB_ML_POI_AFF;i++)if(mob_ml.poi_aff[i].kind==6&&mob_ml.poi_aff[i].name_hash==ml_name_hash("ESSO"))before_cnt=mob_ml.poi_aff[i].count;
  ml_record_poi_use(6,p_fuel.lat,p_fuel.lon,"ESSO");
  mob_live.origin=p_home;mob_live.dest=p_fuel;mob_live.depart=ep+3u*86400u;mob_live.duration=300u;
  ml_commit_session();
  u8 after_cnt=0;
  for(int i=0;i<MOB_ML_POI_AFF;i++)if(mob_ml.poi_aff[i].kind==6&&mob_ml.poi_aff[i].name_hash==ml_name_hash("ESSO"))after_cnt=mob_ml.poi_aff[i].count;
  assert(after_cnt==before_cnt+1u);
  /* Unguided Usual Parking: save unique Favourite "OFFICE" ~275 m from TOWN PARK and arrive 3x unguided */
  int saved_nfaves=nfaves,saved_nrecents=nrecents;
  nfaves=1;nrecents=0;faves[0]=(struct SavedPlace){p_work.lat,p_work.lon,"OFFICE"};
  for(int i=0;i<3;i++){
   ml_guided_target_valid=0;
   mob_live.origin=p_home;mob_live.dest=p_park;mob_live.depart=ep+(u32)(10+i)*86400u;mob_live.duration=600u;
   ml_commit_session();
  }
  struct MobPoint learned_park;
  assert(ml_find_usual_parking(p_work,&learned_park)&&mob_dist(learned_park,p_park)<=50u);
  /* Ambiguity guard: two Favourites within 100-600 m must NOT falsely record unguided parking */
  struct MobPoint p_other={5495950,-161800};
  nfaves=2;faves[1]=(struct SavedPlace){p_other.lat,p_other.lon,"GYM"};
  assert(!ml_unguided_parking_target(p_park,&learned_park));
  /* Habitual car-park fallback (no Favourite/Recent needed!): 3 unguided visits to TOWN PARK
   * already populated mob_ml.poi_aff (kind=7, count>=3), so routing to a brand-new address
   * ~280 m away offers TOWN PARK even with zero Favourites/Recents and no mob_ml.parking match. */
  nfaves=0;nrecents=0;
  struct MobPoint p_new_venue={5496100,-162400}; /* ~255 m west of TOWN PARK, far from OFFICE */
  gps.fix=1;track_valid=0;gps.lat=p_home.lat;gps.lon=p_home.lon;
  assert(ml_find_usual_parking(p_new_venue,&learned_park)&&mob_dist(learned_park,p_park)<=50u);
  /* Guard: departing from inside TOWN PARK itself must NOT offer TOWN PARK */
  gps.lat=p_park.lat;gps.lon=p_park.lon;
  assert(!ml_find_usual_parking(p_new_venue,&learned_park));
  /* Guard: routing directly to a Parking POI itself must NOT trigger Priority 2 */
  gps.lat=p_home.lat;gps.lon=p_home.lon;
  assert(!ml_find_usual_parking(p_park,&learned_park));
  /* Guard: a place with >= 2 direct arrivals in mob.journey (e.g. Home) must NOT fall back to a nearby car park */
  u32 saved_mob_n=mob.n;
  assert(mob.n+2u<=MOB_JOURNEYS);
  memzero(&mob.journey[mob.n],sizeof(struct MobJourney));mob.journey[mob.n].origin=p_home;mob.journey[mob.n].dest=p_new_venue;mob.n++;
  memzero(&mob.journey[mob.n],sizeof(struct MobJourney));mob.journey[mob.n].origin=p_home;mob.journey[mob.n].dest=p_new_venue;mob.n++;
  assert(!ml_find_usual_parking(p_new_venue,&learned_park));
  mob.n=saved_mob_n;
  /* Auto-naming unguided recurring stop: Home -> ESSO (10 min dwell) -> OFFICE names the stop "ESSO" */
  nfaves=1;
  mob_ml.last_origin=p_home;mob_ml.last_dest=p_fuel;mob_ml.last_arrival=ep+20u*86400u;mob_ml.last_valid=1u;
  mob_live.origin=p_fuel;mob_live.dest=p_work;mob_live.depart=ep+20u*86400u+600u;mob_live.duration=400u;
  ml_commit_session();
  int found_named_stop=0;
  for(int i=0;i<MOB_ML_STOPS;i++)if(mob_ml.stops[i].count&&mob_dist(mob_ml.stops[i].stop,p_fuel)<=50u&&streq(mob_ml.stops[i].name,"ESSO"))found_named_stop=1;
  assert(found_named_stop);
  /* POI-aware naming for unguided Destination Predictions & Frequent-Place prompts */
  nfaves=0;nrecents=0;trip.active=0;trip.count=0;
  struct SavedPlace named_out;
  assert(frequent_named_place(p_park,&named_out)&&streq(named_out.name,"TOWN PARK"));
  mob_suggest_valid=1;mob_suggest_point=p_fuel;
  assert(suggested_place(&named_out)&&streq(named_out.name,"ESSO"));
  mob_suggest_valid=0;
  nfaves=saved_nfaves;nrecents=saved_nrecents;memzero(tc,sizeof(tc));mapok=0;
 }
 /* MML1 persistence round-trip, corrupt rejection, and full wipe on mob_clear() */
 assert(ml_save());
 ml_reset();assert(mob_ml.road_cls[3].count==0&&ml_hazard_level(991u,0)==0);
 ml_load();assert(mob_ml.road_cls[3].count==9&&mob_ml.road_cls[3].cold_count==4&&ml_hazard_level(991u,0)==1&&ml_learned_brightness(1)==30);
 FILE*cf=fopen(MOB_ML_FILE,"r+b");assert(cf);fseek(cf,64,SEEK_SET);fputc(0xff,cf);fclose(cf);
 ml_load();assert(mob_ml.road_cls[3].count==0&&ml_hazard_level(991u,0)==0);
 assert(ml_save());mob_clear();assert(fopen(MOB_ML_FILE,"rb")==0);
}
/* Excessive U-turn/spur bypasses are rejected so required turnarounds never
 * turn into multi-minute or multi-kilometre detours. */
static void route_spur_detour_cap(int separate_roads,u32 alt_cost,int expect_bypass){
 struct SpurCapGraph {
  struct CprNode node[7];u32 edge[7];struct CprBase base[7];
  struct CprArc arc[7];struct CprPoint geom[14];
 } g={0};
 const struct CprPoint coords[]={{5497000,-161000},{5497300,-161000},{5497300,-160900},
  {5497300,separate_roads?-160990:-161000},{5497600,-161000},{5497900,-161000},{5497400,-161150}};
 u32 from[]={0,1,2,3,4,0,6},to[]={1,2,3,4,5,6,4};
 u32 cost[]={100,50,50,100,100,alt_cost/2u,alt_cost-alt_cost/2u};
 for(int i=0;i<7;i++){
  g.node[i].lat=coords[i].lat;g.node[i].lon=coords[i].lon;g.node[i].comp=1;
  g.base[i].geom_first=i==2&&!separate_roads?2u:2u*i;g.base[i].geom_count=2;
  if(i==2&&!separate_roads)g.base[i].flags=CPR_ARC_GEOM_REVERSED;
  g.geom[2*i]=coords[from[i]];g.geom[2*i+1]=coords[to[i]];
  g.arc[i]=(struct CprArc){from[i],to[i],cost[i],(u32)i,ROUTE_NOARC,0,0,0};
 }
 u32 n=0;
 for(u32 node=0;node<7;node++){
  g.node[node].first_edge=n;
  for(u32 arc=0;arc<7u;arc++)if(to[arc]==node){g.edge[n++]=arc;g.node[node].edge_count++;}
 }
 FILE*f=tmpfile();assert(f&&fwrite(&g,sizeof(g),1,f)==1);assert(!fflush(f));routefd=fileno(f);
 memzero(&rh,sizeof(rh));rh.nodes=7;rh.route_edges=n;rh.base_edges=rh.ch_arcs=7;rh.geom_points=14;
 rh.node_off=offsetof(struct SpurCapGraph,node);rh.edge_off=offsetof(struct SpurCapGraph,edge);
 rh.base_off=offsetof(struct SpurCapGraph,base);rh.arc_off=offsetof(struct SpurCapGraph,arc);rh.geom_off=offsetof(struct SpurCapGraph,geom);
 cpr_bcache_count=0;memzero(cpr_acache_count,sizeof(cpr_acache_count));memzero(cpr_gcache_count,sizeof(cpr_gcache_count));
 routeok=1;route_geom_map_ok=0;route_dest_comp=1;qadjmode=0;atlas_ready=atlas_probe=0;
 pref.learn_history=pref.diversions=0;gps.mph10=0;route_exact_from=0;
 traffic_invalidate();route_status=R_CALC;route_build_phase=RB_DONE;route_head=0;route_n=6;route_result_cost=400;route_result_m=0;
 for(int i=0;i<6;i++){
  route_lat[i]=coords[i].lat;route_lon[i]=coords[i].lon;route_node[i]=(u32)i;
  route_seg_arc[i]=i?(u32)i-1u:ROUTE_NOARC;route_seg_phys[i]=i?g.base[i-1].geom_first:ROUTE_NOARC;
  route_seg_cost[i]=i?cost[i-1]:0;route_seg_m[i]=i?geo_seg_m(coords[i-1].lat,coords[i-1].lon,coords[i].lat,coords[i].lon):0;
  route_result_m+=route_seg_m[i];
 }
 for(int i=0;route_status==R_CALC&&i<200;i++)regional_service_until(s_now_ms()+10u);
 assert(route_status==R_READY&&route_result_m==route_compute_m());
 if(expect_bypass){assert(route_n==4&&route_node[1]==6&&route_result_cost==alt_cost+100u);}
 else{assert(route_n==6&&route_node[2]==2&&route_result_cost==400u);}
 fclose(f);routefd=-1;routeok=0;route_exact_from=ROUTE_NOARC;
}

/* A cleanup bypass that loops through an earlier prefix coordinate must be
 * rejected by traffic_path_revisits_route() instead of creating a figure-8. */
static void route_cleanup_blocks_revisit(void){
 struct RevisitGraph {
  struct CprNode node[8];u32 edge[9];struct CprBase base[9];
  struct CprArc arc[9];struct CprPoint geom[18];
 } g={0};
 /* Node 0 is retained prefix; spur at [2..4] tries bypass via node 7 which shares
  * coordinates with prefix node 0 (figure-8 revisit). */
 const struct CprPoint coords[]={{5496500,-161000},{5497000,-161000},{5497300,-161000},
  {5497300,-160900},{5497300,-161000},{5497600,-161000},{5497900,-161000},{5496500,-161000}};
 u32 from[]={0,1,2,3,4,5,1,7,7},to[]={1,2,3,4,5,6,7,5,5};
 u32 cost[]={100,100,50,50,100,100,140,180,180};
 for(int i=0;i<8;i++){g.node[i].lat=coords[i].lat;g.node[i].lon=coords[i].lon;g.node[i].comp=1;}
 for(int i=0;i<8;i++){
  g.base[i].geom_first=i==3?4u:2u*i;g.base[i].geom_count=2;
  if(i==3)g.base[i].flags=CPR_ARC_GEOM_REVERSED;
  g.geom[2*i]=coords[from[i]];g.geom[2*i+1]=coords[to[i]];
  g.arc[i]=(struct CprArc){from[i],to[i],cost[i],(u32)i,ROUTE_NOARC,0,0,0};
 }
 u32 n=0;
 for(u32 node=0;node<8;node++){
  g.node[node].first_edge=n;
  for(u32 arc=0;arc<8u;arc++)if(to[arc]==node){g.edge[n++]=arc;g.node[node].edge_count++;}
 }
 FILE*f=tmpfile();assert(f&&fwrite(&g,sizeof(g),1,f)==1);assert(!fflush(f));routefd=fileno(f);
 memzero(&rh,sizeof(rh));rh.nodes=8;rh.route_edges=n;rh.base_edges=rh.ch_arcs=8;rh.geom_points=16;
 rh.node_off=offsetof(struct RevisitGraph,node);rh.edge_off=offsetof(struct RevisitGraph,edge);
 rh.base_off=offsetof(struct RevisitGraph,base);rh.arc_off=offsetof(struct RevisitGraph,arc);rh.geom_off=offsetof(struct RevisitGraph,geom);
 cpr_bcache_count=0;memzero(cpr_acache_count,sizeof(cpr_acache_count));memzero(cpr_gcache_count,sizeof(cpr_gcache_count));
 routeok=1;route_geom_map_ok=0;route_dest_comp=1;qadjmode=0;atlas_ready=atlas_probe=0;
 pref.learn_history=pref.diversions=0;gps.mph10=0;route_exact_from=0;
 traffic_invalidate();route_status=R_CALC;route_build_phase=RB_DONE;route_head=0;route_n=7;route_result_cost=500;route_result_m=0;
 for(int i=0;i<7;i++){
  route_lat[i]=coords[i].lat;route_lon[i]=coords[i].lon;route_node[i]=(u32)i;
  route_seg_arc[i]=i?(u32)i-1u:ROUTE_NOARC;route_seg_phys[i]=i?g.base[i-1].geom_first:ROUTE_NOARC;
  route_seg_cost[i]=i?cost[i-1]:0;route_seg_m[i]=i?geo_seg_m(coords[i-1].lat,coords[i-1].lon,coords[i].lat,coords[i].lon):0;
  route_result_m+=route_seg_m[i];
 }
 /* Run cleanup directly with begin=1, join=5 (node 0 is retained prefix). */
 route_cleanup_begin(s_now_ms());
 route_cleanup_limit_ds=300u+ROUTE_CLEANUP_SPUR_EXTRA_DS;
 route_cleanup_distance_m=route_seg_m[2]+route_seg_m[3]+route_seg_m[4]+route_seg_m[5];
 route_cleanup_extra_m=ROUTE_CLEANUP_SPUR_EXTRA_M;
 route_build_phase=RB_CLEANUP;traffic_search_kind=TRAFFIC_SEARCH_ROUTE_CLEANUP;
 traffic_search_init(1,5,2u,300u,s_now_ms());
 for(int i=0;traffic_searching&&i<64;i++)traffic_search_step(s_now_ms());
 /* Figure-8 revisit through node 7 (same lat/lon as prefix node 0) must be rejected! */
 assert(route_n==7&&route_node[3]==3&&route_result_cost==500u);
 fclose(f);routefd=-1;routeok=0;route_exact_from=ROUTE_NOARC;
}

/* A 4-turn residential zigzag cluster (within 350 m between turns, <= 800 m span)
 * must be simplified in a single TRAFFIC_SEARCH_FEWER_TURNS pass. */
static void route_four_turn_cluster(void){
 struct Zig4Graph {
  struct CprNode node[9];u32 edge[9];struct CprBase base[9];
  struct CprArc arc[9];struct CprPoint geom[18];
 } g={0};
 /* 0->1 (N), 1->2 (E, turn 1), 2->3 (N, turn 2), 3->4 (W, turn 3), 4->5 (N, turn 4), 5->6 (N)
  * Simpler bypass: 0 -> 7 -> 5 (straight N corridor, 0 turns!). */
 const struct CprPoint coords[]={{5497000,-161000},{5497200,-161000},{5497200,-160850},
  {5497350,-160850},{5497350,-161000},{5497550,-161000},{5497800,-161000},
  {5497300,-161000},{5497900,-161000}};
 const u32 from[]={0,1,2,3,4,5,0,7,5},to[]={1,2,3,4,5,6,7,5,8};
 const u32 cost[]={100,80,80,80,100,100,240,240,100};
 for(int i=0;i<9;i++){g.node[i].lat=coords[i].lat;g.node[i].lon=coords[i].lon;g.node[i].comp=1;}
 for(int i=0;i<8;i++){
  g.base[i].geom_first=2u*i;g.base[i].geom_count=2;
  g.geom[2*i]=coords[from[i]];g.geom[2*i+1]=coords[to[i]];
  g.arc[i]=(struct CprArc){from[i],to[i],cost[i],(u32)i,ROUTE_NOARC,0,0,0};
 }
 u32 n=0;
 for(u32 node=0;node<8;node++){
  g.node[node].first_edge=n;
  for(u32 arc=0;arc<8u;arc++)if(to[arc]==node){g.edge[n++]=arc;g.node[node].edge_count++;}
 }
 FILE*f=tmpfile();assert(f&&fwrite(&g,sizeof(g),1,f)==1);assert(!fflush(f));routefd=fileno(f);
 memzero(&rh,sizeof(rh));rh.nodes=8;rh.route_edges=n;rh.base_edges=rh.ch_arcs=8;rh.geom_points=16;
 rh.node_off=offsetof(struct Zig4Graph,node);rh.edge_off=offsetof(struct Zig4Graph,edge);
 rh.base_off=offsetof(struct Zig4Graph,base);rh.arc_off=offsetof(struct Zig4Graph,arc);rh.geom_off=offsetof(struct Zig4Graph,geom);
 cpr_bcache_count=0;memzero(cpr_acache_count,sizeof(cpr_acache_count));memzero(cpr_gcache_count,sizeof(cpr_gcache_count));
 routeok=1;route_geom_map_ok=0;route_dest_comp=1;qadjmode=0;atlas_ready=atlas_probe=0;
 pref.learn_history=pref.diversions=0;gps.mph10=0;route_exact_from=0;
 traffic_invalidate();route_status=R_CALC;route_build_phase=RB_DONE;route_head=0;route_n=7;route_result_cost=540;route_result_m=0;
 for(int i=0;i<7;i++){
  route_lat[i]=coords[i].lat;route_lon[i]=coords[i].lon;route_node[i]=(u32)i;
  route_seg_arc[i]=i?(u32)i-1u:ROUTE_NOARC;route_seg_phys[i]=i?g.base[i-1].geom_first:ROUTE_NOARC;
  route_seg_cost[i]=i?cost[i-1]:0;route_seg_m[i]=i?geo_seg_m(coords[i-1].lat,coords[i-1].lon,coords[i].lat,coords[i].lon):0;
  route_result_m+=route_seg_m[i];
 }
 for(int i=0;route_status==R_CALC&&i<200;i++)regional_service_until(s_now_ms()+10u);
 assert(route_status==R_READY&&route_turn_attempts==1);
 assert(route_n==4&&route_node[1]==7&&route_node[2]==5&&route_result_cost==580u&&route_window_turns(0,route_n-1)==0);
 fclose(f);routefd=-1;routeok=0;route_exact_from=ROUTE_NOARC;
}

/* Two-way start & destination twin seeding eliminates U-turn spurs when the
 * lowest-ID snap arc faces away from the journey, while moving heading penalty
 * steers a moving vehicle forward without inflating the displayed ETA. */
static void route_twoway_endpoint_twins(void){
 struct TwinGraph {
  struct CprNode node[4];
  u32 upf[6],upr[6],tref[6],sref[6];
  struct CprBase base[6];
  struct CprArc arc[6];
  struct CprPoint geom[6];
  struct CprTile tile[1];
 } g={0};
 /* Start street: 0(N) <-> 1(S) [phys geom 0..1]; arc0 is 1->0 (N, wrong way), arc1 is 0->1 (S, right way).
  * Connector: 1 -> 2 [phys geom 2..3], plus long forward loop 0 -> 2 (cost 400) for moving heading test.
  * Dest street: 2(N) <-> 3(S) [phys geom 4..5]; arc4 is 3->2 (N, wrong way), arc5 is 2->3 (S, right way). */
 const s32 base_lat=21472*256;
 const struct CprPoint pts[4]={{base_lat+220,-161000},{base_lat+160,-161000},{base_lat+100,-161000},{base_lat+40,-161000}};
 for(int i=0;i<4;i++){g.node[i].lat=pts[i].lat;g.node[i].lon=pts[i].lon;g.node[i].comp=1;g.node[i].phys=(u32)i;}
 g.geom[0]=pts[1];g.geom[1]=pts[0];
 g.geom[2]=pts[1];g.geom[3]=pts[2];
 g.geom[4]=pts[3];g.geom[5]=pts[2];
 /* Arcs: 0: 1->0(100), 1: 0->1(100,rev), 2: 1->2(100), 3: 0->2(150), 4: 3->2(100), 5: 2->3(100,rev) */
 const u32 af[6]={1,0,1,0,3,2},at[6]={0,1,2,2,2,3},ac[6]={100,100,100,150,100,100};
 const u32 gf[6]={0,0,2,2,4,4};const u8 fl[6]={0,CPR_ARC_GEOM_REVERSED,0,0,0,CPR_ARC_GEOM_REVERSED};
 for(int i=0;i<6;i++){
  g.base[i].cost=ac[i];g.base[i].to_state=at[i];g.base[i].geom_first=gf[i];g.base[i].geom_count=2;
  g.base[i].flags=fl[i];g.base[i].src_first=(u32)i;g.base[i].src_count=1;g.sref[i]=af[i];
  g.arc[i]=(struct CprArc){af[i],at[i],ac[i],(u32)i,ROUTE_NOARC,0,0,0};
  g.tref[i]=(u32)i;
 }
 /* Simple CH upward graphs (all ranks equal -> put all outgoing in upf, all incoming in upr). */
 u32 nf=0,nr=0;
 for(u32 v=0;v<4;v++){
  g.node[v].upf_first=nf;for(u32 i=0;i<6;i++)if(af[i]==v){g.upf[nf++]=i;g.node[v].upf_count++;}
  g.node[v].upr_first=nr;for(u32 i=0;i<6;i++)if(at[i]==v){g.upr[nr++]=i;g.node[v].upr_count++;}
 }
 g.tile[0].tx=tile_div(-161000,6);g.tile[0].ty=tile_div(base_lat+190,6);g.tile[0].first=0;g.tile[0].count=6;
 assert(tile_div(base_lat+70,6)==g.tile[0].ty); /* both start & dest in same Z6 tile */
 FILE*f=tmpfile();assert(f&&fwrite(&g,sizeof(g),1,f)==1);assert(!fflush(f));routefd=fileno(f);
 memzero(&rh,sizeof(rh));rh.nodes=rh.phys_nodes=4;rh.base_edges=rh.ch_arcs=6;rh.geom_points=6;rh.tiles=1;
 rh.upf_count=nf;rh.upr_count=nr;rh.tile_refs=6;rh.src_refs=6;
 rh.minlat=5490000;rh.maxlat=5500000;rh.minlon=-170000;rh.maxlon=-150000;
 rh.node_off=offsetof(struct TwinGraph,node);rh.base_off=offsetof(struct TwinGraph,base);
 rh.arc_off=offsetof(struct TwinGraph,arc);rh.geom_off=offsetof(struct TwinGraph,geom);
 rh.tile_off=offsetof(struct TwinGraph,tile);rh.upf_off=offsetof(struct TwinGraph,upf);
 rh.upr_off=offsetof(struct TwinGraph,upr);rh.tref_off=offsetof(struct TwinGraph,tref);
 rh.src_off=offsetof(struct TwinGraph,sref);
 cpr_bcache_count=0;memzero(cpr_acache_count,sizeof(cpr_acache_count));memzero(cpr_gcache_count,sizeof(cpr_gcache_count));
 routeok=1;route_geom_map_ok=0;qadjmode=0;atlas_ready=atlas_probe=0;pref.learn_history=pref.diversions=0;

 /* Case 1: Parked halfway along 0<->1 going to halfway along 2<->3.
  * Lowest-ID arcs (0 and 4) both face NORTH (wrong way); twin seeding picks
  * SOUTHBOUND arcs (1 and 5) -> direct 200 ds route with ZERO U-turns! */
 gps.fix=1;gps.mph10=0;gps.lat=track_lat=base_lat+190;gps.lon=track_lon=-161000;track_valid=1;
 route_exact_from=route_exact_to=ROUTE_NOARC;
 assert(regional_route_start(base_lat+70,-161000));
 for(int i=0;route_status==R_CALC&&i<200;i++)regional_service_until(s_now_ms()+10u);
 assert(route_status==R_READY&&route_start_used_twin&&route_dest_used_twin);
 assert(route_result_cost==200u);
 for(int i=1;i<route_n-1;i++)assert(!route_cleanup_uturn(i));

 /* Case 2: Moving NORTH (course=0, 20 mph) along 1->0: soft +180 s U-turn penalty
  * on southbound twin (1) makes forward path 0->2 (cost 50+150+50=250) beat
  * backward U-turn (50+1800+100+50=2000), and displayed ETA stays true (250 ds)! */
 route_cancel();
 gps.mph10=200;gps.course=track_course=display_course=0;display_course_valid=1;
 gps.lat=track_lat=base_lat+190;gps.lon=track_lon=-161000;
 assert(regional_route_start(base_lat+70,-161000));
 for(int i=0;route_status==R_CALC&&i<200;i++)regional_service_until(s_now_ms()+10u);
 assert(route_status==R_READY&&!route_start_used_twin&&route_dest_used_twin);
 assert(route_result_cost==250u);

 /* Case 3: Moving NORTH with a 400 s forward detour (cost 4000) and no U-turn
  * edge at node 0: penalized backward twin (50+1800+100+50=2000) beats forward
  * detour (50+4000+50=4100), and the soft +180 s penalty is stripped from ETA (200 ds)! */
 g.arc[3].cost=g.base[3].cost=4000u;g.node[0].upf_first=0;g.node[0].upf_count=1;g.upf[0]=3;g.node[1].upr_count=0;
 rewind(f);assert(fwrite(&g,sizeof(g),1,f)==1&&!fflush(f));
 cpr_bcache_count=0;memzero(cpr_acache_count,sizeof(cpr_acache_count));memzero(cpr_rcache_count,sizeof(cpr_rcache_count));
 route_cancel();
 assert(regional_route_start(base_lat+70,-161000));
 for(int i=0;route_status==R_CALC&&i<200;i++)regional_service_until(s_now_ms()+10u);
 assert(route_status==R_READY&&route_start_used_twin&&route_dest_used_twin);
 assert(route_result_cost==200u);

 route_cancel();fclose(f);routefd=-1;routeok=0;gps.mph10=0;
}

/* Divided-carriageway V-pinch U-turns (two separate one-way carriageways meeting
 * at a 120-150 deg flare that widens to a 160-200 deg reversal over 30 m) must
 * be detected by route_cleanup_uturn() and rejected during RB_CLEANUP so a
 * cheaper V-pinch never replaces a spur ahead of a clean earlier bypass. */
static void route_divided_vpinch_rejected(void){
 struct VPinchGraph {
  struct CprNode node[9];u32 edge[12];struct CprBase base[12];
  struct CprArc arc[12];struct CprPoint geom[26];
 } g={0};
 /* Mirrors Newcastle B1307 Percy Street southbound/northbound V-pinch at node 2. */
 const struct CprPoint coords[]={
  {5497520,-161670}, /* 0: north approach */
  {5497433,-161710}, /* 1: southbound carriageway */
  {5497419,-161725}, /* 2: V-pinch junction node */
  {5497413,-161619}, /* 3: spur tip east */
  {5497419,-161725}, /* 4: spur return state */
  {5497410,-161798}, /* 5: west destination approach */
  {5497434,-161724}, /* 6: northbound carriageway (17 m flare, dir 0 deg) */
  {5497458,-161715}, /* 7: northbound carriageway (44 m corridor, dir 10 deg) */
  {5497520,-161800}, /* 8: clean earlier westward bypass */
 };
 /* Arcs:
  * 0: 0->1 (sb approach, cost 60)
  * 1: 1->2 (sb flare via (5497428,-161714), local exit 220 deg, cost 20)
  * 2: 2->3 (spur out, cost 40)
  * 3: 3->4 (spur back, same phys as 2, cost 40)
  * 4: 4->5 (final west leg, cost 50)
  * 5: 2->6 (nb flare via (5497429,-161725), local entry 0 deg, cost 20)
  * 6: 6->7 (nb carriageway, cost 20)
  * 7: 7->5 (short loop back to 5, cost 60 -> V-pinch total 0->1->2->6->7->5 = 180)
  * 8: 0->8 (clean early turn west, cost 130)
  * 9: 8->5 (clean approach south to 5, cost 130 -> clean total = 260)
  * 10: 5->5 dummy tail? No, use 10: 5->dest not needed; join at 5 with arc 10 outgoing 5->west */
 const u32 af[]={0,1,2,3,4,2,6,7,0,8};
 const u32 at[]={1,2,3,4,5,6,7,5,8,5};
 const u32 ac[]={60,20,40,40,50,20,20,60,130,130};
 for(int i=0;i<9;i++){g.node[i].lat=coords[i].lat;g.node[i].lon=coords[i].lon;g.node[i].comp=1;}
 u32 gp=0;
 for(int i=0;i<10;i++){
  if(i==3){
   g.base[i].geom_first=g.base[2].geom_first;g.base[i].geom_count=2;g.base[i].flags=CPR_ARC_GEOM_REVERSED;
  }else if(i==1){
   g.base[i].geom_first=gp;g.base[i].geom_count=3;
   g.geom[gp++]=coords[1];g.geom[gp++]=(struct CprPoint){5497428,-161714};g.geom[gp++]=coords[2];
  }else if(i==5){
   g.base[i].geom_first=gp;g.base[i].geom_count=3;
   g.geom[gp++]=coords[2];g.geom[gp++]=(struct CprPoint){5497429,-161725};g.geom[gp++]=coords[6];
  }else{
   g.base[i].geom_first=gp;g.base[i].geom_count=2;
   g.geom[gp++]=coords[af[i]];g.geom[gp++]=coords[at[i]];
  }
  g.base[i].cost=ac[i];
  g.arc[i]=(struct CprArc){af[i],at[i],ac[i],(u32)i,ROUTE_NOARC,0,0,0};
 }
 u32 ne=0;
 for(u32 v=0;v<9;v++){
  g.node[v].first_edge=ne;
  for(u32 i=0;i<10;i++)if(at[i]==v){g.edge[ne++]=i;g.node[v].edge_count++;}
 }
 FILE*f=tmpfile();assert(f&&fwrite(&g,sizeof(g),1,f)==1);assert(!fflush(f));routefd=fileno(f);
 memzero(&rh,sizeof(rh));rh.nodes=9;rh.route_edges=ne;rh.base_edges=rh.ch_arcs=10;rh.geom_points=gp;
 rh.node_off=offsetof(struct VPinchGraph,node);rh.edge_off=offsetof(struct VPinchGraph,edge);
 rh.base_off=offsetof(struct VPinchGraph,base);rh.arc_off=offsetof(struct VPinchGraph,arc);rh.geom_off=offsetof(struct VPinchGraph,geom);
 cpr_bcache_count=0;memzero(cpr_acache_count,sizeof(cpr_acache_count));memzero(cpr_gcache_count,sizeof(cpr_gcache_count));
 memzero(route_turn_cache,sizeof(route_turn_cache));
 routeok=1;route_geom_map_ok=0;route_dest_comp=1;qadjmode=0;atlas_ready=atlas_probe=0;
 pref.learn_history=pref.diversions=0;gps.mph10=0;route_exact_from=0;route_exact_to=ROUTE_NOARC;route_dest_twin_ok=0;

 /* Part 1: Verify route_cleanup_uturn() flags the V-pinch (140 deg local, 170 deg 30 m corridor)
  * and ignores it when either carriageway carries the roundabout flag. */
 struct {s32 lat,lon;u32 node,arc;} vp[]={
  {5497520,-161670,0,ROUTE_NOARC},{5497433,-161710,1,0},{5497428,-161714,ROUTE_NOARC,1},
  {5497419,-161725,2,1},{5497429,-161725,ROUTE_NOARC,5},{5497434,-161724,6,5},
  {5497458,-161715,7,6},{5497410,-161798,5,7}
 };
 route_head=0;route_n=8;
 for(int i=0;i<8;i++){
  route_lat[i]=vp[i].lat;route_lon[i]=vp[i].lon;route_node[i]=vp[i].node;route_seg_arc[i]=vp[i].arc;
  route_seg_phys[i]=vp[i].arc==ROUTE_NOARC?ROUTE_NOARC:g.base[vp[i].arc].geom_first;
  route_seg_m[i]=i?geo_seg_m(vp[i-1].lat,vp[i-1].lon,vp[i].lat,vp[i].lon):0;
 }
 assert(!route_turns_back(route_lat[2],route_lon[2],route_lat[3],route_lon[3],route_lat[4],route_lon[4]));
 assert(route_cleanup_uturn(3));
 g.base[5].flags|=4;rewind(f);assert(fwrite(&g,sizeof(g),1,f)==1&&!fflush(f));
 cpr_bcache_count=0;memzero(route_turn_cache,sizeof(route_turn_cache));
 assert(!route_cleanup_uturn(3));
 g.base[5].flags&=~4u;rewind(f);assert(fwrite(&g,sizeof(g),1,f)==1&&!fflush(f));
 cpr_bcache_count=0;memzero(route_turn_cache,sizeof(route_turn_cache));

 /* Part 2: Run RB_CLEANUP on the spur route 0->1->2->3->4->5.
  * The cheaper V-pinch (0->1->2->6->7->5, cost 180) must be rejected in favour
  * of the clean westward bypass (0->8->5, cost 260). */
 struct {s32 lat,lon;u32 node,arc,cost;} sp[]={
  {5497520,-161670,0,ROUTE_NOARC,0},{5497433,-161710,1,0,60},{5497428,-161714,ROUTE_NOARC,1,10},
  {5497419,-161725,2,1,10},{5497413,-161619,3,2,40},{5497419,-161725,4,3,40},{5497410,-161798,5,4,50}
 };
 traffic_invalidate();route_status=R_CALC;route_build_phase=RB_DONE;route_head=0;route_n=7;route_result_cost=210;route_result_m=0;
 for(int i=0;i<7;i++){
  route_lat[i]=sp[i].lat;route_lon[i]=sp[i].lon;route_node[i]=sp[i].node;route_seg_arc[i]=sp[i].arc;
  route_seg_phys[i]=sp[i].arc==ROUTE_NOARC?ROUTE_NOARC:g.base[sp[i].arc].geom_first;
  route_seg_cost[i]=sp[i].cost;
  route_seg_m[i]=i?geo_seg_m(sp[i-1].lat,sp[i-1].lon,sp[i].lat,sp[i].lon):0;
  route_result_m+=route_seg_m[i];
 }
 for(int i=0;route_status==R_CALC&&i<200;i++)regional_service_until(s_now_ms()+10u);
 assert(route_status==R_READY&&route_result_m==route_compute_m());
 assert(route_n==3&&route_node[0]==0&&route_node[1]==8&&route_node[2]==5&&route_result_cost==260u);
 for(int i=1;i<route_n-1;i++)assert(!route_cleanup_uturn(i));

 route_cancel();fclose(f);routefd=-1;routeok=0;route_exact_from=ROUTE_NOARC;
}

int main(void){(void)load_font();learner();predictor_quality();prediction_names();prediction_ui();frequent_places();shared_geometry();tiny_graph();two_diversions();ml_features();route_spur(1,0,0);route_spur(0,1,0);route_spur(0,0,0);route_spur(0,1,1);route_spur(0,0,1);route_spur_detour_cap(0,1300u,0);route_spur_detour_cap(1,950u,0);route_spur_detour_cap(1,800u,1);route_cleanup_blocks_revisit();route_four_turn_cluster();route_twoway_endpoint_twins();route_divided_vpinch_rejected();route_fewer_turns(670u,1,0,1);route_fewer_turns(780u,1,0,0);route_fewer_turns(620u,0,0,0);route_fewer_turns(620u,1,1,0);assert(route_count_turn(0,ROUTE_HEADING_ROUNDABOUT)==1&&route_count_turn(ROUTE_HEADING_ROUNDABOUT,ROUTE_HEADING_ROUNDABOUT)==0&&route_count_turn(ROUTE_HEADING_ROUNDABOUT,9)==0);puts("Mobility, historical diversions, two-way endpoint twins, divided-carriageway V-pinch guard, bounded U-turn avoidance and legal routing with fewer turns passed.");return 0;}
