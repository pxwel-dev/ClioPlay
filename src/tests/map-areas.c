/* Host regression fixture for existing-CPM closed-outline fills. */
#define HOST_TEST 1
#define main clioplay_preview_main
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>

static struct Tile tile;
static const u16 untouched=0xdead;
static void reset_scene(void){
  memzero(&tile,sizeof(tile));tile.lvl=6;tile.tx=-625;tile.ty=21484;
  mh.flags=0;
  freecam=1;cam_lat=(s32)tile.ty*256+128;cam_lon=(s32)tile.tx*256+128;
  freecam_course_valid=1;freecam_course=0;map_tilt_q8=0;
  for(int i=0;i<FB_PIX;i++)fb[i]=untouched;
}
static void point(u8*data,int i,int x,int y){
  data[i*4]=(u8)x;data[i*4+1]=(u8)(x>>8);
  data[i*4+2]=(u8)y;data[i*4+3]=(u8)(y>>8);
}
static u16 sample(int x,int y){
  int sx,sy;world_to_screen((s32)tile.ty*256+y*256/4095,
                           (s32)tile.tx*256+x*256/4095,6,&sx,&sy);
  assert(sx>=0&&sx<W&&sy>=40&&sy<MAP_STRIP_Y);
  return fb[sy*W+sx];
}
static void chrome_intact(void){
  for(int y=0;y<H;y++)if(y<40||y>=MAP_STRIP_Y)
    for(int x=0;x<W;x++)assert(fb[y*W+x]==untouched);
}
int main(void){
  u8 ring[255*4];
  reset_scene();
  point(ring,0,900,900);point(ring,1,3000,900);point(ring,2,3000,3000);
  point(ring,3,900,3000);point(ring,4,900,900);
  draw_closed_area(&tile,256,1,4,5,ring,6);
  assert(sample(2000,2000)==C_WATER);assert(sample(500,2000)==untouched);chrome_intact();
  reset_scene();draw_closed_area(&tile,256,2,1,5,ring,6);
  assert(sample(2000,2000)==C_GREEN);chrome_intact();
  /* Open fragments, even an almost-complete rectangle, must remain unfilled. */
  reset_scene();draw_closed_area(&tile,256,2,1,4,ring,6);
  assert(sample(2000,2000)==untouched);
  /* Closed waterway centre lines and roads are not lake/green polygons. */
  draw_closed_area(&tile,256,1,1,5,ring,6);
  draw_closed_area(&tile,256,0,4,5,ring,6);
  draw_closed_area(&tile,256,3,1,5,ring,6);
  assert(sample(2000,2000)==untouched);
  /* A concave outline must preserve its notch. */
  point(ring,0,900,900);point(ring,1,3000,900);point(ring,2,3000,1900);
  point(ring,3,1900,1900);point(ring,4,1900,3000);point(ring,5,900,3000);
  point(ring,6,900,900);
  reset_scene();draw_closed_area(&tile,256,2,2,7,ring,6);
  assert(sample(1400,2500)==C_GREEN);assert(sample(2500,1400)==C_GREEN);
  assert(sample(2500,2500)==untouched);chrome_intact();
  /* New explicit area contours preserve holes, even across doubled bridges. */
  point(ring,0,900,900);point(ring,1,3000,900);point(ring,2,3000,3000);
  point(ring,3,900,3000);point(ring,4,900,900);
  point(ring,5,1600,1600);point(ring,6,2300,1600);point(ring,7,2300,2300);
  point(ring,8,1600,2300);point(ring,9,1600,1600);point(ring,10,900,900);
  reset_scene();mh.flags=2;draw_closed_area(&tile,256,1,132,11,ring,6);
  assert(sample(1200,1200)==C_WATER);assert(sample(2000,2000)==untouched);chrome_intact();
  reset_scene();mh.flags=2;draw_closed_area(&tile,256,1,4,5,ring,6);
  assert(sample(1200,1200)==untouched);
  /* Maximal CPM path length exercises the bounded edge/intersection arrays. */
  for(int i=0;i<254;i++){
    int d=i*8400/254,x,y;
    if(d<2100){x=900+d;y=900;}
    else if(d<4200){x=3000;y=900+d-2100;}
    else if(d<6300){x=3000-(d-4200);y=3000;}
    else{x=900;y=3000-(d-6300);}
    point(ring,i,x,y);
  }
  point(ring,254,900,900);
  reset_scene();draw_closed_area(&tile,256,2,3,255,ring,6);
  assert(sample(2000,2000)==C_GREEN);chrome_intact();
  /* Fully offscreen shapes and truncated tile records cannot touch chrome. */
  reset_scene();cam_lat+=10000;draw_closed_area(&tile,256,2,3,255,ring,6);
  for(int i=0;i<FB_PIX;i++)assert(fb[i]==untouched);
  reset_scene();tile.len=12;tile.data[0]=1;tile.data[8]=2;
  tile.data[9]=1;tile.data[10]=255;mapfmt=4;
  draw_tile_geometry(&tile,6,1);
  for(int i=0;i<FB_PIX;i++)assert(fb[i]==untouched);
  puts("closed water/green, explicit polygons and holes, open-fragment exclusions, concavity, 255-point paths, clipping and truncated tiles: OK");
  return 0;
}
