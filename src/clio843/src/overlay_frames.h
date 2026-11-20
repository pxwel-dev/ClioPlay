/* Prepare the real overlay compositor against a finished map. Paint times are
 * sampled ahead, but preparation never advances the live menu or popup. */
enum {OVERLAY_MAX_TRANSITION_MS=BT_PRESS_MS,
 OVERLAY_FRAME_COUNT=(OVERLAY_MAX_TRANSITION_MS+FRAME_UI_MS-1u)/FRAME_UI_MS};
struct OverlayFrame {
 u16 pixels[FB_PIX];
 u8 rows[H];
 u32 at,diversion,suggestion;
 struct SavedPlace place;
};
static struct {
 struct OverlayFrame *frames;
 unsigned built,shown,batch_count;
 u32 model,due,start,paint_ms,batch_ms,attempt_end,attempt_input;
 int follow,alloc_failed;
 int batch_page,batch_modal,batch_camera;
} overlay;

static void render_paint_overlay(u32 now,u8*rows){
 overlay_rows=rows;
 hud_glass_frame_begin();
 menu_paint(now);
 popup_paint(now);
 hud_glass_frame_end();
 overlay_rows=0;
}

/* Short lookahead needs the current model because its clock stays live.
 * Include the raw menu inputs as well as the prepared HUD and popup views. */
