/* Priority-ordered cartographic road layout and wide antialiased rendering.
 *
 * Collects visible projected road polylines for the current map pass, scales
 * road widths across Z0-Z9 with 3D perspective tapering, solves close parallel
 * corridors in strict road-class priority order (motorway -> service) so main
 * roads expand first without encroaching on neighbours and adjacent roads
 * expand outwards to stay wide and visually distinct, then renders zero-blob
 * casings and carriageways using the existing six-plane coverage buffer.
 *
 * Strictly freestanding, zero heap allocations, deterministic integer math. */
#ifndef CLIO_ROAD_LAYOUT_H
#define CLIO_ROAD_LAYOUT_H

#define ROAD_MAX_POLYS 4096
#define ROAD_MAX_VERTS 12288
#define ROAD_MAX_SEGS  10240
#define ROAD_GRID_COLS 15
#define ROAD_GRID_ROWS 9
#define ROAD_GRID_CELLS (ROAD_GRID_COLS*ROAD_GRID_ROWS)
#define ROAD_MAX_GRID_NODES 18432

struct RoadVert {
 s32 x,y;            /* projected screen coords in Q8 (1/256 px) */
 s32 draw_x,draw_y;  /* displaced draw coords in Q8 */
 s16 tx,ty;          /* unit tangent in Q8 (len = 256) */
 s16 nx,ny;          /* left unit normal (-ty, tx) in Q8 */
 u16 want_h4;        /* desired half-width (fill) in Q4 (1/16 px) */
 u16 min_h4;         /* minimum preserved half-width in Q4 */
 u16 cas_w4;         /* casing margin per side in Q4 */
 u16 free_l4,free_r4;/* solved max available half-width left/right in Q4 */
 u16 h_l4,h_r4;      /* final asymmetric fill half-width left/right in Q4 */
 u16 out_l4,out_r4;  /* claimed outer extent (fill + casing) left/right in Q4 */
 u16 eff_w4;         /* final fill width in Q4 */
 u16 eff_cas4;       /* final casing margin per side in Q4 */
};

struct RoadEndTaper {
 u16 tgt_w4;       /* target fill width at join (d=0) in Q4 (0 = inactive) */
 s16 dx_q8,dy_q8;  /* draw-center alignment delta (tgt - self) at d=0 in Q8 */
 u8  tgt_cas4;     /* target casing width per side at join in Q4 */
 u8  len_px;       /* total S-curve arc length in px */
 u8  off_px;       /* starting arc distance at this endpoint (multi-poly chain) */
 u8  tgt_slot;     /* target fill/casing colour slot (0..5) */
 u8  flags;        /* bit 0: cross_group (cls==1 motorway -> cls>=2 surface) */
 u8  pad;
};

struct RoadPoly {
 u16 first,count;
 u8 cls,slot;
 struct RoadEndTaper taper[2]; /* 0 = start vertex, 1 = end vertex */
};

struct RoadSegRef {
 u16 v0,poly;
 s16 minx,maxx,miny,maxy; /* pixel bbox */
 s16 tx,ty,nx,ny;         /* segment unit tangent & left normal in Q8 */
 u8 cls;
};

struct RoadGridNode {
 u16 seg,next;
};

static struct RoadPoly road_polys[ROAD_MAX_POLYS];
static struct RoadVert road_verts[ROAD_MAX_VERTS];
static struct RoadSegRef road_segs[ROAD_MAX_SEGS];
static u16 road_grid_head[ROAD_GRID_CELLS];
static struct RoadGridNode road_grid_nodes[ROAD_MAX_GRID_NODES];
static int road_pass_active=0,road_pass_zoom=6,road_thin_open=0;
static int road_npolys=0,road_nverts=0,road_nsegs=0,road_ngrid=0;
static int road_has_motorway=0,road_overflow_drawn=0;

static int road_persp_q8(int sy_px,int zoom){
 if(map_tilt_q8<=0)return 256;
 int anchor=view_anchor_y();
 if(sy_px>=anchor)return 256;
 int ds=anchor-sy_px;
 int scale=256-(int)((2*map_tilt_q8*ds)/(int)PERSPECTIVE_BASE);
 int floor_q8=zoom>=7?104:(zoom==6?136:168);
 if(scale<floor_q8)scale=floor_q8;
 if(scale>256)scale=256;
 return scale;
}

static int road_gap_q4(int zoom){
 if(zoom<=6)return 24;   /* 1.50 px (> 1.0 px AA fringe -> guaranteed crisp gap) */
 if(zoom==7)return 32;   /* 2.00 px */
 if(zoom==8)return 36;   /* 2.25 px */
 return 40;              /* 2.50 px at Z9 */
}

/* Clip a Q8 segment to a conservative screen-space box so huge offscreen
 * projected coordinates never overflow segment length or normal math. */
static int road_clip_q8(s32*ax,s32*ay,s32*bx,s32*by,s32 lo_x,s32 lo_y,s32 hi_x,s32 hi_y){
 s32 x0=*ax,y0=*ay,x1=*bx,y1=*by;
 int a=(x0<lo_x?1:(x0>hi_x?2:0))|(y0<lo_y?4:(y0>hi_y?8:0));
 int b=(x1<lo_x?1:(x1>hi_x?2:0))|(y1<lo_y?4:(y1>hi_y?8:0));
 for(int iter=0;iter<8&&(a|b);iter++){
  if(a&b)return 0;
  int out=a?a:b;
  s32 nx=0,ny=0;
  if(out&12){
   ny=(out&4)?lo_y:hi_y;
   long long dy=(long long)y1-y0;
   if(!dy)return 0;
   nx=x0+(s32)(((long long)(x1-x0)*(ny-y0))/dy);
  }else{
   nx=(out&1)?lo_x:hi_x;
   long long dx=(long long)x1-x0;
   if(!dx)return 0;
   ny=y0+(s32)(((long long)(y1-y0)*(nx-x0))/dx);
  }
  if(a){x0=nx;y0=ny;a=(x0<lo_x?1:(x0>hi_x?2:0))|(y0<lo_y?4:(y0>hi_y?8:0));}
  else{x1=nx;y1=ny;b=(x1<lo_x?1:(x1>hi_x?2:0))|(y1<lo_y?4:(y1>hi_y?8:0));}
 }
 if(a|b)return 0;
 *ax=x0;*ay=y0;*bx=x1;*by=y1;
 return 1;
}

