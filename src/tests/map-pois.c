/* Independent POI layout, ordering and road-pool pressure regressions. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
int main(void){

 for(int night=0;night<2;night++){
  apply_palette(night);
  unsigned hashes[3];
  for(int kind=6;kind<=8;kind++){
   for(int i=0;i<FB_PIX;i++)fb[i]=0xdead;
   poi_icon(kind,100,100);unsigned h=0;
   for(int y=0;y<H;y++)for(int x=0;x<W;x++){
    if(x<92||x>=108||y<92||y>=108)assert(fb[y*W+x]==0xdead);
    else h=h*33u+fb[y*W+x];
   }
   hashes[kind-6]=h;
  }
  assert(hashes[0]!=hashes[1]&&hashes[1]!=hashes[2]&&hashes[0]!=hashes[2]);
 }
 for(int mask=0;mask<5;mask++){
  int partial=0;for(int i=0;i<256;i++){assert(poi_coverage[mask][i]<=15);partial|=poi_coverage[mask][i]>0&&poi_coverage[mask][i]<15;}assert(partial);
 }
 /* Status geometry must not alter map icon membership or consume a slot. */
 for(int mode=0;mode<2;mode++)for(int status=0;status<3;status++){
  freecam=mode;nlbox=npoi=0;nav_card_x=20;nav_card_y=60;nav_card_w=status?300:0;nav_card_h=status*60;
  for(int i=0;i<4;i++)poi_offer(60+i*70,145,6+i%3,34+i);
  poi_reserve();assert(poi_visible==4);
 }
 nav_card_w=nav_card_h=0;roadctx.valid=0;route_status=R_IDLE;
 for(int mode=0;mode<2;mode++){
  freecam=mode;nlbox=nltxt=npoi=poi_visible=ndrive_labels=0;
  if(mode)poi_offer(200,160,7,64);else drive_label_offer(200,160,7,64,"Parking");
  assert(npoi==1&&!ndrive_labels); /* Parking's real builder priority is 64. */
  for(int i=0;i<90;i++){char name[32];snprintf(name,sizeof name,"Road %d",i);drive_label_offer(100+i%200,140+i%50,3,12,name);}
  assert(npoi==1);if(!mode)assert(ndrive_labels==DRIVE_LABEL_CANDIDATES);poi_reserve();assert(poi_visible==1);
  assert(!label_free(195,155,205,165)); /* Road text cannot evict an icon. */
  memset(fb,0,sizeof display_fb);apply_palette(1);poi_render();
  unsigned painted=0;for(int y=150;y<171;y++)for(int x=190;x<211;x++)painted+=fb[y*W+x]!=0;assert(painted);
  struct PoiLabel result[POI_CANDIDATES];
  for(int reverse=0;reverse<2;reverse++){
   npoi=0;
   for(int k=0;k<60;k++){int i=reverse?59-k:k;poi_offer(30+i*7,80+i*2,6+i%3,34+i%3*15);}
   assert(npoi>0&&npoi<=POI_CANDIDATES);
   if(!reverse)memcpy(result,poi_labels,sizeof result);else assert(!memcmp(result,poi_labels,npoi*sizeof(*poi_labels)));
  }
  npoi=0;poi_offer(200,160,7,64);poi_offer(201,161,7,64);assert(npoi==1);
 }
 puts("POIs: parking priority, independent road/POI capacity, reserved icon space, deterministic tile-order selection, duplicates and both view modes passed.");
}
