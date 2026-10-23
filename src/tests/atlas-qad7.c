/* Real UK atlas regression: frame scheduling, paged regional IDs and repair. */
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
static int force_paged=0;
static void*test_mmap(void*a,size_t n,int prot,int flags,int fd,off_t off){
 if(force_paged&&fd>=0)return MAP_FAILED;
 return mmap(a,n,prot,flags,fd,off);
}
static int test_open(const char*p,int f,int m){
 int flags=f&3;if(f&64)flags|=O_CREAT;if(f&512)flags|=O_TRUNC;if(f&1024)flags|=O_APPEND;if(f&2048)flags|=O_NONBLOCK;
 return open(!strcmp(p,"/proc/meminfo")?CLIO_HOST_ROOT "meminfo":p,flags,m);
}
#define mmap test_mmap
#define open test_open
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#undef mmap
#undef open
#include <assert.h>
#include <string.h>

static void position(s32 la,s32 lo){gps.fix=1;gps.mph10=0;gps.lat=track_lat=la;gps.lon=track_lon=lo;track_valid=1;}
static unsigned finish(void){
 unsigned calls=0;
 while(route_status==R_CALC){assert(++calls<10000);route_service_until(s_now_ms()+ROUTE_INITIAL_SLICE_MS);}
 assert(route_status==R_READY);return calls;
}
int main(int argc,char**argv){
 assert(argc==2);assert(load_map()&&atlas_ready&&atlas_nr>1);
 force_paged=!strcmp(argv[1],"paged");assert(load_route_data());
 pref.debug=0;atlas_report();assert(access(ROUTING_STATUS_PATH,F_OK)!=0);
 /* Use repeated IDs in each namespace and revisit old packs. Expected offsets
  * come independently from the selected file, never from the runtime cache. */
 for(u32 pass=0;pass<2;pass++)for(u32 i=0;i<atlas_nr;i++){
  u32 r=pass?atlas_nr-1-i:i;assert(atlas_open_route((int)r));assert(qadjmode==7&&qadjok);
  assert(qadj_map_ok==!force_paged);
  u32 ids[3]={0,rh.nodes>17?17:0,rh.nodes/2};
  for(int k=0;k<3;k++){u32 got=0,want=0;assert(qadj5_index_get(ids[k],&got));assert(s_lseek(qadjfd,qh5.index_off+ids[k]*4u,0)>=0);assert(s_read(qadjfd,&want,4)==4);assert(got==want);}
 }
 position(5497548,-164050);pref.debug=1;assert(route_start(5496800,-160800));
 u32 start=s_now_ms();unsigned calls=finish();u32 elapsed=s_now_ms()-start,queries=atlas_cost_queries;
 assert(queries==1&&atlas_sweeps_done==3&&atlas_sweep_expanded>0);assert(!atlas_distance_queries&&route_prof_packet_pts>0);
 assert(atlas_phase==AP_LEG&&route_exact_to==ROUTE_NOARC);
 u32 cost=route_result_cost;int region=atlas_route_region;
 s32 endlat=route_lat[route_n-1],endlon=route_lon[route_n-1];
 /* A small deviation must retain the regional plan and QAD7 suffix repair.
  * Model a later leg's exact start too: once GPS reseeds it, movement checks
  * must no longer be suppressed by that obsolete gateway start. */
 route_exact_from=route_start_snap.node;
 position(5497598,-164050);struct RouteSnapCandidate c;int idx=-1;
 assert(route_snap_car_adherence(route_dest_comp,&c,&idx)&&idx<0);
 assert(route_retarget_car(&c)&&route_repair_active&&atlas_active);
 assert(route_exact_from==ROUTE_NOARC&&route_exact_to==ROUTE_NOARC);
 unsigned repaircalls=finish();
 assert(atlas_cost_queries==queries&&atlas_route_region==region&&qadjmode==7);
 assert(route_lat[route_n-1]==endlat&&route_lon[route_n-1]==endlon&&route_prof_packet_pts>0);
 /* Compare the atlas result with an ordinary regional CH query, then request
  * the same coordinate as a destination: endpoint caches must be per request. */
 route_cancel();position(5497548,-164050);assert(regional_route_start(5496800,-160800));finish();assert(route_result_cost==cost);
 struct CprNode node;assert(route_read_node(route_start_snap.node,&node));
 route_cancel();position(node.lat,node.lon);assert(route_start(node.lat,node.lon));finish();assert(!route_result_cost&&!route_result_m&&route_n==1);
 /* Repair a real cross-pack journey before its gateway, then execute the
  * retained transfer. The destination and future totals must survive repair. */
 route_cancel();position(5497548,-164050);assert(route_start(5520000,-170000));finish();
 u32 gateway=route_exact_to,future=atlas_future_cost,future_m=atlas_future_m,gatewayqueries=atlas_cost_queries;
 assert(gateway!=ROUTE_NOARC&&future>0&&future_m>0&&atlas_distance_queries==1);
 endlat=route_lat[route_n-1];endlon=route_lon[route_n-1];
 position(5497598,-164050);assert(route_snap_car_adherence(route_dest_comp,&c,&idx)&&idx<0);
 assert(route_retarget_car(&c)&&route_repair_active);finish();
 assert(atlas_cost_queries==gatewayqueries&&route_exact_to==gateway&&atlas_future_cost==future&&atlas_future_m==future_m);
 assert(route_lat[route_n-1]==endlat&&route_lon[route_n-1]==endlon&&atlas_dest_lat==5520000&&atlas_dest_lon==-170000);
 for(int legs=0;route_exact_to!=ROUTE_NOARC;legs++){
  assert(legs<20);position(route_lat[route_n-1],route_lon[route_n-1]);route_head=route_n>1?route_n-2:0;
  route_service_until(s_now_ms()+ROUTE_INITIAL_SLICE_MS);finish();
 }
 assert(!atlas_future_cost&&!atlas_future_m&&geo_seg_m(route_lat[route_n-1],route_lon[route_n-1],5520000,-170000)<1000);
 /* The reported slow Newcastle-London journey must not regress to a search
  * per gateway. Verify every leg and the previous exact result as well as work
  * counts; a fast Mac timer alone would miss the 400 MHz device regression. */
 route_cancel();position(5497548,-164050);assert(route_start(5150740,-12780));start=s_now_ms();unsigned longcalls=finish();
 u32 first_ms=s_now_ms()-start,expanded=atlas_sweep_expanded,sweeps=atlas_sweep_runs,total=0,metres=0;int legs=0;
 assert(atlas_cost_queries==1&&atlas_distance_queries==1&&atlas_sweep_runs<=ROUTE_DEST_CAND_MAX+1&&expanded<5000&&atlas_best==154867&&route_diag_qad7_expanded>0&&!route_diag_cpr_expanded);
 while(1){assert(++legs<20);total=sat_add32(total,route_result_cost);metres=sat_add32(metres,route_result_m);
  if(route_exact_to==ROUTE_NOARC)break;
  position(route_lat[route_n-1],route_lon[route_n-1]);route_head=route_n>1?route_n-2:0;
  route_service_until(s_now_ms()+ROUTE_INITIAL_SLICE_MS);finish();
 }
 assert(total==154867&&metres==457401&&!atlas_future_cost&&!atlas_future_m);
 char report[1024];FILE*f=fopen(ROUTING_STATUS_PATH,"rb");assert(f);size_t bytes=fread(report,1,sizeof(report)-1,f);report[bytes]=0;fclose(f);
 assert(strstr(report,"ATLAS ENDPOINT SWEEPS 1")&&strstr(report,"QAD_MODE=7")&&strstr(report,"CPR_EXPANDED=0"));
 pref.debug=0;remove_stale_logs();atlas_report();assert(access(ROUTING_STATUS_PATH,F_OK)!=0);
 printf("%s QAD7: local %u point query and shared endpoint sweeps, %u calls (%u host ms), repair %u calls; regional cost %u ds, all %u pack caches passed.\n",argv[1],queries,calls,elapsed,repaircalls,cost,atlas_nr);
 printf("Newcastle-London: %u point query + %u sweeps (%u states), %u calls (%u host ms), %d legs, %u ds/%u m match prior result, 1 MiB query arena.\n",atlas_cost_queries,sweeps,expanded,longcalls,first_ms,legs,total,metres);
 return 0;
}