static void road_unit_vec_q8(s32 dx,s32 dy,s16*tx,s16*ty){
 s32 adx=dx<0?-dx:dx,ady=dy<0?-dy:dy;
 while(adx>16384||ady>16384){dx/=2;dy/=2;adx/=2;ady/=2;}
 u32 len2=(u32)((long long)dx*dx+(long long)dy*dy);
 if(!len2){*tx=256;*ty=0;return;}
 u32 len=isqrt32(len2);
 if(!len){*tx=256;*ty=0;return;}
 int ux=(int)udiv32((u32)adx*256u+(len>>1),len);
 int uy=(int)udiv32((u32)ady*256u+(len>>1),len);
 if(ux>256)ux=256;if(uy>256)uy=256;
 *tx=(s16)(dx<0?-ux:ux);
 *ty=(s16)(dy<0?-uy:uy);
}

static void road_pass_begin(int zoom){
 road_pass_active=1;
 road_pass_zoom=zoom;
 road_npolys=road_nverts=road_nsegs=road_ngrid=0;
 road_has_motorway=0;
 road_overflow_drawn=0;
 road_thin_open=(zoom<=5);
 if(road_thin_open){map_mask_begin(1);map_mask_select(0);}
}

static void road_draw_fallback_poly(const int*x,const int*y,int n,int cls,int zoom){
 u16 col;int wid4,cas4;
 road_style_q4(cls,zoom,&col,&wid4,&cas4);
 int slot=cls>=1&&cls<=5?6-cls:0;
 if(road_thin_open){
  map_mask_select(slot);
  for(int i=1;i<n;i++){
   int my=((y[i-1]+y[i])>>9);
   int p=road_persp_q8(my,zoom);
   int w=(wid4*p+128)>>8;if(w<16)w=16;
   map_line_q8_w4(x[i-1],y[i-1],x[i],y[i],col,w);
  }
  road_overflow_drawn=1;
  return;
 }
 map_mask_begin(1);map_mask_select(slot);
 for(int i=1;i<n;i++){
  int my=((y[i-1]+y[i])>>9);
  int p=road_persp_q8(my,zoom);
  int w=(wid4*p+128)>>8;if(w<16)w=16;
  map_line_q8_w4(x[i-1],y[i-1],x[i],y[i],col,w);
 }
 map_mask_end();
 road_overflow_drawn=1;
}

static void road_pass_add(const int*x,const int*y,int n,int cls){
 if(n<2)return;
 u16 col;int wid4,cas4;
 road_style_q4(cls,road_pass_zoom,&col,&wid4,&cas4);
 if(road_thin_open&&cas4==0){
  int slot=cls>=1&&cls<=5?6-cls:0;
  map_mask_select(slot);
  if(wid4<=16)map_stroke_polyline(x,y,n,col,1);
  else{
   int pad=(wid4>>4)+16,m=pad*256;
   for(int i=1;i<n;i++){
    s32 ax=x[i-1],ay=y[i-1],bx=x[i],by=y[i];
    if(!road_clip_q8(&ax,&ay,&bx,&by,-m,(MAP_TOP-pad)*256,(W+pad)*256,(MAP_BOTTOM+pad)*256))continue;
    int p=road_persp_q8((ay+by)>>9,road_pass_zoom);
    int w=(wid4*p+128)>>8;if(w<16)w=16;
    map_line_q8_w4(ax,ay,bx,by,col,w);
   }
  }
  return;
 }
 int pad_px=((wid4+cas4*2)>>4)+24;
 s32 lo_x=-pad_px*256,hi_x=(W+pad_px)*256;
 s32 lo_y=(MAP_TOP-pad_px)*256,hi_y=(MAP_BOTTOM+pad_px)*256;
 int do_subdiv=(road_pass_zoom>=7||(road_pass_zoom==6&&map_tilt_q8>0));
 int slot=cls>=1&&cls<=5?6-cls:0;
 int cur_poly=-1;

 for(int i=1;i<n;i++){
  s32 ax=x[i-1],ay=y[i-1],bx=x[i],by=y[i];
  if(!road_clip_q8(&ax,&ay,&bx,&by,lo_x,lo_y,hi_x,hi_y)){cur_poly=-1;continue;}
  s32 dx=bx-ax,dy=by-ay;
  s32 adx=dx<0?-dx:dx,ady=dy<0?-dy:dy;
  if(adx<8&&ady<8)continue;
  int steps=1;
  if(do_subdiv){
   s32 approx=adx>ady?adx+(ady>>1):ady+(adx>>1);
   if(approx>42*256){
    steps=(approx/(36*256))+1;
    if(steps>12)steps=12;
   }
  }
  /* Continue current polyline only if it joins seamlessly at ax,ay. */
  if(cur_poly>=0){
   struct RoadPoly*rp=&road_polys[cur_poly];
   struct RoadVert*last=&road_verts[rp->first+rp->count-1];
   if(iabs32(last->x-ax)>4||iabs32(last->y-ay)>4)cur_poly=-1;
  }
  int need_verts=(cur_poly<0?1:0)+steps;
  if(road_nverts+need_verts>ROAD_MAX_VERTS||(cur_poly<0&&road_npolys>=ROAD_MAX_POLYS)){
   road_draw_fallback_poly(x+i-1,y+i-1,n-(i-1),cls,road_pass_zoom);
   return;
  }
  if(cur_poly<0){
   cur_poly=road_npolys++;
   memzero(&road_polys[cur_poly],sizeof(road_polys[cur_poly]));
   road_polys[cur_poly].first=(u16)road_nverts;
   road_polys[cur_poly].count=1;
   road_polys[cur_poly].cls=(u8)cls;
   road_polys[cur_poly].slot=(u8)slot;
   if(cls==1)road_has_motorway=1;
   struct RoadVert*v=&road_verts[road_nverts++];
   memzero(v,sizeof(*v));
   v->x=v->draw_x=ax;v->y=v->draw_y=ay;
  }
  struct RoadPoly*rp=&road_polys[cur_poly];
  for(int s=1;s<=steps;s++){
   s32 px=s==steps?bx:ax+(s32)(((long long)dx*s)/steps);
   s32 py=s==steps?by:ay+(s32)(((long long)dy*s)/steps);
   struct RoadVert*v=&road_verts[road_nverts++];
   memzero(v,sizeof(*v));
   v->x=v->draw_x=px;v->y=v->draw_y=py;
   rp->count++;
  }
  /* If endpoint was clipped before x[i],y[i], break chain for next segment. */
  if(bx!=x[i]||by!=y[i])cur_poly=-1;
 }
}

