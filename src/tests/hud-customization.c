/* 2.1.6: UI preferences, physical-hardware gating and layout geometry. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
#include <sys/stat.h>

static void hardware_file(const char*s){
 mkdir(CLIO_HOST_ROOT "proc",0777);
 mkdir(CLIO_HOST_ROOT "proc/barcelona",0777);
 FILE*f=fopen(CLIO_HW_EXPORT_PATH,"w");assert(f);
 assert(fputs(s,f)>=0);assert(fclose(f)==0);
}
static void test_hardware(void){
 hw_bluetooth_enabled=1;
 hardware_file("# TomTom hardware\nexport hw_bluetooth=0\nhw_remote=1\n");
 hardware_load_bluetooth();assert(!hw_bluetooth_enabled);
 hud_capture_clock=1;hud_capture_ms=10000;pref.language=0;
 screen=SCR_MAP;bt_control_mode=0;btui.connected=1;gps.fix=1;
 set_band("CD");assert(!bt_source_selected()&&!bt_controls_available());
 menu_reset(hud_now_ms());key_handle(K_MENU);assert(sheet_page==P_HOME&&menu_count(P_HOME)==2);
 key_handle(K_DOWN);assert(sheet_focus==1);key_handle(K_ENTER);assert(sheet_page==P_SETTINGS);
 key_handle(K_BACK);assert(sheet_page==P_HOME);
 menu_open(P_PHONE,0,hud_now_ms());assert(sheet_page==P_HOME);
 screen=SCR_MAP;menu_sync(hud_now_ms());key_handle(K_BACK);assert(!bt_control_mode);
 /* GPS panel stays directly reachable without Bluetooth hardware. */
 key_handle(K_CONSOLE_GPS);assert(sheet_page==P_GPS&&screen==SCR_GPS&&!sheet_depth);
 key_handle(K_BACK);assert(screen==SCR_MAP&&sheet_page==P_CLOSED);
 pref.cameras=2;char b[96];assert(streq(setting_value(4,b),ui_tr("Visual")));
 set_focus=4;settings_key(K_RIGHT);assert(pref.cameras<2);
 hardware_file("  export hw_bluetooth='1'\n");
 hardware_load_bluetooth();assert(hw_bluetooth_enabled);
 menu_open(P_HOME,0,hud_now_ms());assert(menu_count(P_HOME)==3);
}
static void test_clock_stays_outside(void){
 /* Mirror the time/temperature pairing when switching top HUD sides. */
 struct HudBox left=hud_box(8,8,118,27);
 struct HudBox right=hud_box(HUD_CLOCK_X,8,118,27);
 struct HudClockLayout a=hud_clock_content_layout(left);
 struct HudClockLayout b=hud_clock_content_layout(right);
 assert(a.time_x<a.divider_x&&a.divider_x<a.temp_x);
 assert(b.temp_x<b.divider_x&&b.divider_x<b.time_x);
 assert(a.time_x==W-b.time_x);
 assert(a.temp_x==W-b.temp_x);
 assert(a.divider_x==W-b.divider_x);
 /* A lone time value remains anchored near the outer edge. */
 assert(hud_clock_content_layout(hud_box(8,8,68,27)).time_x<45);
 assert(hud_clock_content_layout(hud_box(W-76,8,68,27)).time_x>W-45);
}
static void test_speed_stays_outside(void){
 /* Speed faces the outer edge; the speed-limit inset faces the map. */
 struct HudBox left=hud_box(HUD_BOTTOM_MARGIN,HUD_BOTTOM_Y,HUD_SIDE_W,43);
 struct HudBox right=hud_box(HUD_TURN_X,HUD_BOTTOM_Y,HUD_SIDE_W,43);
 struct HudSpeedLayout a=hud_speed_content_layout(left);
 struct HudSpeedLayout b=hud_speed_content_layout(right);
 assert(a.speed_x<a.limit_x+24);
 assert(b.limit_x+24<b.speed_x);
 assert(a.speed_x==W-b.speed_x);
 assert(a.limit_x==W-b.limit_x-48);
 assert(a.limit_x+48<=left.x+left.w);
 assert(b.limit_x>=right.x);
}
static void test_prefs_and_geometry(void){
 mkdir(CLIO_HOST_ROOT "clio843",0777);
 pref.top_swap=pref.bottom_swap=0;
 set_focus=10;settings_key(K_RIGHT);assert(pref.top_swap==1);
 set_focus=11;settings_key(K_LEFT);assert(pref.bottom_swap==1);
 save_prefs();
 pref.top_swap=pref.bottom_swap=0;
 load_prefs();assert(pref.top_swap==1&&pref.bottom_swap==1);
 FILE*f=fopen(PREF_PATH,"r");assert(f);char data[512]={0};assert(fread(data,1,sizeof(data)-1,f)>1);fclose(f);
 assert(strstr(data,"hudtopswap=1")&&strstr(data,"hudbottomswap=1"));
 assert(streq(setting_value(10,data),"Clock left"));
 assert(streq(setting_value(11,data),"Direction left"));

 hud_capture_clock=1;hud_capture_ms=12000;screen=SCR_MAP;freecam=0;
 gps.fix=1;gps.hour=12;gps.min=35;car.tempok=1;car.temp=18;
 trip.active=1;trip.count=1;trip.paused=0;
 route_status=R_READY;nav_man_type=MAN_RIGHT;nav_man_idx=0;nav_man_m=220;
 nav_next_type=MAN_LEFT;nav_next_exit=0;nav_next_m=90;
 hud_reset_animations();
 notice_success("Test");hud_prepare(5,hud_capture_ms,1);hud_driver_prepare(hud_capture_ms);
 assert(hud_tween[HC_CLOCK].target.box.x==8);
 assert(hud_tween[HC_STATUS].target.box.x>=W/2);
 assert(hud_tween[HC_SPEED].target.box.x==HUD_TURN_X);
 assert(hud_tween[HC_TURN].target.box.x==HUD_BOTTOM_MARGIN);
 assert(hud_tween[HC_TURN_NEXT].target.box.a==256);
 assert(hud_tween[HC_TURN_NEXT].target.box.x==HUD_BOTTOM_MARGIN);
 assert(hud_tween[HC_TURN_NEXT].target.box.y+hud_tween[HC_TURN_NEXT].target.box.h+HUD_TURN_NEXT_GAP==HUD_BOTTOM_Y);
 menu_open_setting(10,hud_now_ms());hud_prepare(5,hud_capture_ms,0);hud_driver_prepare(hud_capture_ms);
 assert(hud_tween[HC_TURN_NEXT].target.box.a==0);
 assert(hud_status_pose(hud_capture_ms).x>=W/2);
 assert(sheet_to_box.y+sheet_to_box.h<=HUD_BOTTOM_Y);

 pref.top_swap=pref.bottom_swap=0;
 screen=SCR_MAP;hud_prepare(5,hud_capture_ms+500,1);hud_driver_prepare(hud_capture_ms+500);
 assert(hud_tween[HC_CLOCK].target.box.x>=HUD_CLOCK_X);
 assert(hud_tween[HC_SPEED].target.box.x==HUD_BOTTOM_MARGIN);
 assert(hud_tween[HC_TURN].target.box.x==HUD_TURN_X);
 assert(hud_tween[HC_TURN_NEXT].target.box.x==HUD_TURN_X&&hud_tween[HC_TURN_NEXT].target.box.a==256);
 assert(hud_tween[HC_TURN_NEXT].target.box.h==HUD_TURN_NEXT_H);
 /* Settle compact secondary pill, then expand smoothly for a follow-up roundabout
  * so the 35x35 roundabout icon and exit number fit legibly with locked bottom edge. */
 hud_settle_for_capture(hud_capture_ms+500);
 nav_next_type=MAN_ROUNDABOUT;nav_next_exit=2;nav_next_m=110;
 hud_driver_prepare(hud_capture_ms+520);
 assert(hud_tween[HC_TURN_NEXT].target.box.h==HUD_TURN_NEXT_RBT_H);
 assert(hud_tween[HC_TURN_NEXT].target.box.y+hud_tween[HC_TURN_NEXT].target.box.h+HUD_TURN_NEXT_GAP==HUD_BOTTOM_Y);
 assert(streq(hud_tween[HC_TURN_NEXT].target.detail,"Take exit 2"));
 for(u32 step=0;step<=320;step+=40){
  struct HudBox pose=hud_pose(&hud_tween[HC_TURN_NEXT],hud_capture_ms+520+step);
  assert(pose.y+pose.h==HUD_BOTTOM_Y-HUD_TURN_NEXT_GAP);
 }
 /* Render the expanded roundabout pill and verify both cyan roundabout strokes
  * and white exit-number pixels appear inside the icon area. */
 {
  apply_palette(1);fill(0,0,W,H,C_MAP_BG);
  hud_settle_for_capture(hud_capture_ms+900);
  hud_draw_card(HC_TURN_NEXT,hud_capture_ms+900);
  struct HudBox rb=hud_tween[HC_TURN_NEXT].target.box;
  int cyan_px=0,white_px=0;
  for(int py=rb.y+2;py<rb.y+rb.h-2;py++)for(int px=rb.x+4;px<rb.x+42;px++){
   u16 pix=fb[py*W+px];
   if(pix==C_CYAN)cyan_px++;
   if(pix==C_WHITE)white_px++;
  }
  assert(cyan_px>20&&white_px>4);
 }
 nav_next_type=MAN_LEFT;nav_next_exit=0;nav_next_m=90;
 hud_driver_prepare(hud_capture_ms+920);
 assert(hud_tween[HC_TURN_NEXT].target.box.h==HUD_TURN_NEXT_H);
 /* Render both Day and Night frames with the secondary pill active, and verify
  * enter/exit motion never collides with HC_TURN and draw() refreshes glass cache. */
 for(int night=0;night<2;night++){
  apply_palette(night);fill(0,0,W,H,C_MAP_BG);
  hud_settle_for_capture(hud_capture_ms+950);hud_draw_all(hud_capture_ms+950,1);
 }
 /* Map endpoint markers paint route-coloured rings for both intermediate stops and final destinations. */
 {
  apply_palette(1);fill(0,0,W,H,C_MAP_BG);
  gps.lat=track_lat=5497800;gps.lon=track_lon=-161700;track_valid=1;
  gps.course=track_course=display_course=0;display_course_valid=1;
  freecam=1;cam_lat=5497800;cam_lon=-161700;freecam_zoom=5;map_tilt_q8=0;
  trip.active=1;trip.count=2;trip.active_idx=0;route_status=R_IDLE;
  trip.target[0].lat=5497860;trip.target[0].lon=-161700;
  trip.target[1].lat=5497760;trip.target[1].lon=-161700;
  nlbox=0;npoi=poi_visible=0;
  draw_trip_markers(5);
  int sx0,sy0,sx1,sy1,ring0=0,ring1=0;
  world_to_screen(trip.target[0].lat,trip.target[0].lon,5,&sx0,&sy0);
  world_to_screen(trip.target[1].lat,trip.target[1].lon,5,&sx1,&sy1);
  for(int dy=-4;dy<=4;dy++)for(int dx=-4;dx<=4;dx++){
   if(fb[(sy0+dy)*W+(sx0+dx)]!=C_MAP_BG)ring0++;
   if(fb[(sy1+dy)*W+(sx1+dx)]!=C_MAP_BG)ring1++;
  }
  assert(ring0>10&&ring1>10);
  assert(fb[sy0*W+sx0]==C_MAP_BG); /* intermediate stop has hollow centre */
  assert(fb[sy1*W+sx1]!=C_MAP_BG); /* final destination has filled bullseye dot */
  /* When a stop or destination targets a visible map POI, tint the POI icon in route
   * colour and suppress the redundant overlapping circle dot. */
  fill(0,0,W,H,C_MAP_BG);
  npoi=0;poi_offer_at(sx1,sy1,7,64,trip.target[1].lat,trip.target[1].lon,1);
  poi_reserve();assert(poi_visible==1);
  poi_trip_prepare(5);assert(poi_trip_tinted[0]==1);
  poi_render();
  u16 poi_snap[16*16];
  for(int dy=-8;dy<8;dy++)for(int dx=-8;dx<8;dx++)poi_snap[(dy+8)*16+(dx+8)]=fb[(sy1+dy)*W+(sx1+dx)];
  draw_trip_markers(5);
  int same_after_dot=1,route_px=0;
  for(int dy=-8;dy<8;dy++)for(int dx=-8;dx<8;dx++){
   u16 p=fb[(sy1+dy)*W+(sx1+dx)];
   if(p!=poi_snap[(dy+8)*16+(dx+8)])same_after_dot=0;
   if(p==C_ROUTE)route_px++;
  }
  assert(same_after_dot&&route_px>20);
  npoi=poi_visible=0;memzero(poi_trip_tinted,sizeof(poi_trip_tinted));
  freecam=0;trip.count=1;route_status=R_READY;
 }
 {
  u32 bg_before=hud_glass_background;
  pref.theme=THEME_NIGHT;apply_palette(1);mapok=0;
  hud_capture_ms+=600;draw();
  assert(hud_glass_background!=bg_before);
 }
 /* Secondary pill exit animation stays clear of HC_TURN's top border. */
 nav_next_type=MAN_NONE;nav_next_m=0;
 hud_driver_prepare(hud_capture_ms+1000);
 for(u32 step=0;step<=320;step+=40){
  struct HudBox pose=hud_pose(&hud_tween[HC_TURN_NEXT],hud_capture_ms+1000+step);
  assert(pose.y+pose.h<HUD_BOTTOM_Y);
 }
}
int main(void){test_hardware();test_prefs_and_geometry();test_clock_stays_outside();test_speed_stays_outside();
 unlink(CLIO_HW_EXPORT_PATH);unlink(PREF_PATH);
 puts("2.1.6 HUD preferences, persistence, Bluetooth-hardware exclusion and swapped pill geometry passed.");}
