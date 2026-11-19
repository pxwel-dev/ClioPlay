/* Real production renderer, models and remote handler, with isolated host I/O. */
#define HOST_TEST 1
#define main unused_device_entry
#ifndef CLIO_SOURCE
#define CLIO_SOURCE "../clio843/src/clio040.c"
#endif
#include CLIO_SOURCE
#undef main
#include "fixtures/route.h"
#ifndef CLIO_BOOT_ART
#define CLIO_BOOT_ART "../clio843/src/boot_art.h"
#endif
#include CLIO_BOOT_ART
#include <assert.h>
#include <string.h>
static const char*outdir;
static void snapshot(const char*name){char p[1024];snprintf(p,sizeof(p),"%s/%s.rgb565",outdir,name);FILE*f=fopen(p,"wb");assert(f);assert(fwrite(fb,1,sizeof(fb),f)==sizeof(fb));fclose(f);}
static void explore_sample(u32 elapsed){hud_capture_clock=1;hud_capture_ms+=elapsed;freecam_orientation_anim_update(hud_capture_ms);}
static void settle(void){
 draw();
#ifdef HUD_TURN_ICON
 hud_settle_for_capture(hud_now_ms());menu_settle(hud_now_ms());draw();
#endif
}
static void fixture(void){
 mob_stop_recording();mob_model_reset();mob_current_epoch=MOB_UNKNOWN;traffic_invalidate();traffic_learning_reset();
 pref.learn_history=0;pref.destination_predictions=pref.diversions=1;
 boot_fade_cancel();
 pref_keyboard_language=-1;kbd_language=kbd_page=kbd_picker=kbd_column=0;kbd_clear_composition();
 gps.fix=track_valid=display_course_valid=1;gps.lat=track_lat=cp_route[0][0];gps.lon=track_lon=cp_route[0][1];gps.course=track_course=display_course=0;gps.mph10=240;gps.hour=21;gps.min=41;gps.day=19;gps.mon=10;gps.year=2026;
 pref.hoff=1;pref.autozoom=0;pref.debug=0;pref.bright=50;manual_zoom=freecam_zoom=4;
 freecam=0;map_tilt_q8=256;freecam_anim_mode=FC_ANIM_NONE;perspective_lut_tilt=-1;
 car.temp=18;car.tempok=1;copystr(car.volume,sizeof(car.volume),"VOL 12");
 audio_source_live=1;copystr(af.band,sizeof(af.band),"CD");btui.connected=btui.playing=btui.paired=1;copystr(btui.title,sizeof(btui.title),"Night Drive");copystr(btui.artist,sizeof(btui.artist),"ClioPlay demo");copystr(btui.device,sizeof(btui.device),"Demo phone");btui.pos=65;btui.dur=180;
 roadctx.valid=1;roadctx.speed=30;roadctx.speed_flags=0;copystr(roadctx.street,sizeof(roadctx.street),"Quayside");
 route_status=R_READY;route_n=CP_ROUTE_N;route_head=0;route_result_m=2643;route_result_cost=2267;
 for(int i=0;i<route_n;i++){route_lat[i]=cp_route[i][0];route_lon[i]=cp_route[i][1];route_node[i]=ROUTE_NOARC;route_seg_phys[i]=i<8?1:2;route_seg_m[i]=i?13:0;route_seg_cost[i]=i?11:0;}
 nav_man_type=MAN_RIGHT;nav_man_m=183;nav_man_idx=8;
 trip.active=1;trip.count=2;trip.active_idx=trip.paused=0;copystr(trip.target[0].name,sizeof(trip.target[0].name),"Grey Street");trip.target[0].lat=5497370;trip.target[0].lon=-161331;copystr(trip.target[1].name,sizeof(trip.target[1].name),"Home");trip.target[1].lat=5501450;trip.target[1].lon=-169420;
 nrecents=25;nfaves=25;
 for(int i=0;i<25;i++){recents[i].lat=5497548+i;recents[i].lon=-164050-i;snprintf(recents[i].name,sizeof(recents[i].name),"Recent place %02d",i+1);faves[i]=recents[i];snprintf(faves[i].name,sizeof(faves[i].name),"Favourite %02d",i+1);}
 copystr(recents[0].name,sizeof(recents[0].name),"Kareem's Grill");copystr(recents[1].name,sizeof(recents[1].name),"Olive & Bean");
 nbtdev=10;for(int i=0;i<10;i++){snprintf(btdev[i].name,sizeof(btdev[i].name),"Saved phone %02d",i+1);snprintf(btdev[i].addr,sizeof(btdev[i].addr),"00:11:22:33:44:%02d",i);btdev[i].connected=i==0;}
 copystr(btdev[0].name,sizeof(btdev[0].name),"Demo phone");
 modal=MOD_NONE;bt_page_dialog=-1;bt_control_mode=0;trip_need_route=0;notice_text[0]=0;
#ifdef HUD_TURN_ICON
 hud_audio.kind=hud_source.kind=hud_navigation.kind=hud_connection.kind=hud_bt_feedback.kind=HUD_NONE;
 hud_observed_route=route_status;hud_reset_animations();
#endif
}

