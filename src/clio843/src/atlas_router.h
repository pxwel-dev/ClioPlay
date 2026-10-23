/* Cross-pack CH planning, cooperatively scheduled within the UI route budget.
 * One upward CH sweep per endpoint reaches its first gateways. The overlay
 * contains every CH arc between gateways, which the builder contracts last.
 * This is the exact eliminated graph, not thousands of point-to-point trials.
 * No coordinate connector or arrival-context reset crosses a pack boundary. */
#define AP_NONE 0
#define AP_DIRECT 1
#define AP_SEARCH 2
#define AP_SOURCE 3
#define AP_TARGET 4
#define AP_LEG 5
#define AP_SOURCE_SWEEP 6
#define AP_TARGET_SWEEP 7
#define AP_TARGET_SPREAD 8
#define ATLAS_MAX_PATH 4096u
struct AtlasAttach {u32 global,local,cost,metres,known,bound;};
static struct AtlasAttach atlas_sources[ATLAS_MAX_ATTACH],atlas_targets[ATLAS_MAX_ATTACH];
static u32 atlas_sn=0,atlas_tn=0,atlas_phase=0,atlas_query_index=0;
static u32 atlas_dist[ATLAS_MAX_NODES],atlas_prev[ATLAS_MAX_NODES],atlas_prev_edge[ATLAS_MAX_NODES],atlas_hpos[ATLAS_MAX_NODES],atlas_heap[ATLAS_MAX_NODES+1];
static u16 atlas_target_index[ATLAS_MAX_NODES];
static u32 atlas_hn=0,atlas_search_init=0,atlas_best=ROUTE_INF,atlas_best_node=ROUTE_NOARC,atlas_best_target=ROUTE_NOARC,atlas_direct_cost=ROUTE_INF,atlas_direct_m=0;
static u32 atlas_plan[ATLAS_MAX_PATH],atlas_plan_n=0,atlas_plan_at=0,atlas_plan_source=0,atlas_plan_target=0,atlas_leg_end=0,atlas_last_global=ROUTE_NOARC;
static int atlas_source_region=-1,atlas_target_region=-1;
static u32 atlas_cost_queries=0,atlas_distance_queries=0;
static u32 atlas_target_candidate=0,atlas_target_count=0,atlas_target_remaining=0,atlas_sweep_runs=0;
static u32 atlas_target_skip=0;
static u32 atlas_sweeps_done=0,atlas_sweep_expanded=0,atlas_started_ms=0,atlas_report_ms=0;
static int atlas_report_status=-1;
static void atlas_report(void){
 if(!pref.debug)return;
 u32 now=s_now_ms();if(route_status==atlas_report_status&&(route_status!=R_CALC||now-atlas_report_ms<5000u))return;
 atlas_report_status=route_status;atlas_report_ms=now;char b[768];b[0]=0;
 appendstr(b,sizeof(b),"ATLAS ENDPOINT SWEEPS 1\nSTATUS=");append_int(b,sizeof(b),route_status);
 appendstr(b,sizeof(b)," PHASE=");append_int(b,sizeof(b),(int)atlas_phase);appendstr(b,sizeof(b)," ELAPSED_MS=");append_int(b,sizeof(b),(int)(now-atlas_started_ms));
 appendstr(b,sizeof(b),"\nFROM=");append_int(b,sizeof(b),atlas_origin_lat);appendstr(b,sizeof(b),",");append_int(b,sizeof(b),atlas_origin_lon);
 appendstr(b,sizeof(b)," TO=");append_int(b,sizeof(b),atlas_dest_lat);appendstr(b,sizeof(b),",");append_int(b,sizeof(b),atlas_dest_lon);
 appendstr(b,sizeof(b),"\nPACK=");if(atlas_route_region>=0)appendstr(b,sizeof(b),atlas_regions[atlas_route_region].path);
 appendstr(b,sizeof(b)," QAD_MODE=");append_int(b,sizeof(b),qadjmode);appendstr(b,sizeof(b)," MMAP=");append_int(b,sizeof(b),qadj_map_ok);
 appendstr(b,sizeof(b),"\nSWEEPS_DONE=");append_int(b,sizeof(b),(int)atlas_sweeps_done);appendstr(b,sizeof(b)," SWEEP_EXPANDED=");append_int(b,sizeof(b),(int)atlas_sweep_expanded);
 appendstr(b,sizeof(b)," SWEEP_RUNS=");append_int(b,sizeof(b),(int)atlas_sweep_runs);
 appendstr(b,sizeof(b)," COST_QUERIES=");append_int(b,sizeof(b),(int)atlas_cost_queries);appendstr(b,sizeof(b)," DISTANCE_QUERIES=");append_int(b,sizeof(b),(int)atlas_distance_queries);
 appendstr(b,sizeof(b),"\nQAD7_EXPANDED=");append_int(b,sizeof(b),(int)route_diag_qad7_expanded);appendstr(b,sizeof(b)," CPR_EXPANDED=");append_int(b,sizeof(b),(int)route_diag_cpr_expanded);
 appendstr(b,sizeof(b),"\nROUTE_COST_DS=");append_int(b,sizeof(b),(int)route_result_cost);appendstr(b,sizeof(b)," FUTURE_COST_DS=");append_int(b,sizeof(b),(int)atlas_future_cost);
 appendstr(b,sizeof(b),"\nARENA_KB=");append_int(b,sizeof(b),(int)(route_arena_bytes>>10));appendstr(b,sizeof(b),"\n");
 long fd=s_open(ROUTING_STATUS_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd>=0){s_write((int)fd,b,slen(b));s_close((int)fd);}
}
static void atlas_index_release(void);
static int atlas_open_route(int region){if(region==atlas_route_region&&routeok)return 1;route_arena_release();qadj_unmap();if(qadjfd>=0){s_close(qadjfd);qadjfd=-1;}route_geom_unmap();if(!atlas_set_paths(region))return 0;return load_route_data();}
static void atlas_cancel(void){atlas_index_release();atlas_active=atlas_probe=0;atlas_phase=AP_NONE;atlas_future_m=atlas_future_cost=0;atlas_plan_n=atlas_plan_at=0;atlas_search_init=0;}
static void atlas_fail(int status){atlas_cancel();route_status=status;route_arena_release();route_exact_from=route_exact_to=ROUTE_NOARC;}
static int atlas_heap_less(u32 a,u32 b){return atlas_dist[a]<atlas_dist[b]||(atlas_dist[a]==atlas_dist[b]&&a<b);}
static void atlas_heap_update(u32 node){u32 at=atlas_hpos[node];if(at==ROUTE_NOARC)return;if(!at){at=++atlas_hn;atlas_heap[at]=node;atlas_hpos[node]=at;}while(at>1){u32 parent=at>>1,v=atlas_heap[parent];if(!atlas_heap_less(node,v))break;atlas_heap[at]=v;atlas_hpos[v]=at;at=parent;}atlas_heap[at]=node;atlas_hpos[node]=at;}
static u32 atlas_heap_pop(void){u32 node=atlas_heap[1],v=atlas_heap[atlas_hn--];atlas_hpos[node]=ROUTE_NOARC;if(atlas_hn){u32 at=1;while(at*2<=atlas_hn){u32 child=at*2;if(child<atlas_hn&&atlas_heap_less(atlas_heap[child+1],atlas_heap[child]))child++;if(!atlas_heap_less(atlas_heap[child],v))break;atlas_heap[at]=atlas_heap[child];atlas_hpos[atlas_heap[at]]=at;at=child;}atlas_heap[at]=v;atlas_hpos[v]=at;}return node;}
static void atlas_sort_attachments(struct AtlasAttach*a,u32 n){for(u32 gap=n/2;gap;gap/=2)for(u32 i=gap;i<n;i++){struct AtlasAttach v=a[i];u32 j=i;while(j>=gap&&a[j-gap].local>v.local){a[j]=a[j-gap];j-=gap;}a[j]=v;}}
static struct AtlasAttach*atlas_attachment(struct AtlasAttach*a,u32 n,u32 local){u32 lo=0,hi=n;while(lo<hi){u32 mid=lo+(hi-lo)/2;if(a[mid].local<local)lo=mid+1;else hi=mid;}return lo<n&&a[lo].local==local?&a[lo]:0;}
/* Reverse portal adjacency is temporary, bounded by this region's exported
 * edges (10 bytes each), and released before final route reconstruction. */
