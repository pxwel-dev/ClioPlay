/* Multi-leg future route planner (ClioPlay v3.0).
 * Leg 0 (Car -> trip.target[active_idx]) keeps full-fidelity route_lat/lon[]
 * for turn-by-turn guidance, adherence, local repair and diversions.
 * Legs k = active_idx+1 .. trip.count-1 run cooperatively after Leg 0 reaches
 * R_READY, reusing the idle CH search arena (zero extra CH RAM) and storing
 * decimated road polylines in a shared 64 KB static pool. */
#ifndef CLIO_TRIP_PLAN_H
#define CLIO_TRIP_PLAN_H

#define TRIP_FUTURE_MAX_PTS 8192
enum { TLEG_NONE=0, TLEG_READY=1, TLEG_CALC=2, TLEG_UNREACHABLE=3, TLEG_NOROAD=4 };

struct TripLeg {
 u8 status;
 s32 from_lat, from_lon, to_lat, to_lon;
 u32 cost_ds, dist_m;
 u16 pt_off, pt_n;
};

static struct TripLeg trip_legs[TRIP_MAX_TARGETS];
static s32 trip_future_lat[TRIP_FUTURE_MAX_PTS];
static s32 trip_future_lon[TRIP_FUTURE_MAX_PTS];
static u16 trip_future_used = 0;

struct TripPlanSaved {
 struct RouteSnapCandidate start_snap, start_twin, dest_cand[ROUTE_DEST_CAND_MAX], dest_twin;
 int dest_count, dest_idx, start_twin_ok, start_used_twin, dest_twin_ok, dest_used_twin;
 u32 dest_comp, start_pen0, start_pen1, best, meet, expanded;
};
static struct TripPlanSaved tp_saved, tp_work;
static int tp_active = 0;
static int tp_leg = -1;
static int tp_saved_valid = 0;
static struct RouteSnapCandidate tp_start_snap, tp_start_twin, tp_dest_cand[ROUTE_DEST_CAND_MAX];
static int tp_start_twin_ok = 0, tp_dest_count = 0, tp_dest_idx = 0;

static void tp_capture(struct TripPlanSaved*dst){
 dst->start_snap=route_start_snap;dst->start_twin=route_start_twin;
 for(int i=0;i<ROUTE_DEST_CAND_MAX;i++)dst->dest_cand[i]=route_dest_cand[i];
 dst->dest_twin=route_dest_twin;
 dst->dest_count=route_dest_count;dst->dest_idx=route_dest_idx;
 dst->start_twin_ok=route_start_twin_ok;dst->start_used_twin=route_start_used_twin;
 dst->dest_twin_ok=route_dest_twin_ok;dst->dest_used_twin=route_dest_used_twin;
 dst->dest_comp=route_dest_comp;
 dst->start_pen0=route_start_pen0;dst->start_pen1=route_start_pen1;
 dst->best=route_best;dst->meet=route_meet;dst->expanded=route_expanded;
}
static void tp_apply(const struct TripPlanSaved*src){
 route_start_snap=src->start_snap;route_start_twin=src->start_twin;
 for(int i=0;i<ROUTE_DEST_CAND_MAX;i++)route_dest_cand[i]=src->dest_cand[i];
 route_dest_twin=src->dest_twin;
 route_dest_count=src->dest_count;route_dest_idx=src->dest_idx;
 route_start_twin_ok=src->start_twin_ok;route_start_used_twin=src->start_used_twin;
 route_dest_twin_ok=src->dest_twin_ok;route_dest_used_twin=src->dest_used_twin;
 route_dest_comp=src->dest_comp;
 route_start_pen0=src->start_pen0;route_start_pen1=src->start_pen1;
 route_best=src->best;route_meet=src->meet;route_expanded=src->expanded;
}
static void tp_save_leg0(void){if(!tp_saved_valid){tp_capture(&tp_saved);tp_saved_valid=1;}}
static void tp_restore_leg0(void){if(tp_saved_valid){tp_apply(&tp_saved);tp_saved_valid=0;}}
static void tp_yield_slice(void){tp_capture(&tp_work);tp_restore_leg0();}
static void tp_resume_slice(void){tp_save_leg0();tp_apply(&tp_work);}

