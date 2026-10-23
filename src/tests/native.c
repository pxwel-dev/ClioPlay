/* Real production renderer, models and remote handler, with isolated host I/O. */
#define HOST_TEST 1
#define main unused_device_entry
#ifndef CLIO_SOURCE
#define CLIO_SOURCE "../clio843/src/clio040.c"
#endif
#include CLIO_SOURCE
#undef main
#include "fixtures/route.h"
#include <assert.h>
#include <string.h>
static const char*outdir;
static void snapshot(const char*name){char p[1024];snprintf(p,sizeof(p),"%s/%s.rgb565",outdir,name);FILE*f=fopen(p,"wb");assert(f);assert(fwrite(fb,1,sizeof(fb),f)==sizeof(fb));fclose(f);}
static void settle(void){draw();}
static void fixture(void){
 pref_keyboard_language=-1;kbd_language=kbd_page=kbd_picker=kbd_column=0;kbd_clear_composition();
 gps.fix=track_valid=display_course_valid=1;gps.lat=track_lat=cp_route[0][0];gps.lon=track_lon=cp_route[0][1];gps.course=track_course=display_course=0;gps.mph10=240;gps.hour=21;gps.min=41;gps.day=19;gps.mon=10;gps.year=2026;
 pref.hoff=1;pref.autozoom=0;pref.debug=0;pref.bright=50;manual_zoom=freecam_zoom=4;
 freecam=0;map_tilt_q8=256;freecam_anim_mode=FC_ANIM_NONE;perspective_lut_tilt=-1;
 car.temp=18;car.tempok=1;copystr(car.volume,sizeof(car.volume),"VOL 12");
 audio_source_live=1;copystr(af.band,sizeof(af.band),"CD");btui.connected=btui.playing=btui.paired=1;copystr(btui.title,sizeof(btui.title),"Night Drive");copystr(btui.artist,sizeof(btui.artist),"ClioPlay demo");copystr(btui.device,sizeof(btui.device),"Demo phone");btui.pos=65;btui.dur=180;
 roadctx.valid=1;roadctx.speed=30;roadctx.speed_flags=0;copystr(roadctx.street,sizeof(roadctx.street),"Quayside");
 route_status=R_READY;route_n=CP_ROUTE_N;route_head=0;route_result_m=2643;route_result_cost=2267;
 for(int i=0;i<route_n;i++){route_lat[i]=cp_route[i][0];route_lon[i]=cp_route[i][1];route_node[i]=ROUTE_NOARC;route_seg_phys[i]=i<8?1:2;route_seg_m[i]=i?13:0;route_seg_cost[i]=i?11:0;}
 nav_man_type=MAN_RIGHT;nav_man_m=183;nav_man_idx=8;
 trip.active=1;trip.count=2;trip.active_idx=trip.paused=0;copystr(trip.target[0].name,sizeof(trip.target[0].name),"Grey Street");trip.target[0].lat=5497370;trip.target[0].lon=-161331;copystr(trip.target[1].name,sizeof(trip.target[1].name),"Home");trip.target[1].lat=5501450;trip.target[1].lon=-169420;
 nrecents=25;nfaves=25;
 for(int i=0;i<25;i++){recents[i].lat=5497548+i;recents[i].lon=-164050-i;snprintf(recents[i].name,sizeof(recents[i].name),"Recent place %02d",i+1);faves[i]=recents[i];snprintf(faves[i].name,sizeof(faves[i].name),"Favourite %02d",i+1);}
 copystr(recents[0].name,sizeof(recents[0].name),"Kareem's Grill");copystr(recents[1].name,sizeof(recents[1].name),"Olive & Bean");
 nbtdev=10;for(int i=0;i<10;i++){snprintf(btdev[i].name,sizeof(btdev[i].name),"Saved phone %02d",i+1);snprintf(btdev[i].addr,sizeof(btdev[i].addr),"00:11:22:33:44:%02d",i);btdev[i].connected=i==0;}
 copystr(btdev[0].name,sizeof(btdev[0].name),"Demo phone");
 modal=MOD_NONE;bt_page_dialog=-1;bt_control_mode=0;trip_need_route=0;notice_text[0]=0;
}

