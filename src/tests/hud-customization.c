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
 menu_open(P_PHONE,0,hud_now_ms());assert(sheet_page==P_HOME);
 screen=SCR_MAP;menu_sync(hud_now_ms());key_handle(K_BACK);assert(!bt_control_mode);
 pref.cameras=2;char b[96];assert(streq(setting_value(4,b),ui_tr("Visual")));
 set_focus=4;settings_key(K_RIGHT);assert(pref.cameras<2);
 hardware_file("  export hw_bluetooth='1'\n");
 hardware_load_bluetooth();assert(hw_bluetooth_enabled);
 menu_open(P_HOME,0,hud_now_ms());assert(menu_count(P_HOME)==3);
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
 hud_reset_animations();
 notice_success("Test");hud_prepare(5,hud_capture_ms,1);hud_driver_prepare(hud_capture_ms);
 assert(hud_tween[HC_CLOCK].target.box.x==8);
 assert(hud_tween[HC_STATUS].target.box.x>=W/2);
 assert(hud_tween[HC_SPEED].target.box.x==HUD_TURN_X);
 assert(hud_tween[HC_TURN].target.box.x==HUD_BOTTOM_MARGIN);
 menu_open(P_SETTINGS,10,hud_now_ms());hud_prepare(5,hud_capture_ms,0);
 assert(hud_status_pose(hud_capture_ms).x>=W/2);
 assert(sheet_to_box.y+sheet_to_box.h<=HUD_BOTTOM_Y);

 pref.top_swap=pref.bottom_swap=0;
 screen=SCR_MAP;hud_prepare(5,hud_capture_ms+500,1);hud_driver_prepare(hud_capture_ms+500);
 assert(hud_tween[HC_CLOCK].target.box.x>=HUD_CLOCK_X);
 assert(hud_tween[HC_SPEED].target.box.x==HUD_BOTTOM_MARGIN);
 assert(hud_tween[HC_TURN].target.box.x==HUD_TURN_X);
}
int main(void){test_hardware();test_prefs_and_geometry();
 unlink(CLIO_HW_EXPORT_PATH);unlink(PREF_PATH);
 puts("2.1.6 HUD preferences, persistence, Bluetooth-hardware exclusion and swapped pill geometry passed.");}
