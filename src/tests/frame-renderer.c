/* Real compositor with a controlled clock: cadence, isolation and concurrency. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static void setup(int status,u32 now){
 hud_capture_clock=1;hud_capture_ms=now;screen=SCR_MAP;modal=MOD_NONE;pref.debug=0;pref.language=0;
 mapok=0;gps.fix=0;gps.mph10=0;track_valid=1;gps.hour=12;gps.min=35;car.tempok=0;
 track_lat=track_lon=gps.lat=gps.lon=0;track_course=display_course=70;display_course_valid=1;
 trip.active=0;route_status=hud_observed_route=status;route_n=0;freecam=0;map_tilt_q8=256;freecam_anim_mode=FC_ANIM_NONE;
 bt_control_mode=0;hud_audio.kind=hud_source.kind=hud_navigation.kind=hud_connection.kind=hud_bt_feedback.kind=HUD_NONE;
 hud_reset_animations();render_runtime_reset(now);apply_palette(1);
}
static void at(u32 now,int model){hud_capture_ms=now;render_runtime_step(now,model);}
enum {COMPOSITE_FRAMES=36};
static u16 full_frames[COMPOSITE_FRAMES][FB_PIX];
static u16 written_frame[FB_PIX];

static void assert_device_frame(void){
 FILE*f=fopen(FB_PATH,"rb");assert(f);
 assert(fread(written_frame,sizeof(written_frame),1,f)==1);
 assert(fclose(f)==0);
 assert(!memcmp(written_frame,display_fb,sizeof(written_frame)));
}

static void composite_sequence(int full_only){
 setup(R_IDLE,100000);
 car.volume[0]=0;
 btui.connected=btui.playing=1;btui.pos=20;btui.dur=170;
 copystr(btui.title,sizeof(btui.title),"First song");
 copystr(btui.artist,sizeof(btui.artist),"Test artist");
 set_band("CD");
 renderer.valid=renderer.pending=1;renderer.theme=theme_resolved;
 for(int i=0;i<COMPOSITE_FRAMES;i++){
  hud_capture_ms=100000u+(u32)i*FRAME_UI_MS;
  switch(i){
   case 0:
   case 25:
    for(int y=0;y<H;y++)for(int x=0;x<W;x++)
     (*render_front)[y*W+x]=rgb((x+y+i)%256,(x+2*y)%256,(x^y)%256);
    hud_glass_background_changed();renderer.pending=1;
    break;
   case 2:bt_control_mode=1;break;
   case 6:copystr(btui.title,sizeof(btui.title),"Second song");break;
   case 9:key_handle(K_MENU);break;
   case 14:menu_open(P_PHONE,0,hud_capture_ms);screen=menu_screen(sheet_page);break;
   case 18:modal=MOD_ROUTE_POINT;modal_choice=0;break;
   case 22:modal_choice=1;break;
   case 23:modal=MOD_NONE;break;
   case 28:key_handle(K_MAP);break;
   case 32:set_volume(14);break;
  }
  renderer.page=screen;
  if(full_only)renderer.pending=1;
  render_present();
  assert_device_frame();
  if(full_only)memcpy(full_frames[i],display_fb,sizeof(display_fb));
  else if(memcmp(full_frames[i],display_fb,sizeof(display_fb))){
   int n=0;
   for(int pix=0;pix<FB_PIX;pix++)if(full_frames[i][pix]!=display_fb[pix]&&++n==1)
    fprintf(stderr,"Composite differs at %d,%d on frame %d: %04x != %04x\n",pix%W,pix/W,i,full_frames[i][pix],display_fb[pix]);
   fprintf(stderr,"%d differences on frame %d\n",n,i);
   abort();
  }
 }
 if(full_only)assert(render_stats.fb_partial==0&&render_stats.fb_bytes==COMPOSITE_FRAMES*sizeof(fb));
 else{
  assert(frame_fb_verified&&!frame_fb_legacy&&render_stats.fb_partial>0);
  assert(render_stats.fb_bytes<COMPOSITE_FRAMES*sizeof(fb));
 }
}

static void framebuffer_checks(void){
 composite_sequence(1);
 composite_sequence(0);
 /* A failed write retries the whole frame through the legacy path. */
 assert(s_close(frame_fb_fd)==0);
 frame_fb_fd=(int)s_open(FB_PATH,O_RDONLY_,0);assert(frame_fb_fd>=0);
 hud_capture_ms+=FRAME_UI_MS;render_present();
 assert(frame_fb_legacy&&frame_fb_fd<0);
 assert_device_frame();
}
static void prepared_overlay_checks(void){
 for(int family=0;family<3;family++){
  u32 start=120000u+3000u*(u32)family;
  setup(R_IDLE,start);gps.fix=1;
  for(u32 t=start;t<start+650u;t++)at(t,t%100u==0);
  u32 input=start+700u;
  hud_capture_ms=input;
  if(family==0){set_band("FM");car.volume[0]=0;set_volume(17);}
  if(family==1)key_handle(K_MENU);
  if(family==2)modal=MOD_ROUTE_POINT;
  u32 shown=render_stats.presents,buffered=render_stats.overlay_frames;
  at(input,1);
  unsigned count=overlay.batch_count;
  assert(count==7&&overlay.built==1&&render_stats.presents==shown);
  for(unsigned i=1;i<count;i++){
   at(input+i*100u,1);
   assert(overlay.built==i+1&&overlay.shown==0&&render_stats.presents==shown);
  }
  /* Preparation has run past the wall-clock end. Play every sample in
   * order, even when a screen update wakes several frame intervals late. */
  for(unsigned i=0;i<count;i++){
   const struct OverlayFrame*f=&overlay.frames[i];
   assert(f->at==input+((i+1u)*FRAME_UI_MS>320u?320u:(i+1u)*FRAME_UI_MS));
   memcpy(full_frames[0],f->pixels,sizeof(display_fb));
   u32 due=overlay.due;
   at(due+(i==1?180u:0u),1);
   assert(render_stats.overlay_frames==buffered+i+1&&render_stats.presents==shown+i+1);
   assert(!memcmp(full_frames[0],display_fb,sizeof(display_fb)));
   assert_device_frame();
   if(i+1<count){
    assert(overlay.batch_count==count&&overlay.due==renderer.present_at+50u);
    at(overlay.due-1u,0);
    assert(render_stats.overlay_frames==buffered+i+1&&render_stats.presents==shown+i+1);
   }
  }
  assert(!overlay.batch_count&&menu_presented_at==input+320u);
 }
 /* The Bluetooth position poll and clock update must not cancel or
  * advance a menu transition which is already being prepared or played. */
 setup(R_IDLE,140000);gps.fix=1;set_band("CD");
 btui.connected=btui.playing=1;btui.dur=120;btui.pos=15;
 copystr(btui.title,sizeof(btui.title),"Playing");
 for(u32 t=140000;t<140650;t++)at(t,t%100u==0);
 hud_capture_ms=140700;key_handle(K_MENU);at(140700,1);
 unsigned frames=overlay.batch_count;
 u32 prepared_before=render_stats.overlay_frames;
 assert(frames==7&&overlay.built==1);
 for(unsigned i=1;i<frames;i++){
  if(i==2)btui.pos++;
  at(140700+i*100u,1);
  assert(overlay.batch_count==frames&&overlay.built==i+1);
 }
 for(unsigned i=0;i<frames;i++){
  if(i==1)gps.min++;
  u32 due=overlay.due;
  memcpy(full_frames[0],overlay.frames[i].pixels,sizeof(display_fb));
  at(due,1);
  assert(render_stats.overlay_frames==prepared_before+i+1);
  assert(menu_presented_at==140700u+((i+1u)*FRAME_UI_MS>320u?320u:(i+1u)*FRAME_UI_MS));
  assert(!memcmp(full_frames[0],display_fb,sizeof(display_fb)));
 }
 assert(!overlay.batch_count);
 at(renderer.ui.next,0);
 assert(hud_playback.raw_pos==16&&hud_tween[HC_CLOCK].target.title[4]=='6'&&!render_stats.overlay_aborts);

 /* An actual camera alert is composed immediately, even mid-preparation. */
 hud_capture_ms=141700;key_handle(K_MAP);at(141700,1);
 assert(overlay.batch_count&&overlay.built==1);
 camera_visible=1;camera_distance=220;
 at(141701,1);
 assert(!overlay.batch_count&&render_stats.overlay_aborts==1&&hud_tween[HC_ROAD].target.kind==1);
 camera_clear();

 setup(R_IDLE,132000);gps.fix=1;
 for(u32 t=132000;t<132650;t++)at(t,t%100u==0);
 hud_capture_ms=132700;key_handle(K_MENU);at(132700,1);
 assert(overlay.batch_count&&overlay.built==1);
 u32 shown=render_stats.overlay_frames;
 hud_capture_ms=132725;key_handle(K_DOWN);at(132725,1);
 assert(!overlay.batch_count&&render_stats.overlay_frames==shown&&sheet_focus==1);
 at(132726,0);assert(overlay.batch_count);
 route_status=R_CALC;at(132727,0);
 assert(!overlay.frames&&renderer.ui_period==FRAME_CALC_MAP_MS&&render_stats.overlay_frames==shown);

 /* Transport feedback runs longer than a sheet transition. */
 setup(R_IDLE,135000);gps.fix=1;set_band("CD");
 btui.connected=btui.playing=bt_control_mode=1;btui.dur=120;btui.pos=15;
 copystr(btui.title,sizeof(btui.title),"Playing");
 for(u32 t=135000;t<135650;t++)at(t,t%100u==0);
 hud_pulse_at(&bt_transport_press,BT_TRANSPORT_NEXT,135700,BT_PRESS_MS);
 at(135700,1);
 assert(overlay.batch_count==9&&overlay.batch_count==OVERLAY_FRAME_COUNT&&overlay.batch_ms==BT_PRESS_MS);
}

