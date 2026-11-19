#define HOST_TEST 1
#define CLIO_UNICODE 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
#include "locale_cases.h"
static void whole_utf8(const char*s){const unsigned char*p=(const unsigned char*)s;while(*p){unsigned c=*p++,n=0;if(c<128)continue;if(c>=0xc2&&c<=0xdf)n=1;else if(c>=0xe0&&c<=0xef)n=2;else if(c>=0xf0&&c<=0xf4)n=3;else assert(0);while(n--){assert((*p&192)==128);p++;}}}
int main(int argc,char**argv){assert(argc==3&&load_font()&&cliou_load(argv[1]));
 for(int lang=0;lang<UI_LANGUAGE_COUNT;lang++){
  pref.language=lang;assert(ui_language_rtl()==(streq(ui_language_codes[lang],"ar")||streq(ui_language_codes[lang],"he")));assert(cliou_width(ui_language_name(),1)>0);assert(width(ui_language_name(),0)<=180);
  for(int page=SCR_MENU;page<=SCR_KEY;page++){
   screen=page;
   for(int action=KBD_NAME_FAVE;action<=(page==SCR_KEY?KBD_RENAME_FAVE:KBD_NAME_FAVE);action++){
    kbd_action=action;hud_menu_title_prepare();
    whole_utf8(hud_menu_title.first);whole_utf8(hud_menu_title.second);
    assert(width(hud_menu_title.first,hud_menu_title.face)<=hud_menu_title.w);
    assert(width(hud_menu_title.second,hud_menu_title.face)<=hud_menu_title.w);
   }
  }
  assert(width(bt_source_label(),0)<=52);assert(width(bt_label(),3)<=440);assert(streq(bt_source_label(),"BT"));assert(streq(bt_label(),lang?ui_tr("Phone connection"):"Bluetooth"));assert(width(ui_tr("Phone audio"),1)<=180);
  for(int g=0;g<SETTINGS_GROUP_COUNT;g++){const char*t=ui_tr(settings_groups[g].title);assert(width(t,title_face)<=344);assert(width(t,1)<=292);}
  char settings_text[96];pref.top_swap=1;assert(streq(setting_value(10,settings_text),ui_tr("Clock left")));pref.top_swap=0;assert(streq(setting_value(10,settings_text),ui_tr("Status left")));
  pref.bottom_swap=1;assert(streq(setting_value(11,settings_text),ui_tr("Direction left")));pref.bottom_swap=0;assert(streq(setting_value(11,settings_text),ui_tr("Speed left")));
  assert(starts(setting_value(15,settings_text),ui_tr("Journeys: ")));
  assert(streq(place_display_name("MAP POINT"),ui_tr("Map point"))&&streq(place_display_name("PHONE DESTINATION"),ui_tr("Phone destination"))&&streq(place_display_name("Settings"),"Settings"));
  copystr(btui.status,sizeof(btui.status),"BT DAEMON ERROR");pref.debug=0;assert(streq(bt_status_text(),lang?ui_tr("Phone connection unavailable"):"Bluetooth unavailable"));btui.status[0]=0;
  for(unsigned i=0;i<sizeof(locale_cases)/sizeof(locale_cases[0]);i++){assert(streq(ui_tr(locale_cases[i][0]),locale_cases[i][lang]));whole_utf8(locale_cases[i][lang]);assert(cliou_width(locale_cases[i][lang],1)>0);}
  const char*confirm_titles[]={"Add to favourites?","Start suggested trip?","Take diversion?","Cancel route?","Remove stop?","Clear driving history?","Route to point?","Add point as stop?","Cancel current route?","Resume saved trip?"};
  for(unsigned i=0;i<sizeof(confirm_titles)/sizeof(confirm_titles[0]);i++)assert(width(ui_tr(confirm_titles[i]),CONFIRMATION_FACE)<=252);
  assert(width(ui_tr("Yes"),CONFIRMATION_FACE)<=104&&width(ui_tr("No"),CONFIRMATION_FACE)<=104);
  /* The action hint must always fit an available status HUD column. */
  assert(width(ui_tr("BACK to review"),0)<=220&&width(ui_tr("OK to review"),0)<=220);
  assert(width(ui_tr("Faster route available"),0)<=280);
  char traffic_line[112];traffic_saving=3600;diversion_saving_text(traffic_line,sizeof(traffic_line));whole_utf8(traffic_line);assert(width(traffic_line,0)+18<=164);
  for(int media=0;media<2;media++){
   const char*title=ui_tr("Faster route available"),*hint=ui_tr(media?"BACK to review":"OK to review");
   whole_utf8(title);whole_utf8(hint);
   assert(hud_diversion_line_width(title,hint)<=HUD_CLOCK_X-36);
  }
  kbd_language=lang;kbd_page=0;for(int i=36;i<40;i++){if(!keyboard_valid_focus(i))continue;char b[16];const char*key=key_label(i,b);int limit=101;if(width(key,0)>limit)fprintf(stderr,"Key overflow %s %d: %s (%d)\n",ui_language_codes[lang],i,key,width(key,0));assert(width(key,0)<=limit);}
  const char*states[]={"Ready","Calc","No data","No road","No route","Mem full","Error","Wait","Done","Wait"};int over=0;for(unsigned i=0;i<sizeof(states)/sizeof(states[0]);i++){int w=width(ui_tr(states[i]),0);if(w>104){fprintf(stderr,"Status overflow %s %s: %s (%d)\n",ui_language_codes[lang],states[i],ui_tr(states[i]),w);over++;}}if(over)return 1;assert(width(ui_tr("CTRL"),0)<=52);
  nav_man_type=MAN_ROUNDABOUT;nav_man_exit=123;const char*exit=nav_man_text();whole_utf8(exit);assert(strstr(exit,"123"));assert(slen(exit)<64);
  pref.debug=1;struct HudCard zoom={0};hud_nav_text(&zoom,6,HUD_MANUAL_ZOOM,1);whole_utf8(zoom.title);assert(streq(zoom.title,ui_tr("Manual zoom")));pref.debug=0;
 }
 if(cliou_missing())fprintf(stderr,"Missing glyphs after dictionary checks: %u\n",cliou_missing());assert(!cliou_missing());
 pref.language=1;nav_man_type=MAN_LEFT;assert(streq(nav_man_text(),"Tourner à gauche"));pref.units=1;assert(streq(units_value(),"Métrique"));pref.autozoom=0;pref.debug=0;struct HudCard zoom={0};hud_nav_text(&zoom,6,HUD_MANUAL_ZOOM,1);assert(streq(zoom.title,"Zoom manuel"));
 // Keep the existing numeric IDs and persist every new one through real card I/O.
 for(int lang=0;lang<UI_LANGUAGE_COUNT;lang++){pref.language=lang;save_prefs();pref.language=-1;load_prefs();assert(pref.language==lang);}
 char invalid[]="language=999";parse_pref_line(invalid);assert(pref.language==0);
 char negative[]="language=-1";parse_pref_line(negative);assert(pref.language==0);
 // The remote cycles and wraps in both directions without changing unit/camera policy.
 menu_open_setting(7,hud_now_ms());pref.language=0;pref.units=1;pref.cameras=2;pref.clock_auto=1;
 key_handle(K_LEFT);assert(pref.language==UI_LANGUAGE_COUNT-1);key_handle(K_RIGHT);assert(pref.language==0);
 for(int lang=1;lang<=UI_LANGUAGE_COUNT;lang++){key_handle(K_RIGHT);assert(pref.language==lang%UI_LANGUAGE_COUNT);assert(pref.units==1&&pref.cameras==2&&pref.clock_auto==1);}
 // Localize interface literals without altering received names that match them.
 nrecents=nfaves=1;copystr(recents[0].name,sizeof(recents[0].name),"Settings");copystr(faves[0].name,sizeof(faves[0].name),"Cancel");trip.active=1;trip.count=1;trip.active_idx=0;copystr(trip.target[0].name,sizeof(trip.target[0].name),"Saved");route_status=R_IDLE;trip_need_route=0;gps.hour=12;gps.min=35;
 int frames=0;
 for(int lang=0;lang<UI_LANGUAGE_COUNT;lang++)for(int night=0;night<2;night++)for(int page=0;page<19;page++){
  pref.language=lang;pref.theme=night?THEME_NIGHT:THEME_DAY;modal=MOD_NONE;trip.active=trip.count=page==5?0:1;screen=page==0?SCR_MENU:(page==1||page==5||page==6)?SCR_TRIP:page==2?SCR_BT:page==3?SCR_SET:SCR_KEY;if(page>=7&&page<10){screen=SCR_MAP;mapok=page!=7;gps.fix=page==9;track_valid=page==9;track_lat=5500000;track_lon=-160000;}else{mapok=0;gps.fix=0;track_valid=0;}if(screen==SCR_KEY)keyboard_begin();set_focus=7;kbd_action=KBD_NAME_FAVE;copystr(kbd_text,sizeof(kbd_text),"Cancel");if(page>=10&&page<14){screen=page==13?SCR_KEY:SCR_COORDINATES;coordinates.step=page==10?COORD_LAT:page==11?COORD_LON:COORD_REVIEW;coordinates.error=page<12;coordinates.focus=12;coordinates.review_focus=0;coordinates.lat=5497548;coordinates.lon=-164050;copystr(coordinates.value[0],sizeof(coordinates.value[0]),"54.97548");copystr(coordinates.value[1],sizeof(coordinates.value[1]),"-1.64050");copystr(coordinates.name,sizeof(coordinates.name),"Cancel");if(page==13)kbd_action=KBD_NAME_COORDINATES;}if(page==6)modal=MOD_RESUME_TRIP;menu_reset(hud_now_ms());if(page>=14)menu_open(P_SET_DISPLAY+page-14,0,hud_now_ms());draw();hud_settle_for_capture(hud_now_ms());menu_settle(hud_now_ms());draw();assert(streq(recents[0].name,"Settings")&&streq(faves[0].name,"Cancel")&&streq(trip.target[0].name,"Saved")&&streq(kbd_text,"Cancel"));char file[640];snprintf(file,sizeof(file),"%s/%s-%s-%d.rgb565",argv[2],ui_language_codes[lang],night?"night":"day",page);FILE*f=fopen(file,"wb");assert(f);fwrite(fb,1,sizeof(fb),f);fclose(f);frames++;
 }
 // New persistent suggested-trip row and confirmation in both themes and
 // control contexts. Preserve the full blue instruction with a long name.
 for(int lang=0;lang<UI_LANGUAGE_COUNT;lang++)for(int night=0;night<2;night++){
  pref.language=lang;pref.theme=night?THEME_NIGHT:THEME_DAY;pref.learn_history=pref.destination_predictions=1;
  screen=SCR_MAP;modal=MOD_NONE;freecam=0;mapok=0;gps.fix=1;trip.active=trip.count=0;route_status=R_IDLE;
  mob_declined_n=0;mob_suggest_valid=1;mob_suggest_serial++;mob_suggest_point=(struct MobPoint){5497548,-164050};
  nrecents=0;nfaves=1;faves[0].lat=mob_suggest_point.lat;faves[0].lon=mob_suggest_point.lon;
  copystr(faves[0].name,sizeof(faves[0].name),"A very long favourite name for the suggestion width test");
  hw_bluetooth_enabled=btui.connected=1;set_band("CD");btui.playing=1;copystr(btui.title,sizeof(btui.title),"Bluetooth Sonata");
  for(int state=0;state<3;state++){
   hud_reset_animations();screen=SCR_MAP;bt_control_mode=state==1;draw();hud_settle_for_capture(hud_now_ms());draw();
   assert(hud_tween[HC_STATUS].target.suggestion_serial==mob_suggest_serial);
   assert(streq(hud_tween[HC_STATUS].target.traffic_hint,ui_tr(state==1?"BACK to review":"OK to review")));
   assert(!!hud_suggestion_shortcut_ready()==(state!=1));
   if(state==2){key_handle(K_ENTER);assert(sheet_page==P_CONFIRM&&confirm_kind==4&&sheet_focus==0);draw();menu_settle(hud_now_ms());draw();}
   char file[640];snprintf(file,sizeof(file),"%s/%s-%s-suggestion-%d.rgb565",argv[2],ui_language_codes[lang],night?"night":"day",state);
   FILE*f=fopen(file,"wb");assert(f);fwrite(fb,1,sizeof(fb),f);fclose(f);frames++;
  }
  menu_close(hud_now_ms());mob_suggest_valid=0;bt_control_mode=0;
 }
 mob_suggest_valid=0;pref.learn_history=0;trip.active=trip.count=1;
 // The startup frequent-place confirmation uses the same dialog face and
 // remains within the normal HUD area in every locale and theme.
 for(int lang=0;lang<UI_LANGUAGE_COUNT;lang++)for(int night=0;night<2;night++){
  pref.language=lang;pref.theme=night?THEME_NIGHT:THEME_DAY;pref.learn_history=pref.destination_predictions=1;
  screen=SCR_MAP;modal=MOD_NONE;freecam=bt_control_mode=0;mapok=0;gps.fix=1;trip.active=trip.count=0;nfaves=0;route_status=R_IDLE;
  mob_suggest_valid=0;mob_recording=1;mob_started=0;mob_rmc_fix=1;mob_rmc_speed=0;mob_rmc_point=(struct MobPoint){5497548,-164050};
  frequent_boot_session=mob_session_generation;mob_rmc_ms=hud_now_ms();
  confirm_frequent.lat=mob_rmc_point.lat;confirm_frequent.lon=mob_rmc_point.lon;confirm_frequent_named=1;copystr(confirm_frequent.name,sizeof(confirm_frequent.name),"Work");
  assert(width(ui_tr("You come here often"),0)<=328);hud_reset_animations();confirm_kind=5;menu_open(P_CONFIRM,1,hud_now_ms());
  draw();hud_settle_for_capture(hud_now_ms());menu_settle(hud_now_ms());draw();assert(sheet_page==P_CONFIRM&&sheet_focus==1);
  char file[640];snprintf(file,sizeof(file),"%s/%s-%s-frequent.rgb565",argv[2],ui_language_codes[lang],night?"night":"day");FILE*f=fopen(file,"wb");assert(f);fwrite(fb,1,sizeof(fb),f);fclose(f);frames++;
  menu_close(hud_now_ms());
 }
 mob_recording=0;pref.learn_history=0;trip.active=trip.count=1;
 // Exercise translated source headers and saved-phone dialog through actual drawing.
 for(int lang=0;lang<UI_LANGUAGE_COUNT;lang++){
  pref.language=lang;screen=SCR_BT;modal=MOD_NONE;copystr(af.band,sizeof(af.band),"CD");bt_control_mode=0;
  for(int state=0;state<4;state++){btui.connected=state>0;btui.playing=state==2;btui.title[0]=btui.artist[0]=btui.album[0]=0;if(state==3)copystr(btui.title,sizeof(btui.title),"Bluetooth Sonata");header();}
  assert(streq(btui.title,"Bluetooth Sonata")); // Received media titles are never renamed.
  btui.title[0]=0;nbtdev=1;copystr(btdev[0].name,sizeof(btdev[0].name),"Bluetooth Phone");copystr(btdev[0].addr,18,"00:11:22:33:44:00");menu_open(P_PHONE,1,hud_now_ms());key_handle(K_ENTER);draw();menu_settle(hud_now_ms());menu_draw(hud_now_ms());nbtdev=0;
  af.station[0]=af.freq[0]=car.source[0]=0;copystr(af.band,sizeof(af.band),"FM");header();
 }
 assert(!cliou_missing());printf("Interface languages: %d complete dictionaries, glyphs including Arabic/Hebrew bidi and CJK/Devanagari, UTF8 buffers, remote wrap, saved settings, translated phone labels/source states, unchanged units/policies/names and %d actual native framebuffer captures passed.\n",UI_LANGUAGE_COUNT,frames);return 0;
}