static void tp_ch_clear_search(void){
 if(chbucket&&chbucket_n)memzero(chbucket,chbucket_n*4u);
 chst_n=chhf_n=chhb_n=0;chheap_peak=0;
 route_expanded=0;route_best=route_meet=ROUTE_INF;
 route_start_pen0=route_start_pen1=0;
 route_start_used_twin=route_dest_used_twin=route_dest_twin_ok=0;
}
static int tp_ch_begin(const struct RouteSnapCandidate*start,const struct RouteSnapCandidate*dest){
 tp_ch_clear_search();
 if(!ch_seed_start(start,route_dest_comp,1)||!ch_seed_destination(dest,route_dest_comp))return 0;
 ch_update_meet(ch_state(start->node,0));
 if(route_start_twin_ok)ch_update_meet(ch_state(route_start_twin.node,0));
 return 1;
}

static void trip_plan_abort_active(void){
 if(!tp_active)return;
 tp_ch_clear_search();
 route_arena_release();
 tp_restore_leg0();
 if(tp_leg>=0&&tp_leg<TRIP_MAX_TARGETS&&trip_legs[tp_leg].status==TLEG_CALC){
  trip_legs[tp_leg].status=TLEG_NONE;
 }
 tp_active=0;
 tp_leg=-1;
}
static void trip_plan_reset(void){
 trip_plan_abort_active();
 memzero(trip_legs,sizeof(trip_legs));
 trip_future_used=0;
}

/* Compact the shared future-leg geometry pool in-place so repeated edits,
 * reorders, and stop completions never fragment the 64 KB buffer. */
static void trip_plan_compact(void){
 s32 tmp_lat[TRIP_FUTURE_MAX_PTS];
 s32 tmp_lon[TRIP_FUTURE_MAX_PTS];
 u16 out=0;
 for(int k=0;k<TRIP_MAX_TARGETS;k++){
  if(trip_legs[k].status!=TLEG_READY||trip_legs[k].pt_n<2){
   trip_legs[k].pt_off=0;trip_legs[k].pt_n=0;continue;
  }
  u16 off=trip_legs[k].pt_off,n=trip_legs[k].pt_n;
  if((u32)off+(u32)n>TRIP_FUTURE_MAX_PTS||(u32)out+(u32)n>TRIP_FUTURE_MAX_PTS){
   trip_legs[k].pt_off=0;trip_legs[k].pt_n=0;continue;
  }
  for(u16 i=0;i<n;i++){tmp_lat[out+i]=trip_future_lat[off+i];tmp_lon[out+i]=trip_future_lon[off+i];}
  trip_legs[k].pt_off=out;out=(u16)(out+n);
 }
 for(u16 i=0;i<out;i++){trip_future_lat[i]=tmp_lat[i];trip_future_lon[i]=tmp_lon[i];}
 trip_future_used=out;
}

