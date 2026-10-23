#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static void position(s32 la,s32 lo){gps.fix=1;gps.mph10=0;gps.lat=la;gps.lon=lo;track_lat=la;track_lon=lo;track_valid=1;}
int main(int argc,char**argv){assert(argc==2);assert(load_map());assert(atlas_ready&&atlas_nr==2);assert(load_route_data());assert(load_font());position(5500000,-164840);assert(route_start(5500000,-162840));int legs=0,sawdetour=0,sawwest=0,saweast=0,saw_unreachable=0;u32 total=0;
 for(int step=0;step<10000;step++){
  route_service_until(s_now_ms()+20u);assert(route_status==R_CALC||route_status==R_READY);
  if(route_status!=R_READY)continue;
  for(u32 i=0;i<atlas_sn;i++)if(atlas_sources[i].known&&atlas_sources[i].cost==ROUTE_INF)saw_unreachable=1;
  legs++;assert(legs<20);total=sat_add32(total,route_result_cost);sawwest|=atlas_route_region==0;saweast|=atlas_route_region==1;
  for(int i=0;i<route_n;i++)if(route_lat[i]==5500500&&route_lon[i]==-163740)sawdetour=1;
  if(route_exact_to==ROUTE_NOARC)break;
  assert(route_n>0);s32 la=route_lat[route_n-1],lo=route_lon[route_n-1];position(la,lo);route_head=route_n>1?route_n-2:0;route_service_until(s_now_ms()+20u);
 }
 assert(legs>=2&&sawwest&&saweast&&sawdetour&&saw_unreachable);assert(route_status==R_READY&&route_exact_to==ROUTE_NOARC);assert(route_n>0&&route_lat[route_n-1]==5500000&&route_lon[route_n-1]==-162840);assert(total==(u32)atoi(argv[1]));
 assert(atlas_owner(5500000,-163841)==0&&atlas_owner(5500000,-163840)==1);int tx=tile_div(-164000,6),ty=tile_div(5500000,6);assert(get_tile(6,tx,ty));tx=tile_div(-163500,6);assert(get_tile(6,tx,ty));
 printf("Native atlas: %d legs, %u ds matches the unsplit restricted graph; both packs, required detour, exact final endpoint and tile ownership and unreachable attachment recovery passed.\n",legs,total);
 atlas_cancel();route_cancel();assert(!atlas_active&&route_status==R_IDLE);return 0;}
