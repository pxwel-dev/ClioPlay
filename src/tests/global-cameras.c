#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static void tick(void){gps_fix_ms=s_now_ms();camera_next_ms=0;camera_service();}
static void reset_audio_rings(void){
 memzero(camera_alert_ids,sizeof(camera_alert_ids));
 memzero(camera_alert_times,sizeof(camera_alert_times));
 memzero(camera_over_ids,sizeof(camera_over_ids));
 memzero(camera_over_times,sizeof(camera_over_times));
 camera_alert_slot=camera_over_slot=0;
}
int main(int argc,char**argv){
 assert(load_regions());assert(load_cameras());assert(load_font());
 pref.cameras=1;pref.units=0;gps.fix=1;gps.mph10=500;gps.course=0;gps.lat=4041570;gps.lon=-370380;
 track_course=display_course=0;track_valid=display_course_valid=1;region_resolve(gps.lat,gps.lon);route_status=R_IDLE;
 tick();assert(camera_visible&&camera_current.kind==1&&camera_marker_active());
 assert(camera_distance>80&&camera_distance<200);
 gps.course=180;track_course=display_course=180;tick();assert(!camera_visible&&!camera_marker_active());
 gps.course=0;track_course=display_course=0;
 pref.cameras=0;tick();assert(!camera_visible&&!camera_marker_active());
 pref.cameras=1;gps.fix=0;tick();assert(!camera_visible&&!camera_marker_active());
 gps.fix=1;tick();assert(camera_visible&&camera_marker_active());
 gps_fix_ms=s_now_ms()-6000;camera_next_ms=0;camera_service();assert(!camera_visible);
 /* Blocked country (France) suppresses both HUD alert and on-map marker. */
 gps.lat=4885660;gps.lon=235220;region_resolve(gps.lat,gps.lon);tick();assert(!camera_visible&&!camera_marker_active());
 gps.lat=4041570;gps.lon=-370380;region_resolve(gps.lat,gps.lon);tick();assert(camera_visible&&camera_marker_active());

 /* Speed-scaled horizon + curved unguided corridor (tight close up, gently flared far). */
 camera_clear();gps.lat=4041300;gps.lon=-370380;
 struct Camera far_cam={.lat=4041660,.lon=-370380,.heading=0,.kind=1,.speed=50,.id=701};
 u32 dist=0;
 gps.mph10=200;assert(!camera_ahead(&far_cam,&dist)); /* ~400 m > 250 m horizon at 20 mph */
 gps.mph10=700;assert(camera_ahead(&far_cam,&dist)&&dist>350&&dist<450); /* within 558 m at 70 mph */
 /* Curved road ~400 m ahead with 35-unit lateral curve offset is caught at 70 mph. */
 struct Camera curve_cam={.lat=4041660,.lon=-370380+42,.heading=0,.kind=1,.speed=50,.id=702};
 assert(camera_ahead(&curve_cam,&dist));
 /* Close parallel street (~100 m ahead, 28-unit lateral offset) stays strictly rejected. */
 struct Camera parallel_cam={.lat=4041390,.lon=-370380+35,.heading=0,.kind=1,.speed=50,.id=703};
 assert(!camera_ahead(&parallel_cam,&dist));

 /* Exact mph10 overspeed parity with Speed pill. */
 camera_visible=1;camera_average=0;camera_current.kind=1;camera_current.speed=30;camera_current.flags=2;
 gps.mph10=300;assert(!camera_is_overspeed()&&camera_alert_colour()==C_CYAN);
 gps.mph10=301;assert(camera_is_overspeed()&&camera_alert_colour()==C_WARN);
 camera_current.kind=2;gps.mph10=900;assert(!camera_is_overspeed()&&camera_alert_colour()==C_CYAN);

 /* Distance formatting: km/mi above 1 km, clean rounding below. */
 char dbuf[24];pref.units=1;
 camera_distance=1450;camera_distance_text(dbuf,sizeof(dbuf));assert(streq(dbuf,"1.5 km"));
 camera_distance=324;camera_distance_text(dbuf,sizeof(dbuf));assert(streq(dbuf,"320 m"));
 camera_distance=64;camera_distance_text(dbuf,sizeof(dbuf));assert(streq(dbuf,"65 m"));
 pref.units=2;
 camera_distance=1609;camera_distance_text(dbuf,sizeof(dbuf));assert(streq(dbuf,"1.0 mi"));
 camera_distance=180;camera_distance_text(dbuf,sizeof(dbuf));assert(streq(dbuf,"200 yd"));
 pref.units=0;

 /* Average fixture is east of the fixed camera, entry northbound at 40.4170, exit 40.4200. */
 camera_clear();gps.mph10=500;gps.lon=-369000;gps.lat=4041650;
 tick();assert(camera_visible&&camera_current.kind==3&&!camera_average);
 gps.lat=4041690;tick();assert(camera_average);
 camera_entry_ms=s_now_ms()-10000;gps.lat=4041790;tick();assert(camera_average&&camera_average_speed>0);
 gps.lat=4041890;tick();assert(camera_average);
 gps.lat=4041990;tick();assert(!camera_average); /* exit is 40.4200 */

 /* High-speed 1 Hz segment-crossing entry: jumping from 35 m before straight to 15 m past gantry. */
 camera_clear();gps.mph10=700;gps.lon=-369000;gps.lat=4041668;
 tick();assert(camera_visible&&camera_current.kind==3&&!camera_average);
 gps.lat=4041714;tick();assert(camera_average&&camera_visible);

 /* Abandoned average-speed zone (e.g. slip road exit) auto-clears after 15 s of sustained divergence. */
 camera_avg_min_dist=200;camera_distance=350;camera_avg_diverge_ms=s_now_ms()-16000;
 gps.lat=4041600;gps.lon=-369000; /* 444 m from exit 4042000 > min+120 */
 camera_prev_lat=gps.lat;camera_prev_lon=gps.lon;camera_prev_ms=s_now_ms();camera_prev_valid=1;
 tick();assert(!camera_average);

 /* Starting half way through a section must never invent an average. */
 camera_clear();gps.mph10=500;gps.lat=4041800;gps.lon=-369000;tick();assert(!camera_average);

 /* Same geometry on another road (e.g. a bridge) must not trigger a v2 record; along-route avg remaining works. */
 FILE*rf=tmpfile();assert(rf);u32 way[2]={123,1};assert(fwrite(way,sizeof(way),1,rf)==1);fflush(rf);
 routefd=fileno(rf);routeok=1;rh.flags=2;rh.base_edges=1;rh.geom_off=rh.geom_points=0;
 route_status=R_READY;route_head=0;route_n=2;
 route_lat[0]=4041570;route_lat[1]=4041800;route_lon[0]=route_lon[1]=-370380;route_seg_arc[1]=0;
 gps.lat=4041570;gps.lon=-370380;
 struct Camera matched={.lat=4041700,.lon=-370380,.heading=0,.waylo=123,.wayhi=1};
 assert(camera_ahead(&matched,&dist));matched.waylo=124;assert(!camera_ahead(&matched,&dist));
 /* Curved 3-point route remaining distance exceeds straight-line chord. */
 route_n=3;route_lat[0]=4041700;route_lon[0]=-369000;route_lat[1]=4041850;route_lon[1]=-368600;route_lat[2]=4042000;route_lon[2]=-369000;
 gps.lat=4041700;gps.lon=-369000;
 u32 rrem=0;assert(camera_route_remaining(4042000,-369000,&rrem)&&rrem>geo_seg_m(4041700,-369000,4042000,-369000)+100u);
 fclose(rf);routefd=-1;routeok=0;route_status=R_IDLE;

 /* Audio requests: compliant kind=1, overspeed kind=2, Overspeed audio mode (3), red-light soft-only, GPS-loss cancel. */
 gps.lat=4041570;gps.lon=-370380;region_resolve(gps.lat,gps.lon);
 audio_source_live=1;copystr(af.band,sizeof(af.band),"CD");
 /* Mode 2 (Phone audio), compliant -> kind=1 */
 camera_clear();reset_audio_rings();pref.cameras=2;gps.mph10=250;s_unlink(CAMERA_ALERT_PATH);
 tick();assert(camera_visible&&!camera_is_overspeed());
 u32 packet[4];long f=s_open(CAMERA_ALERT_PATH,O_RDONLY_,0);assert(f>=0&&s_read((int)f,packet,16)==16);s_close((int)f);
 assert(packet[0]==0x4c414c43u&&packet[3]==1);
 /* Mode 2 (Phone audio), overspeed -> kind=2 */
 camera_clear();reset_audio_rings();pref.cameras=2;gps.mph10=700;s_unlink(CAMERA_ALERT_PATH);
 tick();assert(camera_visible&&camera_is_overspeed());
 f=s_open(CAMERA_ALERT_PATH,O_RDONLY_,0);assert(f>=0&&s_read((int)f,packet,16)==16);s_close((int)f);
 assert(packet[0]==0x4c414c43u&&packet[3]==2);
 /* Mode 3 (Overspeed audio), compliant fixed camera -> silent */
 camera_clear();reset_audio_rings();pref.cameras=3;gps.mph10=250;s_unlink(CAMERA_ALERT_PATH);
 tick();assert(camera_visible&&!camera_is_overspeed());
 assert(s_open(CAMERA_ALERT_PATH,O_RDONLY_,0)<0);
 /* Mode 3 (Overspeed audio), overspeed fixed camera -> kind=2 */
 gps.mph10=700;tick();assert(camera_visible&&camera_is_overspeed());
 f=s_open(CAMERA_ALERT_PATH,O_RDONLY_,0);assert(f>=0&&s_read((int)f,packet,16)==16);s_close((int)f);
 assert(packet[0]==0x4c414c43u&&packet[3]==2);
 /* Mode 3 (Overspeed audio), red-light camera even at 90 mph -> soft approach kind=1 only */
 reset_audio_rings();s_unlink(CAMERA_ALERT_PATH);
 camera_visible=1;camera_average=0;camera_current.kind=2;camera_current.id=999;camera_current.speed=50;camera_current.flags=0;camera_distance=120;gps.mph10=900;
 camera_sound_service(s_now_ms());
 f=s_open(CAMERA_ALERT_PATH,O_RDONLY_,0);assert(f>=0&&s_read((int)f,packet,16)==16);s_close((int)f);
 assert(packet[0]==0x4c414c43u&&packet[3]==1);
 /* Mode 3 (Overspeed audio), active average-speed zone over limit -> kind=2 */
 reset_audio_rings();s_unlink(CAMERA_ALERT_PATH);
 camera_average=1;camera_current.kind=3;camera_current.id=998;camera_current.speed=50;camera_current.flags=0;camera_average_speed=62;
 camera_sound_service(s_now_ms());
 f=s_open(CAMERA_ALERT_PATH,O_RDONLY_,0);assert(f>=0&&s_read((int)f,packet,16)==16);s_close((int)f);
 assert(packet[0]==0x4c414c43u&&packet[3]==2);
 /* GPS loss cancels audio immediately. */
 gps.fix=0;tick();
 f=s_open(CAMERA_ALERT_PATH,O_RDONLY_,0);assert(f>=0&&s_read((int)f,packet,16)==16);s_close((int)f);
 assert(packet[3]==0);
 gps.fix=1;pref.cameras=1;audio_source_live=0;

 puts("Cameras: speed-scaled horizon, curved corridor, marker policy gate, high-speed avg entry, divergence auto-clear, along-route distance, overspeed parity, two-tier/overspeed audio and GPS-loss cancel passed.");
 if(argc==2){
  load_map();gps.mph10=500;gps.lat=4041570;gps.lon=-370380;tick();
  track_lat=gps.lat;track_lon=gps.lon;pref.autozoom=0;manual_zoom=6;
  gps.hour=12;gps.min=35;car.tempok=1;car.temp=18;set_band("FM");
  copystr(af.station,sizeof(af.station),"Radio");screen=SCR_MAP;pref.theme=THEME_DAY;
  draw();FILE*out=fopen(argv[1],"wb");assert(out);fwrite(fb,1,sizeof(fb),out);fclose(out);
 }
 return 0;
}