static void road_grid_insert(int seg_idx,s16 minx,s16 maxx,s16 miny,s16 maxy){
 int c0=minx/32,c1=maxx/32,r0=(miny-MAP_TOP)/32,r1=(maxy-MAP_TOP)/32;
 if(c0<0)c0=0;if(c1>=ROAD_GRID_COLS)c1=ROAD_GRID_COLS-1;
 if(r0<0)r0=0;if(r1>=ROAD_GRID_ROWS)r1=ROAD_GRID_ROWS-1;
 for(int r=r0;r<=r1;r++)for(int c=c0;c<=c1;c++){
  if(road_ngrid>=ROAD_MAX_GRID_NODES)return;
  int cell=r*ROAD_GRID_COLS+c;
  int node=road_ngrid++;
  road_grid_nodes[node].seg=(u16)seg_idx;
  road_grid_nodes[node].next=road_grid_head[cell];
  road_grid_head[cell]=(u16)(node+1);
 }
}

static void road_pass_solve(void){
 int z=road_pass_zoom;
 /* 1. Compute per-vertex desired widths, tangents, normals and spatial index. */
 for(int c=0;c<ROAD_GRID_CELLS;c++)road_grid_head[c]=0;
 road_nsegs=0;road_ngrid=0;
 int max_half_q4=0;

 for(int p=0;p<road_npolys;p++){
  struct RoadPoly*rp=&road_polys[p];
  u16 col;int wid4,cas4;
  road_style_q4(rp->cls,z,&col,&wid4,&cas4);
  int f=rp->first,cnt=rp->count;
  for(int i=0;i<cnt;i++){
   struct RoadVert*v=&road_verts[f+i];
   int persp=road_persp_q8(v->y>>8,z);
   int w4=(wid4*persp+128)>>8;
   if(w4<16)w4=16;
   int c4=cas4?((cas4*persp+128)>>8):0;
   if(cas4&&c4<8)c4=8;
   int h4=(w4+1)>>1;
   int mh4=h4>>2;if(mh4<10)mh4=10;if(mh4>h4)mh4=h4;
   v->want_h4=(u16)h4;
   v->min_h4=(u16)mh4;
   v->cas_w4=(u16)c4;
   v->free_l4=v->free_r4=1024;
   v->h_l4=v->h_r4=(u16)h4;
   v->out_l4=v->out_r4=(u16)(h4+c4);
   v->eff_w4=(u16)w4;
   v->eff_cas4=(u16)c4;
   if(h4+c4>max_half_q4)max_half_q4=h4+c4;

   s32 tdx=0,tdy=0;
   if(i>0){
    s16 ux,uy;road_unit_vec_q8(v->x-road_verts[f+i-1].x,v->y-road_verts[f+i-1].y,&ux,&uy);
    tdx+=ux;tdy+=uy;
   }
   if(i+1<cnt){
    s16 ux,uy;road_unit_vec_q8(road_verts[f+i+1].x-v->x,road_verts[f+i+1].y-v->y,&ux,&uy);
    tdx+=ux;tdy+=uy;
   }
   road_unit_vec_q8(tdx,tdy,&v->tx,&v->ty);
   v->nx=(s16)(-v->ty);v->ny=v->tx;

   /* At Z4-Z5 skip indexing 1 px service lanes into the grid for fast overview frames. */
   int index_seg=(z>=6)||(z==5&&rp->cls<=6)||(z==4&&rp->cls<=5);
   if(index_seg&&i+1<cnt&&road_nsegs<ROAD_MAX_SEGS){
    struct RoadVert*v1=&road_verts[f+i+1];
    struct RoadSegRef*sg=&road_segs[road_nsegs];
    sg->v0=(u16)(f+i);sg->poly=(u16)p;sg->cls=rp->cls;
    s32 x0=v->x>>8,y0=v->y>>8,x1=v1->x>>8,y1=v1->y>>8;
    sg->minx=(s16)(x0<x1?x0:x1);sg->maxx=(s16)(x0>x1?x0:x1);
    sg->miny=(s16)(y0<y1?y0:y1);sg->maxy=(s16)(y0>y1?y0:y1);
    road_unit_vec_q8(v1->x-v->x,v1->y-v->y,&sg->tx,&sg->ty);
    sg->nx=(s16)(-sg->ty);sg->ny=sg->tx;
    road_grid_insert(road_nsegs,sg->minx,sg->maxx,sg->miny,sg->maxy);
    road_nsegs++;
   }
  }
 }

 /* Only solve parallel road separation when roads are wide enough to overlap. */
 if(z<4||road_nsegs<2)return;

 int gap4=road_gap_q4(z);
 /* 2. Priority-ordered pass: cls 1 (motorway) -> cls 7 (service).
  * Main roads expand first without encroaching on minor road cores; lower-class
  * roads then see the exact claimed outer edge of higher-class roads and expand
  * outwards on their free side to retain width without overlapping. */
 for(int cls=1;cls<=7;cls++){
  if(z==4&&cls>=6)break;
  if(z==5&&cls>=7)break;
  render_checkpoint();
  for(int p=0;p<road_npolys;p++){
   if(road_polys[p].cls!=cls)continue;
   int f=road_polys[p].first,cnt=road_polys[p].count;
   for(int i=0;i<cnt;i++){
    struct RoadVert*v=&road_verts[f+i];
    int want_h4=(int)v->want_h4,cas4=(int)v->cas_w4;
    int sr_q4=want_h4+cas4+max_half_q4+gap4+32;
    int sr=(sr_q4>>4)+2;if(sr>56)sr=56;
    int vx_px=v->x>>8,vy_px=v->y>>8;
    int c0=(vx_px-sr)/32,c1=(vx_px+sr)/32;
    int r0=(vy_px-sr-MAP_TOP)/32,r1=(vy_px+sr-MAP_TOP)/32;
    if(c0<0)c0=0;if(c1>=ROAD_GRID_COLS)c1=ROAD_GRID_COLS-1;
    if(r0<0)r0=0;if(r1>=ROAD_GRID_ROWS)r1=ROAD_GRID_ROWS-1;

    int free_l4=1024,free_r4=1024;
    s32 vx4=v->x>>4,vy4=v->y>>4;

    for(int r=r0;r<=r1;r++)for(int c=c0;c<=c1;c++){
     u16 node=road_grid_head[r*ROAD_GRID_COLS+c];
     while(node){
      struct RoadGridNode*gn=&road_grid_nodes[node-1];
      node=gn->next;
      struct RoadSegRef*sg=&road_segs[gn->seg];
      if((int)sg->poly==p)continue;
      if(vx_px+sr<sg->minx||vx_px-sr>sg->maxx||vy_px+sr<sg->miny||vy_px-sr>sg->maxy)continue;
      /* Smooth tangent alignment ramp (|cos(theta)| 0.75..0.92, ~41..23 deg):
       * cross-streets stay untouched while curved parallel roads stay kink-free. */
      int align=((int)v->tx*(int)sg->tx+(int)v->ty*(int)sg->ty)>>8;
      if(align<0)align=-align;
      if(align<192)continue;
      int align_q8=align>=236?256:((align-192)*256)/44;

      struct RoadVert*a=&road_verts[sg->v0];
      struct RoadVert*b=&road_verts[sg->v0+1];
      s32 ax4=a->x>>4,ay4=a->y>>4,bx4=b->x>>4,by4=b->y>>4;
      s32 sx4=bx4-ax4,sy4=by4-ay4;
      long long len2=(long long)sx4*sx4+(long long)sy4*sy4;
      if(len2<16)continue;
      long long dot=(long long)(vx4-ax4)*sx4+(long long)(vy4-ay4)*sy4;
      if(dot<-(len2>>2)||dot>len2+(len2>>2))continue;
      s32 qx4,qy4;
      if(dot<=0){qx4=ax4;qy4=ay4;}
      else if(dot>=len2){qx4=bx4;qy4=by4;}
      else{
       qx4=ax4+(s32)((dot*sx4)/len2);
       qy4=ay4+(s32)((dot*sy4)/len2);
      }
      s32 rx4=qx4-vx4,ry4=qy4-vy4;
      int sdist4=(int)(((long long)rx4*v->nx+(long long)ry4*v->ny)>>8);
      int dist4=sdist4<0?-sdist4:sdist4;
      /* Smooth longitudinal cushion so passing a curved neighbour's joint never jumps. */
      int along4=(int)(((long long)rx4*v->tx+(long long)ry4*v->ty)>>8);
      if(along4<0)along4=-along4;
      if(along4>=dist4)continue;
      int along_q8=along4<=(dist4>>1)?256:((dist4-along4)*512)/dist4;
      /* Shared junction/merge vertices (<= 1.5 px) stay untouched; ramp smoothly
       * over 1.5..4.5 px so slip roads peel gracefully away from main carriageways. */
      if(dist4<=24||dist4>sr_q4)continue;
      int dist_q8=dist4>=72?256:((dist4-24)*256)/48;
      int ramp_q8=(((dist_q8*align_q8)>>8)*along_q8)>>8;
      if(ramp_q8<=0)continue;

      int avail4;
      if((int)sg->cls<cls){
       /* Higher-priority road already expanded: respect its exact claimed outer edge. */
       int v_on_s_left=(((long long)(-rx4)*sg->nx+(long long)(-ry4)*sg->ny)>=0);
       int s_claim=v_on_s_left?(((int)a->out_l4+(int)b->out_l4+1)>>1):(((int)a->out_r4+(int)b->out_r4+1)>>1);
       avail4=dist4-s_claim-gap4-cas4;
      }else if((int)sg->cls==cls){
       /* Equal-priority parallel roads (e.g. dual carriageways): split cleanly. */
       avail4=((dist4-gap4)>>1)-cas4;
      }else{
       /* Main road seeing a lower-priority neighbour: expand first, but stop clear
        * of the neighbour's core so it remains distinct and can expand outwards. */
       int s_core=(((int)a->min_h4+(int)b->min_h4+1)>>1)+(((int)a->cas_w4+(int)b->cas_w4+1)>>1);
       avail4=dist4-s_core-gap4-cas4;
      }
      int eff_avail=want_h4+(((avail4-want_h4)*ramp_q8)>>8);
      if(eff_avail<0)eff_avail=0;
      if(sdist4>0){if(eff_avail<free_l4)free_l4=eff_avail;}
      else{if(eff_avail<free_r4)free_r4=eff_avail;}
     }
    }

    v->free_l4=(u16)free_l4;
    v->free_r4=(u16)free_r4;
    int hL=want_h4<free_l4?want_h4:free_l4;
    int hR=want_h4<free_r4?want_h4:free_r4;
    /* Outward expansion: when one side is constrained by an adjacent road,
     * expand the open opposite side outwards to preserve generous road width
     * without touching the neighbour. Main roads shift gently so GPS/route
     * stay centered; secondary/minor/service roads expand up to full deficit. */
    int max_out=cls<=3?(want_h4/3):want_h4;
    if(hL<want_h4&&free_r4>hR){
     int add=want_h4-hL,room=free_r4-hR;
     if(add>room)add=room;
     if(add>max_out)add=max_out;
     hR+=add;
    }else if(hR<want_h4&&free_l4>hL){
     int add=want_h4-hR,room=free_l4-hL;
     if(add>room)add=room;
     if(add>max_out)add=max_out;
     hL+=add;
    }
    v->h_l4=(u16)hL;
    v->h_r4=(u16)hR;
   }

   /* Smooth asymmetric half-widths along interior polyline vertices while
    * strictly respecting each vertex's free_l4/free_r4 clearance ceiling. */
   for(int pass=0;pass<2&&cnt>=3;pass++){
    u16 prev_l=road_verts[f].h_l4,prev_r=road_verts[f].h_r4;
    for(int i=1;i+1<cnt;i++){
     struct RoadVert*v=&road_verts[f+i];
     u16 cur_l=v->h_l4,cur_r=v->h_r4;
     u16 next_l=road_verts[f+i+1].h_l4,next_r=road_verts[f+i+1].h_r4;
     int sl=((int)prev_l+2*(int)cur_l+(int)next_l+2)>>2;
     int sr=((int)prev_r+2*(int)cur_r+(int)next_r+2)>>2;
     if(sl>(int)v->free_l4)sl=(int)v->free_l4;
     if(sr>(int)v->free_r4)sr=(int)v->free_r4;
     v->h_l4=(u16)sl;v->h_r4=(u16)sr;
     prev_l=cur_l;prev_r=cur_r;
    }
   }

   /* Finalize claimed boundaries and shifted draw coordinates for this polyline. */
   for(int i=0;i<cnt;i++){
    struct RoadVert*v=&road_verts[f+i];
    int hL=(int)v->h_l4,hR=(int)v->h_r4,cas4=(int)v->cas_w4;
    int eff_w=hL+hR,eff_cas=cas4;
    if(eff_w<16&&cas4>0){
     int borrow=16-eff_w;
     if(borrow>cas4*2)borrow=cas4*2;
     eff_w+=borrow;
     eff_cas=cas4-(borrow+1)/2;
     if(eff_cas<0)eff_cas=0;
    }
    if(eff_w<12)eff_w=12;
    v->out_l4=(u16)(hL+eff_cas);
    v->out_r4=(u16)(hR+eff_cas);
    int shift4=(hL-hR)/2;
    v->draw_x=v->x+((shift4*(int)v->nx)>>4);
    v->draw_y=v->y+((shift4*(int)v->ny)>>4);
    v->eff_w4=(u16)eff_w;
    v->eff_cas4=(u16)eff_cas;
   }
  }
 }
}

