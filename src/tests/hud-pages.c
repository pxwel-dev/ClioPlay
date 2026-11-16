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
 menu_open(P_SETTINGS,8,14000);prepare(14000);prepare(14400);
 assert(hud_tween[HC_STATUS].target.box.h<=96&&sheet_to_box.y>=hud_tween[HC_STATUS].target.box.y+hud_tween[HC_STATUS].target.box.h+8);
 assert(hud_menu_animating(14400));
 puts("Map menu HUD: persistent clock/status across entry, subpages and exit, unchanged notice timers, expanded-notice clearance and live calculation passed.");
}