static void trip_plan_sync(void){
 if(!trip.active||trip.count<=trip.active_idx+1){
  if(tp_active||trip_future_used)trip_plan_reset();
  else memzero(trip_legs,sizeof(trip_legs));
  return;
 }
 struct TripLeg old[TRIP_MAX_TARGETS];
 for(int i=0;i<TRIP_MAX_TARGETS;i++)old[i]=trip_legs[i];
 u8 used_old[TRIP_MAX_TARGETS];memzero(used_old,sizeof(used_old));
 int changed=0;
 /* Clear legs <= active_idx and >= trip.count. */
 for(int k=0;k<TRIP_MAX_TARGETS;k++){
  if(k<=trip.active_idx||k>=trip.count){
   if(trip_legs[k].status!=TLEG_NONE)changed=1;
   if(tp_active&&tp_leg==k)trip_plan_abort_active();
   memzero(&trip_legs[k],sizeof(trip_legs[k]));
  }
 }
 for(int k=trip.active_idx+1;k<trip.count;k++){
  s32 fl=trip.target[k-1].lat,fo=trip.target[k-1].lon;
  s32 tl=trip.target[k].lat,to=trip.target[k].lon;
  if(old[k].status!=TLEG_NONE&&!used_old[k]&&
     old[k].from_lat==fl&&old[k].from_lon==fo&&old[k].to_lat==tl&&old[k].to_lon==to){
   trip_legs[k]=old[k];used_old[k]=1;continue;
  }
  int reused=-1;
  for(int j=0;j<TRIP_MAX_TARGETS;j++){
   if(used_old[j]||old[j].status==TLEG_NONE||old[j].status==TLEG_CALC)continue;
   if(old[j].from_lat==fl&&old[j].from_lon==fo&&old[j].to_lat==tl&&old[j].to_lon==to){reused=j;break;}
  }
  if(tp_active&&tp_leg==k)trip_plan_abort_active();
  changed=1;
  if(reused>=0){
   trip_legs[k]=old[reused];used_old[reused]=1;
  }else{
   memzero(&trip_legs[k],sizeof(trip_legs[k]));
   trip_legs[k].from_lat=fl;trip_legs[k].from_lon=fo;
   trip_legs[k].to_lat=tl;trip_legs[k].to_lon=to;
  }
 }
 if(changed)trip_plan_compact();
}

static int trip_plan_pending(void){
 if(!trip.active||route_status!=R_READY||route_repair_active||!routeok)return 0;
 if(trip.count<=trip.active_idx+1)return 0;
 for(int k=trip.active_idx+1;k<trip.count;k++){
  if(trip_legs[k].status==TLEG_NONE||trip_legs[k].status==TLEG_CALC)return 1;
 }
 return 0;
}
static int trip_plan_all_ready(void){
 if(!trip.active||trip.count<=trip.active_idx+1)return 1;
 for(int k=trip.active_idx+1;k<trip.count;k++)if(trip_legs[k].status!=TLEG_READY)return 0;
 return 1;
}
static u32 trip_plan_future_cost_ds(void){
 u32 sum=0;
 if(!trip.active)return 0;
 for(int k=trip.active_idx+1;k<trip.count;k++){
  if(trip_legs[k].status!=TLEG_READY)return ROUTE_INF;
  sum=sat_add32(sum,trip_legs[k].cost_ds);
 }
 return sum;
}
static u32 trip_plan_future_dist_m(void){
 u32 sum=0;
 if(!trip.active)return 0;
 for(int k=trip.active_idx+1;k<trip.count;k++){
  if(trip_legs[k].status!=TLEG_READY)return 0;
  sum=sat_add32(sum,trip_legs[k].dist_m);
 }
 return sum;
}
static u32 trip_plan_stop_cum_cost_ds(int stop_idx){
 if(!trip.active||route_status!=R_READY||stop_idx<trip.active_idx||stop_idx>=trip.count)return ROUTE_INF;
 u32 sum=sat_add32(traffic_remaining_cost(),atlas_future_cost);
 for(int k=trip.active_idx+1;k<=stop_idx;k++){
  if(trip_legs[k].status!=TLEG_READY)return ROUTE_INF;
  sum=sat_add32(sum,trip_legs[k].cost_ds);
 }
 return sum;
}

/* Append a point to the active future-leg slice with adaptive decimation so
 * 1..9 future legs always fit inside the 64 KB pool while preserving curves. */
