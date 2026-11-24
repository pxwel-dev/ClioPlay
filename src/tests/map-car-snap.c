/* Visual Z6-reference road alignment for close views (Z6-Z9).
 * Uses synthetic Z6 tiles in tc[] so no navigation pack is required. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <stdio.h>

static void put_pt(u8*p,u16 x,u16 y){
 p[0]=(u8)x;p[1]=(u8)(x>>8);
 p[2]=(u8)y;p[3]=(u8)(y>>8);
}
static void tile_begin(struct Tile*t,s32 tx,s32 ty){
 memzero(t,sizeof(*t));
 t->valid=1;t->lvl=6;t->page=0;t->tx=tx;t->ty=ty;t->age=++agec;
 t->len=8;
}
static void tile_add_road(struct Tile*t,u8 cls,const u16(*pts)[2],int np){
 u8*p=t->data+t->len;
 p[0]=0;p[1]=cls;p[2]=(u8)np;p[3]=255;p+=4;
 for(int i=0;i<np;i++,p+=4)put_pt(p,pts[i][0],pts[i][1]);
 t->len=(u32)(p-t->data);
 u16 ng=(u16)(rd16(t->data)+1u);
 t->data[0]=(u8)ng;t->data[1]=(u8)(ng>>8);
}
static void reset_world(s32 ty,s32 tx,s32 lat,s32 lon,int course,int mph10){
 memzero(tc,sizeof(tc));agec=1;
 memzero(&mh,sizeof(mh));
 memcpy(mh.magic,"CPM5",4);mh.ver=5;mh.minlvl=0;mh.maxlvl=6;mh.count=0;
 mh.minlat=(ty-2)*256;mh.maxlat=(ty+3)*256;
 mh.minlon=(tx-2)*256;mh.maxlon=(tx+3)*256;
 atlas_ready=0;mapok=1;mapfmt=5;mapfd=-1;
 gps.fix=track_valid=1;
 gps.lat=track_lat=lat;gps.lon=track_lon=lon;
 gps.course=track_course=display_course=course;
 display_course_valid=course>=0;
 gps.mph10=mph10;
 freecam=0;freecam_anim_mode=FC_ANIM_NONE;
 map_tilt_q8=256;perspective_lut_tilt=-1;
 route_status=R_IDLE;route_n=route_head=0;
 visual_snap_reset();
 apply_palette(0);
}

/* 1. Z6-Z9 align the rendered road straight under (MAP_ANCHOR_X, DRIVE_ANCHOR_Y)
 *    even with perspective tilt and rotation; Z4-Z5 stay unsnapped; raw GPS stays untouched. */
static void test_close_zoom_camera_alignment(void){
 const s32 ty=21000,tx=-625;
 const s32 lat=ty*256+128,lon=tx*256+128;
 reset_world(ty,tx,lat,lon,90,200);
 tile_begin(&tc[0],tx,ty);
 /* East-west road 4.4375 world units south of GPS (ly=1977 -> 1977/16 = 123.5625). */
 const u16 road[2][2]={{256,1977},{3840,1977}};
 tile_add_road(&tc[0],3,road,2);

 for(int z=6;z<=9;z++){
  visual_snap_reset();
  draw_map_at_zoom(z);
  assert(visual_snap.matched);
  /* Road is south of GPS -> negative dlat4 (~ -71 Q4 units), zero dlon4. */
  assert(display_car_lat4()==ty*4096+1977);
  assert(iabs32(display_car_lon4()-lon*16)<=2);
  /* Raw GPS and car_lat/car_lon are never mutated. */
  assert(gps.lat==lat&&gps.lon==lon&&car_lat()==lat&&car_lon()==lon);
  /* Car arrow stays anchored cleanly at (MAP_ANCHOR_X, DRIVE_ANCHOR_Y), never under bottom HUD. */
  int cx,cy;display_car_screen_xy(z,&cx,&cy);
  assert(cx==MAP_ANCHOR_X&&cy==DRIVE_ANCHOR_Y&&cy<HUD_BOTTOM_Y);
  /* Verify the exact road centreline projects right onto the driving anchor! */
  int rx,ry;map_project_q8_4(ty*4096+1977,lon*16,z,&rx,&ry);
  assert(iabs32((rx+128)/256-MAP_ANCHOR_X)<=1);
  assert(iabs32((ry+128)/256-DRIVE_ANCHOR_Y)<=1);
  /* And on the framebuffer (before vehicle icon), road pixels cover the anchor. */
  fill(0,0,W,H,C_MAP_BG);
  draw_map_geometry(z);
  assert(fb[DRIVE_ANCHOR_Y*W+MAP_ANCHOR_X]!=C_MAP_BG);
 }

 /* Z4 and Z5 remain unsnapped and leave display position equal to raw GPS. */
 for(int z=4;z<=5;z++){
  draw_map_at_zoom(z);
  assert(visual_snap.dlat4==0&&visual_snap.dlon4==0&&!visual_snap.matched);
 }
}

