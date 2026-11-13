/* Finished background, background under construction, and displayed composite.
 * A background yield can present only the finished image, never partial roads.
 * Pill/camera clocks are independent; a rotation never resets HUD state. */
static u16 render_maps[2][FB_PIX];
static u16 (*render_front)[FB_PIX]=&render_maps[0],(*render_work)[FB_PIX]=&render_maps[1];
static struct {
 struct FrameClock ui;
 u32 map_at,check_at,present_at;
 int valid,page,theme,building,pending,model_pending,initialized,last_route,camera_pending;
 u32 map_period,ui_period;
} renderer;
static struct {
 u32 at,presents,maps,rotations,late,max_late,max_present_ms,max_map_ms,route_ms;
} render_stats;
static void render_copy(u16*to,const u16*from){for(unsigned i=0;i<FB_PIX;i++)to[i]=from[i];}
static int render_transition_active(u32 now){
 if(renderer.camera_pending||hud_menu_animating(now))return 1;
 if(screen==SCR_MAP){
  for(int id=HC_SPEED;id<HC_COUNT;id++){
   struct HudTween*t=&hud_tween[id];if(!t->seen||(!t->from.a&&!t->target.box.a))continue;
   if(now-t->position_at<320u||hud_status_frame_at-t->position_at<320u||
      now-t->fast_content_at<220u||hud_status_frame_at-t->fast_content_at<220u)return 1;
  }
  if(hud_speed_over||now-hud_speed_at<360u||hud_status_frame_at-hud_speed_at<360u||freecam_transition_active())return 1;
  if(hud_pulse_live(&bt_transport_press,now))return 1;
 }
 return 0;
}
static int render_animation_active(u32 now){
 return render_transition_active(now)||(screen==SCR_MAP&&bt_control_mode&&hud_playback.valid&&
  (hud_playback.playing||now-hud_playback.at<320u));
}
static void render_prepare(u32 now){
 hud_prepare(active_zoom(),now,screen==SCR_MAP);
 if(screen==SCR_MAP)hud_driver_prepare(now);
}
static u32 render_ui_period(u32 now){return frame_ui_period(route_status==R_CALC,render_transition_active(now));}
static void render_sync_ui(u32 now){
 u32 period=render_ui_period(now);
 if(period<renderer.ui_period){
  u32 next=renderer.present_at+period;if(frame_due(now,next))next=now;
  renderer.ui.next=frame_earlier(renderer.ui.next,next);
 }
 renderer.ui_period=period;
}
static void render_advance_ui(u32 now){
 render_sync_ui(now);
 /* Rejoin the map clock after a transition, without delaying a finished map
  * behind an unrelated 100 ms phase. The boundary frame is at least 50 ms. */
 if(renderer.ui_period==FRAME_MAP_MS&&screen==SCR_MAP){
  renderer.ui.next=frame_after(renderer.map_at,now,FRAME_MAP_MS);
  if((s32)(renderer.ui.next-(renderer.present_at+FRAME_UI_MS))<0)renderer.ui.next=renderer.present_at+FRAME_UI_MS;
 }else frame_advance_ui(&renderer.ui,now,route_status==R_CALC,render_transition_active(now));
}
static void render_present(void){
 if(!renderer.valid||renderer.page!=screen||renderer.theme!=theme_resolved)return;
 u32 begin=hud_now_ms();
 int ox=clip_x,oy=clip_y,orr=clip_right,ob=clip_bottom,oa=hud_ink_alpha;
 render_fb=&display_fb;clip_x=clip_y=0;clip_right=W;clip_bottom=H;hud_ink_alpha=16;
 render_copy(fb,*render_front);
 /* Read the presentation clock after restoring the backdrop. */
 u32 now=hud_now_ms();render_prepare(now);renderer.present_at=now;
 hud_draw_all(now,screen==SCR_MAP);
 if(screen!=SCR_MAP)hud_draw_page_title();
 draw_modal();fb_flush();
 clip_x=ox;clip_y=oy;clip_right=orr;clip_bottom=ob;hud_ink_alpha=oa;
 renderer.pending=renderer.model_pending=0;render_stats.presents++;
 u32 elapsed=hud_now_ms()-begin;if(elapsed>render_stats.max_present_ms)render_stats.max_present_ms=elapsed;
 if(frame_due(now,renderer.ui.next)&&now-renderer.ui.next>8u){
  u32 late=now-renderer.ui.next;render_stats.late++;if(late>render_stats.max_late)render_stats.max_late=late;
 }
}
static void render_checkpoint(void){
 /* During calculation, spend this time finishing the one 5 Hz frame. */
 if(route_status==R_CALC||!renderer.building||!renderer.valid||renderer.page!=screen||renderer.theme!=theme_resolved)return;
 u32 now=hud_now_ms();if(!frame_due(now,renderer.check_at))return;
 renderer.check_at=now+2u;
 if(!frame_due(now,renderer.ui.next))return;
 if(!renderer.model_pending&&!render_animation_active(now))return;
 u16(*saved)[FB_PIX]=render_fb;
 render_present();render_fb=saved;
 render_advance_ui(hud_now_ms());
}
static void render_background(void){
 u32 begin=hud_now_ms();renderer.building=1;render_fb=render_work;
 if(screen==SCR_MAP)draw_map_background();
 else if(screen==SCR_MENU)draw_menu();else if(screen==SCR_TRIP)draw_trip();
 else if(screen==SCR_BT)draw_bluetooth();else if(screen==SCR_KEY)draw_keyboard();else if(screen==SCR_COORDINATES)draw_coordinates();else draw_settings();
 renderer.building=0;render_fb=&display_fb;
 u16(*swap)[FB_PIX]=render_front;render_front=render_work;render_work=swap;
 renderer.valid=renderer.pending=1;renderer.camera_pending=0;renderer.page=screen;renderer.theme=theme_resolved;render_background_dirty=0;
 u32 elapsed=hud_now_ms()-begin;if(elapsed>render_stats.max_map_ms)render_stats.max_map_ms=elapsed;
}
static void render_runtime_reset(u32 now){
 memzero(&renderer,sizeof(renderer));memzero(&render_stats,sizeof(render_stats));
 renderer.initialized=1;renderer.page=-1;renderer.theme=-1;renderer.map_at=now;renderer.ui.next=now;
 renderer.last_route=route_status;renderer.map_period=route_status==R_CALC?FRAME_CALC_MAP_MS:FRAME_MAP_MS;
 renderer.ui_period=renderer.map_period;renderer.present_at=now-FRAME_MAP_MS;render_background_dirty=1;render_fb=&display_fb;
}
static void render_runtime_step(u32 now,int model_changed){
 if(!renderer.initialized)render_runtime_reset(now);
 int calculating=route_status==R_CALC;
 if(calculating!=(renderer.last_route==R_CALC)){
  /* Enter the cap immediately; restore fast presentation as soon as it ends. */
  renderer.ui.next=now;
 }
 int was_rotation=freecam_transition_active();freecam_orientation_anim_update(now);
 if(screen==SCR_MAP&&(was_rotation||freecam_transition_active()))renderer.camera_pending=1;
 renderer.model_pending|=model_changed;
 u32 period=route_status==R_CALC?FRAME_CALC_MAP_MS:FRAME_MAP_MS;
 if(period!=renderer.map_period){renderer.map_period=period;renderer.map_at=now;}
 int map_due=screen==SCR_MAP&&frame_due(now,renderer.map_at);
 int route_finished=renderer.last_route==R_CALC&&route_status!=R_CALC;
 renderer.last_route=route_status;
 /* No dirty page, new input, camera or animation can bypass the 5 Hz cap.
  * Model/input services still run normally and queue the latest state. */
 if(calculating&&!frame_due(now,renderer.ui.next))return;
 /* Observe new content on model/input changes so it can promptly promote the
  * next presentation to 20 Hz; routine values do not keep that promotion alive. */
 if(model_changed||render_background_dirty||!renderer.valid||renderer.page!=screen||renderer.theme!=theme_resolved)render_prepare(now);
 render_sync_ui(now);
 int invalid=!renderer.valid||renderer.page!=screen||renderer.theme!=theme_resolved;
 int camera_due=renderer.camera_pending&&frame_due(now,renderer.ui.next);
 /* Normal map updates continue on their clock, even during Explore. Extra
  * camera presentation passes reuse the map tile cache and never gate model
  * updates, routing, or HUD transitions on animation completion. */
 if(invalid||render_background_dirty||map_due||camera_due||route_finished){
  render_background();
  if(map_due){render_stats.maps++;renderer.map_at=frame_after(renderer.map_at,now,period);}
  else if(camera_due)render_stats.rotations++;
 }
 now=hud_now_ms();
 if(frame_due(now,renderer.ui.next)){
  if(renderer.pending||renderer.model_pending||render_animation_active(now))render_present();
  render_advance_ui(hud_now_ms());
 }
}
static u32 render_runtime_deadline(u32 now){
 if(route_status==R_CALC)return renderer.ui.next;
 u32 next=screen==SCR_MAP?renderer.map_at:now+FRAME_MODEL_MS;
 if(renderer.pending||renderer.model_pending||render_animation_active(now))next=frame_earlier(next,renderer.ui.next);
 return next;
}
static void render_profile_service(u32 now){
 if(!pref.debug){memzero(&render_stats,sizeof(render_stats));return;}
 if(!render_stats.at){render_stats.at=now;return;}
 if(now-render_stats.at<5000u)return;
 char b[240]="FRAME 5S HUD=";append_int(b,sizeof(b),(int)render_stats.presents);
 appendstr(b,sizeof(b)," MAP=");append_int(b,sizeof(b),(int)render_stats.maps);
 appendstr(b,sizeof(b)," CAMERA=");append_int(b,sizeof(b),(int)render_stats.rotations);
 appendstr(b,sizeof(b)," LATE=");append_int(b,sizeof(b),(int)render_stats.late);
 appendstr(b,sizeof(b)," MAXLATE_MS=");append_int(b,sizeof(b),(int)render_stats.max_late);
 appendstr(b,sizeof(b)," MAXHUD_MS=");append_int(b,sizeof(b),(int)render_stats.max_present_ms);
 appendstr(b,sizeof(b)," MAXBG_WALL_MS=");append_int(b,sizeof(b),(int)render_stats.max_map_ms);
 appendstr(b,sizeof(b)," ROUTE_MS=");append_int(b,sizeof(b),(int)render_stats.route_ms);
 debug_log(STATE_LOG,b);memzero(&render_stats,sizeof(render_stats));render_stats.at=now;
}