static int road_seg_len_q8(s32 x0,s32 y0,s32 x1,s32 y1){
 s32 dx=x1-x0,dy=y1-y0;
 s32 adx=dx<0?-dx:dx,ady=dy<0?-dy:dy;
 int sh=0;
 while(adx>32767||ady>32767){adx>>=1;ady>>=1;sh++;}
 return (int)(isqrt32((u32)((long long)adx*adx+(long long)ady*ady))<<sh);
}

static int road_poly_len_q8(int p){
 const struct RoadPoly*rp=&road_polys[p];
 int total=0,f=rp->first,cnt=rp->count;
 for(int i=1;i<cnt;i++){
  const struct RoadVert*a=&road_verts[f+i-1],*b=&road_verts[f+i];
  total+=road_seg_len_q8(a->draw_x,a->draw_y,b->draw_x,b->draw_y);
 }
 return total;
}

static int road_poly_is_loop(int p){
 const struct RoadPoly*rp=&road_polys[p];
 if(rp->count<3)return 0;
 const struct RoadVert*a=&road_verts[rp->first];
 const struct RoadVert*b=&road_verts[rp->first+rp->count-1];
 return iabs32(a->x-b->x)<=384&&iabs32(a->y-b->y)<=384;
}

static int road_smoothstep_q8(int num,int den){
 if(num<=0||den<=0)return 0;
 if(num>=den)return 256;
 int t=(int)(((long long)num*256+(den>>1))/den);
 int t2=(t*t)>>8;
 int s=3*t2-((2*t2*t)>>8);
 if(s<0)s=0;if(s>256)s=256;
 return s;
}