/* 2. Heading alignment blocks perpendicular cross-street capture both while
 *    moving and while stopped at traffic lights (latched display_course). */
static void test_heading_blocks_cross_street(void){
 const s32 ty=21000,tx=-625;
 const s32 lat=ty*256+128,lon=tx*256+128;
 /* Driving North (0 deg) alongside N-S road 4 units east (lx=2048+112 -> ~4 lat-eq east at 55N)
  * while a perpendicular E-W cross street is only 1 unit south (ly=2032)! */
 for(int stopped=0;stopped<=1;stopped++){
  reset_world(ty,tx,lat,lon,0,stopped?0:200);
  tile_begin(&tc[0],tx,ty);
  const u16 ns_road[2][2]={{2160,256},{2160,3840}};
  const u16 ew_cross[2][2]={{256,2032},{3840,2032}};
  tile_add_road(&tc[0],6,ns_road,2);
  tile_add_road(&tc[0],6,ew_cross,2);

  update_visual_snap(9,1);
  assert(visual_snap.matched&&visual_snap.lock_gi==0);
  assert(display_car_lon4()==tx*4096+2161);
  assert(iabs32(display_car_lat4()-lat*16)<=2);
 }

 /* Now turn East (90 deg) while moving -> cleanly transitions onto the E-W street! */
 gps.course=track_course=display_course=90;gps.mph10=150;
 update_visual_snap(9,1);
 assert(visual_snap.matched&&visual_snap.lock_gi==1);
 assert(display_car_lat4()==ty*4096+2032);
}

/* 3. Service lanes (cls 7) do not steal nearby residential/main road driving,
 *    but still snap cleanly once you pull into the service lane/car park. */
static void test_service_road_penalty(void){
 const s32 ty=21000,tx=-625;
 const s32 lat=ty*256+128,lon=tx*256+128;
 reset_world(ty,tx,lat,lon,90,200);
 tile_begin(&tc[0],tx,ty);
 /* Residential road 4 units north (ly=2112); parallel service lane 2 units south (ly=2016). */
 const u16 res_road[2][2]={{256,2112},{3840,2112}};
 const u16 svc_lane[2][2]={{256,2016},{3840,2016}};
 tile_add_road(&tc[0],6,res_road,2);
 tile_add_road(&tc[0],7,svc_lane,2);

 update_visual_snap(9,1);
 assert(visual_snap.matched&&visual_snap.lock_gi==0&&visual_snap.lock_cls==6);

 /* Pull off into the service road (9 units south of main road, 1 unit north of service lane). */
 visual_snap_reset();
 gps.lat=track_lat=ty*256+127-0;
 /* Put car at lat = ty*256 + 123 (ly ~ 1968 -> 9 units south of res_road at 2112, 3 units from svc at 2016). */
 gps.lat=track_lat=ty*256+123;
 update_visual_snap(9,1);
 assert(visual_snap.matched&&visual_snap.lock_gi==1&&visual_snap.lock_cls==7);
}

/* 4. Active-route corridor preference keeps the car on the routed carriageway
 *    and anchors the magenta route directly beneath the car icon; perpendicular
 *    cross streets intersecting the route do not steal the route bonus. */
static void test_active_route_preference(void){
 const s32 ty=21000,tx=-625;
 const s32 lat=ty*256+128,lon=tx*256+128;
 reset_world(ty,tx,lat,lon,90,250);
 tile_begin(&tc[0],tx,ty);
 /* Parallel E-W road A is 2 units north (ly=2080, closer); routed road B is 4 units south (ly=1984).
  * Plus a perpendicular N-S cross street right through the car (lx=2048). */
 const u16 road_a[2][2]={{256,2080},{3840,2080}};
 const u16 road_b[2][2]={{256,1984},{3840,1984}};
 const u16 cross_ns[2][2]={{2048,256},{2048,3840}};
 tile_add_road(&tc[0],6,road_a,2);
 tile_add_road(&tc[0],6,road_b,2);
 tile_add_road(&tc[0],6,cross_ns,2);

 /* Active route runs East along road B (including a long 20 km motorway segment to verify no overflow). */
 route_status=R_READY;route_head=0;route_n=3;
 route_lat[0]=ty*256+124;route_lon[0]=tx*256+16;
 route_lat[1]=ty*256+124;route_lon[1]=tx*256+240;
 route_lat[2]=ty*256+124;route_lon[2]=tx*256+20240; /* 20 km ahead */

 draw_map_at_zoom(9);
 assert(visual_snap.matched&&visual_snap.on_route&&visual_snap.lock_gi==1);
 assert(display_car_lat4()==ty*4096+1984);

 /* Route overlay starts directly under the car anchor (MAP_ANCHOR_X, DRIVE_ANCHOR_Y). */
 fill(0,0,W,H,C_MAP_BG);
 draw_route_overlay(9);
 assert(fb[DRIVE_ANCHOR_Y*W+MAP_ANCHOR_X]==C_ROUTE);

 /* Cross street does NOT match the E-W route even at distance 0 (angle check rejects 90 deg). */
 assert(!visual_snap_seg_on_route(ty*4096+1984,lon*16,0,2000,coslat_q15(lat),0,1));
 assert(visual_snap_seg_on_route(ty*4096+1984,lon*16,2000,0,coslat_q15(lat),0,1));
}

