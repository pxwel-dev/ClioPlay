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
static void choose(int row){
 if(sheet_page==P_TRIP&&trip.count&&row>=trip.count){
  for(int i=0;sheet_focus<trip.count&&i<trip.count;i++)key_handle(K_DOWN);
  for(int i=0;sheet_focus!=row&&i<3;i++)key_handle(K_RIGHT);
 }else{
  while(sheet_focus<row)key_handle(K_DOWN);
  while(sheet_focus>row)key_handle(K_UP);
 }
 assert(sheet_focus==row);key_handle(K_ENTER);
}
static void tabs(void){
 reset();for(int i=0;i<15;i++){char name[40];snprintf(name,sizeof(name),"Place %d",i);add_recent(name,100+i*100,200+i*100);add_fave_place(name,100+i*100,200+i*100);}
 /* Heights on either side of a row boundary keep the same pixel motion. */
 for(int height=130,last=0;height>=90;height--){
  struct MenuRows rows=menu_rows(54,height,22,12,7);
  assert(rows.offset==8*22-height&&(height==130||rows.offset==last+1));
  assert(rows.first<=7&&rows.end>7&&clip_y==54&&clip_bottom==54+height);
  int y=rows.top+7*22-rows.offset;
  assert(menu_row_opacity(rows,y,22,16)==16);
  assert(menu_row_opacity(rows,rows.top-11,22,16)==8);
  last=rows.offset;menu_rows_end(rows,450,22,12,16);
  assert(clip_y==0&&clip_bottom==H);
 }
 key_handle(K_SR);key_handle(K_ESC);assert(sheet_page==P_TRIP&&sheet_focus==0);
 key_handle(K_RIGHT);assert(sheet_page==P_PLACES&&!places_tab&&sheet_focus==0);
 key_handle(K_UP);assert(sheet_focus==nrecents-1);key_handle(K_DOWN);assert(sheet_focus==0);
 for(int i=0;i<10;i++)key_handle(K_DOWN);assert(sheet_focus==10);
 key_handle(K_RIGHT);assert(places_tab==1&&sheet_focus==0);key_handle(K_DOWN);
 key_handle(K_RIGHT);assert(sheet_page==P_NEARBY&&sheet_focus==0);key_handle(K_DOWN);assert(sheet_focus==1);
 key_handle(K_RIGHT);assert(sheet_page==P_TRIP);key_handle(K_RIGHT);assert(!places_tab&&sheet_focus==10);
 key_handle(K_LEFT);assert(sheet_page==P_TRIP);key_handle(K_LEFT);assert(sheet_page==P_NEARBY&&sheet_focus==1);
 key_handle(K_LEFT);assert(sheet_page==P_PLACES&&places_tab==1&&sheet_focus==1);
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
 key_handle(K_DOWN);assert(sheet_focus==trip.count);
 key_handle(K_RIGHT);assert(sheet_focus==trip.count+1);
 key_handle(K_RIGHT);assert(sheet_focus==trip.count);
 key_handle(K_LEFT);assert(sheet_focus==trip.count+1);
 key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&sheet_focus==1);key_handle(K_BACK);assert(trip.active&&sheet_focus==trip.count+1);
 key_handle(K_DOWN);assert(sheet_focus==0);key_handle(K_UP);assert(sheet_focus==trip.count);
 key_handle(K_LEFT);key_handle(K_ENTER);key_handle(K_LEFT);key_handle(K_ENTER);assert(!trip.active&&!trip.count);
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
 key_handle(K_DOWN);assert(sheet_focus==11);
 key_handle(K_LEFT);key_handle(K_RIGHT);assert(sheet_focus==11&&trip.count==10);
 key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&sheet_focus==1);key_handle(K_BACK);
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
  int list_top=b.y+69,list_space=h-99,step=21;
  int show=list_space/step,view_h=show*step;
  int view_top=list_top+(list_space-view_h+(view_h<list_space))/2;
  int first_cy=view_top+(step-1)/2,last_cy=first_cy+(show-1)*step;
  /* Visible rows stay centred within the list viewport with equal margins (+/-1 px). */
  int top_gap=view_top-list_top,bot_gap=(list_top+list_space)-(view_top+view_h-1);
  assert(top_gap>=0&&bot_gap>=0&&top_gap-bot_gap>=-1&&top_gap-bot_gap<=1);
  /* Check top, scrolled middle, bottom, and footer-focused states: no vertical
   * connector may ever appear above the top visible marker or below the bottom one. */
  const int selects[]={0,1,show,show+2,trip.count-1,trip.count};
  for(unsigned s=0;s<sizeof(selects)/sizeof(selects[0]);s++){
   int sel=selects[s];
   fill(0,0,W,H,C_BG);trip_content(b,sel,16);
   int top_limit=(sel==0)?view_top:first_cy-4;
   for(int y=list_top;y<top_limit;y++)assert(fb[y*W+b.x+23]==C_BG);
   int bot_start=(sel>=show-1&&sel<trip.count)?view_top+view_h-1:last_cy+5;
   for(int y=bot_start;y<b.y+b.h-25;y++)assert(fb[y*W+b.x+23]==C_BG);
   for(int k=0;k<show-1;k++)assert(fb[(first_cy+k*step+6)*W+b.x+23]!=C_BG);
  }
 }
}
static void bluetooth(void){
 reset();nbtdev=2;btui.connected=1;
 copystr(btdev[0].name,sizeof(btdev[0].name),"One");copystr(btdev[0].addr,18,"00:11:22:33:44:00");btdev[0].connected=0;
 copystr(btdev[1].name,sizeof(btdev[1].name),"Two");copystr(btdev[1].addr,18,"00:11:22:33:44:01");btdev[1].connected=1;
 menu_open(P_PHONE,2,hud_now_ms());key_handle(K_SR);assert(sheet_page==P_PHONE&&sheet_focus==2);
 {FILE*none=fopen(BT_CMD_PATH,"r");assert(!none);}
 key_handle(K_DOWN);assert(sheet_focus==0);key_handle(K_ENTER);assert(sheet_page==P_PAIR);key_handle(K_BACK);assert(sheet_page==P_PHONE&&sheet_focus==0);
 key_handle(K_UP);assert(sheet_focus==2);
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
 /* Heading follows the map course, stays visible when stopped and needs a fix. */
 display_course_valid=0;track_course=-1;gps.fix=1;gps.course=91;gps.mph10=0;assert(gps_panel_heading()==91);
 display_course=200;display_course_valid=1;assert(gps_panel_heading()==200);
 gps.fix=0;assert(gps_panel_heading()<0);gps.fix=1;display_course_valid=0;
 char b[32];pref.units=1;gps_accuracy_text(b,sizeof(b),34);assert(streq(b,"3 m"));
 gps_accuracy_text(b,sizeof(b),2);assert(streq(b,"1 m"));gps_accuracy_text(b,sizeof(b),20000);assert(streq(b,">999 m"));
 gps_altitude_text(b,sizeof(b),1204);assert(streq(b,"120 m"));gps_altitude_text(b,sizeof(b),-4156);assert(streq(b,"-416 m"));
 pref.units=2;gps_accuracy_text(b,sizeof(b),40);assert(streq(b,"13 ft"));gps_altitude_text(b,sizeof(b),1200);assert(streq(b,"394 ft"));pref.units=0;
 pref.hoff=1;pref.moff=0;gps.hour=23;gps.min=59;gps.sec=7;gps_time_text(b,sizeof(b));assert(streq(b,"00:59:07"));pref.hoff=0;
 gps.hour=-1;gps_time_text(b,sizeof(b));assert(streq(b,"--:--:--"));gps.hour=12;gps.min=35;
 /* Degrees and decimal minutes: 2-digit latitude, 3-digit longitude degrees. */
 gps_coordinate_text(b,sizeof(b),4749811,'N','S');assert(streq(b,"47° 29.887' N"));
 gps_coordinate_text(b,sizeof(b),-165638,'E','W');assert(streq(b,"001° 39.383' W"));
 gps_coordinate_text(b,sizeof(b),-5,'N','S');assert(streq(b,"00° 00.003' S"));
 gps_coordinate_text(b,sizeof(b),17999999,'E','W');assert(streq(b,"179° 59.999' E"));
 gps_coordinate_text(b,sizeof(b),0,'E','W');assert(streq(b,"000° 00.000' E"));
 /* Opened directly, BACK or the same action closes it; OK and the knob are inert. */
 gps_panel_toggle(hud_now_ms());assert(sheet_page==P_GPS&&screen==SCR_GPS&&!sheet_depth);
 key_handle(K_ENTER);key_handle(K_DOWN);key_handle(K_ROTCW);assert(sheet_page==P_GPS);
 key_handle(K_BACK);assert(screen==SCR_MAP&&sheet_page==P_CLOSED);
 gps_panel_toggle(hud_now_ms());gps_panel_toggle(hud_now_ms());assert(screen==SCR_MAP&&sheet_page==P_CLOSED);
 /* From Home it is the last row, and BACK returns to Home. */
 key_handle(K_MENU);while(sheet_focus+1<menu_count(P_HOME))key_handle(K_DOWN);key_handle(K_ENTER);
 assert(sheet_page==P_GPS);key_handle(K_BACK);assert(sheet_page==P_HOME);key_handle(K_MAP);assert(screen==SCR_MAP);
}
static void seed_poi_tile(int slot,u8 lvl,s32 tx,s32 ty,const int*kinds,const s32*lats,const s32*lons,const char*const*names,int count){
 struct Tile*t=&tc[slot];memzero(t,sizeof(*t));t->valid=1;t->lvl=lvl;t->tx=tx;t->ty=ty;t->age=++agec;
 u8*p=t->data;p[0]=0;p[1]=0;p[2]=(u8)count;p[3]=(u8)(count>>8);p[4]=p[5]=p[6]=p[7]=0;p+=8;
 int span=span_for_level((int)lvl);
 for(int i=0;i<count;i++){
  u32 lx=(u32)((lons[i]-tx*span)*4095LL/span),ly=(u32)((lats[i]-ty*span)*4095LL/span);
  int tl=(int)slen(names[i]);
  p[0]=(u8)kinds[i];p[1]=46;p[2]=(u8)lx;p[3]=(u8)(lx>>8);p[4]=(u8)ly;p[5]=(u8)(ly>>8);p[6]=(u8)tl;p[7]=0;p+=8;
  memcpy(p,names[i],(size_t)tl);p+=tl;
 }
 t->len=(u32)(p-t->data);
}
static void nearby_pois_workflow(void){
 reset();load_font();atlas_ready=0;memzero(tc,sizeof(tc));
 /* Missing map and missing GPS produce clean empty states. */
 mapok=0;gps.fix=0;track_valid=0;lastfix_valid=0;
 nearby_scan(NEARBY_CAT_FUEL);assert(nearby_count==0&&nearby_status==1);
 mapok=1;mh.minlvl=0;mh.maxlvl=6;mh.minlat=5490000;mh.maxlat=5510000;mh.minlon=-175000;mh.maxlon=-150000;
 nearby_scan(NEARBY_CAT_FUEL);assert(nearby_count==0&&nearby_status==2);
 /* Populate Z5 (inner city) and Z3 (regional) tiles with duplicate + distinct POIs. */
 gps.fix=1;gps.lat=track_lat=5497800;gps.lon=track_lon=-161700;track_valid=1;
 int cx5=tile_div(gps.lon,5),cy5=tile_div(gps.lat,5);
 int cx3=tile_div(gps.lon,3),cy3=tile_div(gps.lat,3);
 /* Fill empty negative entries only implicitly; seed two tiles with POIs. */
 int k5[]={6,8};s32 la5[]={5498000,5497950};s32 lo5[]={-161650,-161500};const char*n5[]={"Shell","Royal Victoria Infirmary"};
 seed_poi_tile(0,5,cx5,cy5,k5,la5,lo5,n5,2);
 /* Z3 has a generic duplicate near Shell (~15 m away) plus a farther Esso and a Parking lot. */
 int k3[]={6,6,7};s32 la3[]={5498010,5499200,5498300};s32 lo3[]={-161650,-161400,-161700};const char*n3[]={"FUEL","Esso","Grainger Car Park"};
 seed_poi_tile(1,3,cx3,cy3,k3,la3,lo3,n3,3);
 /* Fuel scan: Shell (closest, upgraded over generic FUEL dup) then Esso. */
 nearby_scan(NEARBY_CAT_FUEL);
 assert(nearby_count==2&&streq(nearby_pois[0].name,"Shell")&&streq(nearby_pois[1].name,"Esso"));
 assert(nearby_pois[0].dist_m<nearby_pois[1].dist_m);
 /* Hospital scan finds the inner-city Z5 hospital even when absent from Z3. */
 nearby_scan(NEARBY_CAT_HOSPITAL);
 assert(nearby_count==1&&streq(nearby_pois[0].name,"Royal Victoria Infirmary"));
 /* Distance formatting in metric and imperial. */
 char dist[24]="";pref.units=1;append_nearby_distance(dist,sizeof(dist),243);assert(streq(dist,"240 m"));
 dist[0]=0;append_nearby_distance(dist,sizeof(dist),1450);assert(streq(dist,"1.5 km"));
 dist[0]=0;pref.units=2;append_nearby_distance(dist,sizeof(dist),100);assert(streq(dist,"110 yd"));
 dist[0]=0;append_nearby_distance(dist,sizeof(dist),1609);assert(streq(dist,"1.0 mi"));
 pref.units=0;
 /* Full sheet navigation: Trip -> Left -> Nearby -> Fuel -> Shell -> Route as new. */
 key_handle(K_LEFT);assert(sheet_page==P_NEARBY&&sheet_focus==0);
 key_handle(K_ENTER);assert(sheet_page==P_NEARBY_LIST&&nearby_count==2);
 key_handle(K_ENTER);assert(sheet_page==P_PLACE&&place_from_nearby&&menu_count(P_PLACE)==3);
 key_handle(K_ENTER);assert(screen==SCR_MAP&&trip.active&&trip.count==1&&streq(trip.target[0].name,"Shell"));
 /* Add stop via P_SOURCE -> Nearby -> Parking -> Review -> Add stop. */
 menu_open(P_TRIP,trip.count,hud_now_ms());key_handle(K_ENTER);assert(sheet_page==P_SOURCE);
 choose(2);assert(sheet_page==P_NEARBY&&places_picker);
 /* Left/Right in picker mode does not escape into Trip tabs. */
 key_handle(K_LEFT);assert(sheet_page==P_NEARBY&&places_picker);
 choose(1);assert(sheet_page==P_NEARBY_LIST&&nearby_count==1&&streq(nearby_pois[0].name,"Grainger Car Park"));
 key_handle(K_ENTER);assert(sheet_page==P_REVIEW&&review_from_place&&streq(coordinates.name,"Grainger Car Park"));
 key_handle(K_ENTER);assert(sheet_page==P_TRIP&&trip.count==2&&streq(trip.target[0].name,"Grainger Car Park"));
 mapok=0;gps.fix=0;track_valid=0;memzero(tc,sizeof(tc));
}
static void bounds(void){
 reset();load_font();const int pages[]={P_HOME,P_SETTINGS,P_SET_DISPLAY,P_SET_HUD,P_SET_NAV,P_SET_HISTORY,P_SET_SYSTEM,P_PHONE,P_TRIP,P_NEARBY,P_NEARBY_LIST,P_SOURCE,P_COORD,P_NAME,P_REVIEW,P_CONFIRM,P_GPS};
 for(int night=0;night<2;night++)for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);i++){
  apply_palette(night);menu_open(pages[i],0,hud_now_ms());hud_prepare(5,hud_now_ms(),1);hud_driver_prepare(hud_now_ms());menu_prepare(hud_now_ms());
  struct HudBox b=sheet_to_box;assert(b.x>=0&&b.x+b.w<=W&&b.y>=0&&b.y+b.h<=H);
  if(pages[i]==P_CONFIRM)assert(b.w==280&&b.h==92);
  if(compact_page(pages[i])&&hud_tween[HC_SPEED].target.box.a)assert(b.y+b.h+8<=hud_tween[HC_SPEED].target.box.y);
  menu_settle(hud_now_ms());menu_draw(hud_now_ms());assert(clip_x==0&&clip_y==0&&clip_right==W&&clip_bottom==H&&hud_ink_alpha==16);
 }
}
static void confirmation_typography(void){
 reset();load_font();struct HudBox b={100,90,280,92,256};
 static u16 expected[FB_PIX];
 for(int night=0;night<2;night++)for(int kind=0;kind<4;kind++){
  apply_palette(night);confirm_kind=kind;
  fill(0,0,W,H,C_BG);hud_ink_alpha=16;
  box_text(b.x+14,b.y+4,b.w-28,22,CONFIRMATION_FACE,ui_tr(menu_title(P_CONFIRM)),C_WHITE);
  memcpy(expected,fb,sizeof(fb));
  fill(0,0,W,H,C_BG);menu_contents(P_CONFIRM,0,b,16,hud_now_ms());
  for(int y=b.y+4;y<b.y+26;y++)assert(!memcmp(&expected[y*W],&fb[y*W],W*sizeof(u16)));
 }
 const int modals[]={MOD_ROUTE_POINT,MOD_CANCEL_ROUTE,MOD_REMOVE_STOP,MOD_RESUME_TRIP};
 for(unsigned i=0;i<sizeof(modals)/sizeof(modals[0]);i++){
  struct PopupView v;popup_build(&v,modals[i]);assert(v.count==3);
  for(int j=0;j<v.count;j++)assert(v.item[j].face==CONFIRMATION_FACE);
 }
}
/* Traverse every category with the real remote/console handler. */
static void settings_categories(void){
 reset();key_handle(K_MENU);choose(menu_count(P_HOME)-2);assert(sheet_page==P_SETTINGS);
 unsigned seen=0;
 for(int g=0;g<SETTINGS_GROUP_COUNT;g++){
  choose(g);assert(sheet_page==P_SET_DISPLAY+g&&sheet_focus==0&&screen==SCR_SET);
  key_handle(K_UP);assert(sheet_focus==menu_count(sheet_page)-1);
  key_handle(K_DOWN);assert(sheet_focus==0);
  for(int row=0;row<menu_count(sheet_page);row++){
   assert(sheet_focus==row);int id=settings_row(sheet_page,row);
   assert(id>=0&&id<SETTING_COUNT&&!(seen&(1u<<id)));seen|=1u<<id;
   if(row+1<menu_count(sheet_page))key_handle(K_DOWN);
  }
  key_handle(K_BACK);assert(sheet_page==P_SETTINGS&&sheet_focus==g);
 }
 assert(seen==(1u<<SETTING_COUNT)-1);
 choose(3);choose(3);assert(sheet_page==P_CONFIRM&&confirm_kind==2&&sheet_focus==1);
 key_handle(K_ENTER);assert(sheet_page==P_SET_HISTORY&&sheet_focus==3);
 key_handle(K_BACK);assert(sheet_page==P_SETTINGS&&sheet_focus==3);
 choose(4);choose(0);assert(sheet_page==P_LANG&&!language_keyboard);
 key_handle(K_UP);assert(sheet_focus==UI_LANGUAGE_COUNT-1);
 key_handle(K_DOWN);assert(sheet_focus==0);
 key_handle(K_ESC);assert(sheet_page==P_LANG);
 key_handle(K_BACK);assert(sheet_page==P_SET_SYSTEM&&sheet_focus==0);
 key_handle(K_BACK);assert(sheet_page==P_SETTINGS&&sheet_focus==4);
 key_handle(K_MAP);assert(sheet_page==P_CLOSED&&screen==SCR_MAP);
}
static void input_wrap(void){
 reset();key_handle(K_MENU);key_handle(K_UP);assert(sheet_focus==menu_count(P_HOME)-1);
 key_handle(K_DOWN);assert(sheet_focus==0);
 coordinates.step=COORD_REVIEW;menu_open(P_REVIEW,0,hud_now_ms());
 key_handle(K_UP);assert(sheet_focus==3);key_handle(K_DOWN);assert(sheet_focus==0);
 for(int lang=0;lang<UI_LANGUAGE_COUNT;lang++){
  pref_keyboard_language=lang;keyboard_open_fave_name(100,200);menu_open(P_NAME,0,hud_now_ms());
  pk_focus=PK_LANGUAGE;key_handle(K_UP);assert(pk_key_row(pk_focus)==pk_grid_rows()-1&&pk_focus<PK_LANGUAGE);
  key_handle(K_DOWN);assert(pk_focus==PK_LANGUAGE);
  key_handle(K_LEFT);assert(pk_focus==PK_SAVE);key_handle(K_RIGHT);assert(pk_focus==PK_LANGUAGE);
  key_handle(K_ENTER);assert(sheet_page==P_LANG&&language_keyboard);
  key_handle(K_UP);assert(kbd_pick==wrap_focus(lang,-1,UI_LANGUAGE_COUNT));
  key_handle(K_DOWN);assert(kbd_pick==lang);
  key_handle(K_ESC);assert(sheet_page==P_LANG);key_handle(K_BACK);assert(sheet_page==P_NAME);
 }
 pref_keyboard_language=-1;
}
int main(void){input_wrap();tabs();saved_places();nearby_pois_workflow();itinerary();long_itinerary();itinerary_connector_bounds();bluetooth();gps_panel();bounds();confirmation_typography();settings_categories();puts("Menu sheets: four tabs/remembered selections, nearby POIs, compact scrolling places, naming/real coordinates, reviewed stops, ten-target persistence and unsupported-format rejection, reorder/remove/cancel, connector bounds, pinned pairing, address-stable phone actions, GPS panel accuracy/heading/toggle, theme/HUD bounds passed.");}
