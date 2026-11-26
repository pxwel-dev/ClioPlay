/* Finished background, background under construction, and displayed composite.
 * A background yield can present only the finished image, never partial roads.
 * Pill/camera clocks are independent; a rotation never resets HUD state. */
static u16 render_maps[2][FB_PIX] __attribute__((aligned(4)));
static u16 (*render_front)[FB_PIX]=&render_maps[0],(*render_work)[FB_PIX]=&render_maps[1];
struct RenderMapScene {
 int zoom,close_road_hud;
 u32 state[23];
};
static struct {
 struct FrameClock ui;
 struct RenderMapScene scene;
 u32 map_at,check_at,present_at;
 int valid,page,theme,building,pending,model_pending,initialized,last_route,camera_pending;
 int last_fade,moving;
 u8 last_overlay[H];
 u32 map_period,ui_period;
} renderer;
static struct {
 u32 at,presents,maps,rotations,late,max_late,max_present_ms,max_map_ms,route_ms;
 u32 fb_partial,fb_bytes,max_copy_ms,max_flush_ms,explore_prep_ms,overlay_frames,overlay_prep_ms,overlay_aborts;
} render_stats;
/* Align a shared halfword offset before word bursts. Keep a scalar fallback
 * for mismatched alignments; the RGB565 globals need only two-byte alignment. */
typedef u32 FrameWord __attribute__((__may_alias__));
static void render_copy_pixels(u16*to,const u16*from,unsigned count){
 if(((unsigned long)to^(unsigned long)from)&3u){for(unsigned i=0;i<count;i++)to[i]=from[i];return;}
 if((unsigned long)to&3u){*to++=*from++;count--;}
 FrameWord*d=(FrameWord*)to;const FrameWord*s=(const FrameWord*)from;
 unsigned i=0,words=count/2;
 for(;i+8<=words;i+=8){
  FrameWord a=s[i],b=s[i+1],c=s[i+2],e=s[i+3],f=s[i+4],g=s[i+5],h=s[i+6],j=s[i+7];
  d[i]=a;d[i+1]=b;d[i+2]=c;d[i+3]=e;d[i+4]=f;d[i+5]=g;d[i+6]=h;d[i+7]=j;
 }
 for(;i<words;i++)d[i]=s[i];
 if(count&1u)to[count-1]=from[count-1];
}
static void render_copy(u16*to,const u16*from){render_copy_pixels(to,from,FB_PIX);}

struct FrameBand {int first,end;};
static int frame_band_next(const u8*rows,int from,struct FrameBand*band){
 while(from<H&&!rows[from])from++;
 if(from==H)return 0;
 band->first=from;
 while(from<H&&rows[from])from++;
 band->end=from;
 return 1;
}
static void render_restore_rows(void){
 struct FrameBand band;
 for(int row=0;frame_band_next(renderer.last_overlay,row,&band);row=band.end){
  unsigned start=(unsigned)band.first*W;
  render_copy_pixels(fb+start,*render_front+start,(unsigned)(band.end-band.first)*W);
 }
}

static int frame_fb_fd=-1,frame_fb_ready=0,frame_fb_verified=0,frame_fb_legacy=0;

static int frame_fb_write(int first,int end){
 unsigned offset=(unsigned)first*W*sizeof(u16),bytes=(unsigned)(end-first)*W*sizeof(u16);
 if(s_lseek(frame_fb_fd,(long)offset,SEEK_SET_)!=(long)offset)return 0;
 long written=s_write(frame_fb_fd,fb+first*W,bytes);
 if(written>0)render_stats.fb_bytes+=(u32)written;
 return written==(long)bytes;
}
/* Check the full first partial update, including untouched rows. A driver
 * which ignores the seek or cannot verify readback uses the legacy write. */
