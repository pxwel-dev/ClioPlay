/* Real compositor with a controlled clock: cadence, isolation and concurrency. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static void setup(int status,u32 now){
 hud_capture_clock=1;hud_capture_ms=now;screen=SCR_MAP;modal=MOD_NONE;pref.debug=0;pref.language=0;
 mapok=0;gps.fix=0;track_valid=1;gps.hour=12;gps.min=35;car.tempok=0;
 track_lat=track_lon=gps.lat=gps.lon=0;track_course=display_course=70;display_course_valid=1;
 trip.active=0;route_status=hud_observed_route=status;route_n=0;freecam=0;map_tilt_q8=256;freecam_anim_mode=FC_ANIM_NONE;
 bt_control_mode=0;hud_audio.kind=hud_source.kind=hud_navigation.kind=hud_connection.kind=hud_bt_feedback.kind=HUD_NONE;
 hud_reset_animations();render_runtime_reset(now);apply_palette(1);
}
static void at(u32 now,int model){hud_capture_ms=now;render_runtime_step(now,model);}
int main(void){
 struct FrameClock clock={0};unsigned frames=0;
 for(u32 t=0;t<1000;t++)if(frame_due(t,clock.next)){frames++;frame_advance_ui(&clock,t,0);}
 assert(frames==20&&clock.next==1000);
 clock=(struct FrameClock){0xffffff00u};u32 before=clock.next;frame_advance_ui(&clock,before,0);assert(clock.next-before==50);
 assert(frame_after(100,450,100)==500&&frame_after(0xfffffff0u,0x30u,100u)==0x54u);
 assert(frame_route_budget(100,101)==0&&frame_route_budget(100,110)==5&&frame_route_budget(100,104)==2);

 setup(R_IDLE,10000);
 for(u32 t=10000;t<11000;t++)at(t,t%100==0);
 assert(render_stats.maps==10&&render_stats.presents>10&&render_stats.presents<20); /* 20 Hz during entrance, idle thereafter */
 assert(render_stats.rotations==0);
 for(int i=0;i<FB_PIX;i++)assert((*render_front)[i]==C_BG); /* no pills in background */
 setup(R_CALC,10000);
 for(u32 t=10000;t<11000;t++)at(t,t%100==0);
 assert(render_stats.maps==5&&render_stats.presents==5); /* whole presentation is capped */
 renderer.building=1;hud_capture_ms=renderer.ui.next;renderer.model_pending=1;
 render_checkpoint();assert(render_stats.presents==5);renderer.building=0;
 route_rerouting=1;gps.mph10=400;
 for(u32 t=11000;t<12000;t++)at(t,t%100==0);
 assert(render_stats.maps==10&&render_stats.presents==10); /* moving reroutes too */
 route_status=R_READY;at(12000,1);assert(renderer.map_period==100&&renderer.map_at==12100);

 /* Repeated page changes and dirty inputs cannot bypass the calculation cap. */
 setup(R_CALC,13000);
 for(u32 t=13000;t<14000;t++){
  screen=(t%40<20)?SCR_MENU:SCR_TRIP;render_background_dirty=1;
  at(t,t%100==0);
 }
 assert(render_stats.presents==5&&render_stats.maps==0);
 assert(render_runtime_deadline(14000)==14000);
 /* Explore and an active volume/route pill also share the same five frames. */
 setup(R_CALC,15000);gps.fix=1;set_band("FM");car.volume[0]=0;set_volume(16);
 enter_freecam();u32 calc_term=hud_audio.until;
 for(u32 t=15000;t<16000;t++)at(t,t%100==0);
 assert(render_stats.maps==5&&render_stats.presents==5&&render_stats.rotations==0);
 assert(!freecam_transition_active()&&hud_audio.until==calc_term);
 /* Enter/leave on an arbitrary millisecond, including failure/cancellation. */
 const int exits[]={R_READY,R_ERROR,R_IDLE};
 for(unsigned e=0;e<sizeof(exits)/sizeof(exits[0]);e++){
  setup(R_IDLE,17000);at(17000,1);route_status=R_CALC;at(17011,1);
  u32 shown=render_stats.presents;assert(renderer.ui.next==17211);
  for(u32 t=17012;t<17211;t++){render_background_dirty=1;at(t,t%100==0);}
  assert(render_stats.presents==shown);
  route_status=exits[e];at(17210,1);
  assert(render_stats.presents==shown+1&&renderer.ui.next==17260&&renderer.map_period==100);
  for(u32 t=17211;t<17310;t++)at(t,t%100==0);
  assert(render_stats.presents>=shown+2); /* 20 Hz animation resumes */
 }

 setup(R_IDLE,20000);at(20000,1);at(20400,1);
 /* Yielding a partial background presents only the last finished image. */
 render_fb=render_work;fill(0,0,W,H,0xf81f);renderer.building=1;
 map_mask_active=1;map_mask_shift=10;map_coverage[20]=31u<<10;
 renderer.model_pending=1;u32 yield_count=render_stats.presents;
 hud_capture_ms=20450;render_checkpoint();assert(render_stats.presents==yield_count+1);
 assert(render_fb==render_work&&(*render_work)[100*W+250]==0xf81f);
 assert(display_fb[100*W+250]==C_BG&&(*render_front)[100*W+250]==C_BG);
 assert(map_mask_active==1&&map_mask_shift==10&&map_coverage[20]==31u<<10);
 renderer.building=0;map_mask_active=0;map_coverage[20]=0;render_fb=&display_fb;
 /* A fully restored clean backdrop cannot retain old glyph/pill rectangles. */
 at(21000,1);u16 reference[FB_PIX];render_copy(reference,display_fb);
 fill(0,0,W,H,0xf81f);hud_capture_ms=21033;render_present();
 assert(!memcmp(reference,display_fb,sizeof(reference)));
 apply_palette(0);at(21100,1);assert(renderer.theme==0&&display_fb[150*W+250]==C_BG);

 /* 500 ms camera trajectory has identical results at any sampling cadence. */
 setup(R_IDLE,30000);gps.fix=1;enter_freecam();
 freecam_orientation_anim_update(30166);int angle=freecam_course,tilt=map_tilt_q8;
 freecam_orientation_anim_update(30333);assert(freecam_course<angle&&map_tilt_q8<tilt);
 freecam_orientation_anim_update(30500);assert(!freecam_transition_active()&&freecam_course==0&&map_tilt_q8==0);
 recenter_map();hud_capture_ms=30000;enter_freecam();freecam_orientation_anim_update(30166);
 assert(freecam_course==angle&&map_tilt_q8==tilt);
 freecam_orientation_anim_update(31000);assert(!freecam_transition_active());
 hud_capture_ms=31000;leave_freecam();freecam_orientation_anim_update(31500);
 assert(!freecam_transition_active()&&map_tilt_q8==256&&view_course()==70);

 /* Camera presentation does not pause normal map updates or restart a pill. */
 setup(R_IDLE,40000);gps.fix=1;set_band("FM");car.volume[0]=0;set_volume(16);at(40000,1);
 u32 entrance=hud_tween[HC_STATUS].position_at,term=hud_audio.until;
 key_handle(K_MAP);
 for(u32 t=40001;t<41000;t++)at(t,t%100==0);
 assert(render_stats.maps==10&&render_stats.rotations>=4&&render_stats.presents>=15&&render_stats.presents<=20);
 assert(!freecam_transition_active()&&hud_audio.until==term);
 /* Entering Explore adds its zoom notice, but cannot restart the volume term. */
 assert(entrance==40000);

 /* A completion between presentation deadlines must still draw its final pose. */
 setup(R_IDLE,45000);gps.fix=1;at(45000,1);hud_capture_ms=45001;enter_freecam();
 at(45499,0);renderer.ui.next=45550;renderer.map_at=45600;at(45501,0);
 assert(!freecam_transition_active()&&renderer.camera_pending);
 assert(render_runtime_deadline(45501)==45550);
 at(45550,0);assert(!renderer.camera_pending&&map_tilt_q8==0&&view_course()==0);

 /* Profiling stays in memory unless Debug is explicitly enabled. */
 s_unlink(STATE_LOG);pref.debug=0;render_profile_service(50000);render_profile_service(56000);
 assert(s_open(STATE_LOG,O_RDONLY_,0)<0);
 pref.debug=1;render_profile_service(60000);render_profile_service(65000);
 long fd=s_open(STATE_LOG,O_RDONLY_,0);assert(fd>=0);char log[256]={0};assert(s_read((int)fd,log,255)>0);s_close((int)fd);
 assert(strstr(log,"FRAME 5S HUD="));s_unlink(STATE_LOG);pref.debug=0;
 puts("Frame renderer: 10/5 map clocks, whole-UI 5 Hz calculation cap, restored 20 Hz presentation, routing deadline budget, clean background yields, independent Explore, overlap and debug-only profiles passed.");
}
