#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
static void position(s32 la,s32 lo){gps.fix=1;gps.mph10=0;gps.lat=la;gps.lon=lo;track_lat=la;track_lon=lo;track_valid=1;}
int main(int argc,char**argv){assert(argc==2);assert(load_map()&&atlas_ready&&atlas_nr==3&&load_route_data());position(5500000,-168840);assert(route_start(5500000,-142456));int legs=0,seen=0,detour=0;u32 total=0;
 for(int step=0;step<10000;step++){
  route_service_until(s_now_ms()+20);assert(route_status==R_CALC||route_status==R_READY);if(route_status!=R_READY)continue;assert(++legs<20);seen|=1<<atlas_route_region;total=sat_add32(total,route_result_cost);
  for(int i=0;i<route_n;i++)if(route_lat[i]==5500500&&route_lon[i]==-163540)detour=1;
  if(route_exact_to==ROUTE_NOARC)break;assert(atlas_future_cost>0&&atlas_future_m>0&&route_path_m()>route_result_m);position(route_lat[route_n-1],route_lon[route_n-1]);route_head=route_n>1?route_n-2:0;
  trip.active=1;trip.count=1;trip.active_idx=0;trip.paused=0;trip.target[0].lat=gps.lat;trip.target[0].lon=gps.lon;arrival_ticks=ARRIVAL_TICKS;trip_service();assert(!trip.paused&&modal==MOD_NONE);trip.active=0;
  route_service_until(s_now_ms()+20);
 }
 assert(legs>=3&&seen==7&&detour&&route_status==R_READY&&route_exact_to==ROUTE_NOARC&&atlas_plan_n>0);assert(total==(u32)atoi(argv[1]));assert(!atlas_future_cost&&!atlas_future_m);printf("Three-pack atlas: %d legs, all three regions, explicit middle overlay, required detour, %u ds matches unsplit; future distance/cost and no gateway arrival passed.\n",legs,total);return 0;}
