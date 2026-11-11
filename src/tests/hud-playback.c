/* Playback inference, seeks, pauses, unknown data and RGB565 bar bounds. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static struct HudCard*prepare(u32 now){hud_capture_ms=now;hud_prepare(5,now,1);return &hud_tween[HC_STATUS].target;}
static void baseline(u32 now){
 hud_capture_clock=1;hud_capture_ms=now;gps.fix=mapok=track_valid=1;gps.hour=12;
 mh.minlat=mh.minlon=-90000000;mh.maxlat=mh.maxlon=90000000;
 trip.active=1;route_status=hud_observed_route=R_READY;screen=SCR_MAP;modal=MOD_NONE;freecam=0;
 hud_audio.kind=hud_source.kind=hud_navigation.kind=hud_connection.kind=hud_bt_feedback.kind=HUD_NONE;
 set_band("CD");hud_source.kind=HUD_NONE;btui.connected=btui.playing=1;btui.pos=30;btui.dur=120;
 copystr(btui.title,sizeof(btui.title),"Track one");copystr(btui.artist,sizeof(btui.artist),"Artist");bt_control_mode=1;
 hud_reset_animations();
}
int main(void){
 baseline(10000);struct HudCard*c=prepare(10000);assert(c->flags&HUD_PLAYBACK);int height=c->box.h;
 assert(!hud_playback_fraction(10000));
 u32 middle=hud_playback_fraction(10160),full=hud_playback_fraction(10320);
 assert(middle>0&&middle<full&&full>16384);
 u32 text_at=hud_tween[HC_STATUS].content_at,layout_at=hud_status_layout.at;
 prepare(10400);u32 p1=hud_playback_fraction(10400);prepare(10800);u32 p2=hud_playback_fraction(10800);
 assert(p2>p1&&hud_tween[HC_STATUS].content_at==text_at&&hud_status_layout.at==layout_at);
 assert(c->box.h==height); /* progress clock never changes pill size or text fade */
 btui.pos=31;prepare(11000);assert(hud_playback_fraction(11000)==hud_playback_ratio(31000,120000));
 /* A reported seek eases from the current position, in either direction. */
 u32 from=hud_playback_fraction(11100);btui.pos=90;prepare(11100);
 assert(hud_playback_fraction(11100)==from);
 middle=hud_playback_fraction(11260);full=hud_playback_fraction(11420);
 assert(middle>from&&middle<full&&full>49000);
 from=full;btui.pos=10;prepare(11420);assert(hud_playback_fraction(11420)==from);
 assert(hud_playback_fraction(11580)<from&&hud_playback_fraction(11580)>hud_playback_fraction(11740));
 /* Remote pause freezes immediately through stale phone status; resume advances. */
 prepare(12000);hud_capture_ms=12000;bt_transport_command(BT_TRANSPORT_PLAY_PAUSE);assert(bt_playback_request==0);
 prepare(12000);u32 frozen=hud_playback_fraction(12320);prepare(12500);assert(hud_playback_fraction(12500)==frozen);
 btui.playing=0;bt_playback_request=-1;prepare(12600);
 hud_capture_ms=12600;bt_transport_command(BT_TRANSPORT_PLAY_PAUSE);assert(bt_playback_request==1);
 prepare(12600);assert(hud_playback_fraction(13000)>frozen);
 bt_playback_request=-1;btui.playing=1;
 /* New track starts its own progress rather than animating the old tail across. */
 copystr(btui.title,sizeof(btui.title),"Track two");btui.pos=0;prepare(14000);assert(!hud_playback_fraction(14000));
 assert(hud_playback_fraction(14320)<1000);
 /* No extrapolation runs away if the phone stops supplying position updates. */
 prepare(18000);u32 stale=hud_playback_fraction(18000);prepare(23000);assert(hud_playback_fraction(23000)==stale);
 const char*bad[]={"-1","--","", "30bad","4294968","99999999999999999999999"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++){
  assert(bt_media_seconds(bad[i])<0);char line[96];snprintf(line,sizeof(line),"POS=%s",bad[i]);bt_parse_meta_line(line);
  prepare(24000+i*400);assert(!(c->flags&HUD_PLAYBACK)&&c->box.h==height);
 }
 bt_parse_meta_line("POS=0");bt_parse_meta_line("DUR=0");prepare(28000);assert(!(c->flags&HUD_PLAYBACK));
 bt_parse_meta_line("DUR=--");assert(!btui.dur);
 btui.dur=120;btui.pos=121;prepare(28500);assert(!(c->flags&HUD_PLAYBACK));
 btui.pos=120;prepare(29000);assert(hud_playback_fraction(29320)==65536);
 btui.connected=0;prepare(29500);assert(!hud_playback.valid&&(!c->box.a||!(c->flags&HUD_PLAYBACK)));
 /* Full-range ratios and wrapped monotonic time remain safe without floats. */
 assert(hud_playback_ratio(2147483500u,4294967000u)==32768);
 assert(hud_playback_ratio(4294967000u,4294967000u)==65536);
 baseline(0xffffff00u);prepare(0xffffff00u);prepare(0x100u);assert(hud_playback_fraction(0x100u)>16384);
 /* Rounded subpixel ends and clipping, including tiny/empty/full bars. */
 for(int night=0;night<2;night++){
  apply_palette(night);fill(0,0,W,H,0x1234);hud_ink_alpha=16;
  hud_playback_strip(20,30,100*256+128,C_CYAN);
  assert(fb[31*W+50]==C_CYAN&&fb[31*W+120]!=0x1234&&fb[31*W+120]!=C_CYAN);
  assert(fb[31*W+121]==0x1234&&fb[29*W+50]==0x1234&&fb[33*W+50]==0x1234);
  fill(0,0,W,H,0x1234);hud_playback_strip(20,30,0,C_CYAN);assert(fb[30*W+20]==0x1234);
  hud_playback_strip(20,30,128,C_CYAN);assert(fb[31*W+21]==0x1234);
 }
 baseline(30000);route_status=R_CALC;
 copystr(hud_connected_name,sizeof(hud_connected_name),"Phone");hud_pulse_at(&hud_connection,HUD_CONNECTION,30000,4200);
 copystr(hud_bt_text,sizeof(hud_bt_text),"Reconnecting");hud_pulse_at(&hud_bt_feedback,HUD_BT_FEEDBACK,30000,2500);
 set_volume(18);prepare(30000);assert(c->box.h<=96&&(c->flags&HUD_PLAYBACK)&&(c->flags&HUD_PROGRESS));
 for(u32 t=30000;t<=30320;t+=64){fill(0,0,W,H,0x1234);hud_draw_card(HC_STATUS,t);}
 puts("Playback bar: progress/seek easing, pause/resume, track changes, stale/unknown data, subpixel AA, wraparound and busy pill bounds passed.");
}