static u8*atlas_reverse=0;static u32 atlas_reverse_bytes=0,atlas_reverse_n=0;
static u32 *atlas_reverse_id=0,*atlas_reverse_next=0;static u16*atlas_reverse_from=0;

static int atlas_target_started=0;
static void atlas_index_release(void){if(atlas_reverse)s_munmap(atlas_reverse,atlas_reverse_bytes);atlas_reverse=0;atlas_reverse_bytes=atlas_reverse_n=0;}
static int atlas_index_begin(void){
 atlas_index_release();for(u32 u=0;u<atlas_nn;u++){atlas_prev_edge[u]=ROUTE_NOARC;atlas_target_index[u]=65535;}
 for(u32 i=0;i<atlas_tn;i++)atlas_target_index[atlas_targets[i].global]=(u16)i;
 for(u32 i=0;i<atlas_ne;i++){const struct AtlasEdge*e=atlas_edge(i);if(e->region==(u32)atlas_target_region)atlas_reverse_n++;}
 if(!atlas_reverse_n)return 1;
 atlas_reverse_bytes=atlas_reverse_n*10u;
 atlas_reverse=(u8*)s_mmap2(0,atlas_reverse_bytes,PROT_READ_|PROT_WRITE_,MAP_PRIVATE_|MAP_ANONYMOUS_,-1,0);
 if(!atlas_reverse)return 0;
 atlas_reverse_id=(u32*)atlas_reverse;atlas_reverse_next=atlas_reverse_id+atlas_reverse_n;atlas_reverse_from=(u16*)(atlas_reverse_next+atlas_reverse_n);
 u32 at=0;for(u32 u=0;u<atlas_nn;u++){const struct AtlasNode*n=atlas_node(u);
  for(u32 j=0;j<n->count;j++){u32 id=n->first+j;const struct AtlasEdge*e=atlas_edge(id);if(e->region!=(u32)atlas_target_region)continue;
   if(at>=atlas_reverse_n)return 0;
   atlas_reverse_id[at]=id;atlas_reverse_from[at]=(u16)u;atlas_reverse_next[at]=atlas_prev_edge[e->to];atlas_prev_edge[e->to]=at++;
  }
 }
 return at==atlas_reverse_n;
}
/* One reverse sweep per destination candidate, in normal snapping order.
 * Weak components do not prove directional reachability. Propagating through
 * this region's reverse portal graph gives the exact regional cost and makes
 * later-candidate fallback legal only where every earlier road is unreachable. */
