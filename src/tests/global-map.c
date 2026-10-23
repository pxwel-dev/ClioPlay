#define HOST_TEST 1
#define CLIO_UNICODE 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static void capture(const char*dir,const char*name){char path[1024];snprintf(path,sizeof(path),"%s/%s.rgb565",dir,name);FILE*f=fopen(path,"wb");assert(f);assert(fwrite(fb,1,sizeof(fb),f)==sizeof(fb));fclose(f);}
int main(int argc,char**argv){assert(argc==6);assert(load_font());assert(cliou_load(CLIO_HOST_ROOT "clio843/maps/fonts.cpf"));assert(load_map()&&mapfmt==5);assert(load_regions());assert(load_cameras());gps.fix=1;gps_fix_ms=s_now_ms();gps.lat=atoi(argv[2]);gps.lon=atoi(argv[3]);gps.course=0;gps.mph10=0;gps.sats=8;gps.hour=12;gps.min=35;gps.year=2026;gps.mon=10;gps.day=19;track_lat=gps.lat;track_lon=gps.lon;track_valid=1;track_course=display_course=0;display_course_valid=1;region_resolve(gps.lat,gps.lon);pref.units=0;pref.autozoom=0;manual_zoom=6;set_band("FM");copystr(af.station,sizeof(af.station),"Radio");car.tempok=1;car.temp=18;
 assert(!display_imperial()||!strcmp(region_iso,"US"));assert(load_route_data()&&rh.ver==5&&qadjmode==7);assert(route_start(atoi(argv[4]),atoi(argv[5])));u32 deadline=s_now_ms()+10000;
 while(route_status==R_CALC&&time_before_u32(s_now_ms(),deadline))route_service_until(s_now_ms()+12);
 assert(route_status==R_READY&&route_n>1);gps.mph10=300;for(int i=1;i<route_n;i++){int h=nav_heading10(route_lat[0],route_lon[0],route_lat[i],route_lon[i]);if(h>=0){gps.course=track_course=display_course=h;break;}}update_road_context();assert(gps.lat>=mh.minlat-2&&gps.lat<=mh.maxlat+2&&gps.lon>=mh.minlon-2&&gps.lon<=mh.maxlon+2);assert(get_tile(6,tile_div(gps.lon,6),tile_div(gps.lat,6)));screen=SCR_MAP;pref.theme=THEME_DAY;draw();capture(argv[1],"day");pref.theme=THEME_NIGHT;draw();capture(argv[1],"night");screen=SCR_SET;set_focus=2;draw();capture(argv[1],"settings");printf("Global native routing: %s %d points, %u metres, %u ds, native index width %zu, metadata limit %d flags %d\n",region_iso,route_n,route_path_m(),route_result_cost,sizeof(struct CpmIndex),roadctx.speed,roadctx.speed_flags);
 if(mh.minlon< -8388608){int tx=tile_div(gps.lon,6),ty=tile_div(gps.lat,6);assert(tx< -32768);assert(get_tile(6,tx,ty));struct CprTile t;assert(route_find_tile(tx,ty,&t));}
 return 0;}
