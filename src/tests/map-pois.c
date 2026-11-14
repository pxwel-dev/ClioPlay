/* Independent POI layout, ordering and road-pool pressure regressions. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
int main(void){
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
