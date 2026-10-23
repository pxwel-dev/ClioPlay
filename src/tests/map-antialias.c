/* Geometry-level checks for edge coverage, clipping, symmetry and tile seams. */
#define HOST_TEST 1
#define main unused_device_entry
#ifndef CLIO_SOURCE
#define CLIO_SOURCE "../clio843/src/clio040.c"
#endif
#include CLIO_SOURCE
#undef main
#include <assert.h>
#include <string.h>
#include <math.h>
static u16 saved[FB_PIX];
static void clear(void){fill(0,0,W,H,0x1234);fill(0,40,W,MAP_STRIP_Y-40,0);}
static void chrome(void){for(int y=0;y<H;y++)if(y<40||y>=MAP_STRIP_Y)for(int x=0;x<W;x++)assert(fb[y*W+x]==0x1234);}
static void strokes(void){
 const int ends[][4]={{60,80,180,100},{60,80,100,180},{60,180,180,80},{60,120,180,120},{80,60,80,180},{60,80,160,180}};
 for(unsigned i=0;i<sizeof(ends)/sizeof(ends[0]);i++)for(int w=1;w<=4;w++){
  clear();map_line(ends[i][0],ends[i][1],ends[i][2],ends[i][3],0xffff,w);chrome();memcpy(saved,fb,sizeof(fb));
  clear();map_line(ends[i][2],ends[i][3],ends[i][0],ends[i][1],0xffff,w);assert(!memcmp(saved,fb,sizeof(fb)));
 }
 clear();map_line(60,80,160,180,0xffff,1);assert(fb[130*W+110]>fb[129*W+110]&&fb[130*W+110]<0xffff);
 assert(fb[129*W+110]>0&&fb[129*W+110]<0xffff);assert(fb[131*W+110]>0&&fb[131*W+110]<0xffff);
 clear();map_line(-100000,-100000,100000,100000,0xffff,1);chrome();
 clear();map_line(60,80,160,180,0xffff,3);assert(fb[130*W+110]==0xffff);
 clear();map_line(-100000,-100000,100000,100000,0xffff,3);chrome();
 clear();map_line(-100000,120,100000,120,0xffff,3);chrome();assert(fb[120*W]==0xffff&&fb[120*W+W-1]==0xffff);
 clear();map_line(80,-100000,80,100000,0xffff,3);chrome();assert(fb[40*W+80]==0xffff&&fb[(MAP_STRIP_Y-1)*W+80]==0xffff);
 clear();map_line(100,100,100,100,0xffff,3);chrome();assert(fb[100*W+100]==0xffff);
 puts("AA strokes: partial edge coverage, pixel-area thin centres, all octants/widths, reversed endpoints, long/offscreen and degenerate clipping passed.");
}
/* Independent numerical area oracle: integrate 512 vertical slices of each
 * pixel, away from caps. This catches the old centre-column approximation. */
static void thin_roads(void){
 double worst=0;
 for(int angle=0;angle<=64;angle++)for(int phase=0;phase<256;phase+=32){
  double slope=angle/64.0,half=sqrt(1+slope*slope)/2;
  int y0=150*256+phase-angle*400,y1=y0+angle*800;
  clear();map_mask_begin(2);map_mask_select(0);
  map_line_q8(50*256,y0,250*256,y1,C_ROAD,1);
  double total=0;
  for(int y=147;y<=153;y++){
   double reference=0;
   for(int sample=0;sample<512;sample++){
    double center=150+phase/256.0+slope*((sample+.5)/512-.5);
    double lo=center-half,hi=center+half;
    if(lo<y-.5)lo=y-.5;if(hi>y+.5)hi=y+.5;
    if(hi>lo)reference+=(hi-lo)/512;
   }
   double actual=(map_coverage[(y-40)*W+150]&31u)/31.0;
   double error=fabs(actual-reference);if(error>worst)worst=error;
   assert(error<.05);total+=actual;
  }
  assert(fabs(total-sqrt(1+slope*slope))<.08);
  map_mask_end();
 }
 printf("Thin roads: 520 angle/subpixel cases match independently integrated pixel area (worst alpha error %.4f); stable total coverage.\n",worst);
}
/* Independent quadrature of the minor-road reconstruction kernel. Check
 * between lookup rows, translated centres, both octants and clipped caps. */
