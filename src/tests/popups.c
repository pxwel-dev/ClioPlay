/* Actual popup renderer, handlers and compositor with a controlled clock. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static void setup(u32 now){
 hud_capture_clock=1;hud_capture_ms=now;screen=SCR_MAP;modal=MOD_NONE;modal_choice=0;modal_index=0;
 bt_page_dialog=-1;kbd_picker=0;coordinates_active=0;trip.active=0;trip.count=2;trip.active_idx=0;
 pref.language=0;pref.debug=0;pref.theme=THEME_NIGHT;nbtdev=1;
 copystr(btdev[0].name,sizeof(btdev[0].name),"Saved phone");
 mapok=0;gps.fix=0;track_valid=1;gps.hour=12;gps.min=35;car.tempok=0;
 track_lat=track_lon=gps.lat=gps.lon=0;track_course=display_course=70;display_course_valid=1;
 route_status=R_IDLE;freecam=0;freecam_anim_mode=FC_ANIM_NONE;bt_control_mode=0;
 hud_audio.kind=hud_source.kind=hud_navigation.kind=hud_connection.kind=hud_bt_feedback.kind=HUD_NONE;
 hud_reset_animations();apply_palette(1);render_runtime_reset(now);
}
static void at(u32 now,int changed){hud_capture_ms=now;render_runtime_step(now,changed);}
static void gradient(void){for(int y=0;y<H;y++)for(int x=0;x<W;x++)fb[y*W+x]=rgb((x*3+y)%256,(x+y*2)%256,(x*2+y)%256);}
int main(void){
 setup(10000);at(10000,1);at(10400,1);
 u32 clock_at=hud_tween[HC_CLOCK].position_at;
 modal=MOD_ROUTE_POINT;popup_prepare(10401);
 assert(popup_pose(10401).a==0&&popup_animating(10401));
 struct HudBox mid=popup_pose(10561);assert(mid.a>0&&mid.a<256&&mid.w<356);
 popup_prepare(10561);assert(popup.at==10401); /* observation must not replay entry */
 popup_prepare(10721);hud_capture_ms=10721;draw_modal();assert(popup_pose(10721).a==256&&!popup_animating(10721));
 assert(hud_tween[HC_CLOCK].position_at==clock_at);
 struct HudBox focus=popup_focus(10721);modal_choice=1;popup_prepare(10722);
 struct HudBox half=popup_focus(10802);assert(half.x>focus.x&&half.x<popup.focus_to.x);
 assert(popup.at==10401&&popup_animating(10802));
 struct HudBox moving=popup_focus(10802);u32 content_at=popup.content_at;
 modal=MOD_NONE;popup_prepare(10802);
 assert(hud_box_eq(moving,popup_focus(10802))&&popup.content_at==content_at);
 modal=MOD_ROUTE_POINT;popup_prepare(10803);popup_settle_for_capture(10803);
 /* Parent smoothly resizes and hands text over when one prompt replaces another. */
 modal=MOD_SOURCE_PICK;dest_pick_action=DEST_PICK_ADD_STOP;popup_prepare(10900);
 assert(popup_pose(10900).h==126);mid=popup_pose(11060);assert(mid.h>126&&mid.h<192);
 popup_prepare(11220);assert(popup_pose(11220).h==192&&popup.card.count==6);
 /* Exit survives the model being cleared and the underlying screen changing. */
 modal=MOD_NONE;screen=SCR_MENU;popup_prepare(11221);
 assert(popup.card.count==6&&popup_pose(11221).a==256);
 assert(popup_pose(11381).a>0&&popup_pose(11381).a<256);
 assert(popup_pose(11541).a==0&&popup_animating(11541));
 hud_capture_ms=11541;draw_modal();assert(!popup_animating(11541)); /* final clear is presented */
 screen=SCR_TRIP;popup_prepare(11550);assert(!popup_animating(11550));
 /* Closing during entry, then opening another popup, starts at the current pose. */
 modal=MOD_FAVE;trip.active=1;popup_prepare(11600);popup_prepare(11700);
 mid=popup_pose(11700);modal=MOD_NONE;popup_prepare(11700);assert(hud_box_eq(mid,popup_pose(11700)));
 modal=MOD_WAIT_PHONE;popup_prepare(11750);assert(popup.from.a>0&&popup.at==11750);

 /* Each modal family, both trip modes, tall glass and the non-modal dialogs. */
 for(int night=0;night<2;night++)for(int active=0;active<2;active++)for(int type=MOD_ROUTE_POINT;type<=MOD_WAIT_PHONE;type++){
  setup(20000);apply_palette(night);trip.active=active;modal=type;modal_index=0;popup_prepare(20000);popup_settle_for_capture(20000);
  assert(popup.card.count>0&&popup.card.count<=POPUP_ITEMS&&popup.card.box.h<=H);
  gradient();draw_modal();
  assert(clip_x==0&&clip_y==0&&clip_right==W&&clip_bottom==H&&hud_ink_alpha==16);
 }
 setup(30000);screen=SCR_BT;bt_page_dialog=0;popup_prepare(30000);popup_settle_for_capture(30000);
 bt_page_dialog=-1;nbtdev=0;popup_prepare(30001);
 assert(!strcmp(popup.card.item[0].text,"Saved phone"));hud_capture_ms=30161;gradient();draw_modal(); /* no dangling device pointer */
 setup(30500);apply_palette(0);modal=MOD_ROUTE_POINT;popup_prepare(30500);popup_settle_for_capture(30500);
 gradient();draw_modal();assert(fb[140*W+149]==C_ACTIVE); /* same light-theme menu blue */
 setup(31000);screen=SCR_KEY;kbd_picker=1;kbd_pick=8;popup_prepare(31000);popup_settle_for_capture(31000);
 assert(popup.card.box.h==222&&popup.card.count==8&&popup.card.selected>=1);gradient();draw_modal();
 assert(popup_requested_key()==POPUP_KEYBOARD);
 kbd_pick++;popup_prepare(31001);assert(popup.content_at==31001); /* list scroll crossfade */

 /* Popup-only controls preserve the clean map; legal actions remain immediate. */
 setup(40000);freecam=1;render_background_dirty=0;key_handle(K_ENTER);
 assert(modal==MOD_ROUTE_POINT&&!render_background_dirty);
 key_handle(K_RIGHT);assert(modal_choice==1&&!render_background_dirty);
 key_handle(K_BACK);assert(modal==MOD_NONE&&!render_background_dirty);
 screen=SCR_KEY;kbd_picker=1;kbd_pick=2;render_background_dirty=0;
 key_handle(K_DOWN);assert(kbd_pick==3&&!render_background_dirty);
 key_handle(K_ENTER);assert(!kbd_picker&&render_background_dirty);

 /* Popup frame requests use the pill clock without waiting for a map frame. */
 setup(50000);at(50000,1);at(50500,1);renderer.map_at=60000;render_background_dirty=0;
 modal=MOD_ROUTE_POINT;at(50501,0);u32 presents=render_stats.presents,maps=render_stats.maps;
 for(u32 t=50502;t<50840;t++)at(t,0);
 assert(render_stats.presents>=presents+4&&render_stats.maps==maps&&!popup_animating(50839));
 modal=MOD_NONE;at(50841,0);assert(renderer.ui_period==67);
 for(u32 t=50842;t<51200;t++)at(t,0);
 assert(!popup_animating(51200)&&popup_pose(51200).a==0);
 /* Menu dismissal must request the final clear without a new background. */
 setup(52000);screen=SCR_MENU;modal=MOD_CANCEL_ROUTE;
 for(u32 t=52000;t<52500;t++)at(t,t==52000);
 modal=MOD_NONE;u32 menu_maps=render_stats.maps;
 for(u32 t=52500;t<53000;t++)at(t,0);
 assert(!popup_animating(53000)&&popup_pose(53000).a==0&&popup.presented_at>=52820&&render_stats.maps==menu_maps);
 /* Route calculation caps popup, HUD and map together at five presentations. */
 setup(60000);route_status=R_CALC;modal=MOD_SOURCE_PICK;dest_pick_action=DEST_PICK_ADD_STOP;
 for(u32 t=60000;t<61000;t++)at(t,t%100==0);
 assert(render_stats.presents==5&&render_stats.maps==5);
 /* A clipped child glass surface cannot leak outside an animated parent. */
 setup(70000);gradient();u16 original[FB_PIX];memcpy(original,fb,sizeof original);
 clip_x=100;clip_y=100;clip_right=140;clip_bottom=130;
 hud_glass_surface((struct HudBox){80,80,100,80,256},C_PANEL,C_BORDER,12,0);
 for(int y=0;y<H;y++)for(int x=0;x<W;x++)if(x<100||x>=140||y<100||y>=130)assert(fb[y*W+x]==original[y*W+x]);
 clip_x=clip_y=0;clip_right=W;clip_bottom=H;
 /* Unsigned clock rollover retains the same entrance duration. */
 setup(0xffffff80u);modal=MOD_ROUTE_POINT;popup_prepare(0xffffff80u);
 assert(popup_pose(0x20u).a>0&&popup_pose(0x20u).a<256);
 hud_capture_ms=0xc0u;draw_modal();assert(popup_pose(0xc0u).a==256&&!popup_animating(0xc0u));
 puts("Popups: all families, glass, entrance/exit/resize/reversal, animated focus/text, retained close data, input, clipping, independent 15 Hz scheduling and 5 Hz routing cap passed.");
}