/* Detect end-to-end wider-to-narrower road transitions so wide carriageways
 * glide smoothly into narrower continuations via a C1 S-curve funnel and
 * optional colour cross-fade, with zero bulbous overhangs or hard steps. */
static void road_solve_transitions(void){
 int z=road_pass_zoom;
 if(z<4||road_nsegs<2)return;
 const int tol_q8=384; /* 1.5 px captures same-tile, cross-tile and simplified junctions */
 int min_len=z>=9?24:(z==8?18:(z==7?14:(z==6?11:8)));
 int max_len=z>=9?64:(z==8?48:(z==7?36:24));

 for(int p=0;p<road_npolys;p++){
  struct RoadPoly*rp=&road_polys[p];
  if(rp->count<2||road_poly_is_loop(p))continue;
  for(int end=0;end<2;end++){
   int vi=end==0?rp->first:(rp->first+rp->count-1);
   const struct RoadVert*v=&road_verts[vi];
   if(v->x<-16*256||v->x>(W+16)*256||v->y<(MAP_TOP-16)*256||v->y>(MAP_BOTTOM+16)*256)continue;

   int vx_px=v->x>>8,vy_px=v->y>>8;
   int c0=(vx_px-3)/32,c1=(vx_px+3)/32;
   int r0=(vy_px-3-MAP_TOP)/32,r1=(vy_px+3-MAP_TOP)/32;
   if(c0<0)c0=0;if(c1>=ROAD_GRID_COLS)c1=ROAD_GRID_COLS-1;
   if(r0<0)r0=0;if(r1>=ROAD_GRID_ROWS)r1=ROAD_GRID_ROWS-1;
   s32 vx4=v->x>>4,vy4=v->y>>4;

   int blocked=0,best_w4=0,best_cas4=0,best_slot=0,best_cls=99;
   s32 best_dx=0,best_dy=0;
   for(int r=r0;r<=r1&&!blocked;r++)for(int c=c0;c<=c1&&!blocked;c++){
    u16 node=road_grid_head[r*ROAD_GRID_COLS+c];
    while(node){
     struct RoadGridNode*gn=&road_grid_nodes[node-1];
     node=gn->next;
     struct RoadSegRef*sg=&road_segs[gn->seg];
     if((int)sg->poly==p)continue;
     if(vx_px+3<sg->minx||vx_px-3>sg->maxx||vy_px+3<sg->miny||vy_px-3>sg->maxy)continue;
     const struct RoadVert*a=&road_verts[sg->v0],*b=&road_verts[sg->v0+1];
     s32 ax4=a->x>>4,ay4=a->y>>4,sx4=(b->x>>4)-ax4,sy4=(b->y>>4)-ay4;
     long long len2=(long long)sx4*sx4+(long long)sy4*sy4;
     int t_q12=0;
     s32 qx=a->x,qy=a->y;
     if(len2>0){
      long long dot=(long long)(vx4-ax4)*sx4+(long long)(vy4-ay4)*sy4;
      if(dot>=len2){t_q12=4096;qx=b->x;qy=b->y;}
      else if(dot>0){
       t_q12=(int)((dot*4096)/len2);
       qx=a->x+(s32)(((long long)(b->x-a->x)*t_q12)>>12);
       qy=a->y+(s32)(((long long)(b->y-a->y)*t_q12)>>12);
      }
     }
     if(iabs32(qx-v->x)>tol_q8||iabs32(qy-v->y)>tol_q8)continue;
     int uw4=(int)a->eff_w4+((((int)b->eff_w4-(int)a->eff_w4)*t_q12+2048)>>12);
     int qcls=(int)road_polys[sg->poly].cls;
     if(qcls<=(int)rp->cls||uw4>=(int)v->eff_w4-4){blocked=1;break;}
     if(uw4>best_w4||(uw4==best_w4&&qcls<best_cls)){
      int ucas4=(int)a->eff_cas4+((((int)b->eff_cas4-(int)a->eff_cas4)*t_q12+2048)>>12);
      s32 udx=a->draw_x+(s32)(((long long)(b->draw_x-a->draw_x)*t_q12)>>12);
      s32 udy=a->draw_y+(s32)(((long long)(b->draw_y-a->draw_y)*t_q12)>>12);
      best_w4=uw4;
      best_cas4=ucas4;
      best_slot=(int)road_polys[sg->poly].slot;
      best_cls=qcls;
      best_dx=udx-v->draw_x;
      best_dy=udy-v->draw_y;
     }
    }
   }
   if(blocked||best_w4<=0)continue;
   int dw4=(int)v->eff_w4-best_w4;
   if(dw4<6&&best_slot==(int)rp->slot)continue;

   int len_px=(dw4*3+8)>>4;
   if(len_px<min_len)len_px=min_len;
   if(len_px>max_len)len_px=max_len;
   if(best_dx<-2048)best_dx=-2048;if(best_dx>2048)best_dx=2048;
   if(best_dy<-2048)best_dy=-2048;if(best_dy>2048)best_dy=2048;

   struct RoadEndTaper*tp=&rp->taper[end];
   tp->tgt_w4=(u16)best_w4;
   tp->tgt_cas4=(u8)(best_cas4>255?255:best_cas4);
   tp->len_px=(u8)len_px;
   tp->off_px=0;
   tp->dx_q8=(s16)best_dx;
   tp->dy_q8=(s16)best_dy;
   tp->tgt_slot=(u8)best_slot;
   tp->flags=(u8)((rp->cls==1&&best_cls>=2)?1:0);
  }
 }

 /* Resolve short polylines: clamp opposing double tapers or propagate an
  * uninterrupted S-curve backward across up to 2 collinear same-class segments. */
 for(int p=0;p<road_npolys;p++){
  struct RoadPoly*rp=&road_polys[p];
  int has0=(rp->taper[0].tgt_w4!=0&&rp->taper[0].off_px==0);
  int has1=(rp->taper[1].tgt_w4!=0&&rp->taper[1].off_px==0);
  if(!has0&&!has1)continue;
  int plen_px=(road_poly_len_q8(p)+128)>>8;
  if(plen_px<1)plen_px=1;
  if(has0&&has1){
   int sum=(int)rp->taper[0].len_px+(int)rp->taper[1].len_px;
   if(sum>plen_px){
    int l0=(int)rp->taper[0].len_px*plen_px/sum;
    int l1=plen_px-l0;
    if(l0<1)l0=1;if(l1<1)l1=1;
    rp->taper[0].len_px=(u8)l0;
    rp->taper[1].len_px=(u8)l1;
   }
   continue;
  }
  int end=has0?0:1;
  int want_len=(int)rp->taper[end].len_px;
  if(plen_px>=want_len)continue;

  int chain_p[3]={p,-1,-1},chain_e[3]={end,0,0},nchain=1;
  int cur_p=p,cur_end=end,cur_off=plen_px;
  for(int hop=0;hop<2&&cur_off<want_len;hop++){
   struct RoadPoly*cp=&road_polys[cur_p];
   int oend=1-cur_end;
   int ovi=oend==0?cp->first:(cp->first+cp->count-1);
   int ovi2=oend==0?(cp->first+1):(cp->first+cp->count-2);
   const struct RoadVert*ov=&road_verts[ovi];
   s16 ux,uy;
   road_unit_vec_q8(road_verts[ovi2].x-ov->x,road_verts[ovi2].y-ov->y,&ux,&uy);

   int vx_px=ov->x>>8,vy_px=ov->y>>8;
   int c0=(vx_px-3)/32,c1=(vx_px+3)/32;
   int r0=(vy_px-3-MAP_TOP)/32,r1=(vy_px+3-MAP_TOP)/32;
   if(c0<0)c0=0;if(c1>=ROAD_GRID_COLS)c1=ROAD_GRID_COLS-1;
   if(r0<0)r0=0;if(r1>=ROAD_GRID_ROWS)r1=ROAD_GRID_ROWS-1;

   int cand_p=-1,cand_e=0,same_or_wider=0;
   for(int r=r0;r<=r1&&same_or_wider<=1;r++)for(int c=c0;c<=c1&&same_or_wider<=1;c++){
    u16 node=road_grid_head[r*ROAD_GRID_COLS+c];
    while(node){
     struct RoadGridNode*gn=&road_grid_nodes[node-1];
     node=gn->next;
     struct RoadSegRef*sg=&road_segs[gn->seg];
     int q=(int)sg->poly;
     if(q==cur_p)continue;
     if(vx_px+3<sg->minx||vx_px-3>sg->maxx||vy_px+3<sg->miny||vy_px-3>sg->maxy)continue;
     struct RoadPoly*rq=&road_polys[q];
     for(int k=0;k<2;k++){
      int qvi=(int)sg->v0+k;
      const struct RoadVert*qu=&road_verts[qvi];
      if(iabs32(qu->x-ov->x)>tol_q8||iabs32(qu->y-ov->y)>tol_q8)continue;
      if(rq->cls<=cp->cls){
       if(q!=cand_p)same_or_wider++;
       int is_q0=(qvi==(int)rq->first),is_q1=(qvi==(int)rq->first+rq->count-1);
       if(rq->cls==cp->cls&&(is_q0||is_q1)&&!road_poly_is_loop(q)){
        int qe=is_q0?0:1;
        int qvi2=is_q0?(rq->first+1):(rq->first+rq->count-2);
        s16 qx,qy;
        road_unit_vec_q8(road_verts[qvi2].x-qu->x,road_verts[qvi2].y-qu->y,&qx,&qy);
        int dot=((int)ux*(int)qx+(int)uy*(int)qy)>>8;
        if(dot<=-170&&!rq->taper[qe].tgt_w4&&!rq->taper[1-qe].tgt_w4){
         cand_p=q;cand_e=qe;
        }
       }
      }
     }
    }
   }
   if(same_or_wider!=1||cand_p<0)break;
   struct RoadPoly*nq=&road_polys[cand_p];
   nq->taper[cand_e]=rp->taper[end];
   nq->taper[cand_e].off_px=(u8)(cur_off>255?255:cur_off);
   chain_p[nchain]=cand_p;chain_e[nchain]=cand_e;nchain++;
   int qlen=(road_poly_len_q8(cand_p)+128)>>8;
   if(qlen<1)qlen=1;
   cur_off+=qlen;
   cur_p=cand_p;cur_end=cand_e;
  }
  if(cur_off<want_len){
   int cl=cur_off<2?2:cur_off;
   for(int i=0;i<nchain;i++)road_polys[chain_p[i]].taper[chain_e[i]].len_px=(u8)cl;
  }
 }
}