static int frame_fb_check(void){
 u16 pixels[2048];
 if(s_lseek(frame_fb_fd,0,SEEK_SET_)!=0)return 0;
 for(unsigned i=0;i<FB_PIX;i+=2048u){
  unsigned n=FB_PIX-i;if(n>2048u)n=2048u;
  if(s_read(frame_fb_fd,pixels,n*sizeof(*pixels))!=(long)(n*sizeof(*pixels)))return 0;
  for(unsigned j=0;j<n;j++)if(pixels[j]!=fb[i+j])return 0;
 }
 return 1;
}
static void frame_fb_present(int full,const u8*rows){
 if(!frame_fb_legacy){
  if(frame_fb_fd<0)frame_fb_fd=(int)s_open(FB_PATH,O_RDWR_|O_CREAT_,0666);
  if(frame_fb_fd>=0){
   if(!full&&frame_fb_ready){
    struct FrameBand bands[8],b;
    unsigned n=0,coverage=0;
    for(int row=0;frame_band_next(rows,row,&b);row=b.end){
     if(n==sizeof(bands)/sizeof(bands[0])){full=1;break;}
     bands[n++]=b;
     coverage+=(unsigned)(b.end-b.first);
    }
    if(!n)return;
    /* Wide coverage costs nearly a full transfer. Use a single write. */
    if(!full&&coverage<=H*3u/4u){
     unsigned i=0;
     for(;i<n;i++)if(!frame_fb_write(bands[i].first,bands[i].end))break;
     if(i==n&&(frame_fb_verified||frame_fb_check())){
      frame_fb_verified=1;render_stats.fb_partial++;return;
     }
     /* A failed band may have reached the display. Restore the whole frame. */
    }else if(frame_fb_write(0,H))return;
   }else if(frame_fb_write(0,H)){frame_fb_ready=1;return;}
   s_close(frame_fb_fd);frame_fb_fd=-1;
  }
  frame_fb_legacy=1;
 }
 frame_fb_ready=0;
 if(fb_flush())render_stats.fb_bytes+=sizeof(fb);
}

/* Keep labels steady through HUD animation. Close views refresh once when the
 * current street moves between the temporary HUD name and the map labels. */
static struct RenderMapScene render_map_scene(void){
 int z=active_zoom(),route=route_status==R_READY,offer=traffic_offer_valid(),cam_m=camera_marker_active();
 return (struct RenderMapScene){z,z>6&&crop_road_hud_shown(hud_now_ms()),{
  (u32)freecam,freecam?(u32)cam_lat:0,freecam?(u32)cam_lon:0,
  (u32)gps.fix,(u32)track_valid,(u32)display_course_valid,(u32)mapok,(u32)mapfmt,
  (u32)route,route?(u32)route_n:0,route?(u32)route_head:0,
  route?traffic_route_revision:0,route?(u32)nav_man_idx:0,route?(u32)nav_man_type:0,
  (u32)offer,offer?traffic_offer_serial:0,offer?traffic_saving:0,offer?(u32)pref.language:0,
  (u32)visual_snap.dlat4,(u32)visual_snap.dlon4,
  (u32)cam_m,cam_m?camera_current.id:0,cam_m?(u32)(camera_average|(camera_is_overspeed()<<1)):0
 }};
}
static int render_same_scene(const struct RenderMapScene*a,const struct RenderMapScene*b){
 if(a->zoom!=b->zoom||a->close_road_hud!=b->close_road_hud)return 0;
 for(unsigned i=0;i<sizeof(a->state)/sizeof(a->state[0]);i++)if(a->state[i]!=b->state[i])return 0;
 return 1;
}

#include "explore_frames.h"