static void tp_emit_pt(u16 base_off,u16*cnt,u16 cap,u32*step_m,s32*last_lat,s32*last_lon,s32 lat,s32 lon,int force){
 if(*cnt==0){
  if(base_off<TRIP_FUTURE_MAX_PTS){trip_future_lat[base_off]=lat;trip_future_lon[base_off]=lon;*cnt=1;*last_lat=lat;*last_lon=lon;}
  return;
 }
 if(!force&&geo_seg_m(*last_lat,*last_lon,lat,lon)<*step_m)return;
 if(*cnt>=cap&&cap>=8){
  /* Downsample existing slice in-place by 2x keeping start and latest point. */
  u16 out=1;
  for(u16 i=2;i<*cnt;i+=2){
   trip_future_lat[base_off+out]=trip_future_lat[base_off+i];
   trip_future_lon[base_off+out]=trip_future_lon[base_off+i];
   out++;
  }
  *cnt=out;
  *step_m=(*step_m<2000u)?(*step_m*2u):2000u;
 }
 if(*cnt<cap&&(u32)base_off+(u32)(*cnt)<TRIP_FUTURE_MAX_PTS){
  trip_future_lat[base_off+*cnt]=lat;trip_future_lon[base_off+*cnt]=lon;
  (*cnt)++;*last_lat=lat;*last_lon=lon;
 }
}

/* Solve a cross-region Atlas future leg using only the already-loaded in-RAM
 * portal overlay, so Leg 0's active regional route file is never disturbed. */
static int tp_solve_atlas_cross(int leg_idx,int r0,int r1){
 if(!atlas_ready||!atlas_validate_graph()||r0<0||r1<0)return 0;
 struct TripLeg*lg=&trip_legs[leg_idx];
 u32 best_s=ROUTE_NOARC,best_sd=0xffffffffu,best_t=ROUTE_NOARC,best_td=0xffffffffu;
 struct AtlasRegion*rs=&atlas_regions[r0],*rt=&atlas_regions[r1];
 for(u32 i=0;i<rs->count;i++){
  const struct AtlasMember*m=atlas_member(rs->first+i);if(!m)continue;
  const struct AtlasNode*n=atlas_node(m->global);if(!n)continue;
  u32 d=geo_seg_m(lg->from_lat,lg->from_lon,n->lat,n->lon);
  if(d<best_sd){best_sd=d;best_s=m->global;}
 }
 for(u32 i=0;i<rt->count;i++){
  const struct AtlasMember*m=atlas_member(rt->first+i);if(!m)continue;
  const struct AtlasNode*n=atlas_node(m->global);if(!n)continue;
  u32 d=geo_seg_m(lg->to_lat,lg->to_lon,n->lat,n->lon);
  if(d<best_td){best_td=d;best_t=m->global;}
 }
 if(best_s==ROUTE_NOARC||best_t==ROUTE_NOARC)return 0;
 /* Run tiny Dijkstra over atlas_nn portal nodes (reusing idle atlas arrays). */
 for(u32 u=0;u<atlas_nn;u++){atlas_dist[u]=ROUTE_INF;atlas_prev[u]=ROUTE_NOARC;atlas_prev_edge[u]=ROUTE_NOARC;atlas_hpos[u]=0;}
 atlas_hn=0;atlas_dist[best_s]=0;atlas_heap_update(best_s);
 while(atlas_hn){
  u32 u=atlas_heap_pop();
  if(u==best_t)break;
  u32 du=atlas_dist[u];
  const struct AtlasNode*n=atlas_node(u);if(!n)break;
  for(u32 j=0;j<n->count;j++){
   u32 eid=n->first+j;const struct AtlasEdge*e=atlas_edge(eid);if(!e)continue;
   u32 nd=sat_add32(du,e->cost);
   if(nd<atlas_dist[e->to]&&atlas_hpos[e->to]!=ROUTE_NOARC){
    atlas_dist[e->to]=nd;atlas_prev[e->to]=u;atlas_prev_edge[e->to]=eid;atlas_heap_update(e->to);
   }
  }
 }
 if(atlas_dist[best_t]==ROUTE_INF)return 0;
 /* Reconstruct portal chain + straight-line access estimates (~50 km/h). */
 u32 access_m=sat_add32(best_sd,best_td);
 u32 access_ds=udiv32(access_m*72u+50u,100u); /* ~50 km/h => 0.72 ds/m */
 u32 total_m=access_m,total_ds=sat_add32(atlas_dist[best_t],access_ds);
 u32 chain[256];u32 cn=0,cur=best_t;
 while(cur!=ROUTE_NOARC&&cn<256u){
  chain[cn++]=cur;
  if(cur==best_s)break;
  u32 eid=atlas_prev_edge[cur];
  if(eid!=ROUTE_NOARC){const struct AtlasEdge*e=atlas_edge(eid);if(e)total_m=sat_add32(total_m,e->metres);}
  cur=atlas_prev[cur];
 }
 u16 base=trip_future_used,cnt=0;
 u16 cap=(u16)(TRIP_FUTURE_MAX_PTS>base?TRIP_FUTURE_MAX_PTS-base:0);
 if(cap>300)cap=300;
 u32 step_m=25u;s32 ll=0,lo=0;
 tp_emit_pt(base,&cnt,cap,&step_m,&ll,&lo,lg->from_lat,lg->from_lon,1);
 for(int i=(int)cn-1;i>=0;i--){
  const struct AtlasNode*n=atlas_node(chain[i]);
  if(n)tp_emit_pt(base,&cnt,cap,&step_m,&ll,&lo,n->lat,n->lon,1);
 }
 tp_emit_pt(base,&cnt,cap,&step_m,&ll,&lo,lg->to_lat,lg->to_lon,1);
 lg->pt_off=base;lg->pt_n=cnt;trip_future_used=(u16)(trip_future_used+cnt);
 lg->cost_ds=total_ds;lg->dist_m=total_m;lg->status=TLEG_READY;
 return 1;
}

