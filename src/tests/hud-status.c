/* State transitions, bounded composition, queue display time and menu clipping. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static struct HudCard*card(void){return &hud_tween[HC_STATUS].target;}
static void prepare(u32 now,int map){hud_capture_ms=now;screen=map?SCR_MAP:SCR_SET;hud_prepare(5,now,map);}
static int contains(const char*text){struct HudCard*c=card();return c->box.a&&(strstr(c->title,text)||strstr(c->detail,text)||strstr(c->connection,text)||strstr(c->feedback,text));}
static void baseline(void){
 hud_capture_clock=1;hud_capture_ms=10000;gps.fix=mapok=track_valid=1;gps.hour=12;gps.min=35;
 mh.minlat=mh.minlon=-90000000;mh.maxlat=mh.maxlon=90000000;
 trip.active=1;trip.paused=0;route_status=R_READY;hud_observed_route=R_READY;
 freecam=bt_control_mode=0;modal=MOD_NONE;car.tempok=0;pref.language=0;
 hud_audio.kind=hud_source.kind=hud_navigation.kind=hud_connection.kind=hud_bt_feedback.kind=HUD_NONE;
 audio_source_live=1;copystr(af.band,sizeof(af.band),"CD");btui.connected=1;
 hud_reset_animations();prepare(10000,1);assert(!card()->box.a);
}
int main(void){
 baseline();gps.fix=0;set_volume(16);prepare(10100,1);
 assert(card()->kind==HUD_VOLUME&&contains("Acquiring GPS"));
 assert(((card()->ink>>6)&3)==HUD_WARNING);u32 entered=hud_tween[HC_STATUS].position_at;
 prepare(10260,1);assert(hud_pose(&hud_tween[HC_STATUS],10260).a>0);
 assert(hud_tween[HC_STATUS].position_at==entered);
 prepare(14500,1);assert(!contains("Acquiring GPS"));
 prepare(20000,1);assert(!contains("Acquiring GPS")); /* no periodic spam */
 gps.fix=1;prepare(21000,0);assert(contains("GPS active")); /* menu also observes recovery */
 assert(card()->box.y+card()->box.h<MENU_BODY_Y);

 baseline();mapok=0;gps.fix=0;route_status=R_UNREACHABLE;
 prepare(10100,1);assert(card()->kind==HUD_FAILURE&&contains("Acquiring GPS"));
 assert(hud_events[HUD_EVENT_MAP].left==4200);
 prepare(14500,1);assert(contains("Map data missing"));
 mapok=1;gps.fix=1;prepare(14600,1);assert(!hud_events[HUD_EVENT_MAP].left);
 assert(contains("GPS active")); /* stale missing-map event is cancelled */
 trip.active=0;route_status=R_IDLE;prepare(20000,1);assert(contains("NO ACTIVE ROUTE"));
 prepare(25000,1);assert(!contains("NO ACTIVE ROUTE"));
 prepare(30000,1);assert(!contains("NO ACTIVE ROUTE"));
 trip.active=1;route_status=R_CALC;prepare(30100,1);assert(!hud_events[HUD_EVENT_ROUTE].left);

 baseline();bt_control_mode=1;route_status=R_CALC;
 copystr(btui.title,sizeof(btui.title),"Night Drive");
 copystr(hud_connected_name,sizeof(hud_connected_name),"Demo phone");
 copystr(hud_bt_text,sizeof(hud_bt_text),"Reconnecting");
 hud_pulse_at(&hud_connection,HUD_CONNECTION,10000,4200);
 hud_pulse_at(&hud_bt_feedback,HUD_BT_FEEDBACK,10000,2500);
 set_volume(18);notice_fail("STOP NOT ADDED");
 prepare(10100,1);assert(card()->flags==(HUD_KEYS|HUD_PROGRESS));
 assert(streq(card()->badge,"VOL 18")&&card()->box.h<=96);
 assert(!contains("STOP NOT ADDED")); /* all three existing rows are in use */
 prepare(12000,1);assert(hud_events[HUD_EVENT_ACTION].left==4200);
 prepare(12600,1);assert(contains("STOP NOT ADDED")&&card()->box.h<=96);
 assert(card()->flags&HUD_KEYS);assert(card()->flags&HUD_PROGRESS);
 prepare(12680,1);fill(0,0,W,H,0x1234);hud_draw_all(12680,1);
 prepare(15000,0);assert(card()->box.y+card()->box.h<MENU_BODY_Y);
 fill(0,0,W,H,0x1234);hud_draw_card(HC_STATUS,15320);
 for(int i=MENU_BODY_Y*W;i<W*H;i++)assert(fb[i]==0x1234);

 baseline();screen=SCR_TRIP;modal=MOD_RECENT;
 notice_success("Favourite saved");notice_success("Stop moved");
 prepare(11000,0);prepare(18000,0);
 assert(hud_events[HUD_EVENT_ACTION].left==4200); /* dialog does not consume lifetime */
 modal=MOD_NONE;prepare(19000,0);assert(contains("Favourite saved"));
 prepare(23400,0);assert(contains("Stop moved"));
 prepare(27800,0);assert(!card()->box.a);
 /* Exit is animated and leaves the clock alone. */
 assert(hud_pose(&hud_tween[HC_STATUS],27960).a>0);
 assert(!hud_pose(&hud_tween[HC_STATUS],28120).a);

 baseline();mh.flags=1;prepare(10100,1);assert(contains("Test map"));
 prepare(14500,1);assert(!contains("Test map"));mh.flags=0;

 baseline();hud_capture_ms=0xfffffff0u;notice_success("Saved");prepare(0xfffffff0u,1);
 prepare(0x100u,1);assert(contains("Saved"));
 prepare(0x1200u,1);assert(!contains("Saved"));
 puts("Shared status notifications: transitions, deferred display, expiry, wraparound, menu bounds and busy BT/route composition passed.");
}