static int render_transition_active(u32 now){
 if(boot_fade.image||renderer.camera_pending||hud_menu_animating(now)||popup_animating(now)||menu_animating(now))return 1;
 if(screen==SCR_MAP||bottom_pose(now)>0){
  for(int id=HC_SPEED;id<HC_COUNT;id++){
   struct HudTween*t=&hud_tween[id];if(!t->seen||(!t->from.a&&!t->target.box.a))continue;
   if(now-t->position_at<320u||hud_status_frame_at-t->position_at<320u||
      now-t->fast_content_at<220u||hud_status_frame_at-t->fast_content_at<220u)return 1;
  }
  if(hud_speed_over||now-hud_speed_at<360u||hud_status_frame_at-hud_speed_at<360u||
     hud_camera_over||now-hud_camera_at<360u||hud_status_frame_at-hud_camera_at<360u||
     (freecam_transition_active()&&!explore_preparing()))return 1;
  if(hud_pulse_live(&bt_transport_press,now))return 1;
 }
 return 0;
}
static int render_animation_active(u32 now){
 if(render_transition_active(now)||
    (screen==SCR_MAP&&bt_control_mode&&hud_playback.valid&&(hud_playback.playing||now-hud_playback.at<320u))||
    (screen==SCR_MAP&&hud_camera_bar.valid&&now-hud_camera_bar.at<320u))return 1;
 /* Value-only updates share the HUD cadence until their last fade frame.
  * An idle display still holds its last image without repainting it. */
 int cards=screen==SCR_MAP||bottom_pose(now)>0?HC_COUNT:HC_SPEED;
 for(int id=0;id<cards;id++){
  const struct HudTween*t=&hud_tween[id];
  if(t->target.box.a&&!frame_due(menu_presented_at,t->content_at+220u))return 1;
 }
 return 0;
}
static void render_prepare(u32 now){
 hud_prepare(active_zoom(),now,1);hud_driver_prepare(now);menu_prepare(now);
 if(menu_pending)renderer.model_pending=1;
}
#include "overlay_frames.h"

