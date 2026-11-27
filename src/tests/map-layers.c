/* Exercise the production map renderer: no water/green pixel may cover a road. */
#define HOST_TEST 1
#define main clioplay_preview_main
#ifndef CLIO_SOURCE
#define CLIO_SOURCE "../clio843/src/clio040.c"
#endif
#include CLIO_SOURCE
#undef main
#include <assert.h>
#include <string.h>

static u16 combined[FB_PIX],background[FB_PIX],roads_on_marker[FB_PIX];
static const u16 marker=0xdead;

static void lines_only(struct Tile*t,int z,int roads){
  if(!t||t->len<8)return;
  const u8*p=t->data,*end=p+t->len;int count=rd16(p),span=span_for_level(t->lvl);p+=8;
  for(int i=0;i<count&&p+4<=end;i++){
    int layer=p[0],style=p[1],np=p[2];p+=4;if(p+np*4>end)return;
    if(!(style&128)&&((layer==0)==roads))draw_path(t,span,layer,style,np,&p,end,z,0);
    else p+=np*4;
  }
}
static void visible_lines(int z,int roads){
#ifdef CLIO_MAP_RASTER_H
  if(roads)road_pass_begin(z);
  else map_mask_begin(0);
#endif
  int a,b,c,d;map_visible_tile_bounds(z,z,&a,&b,&c,&d);
  for(int ty=c;ty<=d;ty++)for(int tx=a;tx<=b;tx++)
    if(map_tile_in_pack(z,tx,ty))lines_only(get_tile(z,tx,ty),z,roads);
#ifdef CLIO_MAP_RASTER_H
  if(roads)road_pass_end();
  else map_mask_end();
#endif
}
static void scene(s32 lat,s32 lon,int z,int night,int driving,int heading){
  memzero(tc,sizeof(tc));agec=1;gps.fix=track_valid=display_course_valid=1;
  gps.lat=track_lat=cam_lat=lat;gps.lon=track_lon=cam_lon=lon;
  gps.course=track_course=display_course=heading;
  freecam=!driving;freecam_course_valid=1;freecam_course=0;
  freecam_anim_mode=FC_ANIM_NONE;map_tilt_q8=driving?256:0;
  perspective_lut_tilt=-1;theme_resolved=night;apply_palette(night);
  mapok=1;mapfmt=5;font_ok=0;nav_card_w=nav_card_h=ndrive_labels=0;
  (void)z;
}
static unsigned verify(int z,unsigned*water,unsigned*green){
  draw_map_geometry(z);memcpy(combined,fb,sizeof(fb));
  fill(0,0,W,H,C_MAP_BG);
  int a,b,c,d;map_visible_tile_bounds(z,z,&a,&b,&c,&d);
  for(int ty=c;ty<=d;ty++)for(int tx=a;tx<=b;tx++)
    if(map_tile_in_pack(z,tx,ty))draw_tile_geometry(get_tile(z,tx,ty),z,1);
  visible_lines(z,0);memcpy(background,fb,sizeof(fb));
  /* Compose roads explicitly over the complete background. Minor roads now
   * have soft centres too, so an oracle requiring opaque cores misses them. */
  visible_lines(z,1);memcpy(roads_on_marker,fb,sizeof(fb));
  unsigned hidden=0;*water=*green=0;
  for(int y=MAP_TOP;y<MAP_BOTTOM;y++)for(int x=0;x<W;x++){
    int i=y*W+x;
    if(roads_on_marker[i]==background[i])continue;
    if(background[i]==C_WATER)(*water)++;
    if(background[i]==C_GREEN)(*green)++;
    u16 expected=roads_on_marker[i],actual=combined[i];
    int dr=(int)(actual>>11)-(int)(expected>>11),dg=(int)((actual>>5)&63)-(int)((expected>>5)&63),db=(int)(actual&31)-(int)(expected&31);
    /* Ignore a single RGB565 step when overlapping fringes round differently. */
    if(dr>1||dr<-1||dg>1||dg<-1||db>1||db<-1)hidden++;
  }
  return hidden;
}
static void point(u8*p,int i,int x,int y){
  p[i*4]=x;p[i*4+1]=x>>8;p[i*4+2]=y;p[i*4+3]=y>>8;
}
static void cropped_labels(void){
  static const char visible[]="Long Street Crossing the Entire Crop";
  static const char outside[]="Off-screen Street";
  struct Tile tile={.lvl=6,.data={2,0,0,0,2,0,0,0,0,6,2,0}};
  point(tile.data+12,0,128,2192);point(tile.data+12,1,3968,2192);
  tile.data[21]=6;tile.data[22]=2;tile.data[23]=1;
  point(tile.data+24,0,128,3968);point(tile.data+24,1,3968,3968);
  tile.data[36]=sizeof(visible)-1;
  memcpy(tile.data+37,visible,sizeof(visible)-1);
  int at=37+sizeof(visible)-1;
  tile.data[at+4]=sizeof(outside)-1;
  memcpy(tile.data+at+5,outside,sizeof(outside)-1);
  tile.len=at+5+sizeof(outside)-1;

  for(int driving=0;driving<=1;driving++){
    scene(128,65,9,0,driving,0);
    perspective_scale_update();
    drive_label_prepare(9);
    fill(0,0,W,H,C_MAP_BG);
    road_pass_begin(9);
    draw_tile_geometry(&tile,9,MAP_DRAW_ROADS);
    road_pass_end();
    assert(ndrive_labels==1&&streq(drive_labels[0].name,visible));
    assert(drive_labels[0].x>200&&drive_labels[0].x<280);
    nltxt=nlbox=0;
    drive_label_render(9);
    assert(nltxt==1&&label_text_seen(visible));
  }
  /* A route passing near the opposite corner of a box is not an overlap. */
  drive_route_segs[0]=(struct DriveRouteSeg){0,0,300,200};ndrive_route_segs=1;
  assert(!drive_label_hits_route(150,30,200,45,1));
  assert(drive_label_hits_route(150,90,200,105,1));
  puts("Cropped labels: road crossing an off-screen anchor, visible placement, long-name dedup and precise route clearance.");
}
static unsigned synthetic(void){
  FILE*f=tmpfile();assert(f);mapfd=fileno(f);
  memzero(&mh,sizeof(mh));memcpy(mh.magic,"CPM5",4);mh.ver=5;mh.maxlvl=6;
  mh.flags=2;mh.count=2;mh.index_off=sizeof(mh);mh.data_off=sizeof(mh)+2*sizeof(struct CpmIndex);
  mh.minlat=21484*256;mh.maxlat=21485*256;mh.minlon=-625*256;mh.maxlon=-623*256;
  u8 road[20]={1,0,0,0,0,0,0,0,0,6,2,255};
  point(road+12,0,3000,2048);point(road+12,1,4095,2048);
  u8 water[44]={2,0,0,0,0,0,0,0,1,132,5,255};
  point(water+12,0,0,900);point(water+12,1,3000,900);point(water+12,2,3000,3200);
  point(water+12,3,0,3200);point(water+12,4,0,900);
  water[32]=1;water[33]=1;water[34]=2;water[35]=255;
  point(water+36,0,0,900);point(water+36,1,0,3200);
  struct CpmIndex ix[2]={{.lvl=6,.tx=-625,.ty=21484,.off=0,.len=sizeof(road),.features=1},
                         {.lvl=6,.tx=-624,.ty=21484,.off=sizeof(road),.len=sizeof(water),.features=2}};
  assert(fwrite(&mh,1,sizeof(mh),f)==sizeof(mh));
  assert(fwrite(ix,1,sizeof(ix),f)==sizeof(ix));
  assert(fwrite(road,1,sizeof(road),f)==sizeof(road));
  assert(fwrite(water,1,sizeof(water),f)==sizeof(water));fflush(f);
  scene(21484*256+128,-624*256,6,0,0,0);
  unsigned w,g,hidden=verify(6,&w,&g);
  printf("adjacent-tile road/water-stroke overlap: hidden=%u water_overlap=%u\n",hidden,w);
  assert(w>0);fclose(f);mapfd=-1;return hidden;
}
static void parallel_road_expansion(void){
  /* Wide Q4 strokes are endpoint-order symmetric across thin, medium and wide widths. */
  static u16 ref_fb[FB_PIX];
  const int widths_q4[]={24,96,296,568};
  for(unsigned k=0;k<sizeof(widths_q4)/sizeof(widths_q4[0]);k++){
    apply_palette(0);fill(0,0,W,H,C_MAP_BG);
    map_mask_begin(1);map_mask_select(3);
    map_line_q8_w4(70*256+43,60*256+19,210*256+111,170*256+203,road_colours[3],widths_q4[k]);
    map_mask_end();memcpy(ref_fb,fb,sizeof(fb));
    fill(0,0,W,H,C_MAP_BG);
    map_mask_begin(1);map_mask_select(3);
    map_line_q8_w4(210*256+111,170*256+203,70*256+43,60*256+19,road_colours[3],widths_q4[k]);
    map_mask_end();
    assert(!memcmp(ref_fb,fb,sizeof(fb)));
  }

  /* Priority expansion & outward displacement: a minor road tightly parallel
   * to a main trunk road must stay distinct (canvas gap between outer kerbs)
   * while expanding outwards on its open side to retain generous width. */
  for(int night=0;night<2;night++)for(int z=7;z<=9;z++){
    scene(128,65,z,night,0,0);
    fill(0,0,W,H,C_MAP_BG);
    int ys[2]={20*256,250*256};
    int xm[2]={180*256,180*256},xr[2]={192*256,192*256};
    int cx[2]={80*256,300*256},cy[2]={135*256,135*256};
    road_pass_begin(z);
    road_pass_add(xm,ys,2,2); /* Trunk */
    road_pass_add(xr,ys,2,6); /* Parallel minor road 12 px east */
    road_pass_add(cx,cy,2,4); /* Perpendicular secondary cross-street */
    road_pass_end();
    assert(road_npolys==3);
    /* Pick mid-polyline vertex away from cross-street (row y=70). */
    const struct RoadVert*vm=&road_verts[road_polys[0].first+road_polys[0].count/2];
    const struct RoadVert*vr=&road_verts[road_polys[1].first+road_polys[1].count/2];
    const struct RoadVert*vc=&road_verts[road_polys[2].first+road_polys[2].count/2];
    /* Main road expands wider than minor road; minor road shifts outwards (+X). */
    assert(vm->eff_w4>=vr->eff_w4);
    if(z>=8)assert(vr->draw_x>vr->x);
    /* Minor road retains substantial width via outward expansion instead of collapsing. */
    assert(vr->eff_w4>=vr->want_h4);
    /* Perpendicular cross-street is not displaced vertically. */
    assert(vc->draw_y==vc->y);
    /* Verify pixel-level separation at y=70: trunk fill -> canvas gap -> minor fill. */
    int main_x=vm->draw_x>>8,minor_x=vr->draw_x>>8;
    assert(main_x<minor_x);
    assert(fb[70*W+main_x]==road_colours[4]);
    assert(fb[70*W+minor_x]==road_colours[0]);
    int saw_gap=0;
    for(int x=main_x+1;x<minor_x;x++)if(fb[70*W+x]==C_MAP_BG){saw_gap=1;break;}
    assert(saw_gap);
  }

  /* Equal-priority dual carriageway (two parallel Primary roads 16 px apart at Z8)
   * splits symmetrically and preserves a clean central median. */
  scene(128,65,8,0,0,0);
  fill(0,0,W,H,C_MAP_BG);
  int ys[2]={20*256,250*256},xa[2]={200*256,200*256},xb[2]={216*256,216*256};
  road_pass_begin(8);
  road_pass_add(xa,ys,2,3);
  road_pass_add(xb,ys,2,3);
  road_pass_end();
  assert(fb[100*W+208]==C_MAP_BG);
  assert(fb[100*W+(road_verts[road_polys[0].first].draw_x>>8)]==road_colours[3]);
  assert(fb[100*W+(road_verts[road_polys[1].first].draw_x>>8)]==road_colours[3]);
  puts("Cartographic road layout: wide Q4 symmetry, priority main-first expansion, outward minor-road displacement, cross-street neutrality and dual-carriageway medians passed.");
}
static int column_non_bg_span(int x,int y0,int y1){
  int lo=-1,hi=-1;
  for(int y=y0;y<=y1;y++)if(fb[y*W+x]!=C_MAP_BG){if(lo<0)lo=y;hi=y;}
  return lo<0?0:(hi-lo+1);
}
static void road_end_to_end_transitions(void){
  static u16 ref_fb[FB_PIX];
  /* 1. Tapered capsule rasterizer is bit-exact endpoint-order symmetric across
   * widths, diagonals, butt caps, padded seams and axial alpha ramps. */
  const struct {int x0,y0,x1,y1,w0,w1,a0,a1,caps;} cases[]={
    {60*256+17,110*256+33,220*256+91,110*256+33,448,160,32,32,3},
    {150*256+40,40*256+11,150*256+40,210*256+199,512,200,32,0,1|8},
    {80*256+77,60*256+15,240*256+211,185*256+43,360,120,24,8,2|4},
    {90*256+5,190*256+88,260*256+140,70*256+200,280,520,32,32,0},
    {120*256,120*256,190*256,145*256,568,264,32,32,1},
  };
  for(unsigned k=0;k<sizeof(cases)/sizeof(cases[0]);k++){
    int swap_caps=((cases[k].caps&1)<<1)|((cases[k].caps&2)>>1)|((cases[k].caps&4)<<1)|((cases[k].caps&8)>>1);
    apply_palette(1);fill(0,0,W,H,C_MAP_BG);
    map_mask_begin(1);map_mask_select(4);
    map_line_q8_taper(cases[k].x0,cases[k].y0,cases[k].x1,cases[k].y1,road_colours[4],
                      cases[k].w0,cases[k].w1,cases[k].a0,cases[k].a1,cases[k].caps);
    map_mask_end();memcpy(ref_fb,fb,sizeof(fb));
    fill(0,0,W,H,C_MAP_BG);
    map_mask_begin(1);map_mask_select(4);
    map_line_q8_taper(cases[k].x1,cases[k].y1,cases[k].x0,cases[k].y0,road_colours[4],
                      cases[k].w1,cases[k].w0,cases[k].a1,cases[k].a0,swap_caps);
    map_mask_end();
    assert(!memcmp(ref_fb,fb,sizeof(fb)));
  }

  /* 2. Primary (cls=3) -> Local (cls=6) end-to-end join smoothly tapers width
   * monotonically and blends colour across the funnel with zero bulbous step. */
  for(int night=0;night<2;night++)for(int z=8;z<=9;z++){
    scene(128,65,z,night,0,0);fill(0,0,W,H,C_MAP_BG);
    int x1[2]={40*256,220*256},y1[2]={120*256,120*256};
    int x2[2]={220*256,380*256},y2[2]={120*256,120*256};
    road_pass_begin(z);
    road_pass_add(x1,y1,2,3);
    road_pass_add(x2,y2,2,6);
    road_pass_end();
    assert(road_npolys==2);
    assert(road_polys[0].taper[1].tgt_w4>0);
    assert(road_polys[0].taper[1].tgt_slot==0);
    assert(road_polys[1].taper[0].tgt_w4==0&&road_polys[1].taper[1].tgt_w4==0);
    int w_main=column_non_bg_span(100,60,180);
    int w_mid=column_non_bg_span(220-(int)road_polys[0].taper[1].len_px/2,60,180);
    int w_join=column_non_bg_span(220,60,180);
    int w_minor=column_non_bg_span(300,60,180);
    assert(w_main>w_mid&&w_mid>w_join);
    assert(w_join<=w_minor+1&&w_join>=w_minor-1);
    /* Colour transitions smoothly from Primary to Local with intermediate blend pixels. */
    assert(fb[120*W+100]==road_colours[3]);
    assert(fb[120*W+220]==road_colours[0]);
    int saw_blend=0,tlen=(int)road_polys[0].taper[1].len_px;
    for(int x=220-tlen+1;x<220;x++){
      u16 px=fb[120*W+x];
      if(px!=road_colours[3]&&px!=road_colours[0]){saw_blend=1;break;}
    }
    assert(saw_blend);
  }

  /* 3. Motorway (cls=1) -> Local (cls=6) end-to-end join smoothly tapers across
   * render groups and never slices a dark motorway casing bar across the local road. */
  for(int night=0;night<2;night++){
    scene(128,65,9,night,0,0);fill(0,0,W,H,C_MAP_BG);
    int x1[2]={40*256,220*256},y1[2]={130*256,130*256};
    int x2[2]={220*256,380*256},y2[2]={130*256,130*256};
    road_pass_begin(9);
    road_pass_add(x1,y1,2,1);
    road_pass_add(x2,y2,2,6);
    road_pass_end();
    assert((road_polys[0].taper[1].flags&1)!=0);
    assert(fb[130*W+100]==road_colours[5]);
    /* At and past the join, centerline must stay pure Local fill (zero casing bar). */
    for(int x=220;x<=260;x++)assert(fb[130*W+x]==road_colours[0]);
    int w_join=column_non_bg_span(220,70,190),w_minor=column_non_bg_span(300,70,190);
    assert(w_join<=w_minor+1&&w_join>=w_minor-1);
  }

  /* 4. Point-to-segment detection (wide Primary dead-ending into simplified Local),
   * plus negative checks: cul-de-sac, closed roundabout seam, continuous T-junction,
   * and motorway flyover must not spuriously taper. */
  scene(128,65,8,1,0,0);fill(0,0,W,H,C_MAP_BG);
  road_pass_begin(8);
  /* Poly 0 & 1: wide Primary dead-ending onto midpoint of long Local street */
  {int x1[2]={40*256,120*256},y1[2]={80*256,80*256};
   int x2[2]={120*256,120*256},y2[2]={30*256,130*256};
   road_pass_add(x1,y1,2,3);road_pass_add(x2,y2,2,6);}
  /* Poly 2: unconnected Primary cul-de-sac */
  {int x1[2]={40*256,120*256},y1[2]={180*256,180*256};
   road_pass_add(x1,y1,2,3);}
  /* Poly 3 & 4: closed Primary roundabout loop + Local exit right at seam vertex 0 */
  {int rx[5]={240*256,280*256,280*256,240*256,240*256};
   int ry[5]={60*256,60*256,100*256,100*256,60*256};
   int lx[2]={240*256,200*256},ly[2]={60*256,60*256};
   road_pass_add(rx,ry,5,3);road_pass_add(lx,ly,2,6);}
  /* Poly 5, 6, 7: continuous Primary T-junction with minor side road */
  {int xa[2]={200*256,270*256},xb[2]={270*256,340*256},yh[2]={180*256,180*256};
   int xc[2]={270*256,270*256},yc[2]={180*256,230*256};
   road_pass_add(xa,yh,2,3);road_pass_add(xb,yh,2,3);road_pass_add(xc,yc,2,6);}
  /* Poly 8 & 9: Motorway flyover crossing over Primary */
  {int xm[2]={420*256,420*256},ym[2]={30*256,220*256};
   int xp[2]={380*256,460*256},yp[2]={120*256,120*256};
   road_pass_add(xm,ym,2,1);road_pass_add(xp,yp,2,3);}
  road_pass_end();
  assert(road_npolys==10);
  assert(road_polys[0].taper[1].tgt_w4>0);  /* dead-end into narrow Local tapers */
  assert(road_polys[2].taper[0].tgt_w4==0&&road_polys[2].taper[1].tgt_w4==0); /* cul-de-sac */
  assert(road_polys[3].taper[0].tgt_w4==0&&road_polys[3].taper[1].tgt_w4==0); /* roundabout loop */
  assert(road_polys[5].taper[1].tgt_w4==0&&road_polys[6].taper[0].tgt_w4==0); /* continuous T-junction */
  assert(road_polys[8].taper[0].tgt_w4==0&&road_polys[8].taper[1].tgt_w4==0); /* motorway flyover */
  assert(fb[120*W+420]==road_colours[5]);   /* motorway remains elevated over primary */
  puts("Cartographic road transitions: tapered-capsule symmetry, S-curve width/colour funnels, cross-group motorway joins and topology guards passed.");
}
static void label_hysteresis_and_anchoring(void){
  /* 1. Competition hysteresis & re-entry debounce on Z6 (capped at 6 labels). */
  hud_capture_clock=1;hud_capture_ms=20000;
  scene(1000,1000,6,1,1,0);label_track_reset();
  const char*names[7]={"Alpha Rd","Bravo Rd","Charlie Rd","Delta Rd","Echo Rd","Foxtrot Rd","Golf Rd"};
  const int xs[7]={90,190,300,390,110,360,240};
  const int ys[7]={130,130,130,130,175,175,92};
  drive_label_prepare(6);nlbox=nltxt=0;
  for(int i=0;i<7;i++)drive_label_offer_geo(xs[i],ys[i],1000+i*10,1000+i*10,1,2,48,names[i]);
  drive_label_render(6);
  assert(nltxt==DRIVE_ROAD_LABELS);
  assert(label_text_seen("Alpha Rd")&&!label_text_seen("Golf Rd"));

  /* Frame 2 (+100 ms): Golf Rd moves right beside the car sweet spot (better raw
   * score than Alpha Rd), yet active hysteresis preserves Alpha Rd with zero flicker. */
  hud_capture_ms+=100;
  drive_label_prepare(6);nlbox=nltxt=0;
  for(int i=0;i<6;i++)drive_label_offer_geo(xs[i],ys[i],1000+i*10,1000+i*10,1,2,48,names[i]);
  drive_label_offer_geo(240,165,1060,1060,1,2,48,"Golf Rd");
  drive_label_render(6);
  assert(nltxt==DRIVE_ROAD_LABELS);
  assert(label_text_seen("Alpha Rd")&&!label_text_seen("Golf Rd"));

  /* Frame 3 (+100 ms): Alpha Rd exits screen -> Golf Rd cleanly takes the vacancy. */
  hud_capture_ms+=100;
  drive_label_prepare(6);nlbox=nltxt=0;
  for(int i=1;i<6;i++)drive_label_offer_geo(xs[i],ys[i],1000+i*10,1000+i*10,1,2,48,names[i]);
  drive_label_offer_geo(240,165,1060,1060,1,2,48,"Golf Rd");
  drive_label_render(6);
  assert(label_text_seen("Golf Rd")&&!label_text_seen("Alpha Rd"));

  /* Frame 4 (+200 ms, within cooldown): Alpha Rd jitters back inside -> debounced! */
  hud_capture_ms+=200;
  drive_label_prepare(6);nlbox=nltxt=0;
  drive_label_offer_geo(90,130,1000,1000,1,2,48,"Alpha Rd");
  drive_label_render(6);
  assert(!label_text_seen("Alpha Rd"));

  /* Frame 5 (+1300 ms, past cooldown): Alpha Rd is cleanly eligible again. */
  hud_capture_ms+=1300;
  drive_label_prepare(6);nlbox=nltxt=0;
  drive_label_offer_geo(90,130,1000,1000,1,2,48,"Alpha Rd");
  drive_label_render(6);
  assert(label_text_seen("Alpha Rd"));

  /* 2. Same-road multi-point anchor lock on Z6: once locked to Point A, Point B
   * having a better raw score must not teleport the road name along the road. */
  label_track_reset();hud_capture_ms+=100;
  drive_label_prepare(6);nlbox=nltxt=0;
  drive_label_offer_geo(200,175,1100,1200,1,2,48,"High Street"); /* Point A (wins) */
  drive_label_offer_geo(360,115,1500,1600,1,2,48,"High Street"); /* Point B */
  drive_label_render(6);
  struct LabelTrack*tr=label_track_find("High Street");
  assert(tr&&tr->active&&tr->lat4==1100*16&&tr->lon4==1200*16);

  hud_capture_ms+=100;
  drive_label_prepare(6);nlbox=nltxt=0;
  drive_label_offer_geo(140,194,1100,1200,1,2,48,"High Street"); /* Point A drifted */
  drive_label_offer_geo(240,175,1500,1600,1,2,48,"High Street"); /* Point B closer to sweet spot */
  drive_label_render(6);
  tr=label_track_find("High Street");
  assert(tr&&tr->active&&tr->lat4==1100*16&&tr->lon4==1200*16&&tr->last_x==140&&tr->last_y==194);

  /* 3. Close-zoom (Z8) world-anchored geometry locking: as the car drives along
   * a road with heading jitter, the locked Q4 world anchor and side remain fixed. */
  static const char king[]="King Street";
  struct Tile tile={.lvl=6,.data={1,0,0,0,1,0,0,0,0,6,2,0}};
  point(tile.data+12,0,128,2200);point(tile.data+12,1,3968,2200);
  tile.data[24]=sizeof(king)-1;
  memcpy(tile.data+25,king,sizeof(king)-1);
  tile.len=25+sizeof(king)-1;

  scene(128,128,8,1,1,0);track_lat4=128*16;track_lon4=128*16;
  label_track_reset();hud_capture_ms+=100;
  perspective_scale_update();drive_label_prepare(8);
  fill(0,0,W,H,C_MAP_BG);road_pass_begin(8);draw_tile_geometry(&tile,8,MAP_DRAW_ROADS);road_pass_end();
  nlbox=nltxt=0;drive_label_render(8);
  tr=label_track_find(king);
  assert(tr&&tr->active&&tr->has_geo&&tr->cropped);
  s32 locked_lat4=tr->lat4,locked_lon4=tr->lon4;
  signed char locked_side=tr->side;
  u8 locked_vert=tr->vertical;
  for(int step=1;step<=4;step++){
    hud_capture_ms+=100;
    gps.lat=track_lat=cam_lat=128+step*2;track_lat4=gps.lat*16;
    gps.course=track_course=display_course=(step&1)?2:358;
    perspective_scale_update();drive_label_prepare(8);
    fill(0,0,W,H,C_MAP_BG);road_pass_begin(8);draw_tile_geometry(&tile,8,MAP_DRAW_ROADS);road_pass_end();
    nlbox=nltxt=0;drive_label_render(8);
    tr=label_track_find(king);
    assert(tr&&tr->active&&tr->lat4==locked_lat4&&tr->lon4==locked_lon4);
    assert(tr->side==locked_side&&tr->vertical==locked_vert);
  }

  /* 4. Context reset: zoom change cleanly clears stale tracks. */
  hud_capture_ms+=100;
  drive_label_prepare(7);
  assert(nlabel_tracks==0);
  hud_capture_clock=0;
  puts("Road label stability: competition hysteresis, re-entry debounce, multi-point anchor locking, close-zoom Q4 world anchoring and context reset passed.");
}
int main(int argc,char**argv){
  /* Style lookup and packed coverage must agree in both palettes at all zooms.
   * Widths increase monotonically across road classes and zoom tiers. */
  for(int night=0;night<2;night++){
    int prev_zoom_w4[8]={0};
    for(int z=0;z<=9;z++){
      apply_palette(night);map_mask_begin(1);
      int previous=0,prev_q4=0;
      for(int cls=7;cls>=1;cls--){
        u16 colour;int width,w4,c4;
        road_style(cls,z,&colour,&width);
        road_style_q4(cls,z,&colour,&w4,&c4);
        assert(colour==map_coverage_cols[cls<=5?6-cls:0]);
        assert(width>=previous&&width<=36);
        assert(w4>=prev_q4&&w4>=prev_zoom_w4[cls]);
        previous=width;prev_q4=w4;prev_zoom_w4[cls]=w4;
      }
      map_mask_end();
      map_mask_begin(MAP_MASK_CASING);
      for(int slot=0;slot<6;slot++)assert(map_coverage_cols[slot]==road_casing_colours[slot]);
      map_mask_end();
    }
  }
  parallel_road_expansion();
  road_end_to_end_transitions();
  label_hysteresis_and_anchoring();
  cropped_labels();
  unsigned failures=synthetic();
  if(argc>1){
    mapfd=open(argv[1],O_RDONLY);assert(mapfd>=0);
    assert(read(mapfd,&mh,sizeof(mh))==sizeof(mh));assert(!memcmp(mh.magic,"CPM5",4));
    const struct {const char*name;s32 lat,lon;} sites[]={
      {"Tyne Bridge",5496830,-160580},{"Waterloo Bridge",5150810,-11650},
      {"Tower Bridge",5150550,-7540}};
    for(int s=0;s<3;s++){
      unsigned total_water=0,total_green=0,total_hidden=0;
      for(int z=4;z<=6;z++)for(int night=0;night<2;night++)
        for(int drive=0;drive<2;drive++)for(int h=0;h<2;h++){
          scene(sites[s].lat,sites[s].lon,z,night,drive,h?70:0);
          unsigned w,g,hidden=verify(z,&w,&g);total_water+=w;total_green+=g;total_hidden+=hidden;
        }
      printf("%s: 24 views, road-over-water pixels=%u road-over-green pixels=%u hidden=%u\n",
             sites[s].name,total_water,total_green,total_hidden);
      if(!total_water||total_hidden)failures++;
    }
    close(mapfd);
  }
  return failures?1:0;
}
