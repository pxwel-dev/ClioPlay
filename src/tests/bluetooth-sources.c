/* Real Focus/daemon inputs: source aliases, metadata, controls and output gate. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static void file(const char*p,const char*s){FILE*f=fopen(p,"w");assert(f);assert(fputs(s,f)>=0&&fclose(f)==0);}
static struct HudCard*prepare(void){hud_prepare(5,hud_capture_ms,screen==SCR_MAP);return &hud_tween[HC_STATUS].target;}
static void gate(int expected){char b[4]={0};long fd=s_open(BT_ACTIVE_PATH,O_RDONLY_,0);assert(fd>=0);assert(s_read((int)fd,b,3)==2);s_close((int)fd);assert(b[0]=='0'+expected);}
static void command(const char*expected){char b[64]={0};long fd=s_open(BT_CMD_PATH,O_RDONLY_,0);assert(fd>=0);assert(s_read((int)fd,b,sizeof(b)-1)>0);s_close((int)fd);assert(strstr(b,expected));}
static void baseline(void){
 hud_capture_clock=1;hud_capture_ms=10000;screen=SCR_MAP;modal=MOD_NONE;freecam=bt_control_mode=0;
 pref.debug=pref.language=0;trip.active=1;trip.paused=0;route_status=hud_observed_route=R_READY;
 gps.fix=mapok=track_valid=1;gps.hour=12;gps.min=35;car.tempok=0;
 mh.minlat=mh.minlon=-90000000;mh.maxlat=mh.maxlon=90000000;
 memzero(&af,sizeof(af));memzero(&audio_presented,sizeof(audio_presented));memzero(&btui,sizeof(btui));
 car.source[0]=car.volume[0]=0;audio_source_live=0;bt_active_written=-1;nbtdev=0;bt_hud_link=0;bt_hud_peer[0]=0;
 hud_audio.kind=hud_source.kind=hud_navigation.kind=hud_connection.kind=hud_bt_feedback.kind=HUD_NONE;
 hud_reset_animations();bt_playback_request=-1;bt_transport_press.kind=0;
 file(BT_DEVLIST_PATH,"COUNT=0\n");
}
static void metadata(const char*title,int position){
 char b[512];snprintf(b,sizeof(b),"DEVICE=Demo phone\nCONNECTED=1\nPLAYING=1\nTITLE=%s\nARTIST=Demo artist\nALBUM=Demo album\nPOS=%d\nDUR=180\n",title,position);
 file(BT_STATUS_PATH,"BT STREAMING\n");file(BT_META_PATH,b);bt_poll_status();bt_source_service();
}
int main(void){
 const struct {const char*source;int phone,selected,enabled;const char*tag;} cases[]={
  {"CD",0,1,1,"BT"},{"CD",1,1,1,"BT"},{"AUX",0,0,0,"AUX"},{"AUX",1,0,0,"AUX"},
  {"FM",1,0,0,"FM"},{"MW",1,0,0,"MW"},{"LW",1,0,0,"LW"},{"--",0,1,1,"BT"},{"--",1,1,1,"BT"},
 };
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
  baseline();btui.connected=cases[i].phone;set_band(cases[i].source);rebuild_source();bt_source_service();
  assert(bt_source_selected()==cases[i].selected);gate(cases[i].enabled);
  assert(bt_controls_available()==(cases[i].phone&&cases[i].enabled));
  struct HudCard*c=prepare();assert(c->kind==HUD_SOURCE&&streq(c->tag,cases[i].tag));
  assert(!c->title[0]&&!c->badge[0]);
 }
 /* A phone connection never turns wired AUX into a Bluetooth playback path. */
 baseline();set_band("AUX");rebuild_source();bt_source_service();
 u32 until=hud_source.until;hud_capture_ms+=500;metadata("Phone track",65);
 struct HudCard*c=prepare();assert(streq(c->tag,"AUX")&&!c->title[0]&&hud_source.until==until);gate(0);
 assert(!bt_controls_available()&&!hud_playback.valid);
 key_handle(K_BACK);prepare();assert(!bt_control_mode&&!(c->flags&HUD_KEYS));
 /* A live CD event enables native integration even before a volume value exists. */
 assert(!car.volume[0]);set_band("CD");rebuild_source();bt_source_service();prepare();
 assert(streq(c->tag,"BT")&&bt_controls_available());gate(1);
 key_handle(K_BACK);prepare();assert(bt_control_mode&&(c->flags&HUD_KEYS)&&(c->flags&HUD_PLAYBACK));
 /* No readable source permits independent BT, without inventing a volume. */
 baseline();bt_source_service();prepare();
 assert(streq(c->tag,"BT")&&!c->title[0]&&!c->badge[0]&&!bt_controls_available());gate(1);
 metadata("Independent phone track",12);prepare();
 assert(streq(c->title,"Independent phone track")&&bt_controls_available()&&hud_playback.valid&&!c->badge[0]);gate(1);
 key_handle(K_BACK);prepare();assert(bt_control_mode&&(c->flags&HUD_KEYS)&&(c->flags&HUD_PLAYBACK));
 key_handle(K_LEFT);command(" PREV\n");assert(bt_transport_press.kind==BT_TRANSPORT_PREVIOUS);
 key_handle(K_RIGHT);command(" NEXT\n");assert(bt_transport_press.kind==BT_TRANSPORT_NEXT);
 key_handle(K_ENTER);command(" PAUSE\n");assert(bt_playback_request==0);
 key_handle(K_ENTER);command(" PLAY\n");assert(bt_playback_request==1);
 u32 sequence=bt_cmd_seq;key_handle(K_UP);key_handle(K_DOWN);assert(bt_cmd_seq==sequence&&!car.volume[0]);
 key_handle(K_BACK);prepare();assert(!bt_control_mode);
 /* A previous radio/volume record does not disable BT or leak stale labels. */
 baseline();copystr(af.band,sizeof(af.band),"FM");copystr(car.source,sizeof(car.source),"FM Old station");
 copystr(car.volume,sizeof(car.volume),"VOL 99");bt_source_service();prepare();
 assert(streq(c->tag,"BT")&&!c->title[0]&&!c->badge[0]);gate(1);
 metadata("Connected phone track",12);prepare();
 assert(streq(c->tag,"BT")&&streq(c->title,"Connected phone track")&&bt_controls_available()&&hud_playback.valid);gate(1);
 until=hud_source.until;
 for(int i=0;i<5;i++){hud_capture_ms+=100;process_audio_source("UNKNOWN");bt_source_service();prepare();assert(hud_source.until==until);gate(1);}
 key_handle(K_BACK);prepare();assert(bt_control_mode&&(c->flags&HUD_KEYS));
 /* A later valid radio source takes priority over the fallback. */
 set_band("CD");rebuild_source();bt_source_service();assert(bt_controls_available());gate(1);
 set_band("FM");rebuild_source();bt_source_service();prepare();
 assert(streq(c->tag,"FM")&&!bt_controls_available()&&!(c->flags&HUD_KEYS));gate(0);
 /* Losing the radio feed returns to BT once, with menu metadata available. */
 process_audio_source("UNKNOWN");bt_source_service();screen=SCR_SET;metadata("Independent menu track",0);prepare();
 assert(streq(c->tag,"BT")&&streq(c->title,"Independent menu track")&&bt_controls_available());gate(1);
 /* Menus observe the same AUX rule, including unknown phone metadata. */
 set_band("AUX");rebuild_source();bt_source_service();screen=SCR_SET;metadata("Menu track",0);prepare();
 assert(streq(c->tag,"AUX")&&!c->title[0]&&!(c->flags&HUD_KEYS));gate(0);
 file(BT_STATUS_PATH,"BT CONNECTED\n");
 file(BT_META_PATH,"CONNECTED=1\nPLAYING=0\nDEVICE=--\nTITLE=--\nARTIST=--\nALBUM=--\n");
 bt_poll_status();bt_source_service();prepare();assert(streq(c->tag,"AUX")&&!bt_controls_available());gate(0);
 s_unlink(BT_CMD_PATH);s_unlink(BT_ACTIVE_PATH);s_unlink(BT_STATUS_PATH);s_unlink(BT_META_PATH);s_unlink(BT_DEVLIST_PATH);
 puts("Bluetooth sources: CD and independent BT output, wired AUX, no-source controls/progress, transport commands, first-boot unknown volume, cached readings, live source transitions and menus passed.");
}