static void minor_roads(void){
 double worst=0;
 for(int angle=0;angle<=64;angle++)for(int phase=0;phase<256;phase+=32){
  double slope=angle/64.0,norm=sqrt(1+slope*slope);
  int y0=150*256+phase-angle*400,y1=y0+angle*800;
  clear();map_mask_begin(1);map_mask_select(0);
  map_line_q8(50*256,y0,250*256,y1,C_ROAD,1);
  double total=0;
  for(int y=147;y<=153;y++){
   double d=(y-150-phase/256.0)/norm,reference=0;
   for(int sample=0;sample<512;sample++){
    double t=-.6+(sample+.5)*1.2/512,offset=(d-t)/.55;
    reference+=exp(-offset*offset/2)*1.2/(512*.55*sqrt(2*3.141592653589793));
   }
   double actual=(map_coverage[(y-40)*W+150]&31u)/31.0;
   double error=fabs(actual-reference);if(error>worst)worst=error;
   assert(error<.055);total+=actual;
  }
  assert(fabs(total-1.2*norm)<.12);map_mask_end();
 }
 for(int direction=-1;direction<=1;direction+=2){
  clear();map_mask_begin(1);map_line_q8(60*256+37,120*256+71,180*256+37,(120+direction*60)*256+71,C_ROAD,1);map_mask_end();memcpy(saved,fb,sizeof(fb));
  clear();map_mask_begin(1);map_line_q8(180*256+37,(120+direction*60)*256+71,60*256+37,120*256+71,C_ROAD,1);map_mask_end();assert(!memcmp(saved,fb,sizeof(fb)));chrome();
 }
 clear();map_mask_begin(1);map_line(-100000,-100000,100000,100000,C_ROAD,1);map_mask_end();chrome();
 clear();map_mask_begin(1);map_line(-100000,120,100000,120,C_ROAD,1);map_mask_end();chrome();
 assert(fb[119*W+150]>0&&fb[121*W+150]>0);
 clear();map_mask_begin(1);map_line(-2,40,-2,40,C_ROAD,1);map_mask_end();chrome();
 printf("Minor roads: 520 reconstruction cases match independent Gaussian quadrature (worst alpha error %.4f); constant width, reversed caps and offscreen clipping passed.\n",worst);
}
static void polygons(void){
 clear();map_area_span(100,60*4096+2048,120*4096+2048,0,0,0xffff);
 assert(fb[100*W+60]>0&&fb[100*W+60]<0xffff);assert(fb[100*W+61]==0xffff);
 assert(fb[100*W+120]>0&&fb[100*W+120]<0xffff);assert(!fb[100*W+121]);chrome();
 clear();map_area_span(100,60*4096,100*4096,0,0,0xffff);map_area_span(100,100*4096,140*4096,0,0,0xffff);
 for(int x=60;x<140;x++)assert(fb[100*W+x]==0xffff);
 clear();map_area_span(100,80*4096,180*4096,100*4096,-100*4096,0xffff);chrome();
 assert(fb[100*W+130]==0xffff);assert(fb[100*W+60]>0&&fb[100*W+60]<0xffff);
 puts("AA areas: fractional boundaries, solid interiors, adjacent hard tile closures and steep fringes passed.");
}

static void route_path(int repeated,int split,int width){
 const int x0=60*256+64,y0=80*256+128;
 map_mask_begin(width==1?1:2);
 for(int repeat=0;repeat<repeated;repeat++){
  if(split){for(int i=0;i<120;i++)map_line_q8(x0+i*256,y0+i*256,x0+(i+1)*256,y0+(i+1)*256,C_ROUTE,width);}
  else map_line_q8(x0,y0,x0+120*256,y0+120*256,C_ROUTE,width);
 }
 map_mask_end();
}
static double segment_d2(double x,double y,double ax,double ay,double bx,double by){double dx=bx-ax,dy=by-ay,len=dx*dx+dy*dy,t=len?((x-ax)*dx+(y-ay)*dy)/len:0;if(t<0)t=0;if(t>1)t=1;dx=x-ax-t*dx;dy=y-ay-t*dy;return dx*dx+dy*dy;}
static void joins(void){
 for(int width=1;width<=3;width+=2){
  clear();route_path(1,0,width);memcpy(saved,fb,sizeof(fb));clear();route_path(2,0,width);assert(!memcmp(saved,fb,sizeof(fb)));
  clear();route_path(1,1,width);assert(!memcmp(saved,fb,sizeof(fb)));
 }
 const int path[][2]={{60*256+64,80*256+128},{120*256+64,80*256+128},{120*256+64,120*256+128},{160*256+64,120*256+128},{160*256+64,180*256+128},{100*256+64,180*256+128},{100*256+64,140*256+128}};
 clear();map_mask_begin(2);for(unsigned i=1;i<sizeof(path)/sizeof(path[0]);i++){map_line_q8(path[i-1][0],path[i-1][1],path[i][0],path[i][1],C_ROUTE,3);map_line_q8(path[i][0],path[i][1],path[i][0],path[i][1],C_ROUTE,3);}map_mask_end();chrome();
 unsigned coloured=0;for(int y=40;y<MAP_STRIP_Y;y++)for(int x=0;x<W;x++)if(fb[y*W+x]){
  double best=1e30;for(unsigned i=1;i<sizeof(path)/sizeof(path[0]);i++){double d=segment_d2(x,y,path[i-1][0]/256.0,path[i-1][1]/256.0,path[i][0]/256.0,path[i][1]/256.0);if(d<best)best=d;}
  assert(best<=2.25*2.25);coloured++;
 }assert(coloured>500);
 memcpy(saved,fb,sizeof(fb));clear();map_mask_begin(2);for(unsigned i=(unsigned)(sizeof(path)/sizeof(path[0]))-1;i>0;i--)map_line_q8(path[i][0],path[i][1],path[i-1][0],path[i-1][1],C_ROUTE,3);map_mask_end();assert(!memcmp(saved,fb,sizeof(fb)));
 for(unsigned i=0;i<sizeof(map_coverage)/sizeof(map_coverage[0]);i++)assert(!map_coverage[i]);
 puts("Thin/route joins: duplicate drawing and 120-way line subdivision are pixel-identical; sharp turns, hairpin, repeated points and reversed traversal stay within a constant-width stroke; coverage clears fully.");
}
int main(void){strokes();thin_roads();minor_roads();polygons();joins();return 0;}
