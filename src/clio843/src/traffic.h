/* Optional historical traffic adapter for CPR2 v5. All learned profiles
 * originate in the map-independent mobility recorder. Static CH/QAD7 costs
 * are never changed. Bounded local reverse base-graph search shares the
 * route repair invariant: splice only at exactly identical turn states.
 */
#ifndef CLIO_TRAFFIC_H
#define CLIO_TRAFFIC_H
#define TRAFFIC_SEARCH_MAX 768
#define TRAFFIC_PATH_MAX 1024
#define TRAFFIC_LOOKAHEAD_DS 18000u
#define TRAFFIC_MIN_SAVING_DS 1200u
static u32 traffic_route_revision;
static int traffic_begin_idx=-1,traffic_join_idx=-1,traffic_offer_n,traffic_searching;
static int traffic_region=-2;
static u32 traffic_offer_revision,traffic_saving,traffic_scan_ms,traffic_eta_ds,traffic_eta_revision;
static u32 traffic_block_arc,traffic_base_ds;
static int traffic_eta_head,traffic_slow_end;
static u32 traffic_search_started,traffic_dismiss_revision=0xffffffffu;
struct TrafficPoint {s32 lat,lon;u32 node,cost,arc,phys,m;};
static struct TrafficPoint traffic_path[TRAFFIC_PATH_MAX];
struct TrafficState {u32 node,cost,arc;int next,done;};
static struct TrafficState traffic_states[TRAFFIC_SEARCH_MAX];
static unsigned traffic_state_n;
static int traffic_expand=-1;
static u32 traffic_in_first,traffic_in_left;
static int traffic_warn;
static void traffic_invalidate(void){
 traffic_offer_n=traffic_searching=traffic_warn=0;traffic_begin_idx=traffic_join_idx=-1;
 traffic_scan_ms=0;traffic_eta_revision=0xffffffffu;traffic_saving=0;traffic_route_revision++;
}
static int traffic_offer_valid(void){
 return pref.learn_history&&pref.diversions&&gps.fix&&traffic_offer_n>1&&route_status==R_READY&&traffic_offer_revision==traffic_route_revision&&traffic_region==atlas_route_region&&
  traffic_begin_idx>route_head&&traffic_join_idx>traffic_begin_idx&&traffic_join_idx<route_n&&
  route_node[traffic_begin_idx]==traffic_path[0].node&&route_node[traffic_join_idx]==traffic_path[traffic_offer_n-1].node&&
  route_lat[traffic_begin_idx]==traffic_path[0].lat&&route_lon[traffic_begin_idx]==traffic_path[0].lon&&
  route_lat[traffic_join_idx]==traffic_path[traffic_offer_n-1].lat&&route_lon[traffic_join_idx]==traffic_path[traffic_offer_n-1].lon;
}
static u32 traffic_key(u32 id,const struct CprBase*b){
 struct CprPoint p[3];u32 way[2];
 if(b->geom_count<2||!route_read_way(id,&way[0],&way[1])||
  !route_base_point(b,0,&p[0])||!route_base_point(b,b->geom_count/2u,&p[1])||!route_base_point(b,b->geom_count-1u,&p[2]))return 0;
 u32 h=mob_hash(2166136261u,way,sizeof(way));h=mob_hash(h,p,sizeof(p));return h?h:1u;
}
static u32 traffic_key_for_arc(u32 id,u32*full){
 struct CprBase b;if(id==ROUTE_NOARC||!route_read_base(id,&b))return 0;
 if(full)*full=b.cost;
 return traffic_key(id,&b);
}
/* Match a continuous traversal, not instantaneous speeds. An entire
 * directed base arc must be observed from near its entrance to near its
 * exit. Reject ambiguous parallel roads and GPS discontinuities. */
