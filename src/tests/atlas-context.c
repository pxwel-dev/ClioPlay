#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
static void position(s32 la,s32 lo){gps.fix=1;gps.mph10=0;gps.lat=la;gps.lon=lo;track_lat=la;track_lon=lo;track_valid=1;}
static u32 journey(int force_overlay,int from_context){assert(route_start(5500000,-142456));if(from_context)assert(route_start_snap.node>=rh.phys_nodes);
 if(force_overlay){/* Ignore the optional direct upper bound to exercise the
  * portal planner even though complete relation geometry also permits a local route. */route_arena_release();atlas_probe=0;atlas_phase=AP_SEARCH;atlas_direct_cost=ROUTE_INF;atlas_search_init=0;route_status=R_CALC;}
 u32 total=0;int legs=0,via=0;
 for(int step=0;step<10000;step++){route_service_until(s_now_ms()+20);assert(route_status==R_CALC||route_status==R_READY);if(route_status!=R_READY)continue;assert(++legs<20);total=sat_add32(total,route_result_cost);for(int i=0;i<route_n;i++)if(route_lat[i]==5500500&&route_lon[i]==-155840)via=1;if(route_exact_to==ROUTE_NOARC)break;position(route_lat[route_n-1],route_lon[route_n-1]);route_head=route_n>1?route_n-2:0;route_service_until(s_now_ms()+20);}
 assert(route_status==R_READY&&route_exact_to==ROUTE_NOARC&&via);if(force_overlay)assert(legs>=2);return total;
}
int main(int argc,char**argv){assert(argc==3&&load_map()&&atlas_ready&&load_route_data());position(5500000,-168840);u32 a=journey(1,0);assert(a==(u32)atoi(argv[1]));atlas_cancel();route_cancel();assert(atlas_open_route(0));position(5500000,-167840);u32 b=journey(1,1);assert(b==(u32)atoi(argv[2]));
 // Already at a one-way road's sink must produce a zero route, not a false
 // unreachable result or a fabricated backwards connector over the final road.
 atlas_cancel();route_exact_from=route_exact_to=ROUTE_NOARC;position(5500000,-142456);assert(regional_route_start(5500000,-142456));for(int i=0;i<1000&&route_status==R_CALC;i++)regional_service_until(s_now_ms()+20);assert(route_status==R_READY&&!route_result_cost&&!route_result_m&&route_n==1);
 printf("Long via-way atlas: %u/%u ds equal unsplit, forbidden faster shortcut never used, active context preserved across cut planning, valid pre/post default transfers and zero route at one-way sink passed.\n",a,b);return 0;}