static void road_draw_taper_piece(int x0,int y0,int x1,int y1,
                                  int w0,int w1,int cas0,int cas1,
                                  int fa0,int fa1,int slot,int tgt_slot,
                                  int is_casing,int caps,int fade_caps){
 if(is_casing){
  if(cas0<=0&&cas1<=0)return;
  int cw0=w0+cas0*2,cw1=w1+cas1*2;
  if(tgt_slot<slot&&road_casing_colours[tgt_slot]!=road_casing_colours[slot]){
   map_mask_select(tgt_slot);
   map_line_q8_taper(x0,y0,x1,y1,road_casing_colours[tgt_slot],cw0,cw1,32,32,caps);
   map_mask_select(slot);
   map_line_q8_taper(x0,y0,x1,y1,road_casing_colours[slot],cw0,cw1,fa0,fa1,fade_caps);
  }else{
   map_line_q8_taper(x0,y0,x1,y1,road_casing_colours[slot],cw0,cw1,32,32,caps);
  }
 }else{
  if(tgt_slot<slot){
   map_mask_select(tgt_slot);
   map_line_q8_taper(x0,y0,x1,y1,road_colours[tgt_slot],w0,w1,32,32,3);
   map_mask_select(slot);
   map_line_q8_taper(x0,y0,x1,y1,road_colours[slot],w0,w1,fa0,fa1,fade_caps);
  }else{
   map_line_q8_taper(x0,y0,x1,y1,road_colours[slot],w0,w1,32,32,3);
  }
 }
}