/* Controlled learned records, not private driver history. The destination
 * sample uses the real predictor and favourite matching. The diversion
 * sample presents the output of a synthetic legal-state fixture; the
 * actual candidate search and splice are tested in mobility.c. */
static void preview_prediction_seed(void){
 clear_trip();pref.learn_history=pref.destination_predictions=1;
 mob_model_reset();mob_origin_valid=1;mob_origin_confirmed=0;mob_started=0;mob_declined_n=0;mob_suggest_valid=0;
 mob_live.origin=(struct MobPoint){car_lat(),car_lon()};
 u32 e=mob_epoch(2010,1,4,7,30,0);
 for(unsigned i=0;i<5;i++){
  struct MobJourney*j=&mob.journey[i];
  j->origin=mob_live.origin;j->dest=(struct MobPoint){5497370,-161331};
  j->depart=e+i*7u*86400u;j->serial=i+1;
 }
 mob.n=mob.head=mob.serial=5;mob_current_epoch=e+35u*86400u;mob_clock_ms=hud_now_ms();
 nfaves=1;faves[0].lat=5497370;faves[0].lon=-161331;
 copystr(faves[0].name,sizeof(faves[0].name),"Work");
 struct SavedPlace choice;assert(mob_predict(hud_now_ms())&&suggested_place(&choice)&&streq(choice.name,"Work"));
}
/* Five distinct completed sessions at the current stationary position.
 * Run the actual startup service, including its persistence and GPS checks. */
