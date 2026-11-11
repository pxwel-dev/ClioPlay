/* Regression: translated page names, shared BT badge and camera strips. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>

static void write_fixture(const char*path,const char*text){
 FILE*f=fopen(path,"w");assert(f);
 assert(fputs(text,f)>=0&&fclose(f)==0);
}

static void test_clock_temperature(void){
 hud_reset_animations();pref.units=1;gps.hour=12;
 write_fixture(CAR_PATH,"source=radio\ntemp=999\n");poll_car();assert(!car.tempok);
 struct HudTween*clock=&hud_tween[HC_CLOCK];
 hud_clock_prepare(100);assert(clock->target.box.w==68&&!clock->target.detail[0]);
 const char*invalid[]={"temp=--","temp=18bad","temp=-999","not_temp=18","temp=","temp=2147483647"};
 for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++){
  car_read_temperature(invalid[i]);assert(!car.tempok);
 }
 write_fixture(CAR_PATH,"source=radio\ntemp=-7\r\nice=1\n");poll_car();
 assert(car.tempok&&car.temp==-7);
 hud_clock_prepare(500);assert(streq(clock->target.detail,"-7°C"));
 assert(hud_pose(clock,500).w==68&&hud_pose(clock,660).w>68&&hud_pose(clock,660).w<118);
 assert(hud_pose(clock,820).w==118);
 car_read_temperature("temp=0");assert(car.tempok&&car.temp==0);
 car_read_temperature("temp=--");hud_clock_prepare(900);
 assert(hud_pose(clock,1060).w>68&&hud_pose(clock,1220).w==68&&!clock->target.detail[0]);
 gps.hour=-1;hud_clock_prepare(1300);
 assert(hud_pose(clock,1620).a==0);
 car_read_temperature("temp=18");hud_clock_prepare(1700);
 assert(!clock->target.title[0]&&streq(clock->target.detail,"18°C"));
 s_unlink(CAR_PATH);
}

static void test_clock_page_changes(void){
 hud_reset_animations();hud_capture_clock=1;hud_capture_ms=90000;
 pref.units=1;gps.hour=12;gps.min=35;car_read_temperature("temp=18");
 screen=SCR_MAP;modal=MOD_NONE;freecam=0;
 hud_prepare(5,hud_capture_ms,1);
 struct HudTween*clock=&hud_tween[HC_CLOCK];
 u32 boot=hud_capture_ms;
 struct HudBox first=hud_pose(clock,boot);
 assert(first.a==0&&first.y==4);
 for(u32 elapsed=80;elapsed<320;elapsed+=80){
  hud_prepare(5,boot+elapsed,1);
  struct HudBox entering=hud_pose(clock,boot+elapsed);
  assert(entering.a>0&&entering.a<256&&entering.y>=4&&entering.y<=8);
  assert(clock->position_at==boot);
 }
 hud_capture_ms=boot+320;hud_prepare(5,hud_capture_ms,1);
 struct HudBox expected=hud_pose(clock,hud_capture_ms);
 u32 position_at=clock->position_at;
 assert(expected.a==256&&expected.y==8);
 const int pages[]={SCR_MENU,SCR_TRIP,SCR_BT,SCR_SET,SCR_KEY,SCR_MAP};
 for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);i++){
  screen=pages[i];hud_capture_ms+=400;
  for(u32 elapsed=0;elapsed<=320;elapsed+=64){
   hud_prepare(5,hud_capture_ms+elapsed,screen==SCR_MAP);
   assert(hud_box_eq(hud_pose(clock,hud_capture_ms+elapsed),expected));
   assert(clock->position_at==position_at);
  }
 }
}

static void test_ui_coverage(void){
 apply_palette(1);hud_reset_animations();
 u16 background=C_BG;
 for(int selected=0;selected<=1;selected++){
  fill(0,0,W,H,background);ui_choice(20,30,60,22,selected);
  u16 interior=fb[40*W+40],edge=fb[31*W+20];
  assert(interior!=background&&edge!=background&&edge!=interior);
  assert(fb[30*W+20]==background&&fb[30*W+40]==interior);
 }
 fill(0,0,W,H,background);bt_control_mode=1;
 hud_pulse_at(&bt_transport_press,BT_TRANSPORT_PREVIOUS,100,BT_PRESS_MS);
 hud_draw_transport(40,240,60,150,0,256);
 u16 centre=fb[60*W+86],edge=fb[54*W+79];
 assert(centre!=background&&edge!=background&&edge!=centre);
 bt_control_mode=0;
 for(int selected=0;selected<=1;selected++){
  fill(0,0,W,H,0);keyboard_surface(20,30,60,26,selected);
  u16 dark=fb[42*W+48];
  assert(fb[30*W+20]==0);
  fill(0,0,W,H,0xffff);keyboard_surface(20,30,60,26,selected);
  assert(fb[42*W+48]!=dark);
 }
 keyboard_open_fave_name(5497370,-161331);screen=SCR_KEY;
 kbd_focus=0;draw_keyboard();keyboard_move(1,0);draw_keyboard();
 u16 after_move[52*26];
 for(int y=0;y<26;y++)for(int x=0;x<52;x++)after_move[y*52+x]=fb[(115+y)*W+12+x];
 fill(0,0,W,H,0xffff);draw_keyboard();
 for(int y=0;y<26;y++)for(int x=0;x<52;x++)assert(after_move[y*52+x]==fb[(115+y)*W+12+x]);
}

static void test_speed_limit_glass(void){
 static const struct {int speed,flags;const char*text;} limits[]={
  {30,2,"30"},{30,3,"30?"},{0,8,"VAR"},{0,4,"INF"},{0,0,"--"},
 };
 gps.fix=1;gps.mph10=240;roadctx.valid=1;pref.units=2;
 freecam=0;camera_visible=0;
 int sample=(HUD_BOTTOM_Y+9)*W+HUD_BOTTOM_MARGIN+HUD_SIDE_W-30;
 for(unsigned i=0;i<sizeof(limits)/sizeof(limits[0]);i++){
  u16 backing[2];
  roadctx.speed=limits[i].speed;roadctx.speed_flags=limits[i].flags;
  for(int night=0;night<=1;night++){
   apply_palette(night);hud_reset_animations();hud_driver_prepare(1000);
   const struct HudCard*speed=&hud_tween[HC_SPEED].target;
   assert(streq(speed->detail,limits[i].text));
   assert(!speed->tag[0]);
   assert(speed->flags==(i==0));
   fill(0,0,W,H,0);hud_glass(speed->box,0);
   u16 outer=fb[sample];
   fill(0,0,W,H,0);hud_draw_card(HC_SPEED,1320);
   backing[night]=fb[sample];
   assert(backing[night]!=outer);
   fill(0,0,W,H,0xffff);hud_draw_card(HC_SPEED,1320);
   assert(fb[sample]!=backing[night]);
  }
  assert(((backing[0]>>5)&63)>((backing[1]>>5)&63));
 }
}

static void test_status_motion(void){
 hud_reset_animations();screen=SCR_MAP;modal=MOD_NONE;freecam=0;
 route_status=hud_observed_route=R_IDLE;route_dest_idx=0;
 hud_audio.kind=hud_source.kind=hud_navigation.kind=hud_connection.kind=hud_bt_feedback.kind=HUD_NONE;
 copystr(af.band,sizeof(af.band),"CD");audio_source_live=btui.connected=btui.playing=1;
 copystr(btui.title,sizeof(btui.title),"Night Drive");copystr(btui.artist,sizeof(btui.artist),"Demo artist");
 bt_control_mode=0;hud_capture_clock=1;hud_capture_ms=70000;
 set_volume(16);hud_prepare(5,hud_capture_ms,1);
 struct HudTween*status=&hud_tween[HC_STATUS];
 struct HudBox volume=status->target.box;
 hud_capture_ms+=400;key_handle(K_BACK);hud_prepare(5,hud_capture_ms,1);
 u32 enter=hud_capture_ms;
 assert(status->target.flags&HUD_KEYS);
 struct HudBox half=hud_pose(status,enter+160);
 assert(half.h>volume.h&&half.h<status->target.box.h&&half.w>volume.w);
 hud_capture_ms+=160;set_volume(17);hud_prepare(5,hud_capture_ms,1);
 assert(hud_status_layout.at==enter&&streq(status->target.badge,"VOL 17"));
 hud_capture_ms+=400;key_handle(K_BACK);hud_prepare(5,hud_capture_ms,1);
 half=hud_pose(status,hud_capture_ms+160);
 assert(status->target.box.h==volume.h&&half.h>volume.h&&half.h<status->from.h);

 hud_capture_ms+=400;route_status=R_CALC;hud_prepare(5,hud_capture_ms,1);
 u32 start=hud_capture_ms;
 assert(hud_progress_at==start&&(status->target.flags&HUD_PROGRESS));
 int previous_x=-1;
 for(u32 elapsed=440;elapsed<=660;elapsed+=220){
  hud_capture_ms=start+elapsed;hud_prepare(5,hud_capture_ms,1);
  assert(hud_progress_at==start);
  fill(0,0,W,H,0);hud_draw_card(HC_STATUS,hud_capture_ms);
  struct HudBox box=status->target.box;
  int y=box.y+box.h-5,x=box.x+10;
  while(x<box.x+box.w-10&&fb[y*W+x]!=C_CYAN)x++;
  assert(x==box.x+10+(int)(elapsed/22u)&&x>previous_x);
  previous_x=x;
 }
}

static void test_missing_readings(void){
 hud_capture_ms=60000;screen=SCR_MAP;freecam=0;modal=MOD_NONE;route_status=R_IDLE;
 hud_observed_route=R_IDLE;hud_navigation.kind=HUD_NONE;
 hud_reset_animations();
 process_decoded_line("SOURCE_STATUS text=\"--\"");assert(!audio_source_detected());
 set_volume(16);hud_prepare(5,hud_capture_ms,1);
 assert(hud_kind==HUD_NONE&&!bt_controls_available());
 process_decoded_line("SOURCE_STATUS text=\"RADIO FM\"");hud_prepare(5,hud_capture_ms,1);
 assert(hud_kind==HUD_SOURCE&&streq(hud_tween[HC_STATUS].target.tag,"FM"));
 int changed=0;
 process_property(66,(const u8*)"--",2,&changed);assert(!changed);
 process_decoded_line("SOURCE_STATUS text=\"AUDIO VOLUME 17\"");hud_prepare(5,hud_capture_ms,1);
 assert(streq(hud_tween[HC_STATUS].target.badge,"VOL 17"));

 // A missing Bluetooth status must not reuse the last connection's metadata.
 write_fixture(BT_STATUS_PATH,"--\n");
 write_fixture(BT_META_PATH,"DEVICE=Old phone\nCONNECTED=1\nPLAYING=1\n");bt_poll_status();
 process_decoded_line("SOURCE_STATUS text=\"CD\"");key_handle(K_BACK);hud_prepare(5,hud_capture_ms,1);
 assert(!btui.connected&&!bt_control_mode&&hud_kind==HUD_NONE);
 write_fixture(BT_STATUS_PATH,"BT CONNECTED\n");
 write_fixture(BT_META_PATH,"DEVICE=--\nCONNECTED=1\nTITLE=--\nARTIST=--\nALBUM=--\n");
 bt_poll_status();assert(btui.connected&&!hud_connection.kind);
 key_handle(K_BACK);hud_prepare(5,hud_capture_ms,1);
 assert(bt_control_mode&&hud_kind==HUD_MEDIA);
 assert(!hud_tween[HC_STATUS].target.title[0]&&!hud_tween[HC_STATUS].target.detail[0]);
 s_unlink(BT_STATUS_PATH);s_unlink(BT_META_PATH);
}

static void test_menu_states(void){
 hud_capture_clock=1;hud_capture_ms=20000;
 pref=(struct Prefs){50,0,0,1,0,THEME_DAY,0,1,1,0,CLIO_DAY_START,CLIO_NIGHT_START};
 screen=SCR_SET;modal=MOD_NONE;
 hud_audio.kind=hud_source.kind=hud_navigation.kind=hud_connection.kind=hud_bt_feedback.kind=HUD_NONE;
 hud_reset_animations();
 set_focus=0;pref.bright=100;key_handle(K_RIGHT);assert(pref.bright==100);
 key_handle(K_DOWN);assert(set_focus==1);
 pref.bright=50;
 for(int i=0;i<8;i++){
  char b[96],before[96];copystr(before,sizeof(before),setting_value(i,b));
  set_focus=i;hud_capture_ms+=3000;key_handle(K_RIGHT);
  assert(!streq(before,setting_value(i,b)));
  hud_prepare(0,hud_capture_ms,0);assert(hud_kind==HUD_NONE);
 }
 pref.language=0;set_focus=5;key_handle(K_ENTER);assert(pref.clock_auto);
 set_focus=7;key_handle(K_ENTER);assert(pref.language==1);
 pref.language=0;
 screen=SCR_TRIP;
 set_destination("Station",12,34);assert(streq(notice_text,"ROUTE STARTED"));
 assert(trip_add_stop("Museum",56,78)&&streq(notice_text,"STOP ADDED"));
 assert(trip_add_stop("Park",90,12));trip_move_stop(1,-1);assert(streq(notice_text,"Stop moved"));
 trip_remove_stop(0);assert(streq(notice_text,"Stop removed"));
 add_fave_place("Museum",56,78);assert(streq(notice_text,"Favourite saved"));
 rename_fave(0,"Gallery");assert(streq(notice_text,"Favourite renamed"));
 remove_fave(0);assert(streq(notice_text,"Favourite removed"));
 modal=MOD_CANCEL_ROUTE;modal_choice=0;key_handle(K_ENTER);assert(streq(notice_text,"Route cancelled"));
 hud_prepare(0,hud_capture_ms,0);assert(hud_kind==HUD_NONE);
}

static void test_bluetooth_feedback(void){
 hud_capture_ms=50000;screen=SCR_BT;
 gps.hour=12;car_read_temperature("temp=--");
 hud_audio.kind=hud_source.kind=hud_navigation.kind=hud_bt_feedback.kind=hud_connection.kind=HUD_NONE;
 hud_reset_animations();memzero(&btui,sizeof(btui));
 write_fixture(BT_STATUS_PATH,"BT CONNECTED\n");
 write_fixture(BT_META_PATH,"DEVICE=PHONE\nCONNECTED=1\n");
 write_fixture(BT_DEVLIST_PATH,"COUNT=1\nDEV=00:11:22:33:44:55|1|UNKNOWN DEVICE\n");
 bt_poll_status();assert(!hud_connection.kind);
 hud_capture_ms+=500;write_fixture(BT_META_PATH,"DEVICE=Pawel's phone\nCONNECTED=1\n");
 bt_poll_status();assert(streq(hud_connected_name,"Pawel's phone"));
 struct HudTween*status=&hud_tween[HC_STATUS];
 header();assert(streq(status->target.title,"Connected to")&&streq(status->target.detail,"Pawel's phone"));
 assert(hud_pose(status,hud_capture_ms).a==0);
 u32 until=hud_connection.until;
 hud_capture_ms+=500;write_fixture(BT_STATUS_PATH,"BT STREAMING\n");bt_poll_status();
 assert(hud_connection.until==until);
 // A full map card can carry route progress, transport, volume and a named connection.
 screen=SCR_MAP;set_band("CD");hud_source.kind=HUD_NONE;bt_control_mode=1;
 copystr(btui.title,sizeof(btui.title),"Example track");
 route_status=R_CALC;set_volume(18);hud_prepare(5,hud_capture_ms,1);
 assert(status->target.flags==(HUD_KEYS|HUD_PROGRESS)&&streq(status->target.badge,"VOL 18"));
 assert(streq(status->target.detail,"Example track")&&streq(status->target.connection,"Connected to Pawel's phone"));
 assert(status->target.box.h>MENU_BODY_Y);
 // Opening a menu resets the tall map pose. The compact card retains both live notices and volume.
 screen=SCR_SET;hud_pulse(&hud_source,HUD_SOURCE,2300);header();
 assert((status->target.flags&HUD_MENU_TWO_LINE)&&streq(status->target.connection,"Connected to Pawel's phone"));
 assert(streq(status->target.badge,"VOL 18"));
 struct HudBox mid=hud_pose(status,hud_capture_ms+160);
 assert(mid.a>0&&mid.y+mid.h<MENU_BODY_Y);
 hud_capture_ms+=320;
 car_read_temperature("temp=--");header();
 struct HudBox without_temp=hud_status_pose(hud_capture_ms);
 car_read_temperature("temp=18");header();
 for(u32 elapsed=0;elapsed<=320;elapsed+=64){
  struct HudBox pill=hud_status_pose(hud_capture_ms+elapsed);
  struct HudBox clock=hud_pose(&hud_tween[HC_CLOCK],hud_capture_ms+elapsed);
  int left=HUD_MENU_TITLE_X+hud_menu_title.w+HUD_MENU_GAP,right=clock.x-HUD_MENU_GAP;
  assert(pill.x>=left&&pill.x+pill.w<=right);
  assert(2*pill.x+pill.w>=left+right-1&&2*pill.x+pill.w<=left+right);
  fill(0,0,W,H,0x1234);hud_draw_all(hud_capture_ms+elapsed,0);
  for(int i=MENU_BODY_Y*W;i<W*H;i++)assert(fb[i]==0x1234);
 }
 struct HudBox with_temp=hud_status_pose(hud_capture_ms+320);
 assert(2*without_temp.x+without_temp.w-(2*with_temp.x+with_temp.w)>=49);
 assert(status->target.box.y+status->target.box.h<MENU_BODY_Y);
 hud_capture_ms=until;hud_prepare(5,hud_capture_ms,1);assert(!status->target.connection[0]);
 screen=SCR_BT;bt_forget_device("00:11:22:33:44:55");bt_poll_status();assert(bt_forget_addr[0]);
 write_fixture(BT_STATUS_PATH,"BT DEVICE FORGOTTEN\n");
 write_fixture(BT_META_PATH,"DEVICE=\nCONNECTED=0\n");
 write_fixture(BT_DEVLIST_PATH,"COUNT=0\n");bt_poll_status();
 assert(!bt_forget_addr[0]&&streq(hud_bt_text,"Device forgotten"));
 // A nameless device gets a generic notice, then updates once its name resolves.
 hud_capture_ms+=100;
 write_fixture(BT_STATUS_PATH,"BT CONNECTED\n");
 write_fixture(BT_META_PATH,"DEVICE=PHONE\nCONNECTED=1\n");bt_poll_status();
 assert(!hud_connection.kind);hud_capture_ms+=1000;bt_poll_status();
 assert(hud_connection.kind&&!hud_connected_name[0]);
 write_fixture(BT_META_PATH,"DEVICE=Another phone\nCONNECTED=1\n");bt_poll_status();
 assert(streq(hud_connected_name,"Another phone"));

 screen=SCR_MAP;freecam=0;modal=MOD_NONE;bt_control_mode=0;
 key_handle(K_BACK);assert(bt_control_mode);
 key_handle(K_LEFT);assert(bt_transport_press.kind==BT_TRANSPORT_PREVIOUS);
 key_handle(K_RIGHT);assert(bt_transport_press.kind==BT_TRANSPORT_NEXT);
 assert(hud_pulse_live(&bt_transport_press,hud_capture_ms+BT_PRESS_MS-1));
 assert(!hud_pulse_live(&bt_transport_press,hud_capture_ms+BT_PRESS_MS));
 key_handle(K_ENTER);assert(bt_transport_press.kind==BT_TRANSPORT_PLAY_PAUSE);
 assert(!btui.playing&&bt_transport_playing(hud_capture_ms));
 bt_poll_status();assert(bt_transport_playing(hud_capture_ms));
 write_fixture(BT_META_PATH,"CONNECTED=1\nPLAYING=1\n");bt_poll_status();
 assert(bt_playback_request<0&&bt_transport_playing(hud_capture_ms));
 key_handle(K_ENTER);assert(!bt_transport_playing(hud_capture_ms));
 assert(bt_transport_playing(hud_capture_ms+BT_PLAYBACK_REPLY_MS));
 write_fixture(BT_META_PATH,"CONNECTED=1\nPLAYING=0\n");bt_poll_status();
 assert(bt_playback_request<0&&!bt_transport_playing(hud_capture_ms+BT_PLAYBACK_REPLY_MS));
 key_handle(K_BACK);assert(!bt_control_mode&&!bt_transport_press.kind);
 s_unlink(BT_STATUS_PATH);s_unlink(BT_META_PATH);s_unlink(BT_DEVLIST_PATH);
}

int main(void){
 pref.language=0;pref.units=2;apply_palette(0);
 assert(streq(bt_label(),"Bluetooth")&&streq(bt_source_label(),"BT"));
 for(int lang=1;lang<UI_LANGUAGE_COUNT;lang++){
  pref.language=lang;assert(streq(bt_label(),ui_tr("Phone connection")));
  assert(streq(bt_source_label(),"BT"));
 }
 // Menu headers have no permanent audio content.
 screen=SCR_SET;copystr(af.band,sizeof(af.band),"CD");audio_source_live=1;btui.connected=1;
 copystr(btui.title,sizeof(btui.title),"Example track");car.tempok=1;gps.hour=12;
 header();assert(!nav_card_w);
 // One capsule with real enter/exit poses and burst coalescing.
 screen=SCR_MAP;route_status=R_IDLE;set_volume(12);u32 t=hud_audio.since;
 struct HudTween*status=&hud_tween[HC_STATUS];
 hud_prepare(5,t,1);assert(hud_kind==HUD_VOLUME&&streq(status->target.title,"VOL 12")&&hud_pose(status,t).a==0);
 struct HudBox half=hud_pose(status,t+160);assert(half.a>0&&half.a<256&&half.y<8);
 hud_prepare(5,t+320,1);assert(hud_pose(status,t+320).a==256&&nav_card_y==8);
 hud_pulse_at(&hud_audio,HUD_VOLUME,t+500,1800);assert(hud_audio.since==t);
 hud_prepare(5,t+2300,1);assert(hud_kind==HUD_NONE&&hud_pose(status,t+2460).a>0&&hud_pose(status,t+2620).a==0);
 // Route progress, media keys and volume can all occupy the same capsule.
 bt_control_mode=1;route_status=R_CALC;hud_pulse_at(&hud_audio,HUD_VOLUME,t+3000,1800);
 hud_prepare(5,t+3100,1);
 assert(hud_kind==HUD_CALC&&status->target.flags==(HUD_KEYS|HUD_PROGRESS)&&streq(status->target.badge,"VOL 12"));
 route_status=R_READY;hud_prepare(5,t+3600,1);
 assert(hud_kind==HUD_MEDIA&&(status->target.flags&HUD_KEYS)&&streq(status->target.title,"Example track")&&streq(status->target.badge,"VOL 12"));
 hud_prepare(5,t+4900,1);assert(hud_kind==HUD_MEDIA&&!status->target.badge[0]);
 bt_control_mode=0;hud_prepare(5,t+8000,1);assert(hud_kind==HUD_NONE);
 hud_audio.kind=HUD_NONE;hud_pulse_at(&hud_audio,HUD_VOLUME,0xffffff00u,1800);
 assert(hud_pulse_live(&hud_audio,0x100u)&&!hud_pulse_live(&hud_audio,0x800u));
 hud_audio.kind=HUD_NONE;
 // The road notice and the camera can arrive and leave independently.
 hud_reset_animations();gps.fix=1;roadctx.valid=1;route_status=R_READY;
 copystr(roadctx.street,sizeof(roadctx.street),"Quayside");
 struct HudTween*road=&hud_tween[HC_ROAD],*name=&hud_tween[HC_ROAD_NAME];trip.active=1;trip.paused=0;
 // New road alone.
 hud_driver_prepare(t+10000);
 assert(road->target.box.w==width("Quayside",1)+2*HUD_ROAD_PAD);
 assert(road->target.box.x==(W-road->target.box.w)/2);
 struct HudBox on_trip=hud_pose(road,t+10160);
 assert(on_trip.x==(W-on_trip.w)/2&&on_trip.a>0);
 assert(hud_pose(name,t+10000).a==0&&hud_pose(name,t+10160).a>0);
 hud_driver_prepare(t+14000);assert(!road->target.box.a&&hud_pose(road,t+14160).a>0&&hud_pose(road,t+14320).a==0);
 // Camera alone on the same road. A new road expands it, then expires.
 camera_visible=1;camera_current.kind=1;camera_current.speed=30;camera_current.flags=2;camera_distance=180;
 hud_driver_prepare(t+15000);
 assert(road->target.kind&&road->target.box.h==29&&!name->target.box.a&&hud_pose(road,t+15160).a>0);
 int camera_w=2*HUD_CAMERA_PAD+HUD_CAMERA_ICON_W+2*HUD_CAMERA_GAP+
  width(road->target.title,0)+width(road->target.badge,0);
 assert(road->target.box.w==camera_w);
 copystr(roadctx.street,sizeof(roadctx.street),"Sandgate");hud_driver_prepare(t+16000);
 assert(road->target.box.h==43&&hud_pose(road,t+16160).h>29&&hud_pose(road,t+16160).h<43);
 assert(road->target.box.x==(W-road->target.box.w)/2&&road->target.box.w==camera_w);
 assert(hud_pose(name,t+16000).a==0&&hud_pose(name,t+16160).a>0&&streq(name->target.title,"Sandgate"));
 hud_driver_prepare(t+20000);
 assert(road->target.kind&&road->target.box.h==29&&!name->target.box.a);
 assert(hud_pose(road,t+20160).h>29&&hud_pose(name,t+20160).a>0&&hud_pose(name,t+20320).a==0);
 camera_visible=0;hud_driver_prepare(t+21000);
 assert(!road->target.box.a&&!name->target.box.a&&hud_pose(road,t+21160).a>0);
 // Without guidance, a camera joins the same centred pill as the road name.
 route_status=R_IDLE;trip.active=0;nav_man_type=MAN_NONE;
 copystr(roadctx.street,sizeof(roadctx.street),"City Road");hud_driver_prepare(t+22000);
 assert(!road->target.kind&&road->target.box.x==(W-road->target.box.w)/2);
 struct HudBox off_trip=hud_pose(road,t+22160);
 assert(hud_pose(road,t+22000).x==road->target.box.x);
 assert(off_trip.x==road->target.box.x&&off_trip.a>0&&off_trip.a<256);
 camera_visible=1;hud_driver_prepare(t+23000);
 assert(road->target.kind&&road->target.box.h==43&&road->target.box.x==(W-road->target.box.w)/2);
 assert(road->target.box.w==camera_w);
 struct HudBox expanding=hud_pose(road,t+23160);
 assert(abs(2*expanding.x+expanding.w-W)<=2&&expanding.h>29&&expanding.h<43);
 hud_driver_prepare(t+25999);assert(name->target.box.a);
 hud_driver_prepare(t+26000);
 assert(!name->target.box.a&&road->target.box.a&&road->target.kind&&road->target.box.h==29);
 // Replacing a still-visible name also animates its text, even at equal width.
 copystr(roadctx.street,sizeof(roadctx.street),"A Road");hud_driver_prepare(t+28000);
 assert(road->target.kind&&road->target.box.h==43);
 copystr(roadctx.street,sizeof(roadctx.street),"B Road");hud_driver_prepare(t+28600);
 int alpha,offset;
 assert(streq(hud_ink_card(name,t+28600,&alpha,&offset)->title,"A Road"));
 const struct HudCard*incoming=hud_ink_card(name,t+28700,&alpha,&offset);
 assert(streq(incoming->title,"B Road")&&alpha<16);
 trip.active=1;route_status=R_READY;hud_driver_prepare(t+29000);
 assert(road->position_at==t+28000&&name->content_at==t+28600);
 assert(road->target.kind&&road->target.box.h==43&&road->target.box.x==(W-road->target.box.w)/2);
 nav_man_type=MAN_RIGHT;nav_man_idx=0;hud_driver_prepare(t+29500);
 struct HudBox speed_box=hud_tween[HC_SPEED].target.box,turn_box=hud_tween[HC_TURN].target.box;
 assert(speed_box.w==turn_box.w&&speed_box.h==turn_box.h&&speed_box.y==turn_box.y);
 assert(speed_box.x+speed_box.w+turn_box.x==W);
 assert(road->target.box.x+road->target.box.w<turn_box.x&&road->position_at==t+28000);
 assert(hud_pose(name,t+29500).a==256);
 copystr(roadctx.street,sizeof(roadctx.street),"Great North Road towards Newcastle upon Tyne");
 hud_driver_prepare(t+30000);
 assert(road->target.box.x==speed_box.x+speed_box.w+HUD_BOTTOM_GAP);
 assert(road->target.box.x+road->target.box.w==turn_box.x-HUD_BOTTOM_GAP);
 camera_visible=0;
 route_status=R_IDLE;route_rerouting=0;trip.active=0;
 t+=10000;
 // Confirmed limit uses raw speed, independent of the display unit. Estimation,
 // variable/unrestricted, and missing data never start a pulse.
 roadctx.speed=30;roadctx.speed_flags=2;gps.mph10=300;assert(!hud_is_overspeed());
 gps.mph10=301;assert(hud_is_overspeed());hud_driver_prepare(t+25000);
 assert(hud_speed_red(t+25000)==0&&hud_speed_red(t+25180)>0&&hud_speed_red(t+25360)==256);
 assert(hud_speed_red(t+26260)<hud_speed_red(t+25360)&&hud_speed_red(t+27160)==256);
 roadctx.speed_flags=3;assert(!hud_is_overspeed());hud_driver_prepare(t+27160);
 assert(hud_speed_red(t+27340)>0&&hud_speed_red(t+27520)==0);
 roadctx.speed_flags=8;assert(!hud_is_overspeed());
 roadctx.speed_flags=4;assert(!hud_is_overspeed());
 roadctx.speed=0;roadctx.speed_flags=2;assert(!hud_is_overspeed());
 // The card interior depends on its backdrop, with untouched outside corners.
 struct HudBox glass=hud_box(20,40,60,32);fill(0,0,W,H,0);
 fill(50,0,W-50,H,0xffff);hud_glass(glass,0);
 assert(fb[40*W+20]==0&&fb[56*W+32]!=fb[56*W+68]&&fb[56*W+32]!=0&&fb[56*W+68]!=0xffff);
 pref.language=0;camera_visible=gps.fix=1;camera_distance=180;
 camera_current.kind=2;camera_current.speed=30;camera_current.flags=2;
 char text[64];camera_strip_text(text,sizeof(text));assert(streq(text,"RED LIGHT  200 yd"));
 gps.mph10=900;u16 ink=camera_alert_colour();assert(ink==C_CYAN);
 camera_current.speed=0;gps.mph10=0;assert(camera_alert_colour()==ink);
 camera_current.kind=1;camera_current.speed=30;camera_strip_text(text,sizeof(text));assert(streq(text,"SPEED CAMERA / 30  200 yd"));
 camera_current.kind=3;camera_strip_text(text,sizeof(text));assert(streq(text,"AVERAGE CHECK / 30  200 yd"));
 camera_average=1;camera_average_speed=48;camera_strip_text(text,sizeof(text));assert(starts(text,"AVG 30 mph / 30"));
 camera_average=0;camera_current.kind=2;pref.units=1;camera_strip_text(text,sizeof(text));assert(streq(text,"RED LIGHT  180 m"));
 test_ui_coverage();test_speed_limit_glass();test_clock_temperature();test_clock_page_changes();test_menu_states();test_bluetooth_feedback();test_missing_readings();
 test_status_motion();
 puts("Glass and status animation, independent road/camera states, speed alerts, temperature validation, menu header bounds and Bluetooth feedback passed.");return 0;
}
