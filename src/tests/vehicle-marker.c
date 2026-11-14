/* Directional sprite coverage, theme contrast and clipped framebuffer writes. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static unsigned hash(void){unsigned h=0;for(int i=0;i<FB_PIX;i++)h=h*33u+fb[i];return h;}
int main(void){
 map_tilt_q8=0;
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
 /* Foreshorten in screen depth, regardless of bearing. Preserve Explore. */
 int last_height=100;
 for(int tilt=0;tilt<=256;tilt+=16){
  map_tilt_q8=tilt;fill(0,0,W,H,0xdead);vehicle_icon(100,100,0);
  int top=H,bottom=0;
  for(int y=80;y<120;y++)for(int x=80;x<120;x++)if(fb[y*W+x]!=0xdead){if(y<top)top=y;if(y>bottom)bottom=y;}
  int height=bottom-top+1;assert(height<=last_height);last_height=height;
 }
 assert(last_height>=13&&last_height<=17);
 map_tilt_q8=-1;assert(vehicle_scale_q8()==256);
 map_tilt_q8=999;assert(vehicle_scale_q8()==156);
 /* Follow the SAME tilt trajectory in both directions; mode flag must not snap it. */
 gps.fix=track_valid=display_course_valid=1;display_course=track_course=gps.course=135;
 freecam=0;freecam_anim_mode=FC_ANIM_NONE;map_tilt_q8=256;
 hud_capture_clock=1;hud_capture_ms=10000;enter_freecam();assert(freecam&&vehicle_scale_q8()==156);
 int scale=156;
 for(int at=0;at<=500;at+=10){freecam_orientation_anim_update(10000+at);int next=vehicle_scale_q8();assert(next>=scale);scale=next;}
 assert(scale==256);
 hud_capture_ms=10500;leave_freecam();assert(!freecam&&vehicle_scale_q8()==256);
 for(int at=0;at<=500;at+=10){freecam_orientation_anim_update(10500+at);int next=vehicle_scale_q8();assert(next<=scale);scale=next;}
 assert(scale==156);
 hud_capture_ms=11000;enter_freecam();hud_capture_ms=11200;freecam_orientation_anim_update(hud_capture_ms);
 scale=vehicle_scale_q8();assert(scale>156&&scale<256);leave_freecam();assert(vehicle_scale_q8()==scale);
 puts("Vehicle marker: headings/themes, AA, clipping, no-GPS hiding, depth compression, entry/exit interpolation and interrupted reversal passed.");
}
