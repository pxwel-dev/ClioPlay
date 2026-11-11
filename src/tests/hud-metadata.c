/* Real daemon/Focus consumers: track identity and frequency changes, no timer spam. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static void file(const char*path,const char*text){FILE*f=fopen(path,"w");assert(f);assert(fputs(text,f)>=0&&fclose(f)==0);}
static void metadata(const char*title,const char*artist,const char*album,int pos){
 char text[1024];snprintf(text,sizeof(text),"DEVICE=Demo phone\nTITLE=%s\nARTIST=%s\nALBUM=%s\nCONNECTED=1\nPLAYING=1\nPOS=%d\nDUR=240\n",title,artist,album,pos);
 file(BT_META_PATH,text);bt_poll_status();
}
static struct HudCard*prepare(void){hud_prepare(5,hud_capture_ms,screen==SCR_MAP);return &hud_tween[HC_STATUS].target;}
int main(void){
 hud_capture_clock=1;hud_capture_ms=10000;gps.fix=mapok=track_valid=1;gps.hour=12;trip.active=1;
 mh.minlat=mh.minlon=-90000000;mh.maxlat=mh.maxlon=90000000;
 route_status=hud_observed_route=R_READY;screen=SCR_MAP;modal=MOD_NONE;
 hud_reset_animations();set_band("CD");hud_source.kind=HUD_NONE;
 file(BT_STATUS_PATH,"BT STREAMING\n");file(BT_DEVLIST_PATH,"COUNT=0\n");
 metadata("First track","First artist","Album",10);
 assert(hud_source.kind==HUD_TRACK);
 hud_connection.kind=HUD_NONE;
 struct HudCard*c=prepare();assert(c->kind==HUD_SOURCE&&streq(c->title,"First track")&&streq(c->detail,"First artist"));
 u32 until=hud_source.until;
 hud_capture_ms+=1000;metadata("First track","First artist","Album",11);
 assert(hud_source.until==until); /* playback clock is not a new song */
 hud_capture_ms=until;prepare();hud_capture_ms+=500;prepare();assert(!c->box.a);
 hud_capture_ms+=100;metadata("Second track","Second artist","Album",0);prepare();
 assert(c->kind==HUD_SOURCE&&streq(c->title,"Second track"));
 assert(!hud_pose(&hud_tween[HC_STATUS],hud_capture_ms).a);
 assert(hud_pose(&hud_tween[HC_STATUS],hud_capture_ms+160).a>0);
 hud_capture_ms+=500;bt_control_mode=1;set_volume(18);prepare();
 hud_capture_ms+=500;metadata("Third track","Third artist","Album",0);prepare();
 assert(c->kind==HUD_MEDIA&&(c->flags&HUD_KEYS)&&streq(c->title,"Third track")&&streq(c->badge,"VOL 18"));
 assert(hud_tween[HC_STATUS].content_at==hud_capture_ms);
 hud_capture_ms+=500;metadata("Third track","New artist","Album",1);prepare();
 assert(streq(c->detail,"New artist / Album"));
 assert(hud_tween[HC_STATUS].content_at==hud_capture_ms);
 bt_control_mode=0;hud_audio.kind=HUD_NONE;hud_capture_ms+=500;metadata("--","--","--",0);prepare();
 assert(hud_source.kind==HUD_NONE&&!c->box.a); /* no placeholder update */

 set_band("FM");copystr(af.station,sizeof(af.station),"STATION");copystr(af.freq,sizeof(af.freq),"98.8");rebuild_source();
 hud_source.kind=HUD_NONE;hud_capture_ms+=500;
 metadata("Phone track","Phone artist","Album",0);assert(!hud_source.kind); /* other source active */
 int changed=0;process_property(65,(const u8*)"99.2",4,&changed);assert(changed);rebuild_source();
 prepare();assert(c->kind==HUD_SOURCE&&strstr(c->title,"STATION")&&strstr(c->title,"99.2"));
 until=hud_source.until;hud_capture_ms+=500;changed=0;
 process_property(65,(const u8*)"99.2",4,&changed);assert(!changed);rebuild_source();assert(hud_source.until==until);
 process_property(65,(const u8*)"--",2,&changed);assert(!changed&&streq(af.freq,"99.2"));
 hud_capture_ms+=500;screen=SCR_SET;process_property(65,(const u8*)"101.4",5,&changed);assert(changed);rebuild_source();
 prepare();assert(strstr(c->title,"101.4")&&c->box.y+c->box.h<MENU_BODY_Y);

 /* Every severity has a distinct, theme-appropriate ink and correct domain. */
 for(int night=0;night<2;night++){
  apply_palette(night);
  u16 colours[]={hud_status_colour(HUD_INFO,C_WHITE),hud_status_colour(HUD_SUCCESS,C_WHITE),hud_status_colour(HUD_WARNING,C_WHITE),hud_status_colour(HUD_ERROR,C_WHITE)};
  for(int i=0;i<4;i++)for(int j=i+1;j<4;j++)assert(colours[i]!=colours[j]);
 }
 hud_bt_notice(ui_tr("Disconnected"));assert(hud_bt_level==HUD_WARNING);
 btui.connected=0;hud_source.kind=HUD_NONE;hud_capture_ms+=100;prepare();
 assert(c->kind==HUD_BT_FEEDBACK&&(c->ink&3)==HUD_WARNING);
 hud_bt_notice(ui_tr("Device forgotten"));assert(hud_bt_level==HUD_SUCCESS);
 hud_bt_notice(ui_tr("Pairing phone"));assert(hud_bt_level==HUD_INFO);
 s_unlink(BT_STATUS_PATH);s_unlink(BT_META_PATH);s_unlink(BT_DEVLIST_PATH);
 puts("Metadata: song/artist changes, position-only silence, BT controls, radio frequency changes with unchanged RDS, menu bounds, placeholders and importance colours passed.");
}
