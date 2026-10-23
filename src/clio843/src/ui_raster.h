/* Shared integer RGB565 shapes and licensed proportional glyph rendering. */
#ifndef CLIO_UI_RASTER_H
#define CLIO_UI_RASTER_H
#include "font_atlas.h"
#include "utf8.h"
static u16 rgb(int r,int g,int b){return (u16)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));}
static u16 ui_canvas(int night){return night?rgb(16,22,40):rgb(218,225,237);}
static u16 mix(u16 a,u16 b,int alpha){
 int ar=(a>>11)&31,ag=(a>>5)&63,ab=a&31;
 return (u16)((((ar*(15-alpha)+((b>>11)&31)*alpha+7)/15)<<11)|
  (((ag*(15-alpha)+((b>>5)&63)*alpha+7)/15)<<5)|((ab*(15-alpha)+(b&31)*alpha+7)/15));
}
static void blend_pixel(int x,int y,u16 c,int a){if(x>=clip_x&&x<clip_right&&y>=clip_y&&y<clip_bottom)fb[y*W+x]=mix(fb[y*W+x],c,a);}
static int rounded_inside(int px0,int py0,int w,int h,int r){
 if(px0<0||py0<0||px0>=w||py0>=h)return 0;
 int dx=px0<r?r-px0:(px0>=w-r?px0-(w-r-1):0);
 int dy=py0<r?r-py0:(py0>=h-r?py0-(h-r-1):0);
 return !dx||!dy||dx*dx+dy*dy<=r*r;
}
static void round_rect(int x,int y,int w,int h,int r,u16 c,int a){
 if(w<=0||h<=0)return;if(r>w/2)r=w/2;if(r>h/2)r=h/2;
 /* Opaque card interiors are fast row spans. Alpha is reserved for corners/shadows. */
 for(int j=0;j<h;j++){
  int inset=0;
  if(j<r||j>=h-r)while(inset<r&&!rounded_inside(inset,j,w,h,r))inset++;
  if(a==15)fill(x+inset,y+j,w-2*inset,1,c);
  else for(int i=inset;i<w-inset;i++)blend_pixel(x+i,y+j,c,a);
 }
}
static void circle(int x,int y,int r,u16 color){
 for(int j=-r-1;j<=r+1;j++)for(int i=-r-1;i<=r+1;i++){
  int d=i*i+j*j;
  if(d<=r*r)px(x+i,y+j,color);
  else if(d<=(r+1)*(r+1))blend_pixel(x+i,y+j,color,5);
 }
}
static void stroke(int x0,int y0,int x1,int y1,u16 c,int radius){
 int dx=x1>x0?x1-x0:x0-x1,dy=-(y1>y0?y1-y0:y0-y1),sx=x0<x1?1:-1,sy=y0<y1?1:-1,err=dx+dy;
 for(;;){circle(x0,y0,radius,c);if(x0==x1&&y0==y1)break;int e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}}
}
static int cp_code(const char**s){return utf8_code(s);}

static int cp_slot(int code){return code>=32&&code<=126?code-32:(code==176?95:(code==8226?96:(code==8230?97:31)));}
static int width(const char*s,int face){
#if CLIO_UNICODE
 if(cliou_ready()&&utf8_native(s))return cliou_width(s,face);
#endif
 int w=0;while(*s){int code=cp_code(&s);w+=cp_glyphs[face*98+cp_slot(code)].advance;}return w;}
static void label(int x,int y,int face,const char*s,u16 color,int limit){
#if CLIO_UNICODE
 if(cliou_ready()&&utf8_native(s)){cliou_draw(x,y,face,s,color,limit);return;}
#endif
 int origin=x;while(s&&*s){int code=cp_code(&s);const struct CpGlyph*g=&cp_glyphs[face*98+cp_slot(code)];
  if(limit>0&&x-origin+g->advance>limit)break;
  for(int yy=0;yy<g->h;yy++)for(int xx=0;xx<g->w;xx++){
   int n=yy*g->w+xx,a=(cp_alpha[g->off+n/2]>>((n&1)*4))&15;
   if(a&&(!limit||x+g->left+xx<origin+limit))blend_pixel(x+g->left+xx,y+g->top+yy,color,a);
  }x+=g->advance;
 }
}
static void centered(int x,int y,int face,const char*s,u16 color){label(x-width(s,face)/2,y,face,s,color,0);}

#endif