static u32 tl_arc=ROUTE_NOARC,tl_key,tl_epoch,tl_first_ms,tl_last_ms,tl_first_u,tl_last_u,tl_total,tl_stop_ms;
static int tl_region=-2,tl_armed,tl_done;
static void traffic_learning_reset(void){tl_arc=ROUTE_NOARC;tl_armed=tl_done=0;tl_stop_ms=0;}
static int traffic_heading_delta(int a,int b){int d=a-b;if(d<0)d=-d;return d>180?360-d:d;}
static void traffic_learn_sample(void){
 if(!mob_started||!routeok||atlas_probe||route_status==R_CALC){traffic_learning_reset();return;}
 if(atlas_ready&&atlas_owner(mob_rmc_point.lat,mob_rmc_point.lon)!=atlas_route_region){traffic_learning_reset();return;}
 struct RouteSnapCandidate c[8];int n=route_snap_candidates(mob_rmc_point.lat,mob_rmc_point.lon,625u,0,0,c,8);
 int best=-1;u32 bd=0xffffffffu,second=0xffffffffu;
 for(int i=0;i<n;i++){
  struct CprBase b;struct CprPoint a,z;if(!route_read_base(c[i].arc,&b)||c[i].geom_seg+1u>=b.geom_count||
    !route_base_point(&b,c[i].geom_seg,&a)||!route_base_point(&b,c[i].geom_seg+1u,&z))continue;
  int h=nav_heading10(a.lat,a.lon,z.lat,z.lon);
  if(mob_rmc_speed>=32){if(h<0||traffic_heading_delta(h,mob_rmc_heading)>45)continue;}
  else if(c[i].arc!=tl_arc||tl_region!=atlas_route_region)continue;
  if(c[i].d2<bd){second=bd;bd=c[i].d2;best=i;}else if(c[i].d2<second)second=c[i].d2;
 }
 if(best<0){traffic_learning_reset();return;}
 struct RouteSnapCandidate*p=&c[best];
 /* Only retain an existing unambiguous match at a junction. Never learn
  * from a new road if multiple directional candidates are comparably close. */
 if(second!=0xffffffffu&&second<=bd+16u&&p->arc!=tl_arc){traffic_learning_reset();return;}
 if(p->total_u<35u||p->total_u>100000u){traffic_learning_reset();return;}
 u32 margin=p->total_u/8u;if(margin>20u)margin=20u;if(margin<5u)margin=5u;
 if(tl_arc!=p->arc||tl_region!=atlas_route_region||mob_rmc_ms-tl_last_ms>3000u||
   (p->along_u+20u<tl_last_u)){
  traffic_learning_reset();tl_arc=p->arc;tl_region=atlas_route_region;tl_total=p->total_u;
  tl_key=traffic_key_for_arc(p->arc,0);tl_first_u=p->along_u;tl_first_ms=mob_rmc_ms;tl_epoch=mob_rmc_epoch;
  tl_armed=tl_key&&tl_epoch!=MOB_UNKNOWN&&p->along_u<=margin&&mob_rmc_speed>=32;
 }
 if(mob_rmc_speed<10){if(!tl_stop_ms)tl_stop_ms=mob_rmc_ms;else if(mob_rmc_ms-tl_stop_ms>600000u)tl_armed=0;}
 else tl_stop_ms=0;
 tl_last_u=p->along_u;tl_last_ms=mob_rmc_ms;
 if(tl_armed&&!tl_done&&p->along_u+margin>=tl_total&&p->along_u>tl_first_u){
  u32 covered=p->along_u-tl_first_u,elapsed=(mob_rmc_ms-tl_first_ms)/100u;
  if(covered>=tl_total*3u/4u&&elapsed>=10u&&elapsed<=36000u){
   u32 ds=(elapsed*tl_total)/covered;
   mob_stage_road(tl_key,tl_epoch,ds);tl_done=1;
  }
 }
}
static u32 traffic_adjust(u32 arc,u32 epoch,u32 base,int full_arc,int*known){
 if(known)*known=0;
 if(epoch==MOB_UNKNOWN||!base)return base;
 u32 full=0,key=traffic_key_for_arc(arc,&full);
 if(!key||!full||full>36000u)return base;
 u32 adjusted=mob_road_cost(key,epoch,full,known);
 if(full_arc)return adjusted;
 /* Geometry chunks and trimmed prefixes use their proportional static
  * cost; a single profile is never applied in full to each vertex. */
 if(adjusted<=full)return base;
 return base<=full?route_cost_for_units(adjusted,base,full):sat_add32(base,adjusted-full);
}
static int traffic_read_incoming(u32 ref,u32*out){
 if(ref>=rh.route_edges)return 0;
 u8 b[4];u32 off=rh.edge_off+ref*4u;
 if(s_lseek(routefd,(long)off,SEEK_SET_)<0||s_read(routefd,b,4)!=4)return 0;
 *out=rd32u(b);return *out<rh.ch_arcs;
}
/* Return the cost of the remaining current regional leg. The map renderer
 * and guidance continue to use original geometry/turn metadata. */
