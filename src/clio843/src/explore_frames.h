/* Camera background snapshots. The HUD is drawn live on top of each frame. */
#define EXPLORE_FRAME_COUNT ((FREECAM_ORIENT_ANIM_MS+FRAME_UI_MS-1u)/FRAME_UI_MS)

struct ExploreView {
 struct FreecamPose pose;
 s32 track_lat,track_lon;
 int track_valid,track_course,display_course,display_course_valid;
 int card_x,card_y,card_w,card_h;
};
static struct {
 u16 (*frames)[FB_PIX],(*home)[FB_PIX];
 struct FreecamPose pose[EXPLORE_FRAME_COUNT];
 struct RenderMapScene scene;
 struct ExploreView view;
 u32 attempted_serial,at;
 unsigned built,shown;
 int heading;
} explore;
/* Only set during a background draw. A cooperative HUD yield temporarily
 * restores this real camera so synthetic poses never leak into the UI. */
static const struct ExploreView *explore_live;

static struct ExploreView explore_view(void){
 return (struct ExploreView){
  freecam_current_pose(),track_lat,track_lon,track_valid,track_course,display_course,display_course_valid,
  nav_card_x,nav_card_y,nav_card_w,nav_card_h
 };
}
static void explore_set_view(struct ExploreView v){
 freecam_set_pose(v.pose);
 track_lat=v.track_lat;track_lon=v.track_lon;track_valid=v.track_valid;track_course=v.track_course;
 display_course=v.display_course;display_course_valid=v.display_course_valid;
 nav_card_x=v.card_x;nav_card_y=v.card_y;nav_card_w=v.card_w;nav_card_h=v.card_h;
}
static int explore_preparing(void){return explore.frames&&explore.built<EXPLORE_FRAME_COUNT;}
static void explore_dispose(void){
 if(!explore.frames)return;
 if(explore.shown){
  render_copy(*explore.home,*render_front);
  render_front=explore.home;
 }
 s_munmap(explore.frames,EXPLORE_FRAME_COUNT*sizeof(*explore.frames));
 explore.frames=0;freecam_buffered=0;
}
static int explore_sync(u32 now,int calculating,const struct RenderMapScene*scene){
 if(explore.frames){
  if(explore.attempted_serial!=freecam_anim_serial||!freecam_transition_active())explore_dispose();
  else if(calculating||renderer.theme!=theme_resolved||!render_same_scene(&explore.scene,scene)){
   /* A pan, zoom, route or theme change must take effect on the live map.
    * Continue the rotation from the point already shown. */
   freecam_anim_at=now-explore.shown*FRAME_UI_MS;
   explore_dispose();
  }else return 1;
 }
 if(!freecam_transition_active()||explore.attempted_serial==freecam_anim_serial)return 0;
 explore.attempted_serial=freecam_anim_serial;
 if(calculating||!renderer.valid||renderer.theme!=theme_resolved||boot_fade.image)return 0;
 explore.frames=s_mmap2(0,EXPLORE_FRAME_COUNT*sizeof(*explore.frames),
                       PROT_READ_|PROT_WRITE_,MAP_PRIVATE_|MAP_ANONYMOUS_,-1,0);
 if(!explore.frames)return 0;
 explore.home=render_front;explore.scene=*scene;explore.view=explore_view();explore.heading=map_course();
 explore.built=explore.shown=0;explore.at=now;freecam_buffered=1;renderer.camera_pending=0;
 if(gps.fix){
  explore.view.track_lat=car_lat();explore.view.track_lon=car_lon();explore.view.track_valid=1;
  explore.view.track_course=shown_course();
 }
 return 1;
}
static void explore_build_frame(void){
 unsigned i=explore.built;
 struct ExploreView live=explore_view();
 explore_live=&live;
 explore_set_view(explore.view);
 explore.pose[i]=freecam_sample_pose((i+1u)*FRAME_UI_MS,explore.heading);
 freecam_set_pose(explore.pose[i]);
 renderer.building=1;render_fb=&explore.frames[i];
 draw_map_at_zoom(explore.scene.zoom);
 renderer.building=0;render_fb=&display_fb;
 explore_set_view(live);explore_live=0;
 if(++explore.built==EXPLORE_FRAME_COUNT){
  u32 elapsed=hud_now_ms()-explore.at;
  if(elapsed>render_stats.explore_prep_ms)render_stats.explore_prep_ms=elapsed;
 }
}
static void explore_show_frame(void){
 unsigned i=explore.shown++;
 freecam_set_pose(explore.pose[i]);
 render_front=&explore.frames[i];
 hud_glass_background_changed();
 renderer.pending=1;
 render_stats.rotations++;
}