static void road_draw_taper_span(const struct RoadVert*a,const struct RoadVert*b,
                                 int s0_q8,int seg_q8,int total_q8,
                                 int ia,int ib,const struct RoadEndTaper*tp,
                                 int end_idx,int slot,int is_casing){
 int span_q8=ib-ia;
 if(span_q8<=0||seg_q8<=0)return;
 int steps=(span_q8+1023)>>10; /* ~4 px per S-curve step */
 if(steps<2&&span_q8>=512)steps=2;
 if(steps<1)steps=1;
 if(steps>8)steps=8;

 int len_q8=(int)tp->len_px*256;
 int off_q8=(int)tp->off_px*256;
 int tgt_w4=(int)tp->tgt_w4,tgt_cas4=(int)tp->tgt_cas4;
 int tgt_slot=(int)tp->tgt_slot,cross_grp=(tp->flags&1);

 int px=0,py=0,pw=0,pcas=0,pfa=32,pd=0;
 for(int k=0;k<=steps;k++){
  int t=k==steps?ib:(ia+(int)(((long long)span_q8*k)/steps));
  int rel=t-s0_q8;
  int bx=a->draw_x+(int)(((long long)(b->draw_x-a->draw_x)*rel)/seg_q8);
  int by=a->draw_y+(int)(((long long)(b->draw_y-a->draw_y)*rel)/seg_q8);
  int bw=(int)a->eff_w4+(int)(((long long)((int)b->eff_w4-(int)a->eff_w4)*rel)/seg_q8);
  int bc=(int)a->eff_cas4+(int)(((long long)((int)b->eff_cas4-(int)a->eff_cas4)*rel)/seg_q8);
  int d_q8=end_idx==0?(off_q8+t):(off_q8+(total_q8-t));
  int S=road_smoothstep_q8(d_q8,len_q8);
  int w4=tgt_w4+(((bw-tgt_w4)*S+128)>>8);
  int cas4=tgt_cas4+(((bc-tgt_cas4)*S+128)>>8);
  int sx=bx+(((int)tp->dx_q8*(256-S)+128)>>8);
  int sy=by+(((int)tp->dy_q8*(256-S)+128)>>8);
  int fa=(S*32+128)>>8;
  if(k>0){
   int caps=3;
   if(is_casing&&cross_grp){
    if(pd==0)caps&=~1;
    if(d_q8==0)caps&=~2;
   }
   /* For the colour-fade overlay, keep a round cap only on the higher-alpha
    * end (filling outer bend wedges invisibly) and use a half-px padded butt
    * cap on the lower-alpha end so wide round caps never ring into the fade. */
   int fade_caps=end_idx==0?(2|4):(1|8);
   if(is_casing&&cross_grp){
    if(pd==0)fade_caps&=~(1|4);
    if(d_q8==0)fade_caps&=~(2|8);
   }
   road_draw_taper_piece(px,py,sx,sy,pw,w4,pcas,cas4,pfa,fa,slot,tgt_slot,is_casing,caps,fade_caps);
  }
  px=sx;py=sy;pw=w4;pcas=cas4;pfa=fa;pd=d_q8;
 }
}