static void preview_frequent_refresh(void){
 u32 now=hud_now_ms(),epoch=mob_epoch(2026,10,21,12,35,0);
 mob_rmc(now,epoch+(now-mob_begin_ms)/1000u,1,gps.lat,gps.lon,0,0);
}
static void preview_frequent_seed(void){
 clear_trip();pref.learn_history=pref.destination_predictions=1;pref.language=0;pref_keyboard_language=-1;
 mob_model_reset();mob_fave_reset();nfaves=0;nrecents=1;gps.fix=1;gps.mph10=0;bt_control_mode=0;freecam=0;
 struct MobPoint p={car_lat(),car_lon()};gps.lat=track_lat=p.lat;gps.lon=track_lon=p.lon;
 recents[0].lat=p.lat;recents[0].lon=p.lon;copystr(recents[0].name,sizeof(recents[0].name),"Work 2");
 u32 now=hud_now_ms(),epoch=mob_epoch(2026,10,21,12,35,0);
 /* Earlier startup notices have already had their normal screen time.
  * Pre-roll the production event queue, so Save's own feedback is visible
  * during this short capture even if the host has no installed map. */
 hud_status_reset();hud_status_observe(1,now-10000u);
 struct HudCard pre={0};hud_status_compose(&pre,now-10000u);
 memzero(&pre,sizeof(pre));hud_status_compose(&pre,now-5000u);
 memzero(&pre,sizeof(pre));hud_status_compose(&pre,now);
 mob_session_begin(now-5000u,1,p.lat,p.lon);frequent_arm_startup();
 for(unsigned i=0;i<5;i++){struct MobJourney*j=&mob.journey[i];j->origin=(struct MobPoint){p.lat+1000,p.lon};j->dest=p;j->depart=epoch-(5u-i)*86400u;j->serial=i+1;j->duration=600;}
 mob.n=mob.head=mob.serial=5;
 for(unsigned i=1;i<=5;i++){hud_capture_ms=now-5000u+i*1000u;preview_frequent_refresh();int fresh=mob_sample(hud_now_ms());frequent_startup_service(hud_now_ms(),fresh);}
 assert(sheet_page==P_CONFIRM&&confirm_kind==5&&sheet_focus==1&&nfaves==0);
}
static void preview_diversion_seed(void){
 /* This is a presentation fixture, not an assertion about real roads.
  * Keep the installed map hidden rather than draw invented geometry over it. */
 mapok=0;pref.learn_history=pref.diversions=1;gps.fix=1;
 route_status=R_READY;route_head=0;route_n=4;route_result_cost=2700;route_result_m=1500;
 s32 lat=car_lat(),lon=car_lon();
 const s32 dy[]={0,100,500,700},dx[]={0,0,0,0};
 for(int i=0;i<4;i++){
  route_lat[i]=lat+dy[i];route_lon[i]=lon+dx[i];route_node[i]=(u32)i;
  route_seg_arc[i]=route_seg_phys[i]=ROUTE_NOARC;
  route_seg_cost[i]=i==2?2300:i?200:0;route_seg_m[i]=i==2?440:i?110:0;
 }
 traffic_invalidate();traffic_region=atlas_route_region;traffic_offer_revision=traffic_route_revision;
 traffic_begin_idx=1;traffic_join_idx=2;traffic_offer_n=4;
 traffic_path[0]=(struct TrafficPoint){lat+100,lon,1,0,ROUTE_NOARC,ROUTE_NOARC,0};
 traffic_path[1]=(struct TrafficPoint){lat+210,lon+150,4,250,ROUTE_NOARC,ROUTE_NOARC,190};
 traffic_path[2]=(struct TrafficPoint){lat+410,lon+150,5,250,ROUTE_NOARC,ROUTE_NOARC,220};
 traffic_path[3]=(struct TrafficPoint){lat+500,lon,2,250,ROUTE_NOARC,ROUTE_NOARC,170};
 traffic_warn=1;traffic_eta_revision=traffic_route_revision;traffic_slow_end=2;traffic_eta_ds=2000;traffic_saving=3550;
 trip.active=1;trip.count=1;trip.active_idx=0;
 copystr(trip.target[0].name,sizeof(trip.target[0].name),"Work");
 assert(traffic_prepare_offer()&&traffic_offer_valid());
}