static int tp_finish_ch(int leg_idx){
 if(route_best==ROUTE_INF||route_meet==ROUTE_INF)return 0;
 /* Trace forward + backward CH arc sequences into idle ch_arc_tmp/ch_arc_path. */
 int nf=0,nb=0;
 u32 u=route_meet;
 int start_used_twin=0,dest_used_twin=0;
 for(;;){
  struct ChState*s=ch_state(u,0);if(!s)return 0;
  if(s->pf==ROUTE_NOARC&&(u==tp_start_snap.node||(tp_start_twin_ok&&u==tp_start_twin.node))){
   start_used_twin=(s->seed_flags&1u)!=0;break;
  }
  if(nf>=ROUTE_PATH_MAX||s->pf==ROUTE_NOARC)return 0;
  u32 fr=0;
  if(qadjmode==7){struct QRec7 q;if(!qadj7_read_qrec(s->pf,&q))return 0;fr=q.from;}
  else{struct CprArc a;if(!route_read_arc(s->pf,&a))return 0;fr=a.from;}
  ch_arc_tmp[nf++]=s->pf;u=fr;
 }
 for(int i=nf-1;i>=0;i--)ch_arc_path[nb++]=ch_arc_tmp[i];
 u=route_meet;
 for(;;){
  struct ChState*s=ch_state(u,0);if(!s||s->db==ROUTE_INF)return 0;
  if(s->pb==ROUTE_NOARC){dest_used_twin=(s->seed_flags&2u)!=0;break;}
  if(nb>=ROUTE_PATH_MAX)return 0;
  u32 to=0;
  if(qadjmode==7){struct QRec7 q;if(!qadj7_read_qrec(s->pb,&q))return 0;to=qrec7_to(&q);}
  else{struct CprArc a;if(!route_read_arc(s->pb,&a))return 0;to=a.to;}
  ch_arc_path[nb++]=s->pb;u=to;
 }
 const struct RouteSnapCandidate*rs=start_used_twin?&tp_start_twin:&tp_start_snap;
 const struct RouteSnapCandidate*rd=dest_used_twin?&route_dest_twin:&tp_dest_cand[tp_dest_idx];
 u32 spen=start_used_twin?route_start_pen1:route_start_pen0;
 u32 total_cost=route_best>spen?route_best-spen:route_best;

 int rem_legs=(int)trip.count-leg_idx;if(rem_legs<1)rem_legs=1;
 u16 base=trip_future_used,cnt=0;
 u16 avail=(u16)(TRIP_FUTURE_MAX_PTS>base?TRIP_FUTURE_MAX_PTS-base:0);
 u16 cap=(u16)(avail/(u16)rem_legs);if(cap<64&&avail>=64)cap=64;if(cap>avail)cap=avail;
 u32 step_m=25u,total_m=0;
 s32 last_lat=0,last_lon=0,prev_lat=rs->snap_lat,prev_lon=rs->snap_lon;
 tp_emit_pt(base,&cnt,cap,&step_m,&last_lat,&last_lon,rs->snap_lat,rs->snap_lon,1);

 /* Emit partial start base edge. */
 if(rs->arc!=ROUTE_NOARC){
  struct CprBase sb;
  if(route_read_base(rs->arc,&sb)&&sb.geom_count>=2){
   for(u32 i=rs->geom_seg+1u;i<sb.geom_count;i++){
    struct CprPoint pt;if(!route_base_point(&sb,i,&pt))break;
    total_m=sat_add32(total_m,geo_seg_m(prev_lat,prev_lon,pt.lat,pt.lon));
    prev_lat=pt.lat;prev_lon=pt.lon;
    tp_emit_pt(base,&cnt,cap,&step_m,&last_lat,&last_lon,pt.lat,pt.lon,0);
   }
  }
 }
 /* Unpack all CH arcs via idle ch_unpack_stack[]. */
 for(int ai=0;ai<nb;ai++){
  int sp=0;ch_unpack_stack[sp++]=ch_arc_path[ai];
  while(sp>0){
   u32 x=ch_unpack_stack[--sp];
   u32 base_id=ROUTE_NOARC;
   if(qadjmode==7){
    struct QRec7 q;if(!qadj7_read_qrec(x,&q))return 0;
    if(qrec7_flags(&q)&CPR_ARC_SHORTCUT){
     if(sp+2>ROUTE_PATH_MAX)return 0;
     ch_unpack_stack[sp++]=q.right_geom;ch_unpack_stack[sp++]=q.left;continue;
    }
    base_id=q.left;
   }else{
    struct CprArc a;if(!route_read_arc(x,&a))return 0;
    if(a.flags&CPR_ARC_SHORTCUT){
     if(sp+2>ROUTE_PATH_MAX)return 0;
     ch_unpack_stack[sp++]=a.right;ch_unpack_stack[sp++]=a.left;continue;
    }
    base_id=a.left;
   }
   struct CprBase bb;
   if(base_id<rh.base_edges&&route_read_base(base_id,&bb)&&bb.geom_count>=2){
    for(u32 gi=1;gi<bb.geom_count;gi++){
     struct CprPoint pt;if(!route_base_point(&bb,gi,&pt))break;
     total_m=sat_add32(total_m,geo_seg_m(prev_lat,prev_lon,pt.lat,pt.lon));
     prev_lat=pt.lat;prev_lon=pt.lon;
     tp_emit_pt(base,&cnt,cap,&step_m,&last_lat,&last_lon,pt.lat,pt.lon,0);
    }
   }
  }
 }
 /* Emit partial destination base edge. */
 if(rd->arc!=ROUTE_NOARC){
  struct CprBase db;
  if(route_read_base(rd->arc,&db)&&db.geom_count>=2){
   for(u32 i=1;i<=rd->geom_seg&&i<db.geom_count;i++){
    struct CprPoint pt;if(!route_base_point(&db,i,&pt))break;
    total_m=sat_add32(total_m,geo_seg_m(prev_lat,prev_lon,pt.lat,pt.lon));
    prev_lat=pt.lat;prev_lon=pt.lon;
    tp_emit_pt(base,&cnt,cap,&step_m,&last_lat,&last_lon,pt.lat,pt.lon,0);
   }
  }
 }
 total_m=sat_add32(total_m,geo_seg_m(prev_lat,prev_lon,rd->snap_lat,rd->snap_lon));
 tp_emit_pt(base,&cnt,cap,&step_m,&last_lat,&last_lon,rd->snap_lat,rd->snap_lon,1);

 struct TripLeg*lg=&trip_legs[leg_idx];
 lg->pt_off=base;lg->pt_n=cnt;trip_future_used=(u16)(trip_future_used+cnt);
 lg->cost_ds=total_cost;lg->dist_m=total_m;lg->status=TLEG_READY;
 return 1;
}

