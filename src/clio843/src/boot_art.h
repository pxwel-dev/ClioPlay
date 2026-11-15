#ifndef CLIO_BOOT_ART_H
#define CLIO_BOOT_ART_H
#include "splash_icon.h"
static void spl_icon(int ix,int iy){
 /* Composite canvas, blue tile and white mark once per pixel. The masks are
  * sampled offline, so smooth edges add no large buffer or boot-time sampling.
  * A single composite avoids a dark seam around the white arrow. */
 for(int y=0;y<SPL_ICON_SIZE;y++){
  int d=SPL_ICON_SIZE-1;
  u16 blue=rgb((74*(d-y)+d/2)/d,(177*(d-y)+96*y+d/2)/d,(255*(d-y)+235*y+d/2)/d);
  for(int x=0;x<SPL_ICON_SIZE;x++){
   int i=y*SPL_ICON_SIZE+x,a=spl_icon_outer[i],white=spl_icon_arrow[i];if(!a)continue;
   u16 bg=fb[(iy+y)*W+ix+x];int tile=a-white;
   int r=(((bg>>11)&31)*(64-a)+((blue>>11)&31)*tile+31*white+32)>>6;
   int g=(((bg>>5)&63)*(64-a)+((blue>>5)&63)*tile+63*white+32)>>6;
   int b=((bg&31)*(64-a)+(blue&31)*tile+31*white+32)>>6;
   px(ix+x,iy+y,(u16)((r<<11)|(g<<5)|b));
  }
 }
}
/* Soft, procedural backdrop: no map I/O or image decoding at boot. Colour
 * remains eight-bit until the final screen-anchored ordered RGB565 write. */
struct BootColour {int r,g,b;};
static struct BootColour boot_backdrop(int x,int y,int night){
 int dx=x-84,dy=y-52,a=65536-dx*dx-dy*dy;if(a<0)a=0;a>>=10;
 dx=x-406;dy=y-224;int b=65536-dx*dx-dy*dy;if(b<0)b=0;b>>=10;
 int curve=98+((y-130)*(y-130)>>9)+(y>>2),d=x-curve;if(d<0)d=-d;
 int ribbon=d<30?30-d:0;
 if(night)return (struct BootColour){9+(b>>2),17+(a>>2)+(ribbon>>2),26+(a>>1)+(b>>3)+(ribbon>>1)};
 return (struct BootColour){199-(a*3>>2)+(b*3>>2),216-(a>>3)-(b>>2),231+(a>>2)-(b>>3)-(ribbon>>2)};
}
static unsigned boot_bayer(unsigned x,unsigned y){
 unsigned v=0;for(unsigned bit=0;bit<3;bit++)v=(v<<2)|(((x>>bit&1u)^(y>>bit&1u))<<1)|(y>>bit&1u);return v;
}
static u16 boot_colour(struct BootColour c,int x,int y){
 unsigned d=boot_bayer((unsigned)x,(unsigned)y);int r=(c.r*8+(int)d)>>6,g=(c.g*16+(int)d)>>6,b=(c.b*8+(int)d)>>6;
 if(r>31)r=31;if(g>63)g=63;if(b>31)b=31;return (u16)((r<<11)|(g<<5)|b);
}
static void spl_render(int night){
 for(int y=0;y<H;y++)for(int x=0;x<W;x++)fb[y*W+x]=boot_colour(boot_backdrop(x,y,night),x,y);
 /* Align the visible wordmark, excluding the font's side bearings. */
 const char*title="ClioPlay",*p=title;int pen=0,left_ink=W,right_ink=0,top_ink=H,bottom_ink=0;
 while(*p){const struct CpGlyph*g=&cp_glyphs[6*98+cp_slot(cp_code(&p))];
  if(g->w&&g->h){if(pen+g->left<left_ink)left_ink=pen+g->left;if(pen+g->left+g->w>right_ink)right_ink=pen+g->left+g->w;if(g->top<top_ink)top_ink=g->top;if(g->top+g->h>bottom_ink)bottom_ink=g->top+g->h;}pen+=g->advance;}
 int group_y=(H-(SPL_ICON_SIZE+22+bottom_ink-top_ink))/2;
 spl_icon((W-SPL_ICON_SIZE)/2,group_y);
 label((W-(right_ink-left_ink))/2-left_ink,group_y+SPL_ICON_SIZE+22-top_ink,6,title,night?rgb(244,249,253):rgb(24,44,59),0);
 centered(W/2,244,0,CLIOPLAY_VERSION,night?rgb(134,157,177):rgb(91,118,136));
}
#endif
