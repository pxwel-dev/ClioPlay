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
static void blend_pixel(int x,int y,u16 c,int a){
#ifdef CLIO_UI_ALPHA
 if(hud_ink_alpha!=16)a=(a*hud_ink_alpha+8)>>4;
#endif
 if(a&&x>=clip_x&&x<clip_right&&y>=clip_y&&y<clip_bottom)fb[y*W+x]=mix(fb[y*W+x],c,a);
}
static int round_corner_coverage(int x,int y,int r){
 int coverage=0,radius=r*8;
 for(int sy=1;sy<8;sy+=2)for(int sx=1;sx<8;sx+=2){
  int dx=radius-x*8-sx,dy=radius-y*8-sy;
  coverage+=dx*dx+dy*dy<=radius*radius;
 }
 return coverage;
}
static void round_rect(int x,int y,int w,int h,int r,u16 c,int a){
 if(w<=0||h<=0||a<=0)return;
 if(r>w/2)r=w/2;if(r>h/2)r=h/2;
 /* Cache the subpixel corners. Large opaque interiors still use row spans. */
 enum {CACHED_RADIUS=6};
 static u8 corner[CACHED_RADIUS+1][CACHED_RADIUS][CACHED_RADIUS];
 static unsigned ready;
 if(r>0&&r<=CACHED_RADIUS&&!(ready&(1u<<r))){
  for(int j=0;j<r;j++)for(int i=0;i<r;i++)corner[r][j][i]=(u8)round_corner_coverage(i,j,r);
  ready|=1u<<r;
 }
 int opaque=a==15;
#ifdef CLIO_UI_ALPHA
 opaque=opaque&&hud_ink_alpha==16;
#endif
 int start=y<clip_y?clip_y:y,end=y+h>clip_bottom?clip_bottom:y+h;
 for(int row=start;row<end;row++){
  int dy=row-y,opposite=y+h-1-row;
  if(opposite<dy)dy=opposite;
  int inset=dy<r?r:0;
  int left=x+inset,right=x+w-inset;
  if(left<clip_x)left=clip_x;if(right>clip_right)right=clip_right;
  if(opaque)fill(left,row,right-left,1,c);
  else for(int col=left;col<right;col++)blend_pixel(col,row,c,a);
  for(int i=0;i<inset;i++){
   int coverage=r<=CACHED_RADIUS?corner[r][dy][i]:round_corner_coverage(i,dy,r);
   int alpha=(coverage*a+8)>>4;
   blend_pixel(x+i,row,c,alpha);
   blend_pixel(x+w-1-i,row,c,alpha);
  }
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