static int tp_begin_cand(int idx){
 if(idx<0||idx>=tp_dest_count)return 0;
 tp_dest_idx=idx;
 struct CprNode sn;if(!route_read_node(tp_start_snap.node,&sn))return 0;
 if(!route_dest_has_component(&tp_dest_cand[idx],sn.comp))return 0;
 route_dest_comp=sn.comp;
 route_start_snap=tp_start_snap;route_start_twin=tp_start_twin;
 route_start_twin_ok=tp_start_twin_ok;
 route_dest_count=tp_dest_count;route_dest_idx=idx;
 for(int i=0;i<tp_dest_count;i++)route_dest_cand[i]=tp_dest_cand[i];
 return tp_ch_begin(&tp_start_snap,&tp_dest_cand[idx]);
}

static int tp_start_leg(int leg_idx){
 struct TripLeg*lg=&trip_legs[leg_idx];
 if(atlas_ready){
  int r0=atlas_owner(lg->from_lat,lg->from_lon),r1=atlas_owner(lg->to_lat,lg->to_lon);
  if(r0<0||r1<0){lg->status=TLEG_NOROAD;return 0;}
  if(r0!=atlas_route_region||r1!=atlas_route_region){
   if(!tp_solve_atlas_cross(leg_idx,r0,r1))lg->status=TLEG_UNREACHABLE;
   render_background_dirty=1;return 0;
  }
 }
 tp_save_leg0();
 tp_start_twin_ok=0;tp_dest_count=0;tp_dest_idx=0;
 if(!route_snap_node_ex(lg->from_lat,lg->from_lon,&tp_start_snap,&tp_start_twin,&tp_start_twin_ok)){
  tp_restore_leg0();lg->status=TLEG_NOROAD;return 0;
 }
 tp_dest_count=route_snap_candidates(lg->to_lat,lg->to_lon,ROUTE_DEST_SNAP_D2,1,1,tp_dest_cand,ROUTE_DEST_CAND_MAX);
 if(tp_dest_count<1){tp_restore_leg0();lg->status=TLEG_NOROAD;return 0;}
 if(route_same_endpoint(&tp_start_snap,&tp_dest_cand[0])){
  u16 base=trip_future_used,cnt=0;u32 sm=25u;s32 ll=0,lo=0;
  tp_emit_pt(base,&cnt,4,&sm,&ll,&lo,tp_start_snap.snap_lat,tp_start_snap.snap_lon,1);
  tp_emit_pt(base,&cnt,4,&sm,&ll,&lo,tp_dest_cand[0].snap_lat,tp_dest_cand[0].snap_lon,1);
  lg->pt_off=base;lg->pt_n=cnt;trip_future_used=(u16)(trip_future_used+cnt);
  lg->cost_ds=0;lg->dist_m=0;lg->status=TLEG_READY;
  tp_restore_leg0();render_background_dirty=1;return 0;
 }
 if(!route_arena_prepare()){tp_restore_leg0();lg->status=TLEG_UNREACHABLE;return 0;}
 int started=0;
 for(int i=0;i<tp_dest_count;i++){if(tp_begin_cand(i)){started=1;break;}}
 if(!started){tp_ch_clear_search();route_arena_release();tp_restore_leg0();lg->status=TLEG_UNREACHABLE;return 0;}
 lg->status=TLEG_CALC;tp_active=1;tp_leg=leg_idx;
 tp_capture(&tp_work);
 return 1;
}

