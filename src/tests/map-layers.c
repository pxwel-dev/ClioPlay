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
  map_mask_begin(roads?1:0);
#endif
  int a,b,c,d;map_visible_tile_bounds(z,z,&a,&b,&c,&d);
  for(int ty=c;ty<=d;ty++)for(int tx=a;tx<=b;tx++)
    if(map_tile_in_pack(z,tx,ty))lines_only(get_tile(z,tx,ty),z,roads);
#ifdef CLIO_MAP_RASTER_H
  map_mask_end();
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
    map_mask_begin(1);
    draw_tile_geometry(&tile,9,MAP_DRAW_ROADS);
    map_mask_end();
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
int main(int argc,char**argv){
  /* Style lookup and packed coverage must agree in both palettes at all zooms.
   * This also covers service roads sharing the local colour plane. */
  for(int night=0;night<2;night++)for(int z=0;z<=9;z++){
    apply_palette(night);map_mask_begin(1);
    int previous=0;
    for(int cls=7;cls>=1;cls--){
      u16 colour;int width;road_style(cls,z,&colour,&width);
      assert(colour==map_coverage_cols[cls<=5?6-cls:0]);
      assert(width>=previous&&width<=5);previous=width;
    }
    map_mask_end();
  }
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