static u32 overlay_model(void){
 u32 hash=2166136261u;
 const int context[]={screen,theme_resolved,pref.language,route_status,freecam,
  bt_control_mode,bt_controls_available(),hud_speed_over,hud_speed_from,
  traffic_offer_valid(),mob_suggest_valid,hw_bluetooth_enabled,
  sheet_page,sheet_old_page,sheet_focus,sheet_old_focus,places_tab,sheet_old_tab,
  bottom_from,bottom_to,modal,modal_choice};
 hash=mob_hash(hash,context,sizeof(context));
#define OVERLAY_HASH(value) hash=mob_hash(hash,&(value),sizeof(value))
 OVERLAY_HASH(overlay_input_serial);OVERLAY_HASH(hud_glass_background);
 OVERLAY_HASH(hud_tween);OVERLAY_HASH(hud_status_layout);OVERLAY_HASH(hud_progress_at);
 OVERLAY_HASH(hud_playback);OVERLAY_HASH(bt_transport_press);OVERLAY_HASH(hud_speed_at);
 OVERLAY_HASH(traffic_offer_serial);OVERLAY_HASH(mob_suggest_serial);
 if(sheet_page||sheet_old_page){
  OVERLAY_HASH(sheet_from_box);OVERLAY_HASH(sheet_to_box);
  OVERLAY_HASH(menu_at);OVERLAY_HASH(focus_at);OVERLAY_HASH(bottom_at);OVERLAY_HASH(menu_interaction_at);
  OVERLAY_HASH(pref);OVERLAY_HASH(trip);OVERLAY_HASH(traffic_saving);
  const u32 remaining[]={route_path_m(),traffic_remaining_cost(),atlas_future_cost,
   (u32)menu_suggestion_current(),(u32)menu_diversion_current()};
  OVERLAY_HASH(remaining);
  if(sheet_page==P_PLACES||sheet_old_page==P_PLACES||sheet_page==P_DEST||sheet_old_page==P_DEST){
   OVERLAY_HASH(nrecents);OVERLAY_HASH(nfaves);
   hash=mob_hash(hash,recents,(unsigned)nrecents*sizeof(*recents));
   hash=mob_hash(hash,faves,(unsigned)nfaves*sizeof(*faves));
  }
  if(screen==SCR_BT||sheet_old_page==P_PHONE||sheet_old_page==P_PHONE_ACTIONS||sheet_old_page==P_PAIR){
   OVERLAY_HASH(nbtdev);OVERLAY_HASH(btui.status);OVERLAY_HASH(nbtname_cache);
   hash=mob_hash(hash,btdev,(unsigned)nbtdev*sizeof(*btdev));
   hash=mob_hash(hash,btname_cache,(unsigned)nbtname_cache*sizeof(*btname_cache));
  }
  if(screen==SCR_KEY||sheet_old_page==P_NAME||sheet_old_page==P_LANG){
   OVERLAY_HASH(kbd_text);OVERLAY_HASH(kbd_language);OVERLAY_HASH(kbd_pick);
   OVERLAY_HASH(pk_focus);OVERLAY_HASH(pk_stage);OVERLAY_HASH(pk_page);
   OVERLAY_HASH(pk_symbols);OVERLAY_HASH(pk_syllable);OVERLAY_HASH(pk_kana);
  }
  if(screen==SCR_COORDINATES||sheet_old_page==P_COORD||sheet_old_page==P_REVIEW) {
   OVERLAY_HASH(coordinates);
  }
 }
 if(popup.from.a||popup.to.a){
  OVERLAY_HASH(popup.card);OVERLAY_HASH(popup.previous);
  OVERLAY_HASH(popup.from);OVERLAY_HASH(popup.to);
  OVERLAY_HASH(popup.focus_from);OVERLAY_HASH(popup.focus_to);
  OVERLAY_HASH(popup.at);OVERLAY_HASH(popup.content_at);OVERLAY_HASH(popup.focus_at);
 }
#undef OVERLAY_HASH
 return hash;
}
static void overlay_invalidate(void){overlay.built=overlay.shown=overlay.batch_count=0;overlay.follow=0;}
static void overlay_release(void){
 if(overlay.frames)s_munmap(overlay.frames,OVERLAY_FRAME_COUNT*sizeof(*overlay.frames));
 overlay.frames=0;overlay_invalidate();
}
static int overlay_eligible(void){
 return renderer.valid&&!renderer.pending&&!renderer.building&&renderer.theme==theme_resolved&&
  renderer.page==screen&&route_status!=R_CALC&&
  !boot_fade.image&&!renderer.last_fade&&!explore.frames&&!freecam_transition_active()&&!renderer.camera_pending;
}
static int overlay_batch_current(void){
 return overlay_eligible()&&!renderer.moving&&overlay.attempt_input==overlay_input_serial&&
  overlay.batch_page==sheet_page&&overlay.batch_modal==modal&&overlay.batch_camera==camera_visible;
}
static void overlay_cancel_batch(u32 now){
 overlay_invalidate();renderer.ui.next=now;renderer.model_pending=1;
 render_stats.overlay_aborts++;
}
static int overlay_has_work(u32 now){
 if(!overlay_eligible()||!render_transition_active(now)||overlay.batch_count||overlay.alloc_failed||overlay.built>=OVERLAY_FRAME_COUNT||
    (s32)(renderer.ui.next-now)<=(s32)(FRAME_GUARD_MS+overlay.paint_ms))return 0;
 u32 sample=overlay.built?overlay.start+overlay.built*FRAME_UI_MS:renderer.ui.next;
 return !renderer.moving||!frame_due(sample,renderer.map_at);
}
static int overlay_allocate(void){
 if(!overlay.frames){
  overlay.frames=s_mmap2(0,OVERLAY_FRAME_COUNT*sizeof(*overlay.frames),
    PROT_READ_|PROT_WRITE_,MAP_PRIVATE_|MAP_ANONYMOUS_,-1,0);
  if(!overlay.frames)overlay.alloc_failed=1;
 }
 return overlay.frames!=0;
}
static void overlay_build(u32 sample){
 struct OverlayFrame*f=&overlay.frames[overlay.built];
 f->at=sample;
 render_copy(f->pixels,*render_front);
 memzero(f->rows,sizeof(f->rows));
 u16(*saved)[FB_PIX]=render_fb;
 int x=clip_x,y=clip_y,right=clip_right,bottom=clip_bottom,ink=hud_ink_alpha;
 u32 diversion=hud_diversion_invited,suggestion=hud_suggestion_invited;
 struct SavedPlace place=hud_suggestion_place;
 render_fb=&f->pixels;clip_x=clip_y=0;clip_right=W;clip_bottom=H;hud_ink_alpha=16;
 render_paint_overlay(f->at,f->rows);
 f->diversion=hud_diversion_invited;f->suggestion=hud_suggestion_invited;
 f->place=hud_suggestion_place;
 hud_diversion_invited=diversion;hud_suggestion_invited=suggestion;hud_suggestion_place=place;
 render_fb=saved;clip_x=x;clip_y=y;clip_right=right;clip_bottom=bottom;hud_ink_alpha=ink;
 overlay.built++;
}

static void overlay_prepare_one(u32 now){
 if(overlay.batch_count)return;
 if(!overlay_eligible()||!render_transition_active(now)){overlay_release();return;}
 if(!overlay_has_work(now))return;
 u32 model=overlay_model();
 if(overlay.built&&(overlay.due!=renderer.ui.next||overlay.model!=model)){
  overlay_invalidate();
  if(!overlay_has_work(now))return;
 }
 if(!overlay_allocate())return;
 if(!overlay.built){
  overlay.model=model;overlay.start=overlay.due=renderer.ui.next;
 }
 overlay_build(overlay.start+overlay.built*FRAME_UI_MS);
 /* Reserve time for the next live presentation on moving maps. */
 u32 elapsed=hud_now_ms()-now;
 overlay.paint_ms=elapsed<FRAME_UI_MS?elapsed:FRAME_UI_MS-FRAME_GUARD_MS-1u;
}