static int atlas_seed_target(void){
 if(atlas_target_candidate>=atlas_target_count)return 1;
 struct RouteSnapCandidate*dest=&route_dest_cand[atlas_target_candidate];
 if(!atlas_target_candidate)for(u32 i=0;i<atlas_tn;i++){
  const struct AtlasNode*n=atlas_node(atlas_targets[i].global);struct RouteSnapCandidate c;
  if(!n)return 0;if(n->lat!=dest->snap_lat||n->lon!=dest->snap_lon)continue;
  if(!route_exact_snap(atlas_targets[i].local,&c))return 0;
  if(route_same_endpoint(&c,dest)){struct ChState*s=ch_state(c.node,1);if(!s)return 0;s->db=0;if(!ch_heap_insert(1,s))return 0;}
 }
 struct CprBase b;if(!route_read_base(dest->arc,&b))return 0;
 for(u32 k=0;k<b.src_count;k++){
  u32 node;if(!route_read_srcref(b.src_first+k,&node))return 0;struct ChState*s=ch_state(node,1);if(!s)return 0;
  if(dest->extra<s->db){s->db=dest->extra;if(!ch_heap_insert(1,s))return 0;}
 }
 return 1;
}
static int atlas_sweep_begin(int dir){
 atlas_phase=dir?AP_TARGET_SWEEP:AP_SOURCE_SWEEP;atlas_probe=1;route_exact_from=route_exact_to=ROUTE_NOARC;
 if(!atlas_open_route(dir?atlas_target_region:atlas_source_region)){atlas_fail(R_NOFILE);return 0;}
 route_repair_reset();route_arena_release();route_prof_reset();
 struct AtlasAttach*a=dir?atlas_targets:atlas_sources;u32 n=dir?atlas_tn:atlas_sn;
 if(dir){
  atlas_target_count=(u32)route_query_dest_snaps(atlas_dest_lat,atlas_dest_lon);
  if(!atlas_target_started){atlas_target_started=1;atlas_target_candidate=0;atlas_target_remaining=n;
   for(u32 i=0;i<n;i++){a[i].cost=ROUTE_INF;a[i].known=0;a[i].metres=0;}
   if(!atlas_index_begin()){atlas_fail(R_MEMORY);return 0;}
  }
  for(u32 u=0;u<atlas_nn;u++){atlas_dist[u]=ROUTE_INF;atlas_hpos[u]=0;}
 }else for(u32 i=0;i<n;i++){a[i].cost=ROUTE_INF;a[i].known=1;a[i].metres=0;}
 if(!route_arena_prepare()){atlas_fail(R_MEMORY);return 0;}ch_reset();atlas_sweep_runs++;
 if(dir){if(!atlas_seed_target()){atlas_fail(R_MEMORY);return 0;}}
 else if(route_query_start_snap(&route_start_snap)){
  struct ChState*s=ch_state(route_start_snap.node,1);if(!s){atlas_fail(R_MEMORY);return 0;}
  s->df=route_start_snap.extra;if(!ch_heap_insert(0,s)){atlas_fail(R_MEMORY);return 0;}
 }
 route_status=R_CALC;return 1;
}
static int atlas_sweep_until(u32 deadline){
 int dir=atlas_phase==AP_TARGET_SWEEP;struct AtlasAttach*a=dir?atlas_targets:atlas_sources;u32 n=dir?atlas_tn:atlas_sn;unsigned ops=0;
 while(n&&ch_heap_min(dir)!=ROUTE_INF&&(dir||ch_heap_min(dir)<atlas_direct_cost)){
  if((ops++&ROUTE_CLOCK_CHECK_MASK)==0&&!time_before_u32(s_now_ms(),deadline))return 0;
  struct ChState*s=&chst[ch_heap(dir)[1]];struct AtlasAttach*stop=atlas_attachment(a,n,s->node);
  if(stop){(void)ch_heap_pop(dir);if(dir)atlas_dist[stop->global]=ch_dist(s,dir);else stop->cost=ch_dist(s,dir);if(dir)s->sb=1;else s->sf=1;continue;}
  int r=ch_expand_one();if(r<0){atlas_fail(r==-2?R_MEMORY:R_ERROR);return 0;}atlas_sweep_expanded++;
 }
 /* If every seed of a later road already has a legal path to this earlier
  * candidate, it cannot rescue any still-unreachable portal. This is directed
  * reachability evidence, not the weaker component shortcut. */
 if(dir)for(u32 j=atlas_target_candidate+1;j<atlas_target_count;j++)if(!(atlas_target_skip&(1u<<j))){
  struct CprBase b;if(!route_read_base(route_dest_cand[j].arc,&b)){atlas_fail(R_ERROR);return 0;}
  int all=1;for(u32 k=0;k<b.src_count;k++){u32 node;if(!route_read_srcref(b.src_first+k,&node)){atlas_fail(R_ERROR);return 0;}struct ChState*s=ch_state(node,0);if(!s||s->db==ROUTE_INF){all=0;break;}}
  if(all)atlas_target_skip|=1u<<j;
 }
 route_arena_release();atlas_probe=0;route_status=R_CALC;
 if(dir){atlas_hn=0;for(u32 i=0;i<n;i++)if(atlas_dist[a[i].global]!=ROUTE_INF)atlas_heap_update(a[i].global);atlas_phase=AP_TARGET_SPREAD;}
 else{atlas_sweeps_done|=1u;atlas_phase=AP_SEARCH;atlas_search_init=0;}
 return 1;
}
static int atlas_target_spread_until(u32 deadline){
 unsigned ops=0;while(atlas_hn){
  if((ops++&31u)==0&&!time_before_u32(s_now_ms(),deadline))return 0;
  u32 u=atlas_heap_pop(),du=atlas_dist[u];u16 ti=atlas_target_index[u];if(ti==65535){atlas_fail(R_ERROR);return 0;}
  struct AtlasAttach*a=&atlas_targets[ti];if(!a->known){a->known=1;a->cost=du;atlas_target_remaining--;}
  for(u32 link=atlas_prev_edge[u];link!=ROUTE_NOARC;link=atlas_reverse_next[link]){
   u32 v=atlas_reverse_from[link];const struct AtlasEdge*e=atlas_edge(atlas_reverse_id[link]);u32 nd=sat_add32(du,e->cost);
   if(nd<atlas_dist[v]&&atlas_hpos[v]!=ROUTE_NOARC){atlas_dist[v]=nd;atlas_heap_update(v);}
  }
 }
 if(!atlas_target_candidate&&atlas_target_remaining){
  /* Portals in components absent from every snap candidate cannot be reached
   * by a later candidate either. Avoid repeat sweeps for disconnected islands. */
  for(u32 i=0;i<atlas_tn;i++)if(!atlas_targets[i].known){struct CprNode n;if(!route_read_node(atlas_targets[i].local,&n)){atlas_fail(R_ERROR);return 0;}
   int possible=0;for(u32 c=0;c<atlas_target_count;c++)if(route_dest_has_component(&route_dest_cand[c],n.comp)){possible=1;break;}
   if(!possible){atlas_targets[i].known=1;atlas_target_remaining--;}
  }
 }
 if(atlas_target_remaining){do{atlas_target_candidate++;}while(atlas_target_candidate<atlas_target_count&&(atlas_target_skip&(1u<<atlas_target_candidate)));if(atlas_target_candidate<atlas_target_count)return atlas_sweep_begin(1);}
 for(u32 i=0;i<atlas_tn;i++)atlas_targets[i].known=1;
 atlas_index_release();atlas_sweeps_done|=2u;atlas_phase=AP_SEARCH;atlas_search_init=0;return 1;
}
static void atlas_search_begin(void){atlas_hn=0;atlas_best=atlas_direct_cost;atlas_best_node=atlas_best_target=ROUTE_NOARC;for(u32 n=0;n<atlas_nn;n++){atlas_dist[n]=ROUTE_INF;atlas_prev[n]=atlas_prev_edge[n]=ROUTE_NOARC;atlas_hpos[n]=0;atlas_target_index[n]=65535;}
 for(u32 i=0;i<atlas_tn;i++)atlas_target_index[atlas_targets[i].global]=(u16)i;
 for(u32 i=0;i<atlas_sn;i++){struct AtlasAttach*a=&atlas_sources[i];u32 cost=a->known?a->cost:a->bound;if(cost<atlas_dist[a->global]){atlas_dist[a->global]=cost;atlas_prev_edge[a->global]=0x80000000u|i;atlas_heap_update(a->global);}}
 atlas_search_init=1;
}
static int atlas_query(int phase,u32 idx){atlas_query_index=idx;atlas_phase=(u32)phase;atlas_probe=phase==AP_TARGET&&atlas_targets[idx].known?2:1;if(atlas_probe==2)atlas_distance_queries++;else atlas_cost_queries++;route_exact_from=route_exact_to=ROUTE_NOARC;int region=atlas_source_region,ok=0;
 if(phase==AP_SOURCE)route_exact_to=atlas_sources[idx].local;
 if(phase==AP_TARGET){region=atlas_target_region;route_exact_from=atlas_targets[idx].local;}
 if(!atlas_open_route(region)){atlas_fail(R_NOFILE);return 0;}
 if(phase==AP_SOURCE){const struct AtlasNode*n=atlas_node(atlas_sources[idx].global);if(!n){atlas_fail(R_ERROR);return 0;}ok=regional_route_start(n->lat,n->lon);}else ok=regional_route_start(atlas_dest_lat,atlas_dest_lon);
 if(ok)return 1;
 if(route_status!=R_NOROAD&&route_status!=R_UNREACHABLE){atlas_fail(route_status);return 0;}
 /* An unreachable virtual attachment is an expected refinement result. Consume
  * it immediately: exposing R_UNREACHABLE here would stop the main scheduler
  * before it could try the next portal. */
 if(phase==AP_DIRECT){atlas_direct_cost=ROUTE_INF;atlas_direct_m=0;}else{struct AtlasAttach*a=phase==AP_SOURCE?&atlas_sources[idx]:&atlas_targets[idx];a->known=1;a->cost=ROUTE_INF;a->metres=0;}
 atlas_probe=0;route_arena_release();atlas_phase=AP_SEARCH;atlas_search_init=0;route_status=R_CALC;return 1;
}