static void memory_and_overlay_model_checks(void){
 /* Pure /proc kB parser handles Linux 2.6 /proc/self/status and /proc/meminfo. */
 const char*status_sample="Name:\tclio040\nState:\tR (running)\nVmPeak:\t   28416 kB\nVmSize:\t   26112 kB\nVmRSS:\t    7428 kB\nVmData:\t    6144 kB\n";
 const char*meminfo_sample="MemTotal:        61440 kB\nMemFree:         24812 kB\nBuffers:          1024 kB\n";
 u32 rss=0,memfree=0,missing=99;
 assert(render_parse_kb_field(status_sample,"VmRSS:",&rss)&&rss==7428u);
 assert(render_parse_kb_field(meminfo_sample,"MemFree:",&memfree)&&memfree==24812u);
 assert(!render_parse_kb_field(status_sample,"MemFree:",&missing)&&missing==99u);
 assert(!render_parse_kb_field("VmRSS:\tabc kB\n","VmRSS:",&missing)&&missing==99u);
 assert(!render_parse_kb_field("","VmRSS:",&missing));

 /* Regression guard: overlay_model() must change when key UI/HUD/sheet fields mutate. */
 setup(R_READY,200000);gps.fix=1;trip.active=trip.count=1;
 nav_man_type=MAN_RIGHT;nav_man_idx=2;nav_man_m=200;nav_next_type=MAN_LEFT;nav_next_m=80;
 hud_prepare(5,200000,1);hud_driver_prepare(200000);
 u32 base=overlay_model();
 nav_next_type=MAN_UTURN;hud_driver_prepare(200000);
 assert(overlay_model()!=base);
 nav_next_type=MAN_LEFT;hud_driver_prepare(200000);
 menu_open(P_CONFIRM,0,200000);confirm_kind=0;u32 m0=overlay_model();
 confirm_kind=3;assert(overlay_model()!=m0);
 menu_open(P_NEARBY_LIST,0,200000);nearby_count=1;nearby_pois[0].dist_m=120;copystr(nearby_pois[0].name,sizeof(nearby_pois[0].name),"Shell");
 u32 n0=overlay_model();
 nearby_pois[0].dist_m=340;assert(overlay_model()!=n0);
 copystr(nearby_pois[0].name,sizeof(nearby_pois[0].name),"Esso");assert(overlay_model()!=n0);
 nearby_count=0;menu_close(200000);nav_next_type=MAN_NONE;
}
int main(void){
 struct FrameClock clock={0};unsigned frames=0;
 for(u32 t=0;t<1000;t++)if(frame_due(t,clock.next)){frames++;frame_advance_ui(&clock,t,frame_ui_period(0));}
 assert(frames==20&&clock.next==1000);
 for(u32 t=1000;t<5000;t++)if(frame_due(t,clock.next)){frames++;frame_advance_ui(&clock,t,frame_ui_period(0));}
 assert(frames==100&&clock.next==5000);
 clock=(struct FrameClock){0xffffff00u};u32 before=clock.next;frame_advance_ui(&clock,before,frame_ui_period(0));assert(clock.next-before==50);
 assert(frame_ui_period(0)==50&&frame_ui_period(1)==200);
 assert(frame_after(100,450,100)==500&&frame_after(0xfffffff0u,0x30u,100u)==0x54u);
 assert(frame_route_budget(100,101)==0&&frame_route_budget(100,110)==5&&frame_route_budget(100,104)==2);

 setup(R_IDLE,10000);
 for(u32 t=10000;t<11000;t++)at(t,t%100==0);
 assert(render_stats.maps==1&&render_stats.presents>10&&render_stats.presents<=20); /* 20 Hz during entrance, idle thereafter */
 assert(render_stats.rotations==0);
 for(int i=0;i<FB_PIX;i++)assert((*render_front)[i]==C_MAP_BG); /* no pills in background */
 setup(R_CALC,10000);
 for(u32 t=10000;t<11000;t++)at(t,t%100==0);
 assert(render_stats.maps==1&&render_stats.presents==5); /* parked calculation still caps the HUD */
 renderer.building=1;hud_capture_ms=renderer.ui.next;renderer.model_pending=1;
 render_checkpoint();assert(render_stats.presents==5);renderer.building=0;
 route_rerouting=1;gps.fix=1;gps.mph10=400;
 for(u32 t=11000;t<12000;t++)at(t,t%100==0);
 assert(render_stats.maps==6&&render_stats.presents==10); /* moving reroutes rebuild at 5 Hz */
 route_status=R_READY;at(12000,1);assert(renderer.map_period==100&&renderer.map_at==12100);

 /* Repeated page changes and dirty inputs cannot bypass the calculation cap. */
 setup(R_CALC,13000);
 for(u32 t=13000;t<14000;t++){
  screen=(t%40<20)?SCR_MENU:SCR_TRIP;render_background_dirty=1;
  at(t,t%100==0);
 }
 assert(render_stats.presents==5&&render_stats.maps==1);
 assert(render_runtime_deadline(14000)==14000);
 /* Explore and an active volume/route pill also share the same five frames. */
 setup(R_CALC,15000);gps.fix=1;set_band("FM");car.volume[0]=0;set_volume(16);
 enter_freecam();u32 calc_term=hud_audio.until;
 for(u32 t=15000;t<16000;t++)at(t,t%100==0);
 assert(render_stats.presents==5&&!explore.frames);
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
 hud_capture_ms=20450;renderer.ui.next=20450;render_checkpoint();assert(render_stats.presents==yield_count+1);
 assert(render_fb==render_work&&(*render_work)[100*W+250]==0xf81f);
 assert(display_fb[100*W+250]==C_MAP_BG&&(*render_front)[100*W+250]==C_MAP_BG);
 assert(map_mask_active==1&&map_mask_shift==10&&map_coverage[20]==31u<<10);
 renderer.building=0;map_mask_active=0;map_coverage[20]=0;render_fb=&display_fb;
 /* A fully restored clean backdrop cannot retain old glyph/pill rectangles. */
 at(21000,1);u16 reference[FB_PIX];render_copy(reference,display_fb);
 for(int row=0;row<H;row++)if(renderer.last_overlay[row])fill(0,row,W,1,0xf81f);
 hud_capture_ms=21033;render_present();
 assert(!memcmp(reference,display_fb,sizeof(reference)));
 apply_palette(0);at(21100,1);assert(renderer.theme==0&&display_fb[150*W+250]==C_MAP_BG);

 /* All representable tile coordinates, every supported tile span. */
 for(unsigned span=256;span<=16384;span*=2)for(unsigned local=0;local<=65535;local++)
  assert(map_tile_offset(local,span)==(int)(local*span/4095u));
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

 /* GPS jitter and HUD-only changes keep the last parked map. Live input,
  * a new route, and a theme or fix change can still invalidate it. */
 setup(R_IDLE,40000);gps.fix=1;set_band("FM");car.volume[0]=0;set_volume(16);
 at(40000,1);u32 parked_draws=render_stats.maps;
 for(u32 t=40001;t<41000;t++){
  if(t%100u==0){track_lat+=1;track_lon-=2;gps.min=(int)(t/100)%60;}
  if(t==40300){hud_capture_ms=t;key_handle(K_MENU);}
  if(t==40600){hud_capture_ms=t;key_handle(K_MAP);}
  at(t,t%100u==0);
 }
 assert(render_stats.maps==parked_draws&&render_stats.presents>=9);
 route_status=R_READY;route_n=2;at(41000,1);assert(render_stats.maps==++parked_draws);
 route_head=1;at(41100,1);assert(render_stats.maps==++parked_draws);
 manual_zoom=4;pref.autozoom=0;at(41200,1);assert(render_stats.maps==++parked_draws);
 apply_palette(0);at(41300,1);assert(render_stats.maps==++parked_draws);
 gps.fix=0;at(41400,1);assert(render_stats.maps==++parked_draws);
 gps.fix=1;gps.mph10=20;at(41500,1);assert(render_stats.maps==++parked_draws);
 at(41600,1);assert(render_stats.maps==++parked_draws);
 gps.mph10=0;at(41700,1);assert(render_stats.maps==++parked_draws);
 at(41800,1);assert(render_stats.maps==parked_draws);

 /* Prepare every camera background before showing any, even with audio
  * notices active. Late screen service consumes exactly one queued frame. */
 assert(EXPLORE_FRAME_COUNT==10);
 for(int leaving=0;leaving<2;leaving++){
  u32 start=46000u+(u32)leaving*2000u;
  setup(R_IDLE,start);gps.fix=1;pref.autozoom=1;
  if(leaving){freecam=1;freecam_course_valid=1;freecam_course=0;map_tilt_q8=0;}
  set_band("FM");car.volume[0]=0;set_volume(16);
  at(start,1);u32 term=hud_audio.until;
  u16 (*held)[FB_PIX]=render_front;
  key_handle(K_MAP);
  for(unsigned i=0;i<EXPLORE_FRAME_COUNT;i++){
   at(start+1u+i*40u,1);
   assert(explore.built==i+1&&explore.shown==0&&render_front==held);
   assert(map_tilt_q8==(leaving?0:256)&&render_stats.rotations==0);
  }
  assert(!explore_preparing()&&render_stats.explore_prep_ms>=360);
  for(unsigned i=0;i<EXPLORE_FRAME_COUNT;i++){
   u32 due=renderer.ui.next;
   at(due+(i==3?210u:0u),1);
   assert(render_stats.rotations==i+1);
   if(i+1<EXPLORE_FRAME_COUNT){
    assert(explore.shown==i+1&&render_front==&explore.frames[i]);
    assert(renderer.ui.next==renderer.present_at+50u);
    at(renderer.ui.next-1u,0);
    assert(render_stats.rotations==i+1&&explore.shown==i+1);
   }
  }
  assert(!explore.frames&&!freecam_transition_active()&&render_front==held);
  assert(map_tilt_q8==(leaving?256:0)&&view_course()==(leaving?70:0));
  assert(hud_audio.until==term&&render_stats.maps==1);
  at(renderer.ui.next,1);assert(render_stats.maps==1);
 }
 /* Panning during preparation discards the stale sequence and goes live.
  * A new route calculation also releases the queue and keeps the 5 Hz cap. */
 for(int calculating=0;calculating<2;calculating++){
  setup(R_IDLE,51000);gps.fix=1;at(51000,1);key_handle(K_MAP);
  at(51001,1);assert(explore_preparing()&&freecam_buffered);
  hud_capture_ms=51002;
  if(calculating)route_status=R_CALC;else key_handle(K_RIGHT);
  at(51002,1);
  assert(!explore.frames&&!freecam_buffered&&freecam_transition_active());
  if(calculating)assert(renderer.ui_period==FRAME_CALC_MAP_MS);
 }
 /* Reversing during playback starts from the last displayed angle. */
 setup(R_IDLE,54000);gps.fix=1;at(54000,1);key_handle(K_MAP);
 for(unsigned i=0;i<EXPLORE_FRAME_COUNT;i++)at(54001u+i,0);
 at(renderer.ui.next,0);assert(explore.shown==1);
 int displayed=freecam_course;
 hud_capture_ms+=210u;leave_freecam();assert(freecam_anim_from==displayed);
 at(hud_capture_ms,0);assert(explore_preparing()&&explore.shown==0&&explore.built==1);

 /* A steady playing BT CTRL bar, speed, distance and clock values can use
  * the full 20 Hz HUD cadence while the moving map stays at 10 Hz. */
 setup(R_READY,70000);gps.fix=1;roadctx.valid=1;roadctx.speed=80;roadctx.speed_flags=2;roadctx.street[0]=0;
 trip.active=1;nav_man_type=MAN_RIGHT;nav_man_idx=1;nav_man_m=1000;
 set_band("CD");hud_source.kind=HUD_NONE;bt_control_mode=1;
 btui.connected=btui.playing=1;btui.pos=30;btui.dur=120;
 copystr(btui.title,sizeof(btui.title),"Track one");copystr(btui.artist,sizeof(btui.artist),"Artist");
 gps.mph10=350;
 for(u32 t=70000;t<77000;t++)at(t,t%100==0);
 u32 steady=render_stats.presents,steady_maps=render_stats.maps;
 for(u32 t=77000;t<78000;t++){
  if(t%100==0){gps.mph10=350+(int)(t%300)/100;nav_man_m=1000-(int)(t-77000)/10;btui.pos=37+(int)(t-77000)/500;gps.min=(int)(t/100)%60;}
  at(t,t%100==0);
 }
 assert(render_stats.presents-steady==20&&render_stats.maps-steady_maps==10&&renderer.ui_period==50);
 /* New metadata promotes immediately without having to change pill size. */
 hud_capture_ms=78001;copystr(btui.title,sizeof(btui.title),"Track two");at(78001,1);
 assert(renderer.ui_period==50&&hud_tween[HC_STATUS].fast_content_at==78001);
 for(u32 t=78002;t<79000;t++)at(t,t%100==0);
 assert(renderer.ui_period==50&&!render_transition_active(hud_now_ms()));
 /* Adding volume is new information; repeated same-size values are not. */
 hud_capture_ms=79000;car.volume[0]=0;set_volume(15);at(79000,1);
 assert(renderer.ui_period==50);u32 volume_entry=hud_tween[HC_STATUS].fast_content_at;
 for(u32 t=79001;t<80000;t++){
  if(t%100==0){hud_capture_ms=t;set_volume(15+(int)(t%300)/100);}
  at(t,t%100==0);
 }
 assert(renderer.ui_period==50&&hud_tween[HC_STATUS].fast_content_at==volume_entry&&!render_transition_active(hud_now_ms()));
 steady=render_stats.presents;
 for(u32 t=80000;t<81000;t++){
  if(t%100==0){hud_capture_ms=t;set_volume(15+(int)(t%300)/100);}
  at(t,t%100==0);
 }
 assert(render_stats.presents-steady==20);
 /* Paused audio leaves a numeric-only fade to schedule its own second
  * frame, before the next map update. */
 btui.playing=0;bt_control_mode=0;
 hud_capture_ms=83000;set_volume(21);
 for(u32 t=83000;t<83700;t++)at(t,t%100u==0);
 hud_capture_ms=83700;set_volume(22);at(83700,1);
 assert(!render_transition_active(83700)&&!overlay.batch_count);
 steady=render_stats.presents;steady_maps=render_stats.maps;
 at(83749,0);assert(render_stats.presents==steady);
 at(83750,0);assert(render_stats.presents==steady+1&&render_stats.maps==steady_maps);
 /* Eased warning colours remain genuine animations, including the safe fade. */
 gps.mph10=900;at(84000,1);assert(renderer.ui_period==50&&hud_speed_over);
 steady=render_stats.presents;
 for(u32 t=84001;t<=85000;t++)at(t,t%100==0);
 assert(render_stats.presents-steady==20); /* 20 complete 50 ms intervals after the first presentation. */
 gps.mph10=350;at(85001,1);assert(renderer.ui_period==50&&!hud_speed_over);
 for(u32 t=85002;t<85500;t++)at(t,t%100==0);
 assert(renderer.ui_period==50&&!render_transition_active(hud_now_ms()));

 /* Profiling stays in memory unless Debug is explicitly enabled. */
 s_unlink(STATE_LOG);pref.debug=0;render_profile_service(50000);render_profile_service(56000);
 assert(s_open(STATE_LOG,O_RDONLY_,0)<0);
 pref.debug=1;render_profile_service(60000);render_profile_service(65000);
 long fd=s_open(STATE_LOG,O_RDONLY_,0);assert(fd>=0);char log[400]={0};assert(s_read((int)fd,log,sizeof(log)-1)>0);s_close((int)fd);
 assert(strstr(log,"FRAME 5S HUD=")&&strstr(log,"FB_PARTIAL=")&&strstr(log,"MAXFLUSH_MS=")&&strstr(log,"OVERLAY_ABORT="));s_unlink(STATE_LOG);pref.debug=0;
 framebuffer_checks();
 prepared_overlay_checks();
 memory_and_overlay_model_checks();
 puts("Partial framebuffer output matches full presentation across pills, menus, popups, and map changes; a failed band restores the full frame.");
 puts("Prepared overlays and Explore play at 20 Hz without dropping queued frames, with immediate input, camera and route interrupts.");
 puts("Frame renderer: 20 Hz transitions and steady HUD, 10 Hz live maps, whole-UI 5 Hz calculation cap, routing budget, clean background yields, independent Explore and debug-only profiles passed.");
}
