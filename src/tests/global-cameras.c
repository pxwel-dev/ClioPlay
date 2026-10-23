#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static void tick(void){gps_fix_ms=s_now_ms();camera_next_ms=0;camera_service();}
int main(int argc,char**argv){assert(load_regions());assert(load_cameras());assert(load_font());pref.cameras=1;pref.units=0;gps.fix=1;gps.mph10=500;gps.course=0;gps.lat=4041570;gps.lon=-370380;track_course=display_course=0;track_valid=display_course_valid=1;region_resolve(gps.lat,gps.lon);route_status=R_IDLE;tick();assert(camera_visible&&camera_current.kind==1);assert(camera_distance>80&&camera_distance<200);gps.course=180;track_course=display_course=180;tick();assert(!camera_visible);gps.course=0;track_course=display_course=0;
 pref.cameras=0;tick();assert(!camera_visible);pref.cameras=1;gps.fix=0;tick();assert(!camera_visible);gps.fix=1;tick();assert(camera_visible);gps_fix_ms=s_now_ms()-6000;camera_next_ms=0;camera_service();assert(!camera_visible);
 gps.lat=4885660;gps.lon=235220;region_resolve(gps.lat,gps.lon);tick();assert(!camera_visible);gps.lat=4041570;gps.lon=-370380;region_resolve(gps.lat,gps.lon);tick();assert(camera_visible);
 // Average fixture is east of the fixed camera, entry northbound at 40.4170.
 camera_clear();gps.lon=-369000;gps.lat=4041650;tick();assert(camera_visible&&camera_current.kind==3&&!camera_average);gps.lat=4041690;tick();assert(camera_average);camera_entry_ms=s_now_ms()-10000;gps.lat=4041790;tick();assert(camera_average&&camera_average_speed>0);gps.lat=4041890;tick();assert(camera_average);gps.lat=4041990;tick();assert(!camera_average); // exit is 40.4200
 // Starting half way through a section must never invent an average.
 camera_clear();gps.lat=4041800;tick();assert(!camera_average);
 // Same geometry on another road (e.g. a bridge) must not trigger a v2 record.
 FILE*rf=tmpfile();assert(rf);u32 way[2]={123,1};assert(fwrite(way,sizeof(way),1,rf)==1);fflush(rf);routefd=fileno(rf);routeok=1;rh.flags=2;rh.base_edges=1;rh.geom_off=rh.geom_points=0;route_status=R_READY;route_head=0;route_n=2;route_lat[0]=4041570;route_lat[1]=4041800;route_lon[0]=route_lon[1]=-370380;route_seg_arc[1]=0;gps.lat=4041570;gps.lon=-370380;
 struct Camera matched={.lat=4041700,.lon=-370380,.heading=0,.waylo=123,.wayhi=1};u32 distance;assert(camera_ahead(&matched,&distance));matched.waylo=124;assert(!camera_ahead(&matched,&distance));fclose(rf);routefd=-1;routeok=0;route_status=R_IDLE;
 // Audio requests are source-gated and cancelled on GPS loss.
 region_resolve(gps.lat,gps.lon);camera_clear();pref.cameras=2;audio_source_live=1;copystr(af.band,sizeof(af.band),"CD");s_unlink(CAMERA_ALERT_PATH);tick();assert(camera_visible);u32 packet[4];long f=s_open(CAMERA_ALERT_PATH,O_RDONLY_,0);assert(f>=0&&s_read((int)f,packet,16)==16);s_close((int)f);assert(packet[0]==0x4c414c43u&&packet[3]==1);gps.fix=0;tick();f=s_open(CAMERA_ALERT_PATH,O_RDONLY_,0);assert(f>=0&&s_read((int)f,packet,16)==16);s_close((int)f);assert(packet[3]==0);gps.fix=1;pref.cameras=1;audio_source_live=0;
 puts("Cameras: direction, approach distance, user-off, GPS loss/staleness, French suppression, average entry/exit and no mid-zone fabrication, road identity and source-gated audio requests passed.");
 if(argc==2){load_map();gps.lat=4041570;gps.lon=-370380;tick();track_lat=gps.lat;track_lon=gps.lon;pref.autozoom=0;manual_zoom=6;gps.hour=12;gps.min=35;car.tempok=1;car.temp=18;set_band("FM");copystr(af.station,sizeof(af.station),"Radio");screen=SCR_MAP;pref.theme=THEME_DAY;draw();FILE*f=fopen(argv[1],"wb");assert(f);fwrite(fb,1,sizeof(fb),f);fclose(f);}return 0;}