static void trip_plan_service_until(u32 deadline){
 trip_plan_sync();
 if(!trip.active||route_status!=R_READY||route_repair_active||!routeok||trip.count<=trip.active_idx+1){
  trip_plan_abort_active();return;
 }
 for(;;){
  if(!tp_active){
   int next=-1;
   for(int k=trip.active_idx+1;k<trip.count;k++){
    if(trip_legs[k].status==TLEG_NONE){next=k;break;}
   }
   if(next<0)return;
   if(!tp_start_leg(next)){
    if(!time_before_u32(s_now_ms(),deadline))return;
    continue;
   }
  }else{
   tp_resume_slice();
  }
  unsigned ops=0;
  while(tp_active){
   if((ops++&ROUTE_CLOCK_CHECK_MASK)==0u&&!time_before_u32(s_now_ms(),deadline)){
    tp_yield_slice();
    return;
   }
   int r=ch_expand_one();
   if(r==2){
    int ok=tp_finish_ch(tp_leg);
    if(!ok)trip_legs[tp_leg].status=TLEG_UNREACHABLE;
    tp_ch_clear_search();route_arena_release();tp_restore_leg0();
    tp_active=0;tp_leg=-1;render_background_dirty=1;
    break;
   }
   if(r==0){
    int retried=0;
    for(int nxt=tp_dest_idx+1;nxt<tp_dest_count;nxt++){if(tp_begin_cand(nxt)){retried=1;break;}}
    if(!retried){
     trip_legs[tp_leg].status=TLEG_UNREACHABLE;
     tp_ch_clear_search();route_arena_release();tp_restore_leg0();
     tp_active=0;tp_leg=-1;break;
    }
   }else if(r<0){
    trip_legs[tp_leg].status=TLEG_UNREACHABLE;
    tp_ch_clear_search();route_arena_release();tp_restore_leg0();
    tp_active=0;tp_leg=-1;break;
   }
  }
  if(!time_before_u32(s_now_ms(),deadline))return;
 }
}

