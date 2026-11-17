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
 assert(sheet_page==P_REVIEW&&review_from_place);choose(1);assert(sheet_page==P_COORD);
 key_handle(K_ESC);key_handle(K_BACK);assert(sheet_page==P_REVIEW&&review_from_place&&coordinates_active);
 assert(streq(coordinates.value[0],"54.98000")&&streq(coordinates.name,"Renamed"));
 key_handle(K_BACK);assert(sheet_page==P_PLACES);
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
static void write_trip_fixture(u32 magic,const void*data,size_t size){
 FILE*f=fopen(TRIP_PATH,"wb");assert(f);assert(fwrite(&magic,4,1,f)==1);assert(fwrite(data,size,1,f)==1);assert(!fclose(f));
}
static void long_itinerary(void){
 reset();assert(TRIP_MAX_TARGETS==10);set_destination("Home",100,200);
 for(int i=1;i<TRIP_MAX_TARGETS;i++){char name[32];snprintf(name,sizeof(name),"Stop %d",i);assert(trip_add_stop(name,100+i,200+i));}
 assert(trip.count==10&&streq(trip.target[9].name,"Home"));
 assert(!trip_add_stop("Overflow",0,0));
 menu_open(P_TRIP,0,hud_now_ms());assert(menu_count(P_TRIP)==12);
 for(int i=1;i<=9;i++){key_handle(K_DOWN);assert(sheet_focus==i);}
 key_handle(K_ENTER);assert(sheet_page==P_DEST);key_handle(K_BACK);assert(sheet_focus==9);
 key_handle(K_DOWN);key_handle(K_ENTER);assert(sheet_page==P_TRIP&&sheet_focus==10&&trip.count==10);
 key_handle(K_DOWN);key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&sheet_focus==1);key_handle(K_BACK);
 menu_close(hud_now_ms());phone_dest.valid=1;phone_dest.action='S';phone_destination_apply();assert(!phone_dest.valid&&trip.count==10&&streq(notice_text,ui_tr("Trip full - stop not added")));
 trip_change_destination("New home",500,600);assert(trip.count==10&&streq(trip.target[9].name,"New home"));
 trip_move_stop(8,-1);assert(streq(trip.target[7].name,"Stop 9"));trip_move_stop(7,1);
 trip_remove_stop(8);assert(trip.count==9&&streq(trip.target[8].name,"New home"));assert(trip_add_stop("Replacement",777,888));
 for(int i=0;i<4;i++)trip_continue_after_stop();assert(trip.active_idx==4);
 struct TripState saved=trip;memzero(&trip,sizeof(trip));load_trip();assert(!memcmp(&saved,&trip,sizeof(trip)));
 while(!trip_current_is_destination()){int i=trip.active_idx;trip_continue_after_stop();assert(trip.active_idx==i+1&&trip_current()==&trip.target[i+1]);}
 assert(trip.active_idx==9&&streq(trip_current()->name,"New home"));trip_continue_after_stop();assert(trip.active_idx==9);
 /* Only CTR4 is supported; a rejected file is not converted or resumed. */
 const u32 unsupported[]={0x31525443u,0x32525443u,0x33525443u,0};
 for(unsigned i=0;i<sizeof(unsupported)/sizeof(unsupported[0]);i++){
  write_trip_fixture(unsupported[i],&saved,sizeof(saved));load_trip();assert(!trip.active&&!trip.count);
  FILE*f=fopen(TRIP_PATH,"rb");u32 magic=0;assert(f&&fread(&magic,4,1,f)==1&&magic==unsupported[i]);fclose(f);
 }
 write_trip_fixture(0x34525443u,&saved,12);load_trip();assert(!trip.active&&!trip.count);
 struct TripState bad=saved;bad.count=255;write_trip_fixture(0x34525443u,&bad,sizeof(bad));load_trip();assert(!trip.active&&!trip.count);
 bad=saved;bad.active_idx=10;write_trip_fixture(0x34525443u,&bad,sizeof(bad));load_trip();assert(!trip.active&&!trip.count);
}
static void itinerary_connector_bounds(void){
 reset();load_font();set_destination("Destination",100,200);for(int i=0;i<9;i++)assert(trip_add_stop("Stop",i,i));
 for(int night=0;night<2;night++)for(int h=150;h<=220;h++){
  apply_palette(night);struct HudBox b={12,44,456,h,256};
  int visible=(h-99)/21;if(visible<1)visible=1;int step=(h-99)/visible;if(step>26)step=26;
  int last_y=b.y+71+(visible-1)*step-2+(step-1)/2;
  /* When scrolling shows only intermediate stops, no connector may extend
   * past the final visible marker; footer and the intervening gap stay clear. */
  fill(0,0,W,H,C_BG);trip_content(b,0,16);
  for(int y=last_y+5;y<b.y+b.h-25;y++)assert(fb[y*W+b.x+23]==C_BG);
  if(visible>1){int first_cy=b.y+71-2+(step-1)/2;assert(fb[(first_cy+6)*W+b.x+23]!=C_BG);}
 }
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
static void bounds(void){
 reset();load_font();const int pages[]={P_HOME,P_SETTINGS,P_PHONE,P_TRIP,P_COORD,P_NAME,P_REVIEW,P_CONFIRM};
 for(int night=0;night<2;night++)for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);i++){
  apply_palette(night);menu_open(pages[i],0,hud_now_ms());hud_prepare(5,hud_now_ms(),1);hud_driver_prepare(hud_now_ms());menu_prepare(hud_now_ms());
  struct HudBox b=sheet_to_box;assert(b.x>=0&&b.x+b.w<=W&&b.y>=0&&b.y+b.h<=H);
  if(pages[i]==P_CONFIRM)assert(b.w==280&&b.h==92);
  if(compact_page(pages[i])&&hud_tween[HC_SPEED].target.box.a)assert(b.y+b.h+8<=hud_tween[HC_SPEED].target.box.y);
  menu_settle(hud_now_ms());menu_draw(hud_now_ms());assert(clip_x==0&&clip_y==0&&clip_right==W&&clip_bottom==H&&hud_ink_alpha==16);
 }
}
int main(void){tabs();saved_places();itinerary();long_itinerary();itinerary_connector_bounds();bluetooth();bounds();puts("Menu sheets: three tabs/remembered selections, compact scrolling places, naming/real coordinates, reviewed stops, ten-target persistence and unsupported-format rejection, reorder/remove/cancel, connector bounds, pinned pairing, address-stable phone actions, theme/HUD bounds passed.");}