static void render_present(void){
 if(!renderer.valid||renderer.page!=screen||renderer.theme!=theme_resolved)return;
 u32 begin=hud_now_ms();
 int ox=clip_x,oy=clip_y,orr=clip_right,ob=clip_bottom,oa=hud_ink_alpha;
 render_fb=&display_fb;clip_x=clip_y=0;clip_right=W;clip_bottom=H;hud_ink_alpha=16;
 int fading=boot_fade.image!=0;
 int full=renderer.pending||fading||renderer.last_fade;
 if(full)render_copy(fb,*render_front);
 else render_restore_rows();
 /* Read the presentation clock after restoring the backdrop. */
 u32 now=hud_now_ms();renderer.present_at=now;
 const struct OverlayFrame*prepared=overlay.batch_count?overlay_take(full):0;
 int batched=prepared!=0;
 if(!batched){
  render_prepare(now);popup_prepare(now);
  prepared=overlay_take(full);
 }
 u8 current[H]={0};
 u32 paint_at=now;
 if(prepared){
  struct FrameBand band;
  for(int row=0;frame_band_next(prepared->rows,row,&band);row=band.end){
   unsigned start=(unsigned)band.first*W;
   render_copy_pixels(fb+start,prepared->pixels+start,(unsigned)(band.end-band.first)*W);
  }
  for(int row=0;row<H;row++)current[row]=prepared->rows[row];
  hud_diversion_invited=prepared->diversion;hud_suggestion_invited=prepared->suggestion;
  hud_parking_invited=prepared->parking;hud_stop_invited=prepared->stop;
  hud_suggestion_place=prepared->place;paint_at=prepared->at;
 }else render_paint_overlay(now,current);
 menu_note_presented(paint_at);popup.presented_at=paint_at;
 boot_fade_apply(now);
 u8 dirty[H];
 for(int row=0;row<H;row++){
  dirty[row]=current[row]|renderer.last_overlay[row];
  renderer.last_overlay[row]=current[row];
 }
 renderer.last_fade=fading;
 u32 flush_at=pref.debug?hud_now_ms():0;
 frame_fb_present(full,dirty);
 clip_x=ox;clip_y=oy;clip_right=orr;clip_bottom=ob;hud_ink_alpha=oa;
 renderer.pending=0;
 if(!batched)renderer.model_pending=menu_pending=0;
 render_stats.presents++;
 u32 done=hud_now_ms(),elapsed=done-begin;
 if(elapsed>render_stats.max_present_ms)render_stats.max_present_ms=elapsed;
 if(pref.debug){
  u32 copy=now-begin,flush=done-flush_at;
  if(copy>render_stats.max_copy_ms)render_stats.max_copy_ms=copy;
  if(flush>render_stats.max_flush_ms)render_stats.max_flush_ms=flush;
 }
 if(frame_due(now,renderer.ui.next)&&now-renderer.ui.next>8u){
  u32 late=now-renderer.ui.next;render_stats.late++;if(late>render_stats.max_late)render_stats.max_late=late;
 }
}
static void render_checkpoint(void){
 /* During calculation, spend this time finishing the one 5 Hz frame. */
 if(route_status==R_CALC||overlay.batch_count||!renderer.building||!renderer.valid||renderer.page!=screen||renderer.theme!=theme_resolved)return;
 u32 now=hud_now_ms();if(!frame_due(now,renderer.check_at))return;
 renderer.check_at=now+2u;
 if(!frame_due(now,renderer.ui.next))return;
 if(!renderer.model_pending&&!render_animation_active(now))return;
 u16(*saved)[FB_PIX]=render_fb;
 struct ExploreView prepared;
 if(explore_live){
  prepared=explore_view();
  explore_set_view(*explore_live);
 }
 render_present();
 frame_advance_ui(&renderer.ui,hud_now_ms(),renderer.ui_period);
 if(explore_live)explore_set_view(prepared);
 render_fb=saved;
}
static void render_background(void){
 u32 begin=hud_now_ms();renderer.building=1;render_fb=render_work;
 draw_map_background();
 renderer.building=0;render_fb=&display_fb;
 u16(*swap)[FB_PIX]=render_front;render_front=render_work;render_work=swap;
 hud_glass_background_changed();
 renderer.valid=renderer.pending=1;renderer.camera_pending=0;renderer.page=screen;renderer.theme=theme_resolved;render_background_dirty=0;
 u32 elapsed=hud_now_ms()-begin;if(elapsed>render_stats.max_map_ms)render_stats.max_map_ms=elapsed;
}
static void render_runtime_reset(u32 now){
 explore_dispose();overlay_release();overlay.alloc_failed=0;overlay.paint_ms=0;overlay.attempt_end=0;overlay.attempt_input=overlay_input_serial-1u;
 explore.attempted_serial=freecam_anim_serial-1u;
 memzero(&renderer,sizeof(renderer));memzero(&render_stats,sizeof(render_stats));
 frame_fb_ready=0;
 hud_glass_background_changed();popup_reset();
 renderer.initialized=1;renderer.page=-1;renderer.theme=-1;renderer.map_at=now;renderer.ui.next=now;
 renderer.last_route=route_status;renderer.map_period=route_status==R_CALC?FRAME_CALC_MAP_MS:FRAME_MAP_MS;
 renderer.ui_period=frame_ui_period(route_status==R_CALC);renderer.present_at=now-FRAME_MAP_MS;render_background_dirty=1;render_fb=&display_fb;
}
static void render_runtime_step(u32 now,int model_changed){
 if(!renderer.initialized)render_runtime_reset(now);
 int calculating=route_status==R_CALC;
 int route_changed=renderer.last_route!=route_status;
 if(calculating!=(renderer.last_route==R_CALC)){
  renderer.ui.next=now;renderer.model_pending=1;
 }
 renderer.last_route=route_status;
 renderer.ui_period=frame_ui_period(calculating);
 renderer.model_pending|=route_changed||model_changed||render_background_dirty;
 u32 period=calculating?FRAME_CALC_MAP_MS:FRAME_MAP_MS;
 if(period!=renderer.map_period){renderer.map_period=period;renderer.map_at=now;}
 int map_due=frame_due(now,renderer.map_at);
 /* Input/model services still run while the whole display is capped at 5 Hz. */
 if(calculating&&!frame_due(now,renderer.ui.next))return;
 int batch_interrupted=overlay.batch_count&&(route_changed||!overlay_batch_current());
 if(batch_interrupted)overlay_cancel_batch(now);
 /* Model services keep running. Compose their routine updates after the
  * finite sequence so a clock tick or phone position cannot cut it short. */
 if(!overlay.batch_count){
  popup_prepare(now);
  if(renderer.model_pending||!renderer.valid||renderer.page!=screen||renderer.theme!=theme_resolved)render_prepare(now);
 }
 if(renderer.valid&&renderer.theme==theme_resolved)renderer.page=screen;
 struct RenderMapScene scene=render_map_scene();
 /* A rotating map cannot reuse prepared overlay pixels. Reclaim their
  * buffer before allocating the camera's sequence. */
 if(overlay.frames&&freecam_transition_active())overlay_release();
 int queued=explore_sync(now,calculating,&scene);
 if(queued)render_background_dirty=0;
 else{
  int was_rotation=freecam_transition_active();
  freecam_orientation_anim_update(now);
  if(was_rotation)renderer.camera_pending=1;
 }
 int ui_due=frame_due(now,renderer.ui.next);
 int moving=gps.fix&&gps.mph10>=5; /* Under half a mile per hour, hold the parked view. */
 int snap_settling=!freecam&&active_zoom()>=6&&visual_snap_settling();
 int invalid=!renderer.valid||renderer.theme!=theme_resolved;
 int camera_due=renderer.camera_pending&&ui_due;
 int scene_changed=!render_same_scene(&renderer.scene,&scene)||renderer.moving!=moving;
 int live_map_due=(moving||snap_settling)&&(map_due||render_background_dirty);
 int showing_queued=queued&&!explore_preparing()&&ui_due;
 if(showing_queued){
  explore_show_frame();
  renderer.scene=scene;renderer.moving=moving;
 }else if(!queued&&(invalid||((!renderer.camera_pending||ui_due)&&
   (scene_changed||live_map_due||camera_due)))){
  render_background();
  renderer.scene=render_map_scene();renderer.moving=moving;
  if(camera_due)render_stats.rotations++;else render_stats.maps++;
 }else if(!queued&&!renderer.camera_pending&&!moving&&!snap_settling)render_background_dirty=0;
 if(map_due)renderer.map_at=frame_after(renderer.map_at,now,period);
 now=hud_now_ms();
 if(overlay_batch_step(now,route_changed||batch_interrupted))return;
 /* If a camera frame becomes due during this step, present it with
  * its background on the next step instead of redrawing the old pose. */
 if(frame_due(now,renderer.ui.next)&&(!queued||explore_preparing()||showing_queued)){
  if(overlay.batch_count||renderer.pending||renderer.model_pending||render_animation_active(now))render_present();
  if(showing_queued&&!freecam_transition_active()){
   explore_dispose();
   frame_advance_ui(&renderer.ui,hud_now_ms(),renderer.ui_period);
  }else if(showing_queued||(overlay.batch_count&&overlay.follow)){
   /* Present each prepared frame in order. A late write lengthens the
    * animation instead of dropping frames or presenting several at once. */
   renderer.ui.next=renderer.present_at+FRAME_UI_MS;
   u32 done=hud_now_ms();if(frame_due(done,renderer.ui.next))renderer.ui.next=done;
  }else frame_advance_ui(&renderer.ui,hud_now_ms(),renderer.ui_period);
  if(overlay.follow){overlay.due=renderer.ui.next;overlay.follow=0;}
 }
 if(explore_preparing())explore_build_frame();
 else overlay_prepare_one(hud_now_ms());
}
static u32 render_runtime_deadline(u32 now){
 if(route_status==R_CALC)return renderer.ui.next;
 if(explore_preparing()||(overlay.batch_count&&overlay.built<overlay.batch_count)||overlay_has_work(now))return now;
 if(overlay.batch_count)return overlay.due;
 if(explore.frames)return renderer.ui.next;
 u32 next=!renderer.camera_pending&&!freecam_transition_active()?renderer.map_at:now+FRAME_MODEL_MS;
 if(renderer.pending||renderer.model_pending||render_animation_active(now))next=frame_earlier(next,renderer.ui.next);
 return next;
}
static int render_parse_kb_field(const char*buf,const char*key,u32*out){
 if(!buf||!key||!out)return 0;
 unsigned klen=slen(key);if(!klen)return 0;
 const char*p=buf;
 while(*p){
  const char*line=p;
  while(*p&&*p!='\n')p++;
  if((unsigned)(p-line)>=klen&&starts(line,key)){
   const char*v=line+klen;
   while(v<p&&(*v==' '||*v=='\t'))v++;
   if(v<p&&*v>='0'&&*v<='9'){
    u32 n=0;
    while(v<p&&*v>='0'&&*v<='9'){
     u32 d=(u32)(*v-'0');
     if(n>(0xffffffffu-d)/10u){n=0xffffffffu;break;}
     n=n*10u+d;v++;
    }
    *out=n;return 1;
   }
  }
  if(*p=='\n')p++;
 }
 return 0;
}
static int render_read_kb_file(const char*path,const char*key,u32*out){
 long fd=s_open(path,O_RDONLY_,0);if(fd<0)return 0;
 char tmp[1536];long n=s_read((int)fd,tmp,sizeof(tmp)-1u);s_close((int)fd);
 if(n<=0)return 0;
 tmp[n]=0;
 return render_parse_kb_field(tmp,key,out);
}
static void render_profile_service(u32 now){
 if(!pref.debug){memzero(&render_stats,sizeof(render_stats));return;}
 if(!render_stats.at){render_stats.at=now;return;}
 if(now-render_stats.at<5000u)return;
 char b[448]="FRAME 5S HUD=";append_int(b,sizeof(b),(int)render_stats.presents);
 appendstr(b,sizeof(b)," MAP=");append_int(b,sizeof(b),(int)render_stats.maps);
 appendstr(b,sizeof(b)," CAMERA=");append_int(b,sizeof(b),(int)render_stats.rotations);
 appendstr(b,sizeof(b)," LATE=");append_int(b,sizeof(b),(int)render_stats.late);
 appendstr(b,sizeof(b)," MAXLATE_MS=");append_int(b,sizeof(b),(int)render_stats.max_late);
 appendstr(b,sizeof(b)," MAXHUD_MS=");append_int(b,sizeof(b),(int)render_stats.max_present_ms);
 appendstr(b,sizeof(b)," MAXBG_WALL_MS=");append_int(b,sizeof(b),(int)render_stats.max_map_ms);
 appendstr(b,sizeof(b)," ROUTE_MS=");append_int(b,sizeof(b),(int)render_stats.route_ms);
 appendstr(b,sizeof(b)," FB_PARTIAL=");append_int(b,sizeof(b),(int)render_stats.fb_partial);
 appendstr(b,sizeof(b)," FB_BYTES=");append_int(b,sizeof(b),(int)render_stats.fb_bytes);
 appendstr(b,sizeof(b)," MAXCOPY_MS=");append_int(b,sizeof(b),(int)render_stats.max_copy_ms);
 appendstr(b,sizeof(b)," MAXFLUSH_MS=");append_int(b,sizeof(b),(int)render_stats.max_flush_ms);
 appendstr(b,sizeof(b)," EXPLORE_PREP_MS=");append_int(b,sizeof(b),(int)render_stats.explore_prep_ms);
 appendstr(b,sizeof(b)," OVERLAY_READY=");append_int(b,sizeof(b),(int)render_stats.overlay_frames);
 appendstr(b,sizeof(b)," OVERLAY_PREP_MS=");append_int(b,sizeof(b),(int)render_stats.overlay_prep_ms);
 appendstr(b,sizeof(b)," OVERLAY_ABORT=");append_int(b,sizeof(b),(int)render_stats.overlay_aborts);
 u32 rss_kb=0,free_kb=0;
 if(render_read_kb_file("/proc/self/status","VmRSS:",&rss_kb)){appendstr(b,sizeof(b)," RSS_KB=");append_int(b,sizeof(b),(int)rss_kb);}
 if(render_read_kb_file("/proc/meminfo","MemFree:",&free_kb)){appendstr(b,sizeof(b)," FREE_KB=");append_int(b,sizeof(b),(int)free_kb);}
 debug_log(STATE_LOG,b);memzero(&render_stats,sizeof(render_stats));render_stats.at=now;
}