/* 5. Dual-carriageway lock hysteresis prevents flipping across the median,
 *    while crossing a Z6 tile boundary hands off cleanly on the very first frame. */
static void test_hysteresis_and_tile_seam(void){
 const s32 ty=21000,tx=-625;
 /* Two parallel E-W carriageways 8 units apart: South at ly=1984 (124), North at ly=2112 (132). */
 reset_world(ty,tx,ty*256+126,tx*256+128,90,300);
 tile_begin(&tc[0],tx,ty);
 const u16 south_c[2][2]={{256,1984},{3840,1984}};
 const u16 north_c[2][2]={{256,2112},{3840,2112}};
 tile_add_road(&tc[0],3,south_c,2);
 tile_add_road(&tc[0],3,north_c,2);

 update_visual_snap(8,1);
 assert(visual_snap.matched&&visual_snap.lock_gi==0); /* Locked to South carriageway */

 /* Drift 1 unit past the midpoint (129 -> 3 units from North, 5 units from South):
  * hysteresis keeps us rock-steady on South carriageway instead of oscillating! */
 gps.lat=track_lat=ty*256+129;
 update_visual_snap(8,0);
 assert(visual_snap.matched&&visual_snap.lock_gi==0);

 /* Now test tile-boundary handoff: N-S road continues from tile ty (ly 0..4095) into ty+1 (ly 0..4095). */
 reset_world(ty,tx,ty*256+254,tx*256+128,0,250);
 tile_begin(&tc[0],tx,ty);
 tile_begin(&tc[1],tx,ty+1);
 const u16 seg_a[2][2]={{2048,0},{2048,4095}};
 const u16 seg_b[2][2]={{2048,0},{2048,4095}};
 tile_add_road(&tc[0],5,seg_a,2);
 tile_add_road(&tc[1],5,seg_b,2);

 update_visual_snap(9,1);
 assert(visual_snap.matched&&visual_snap.lock_ty==ty);

 /* Move 1 unit north of the seam into tile ty+1: old tile clamps at endpoint ->
  * drops lock bonus immediately and tile ty+1 takes over on frame 1! */
 gps.lat=track_lat=(ty+1)*256+1;
 update_visual_snap(9,0);
 assert(visual_snap.matched&&visual_snap.lock_ty==ty+1);
 assert(iabs32(display_car_lat4()-((ty+1)*256+1)*16)<=16);
}

/* 6. Smooth per-frame easing, off-road debounce, Explore entry/panning, and no-GPS safety. */
static void test_easing_offroad_and_explore(void){
 const s32 ty=21000,tx=-625;
 const s32 lat=ty*256+128,lon=tx*256+128;
 reset_world(ty,tx,lat,lon,90,200);
 tile_begin(&tc[0],tx,ty);
 /* Road 6 units south (ly=1952 -> dlat4 = -96). */
 const u16 road[2][2]={{256,1952},{3840,1952}};
 tile_add_road(&tc[0],6,road,2);

 update_visual_snap(9,1);
 assert(visual_snap.dlat4==-96);

 /* Enter Explore while snapped: cam_lat starts at snapped road (no 48 px jump!),
  * and panning 30 units away still keeps the car marker aligned on the exact Z6 road. */
 enter_freecam();
 assert(cam_lat==ty*256+122);
 cam_lon+=30;
 int cx,cy,rx,ry;
 display_car_screen_xy(9,&cx,&cy);
 map_project_q8_4(ty*4096+1952,lon*16,9,&rx,&ry);
 assert(iabs32(cx-(rx+128)/256)<=1&&iabs32(cy-(ry+128)/256)<=1);

 /* Return to driving, then pull 25 units north off all roads:
  * ticks 1-2 debounce; tick 3 releases lock and eases smoothly (never teleports in 1 frame). */
 freecam=0;freecam_anim_mode=FC_ANIM_NONE;
 gps.lat=track_lat=ty*256+147; /* 25 units north of road at 122 */
 update_visual_snap(9,0);assert(visual_snap.lock_valid&&visual_snap.off_ticks==1);
 gps.lat=track_lat=ty*256+148;
 update_visual_snap(9,0);assert(visual_snap.lock_valid&&visual_snap.off_ticks==2);
 gps.lat=track_lat=ty*256+149;
 update_visual_snap(9,0);assert(!visual_snap.lock_valid&&visual_snap.target_dlat4==0);
 /* Must ease gradually, NOT teleport straight from -96 to 0 in a single tick. */
 assert(visual_snap.dlat4<0&&visual_snap.dlat4>-96);
 for(int step=0;step<24;step++)update_visual_snap(9,0);
 assert(visual_snap.dlat4==0&&visual_snap.dlon4==0);

 /* Missing GPS fix: immediately clears visual snap. */
 gps.lat=track_lat=lat;update_visual_snap(9,1);assert(visual_snap.matched);
 gps.fix=0;update_visual_snap(9,0);
 assert(!visual_snap.init&&visual_snap.dlat4==0&&visual_snap.dlon4==0);
}

