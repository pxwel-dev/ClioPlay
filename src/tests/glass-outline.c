/* Compare the production alert rim with an independently integrated 2px
 * rounded-rectangle ring, including its partially covered corner pixels. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <math.h>
static int inside(double x,double y,double w,double h,double r){
 if(x<0||y<0||x>=w||y>=h)return 0;
 double dx=x<r?x-r:x>w-r?x-(w-r):0,dy=y<r?y-r:y>h-r?y-(h-r):0;
 return dx*dx+dy*dy<=r*r;
}
int main(void){
 const int x0=30,y0=40,w=128,h=43;
 fill(0,0,W,H,0);hud_glass_background_changed();
 hud_glass_surface_rim((struct HudBox){x0,y0,w,h,256},0,0xffff,16,256,1);
 double worst=0,error=0;int count=0;
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){
  unsigned covered=0;
  for(int sy=0;sy<16;sy++)for(int sx=0;sx<16;sx++){
   double px=x+(sx+.5)/16,py=y+(sy+.5)/16;
   covered+=inside(px,py,w,h,10)&&!inside(px-2,py-2,w-4,h-4,8);
  }
  double expected=covered/256.0,actual=(fb[(y0+y)*W+x0+x]>>11)/31.0/(15.0/16.0);
  double d=fabs(expected-actual);if(d>worst)worst=d;error+=d*d;count++;
  assert(fb[(y0+y)*W+x0+x]==fb[(y0+y)*W+x0+w-1-x]||fabs(actual-(fb[(y0+y)*W+x0+w-1-x]>>11)/31.0/(15.0/16.0))<.04);
 }
 assert(worst<.18&&sqrt(error/count)<.025);
 /* Straight parts occupy exactly two pixels, with a clear glass interior. */
 assert(fb[(y0+20)*W+x0]&&fb[(y0+20)*W+x0+1]&&!fb[(y0+20)*W+x0+2]);
 assert(fb[y0*W+x0+60]&&fb[(y0+1)*W+x0+60]&&!fb[(y0+2)*W+x0+60]);
 /* The inset keeps the same occupied bounds, irrespective of rim thickness. */
 struct HudBox outer={8,221,128,43,256};
 struct HudCard speed={0};copystr(speed.title,sizeof(speed.title),"8");copystr(speed.detail,sizeof(speed.detail),"30");
 for(int night=0;night<2;night++){
  apply_palette(night);fill(0,0,W,H,0);hud_glass(outer,0);
  u16 border[W];for(int x=0;x<outer.w;x++)border[x]=fb[outer.y*W+outer.x+x];
  hud_draw_speed(outer,&speed);
  for(int x=0;x<outer.w;x++)assert(border[x]==fb[outer.y*W+outer.x+x]);
 }
 printf("Glass outline: independent 2px ring integration (max error %.4f), even corners/sides, AA and inset clearance in both themes passed.\n",worst);
}
