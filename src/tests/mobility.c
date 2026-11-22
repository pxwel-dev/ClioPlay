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
int main(void){(void)load_font();learner();predictor_quality();prediction_names();prediction_ui();frequent_places();shared_geometry();tiny_graph();two_diversions();ml_features();puts("Mobility recording, contextual predictor, session dismissal, startup and trajectory evidence, map confirmation, offline names, frequent-place prompts and persistence, road profiles, diversion regressions and extended local ML features passed.");return 0;}
