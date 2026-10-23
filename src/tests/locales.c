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
  assert(width(bt_source_label(),0)<=52);assert(width(bt_label(),3)<=440);assert(streq(bt_source_label(),"BT"));assert(streq(bt_label(),lang?ui_tr("Phone connection"):"Bluetooth"));assert(width(ui_tr("Phone audio"),1)<=180);
  copystr(btui.status,sizeof(btui.status),"BT DAEMON ERROR");pref.debug=0;assert(streq(bt_status_text(),lang?ui_tr("Phone connection unavailable"):"Bluetooth unavailable"));btui.status[0]=0;
  for(unsigned i=0;i<sizeof(locale_cases)/sizeof(locale_cases[0]);i++){assert(streq(ui_tr(locale_cases[i][0]),locale_cases[i][lang]));whole_utf8(locale_cases[i][lang]);assert(cliou_width(locale_cases[i][lang],1)>0);}
  kbd_language=lang;kbd_page=0;for(int i=36;i<40;i++){if(!keyboard_valid_focus(i))continue;char b[16];const char*key=key_label(i,b);int limit=101;if(width(key,0)>limit)fprintf(stderr,"Key overflow %s %d: %s (%d)\n",ui_language_codes[lang],i,key,width(key,0));assert(width(key,0)<=limit);}
  const char*states[]={"READY","CALC","NO DATA","NO ROAD","NO ROUTE","MEM FULL","ERROR","WAIT","Done","Wait"};int over=0;for(unsigned i=0;i<sizeof(states)/sizeof(states[0]);i++){int w=width(ui_tr(states[i]),0);if(w>104){fprintf(stderr,"Status overflow %s %s: %s (%d)\n",ui_language_codes[lang],states[i],ui_tr(states[i]),w);over++;}}if(over)return 1;assert(width(ui_tr("CTRL"),0)<=52);
  nav_man_type=MAN_ROUNDABOUT;nav_man_exit=123;const char*exit=nav_man_text();whole_utf8(exit);assert(strstr(exit,"123"));assert(slen(exit)<64);
  pref.debug=1;pref.autozoom=0;nav_status_prepare(6);whole_utf8(nav_zoom);assert(strstr(nav_zoom,"Z6"));pref.debug=0;
 }
 if(cliou_missing())fprintf(stderr,"Missing glyphs after dictionary checks: %u\n",cliou_missing());assert(!cliou_missing());
 pref.language=1;nav_man_type=MAN_LEFT;assert(streq(nav_man_text(),"TOURNER À GAUCHE"));pref.units=1;assert(streq(units_value(),"Métrique"));pref.autozoom=0;pref.debug=0;nav_status_prepare(6);assert(streq(nav_zoom,"Manuel"));
 // Keep the existing numeric IDs and persist every new one through real card I/O.
 for(int lang=0;lang<UI_LANGUAGE_COUNT;lang++){pref.language=lang;save_prefs();pref.language=-1;load_prefs();assert(pref.language==lang);}
 char invalid[]="language=999";parse_pref_line(invalid);assert(pref.language==0);
 char negative[]="language=-1";parse_pref_line(negative);assert(pref.language==0);
 // The remote cycles and wraps in both directions without changing unit/camera policy.
 screen=SCR_SET;set_focus=7;pref.language=0;pref.units=1;pref.cameras=2;pref.clock_auto=1;
 key_handle(K_LEFT);assert(pref.language==UI_LANGUAGE_COUNT-1);key_handle(K_RIGHT);assert(pref.language==0);
 for(int lang=1;lang<=UI_LANGUAGE_COUNT;lang++){key_handle(K_ENTER);assert(pref.language==lang%UI_LANGUAGE_COUNT);assert(pref.units==1&&pref.cameras==2&&pref.clock_auto==1);}
 // Localize interface literals without altering received names that match them.
 nrecents=nfaves=1;copystr(recents[0].name,sizeof(recents[0].name),"Settings");copystr(faves[0].name,sizeof(faves[0].name),"Cancel");trip.active=1;trip.count=1;trip.active_idx=0;copystr(trip.target[0].name,sizeof(trip.target[0].name),"Saved");route_status=R_IDLE;trip_need_route=0;gps.hour=12;gps.min=35;
 int frames=0;
 for(int lang=0;lang<UI_LANGUAGE_COUNT;lang++)for(int night=0;night<2;night++)for(int page=0;page<10;page++){
  pref.language=lang;pref.theme=night?THEME_NIGHT:THEME_DAY;modal=MOD_NONE;trip.active=page==5?0:1;screen=page==0?SCR_MENU:(page==1||page==5||page==6)?SCR_TRIP:page==2?SCR_BT:page==3?SCR_SET:SCR_KEY;if(page>=7){screen=SCR_MAP;mapok=page!=7;gps.fix=page==9;track_valid=page==9;track_lat=5500000;track_lon=-160000;}else{mapok=0;gps.fix=0;track_valid=0;}if(screen==SCR_KEY)keyboard_begin();set_focus=7;kbd_action=KBD_NAME_FAVE;copystr(kbd_text,sizeof(kbd_text),"Cancel");if(page==6)modal=MOD_RESUME_TRIP;draw();assert(streq(recents[0].name,"Settings")&&streq(faves[0].name,"Cancel")&&streq(trip.target[0].name,"Saved")&&streq(kbd_text,"Cancel"));char file[640];snprintf(file,sizeof(file),"%s/%s-%s-%d.rgb565",argv[2],ui_language_codes[lang],night?"night":"day",page);FILE*f=fopen(file,"wb");assert(f);fwrite(fb,1,sizeof(fb),f);fclose(f);frames++;
 }
 // Exercise translated source headers and saved-phone dialog through actual drawing.
 for(int lang=0;lang<UI_LANGUAGE_COUNT;lang++){
  pref.language=lang;screen=SCR_BT;modal=MOD_NONE;copystr(af.band,sizeof(af.band),"CD");bt_control_mode=0;
  for(int state=0;state<4;state++){btui.connected=state>0;btui.playing=state==2;btui.title[0]=btui.artist[0]=btui.album[0]=0;if(state==3)copystr(btui.title,sizeof(btui.title),"Bluetooth Sonata");header();}
  assert(streq(btui.title,"Bluetooth Sonata")); // Received media titles are never renamed.
  btui.title[0]=0;nbtdev=1;copystr(btdev[0].name,sizeof(btdev[0].name),"Bluetooth Phone");bt_page_dialog=0;draw_bluetooth();bt_page_dialog=-1;nbtdev=0;
  af.station[0]=af.freq[0]=car.source[0]=0;copystr(af.band,sizeof(af.band),"FM");header();
 }
 assert(!cliou_missing());printf("Interface languages: %d complete dictionaries, glyphs including Arabic/Hebrew bidi and CJK/Devanagari, UTF8 buffers, remote wrap, saved settings, translated phone labels/source states, unchanged units/policies/names and %d actual native framebuffer captures passed.\n",UI_LANGUAGE_COUNT,frames);return 0;
}