static void draw_future_trip_overlay(int z,int rw4,int rcas4,int passes){
 (void)rcas4;
 if(!trip.active||trip.count<=trip.active_idx+1)return;
 int fw4=(rw4*3)/4;if(fw4<32)fw4=32;
 int fcas4=z>=6?14:10;
 for(int pass=0;pass<passes;pass++){
  int is_cas=(passes==2&&pass==0);
  u16 col=is_cas?C_ROUTE_FUTURE_CASING:C_ROUTE_FUTURE;
  int wid=is_cas?(fw4+fcas4*2):fw4;
  map_mask_begin(is_cas?MAP_MASK_ROUTE_CASING:2);
  map_mask_select(3);
  for(int k=trip.active_idx+1;k<trip.count;k++){
   if(trip_legs[k].status!=TLEG_READY||trip_legs[k].pt_n<2)continue;
   u16 off=trip_legs[k].pt_off,npts=trip_legs[k].pt_n;
   int x[255],y[255],n=0;
   for(u16 i=0;i<npts;i++){
    if(!(i&31))render_checkpoint();
    map_project_q8(trip_future_lat[off+i],trip_future_lon[off+i],z,&x[n],&y[n]);
    n++;
    if(n==255){
     route_stroke_w4(x,y,n,col,wid,z);
     x[0]=x[n-1];y[0]=y[n-1];n=1;
    }
   }
   if(n>1)route_stroke_w4(x,y,n,col,wid,z);
  }
  map_mask_end();
 }
}

#endif