static void overlay_extend(u32 now,u32 began,u32 duration,u32*remaining){
 u32 age=now-began;
 if(age<duration&&duration-age>*remaining)*remaining=duration-age;
}
static u32 overlay_transition_remaining(u32 now){
 u32 remaining=0;
 for(int id=0;id<HC_COUNT;id++){
  const struct HudTween*t=&hud_tween[id];
  if(!t->seen||(!t->from.a&&!t->target.box.a))continue;
  overlay_extend(now,t->position_at,320,&remaining);
  overlay_extend(now,t->fast_content_at,220,&remaining);
 }
 if(hud_tween[HC_STATUS].from.a||hud_tween[HC_STATUS].target.box.a)
  overlay_extend(now,hud_status_layout.at,320,&remaining);
 overlay_extend(now,hud_speed_at,360,&remaining);
 if(hud_pulse_live(&bt_transport_press,now))
  overlay_extend(now,bt_transport_press.changed,BT_PRESS_MS,&remaining);
 if(sheet_page||sheet_from_box.a){
  overlay_extend(now,menu_at,320,&remaining);
  overlay_extend(now,bottom_at,320,&remaining);
  overlay_extend(now,menu_interaction_at,160,&remaining);
  overlay_extend(now,focus_at,160,&remaining);
 }
 if(popup.from.a||popup.to.a){
  overlay_extend(now,popup.at,320,&remaining);
  if(popup.to.a){
   overlay_extend(now,popup.content_at,320,&remaining);
   overlay_extend(now,popup.focus_at,160,&remaining);
  }
 }
 return remaining;
}

/* On a held map, finish a finite transition before presenting its first
 * frame. Build one sample per scheduler iteration so input/model service
 * can interrupt preparation. A moving map keeps using short lookahead. */
static int overlay_batch_step(u32 now,int interrupted){
 if(overlay.batch_count){
  if(!overlay_batch_current()){
   overlay_cancel_batch(now);
   return 0;
  }
 }else{
  if(interrupted||!overlay_eligible()||renderer.moving||overlay.alloc_failed||!render_transition_active(now))return 0;
  u32 remaining=overlay_transition_remaining(now);
  if(!remaining||remaining>OVERLAY_MAX_TRANSITION_MS)return 0;
  u32 end=now+remaining;
  if(overlay.attempt_end==end&&overlay.attempt_input==overlay_input_serial)return 0;
  overlay.attempt_end=end;overlay.attempt_input=overlay_input_serial;
  if(!overlay_allocate())return 0;
  overlay_invalidate();
  overlay.start=now;
  overlay.batch_page=sheet_page;overlay.batch_modal=modal;overlay.batch_camera=camera_visible;
  overlay.batch_ms=remaining;
  overlay.batch_count=(remaining+FRAME_UI_MS-1u)/FRAME_UI_MS;
 }
 if(overlay.built==overlay.batch_count){
  renderer.ui.next=overlay.due;
  return 0;
 }
 u32 elapsed=(overlay.built+1u)*FRAME_UI_MS;
 if(elapsed>overlay.batch_ms)elapsed=overlay.batch_ms;
 overlay_build(overlay.start+elapsed);
 if(overlay.built==overlay.batch_count){
  u32 done=hud_now_ms(),delay=done-overlay.start;
  overlay.due=done;
  if(delay>render_stats.overlay_prep_ms)render_stats.overlay_prep_ms=delay;
 }
 return 1;
}
static const struct OverlayFrame*overlay_take(int full){
 if(!overlay.built)return 0;
 if(full||!overlay_eligible()||overlay.due!=renderer.ui.next||overlay.shown==overlay.built||
    (overlay.batch_count?overlay.built!=overlay.batch_count:overlay.model!=overlay_model())){
  overlay_invalidate();return 0;
 }
 const struct OverlayFrame*f=&overlay.frames[overlay.shown++];
 unsigned count=overlay.batch_count?overlay.batch_count:OVERLAY_FRAME_COUNT;
 if(overlay.shown==count)overlay_invalidate();
 else overlay.follow=1;
 render_stats.overlay_frames++;
 return f;
}