static void test_parked_offroad_releases_snap(void){
 const s32 ty=21000,tx=-625;
 const s32 lat=ty*256+128,lon=tx*256+128;
 const u16 road[2][2]={{256,1952},{3840,1952}};
 reset_world(ty,tx,lat,lon,90,200);
 tile_begin(&tc[0],tx,ty);
 tile_add_road(&tc[0],6,road,2);
 update_visual_snap(9,1);
 assert(visual_snap.matched&&visual_snap.dlat4==-96);

 gps.lat=track_lat=ty*256+147;
 gps.mph10=0;
 update_visual_snap(9,0);
 assert(!visual_snap.matched&&visual_snap.lock_valid&&visual_snap.off_ticks==1);
 u32 tile_age=agec;
 update_visual_snap(9,0);
 assert(visual_snap.lock_valid&&visual_snap.off_ticks==2&&visual_snap.dlat4==-96);
 update_visual_snap(9,0);
 assert(!visual_snap.lock_valid&&visual_snap.target_dlat4==0);
 assert(visual_snap.dlat4>-96&&visual_snap.dlat4<0);
 for(int step=0;step<24;step++)update_visual_snap(9,0);
 assert(display_car_lat4()==gps.lat*16&&display_car_lon4()==gps.lon*16);
 assert(agec==tile_age);

 /* End-to-end framebuffer scheduler test: pulling off-road and stopping
  * (gps.mph10 == 0) must still advance the parked map through the 3-tick
  * grace period and smooth ease-back to GPS, then stop redrawing once settled. */
 reset_world(ty,tx,lat,lon,90,200);
 tile_begin(&tc[0],tx,ty);
 tile_add_road(&tc[0],6,road,2);
 pref.autozoom=0;manual_zoom=9;zoom_override=-1;screen=SCR_MAP;
 hud_capture_clock=1;hud_capture_ms=10000u;
 render_runtime_reset(hud_capture_ms);
 render_runtime_step(hud_capture_ms,1);
 assert(visual_snap.matched&&visual_snap.dlat4==-96);
 /* Pull off-road and come to a complete stop with unchanged coordinates thereafter. */
 gps.lat=track_lat=ty*256+147;gps.mph10=0;
 u32 maps_before=render_stats.maps;
 u32 eased_frames=0;
 for(int tick=1;tick<=30;tick++){
  hud_capture_ms+=FRAME_MAP_MS;
  s32 prev_dlat=visual_snap.dlat4;
  update_visual_snap(active_zoom(),0);
  render_runtime_step(hud_capture_ms,1);
  if(visual_snap.dlat4!=prev_dlat)eased_frames++;
 }
 assert(eased_frames>=3);
 assert(!visual_snap.lock_valid&&visual_snap.dlat4==0&&visual_snap.dlon4==0&&!visual_snap_settling());
 assert(render_stats.maps>=maps_before+eased_frames);
 /* Once settled at GPS while parked, no further map redraws or tile reads occur. */
 u32 settled_maps=render_stats.maps,settled_age=agec;
 for(int tick=0;tick<10;tick++){
  hud_capture_ms+=FRAME_MAP_MS;
  update_visual_snap(active_zoom(),0);
  render_runtime_step(hud_capture_ms,1);
 }
 assert(render_stats.maps==settled_maps&&agec==settled_age);
 hud_capture_clock=0;
}

int main(void){
 test_close_zoom_camera_alignment();
 test_heading_blocks_cross_street();
 test_service_road_penalty();
 test_active_route_preference();
 test_hysteresis_and_tile_seam();
 test_easing_offroad_and_explore();
 test_parked_offroad_releases_snap();
 puts("Z6-reference visual road snap: camera-coupled Z6-Z9 alignment, heading/cross-street filter, service penalty, active-route corridor, hysteresis/tile-seam, moving and parked off-road release, Explore parity and no-GPS safety passed.");
 return 0;
}