static void scenes(void){
 struct Scene{const char*name;int page,night;}scenes[]={
 {"driving-day",SCR_MAP,0},{"driving-night",SCR_MAP,1},{"menu-day",SCR_MENU,0},{"menu-night",SCR_MENU,1},
 {"trip-day",SCR_TRIP,0},{"trip-night",SCR_TRIP,1},{"bluetooth",SCR_BT,1},{"settings",SCR_SET,1},{"keyboard",SCR_KEY,1}};
 for(unsigned i=0;i<sizeof(scenes)/sizeof(scenes[0]);i++){fixture();screen=scenes[i].page;pref.theme=scenes[i].night?THEME_NIGHT:THEME_DAY;trip_tab=trip_pane=trip_focus=0;menu_focus=1;kbd_focus=38;copystr(kbd_text,sizeof(kbd_text),"HOME");settle();snapshot(scenes[i].name);}
 fixture();screen=SCR_TRIP;trip_pane=1;trip_right_focus=1;pref.theme=THEME_NIGHT;settle();snapshot("trip-itinerary");
 trip.count=5;for(int i=2;i<5;i++)trip.target[i]=trip.target[0];settle();snapshot("trip-five-targets");
 trip_pane=0;trip_focus=24;settle();snapshot("trip-last-place");
 fixture();screen=SCR_TRIP;trip.active=0;nrecents=nfaves=0;trip_pane=1;settle();snapshot("trip-empty");
 fixture();screen=SCR_BT;bt_page_focus=bt_page_total()-1;settle();snapshot("bluetooth-last");bt_page_dialog=5;settle();snapshot("bluetooth-dialog");
 fixture();screen=SCR_MAP;bt_control_mode=1;pref.theme=THEME_NIGHT;settle();snapshot("driving-media");
 copystr(btui.title,sizeof(btui.title),"An exceptionally long track title that should never cover the clock");
 copystr(roadctx.street,sizeof(roadctx.street),"An exceptionally long road name for fitting");settle();snapshot("driving-long-text");
 fixture();screen=SCR_MAP;set_band("FM");copystr(af.station,sizeof(af.station),"BBC Radio 1");copystr(af.freq,sizeof(af.freq),"98.8");settle();snapshot("driving-radio");
 u16 header_copy[W*40];memcpy(header_copy,fb,sizeof(header_copy));copystr(btui.title,sizeof(btui.title),"STALE TRACK SHOULD BE HIDDEN");header();assert(!memcmp(header_copy,fb,sizeof(header_copy)));
 set_band("AUX");settle();snapshot("driving-aux");memcpy(header_copy,fb,sizeof(header_copy));btui.title[0]=0;header();assert(!memcmp(header_copy,fb,sizeof(header_copy)));
 fixture();screen=SCR_MAP;gps.fix=0;settle();snapshot("gps-lost");
 fixture();screen=SCR_MAP;route_status=R_CALC;settle();snapshot("calculating");
 fixture();screen=SCR_MAP;route_status=R_IDLE;trip.active=0;settle();snapshot("driving-no-route");
 fixture();screen=SCR_MAP;roadctx.speed=120;gps.mph10=1200;settle();snapshot("speed-120");
 const int modes[]={MOD_ROUTE_POINT,MOD_RECENT,MOD_FAVE,MOD_CANCEL_ROUTE,MOD_TARGET,MOD_REMOVE_STOP,MOD_ARRIVED_STOP,MOD_ARRIVED_DEST,MOD_RESUME_TRIP,MOD_SOURCE_PICK,MOD_WAIT_PHONE};
 for(unsigned i=0;i<sizeof(modes)/sizeof(modes[0]);i++){fixture();screen=SCR_TRIP;modal=modes[i];modal_index=0;modal_choice=0;arrival_modal_deadline=s_now_ms()+5000;settle();char name[64];snprintf(name,sizeof(name),"modal-%d",modal);snapshot(name);}
 puts("Framebuffer scenes passed: all pages, dense lists, 5 targets, empty state, long text, GPS, route calculation and source-specific metadata.");
}
static void command_is(const char*expected){FILE*f=fopen(BT_CMD_PATH,"r");assert(f);char b[96];assert(fgets(b,sizeof(b),f));fclose(f);assert(strstr(b,expected));}
static void remote_tests(void){
 fixture();screen=SCR_MAP;key_handle(K_MENU);assert(screen==SCR_MENU);
 for(int i=0;i<3;i++){screen=SCR_MENU;menu_focus=i;key_handle(K_ENTER);int pages[]={SCR_TRIP,SCR_BT,SCR_SET};assert(screen==pages[i]);key_handle(K_MAP);assert(screen==SCR_MAP&&!freecam);}
 fixture();screen=SCR_MAP;key_handle(K_BACK);assert(bt_control_mode);key_handle(K_RIGHT);command_is(" NEXT");key_handle(K_LEFT);command_is(" PREV");key_handle(K_ENTER);command_is(" PAUSE");key_handle(K_BACK);assert(!bt_control_mode);
 key_handle(K_SR);assert(active_zoom()==5);key_handle(K_ESC);assert(active_zoom()==4);
 pref.autozoom=1;zoom_override=4;key_handle(K_ENTER);assert(zoom_override==-1);
 fixture();screen=SCR_TRIP;trip_pane=trip_tab=trip_focus=0;for(int i=0;i<30;i++)key_handle(K_DOWN);assert(trip_focus==24);key_handle(K_ENTER);assert(modal==MOD_RECENT&&modal_index==24);key_handle(K_BACK);key_handle(K_RIGHT);assert(trip_tab==1&&trip_focus==0);key_handle(K_ENTER);assert(modal==MOD_FAVE);key_handle(K_BACK);
 key_handle(K_SR);assert(trip_pane==1);trip_right_focus=2;key_handle(K_ENTER);assert(modal==MOD_SOURCE_PICK);key_handle(K_DOWN);key_handle(K_DOWN);key_handle(K_ENTER);assert(modal==MOD_WAIT_PHONE&&phone_wait_action==DEST_PICK_ADD_STOP);key_handle(K_BACK);assert(modal==MOD_NONE);
 trip_right_focus=3;key_handle(K_ENTER);assert(modal==MOD_CANCEL_ROUTE&&modal_choice==1);key_handle(K_ENTER);assert(trip.active);key_handle(K_ESC);assert(trip_pane==0);
 fixture();screen=SCR_BT;bt_page_focus=0;bt_page_connected_choice=0;key_handle(K_ENTER);command_is(" DISCONNECT");key_handle(K_RIGHT);key_handle(K_ENTER);command_is(" UNPAIR");bt_page_focus=1;key_handle(K_ENTER);assert(bt_page_dialog>=0);key_handle(K_ENTER);command_is(" CONNECT");bt_page_focus=bt_page_total()-1;key_handle(K_ENTER);command_is(" PAIR");
 fixture();screen=SCR_KEY;keyboard_open_fave_name(5497000,-160000);copystr(kbd_text,sizeof(kbd_text),"HOME");kbd_focus=38;key_handle(K_ESC);assert(!strcmp(kbd_text,"HOM"));nfaves=0;key_handle(K_ENTER);assert(screen==SCR_TRIP&&nfaves==1&&!strcmp(faves[0].name,"HOM"));
 fixture();screen=SCR_MAP;key_handle(K_MAP);assert(freecam&&freecam_transition_active());for(int i=0;i<5;i++){freecam_orientation_anim_step();draw();char name[64];snprintf(name,sizeof(name),"explore-enter-%02d",i);snapshot(name);}assert(!freecam_transition_active()&&map_tilt_q8==0);s32 lon=cam_lon;key_handle(K_RIGHT);assert(cam_lon!=lon);key_handle(K_ENTER);assert(modal==MOD_ROUTE_POINT);key_handle(K_BACK);key_handle(K_MAP);assert(!freecam&&freecam_transition_active());for(int i=0;i<6;i++){freecam_orientation_anim_step();draw();char name[64];snprintf(name,sizeof(name),"explore-leave-%02d",i);snapshot(name);}assert(!freecam_transition_active()&&map_tilt_q8==256);
 fixture();screen=SCR_TRIP;phone_dest.valid=1;phone_dest.action='S';phone_dest.lat=5497548;phone_dest.lon=-164050;copystr(phone_dest.name,sizeof(phone_dest.name),"Kareem's Grill");phone_wait_action=DEST_PICK_ADD_STOP;modal=MOD_WAIT_PHONE;phone_destination_apply();assert(trip.count==3&&!phone_dest.valid&&modal==MOD_NONE&&!strcmp(trip.target[1].name,"Kareem's Grill"));
 fixture();modal=MOD_RESUME_TRIP;screen=SCR_MAP;key_handle(K_MAP);key_handle(K_BACK);assert(modal==MOD_RESUME_TRIP);modal_choice=1;key_handle(K_ENTER);assert(!trip.active&&modal==MOD_NONE);
#ifndef BEFORE
 fixture();trip.active=0;screen=SCR_TRIP;trip_pane=1;key_handle(K_ENTER);assert(modal==MOD_WAIT_PHONE&&phone_wait_action==DEST_PICK_NONE);key_handle(K_BACK);assert(modal==MOD_NONE);
#endif
 puts("Original remote workflows passed: 3-item menu, integrated transport, zoom, saved places, stop/phone/cancel flows, pairing, naming, protected resume and bidirectional Explore animation.");
}
static void live_routing_test(void){
 fixture();route_cancel();assert(load_route_data());assert(route_start(5497370,-161331));u32 deadline=s_now_ms()+30000u;
 while(route_status==R_CALC&&time_before_u32(s_now_ms(),deadline))route_service_until(s_now_ms()+12u);
 assert(route_status==R_READY&&route_n>2&&route_path_m()>100&&route_result_cost>0);
 printf("Actual C routing: status=READY, points=%d, distance=%u m, cost=%u ds, QAD=%d\n",route_n,route_path_m(),route_result_cost,qadjmode);
 screen=SCR_MAP;update_road_context();settle();snapshot("actual-c-route");route_cancel();route_geom_unmap();qadj_unmap();if(routefd>=0)close(routefd);if(qadjfd>=0)close(qadjfd);
}
int main(int argc,char**argv){setbuf(stdout,0);assert(argc==3);outdir=argv[2];assert(load_font());assert(load_map());scenes();remote_tests();live_routing_test();close(mapfd);return 0;}
