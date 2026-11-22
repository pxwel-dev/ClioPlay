/* Real remote input: one notice per mode entry, independent of zoom/pan. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
#ifdef CLIO_CONSOLE_TEST
#include "fixtures/console-input.h"
#endif
static struct HudCard*card(void){return &hud_tween[HC_STATUS].target;}
static void prepare(u32 now){hud_capture_ms=now;freecam_orientation_anim_update(now);hud_prepare(active_zoom(),now,1);}
static void baseline(void){
 hud_capture_clock=1;hud_capture_ms=10000;screen=SCR_MAP;modal=MOD_NONE;
 pref.language=pref.debug=0;pref.autozoom=1;pref.units=0;
 gps.fix=track_valid=display_course_valid=mapok=1;gps.hour=12;gps.min=35;
 gps.mph10=240;gps.lat=track_lat=5498200;gps.lon=track_lon=-165638;
 track_course=display_course=70;car.tempok=0;
 mh.minlvl=0;mh.maxlvl=6;mh.flags=0;mh.minlat=mh.minlon=-90000000;mh.maxlat=mh.maxlon=90000000;
 trip.active=1;trip.paused=0;route_status=hud_observed_route=R_READY;
 bt_control_mode=0;btui.connected=0;audio_source_live=1;copystr(af.band,sizeof(af.band),"FM");
 freecam=0;freecam_anim_mode=FC_ANIM_NONE;map_tilt_q8=256;
 manual_zoom=freecam_zoom=5;zoom_override=auto_zoom_level=-1;
 hud_audio.kind=hud_source.kind=hud_navigation.kind=hud_connection.kind=hud_bt_feedback.kind=HUD_NONE;
 hud_reset_animations();prepare(10000);
}
static void point_confirmation(void){
 for(int stop=0;stop<2;stop++)for(int midway=0;midway<2;midway++)for(int temp=0;temp<2;temp++){
  baseline();car.tempok=temp;car.temp=18;prepare(10400);prepare(10800);
  trip.active=stop;trip.count=stop?1:0;trip.active_idx=0;nrecents=0;
  if(stop){trip.target[0].lat=5500000;trip.target[0].lon=-160000;copystr(trip.target[0].name,sizeof(trip.target[0].name),"Home");}
  else route_status=hud_observed_route=R_IDLE;
  struct HudBox clock=hud_pose(&hud_tween[HC_CLOCK],10800);
  u32 position=hud_tween[HC_CLOCK].position_at,content=hud_tween[HC_CLOCK].content_at;
  hud_capture_ms=11000;key_handle(K_MAP);prepare(11000+(midway?125:500));key_handle(K_UP);
  key_handle(K_ENTER);assert(modal==MOD_ROUTE_POINT&&pending_point_stop==stop);
  s32 lat=pending_lat,lon=pending_lon;int tilt=map_tilt_q8,angle=freecam_course;u32 at=hud_capture_ms;
  key_handle(K_ENTER);
  assert(screen==SCR_MAP&&modal==MOD_NONE&&!freecam&&freecam_anim_mode==FC_ANIM_TO_CAR);
  assert(map_tilt_q8==tilt&&freecam_course==angle&&freecam_anim_at==at);
  assert(trip.count==(stop?2:1)&&trip.target[0].lat==lat&&trip.target[0].lon==lon&&nrecents==1);
  if(stop)assert(streq(trip.target[1].name,"Home"));
  assert(hud_navigation.kind!=HUD_EXPLORE);
  route_status=R_CALC;
  for(u32 dt=0;dt<=500;dt+=100){
   prepare(at+dt);
   assert(hud_box_eq(hud_pose(&hud_tween[HC_CLOCK],at+dt),clock));
   assert(hud_tween[HC_CLOCK].position_at==position&&hud_tween[HC_CLOCK].content_at==content);
   assert(map_tilt_q8>=tilt&&map_tilt_q8<=256);
   if(dt&&dt<500)assert(map_tilt_q8>tilt&&map_tilt_q8<256&&freecam_transition_active());
  }
  assert(!freecam_transition_active()&&map_tilt_q8==256&&view_course()==70);
  route_status=R_READY;prepare(at+600);assert(hud_box_eq(hud_pose(&hud_tween[HC_CLOCK],at+600),clock));
 }
 /* Declining the point keeps Explore and never creates a recent or transition. */
 baseline();trip.active=trip.count=0;nrecents=0;key_handle(K_MAP);prepare(10600);
 key_handle(K_ENTER);key_handle(K_RIGHT);key_handle(K_ENTER);
 assert(screen==SCR_MAP&&freecam&&!freecam_transition_active()&&!trip.active&&!nrecents);
}
static void driving_info(void){
 for(int guidance=0;guidance<2;guidance++)for(int named=0;named<2;named++){
  baseline();
  trip.active=guidance;route_status=hud_observed_route=guidance?R_READY:R_IDLE;
  roadctx.valid=1;copystr(roadctx.street,sizeof(roadctx.street),named?"Quayside":"");
  camera_visible=guidance;camera_current.kind=1;camera_distance=180;
  struct HudTween*name=&hud_tween[HC_ROAD_NAME];
  hud_driver_prepare(10000);
  prepare(15000);hud_driver_prepare(15000);
  assert(!name->target.box.a&&!hud_navigation.kind);
  key_handle(K_ENTER);prepare(15000);hud_driver_prepare(15000);
  assert(!!name->target.box.a==named);
  if(named)assert(streq(name->target.title,"Quayside")&&hud_pose(name,15160).a>0);
  assert(hud_navigation.kind==(guidance?HUD_JOURNEY:HUD_NONE));
  if(guidance)assert(card()->kind==HUD_JOURNEY&&streq(card()->tag,"ETA")&&hud_tween[HC_ROAD].target.kind);
  /* Console INFO extends the driving-info display without opening GPS. */
  prepare(16000);key_handle(K_CONSOLE_INFO);prepare(16000);hud_driver_prepare(19999);
  assert(screen==SCR_MAP&&!!name->target.box.a==named);
  hud_driver_prepare(20000);assert(!name->target.box.a);
  assert(!!hud_tween[HC_ROAD].target.box.a==guidance);
  /* Pressing OK twice within 4 s opens the GPS panel; BACK returns to the map. */
  prepare(20500);key_handle(K_ENTER);assert(screen==SCR_MAP);
  prepare(21500);key_handle(K_ENTER);assert(screen==SCR_GPS&&sheet_page==P_GPS&&!sheet_depth);
  key_handle(K_BACK);assert(screen==SCR_MAP&&sheet_page==P_CLOSED);
 }
 camera_clear();roadctx=(struct RoadCtx){0};
}
static void auto_zoom_bands(void){
 /* Default initial bands (Z9-Z4) when auto_zoom_level is unset. */
 const struct {int mph10,zoom;} initial[]={
  {0,9},{49,9},{50,8},{119,8},{120,7},{249,7},{250,6},{449,6},{450,5},{649,5},{650,4},{800,4}
 };
 for(unsigned i=0;i<sizeof(initial)/sizeof(initial[0]);i++){
  baseline();gps.mph10=initial[i].mph10;auto_zoom_level=-1;
  assert(speed_auto_zoom()==initial[i].zoom&&active_zoom()==initial[i].zoom);
 }
 /* Hysteresis when speeding up (Z9->Z4). */
 baseline();gps.mph10=0;auto_zoom_level=-1;assert(speed_auto_zoom()==9);
 gps.mph10=59;assert(speed_auto_zoom()==9);gps.mph10=60;assert(speed_auto_zoom()==8);
 gps.mph10=139;assert(speed_auto_zoom()==8);gps.mph10=140;assert(speed_auto_zoom()==7);
 gps.mph10=269;assert(speed_auto_zoom()==7);gps.mph10=270;assert(speed_auto_zoom()==6);
 gps.mph10=479;assert(speed_auto_zoom()==6);gps.mph10=480;assert(speed_auto_zoom()==5);
 gps.mph10=679;assert(speed_auto_zoom()==5);gps.mph10=680;assert(speed_auto_zoom()==4);
 /* Hysteresis when slowing down (Z4->Z9). */
 gps.mph10=621;assert(speed_auto_zoom()==4);gps.mph10=620;assert(speed_auto_zoom()==5);
 gps.mph10=421;assert(speed_auto_zoom()==5);gps.mph10=420;assert(speed_auto_zoom()==6);
 gps.mph10=231;assert(speed_auto_zoom()==6);gps.mph10=230;assert(speed_auto_zoom()==7);
 gps.mph10=101;assert(speed_auto_zoom()==7);gps.mph10=100;assert(speed_auto_zoom()==8);
 gps.mph10=41;assert(speed_auto_zoom()==8);gps.mph10=40;assert(speed_auto_zoom()==9);
 /* Large speed jumps step directly to the matching band in one call. */
 gps.mph10=700;assert(speed_auto_zoom()==4);
 gps.mph10=0;assert(speed_auto_zoom()==9);
 /* Manual driving zoom reaches Z9 on standard Z6 map packs. */
 baseline();pref.autozoom=0;manual_zoom=6;
 for(int i=0;i<5;i++)key_handle(K_SR);
 assert(manual_zoom==MAX_EXPLORE_ZOOM&&active_zoom()==MAX_EXPLORE_ZOOM);
}
int main(void){
 baseline();prepare(10100);key_handle(K_MAP);prepare(10100);
 assert(freecam&&hud_navigation.kind==HUD_EXPLORE&&streq(card()->title,"Explore"));
 u32 term=hud_navigation.until,entrance=hud_tween[HC_STATUS].position_at;
 prepare(10450);pref.debug=1;key_handle(K_SR);prepare(10450);
 assert(freecam_zoom==8&&hud_navigation.until==term&&streq(card()->title,"Explore"));
 assert(hud_tween[HC_STATUS].position_at==entrance);
 for(int i=0;i<20;i++){key_handle(K_SR);prepare(10450);}
 assert(freecam_zoom==MAX_EXPLORE_ZOOM&&hud_navigation.until==term);
 prepare(12100);assert(!card()->box.a);
 for(int i=0;i<20;i++){key_handle(K_ESC);prepare(12100);}
 assert(freecam_zoom==0&&!card()->box.a&&!hud_navigation.kind);
 key_handle(K_UP);prepare(12150);assert(!card()->box.a); /* panning cannot replay entry */
 key_handle(K_MAP);prepare(12200);
 assert(!freecam&&hud_navigation.kind==HUD_AUTO_ZOOM&&streq(card()->title,"Auto zoom"));
 term=hud_navigation.until;prepare(12800);gps.mph10=600;prepare(13000);
 assert(streq(card()->title,"Auto zoom")&&hud_navigation.until==term);
 prepare(14500);assert(!card()->box.a);
 key_handle(K_MAP);prepare(14500);assert(freecam&&streq(card()->title,"Explore"));
 assert(hud_navigation.until==16300); /* a fresh Explore session gets one fresh notice */

 baseline();key_handle(K_UP);prepare(10000);assert(freecam&&hud_navigation.kind==HUD_EXPLORE);
 term=hud_navigation.until;prepare(10600);key_handle(K_DOWN);prepare(10600);
 assert(hud_navigation.until==term); /* directional entry and later pans */
 key_handle(K_MAP);prepare(10600);assert(hud_navigation.kind==HUD_AUTO_ZOOM);
 term=hud_navigation.until;key_handle(K_MAP);prepare(10700);assert(hud_navigation.until==term&&!freecam);

 baseline();gps.fix=track_valid=0;key_handle(K_MAP);assert(!freecam&&!hud_navigation.kind);
 baseline();pref.autozoom=0;key_handle(K_MAP);prepare(10600);key_handle(K_MAP);prepare(10600);
 assert(!pref.autozoom&&hud_navigation.kind==HUD_MANUAL_ZOOM&&!streq(card()->title,"Auto zoom"));
 /* Temporary manual override: MAP restores Auto, and only the next MAP explores. */
 baseline();key_handle(K_SR);prepare(10000);
 assert(pref.autozoom&&zoom_override==8&&!freecam&&streq(card()->title,"Manual zoom"));
 term=hud_navigation.until;entrance=hud_tween[HC_STATUS].position_at;
 prepare(10400);key_handle(K_ESC);prepare(10400);
 assert(zoom_override==7&&streq(card()->title,"Manual zoom")&&hud_navigation.until>term);
 assert(hud_tween[HC_STATUS].position_at==entrance);
 key_handle(K_ENTER);prepare(10400);assert(zoom_override==7&&hud_navigation.kind==HUD_JOURNEY);
 key_handle(K_MAP);prepare(10400);assert(zoom_override==-1&&!freecam&&streq(card()->title,"Auto zoom"));
 key_handle(K_MAP);prepare(10400);assert(freecam&&streq(card()->title,"Explore"));
 /* Arrows still enter Explore directly from an override, clearing that override. */
 baseline();key_handle(K_SR);key_handle(K_UP);prepare(10000);
 assert(freecam&&zoom_override==-1&&streq(card()->title,"Explore"));
 /* Manual setting is a default state: MAP enters Explore without a reset step. */
 baseline();pref.autozoom=0;key_handle(K_SR);prepare(10000);
 assert(!pref.autozoom&&zoom_override==-1&&streq(card()->title,"Manual zoom"));
 key_handle(K_MAP);prepare(10000);assert(freecam);
 /* A clamp with no zoom change cannot invent a temporary override or notice. */
 baseline();gps.mph10=0;auto_zoom_level=MAX_EXPLORE_ZOOM;key_handle(K_SR);prepare(10000);
 assert(zoom_override==-1&&!hud_navigation.kind);
 auto_zoom_bands();point_confirmation();driving_info();
 puts("Selected-point routing: direct animated return, mid-entry reversal, stops, persistent time/temperature and cancellation passed.");
 puts("Zoom notices: one Explore entry, silent pan/zoom and limits, stable Debug text, MAP restores Auto before Explore, OK retains ETA, arrows enter from overrides, manual setting and transition guards passed.");
}