static u32 traffic_remaining_cost(void){
 if(pref.learn_history&&pref.diversions&&traffic_eta_revision==traffic_route_revision&&route_head<traffic_slow_end&&traffic_warn)return sat_add32(route_result_cost,traffic_eta_ds);
 return route_result_cost;
}
static void traffic_search_cancel(void){traffic_searching=0;traffic_expand=-1;traffic_state_n=0;}
static int traffic_state_find(u32 node){for(unsigned i=0;i<traffic_state_n;i++)if(traffic_states[i].node==node)return (int)i;return -1;}
static int traffic_search_start(int from,int join,u32 block,u32 raw,u32 before,u32 now){
 if(from<=route_head||join<=from||join>=route_n||route_node[from]==ROUTE_NOARC||route_node[join]==ROUTE_NOARC)return 0;
 traffic_begin_idx=from;traffic_join_idx=join;traffic_block_arc=block;traffic_base_ds=raw;
 (void)before;traffic_search_started=now;
 traffic_region=atlas_route_region;traffic_offer_revision=traffic_route_revision;
 traffic_state_n=1;traffic_states[0]=(struct TrafficState){route_node[join],0,ROUTE_NOARC,-1,0};
 traffic_expand=-1;traffic_in_left=0;traffic_searching=1;return 1;
}
static int traffic_search_valid(void){
 return traffic_searching&&route_status==R_READY&&traffic_offer_revision==traffic_route_revision&&traffic_region==atlas_route_region&&
  traffic_begin_idx>route_head&&traffic_join_idx<route_n&&route_node[traffic_join_idx]==traffic_states[0].node;
}
/* Reconstruct legal base arcs into a private geometry buffer. No active
 * route data changes until the driver accepts the candidate. */