static void scenes(void){
 const int pages[]={P_CLOSED,P_HOME,P_TRIP,P_PHONE,P_SETTINGS,P_NAME};
 const char*names[]={"driving","menu","trip","bluetooth","settings","keyboard"};
 for(int night=0;night<2;night++)for(int i=0;i<6;i++){fixture();screen=SCR_MAP;pref.theme=night?THEME_NIGHT:THEME_DAY;keyboard_open_fave_name(1,2);menu_open(pages[i],0,hud_now_ms());settle();char name[64];snprintf(name,sizeof(name),"%s-%s",names[i],night?"night":"day");snapshot(name);}
 fixture();menu_open(P_TRIP,0,hud_now_ms());settle();snapshot("trip-itinerary");
 for(int i=0;i<2;i++){places_tab=i;menu_open(P_PLACES,24,hud_now_ms());settle();snapshot(i?"trip-favourites":"trip-last-place");}
 fixture();trip.active=trip.count=0;menu_open(P_TRIP,0,hud_now_ms());settle();snapshot("trip-empty");
 fixture();menu_open(P_PHONE,nbtdev,hud_now_ms());settle();snapshot("bluetooth-last");
 fixture();screen=SCR_MAP;bt_control_mode=1;settle();snapshot("driving-media");
 fixture();screen=SCR_MAP;set_band("FM");copystr(af.station,sizeof(af.station),"BBC Radio 1");copystr(af.freq,sizeof(af.freq),"98.8");rebuild_source();settle();snapshot("driving-radio");
 fixture();screen=SCR_MAP;gps.fix=0;settle();snapshot("gps-lost");
 fixture();screen=SCR_MAP;route_status=R_CALC;settle();snapshot("calculating");
 fixture();screen=SCR_MAP;route_status=R_IDLE;trip.active=0;settle();snapshot("driving-no-route");
 puts("Native menu/map scenes passed in both themes.");
}
static void command_is(const char*expected){FILE*f=fopen(BT_CMD_PATH,"r");assert(f);char b[96];assert(fgets(b,sizeof(b),f));fclose(f);assert(strstr(b,expected));}
static void remote_tests(void){
 fixture();screen=SCR_MAP;for(int i=0;i<3;i++){menu_open(P_HOME,i,hud_now_ms());key_handle(K_ENTER);int pages[]={P_TRIP,P_PHONE,P_SETTINGS};assert(sheet_page==pages[i]);key_handle(K_MAP);assert(screen==SCR_MAP&&!freecam);}
 fixture();screen=SCR_MAP;key_handle(K_BACK);assert(bt_control_mode);key_handle(K_RIGHT);command_is(" NEXT");key_handle(K_LEFT);command_is(" PREV");key_handle(K_ENTER);command_is(" PAUSE");key_handle(K_BACK);assert(!bt_control_mode);
 menu_open(P_TRIP,0,hud_now_ms());key_handle(K_RIGHT);for(int i=0;i<30;i++)key_handle(K_DOWN);assert(sheet_focus==24);key_handle(K_ENTER);assert(sheet_page==P_PLACE);key_handle(K_BACK);key_handle(K_RIGHT);assert(places_tab==1);key_handle(K_ENTER);assert(sheet_page==P_PLACE);key_handle(K_BACK);key_handle(K_MAP);
 fixture();screen=SCR_MAP;hud_capture_clock=1;hud_capture_ms=s_now_ms();key_handle(K_MAP);assert(freecam&&freecam_transition_active());for(int i=0;i<5;i++){explore_sample(100);draw();char name[64];snprintf(name,sizeof(name),"explore-enter-%02d",i);snapshot(name);}assert(!freecam_transition_active()&&map_tilt_q8==0);key_handle(K_MAP);for(int i=0;i<6;i++){explore_sample(100);draw();char name[64];snprintf(name,sizeof(name),"explore-leave-%02d",i);snapshot(name);}assert(!freecam_transition_active()&&map_tilt_q8==256);
 puts("Native remote workflows: overlay menus, saved places, Bluetooth transport and bidirectional Explore passed.");
}
static void live_routing_test(void){
 fixture();route_cancel();assert(load_route_data());assert(route_start(5497370,-161331));u32 deadline=s_now_ms()+30000u;
 while(route_status==R_CALC&&time_before_u32(s_now_ms(),deadline))route_service_until(s_now_ms()+12u);
 assert(route_status==R_READY&&route_n>2&&route_path_m()>100&&route_result_cost>0);
 printf("Actual C routing: status=READY, points=%d, distance=%u m, cost=%u ds, QAD=%d\n",route_n,route_path_m(),route_result_cost,qadjmode);
 screen=SCR_MAP;update_road_context();settle();snapshot("actual-c-route");route_cancel();route_geom_unmap();qadj_unmap();if(routefd>=0)close(routefd);if(qadjfd>=0)close(qadjfd);
}
int main(int argc,char**argv){setbuf(stdout,0);assert(argc==3);outdir=argv[2];assert(load_font());
#if CLIO_UNICODE
assert(cliou_load(CLIO_HOST_ROOT "clio843/maps/fonts.cpf"));
#endif
assert(load_regions());assert(load_map());scenes();remote_tests();live_routing_test();close(mapfd);return 0;}
