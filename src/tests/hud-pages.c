/* Map-based sheets preserve global HUD contents, timing and clock geometry. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static void prepare(u32 now){hud_capture_ms=now;hud_prepare(5,now,1);hud_driver_prepare(now);menu_prepare(now);menu_draw(now);}
int main(void){
 hud_capture_clock=1;hud_capture_ms=10000;screen=SCR_MAP;modal=MOD_NONE;freecam=0;
 gps.fix=track_valid=1;mapok=0;gps.hour=12;gps.min=35;car.tempok=1;car.temp=18;
 trip.active=1;trip.count=1;route_status=hud_observed_route=R_READY;pref.language=0;
 hud_reset_animations();set_band("FM");hud_source.kind=HUD_NONE;
 prepare(10000);prepare(10400);set_volume(16);prepare(10500);prepare(10900);
 hud_audio.until=20000;
 struct HudTween *status=&hud_tween[HC_STATUS],*clock=&hud_tween[HC_CLOCK];
 u32 term=hud_audio.until,entry=status->position_at,content=status->content_at,clock_at=clock->position_at;
 struct HudBox cb=hud_pose(clock,10900),sb=hud_status_pose(10900);
 hud_capture_ms=11000;key_handle(K_MENU);prepare(11000);assert(menu_animating(11000));
 assert(hud_box_eq(hud_status_pose(11000),sb));
 for(int i=0;i<3;i++){
  hud_capture_ms=11400+(u32)i*400;menu_open(i==0?P_TRIP:i==1?P_PHONE:P_SETTINGS,0,hud_now_ms());prepare(hud_now_ms());
  assert(hud_audio.until==term&&status->position_at==entry&&status->content_at==content);
  assert(clock->position_at==clock_at&&hud_box_eq(hud_pose(clock,hud_now_ms()),cb));
  assert(sheet_to_box.y>=sb.y+sb.h+8);
 }
 hud_capture_ms=12700;key_handle(K_MAP);prepare(12700);prepare(13100);
 assert(!sheet_page&&!menu_animating(13100)&&clock->position_at==clock_at&&hud_audio.until==term);
 /* A multi-row legal status resizes the sheet rather than covering it. */
 hud_capture_ms=14000;route_status=R_CALC;set_volume(18);hud_bt_notice(ui_tr("Pairing phone"));
 menu_open_setting(8,14000);prepare(14000);prepare(14400);
 assert(hud_tween[HC_STATUS].target.box.h<=96&&sheet_to_box.y>=hud_tween[HC_STATUS].target.box.y+hud_tween[HC_STATUS].target.box.h+8);
 assert(hud_menu_animating(14400));
 /* A status expansion must preserve the current page's transition clock
  * and use the same sampled geometry, including when it happens on entry. */
 route_status=hud_observed_route=R_READY;hud_bt_feedback.kind=hud_navigation.kind=HUD_NONE;
 hud_capture_ms=15000;set_volume(20);hud_pulse(&hud_navigation,HUD_JOURNEY,5000);menu_open(P_PLACES,0,15000);prepare(15000);prepare(15400);
 struct HudBox before=menu_pose(15400);
 u32 page_at=menu_at;
 hud_capture_ms=15500;hud_bt_notice(ui_tr("Pairing phone"));prepare(15500);
 assert(menu_at==page_at&&hud_box_eq(before,menu_pose(15500)));
 int last_top=before.y;
 for(u32 t=15500;t<=15820;t++){
  struct HudBox b=menu_pose(t),pill=hud_status_pose(t);
  assert(b.y>=last_top&&b.y-last_top<=1);
  assert(b.y>=pill.y+pill.h+8&&b.y+b.h==before.y+before.h);
  last_top=b.y;
 }
 assert(last_top>before.y&&hud_box_eq(menu_pose(15820),sheet_to_box));
 hud_capture_ms=16000;menu_open(P_TRIP,0,16000);prepare(16000);
 hud_bt_feedback.kind=HUD_NONE;prepare(16100);
 assert(menu_at==16000&&sheet_old_page==P_PLACES);
 prepare(16420);
 assert(hud_box_eq(menu_pose(16420),sheet_to_box)&&menu_pose(16420).y==before.y);
 /* A tall pill's exit gradually releases the reserved area even while
  * its geometric height stays unchanged during the fade. */
 hud_capture_ms=16500;hud_bt_notice(ui_tr("Pairing phone"));prepare(16500);prepare(16820);
 last_top=menu_pose(16820).y;
 hud_bt_feedback.kind=hud_navigation.kind=hud_audio.kind=HUD_NONE;prepare(16900);
 for(u32 t=16900;t<=17220;t++){
  int top=menu_pose(t).y;
  assert(top<=last_top&&last_top-top<=1);
  last_top=top;
 }
 assert(last_top==before.y&&menu_at==16000);
 puts("Map menu HUD: persistent clock/status across entry, subpages and exit, unchanged notice timers, synchronised status expansion and contraction, independent page fades and live calculation passed.");
}
