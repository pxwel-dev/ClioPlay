#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
static void position(s32 la,s32 lo){gps.fix=1;gps.mph10=0;gps.lat=la;gps.lon=lo;track_lat=la;track_lon=lo;track_valid=1;}
int main(int argc,char**argv){assert(argc==4);s32 dest_lat=atoi(argv[2]),dest_lon=atoi(argv[3]);assert(load_map()&&atlas_ready&&load_route_data()&&load_regions());position(4335340,-178050);assert(route_start(dest_lat,dest_lon));int legs=0,fr=0,es=0;u32 total=0;
 for(int step=0;step<100000;step++){
  route_service_until(s_now_ms()+20);assert(route_status==R_CALC||route_status==R_READY);if(route_status!=R_READY)continue;assert(++legs<100);total=sat_add32(total,route_result_cost);
  for(int i=0;i<route_n;i++){region_resolve(route_lat[i],route_lon[i]);fr|=streq(region_iso,"FR");es|=streq(region_iso,"ES");}
  if(route_exact_to==ROUTE_NOARC)break;assert(route_n>0);s32 la=route_lat[route_n-1],lo=route_lon[route_n-1];position(la,lo);route_head=route_n>1?route_n-2:0;
  // A gateway must never trigger the itinerary arrival state, even when the
  // remaining target is spatially close to that gateway.
  trip.active=1;trip.count=1;trip.active_idx=0;trip.paused=0;trip.target[0].lat=la;trip.target[0].lon=lo;arrival_ticks=ARRIVAL_TICKS;trip_service();assert(!trip.paused&&modal==MOD_NONE);trip.active=0;
  route_service_until(s_now_ms()+20);
 }
 assert(legs>=2&&fr&&es&&route_status==R_READY&&route_exact_to==ROUTE_NOARC);u32 planned=atlas_best;
 atlas_cancel();route_exact_from=route_exact_to=ROUTE_NOARC;position(4335340,-178050);copystr(atlas_route_path,sizeof(atlas_route_path),argv[1]);appendstr(atlas_route_path,sizeof(atlas_route_path),"/route.cpr");copystr(atlas_qadj_path,sizeof(atlas_qadj_path),argv[1]);appendstr(atlas_qadj_path,sizeof(atlas_qadj_path),"/route.qadj");assert(load_route_data());assert(regional_route_start(dest_lat,dest_lon));for(int i=0;i<100000&&route_status==R_CALC;i++)regional_service_until(s_now_ms()+20);assert(route_status==R_READY);u32 reference=route_result_cost;
 printf("Actual France–Spain border atlas: %d legs, %u ds (%u planned) versus %u ds unsplit native; FR/ES crossing and no gateway arrival.\n",legs,total,planned,reference);fflush(stdout);assert(total>=reference-5&&total<=reference+5);return 0;}
