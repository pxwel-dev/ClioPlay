/* Production sheet workflows: real keys, persistence, asynchronous updates. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
#ifdef CLIO_CONSOLE_TEST
#include "fixtures/console-input.h"
#endif
static void reset(void){
 hud_capture_clock=1;hud_capture_ms=10000;screen=SCR_MAP;modal=MOD_NONE;pref.language=0;
 mapok=0;gps.fix=0;gps.hour=12;gps.min=35;route_status=hud_observed_route=R_IDLE;
 memset(&trip,0,sizeof(trip));nrecents=nfaves=0;coordinates_active=0;phone_dest.valid=0;
 hud_reset_animations();s_unlink(TRIP_PATH);s_unlink(RECENT_PATH);s_unlink(FAVES_PATH);s_unlink(BT_CMD_PATH);
 key_handle(K_MENU);key_handle(K_ENTER);assert(sheet_page==P_TRIP&&sheet_focus==0);
}
static void choose(int row){while(sheet_focus<row)key_handle(K_DOWN);while(sheet_focus>row)key_handle(K_UP);key_handle(K_ENTER);}
static void tabs(void){
 reset();for(int i=0;i<15;i++){char name[40];snprintf(name,sizeof(name),"Place %d",i);add_recent(name,100+i*100,200+i*100);add_fave_place(name,100+i*100,200+i*100);}
 assert(places_visible((struct HudBox){0,0,456,220,256})==6);
 key_handle(K_SR);assert(sheet_page==P_PLACES&&!places_tab&&sheet_focus==0);
 for(int i=0;i<10;i++)key_handle(K_DOWN);assert(sheet_focus==10);
 key_handle(K_RIGHT);assert(places_tab==1&&sheet_focus==0);key_handle(K_DOWN);
 key_handle(K_SR);assert(sheet_page==P_TRIP);key_handle(K_SR);assert(!places_tab&&sheet_focus==10);
 key_handle(K_ESC);assert(sheet_page==P_TRIP);key_handle(K_ESC);assert(places_tab==1&&sheet_focus==1);
 key_handle(K_MAP);key_handle(K_MENU);key_handle(K_ENTER);assert(sheet_page==P_TRIP);
}
static void saved_places(void){
 reset();add_recent("Museum",5498000,-164000);key_handle(K_RIGHT);key_handle(K_ENTER);
 assert(sheet_page==P_PLACE);choose(2);assert(sheet_page==P_NAME&&kbd_place_lat==5498000&&kbd_place_lon==-164000);
 copystr(kbd_text,sizeof(kbd_text),"My Museum");pk_focus=PK_SAVE;key_handle(K_ENTER);
 assert(sheet_page==P_PLACES&&places_tab==1&&nfaves==1&&faves[0].lat==5498000&&streq(faves[0].name,"My Museum"));
 key_handle(K_ENTER);choose(2);assert(sheet_page==P_NAME);copystr(kbd_text,sizeof(kbd_text),"Renamed");pk_focus=PK_SAVE;key_handle(K_ENTER);
 assert(streq(faves[0].name,"Renamed"));key_handle(K_ENTER);choose(0);
 assert(screen==SCR_MAP&&trip.active&&trip.count==1&&trip.target[0].lat==5498000);
 menu_open(P_TRIP,1,hud_now_ms());key_handle(K_ENTER);assert(sheet_page==P_SOURCE);
 key_handle(K_ENTER);assert(sheet_page==P_PLACES&&places_picker);key_handle(K_ENTER);
 assert(sheet_page==P_REVIEW&&coordinates.action==DEST_PICK_ADD_STOP&&trip.count==1);
 key_handle(K_ENTER);assert(trip.count==2&&sheet_page==P_TRIP&&trip.target[0].lat==5498000);
 /* Draft cancellation does not modify trip; back preserves the source. */
 menu_open(P_TRIP,trip.count,hud_now_ms());key_handle(K_ENTER);choose(1);key_handle(K_ENTER);
 assert(sheet_page==P_REVIEW);key_handle(K_BACK);assert(sheet_page==P_PLACES);
 key_handle(K_BACK);assert(sheet_page==P_SOURCE);key_handle(K_BACK);assert(sheet_page==P_TRIP&&trip.count==2);
}
static void itinerary(void){
 reset();set_destination("Home",100,200);assert(trip_add_stop("First",110,210));assert(trip_add_stop("Second",120,220));
 menu_open(P_TRIP,1,hud_now_ms());key_handle(K_ENTER);assert(sheet_page==P_STOP);choose(0);
 assert(sheet_page==P_TRIP&&streq(trip.target[0].name,"Second"));
 key_handle(K_ENTER);choose(2);assert(sheet_page==P_CONFIRM&&sheet_focus==1);key_handle(K_LEFT);key_handle(K_ENTER);
 assert(trip.count==2&&streq(trip.target[0].name,"First"));
 menu_open(P_TRIP,1,hud_now_ms());key_handle(K_ENTER);assert(sheet_page==P_DEST);choose(0);assert(sheet_page==P_SOURCE&&source_action==DEST_PICK_CHANGE_DEST);
 key_handle(K_BACK);assert(sheet_page==P_DEST);key_handle(K_BACK);assert(sheet_page==P_TRIP);
 choose(trip.count+1);assert(sheet_page==P_CONFIRM&&sheet_focus==1);key_handle(K_ENTER);assert(trip.active);
 choose(trip.count+1);key_handle(K_LEFT);key_handle(K_ENTER);assert(!trip.active&&!trip.count);
}
static void bluetooth(void){
 reset();nbtdev=2;btui.connected=1;
 copystr(btdev[0].name,sizeof(btdev[0].name),"One");copystr(btdev[0].addr,18,"00:11:22:33:44:00");btdev[0].connected=0;
 copystr(btdev[1].name,sizeof(btdev[1].name),"Two");copystr(btdev[1].addr,18,"00:11:22:33:44:01");btdev[1].connected=1;
 menu_open(P_PHONE,2,hud_now_ms());key_handle(K_SR);assert(sheet_page==P_PAIR);key_handle(K_BACK);assert(sheet_page==P_PHONE);
 key_handle(K_ENTER);assert(sheet_page==P_PHONE_ACTIONS);
 struct BtDeviceUi temp=btdev[0];btdev[0]=btdev[1];btdev[1]=temp;menu_sync(hud_now_ms());assert(phone_index==0);
 key_handle(K_ENTER);assert(sheet_page==P_PHONE);
 FILE*f=fopen(BT_CMD_PATH,"r");assert(f);char cmd[128];assert(fgets(cmd,sizeof(cmd),f));fclose(f);assert(strstr(cmd,"DISCONNECT 00:11:22:33:44:01"));
 sheet_focus=1;key_handle(K_ENTER);nbtdev=0;menu_sync(hud_now_ms());assert(sheet_page==P_PHONE);
}
static void nmea_line(const char*s){char b[128];snprintf(b,sizeof(b),"%s",s);parse_nmea(b);}
static void gps_panel(void){
 reset();key_handle(K_MAP);assert(screen==SCR_MAP);
 memset(&gps,0,sizeof(gps));gps.hour=12;gps.min=35;track_course=-1;
 assert(gps_accuracy_dm()<0&&gps_panel_heading()<0);
 nmea_line("$GPRMC,123507,A,4729.887,N,01902.420,E,010.0,091.0,171126,,,A*00");
 nmea_line("$GPGGA,123507,4729.887,N,01902.420,E,1,07,1.4,120.0,M,,M,,*00");
 assert(gps.fix&&gps.sats==7&&gps.hdop10==14&&gps_accuracy_dm()==70&&gps.alt_valid&&gps.alt_dm==1200);
 /* Fresh GST error estimates take precedence over the HDOP estimate. */
 nmea_line("$GPGST,123507,1.2,2.1,1.6,30.0,3.0,4.0,4.0*00");assert(gps.gst_dm==50&&gps_accuracy_dm()==50);
 gps_gst_ms=s_now_ms()-GPS_GST_FRESH_MS-1;assert(gps_accuracy_dm()==70);
 nmea_line("$GPGST,123508,,,,,,,*00");assert(gps_accuracy_dm()==70);
 nmea_line("$GPGGA,123509,,,,,0,00,,,M,,M,,*00");assert(!gps.fix&&!gps.hdop10&&!gps.alt_valid&&gps_accuracy_dm()<0);
 nmea_line("$GPGGA,123510,3130.000,N,03530.000,E,1,05,1.0,-415.6,M,,M,,*00");assert(gps.alt_valid&&gps.alt_dm==-4156);
 nmea_line("$GPGGA,123511,3130.000,N,03530.000,E,1,05,1.0,,M,,M,,*00");assert(gps.fix&&!gps.alt_valid);
 /* Heading is hidden below the course-update speed. */
 gps.fix=1;gps.course=91;gps.mph10=DISPLAY_COURSE_UPDATE_MPH10-1;assert(gps_panel_heading()<0);
 gps.mph10=DISPLAY_COURSE_UPDATE_MPH10;assert(gps_panel_heading()==91);
 char b[32];pref.units=1;gps_accuracy_text(b,sizeof(b),34);assert(streq(b,"3 m"));
 gps_accuracy_text(b,sizeof(b),2);assert(streq(b,"1 m"));gps_accuracy_text(b,sizeof(b),20000);assert(streq(b,">999 m"));
 gps_altitude_text(b,sizeof(b),1204);assert(streq(b,"120 m"));gps_altitude_text(b,sizeof(b),-4156);assert(streq(b,"-416 m"));
 pref.units=2;gps_accuracy_text(b,sizeof(b),40);assert(streq(b,"13 ft"));gps_altitude_text(b,sizeof(b),1200);assert(streq(b,"394 ft"));pref.units=0;
 pref.hoff=1;pref.moff=0;gps.hour=23;gps.min=59;gps.sec=7;gps_time_text(b,sizeof(b));assert(streq(b,"00:59:07"));pref.hoff=0;
 gps.hour=-1;gps_time_text(b,sizeof(b));assert(streq(b,"--:--:--"));gps.hour=12;gps.min=35;
 gps_coordinate_text(b,sizeof(b),4749811,'N','S');assert(streq(b,"47.49811° N"));
 gps_coordinate_text(b,sizeof(b),-165638,'E','W');assert(streq(b,"1.65638° W"));
 gps_coordinate_text(b,sizeof(b),-5,'N','S');assert(streq(b,"0.00005° S"));
 /* Opened directly, BACK or the same action closes it; OK and the knob are inert. */
 gps_panel_toggle(hud_now_ms());assert(sheet_page==P_GPS&&screen==SCR_GPS&&!sheet_depth);
 key_handle(K_ENTER);key_handle(K_DOWN);key_handle(K_ROTCW);assert(sheet_page==P_GPS);
 key_handle(K_BACK);assert(screen==SCR_MAP&&sheet_page==P_CLOSED);
 gps_panel_toggle(hud_now_ms());gps_panel_toggle(hud_now_ms());assert(screen==SCR_MAP&&sheet_page==P_CLOSED);
 /* From Home it is the last row, and BACK returns to Home. */
 key_handle(K_MENU);while(sheet_focus+1<menu_count(P_HOME))key_handle(K_DOWN);key_handle(K_ENTER);
 assert(sheet_page==P_GPS);key_handle(K_BACK);assert(sheet_page==P_HOME);key_handle(K_MAP);assert(screen==SCR_MAP);
}
static void bounds(void){
 reset();load_font();const int pages[]={P_HOME,P_SETTINGS,P_PHONE,P_TRIP,P_COORD,P_NAME,P_REVIEW,P_GPS};
 for(int night=0;night<2;night++)for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);i++){
  apply_palette(night);menu_open(pages[i],0,hud_now_ms());hud_prepare(5,hud_now_ms(),1);hud_driver_prepare(hud_now_ms());menu_prepare(hud_now_ms());
  struct HudBox b=sheet_to_box;assert(b.x>=0&&b.x+b.w<=W&&b.y>=0&&b.y+b.h<=H);
  if(compact_page(pages[i])&&hud_tween[HC_SPEED].target.box.a)assert(b.y+b.h+8<=hud_tween[HC_SPEED].target.box.y);
  menu_settle(hud_now_ms());menu_draw(hud_now_ms());assert(clip_x==0&&clip_y==0&&clip_right==W&&clip_bottom==H&&hud_ink_alpha==16);
 }
}
int main(void){tabs();saved_places();itinerary();bluetooth();gps_panel();bounds();puts("Menu sheets: three tabs/remembered selections, compact scrolling places, naming/real coordinates, reviewed stops, persisted targets, reorder/remove/cancel, pinned pairing, address-stable phone actions, GPS panel accuracy/heading/toggle, theme/HUD bounds passed.");}