static void road_draw_poly_pass(const struct RoadPoly*rp,int is_casing){
 int slot=rp->slot,f=rp->first,cnt=rp->count;
 u16 col=is_casing?road_casing_colours[slot]:road_colours[slot];
 int has0=(rp->taper[0].tgt_w4!=0),has1=(rp->taper[1].tgt_w4!=0);
 map_mask_select(slot);
 if(!has0&&!has1){
  for(int i=1;i<cnt;i++){
   if(!(i&7))render_checkpoint();
   const struct RoadVert*a=&road_verts[f+i-1],*b=&road_verts[f+i];
   if(is_casing){
    if(!a->eff_cas4&&!b->eff_cas4)continue;
    map_line_q8_taper(a->draw_x,a->draw_y,b->draw_x,b->draw_y,col,
                      (int)a->eff_w4+(int)a->eff_cas4*2,
                      (int)b->eff_w4+(int)b->eff_cas4*2,32,32,3);
   }else{
    map_line_q8_taper(a->draw_x,a->draw_y,b->draw_x,b->draw_y,col,
                      (int)a->eff_w4,(int)b->eff_w4,32,32,3);
   }
  }
  return;
 }

 int total_q8=0;
 for(int i=1;i<cnt;i++){
  const struct RoadVert*a=&road_verts[f+i-1],*b=&road_verts[f+i];
  total_q8+=road_seg_len_q8(a->draw_x,a->draw_y,b->draw_x,b->draw_y);
 }
 if(total_q8<=0)return;
 int b0=has0?((int)rp->taper[0].len_px-(int)rp->taper[0].off_px)*256:0;
 int b1=has1?(total_q8-((int)rp->taper[1].len_px-(int)rp->taper[1].off_px)*256):total_q8;
 if(b0<0)b0=0;if(b0>total_q8)b0=total_q8;
 if(b1<b0)b1=b0;if(b1>total_q8)b1=total_q8;
 int fade0=has0&&((int)rp->taper[0].tgt_slot<slot)&&(!is_casing||road_casing_colours[rp->taper[0].tgt_slot]!=road_casing_colours[slot]);
 int fade1=has1&&((int)rp->taper[1].tgt_slot<slot)&&(!is_casing||road_casing_colours[rp->taper[1].tgt_slot]!=road_casing_colours[slot]);

 int s_q8=0;
 for(int i=1;i<cnt;i++){
  if(!(i&7))render_checkpoint();
  const struct RoadVert*a=&road_verts[f+i-1],*b=&road_verts[f+i];
  int seg_q8=road_seg_len_q8(a->draw_x,a->draw_y,b->draw_x,b->draw_y);
  int s0=s_q8,s1=s_q8+seg_q8;
  s_q8=s1;
  if(seg_q8<=0)continue;
  /* Sub-interval 1: inside taper[0] [s0, min(s1, b0)] */
  if(has0&&s0<b0){
   int ib=s1<b0?s1:b0;
   road_draw_taper_span(a,b,s0,seg_q8,total_q8,s0,ib,&rp->taper[0],0,slot,is_casing);
  }
  /* Sub-interval 2: untapered middle [max(s0, b0), min(s1, b1)] */
  int ma=s0>b0?s0:b0,mb=s1<b1?s1:b1;
  if(ma<mb){
   int ra=ma-s0,rb=mb-s0;
   int x0=ra==0?a->draw_x:a->draw_x+(int)(((long long)(b->draw_x-a->draw_x)*ra)/seg_q8);
   int y0=ra==0?a->draw_y:a->draw_y+(int)(((long long)(b->draw_y-a->draw_y)*ra)/seg_q8);
   int x1=rb==seg_q8?b->draw_x:a->draw_x+(int)(((long long)(b->draw_x-a->draw_x)*rb)/seg_q8);
   int y1=rb==seg_q8?b->draw_y:a->draw_y+(int)(((long long)(b->draw_y-a->draw_y)*rb)/seg_q8);
   int w0=(int)a->eff_w4+(int)(((long long)((int)b->eff_w4-(int)a->eff_w4)*ra)/seg_q8);
   int w1=(int)a->eff_w4+(int)(((long long)((int)b->eff_w4-(int)a->eff_w4)*rb)/seg_q8);
   int c0=(int)a->eff_cas4+(int)(((long long)((int)b->eff_cas4-(int)a->eff_cas4)*ra)/seg_q8);
   int c1=(int)a->eff_cas4+(int)(((long long)((int)b->eff_cas4-(int)a->eff_cas4)*rb)/seg_q8);
   int mid_caps=3;
   if(fade0&&ma==b0)mid_caps=(mid_caps&~1)|4;
   if(fade1&&mb==b1)mid_caps=(mid_caps&~2)|8;
   map_mask_select(slot);
   if(is_casing){
    if(c0>0||c1>0)map_line_q8_taper(x0,y0,x1,y1,col,w0+c0*2,w1+c1*2,32,32,mid_caps);
   }else{
    map_line_q8_taper(x0,y0,x1,y1,col,w0,w1,32,32,mid_caps);
   }
  }
  /* Sub-interval 3: inside taper[1] [max(s0, b1), s1] */
  if(has1&&s1>b1){
   int ia=s0>b1?s0:b1;
   road_draw_taper_span(a,b,s0,seg_q8,total_q8,ia,s1,&rp->taper[1],1,slot,is_casing);
  }
 }
}

static void road_draw_group(int motorways_only){
 /* Pass A: subtle kerb casings into the 6-class coverage buffer (zero blobs). */
 map_mask_begin(MAP_MASK_CASING);
 for(int p=0;p<road_npolys;p++){
  const struct RoadPoly*rp=&road_polys[p];
  if((rp->cls==1)!=motorways_only)continue;
  road_draw_poly_pass(rp,1);
 }
 map_mask_end();

 /* Pass B: carriageway fills over casings (seamless junction mouths). */
 map_mask_begin(1);
 for(int p=0;p<road_npolys;p++){
  const struct RoadPoly*rp=&road_polys[p];
  if((rp->cls==1)!=motorways_only)continue;
  road_draw_poly_pass(rp,0);
 }
 map_mask_end();
}

static void road_pass_end(void){
 road_pass_active=0;
 if(road_thin_open){map_mask_end();road_thin_open=0;}
 if(!road_npolys)return;
 road_pass_solve();
 road_solve_transitions();
 road_draw_group(0);
 if(road_has_motorway)road_draw_group(1);
}

#endif
