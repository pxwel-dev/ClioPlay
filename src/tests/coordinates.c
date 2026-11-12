/* Real remote keys through decimal entry, naming, review and persisted trips. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static void baseline(void){
 coordinates_active=0;memzero(&coordinates,sizeof(coordinates));memzero(&trip,sizeof(trip));
 nrecents=nfaves=0;trip_need_route=trip_route_defer=0;gps.fix=0;screen=SCR_TRIP;
 trip_pane=1;trip_right_focus=0;modal=MOD_NONE;trip_pick_mode=dest_pick_action=phone_wait_action=DEST_PICK_NONE;
 phone_dest.valid=0;pref.language=0;pref_keyboard_language=-1;kbd_picker=0;kbd_action=KBD_NAME_FAVE;
 hud_capture_clock=1;hud_capture_ms=10000;route_status=hud_observed_route=R_IDLE;
 hud_navigation.kind=hud_source.kind=hud_audio.kind=hud_connection.kind=hud_bt_feedback.kind=HUD_NONE;
 hud_reset_animations();s_unlink(RECENT_PATH);s_unlink(FAVES_PATH);s_unlink(TRIP_PATH);
}
static void digits(const char*s){
 while(*s){char c=*s++;assert(screen==SCR_COORDINATES&&coordinates.step!=COORD_REVIEW);
  coordinates.focus=c=='-'?9:c=='.'?11:c=='0'?10:c-'1';key_handle(K_ENTER);
 }
}
static void name(const char*s){assert(screen==SCR_KEY&&kbd_action==KBD_NAME_COORDINATES);while(*s){int cp=utf8_code(&s);keyboard_type(cp);}kbd_focus=38;key_handle(K_ENTER);assert(screen==SCR_COORDINATES&&coordinates.step==COORD_REVIEW);}
static void location(const char*lat,const char*lon,const char*title){digits(lat);key_handle(K_SR);assert(coordinates.step==COORD_LON);digits(lon);key_handle(K_SR);assert(screen==SCR_KEY);name(title);}
static void source(int action){
 screen=SCR_TRIP;modal=MOD_SOURCE_PICK;dest_pick_action=action;modal_choice=0;
 for(int i=0;i<3;i++)key_handle(K_DOWN);key_handle(K_ENTER);
 assert(coordinates_active&&coordinates.action==action&&screen==SCR_COORDINATES);
}
static void parser(void){
 const struct{const char*s;int limit,valid;s32 value;} rows[]={
  {"0",90,1,0},{"-0.0",90,1,0},{"+90.0000000",90,1,9000000},{"-90",90,1,-9000000},
  {"180",180,1,18000000},{"-180.00000",180,1,-18000000},{"54.97548",90,1,5497548},
  {"-1.640505",180,1,-164051},{"1.2345649",90,1,123456},{"1.2345650",90,1,123457},
  {"89.9999999",90,1,9000000},{"-179.9999999",180,1,-18000000},
  {"",90,0,0},{"-",90,0,0},{".",90,0,0},{"1.",90,0,0},{".5",90,0,0},{"90.000001",90,0,0},
  {"-180.0000001",180,0,0},{"91",90,0,0},{"181",180,0,0},{"999999999999999999",180,0,0},
  {"1.23456789",90,0,0},{"1,5",90,0,0},{"1e2",180,0,0},{"1.2.3",90,0,0},{"--1",90,0,0},
  {"12N",90,0,0},{"1 2",90,0,0},{" 1",90,0,0},{"NaN",90,0,0},{"inf",90,0,0},
 };
 for(unsigned i=0;i<sizeof(rows)/sizeof(rows[0]);i++){
  s32 v=777;assert(coordinate_parse(rows[i].s,rows[i].limit,&v)==rows[i].valid);
  if(rows[i].valid)assert(v==rows[i].value);else assert(v==777);
 }
 for(s32 v=-18000000;v<=18000000;v+=9973){char b[20];s32 back;coordinate_format(b,sizeof(b),v);assert(coordinate_parse(b,180,&back)&&back==v);}
}
static void entry_and_cancel(void){
 baseline();key_handle(K_ENTER);assert(modal==MOD_WAIT_PHONE);key_handle(K_BACK);
 key_handle(K_DOWN);key_handle(K_ENTER);assert(screen==SCR_COORDINATES&&coordinates_active);
 key_handle(K_SR);assert(coordinates.error&&coordinates.step==COORD_LAT&&!nrecents);
 digits("90.000001");key_handle(K_SR);assert(coordinates.error&&screen==SCR_COORDINATES);
 coordinates.focus=14;key_handle(K_ENTER);assert(!coordinates.value[0][0]&&!coordinates.error);
 digits("54.97548");key_handle(K_SR);digits("-1.64050");key_handle(K_SR);name("  東京 QUAY  ");
 assert(streq(coordinates.name,"東京 QUAY")&&!nrecents&&!trip.active);
 coordinates.review_focus=1;key_handle(K_ENTER);assert(coordinates.step==COORD_LAT);
 assert(streq(coordinates.value[0],"54.97548")&&streq(coordinates.value[1],"-1.64050"));
 key_handle(K_SR);key_handle(K_SR);assert(streq(kbd_text,"東京 QUAY"));
 key_handle(K_BACK);assert(coordinates.step==COORD_LON&&screen==SCR_COORDINATES);
 key_handle(K_BACK);assert(coordinates.step==COORD_LAT);key_handle(K_BACK);
 assert(screen==SCR_TRIP&&!coordinates_active&&!trip.active&&!nrecents);
 source(DEST_PICK_ADD_STOP);key_handle(K_BACK);assert(modal==MOD_SOURCE_PICK&&modal_choice==3&&!coordinates_active);
 key_handle(K_BACK);coordinates_open(DEST_PICK_NONE);digits("-");key_handle(K_ENTER); /* +/- selected: remove sign */
 assert(!coordinates.value[0][0]);coordinates.focus=11;key_handle(K_ENTER);assert(streq(coordinates.value[0],"0."));
 key_handle(K_ENTER);assert(streq(coordinates.value[0],"0."));key_handle(K_ESC);assert(streq(coordinates.value[0],"0"));
 key_handle(K_MENU);assert(!coordinates_active&&screen==SCR_MENU&&!nrecents);
 coordinates_open(DEST_PICK_NONE);location("0","0","");key_handle(K_MAP);assert(screen==SCR_MAP&&!coordinates_active&&!nrecents);
}
static void soft_keys(void){
 baseline();coordinates_open(DEST_PICK_NONE);digits("54.97548");key_handle(K_SR);
 assert(screen==SCR_COORDINATES&&coordinates.step==COORD_LON);
 digits("-1.64050");key_handle(K_ESC);assert(streq(coordinates.value[1],"-1.6405"));
 key_handle(K_SR);assert(screen==SCR_KEY&&streq(hud_page_title(),"Rename"));
 keyboard_type('A');key_handle(K_SR);assert(screen==SCR_KEY&&streq(kbd_text,"A "));
 keyboard_type('B');key_handle(K_ESC);assert(streq(kbd_text,"A "));
 kbd_focus=38;key_handle(K_ENTER);assert(screen==SCR_COORDINATES&&coordinates.step==COORD_REVIEW&&streq(hud_page_title(),"Review"));
}
static void keypad_navigation(void){
 baseline();coordinates_open(DEST_PICK_NONE);assert(coordinates.focus==0);
 key_handle(K_RIGHT);key_handle(K_RIGHT);key_handle(K_RIGHT);assert(coordinates.focus==12);
 key_handle(K_DOWN);assert(coordinates.focus==13);key_handle(K_DOWN);assert(coordinates.focus==14);
 key_handle(K_LEFT);assert(coordinates.focus==8);key_handle(K_DOWN);assert(coordinates.focus==11);
 key_handle(K_LEFT);assert(coordinates.focus==10);key_handle(K_LEFT);assert(coordinates.focus==9);
 for(int i=0;i<3;i++)key_handle(K_UP);assert(coordinates.focus==0);
 coordinates_open(DEST_PICK_NONE);phone_dest.valid=1;phone_dest.action='N';
 phone_dest.lat=5497000;phone_dest.lon=-164000;copystr(phone_dest.name,sizeof(phone_dest.name),"PHONE");
 screen=SCR_MAP;phone_destination_apply();assert(!coordinates_active&&trip.active&&!phone_dest.valid);
}
static void save_and_reuse(void){
 baseline();key_handle(K_DOWN);key_handle(K_ENTER);location("54.97548","-1.64050","QUAYSIDE");
 phone_dest.valid=1;phone_dest.action='N';phone_dest.lat=100;phone_dest.lon=100;copystr(phone_dest.name,sizeof(phone_dest.name),"Incoming phone point");
 phone_destination_apply();assert(!trip.active&&phone_dest.valid&&!nrecents);
 key_handle(K_ENTER);assert(screen==SCR_MAP&&!coordinates_active&&trip.active&&trip.count==1&&!phone_dest.valid);
 assert(trip.target[0].lat==5497548&&trip.target[0].lon==-164050&&streq(trip.target[0].name,"QUAYSIDE"));
 assert(nrecents==1&&streq(recents[0].name,"QUAYSIDE")&&trip_need_route);
 struct SavedPlace stored[RECENT_MAX];assert(read_places(RECENT_PATH,stored,RECENT_MAX)==1&&stored[0].lat==5497548);
 add_fave(&recents[0]);assert(nfaves==1&&faves[0].lon==-164050&&streq(faves[0].name,"QUAYSIDE"));
 memzero(&trip,sizeof(trip));load_trip();assert(trip.active&&trip.count==1&&streq(trip.target[0].name,"QUAYSIDE"));
 /* Choose the real itinerary Add stop action, then Coordinates. */
 screen=SCR_TRIP;trip_pane=1;trip_right_focus=0;key_handle(K_DOWN);key_handle(K_ENTER);
 assert(modal==MOD_SOURCE_PICK&&dest_pick_action==DEST_PICK_ADD_STOP);
 for(int i=0;i<3;i++)key_handle(K_DOWN);key_handle(K_ENTER);location("54.98000","-1.64000","");
 assert(nrecents==1&&trip.count==1);key_handle(K_ENTER);
 assert(screen==SCR_TRIP&&trip_pane==1&&trip.count==2&&trip.target[0].lat==5498000);
 assert(streq(trip.target[0].name,"54.98000, -1.64000")&&streq(trip.target[1].name,"QUAYSIDE")&&nrecents==2);
 /* Replace only the final destination through its real action menu. */
 trip_right_focus=1;key_handle(K_ENTER);assert(modal==MOD_TARGET);key_handle(K_ENTER);
 assert(modal==MOD_SOURCE_PICK&&dest_pick_action==DEST_PICK_CHANGE_DEST);
 for(int i=0;i<3;i++)key_handle(K_DOWN);key_handle(K_ENTER);location("54.99000","-1.65000","GOSFORTH");key_handle(K_ENTER);
 assert(trip.count==2&&trip.target[0].lat==5498000&&trip.target[1].lat==5499000&&streq(trip.target[1].name,"GOSFORTH")&&nrecents==3);
 source(DEST_PICK_CHANGE_DEST);location("54.99000","-1.65000","RENAMED");key_handle(K_ENTER);
 assert(nrecents==3&&streq(recents[0].name,"RENAMED"));
 assert(read_places(RECENT_PATH,stored,RECENT_MAX)==3&&streq(stored[0].name,"RENAMED"));
}
static void failures_and_layout(void){
 baseline();set_destination("HOME",5499000,-165000);source(DEST_PICK_ADD_STOP);location("54.98","-1.64","STOP");
 int before=nrecents;trip.count=TRIP_MAX_TARGETS;key_handle(K_ENTER);
 assert(coordinates_active&&coordinates.step==COORD_REVIEW&&nrecents==before&&trip.count==TRIP_MAX_TARGETS);
 trip.count=0;trip.active=0;key_handle(K_ENTER);assert(coordinates_active&&nrecents==before);
 coordinates.review_focus=3;key_handle(K_ENTER);assert(screen==SCR_TRIP&&!coordinates_active&&nrecents==before);
 source(DEST_PICK_CHANGE_DEST);location("0","0","");key_handle(K_ENTER);assert(coordinates_active&&nrecents==before);
 /* Shared top-row notifications remain within their existing menu bounds. */
 load_font();for(int night=0;night<2;night++)for(int language=0;language<UI_LANGUAGE_COUNT;language++){
  pref.language=language;apply_palette(night);car.tempok=1;car.temp=18;gps.hour=12;gps.min=35;
  for(int step=0;step<=COORD_REVIEW;step++){
   coordinates.step=step;screen=SCR_COORDINATES;hud_menu_title.text=0;hud_prepare(5,hud_capture_ms,0);
   struct HudBox clock=hud_tween[HC_CLOCK].target.box;
   assert(HUD_MENU_TITLE_X+hud_menu_title.w<clock.x);
   draw_coordinates();
  }
 }
}
static void render_dispatch(void){
 baseline();coordinates_open(DEST_PICK_NONE);u16 expected[FB_PIX];
 for(int step=0;step<=COORD_REVIEW;step++){
  coordinates.step=step;render_fb=&display_fb;draw_coordinates();memcpy(expected,fb,sizeof(expected));
  render_runtime_reset(hud_capture_ms);render_background();
  assert(renderer.page==SCR_COORDINATES&&!memcmp(*render_front,expected,sizeof(expected)));
 }
 render_fb=&display_fb;
}
int main(void){parser();entry_and_cancel();soft_keys();keypad_navigation();save_and_reuse();failures_and_layout();render_dispatch();
 s_unlink(RECENT_PATH);s_unlink(FAVES_PATH);s_unlink(TRIP_PATH);
 puts("Coordinates: bounded decimal parsing, real remote entry, editable/cancelled drafts, Unicode/optional names, review, persisted recents/favourites, stops, replacement, phone arbitration and theme/language layouts passed.");return 0;}
