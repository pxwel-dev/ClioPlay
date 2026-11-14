/* Directional sprite coverage, theme contrast and clipped framebuffer writes. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static unsigned hash(void){unsigned h=0;for(int i=0;i<FB_PIX;i++)h=h*33u+fb[i];return h;}
int main(void){
 unsigned theme_hash[2];
 for(int night=0;night<2;night++){
  apply_palette(night);
  for(int heading=0;heading<360;heading+=10){
   fill(0,0,W,H,C_BG);vehicle_icon(100,100,heading);unsigned h=hash();
   int partial=0,painted=0;
   for(int i=0;i<VEHICLE_ICON_SIZE*VEHICLE_ICON_SIZE;i++){
    unsigned m=vehicle_coverage[heading/10][i];
    for(int layer=0;layer<4;layer++){unsigned a=(m>>(layer*4))&15;partial|=a>0&&a<15;}
   }
   for(int y=0;y<H;y++)for(int x=0;x<W;x++){
    if(x<86||x>=114||y<86||y>=114)assert(fb[y*W+x]==C_BG);
    else painted+=fb[y*W+x]!=C_BG;
   }
   assert(partial&&painted>100&&painted<400);
   fill(0,0,W,H,C_BG);vehicle_icon(100,100,heading+720);assert(hash()==h);
   fill(0,0,W,H,C_BG);vehicle_icon(100,100,heading-720);assert(hash()==h);
  }
  fill(0,0,W,H,C_BG);vehicle_icon(100,100,0);theme_hash[night]=hash();
  /* Whole sprite outside each edge must be a no-op; corner clipping is safe. */
  const int positions[][2]={{-28,100},{W+28,100},{100,MAP_TOP-28},{100,MAP_BOTTOM+28}};
  fill(0,0,W,H,0xdead);unsigned blank=hash();
  for(int i=0;i<4;i++){vehicle_icon(positions[i][0],positions[i][1],45);assert(hash()==blank);}
  for(int h=0;h<360;h+=10){vehicle_icon(0,MAP_TOP,h);vehicle_icon(W-1,MAP_BOTTOM-1,h);}
 }
 assert(theme_hash[0]!=theme_hash[1]);
 gps.fix=0;fill(0,0,W,H,0xdead);unsigned blank=hash();draw_car_marker(5);assert(hash()==blank);
 /* The north-up tip is above its lower, notched base. */
 const u16*m=vehicle_coverage[0];assert(m[3*28+14]);assert(!m[25*28+14]);
 puts("Vehicle marker: all headings/themes, partial edge coverage, clipped writes, wrapped headings and no-GPS hiding passed.");
}
