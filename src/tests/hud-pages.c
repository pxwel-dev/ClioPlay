/* Real page keys, shared composition and bounded-duration frame scheduling. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static struct HudCard*card(void){return &hud_tween[HC_STATUS].target;}
static void prepare(u32 now){hud_capture_ms=now;hud_prepare(5,now,screen==SCR_MAP);}
static void baseline(void){
 hud_capture_clock=1;hud_capture_ms=10000;screen=SCR_MAP;modal=MOD_NONE;freecam=0;
 gps.fix=mapok=track_valid=1;gps.hour=12;gps.min=35;car.tempok=1;car.temp=18;
 mh.minlat=mh.minlon=-90000000;mh.maxlat=mh.maxlon=90000000;
 trip.active=1;route_status=hud_observed_route=R_READY;pref.language=0;
 hud_audio.kind=hud_source.kind=hud_navigation.kind=hud_connection.kind=hud_bt_feedback.kind=HUD_NONE;
 hud_reset_animations();set_band("FM");hud_source.kind=HUD_NONE;btui.connected=1;bt_control_mode=0;
 prepare(10000);prepare(10400);car.volume[0]=0;set_volume(16);hud_audio.until=20000;prepare(10500);prepare(10900);
 assert(card()->kind==HUD_VOLUME);assert(!hud_menu_animating(10900));
}
static void entrance(u32 start,int map){
 const struct HudTween*status=&hud_tween[HC_STATUS];
 assert(hud_pose(status,start).a==0&&hud_pose(status,start).y==2);
 for(u32 dt=100;dt<=300;dt+=100){
  assert(hud_menu_animating(start+dt));prepare(start+dt);
  struct HudBox b=hud_status_pose(start+dt);
  assert(b.a>0&&b.a<256&&b.y>=2&&b.y<=8);
  assert(status->position_at==start);
  if(!map){
   struct HudBox clock=hud_pose(&hud_tween[HC_CLOCK],start+dt);
   int left=HUD_MENU_TITLE_X+hud_menu_title.w+HUD_MENU_GAP,right=clock.x-HUD_MENU_GAP;
   assert(b.x>=left&&b.x+b.w<=right&&abs(2*b.x+b.w-left-right)<=1);
  }else assert(b.x==8);
 }
 assert(hud_menu_animating(start+400)); /* present the final settled frame */
 prepare(start+400);assert(hud_pose(status,start+400).a==256);
}
int main(void){
 baseline();struct HudBox clock=hud_pose(&hud_tween[HC_CLOCK],11000);
 u32 clock_at=hud_tween[HC_CLOCK].position_at;
 hud_capture_ms=11000;key_handle(K_MENU);assert(screen==SCR_MENU&&hud_menu_animating(11000));
 prepare(11000);assert(card()->kind==HUD_VOLUME&&streq(card()->title,"VOL 16"));
 entrance(11000,0);assert(!hud_menu_animating(11400));
 assert(hud_box_eq(hud_pose(&hud_tween[HC_CLOCK],11400),clock)&&hud_tween[HC_CLOCK].position_at==clock_at);
 hud_capture_ms=11500;menu_focus=2;key_handle(K_ENTER);assert(screen==SCR_SET&&hud_menu_animating(11500));
 u32 entrance_at=hud_tween[HC_STATUS].position_at,content_at=hud_tween[HC_STATUS].content_at,until=hud_audio.until;
 prepare(11500);assert(hud_status_pose(11500).a==256);
 assert(hud_tween[HC_STATUS].position_at==entrance_at&&hud_tween[HC_STATUS].content_at==content_at&&hud_audio.until==until);
 assert(!hud_menu_animating(11500));prepare(11900);
 assert(hud_tween[HC_CLOCK].position_at==clock_at);
 hud_capture_ms=12000;key_handle(K_MAP);assert(screen==SCR_MAP);
 prepare(12000);entrance(12000,1);assert(hud_tween[HC_CLOCK].position_at==clock_at);

 /* Page changes mid-entrance continue the original curve, then stay opaque. */
 baseline();hud_capture_ms=11000;key_handle(K_MENU);prepare(11000);prepare(11100);
 entrance_at=hud_tween[HC_STATUS].position_at;content_at=hud_tween[HC_STATUS].content_at;
 menu_focus=2;hud_capture_ms=11200;key_handle(K_ENTER);prepare(11200);
 assert(hud_tween[HC_STATUS].position_at==entrance_at&&hud_tween[HC_STATUS].content_at==content_at);
 assert(hud_status_pose(11200).a==hud_ease(200,320));
 prepare(11400);until=hud_audio.until;
 const int pages[]={SCR_TRIP,SCR_BT,SCR_SET,SCR_KEY,SCR_MENU};
 for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);i++){
  screen=pages[i];prepare(11500+i*100);
  assert(hud_status_pose(hud_capture_ms).a==256&&hud_audio.until==until);
  assert(hud_tween[HC_STATUS].position_at==entrance_at&&hud_tween[HC_STATUS].content_at==content_at);
 }
 /* A queued confirmation also keeps its original visible-time budget. */
 baseline();hud_audio.kind=HUD_NONE;screen=SCR_MENU;hud_capture_ms=11000;notice_success("Saved");prepare(11000);
 for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);i++){
  screen=pages[i];prepare(11400+i*400);
  assert(hud_events[HUD_EVENT_ACTION].left==4200-(hud_capture_ms-11000));
 }
 prepare(15200);assert(!card()->box.a);

 /* Every concurrent legal notice shares the same card fields on either page. */
 baseline();route_status=R_CALC;hud_capture_ms=12000;set_volume(18);
 hud_pulse_at(&hud_source,HUD_SOURCE,12000,5000);copystr(af.station,sizeof(af.station),"RADIO 1");copystr(af.freq,sizeof(af.freq),"99.2");
 copystr(hud_connected_name,sizeof(hud_connected_name),"Demo phone");hud_pulse_at(&hud_connection,HUD_CONNECTION,12000,5000);
 hud_bt_notice(ui_tr("Pairing phone"));prepare(12000);prepare(12400);
 struct HudCard nav=*card();hud_capture_ms=12500;key_handle(K_MENU);prepare(12500);
 assert(card()->kind==nav.kind&&card()->flags==nav.flags&&card()->ink==nav.ink);
 assert(streq(card()->title,nav.title)&&streq(card()->detail,nav.detail)&&streq(card()->connection,nav.connection)&&streq(card()->feedback,nav.feedback)&&streq(card()->badge,nav.badge));
 assert(card()->box.h==nav.box.h&&card()->box.h<=96);entrance(12500,0);
 assert(hud_menu_animating(15000)); /* moving calculation indicator remains live */
 route_status=R_READY;prepare(15000);assert(card()->kind==HUD_JOURNEY||card()->kind==HUD_SOURCE);
 assert(card()->box.a); /* ready/ETA feedback can be displayed while in menus */

 /* Quiet menus return to 5 Hz once their outgoing pill is actually gone. */
 baseline();hud_audio.until=12300;hud_capture_ms=11000;key_handle(K_MENU);prepare(11000);entrance(11000,0);
 prepare(12300);assert(!card()->box.a);assert(hud_menu_animating(12600));prepare(12600);
 assert(hud_menu_animating(12700));prepare(12700);assert(!hud_menu_animating(12700));
 puts("Page status: navigation/menu entrance, persistent submenu state and timers, shared multi-row contents, clock persistence, live route progress and animation-only 10 Hz scheduling passed.");
}
