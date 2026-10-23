#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>

int main(int argc,char**argv){
 assert(argc==3&&load_atlas()&&load_route_data());u32 portal=(u32)atoi(argv[1]);int candidate=atoi(argv[2]);
 gps.fix=1;gps.lat=track_lat=5500000;gps.lon=track_lon=-165000;track_valid=1;
 route_exact_from=portal;assert(regional_route_start(5500000,-164500));
 for(int i=0;i<1000&&route_status==R_CALC;i++)regional_service_until(s_now_ms()+80);
 assert(route_status==R_READY&&route_dest_idx==candidate);u32 expected=route_result_cost;
 assert(candidate?expected==246:expected==2024);
 route_cancel();atlas_active=1;atlas_target_region=0;atlas_tn=0;
 for(u32 i=0;i<atlas_regions[0].count;i++){const struct AtlasMember*m=atlas_member(i);struct AtlasAttach a={m->global,m->local,ROUTE_INF,0,0,0};atlas_targets[atlas_tn++]=a;}
 atlas_sort_attachments(atlas_targets,atlas_tn);atlas_dest_lat=5500000;atlas_dest_lon=-164500;atlas_direct_cost=ROUTE_INF;
 assert(atlas_sweep_begin(1));
 for(int i=0;i<1000&&!(atlas_sweeps_done&2);i++){
  if(atlas_phase==AP_TARGET_SWEEP)atlas_sweep_until(s_now_ms()+80);
  if(atlas_phase==AP_TARGET_SPREAD)atlas_target_spread_until(s_now_ms()+80);
 }
 assert(atlas_sweeps_done&2);struct AtlasAttach*a=atlas_attachment(atlas_targets,atlas_tn,portal);
 assert(a&&a->cost==expected&&!atlas_reverse);
 printf("Ordered destination candidate %d: %u ds matches regional query; reverse portal propagation and temporary index cleanup passed.\n",candidate,expected);
 return 0;
}