static int traffic_build_candidate(int idx,u32 now){
 int n=1,guard=0;u32 elapsed=0,old=0,epoch=mob_time(now);
 if(epoch==MOB_UNKNOWN)return 0;
 /* Recompute arrival time at the divergence point after progress. */
 for(int i=route_head+1;i<=traffic_begin_idx;i++)elapsed=sat_add32(elapsed,route_seg_cost[i]);
 u32 start_elapsed=elapsed;
 traffic_path[0]=(struct TrafficPoint){route_lat[traffic_begin_idx],route_lon[traffic_begin_idx],route_node[traffic_begin_idx],0,ROUTE_NOARC,ROUTE_NOARC,0};
 for(int i=traffic_begin_idx+1;i<=traffic_join_idx;){
  u32 arc=route_seg_arc[i],base=0;while(i<=traffic_join_idx&&route_seg_arc[i]==arc){base=sat_add32(base,route_seg_cost[i]);i++;}
  u32 c=traffic_adjust(arc,epoch+elapsed/10u,base,0,0);old=sat_add32(old,c);elapsed=sat_add32(elapsed,c);
 }
 elapsed=start_elapsed;
 while(traffic_states[idx].next>=0){
  if(++guard>TRAFFIC_SEARCH_MAX)return 0;
  struct TrafficState*t=&traffic_states[idx];struct CprArc a;struct CprBase b;
  if(!route_read_arc(t->arc,&a)||a.from!=t->node||a.to!=traffic_states[t->next].node||
   (a.flags&CPR_ARC_SHORTCUT)||a.left==traffic_block_arc||!route_read_base(a.left,&b)||b.geom_count<2)return 0;
  if(n+(int)b.geom_count-1>TRAFFIC_PATH_MAX)return 0;
  struct CprPoint p,q;if(!route_base_point(&b,0,&p)||p.lat!=traffic_path[n-1].lat||p.lon!=traffic_path[n-1].lon)return 0;
  u32 total=0;for(unsigned k=1;k<b.geom_count;k++){if(!route_base_point(&b,k,&q))return 0;total=sat_add32(total,route_geom_seg_units(&p,&q));p=q;}
  if(!total||!route_base_point(&b,0,&p))return 0;
  u32 cum=0,prev=0;for(unsigned k=1;k<b.geom_count;k++){
   if(!route_base_point(&b,k,&q))return 0;
   cum=sat_add32(cum,route_geom_seg_units(&p,&q));
   u32 cc=k+1u==b.geom_count?a.cost:route_cost_for_units(a.cost,cum,total);
   if(cc<prev)return 0;
   traffic_path[n++]=(struct TrafficPoint){q.lat,q.lon,k+1u==b.geom_count?a.to:ROUTE_NOARC,cc-prev,a.left,b.geom_first,geo_seg_m(p.lat,p.lon,q.lat,q.lon)};
   prev=cc;p=q;
  }
  elapsed=sat_add32(elapsed,traffic_adjust(a.left,epoch+elapsed/10u,a.cost,0,0));idx=t->next;
 }
 if(n<2||traffic_path[n-1].node!=route_node[traffic_join_idx]||traffic_path[n-1].lat!=route_lat[traffic_join_idx]||traffic_path[n-1].lon!=route_lon[traffic_join_idx])return 0;
 /* Exact state joins prevent turn-restriction violations. Reject any
  * candidate that crosses or returns to another state on the retained
  * prefix/suffix, rather than inventing a potentially illegal shortcut. */
 for(int k=1;k<n-1;k++)if(traffic_path[k].node!=ROUTE_NOARC){
  for(int i=route_head;i<=traffic_begin_idx;i++)if(traffic_path[k].lat==route_lat[i]&&traffic_path[k].lon==route_lon[i])return 0;
  int limit=traffic_join_idx+1024;if(limit>=route_n)limit=route_n-1;
  for(int i=traffic_join_idx;i<=limit;i++)if(traffic_path[k].lat==route_lat[i]&&traffic_path[k].lon==route_lon[i])return 0;
 }
 u32 alt=elapsed-start_elapsed,saving=old>alt?old-alt:0;
 u32 remaining=traffic_remaining_cost();
 if(saving<TRAFFIC_MIN_SAVING_DS||saving<remaining/10u)return 0;
 if(route_n-(traffic_join_idx-traffic_begin_idx)+(n-1)>ROUTE_PATH_MAX)return 0;
 traffic_offer_n=n;traffic_saving=saving;return 1;
}
static void traffic_search_step(u32 now){
 if(!traffic_search_valid()||now-traffic_search_started>5000u){traffic_search_cancel();return;}
 /* Bounded incremental work. The original route remains visible. */
 for(int op=0;op<12;op++){
  if(traffic_expand<0){
   int best=-1;for(unsigned i=0;i<traffic_state_n;i++)if(!traffic_states[i].done&&(best<0||traffic_states[i].cost<traffic_states[best].cost))best=(int)i;
   if(best<0||traffic_states[best].cost>traffic_base_ds*3u+6000u){traffic_search_cancel();return;}
   struct TrafficState*t=&traffic_states[best];
   if(t->node==route_node[traffic_begin_idx]){(void)traffic_build_candidate(best,now);traffic_search_cancel();return;}
   struct CprNode node;if(!route_read_node(t->node,&node)){traffic_search_cancel();return;}
   t->done=1;traffic_expand=best;traffic_in_first=node.first_edge;traffic_in_left=node.edge_count;
  }
  if(!traffic_in_left){traffic_expand=-1;continue;}
  u32 id;struct CprArc a;struct CprNode from;
  traffic_in_left--;
  if(!traffic_read_incoming(traffic_in_first++,&id)||!route_read_arc(id,&a)){traffic_search_cancel();return;}
  if(a.to!=traffic_states[traffic_expand].node||(a.flags&CPR_ARC_SHORTCUT)||a.left==traffic_block_arc||a.left>=rh.base_edges||!a.cost)continue;
  if(!route_read_node(a.from,&from)||from.comp!=route_dest_comp)continue;
  if(mob_dist((struct MobPoint){from.lat,from.lon},(struct MobPoint){route_lat[traffic_begin_idx],route_lon[traffic_begin_idx]})>6000u&&
     mob_dist((struct MobPoint){from.lat,from.lon},(struct MobPoint){route_lat[traffic_join_idx],route_lon[traffic_join_idx]})>6000u)continue;
  /* Search a static legal candidate; historical costs are applied only after
   * reconstruction, in forward time order. Never modify CH shortcuts. */
  u32 dist=sat_add32(traffic_states[traffic_expand].cost,a.cost);int at=traffic_state_find(a.from);
  if(at<0){if(traffic_state_n>=TRAFFIC_SEARCH_MAX){traffic_search_cancel();return;}at=(int)traffic_state_n++;traffic_states[at]=(struct TrafficState){a.from,dist,id,traffic_expand,0};}
  else if(!traffic_states[at].done&&dist<traffic_states[at].cost){traffic_states[at].cost=dist;traffic_states[at].arc=id;traffic_states[at].next=traffic_expand;}
 }
}
static void traffic_scan(u32 now){
 u32 epoch=mob_time(now);if(epoch==MOB_UNKNOWN){traffic_warn=traffic_offer_n=0;return;}
 u32 sum=0,stat=0,delay=0,bad_arc=ROUTE_NOARC;int bad=-1,begin=-1,join=-1,anchor=-1,last_slow=-1;
 /* Work on at most the next 30 minutes / 1024 geometry points. Group
  * identical arcs to avoid repeatedly reading the same geometry key. */
 int end=route_head+1024;if(end>=route_n)end=route_n-1;
 for(int i=route_head+1;i<=end&&stat<TRAFFIC_LOOKAHEAD_DS;){
  int first=i;u32 arc=route_seg_arc[i],base=0;
  while(i<=end&&route_seg_arc[i]==arc){base=sat_add32(base,route_seg_cost[i]);i++;}
  int known=0;u32 adjusted=traffic_adjust(arc,epoch+sum/10u,base,0,&known);
  if(known&&adjusted>base)last_slow=i-1;
  if(known&&adjusted>base&&adjusted-base>delay&&anchor>route_head){delay=adjusted-base;bad=first;bad_arc=arc;begin=anchor;}
  stat=sat_add32(stat,base);sum=sat_add32(sum,adjusted);
  if(route_node[i-1]!=ROUTE_NOARC)anchor=i-1;
 }
 traffic_eta_ds=sum>stat?sum-stat:0;traffic_slow_end=last_slow;traffic_eta_head=route_head;
 traffic_eta_revision=traffic_route_revision;traffic_warn=traffic_eta_ds>=600u;
 if(bad<0||delay<600u||traffic_dismiss_revision==traffic_route_revision||traffic_offer_valid())return;
 /* Choose a turn-state rejoin beyond the slow arc and at least 200 m
  * farther along the original route. Reuse the unaffected prefix/suffix. */
 u32 m=0,raw=0,prior=0;
 for(int i=route_head+1;i<route_n&&i<=end;i++){
  if(i<=begin)prior=sat_add32(prior,route_seg_cost[i]);
  else raw=sat_add32(raw,route_seg_cost[i]);
  if(i>bad&&route_seg_arc[i]!=bad_arc)m=sat_add32(m,route_seg_m[i]);
  if(i>bad&&m>=200u&&route_node[i]!=ROUTE_NOARC){join=i;break;}
 }
 if(join>begin&&raw&&raw<=TRAFFIC_LOOKAHEAD_DS)(void)traffic_search_start(begin,join,bad_arc,raw,prior,now);
}
static void traffic_service(u32 now){
 if(!pref.diversions||!pref.learn_history||mob.n<5||!gps.fix||mob_time(now)==MOB_UNKNOWN||!routeok||route_status!=R_READY||atlas_probe||
  (atlas_ready&&atlas_owner(car_lat(),car_lon())!=atlas_route_region)){traffic_offer_n=traffic_warn=0;traffic_search_cancel();return;}
 if(traffic_offer_n&&!traffic_offer_valid()){traffic_offer_n=0;traffic_saving=0;}
 if(traffic_searching){traffic_search_step(now);return;}
 if(route_head>=traffic_slow_end)traffic_warn=0;
 if(traffic_scan_ms&&now-traffic_scan_ms<15000u&&route_head-traffic_eta_head<64)return;
 traffic_scan_ms=now;traffic_scan(now);
}
static void traffic_copy_point(int dst,int src){
 route_lat[dst]=route_lat[src];route_lon[dst]=route_lon[src];route_node[dst]=route_node[src];route_seg_cost[dst]=route_seg_cost[src];route_seg_arc[dst]=route_seg_arc[src];route_seg_phys[dst]=route_seg_phys[src];route_seg_m[dst]=route_seg_m[src];
}
static int traffic_accept(void){
 if(!traffic_offer_valid())return 0;
 int a=traffic_begin_idx,b=traffic_join_idx,n=traffic_offer_n,delta=n-1-(b-a);
 if(route_n+delta>ROUTE_PATH_MAX)return 0;
 u32 oldcost=0,oldm=0,newcost=0,newm=0;
 for(int i=a+1;i<=b;i++){oldcost=sat_add32(oldcost,route_seg_cost[i]);oldm=sat_add32(oldm,route_seg_m[i]);}
 if(delta>0){for(int i=route_n-1;i>b;i--)traffic_copy_point(i+delta,i);}
 else if(delta<0){for(int i=b+1;i<route_n;i++)traffic_copy_point(i+delta,i);}
 for(int i=1;i<n;i++){
  struct TrafficPoint*p=&traffic_path[i];int k=a+i;
  route_lat[k]=p->lat;route_lon[k]=p->lon;route_node[k]=p->node;route_seg_cost[k]=p->cost;route_seg_arc[k]=p->arc;route_seg_phys[k]=p->phys;route_seg_m[k]=p->m;
  newcost=sat_add32(newcost,p->cost);newm=sat_add32(newm,p->m);
 }
 route_n+=delta;route_result_cost=sat_add32(route_result_cost>oldcost?route_result_cost-oldcost:0,newcost);
 route_result_m=sat_add32(route_result_m>oldm?route_result_m-oldm:0,newm);
 route_offroute_checks=route_arc_check_tick=0;nav_maneuver_find();traffic_invalidate();traffic_dismiss_revision=traffic_route_revision;
 return 1;
}
#endif
