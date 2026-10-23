#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>

static void position(s32 lat,s32 lon){gps.fix=1;gps.mph10=0;gps.lat=lat;gps.lon=lon;track_lat=lat;track_lon=lon;track_valid=1;}
int main(int argc,char**argv){
 assert(argc==5);s32 slat=atoi(argv[1]),slon=atoi(argv[2]),tlat=atoi(argv[3]),tlon=atoi(argv[4]);
 position(slat,slon);assert(load_map()&&atlas_ready&&load_route_data()&&load_regions());
 assert(route_start(tlat,tlon));int legs=0,regions=0;u8 seen[ATLAS_MAX_REGIONS]={0};u32 total=0,metres=0,planned=0,begin=s_now_ms(),first_ready=0;
 for(int step=0;step<100000;step++){
  route_service_until(s_now_ms()+20);
  if(route_status!=R_CALC&&route_status!=R_READY){fprintf(stderr,"Route failed: status=%d phase=%u step=%d\n",route_status,atlas_phase,step);return 1;}
  if(route_status!=R_READY)continue;
  if(!legs){planned=atlas_best;first_ready=s_now_ms()-begin;}
  assert(++legs<512&&route_n>0);if(!seen[atlas_route_region]){seen[atlas_route_region]=1;regions++;}
  total=sat_add32(total,route_result_cost);metres=sat_add32(metres,route_result_m);
  if(route_exact_to==ROUTE_NOARC){assert(!atlas_future_cost&&!atlas_future_m);break;}
  assert(atlas_future_cost>0&&route_path_m()>=route_result_m);
  position(route_lat[route_n-1],route_lon[route_n-1]);route_head=route_n>1?route_n-2:0;
  trip.active=1;trip.count=1;trip.active_idx=0;trip.paused=0;trip.target[0].lat=gps.lat;trip.target[0].lon=gps.lon;arrival_ticks=ARRIVAL_TICKS;
  trip_service();assert(!trip.paused&&modal==MOD_NONE);trip.active=0;
  route_service_until(s_now_ms()+20);
 }
 assert(route_status==R_READY&&route_exact_to==ROUTE_NOARC&&legs>0&&total<=planned+5);
 assert(geo_seg_m(route_lat[route_n-1],route_lon[route_n-1],tlat,tlon)<1000);
 printf("{\"legs\":%d,\"regions\":%d,\"cost_ds\":%u,\"planned_ds\":%u,\"metres\":%u,\"first_leg_host_ms\":%u,\"total_host_ms\":%u}\n",legs,regions,total,planned,metres,first_ready,s_now_ms()-begin);
 return 0;
}