static int atlas_request(s32 lat,s32 lon){atlas_cancel();atlas_start_snap_region=atlas_dest_snap_region=-2;route_repair_reset();route_arena_release();route_exact_from=route_exact_to=ROUTE_NOARC;atlas_source_region=atlas_owner(car_lat(),car_lon());atlas_target_region=atlas_owner(lat,lon);
 if(atlas_source_region<0||atlas_target_region<0){route_status=R_NOROAD;return 0;}
 atlas_active=1;atlas_target_started=0;atlas_target_skip=0;atlas_sweep_runs=0;atlas_sweeps_done=atlas_sweep_expanded=0;atlas_cost_queries=atlas_distance_queries=0;atlas_origin_lat=car_lat();atlas_origin_lon=car_lon();atlas_dest_lat=lat;atlas_dest_lon=lon;atlas_plan_n=atlas_plan_at=0;atlas_future_m=atlas_future_cost=0;atlas_sn=atlas_tn=0;atlas_direct_cost=ROUTE_INF;atlas_direct_m=0;
 atlas_started_ms=s_now_ms();atlas_report_status=-1;route_diag_qad7_expanded=route_diag_cpr_expanded=0;
 struct AtlasRegion*s=&atlas_regions[atlas_source_region],*t=&atlas_regions[atlas_target_region];
 for(u32 i=0;i<s->count;i++){const struct AtlasMember*m=atlas_member(s->first+i);if(!m||m->region!=(u32)atlas_source_region){atlas_fail(R_ERROR);return 0;}struct AtlasAttach a={m->global,m->local,ROUTE_INF,0,0,0};atlas_sources[atlas_sn++]=a;}
 for(u32 i=0;i<t->count;i++){const struct AtlasMember*m=atlas_member(t->first+i);if(!m||m->region!=(u32)atlas_target_region){atlas_fail(R_ERROR);return 0;}struct AtlasAttach a={m->global,m->local,ROUTE_INF,0,0,0};atlas_targets[atlas_tn++]=a;}
 atlas_sort_attachments(atlas_sources,atlas_sn);atlas_sort_attachments(atlas_targets,atlas_tn);
 // Complete ways/relations and the overlap may contain the whole journey even
 // when its destination belongs to another core. A legal local route is a valid
 // upper bound and avoids transferring while a long restriction is active.
 (void)atlas_query(AP_DIRECT,0);atlas_report();return atlas_active;
}
static int atlas_start_leg(void){atlas_probe=0;route_exact_from=route_exact_to=ROUTE_NOARC;int region;
 if(atlas_plan_at==0&&atlas_last_global==ROUTE_NOARC){
  region=atlas_source_region;struct AtlasAttach*a=&atlas_sources[atlas_plan_source];route_exact_to=a->local;atlas_last_global=a->global;atlas_leg_end=0;
 }else if(atlas_plan_at<atlas_plan_n){
  const struct AtlasEdge*e=atlas_edge(atlas_plan[atlas_plan_at]);if(!e)return 0;region=(int)e->region;route_exact_from=atlas_local(atlas_last_global,(u32)region);u32 at=atlas_plan_at,last=atlas_last_global;
  while(at<atlas_plan_n){const struct AtlasEdge*x=atlas_edge(atlas_plan[at]);if(!x||x->region!=(u32)region)break;last=x->to;at++;}
  route_exact_to=atlas_local(last,(u32)region);atlas_leg_end=at;atlas_last_global=last;
 }else{
  region=atlas_target_region;route_exact_from=atlas_targets[atlas_plan_target].local;atlas_leg_end=atlas_plan_n;atlas_last_global=ROUTE_NOARC;
 }
 if(!atlas_open_route(region))return 0;
 if((atlas_last_global!=ROUTE_NOARC&&route_exact_to==ROUTE_NOARC)||(atlas_plan_at&&route_exact_from==ROUTE_NOARC))return 0;
 s32 lat=atlas_dest_lat,lon=atlas_dest_lon;if(route_exact_to!=ROUTE_NOARC){const struct AtlasNode*n=atlas_node(atlas_last_global);if(!n)return 0;lat=n->lat;lon=n->lon;}
 atlas_future_m=atlas_future_cost=0;
 if(route_exact_to!=ROUTE_NOARC){for(u32 at=atlas_leg_end;at<atlas_plan_n;at++){const struct AtlasEdge*e=atlas_edge(atlas_plan[at]);if(!e)return 0;atlas_future_m=sat_add32(atlas_future_m,e->metres);atlas_future_cost=sat_add32(atlas_future_cost,e->cost);}struct AtlasAttach*a=&atlas_targets[atlas_plan_target];atlas_future_m=sat_add32(atlas_future_m,a->metres);atlas_future_cost=sat_add32(atlas_future_cost,a->cost);}
 atlas_phase=AP_LEG;return regional_route_start(lat,lon);
}
static int atlas_finish_search(void){
 if(atlas_best==ROUTE_INF){atlas_fail(R_UNREACHABLE);return 0;}
 if(atlas_best_node==ROUTE_NOARC){
  atlas_probe=0;atlas_future_m=atlas_future_cost=0;atlas_phase=AP_LEG;atlas_plan_n=atlas_plan_at=0;atlas_last_global=ROUTE_NOARC;route_exact_from=route_exact_to=ROUTE_NOARC;
  return atlas_open_route(atlas_source_region)&&regional_route_start(atlas_dest_lat,atlas_dest_lon);
 }
 u32 at=atlas_best_node,count=0;while(!(atlas_prev_edge[at]&0x80000000u)){if(count++>=ATLAS_MAX_PATH||atlas_prev[at]>=atlas_nn){atlas_fail(R_ERROR);return 0;}at=atlas_prev[at];}
 u32 si=atlas_prev_edge[at]&0x7fffffffu,ti=atlas_best_target;if(si>=atlas_sn||ti>=atlas_tn){atlas_fail(R_ERROR);return 0;}
 if(!atlas_sources[si].known){(void)atlas_query(AP_SOURCE,si);return 1;}if(!atlas_targets[ti].known){(void)atlas_query(AP_TARGET,ti);return 1;}
 /* Only the winning target attachment needs geometry to measure the future
  * distance. The source attachment is measured while drawing the first leg. */
 if(atlas_targets[ti].known<2){(void)atlas_query(AP_TARGET,ti);return 1;}
 /* The lower-bound winner is now fully realized. Keep the user's final target
  * separate from transient region gateways and rebuild the first leg at GPS. */
 if(geo_seg_m(atlas_origin_lat,atlas_origin_lon,car_lat(),car_lon())>50){return atlas_request(atlas_dest_lat,atlas_dest_lon);}
 atlas_plan_n=count;at=atlas_best_node;while(count){atlas_plan[--count]=atlas_prev_edge[at];at=atlas_prev[at];}
 atlas_plan_source=si;atlas_plan_target=ti;atlas_plan_at=0;atlas_last_global=ROUTE_NOARC;return atlas_start_leg();
}
static void atlas_service_until(u32 deadline){
 /* The UI calls this once per 100 ms frame. A completed cheap CH probe must
  * advance straight into the next one while this slice still has time;
  * returning after every gateway inserts a frame of idle time per trial. */
 for(;;){
 if(!atlas_active){if(gps.fix&&route_status==R_IDLE){int r=atlas_owner(car_lat(),car_lon());if(r>=0)(void)atlas_open_route(r);}regional_service_until(deadline);return;}
 if(atlas_phase==AP_LEG){
  regional_service_until(deadline);
  if(route_status==R_READY&&route_exact_to!=ROUTE_NOARC&&route_n>0&&route_n-route_head<=12&&gps.fix&&geo_seg_m(car_lat(),car_lon(),route_lat[route_n-1],route_lon[route_n-1])<=15){
   atlas_plan_at=atlas_leg_end;if(!atlas_start_leg())atlas_fail(route_status>=R_NOFILE?route_status:R_ERROR);
  }return;
 }
 if(atlas_phase==AP_SOURCE_SWEEP||atlas_phase==AP_TARGET_SWEEP){if(!atlas_sweep_until(deadline))return;}
 if(atlas_phase==AP_TARGET_SPREAD){if(!atlas_target_spread_until(deadline))return;if(atlas_phase==AP_TARGET_SWEEP)continue;}
 if(atlas_phase==AP_DIRECT||atlas_phase==AP_SOURCE||atlas_phase==AP_TARGET){
  if(route_status==R_CALC)regional_service_until(deadline);if(route_status==R_CALC)return;
  if(route_status==R_MEMORY||route_status==R_ERROR||route_status==R_NOFILE){atlas_fail(route_status);return;}
  u32 cost=route_status==R_READY?route_result_cost:ROUTE_INF,m=route_status==R_READY?route_result_m:0;
  if(atlas_phase==AP_DIRECT){atlas_direct_cost=cost;atlas_direct_m=m;}else{struct AtlasAttach*a=atlas_phase==AP_SOURCE?&atlas_sources[atlas_query_index]:&atlas_targets[atlas_query_index];a->cost=cost;a->metres=m;a->known=atlas_probe==2?2:1;}
  atlas_probe=0;route_arena_release();atlas_phase=AP_SEARCH;atlas_search_init=0;route_status=R_CALC;
 }
 if(atlas_phase!=AP_SEARCH)return;
 if(!(atlas_sweeps_done&1u)){if(!atlas_sweep_begin(0))return;continue;}
 if(!(atlas_sweeps_done&2u)){if(!atlas_sweep_begin(1))return;continue;}
 if(!atlas_search_init)atlas_search_begin();unsigned ops=0;
 while(atlas_hn&&atlas_dist[atlas_heap[1]]<atlas_best){
  if((ops++&31u)==0&&!time_before_u32(s_now_ms(),deadline))return;
  u32 u=atlas_heap_pop(),du=atlas_dist[u];u16 ti=atlas_target_index[u];
  if(ti!=65535){struct AtlasAttach*a=&atlas_targets[ti];u32 cost=sat_add32(du,a->known?a->cost:a->bound);if(cost<atlas_best){atlas_best=cost;atlas_best_node=u;atlas_best_target=ti;}}
  const struct AtlasNode*n=atlas_node(u);if(!n){atlas_fail(R_ERROR);return;}
  for(u32 j=0;j<n->count;j++){u32 eid=n->first+j;const struct AtlasEdge*e=atlas_edge(eid);if(!e){atlas_fail(R_ERROR);return;}u32 nd=sat_add32(du,e->cost);if(nd<atlas_dist[e->to]&&atlas_hpos[e->to]!=ROUTE_NOARC){atlas_dist[e->to]=nd;atlas_prev[e->to]=u;atlas_prev_edge[e->to]=eid;atlas_heap_update(e->to);}}
 }
 if(!atlas_finish_search()&&atlas_active)atlas_fail(route_status>=R_NOFILE?route_status:R_ERROR);
 if(!atlas_active||!time_before_u32(s_now_ms(),deadline))return;
 }
}
