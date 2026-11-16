/* Cached exact ink metrics from baked glyphs; no scratch framebuffer or
 * full-surface scan on the head unit. Geometry is shared by both themes. */
struct Ink {char text[193];int face,l,t,w,h;};
static struct Ink ink_cache[128];static unsigned ink_next;
static struct Ink*ink_bounds(const char*s,int face){
 for(unsigned i=0;i<128;i++)if(ink_cache[i].text[0]&&ink_cache[i].face==face&&streq(ink_cache[i].text,s))return &ink_cache[i];
 struct Ink*m=&ink_cache[ink_next++%128];copystr(m->text,sizeof(m->text),s);m->face=face;m->l=m->t=m->w=m->h=0;
#if CLIO_UNICODE
 if(cliou_ready()&&(face>=16||utf8_native(s))){int bounds[4];if(cliou_bounds(s,face>=16?face-16:face,bounds)){m->l=bounds[0];m->t=bounds[1];m->w=bounds[2];m->h=bounds[3];}return m;}
#endif
 if(face>=16)face-=16;int pen=0,left=32767,top=32767,right=-32767,bottom=-32767;
 while(*s){int cp=utf8_code(&s);const struct CpGlyph*g=&cp_glyphs[face*98+cp_slot(cp)];
  for(int y=0;y<g->h;y++)for(int x=0;x<g->w;x++){int at=y*g->w+x,a=(cp_alpha[g->off+at/2]>>((at&1)*4))&15;if(a){int xx=pen+g->left+x,yy=g->top+y;if(xx<left)left=xx;if(xx>right)right=xx;if(yy<top)top=yy;if(yy>bottom)bottom=yy;}}
  pen+=g->advance;
 }if(right>=left){m->l=left;m->t=top;m->w=right-left+1;m->h=bottom-top+1;}return m;
}
static void fitted_text(char out[193],const char*s,int face,int width_limit){
 copystr(out,193,s?s:"");if(width(out,face)<=width_limit)return;
 while(out[0]&&width(out,face)+width("…",face)>width_limit)utf8_backspace(out);
 appendstr(out,193,"…");
}
static void box_text(int x,int y,int w,int h,int face,const char*s,u16 colour){
 if(w<=0||h<=0||!s||!*s)return;char text[193];fitted_text(text,s,face,w);
 struct Ink m=*ink_bounds(text,face);label(x+(w-m.w)/2-m.l,y+(h-m.h)/2-m.t,face,text,colour,0);
}
static int title_face=2;
static void title_draw(struct HudBox b,const char*s){box_text(b.x+14,b.y+4,b.w-28,22,title_face,ui_tr(s),C_WHITE);}
/* A small union-sampled chevron, shared by disclosure and adjustment hints. */
static void menu_chevron(int cx,int cy,int direction,u16 colour){
 static u8 coverage[13][9];static int ready;
 if(!ready){
  for(int y=-6;y<=6;y++)for(int x=-4;x<=4;x++){int n=0;
   for(int sy=2;sy<16;sy+=4)for(int sx=2;sx<16;sx+=4){int px=x*16+sx-8,py=y*16+sy-8,inside=0;
    for(int arm=0;arm<2;arm++){int ax=arm?32:-32,ay=arm?0:-64,vx=arm?-64:64,vy=64,length=vx*vx+vy*vy,t=(px-ax)*vx+(py-ay)*vy;
     if(t<0)t=0;if(t>length)t=length;int dx=px-(ax+vx*t/length),dy=py-(ay+vy*t/length);if(dx*dx+dy*dy<=144)inside=1;
    }n+=inside;
   }coverage[y+6][x+4]=(u8)((n*15+8)/16);
  }ready=1;
 }
 for(int y=-6;y<=6;y++)for(int x=-4;x<=4;x++)blend_pixel(cx+x*direction,cy+y,colour,coverage[y+6][x+4]);
}
struct RowPair {
 int x,nw,vx,vw,left,right,adjust;
 char name[193],value[193];struct Ink ni,vi;
};
static struct RowPair pair_layout(struct HudBox b,int face,const char*name,const char*value,int adjust){
 struct RowPair p;memzero(&p,sizeof(p));int has=value&&*value&&!streq(value,">"),room=b.w-80,gap=has?16:0;
 p.adjust=has&&adjust;int brackets=p.adjust?28:0;
 if(has){fitted_text(p.value,value,face,room/2-brackets);p.vi=*ink_bounds(p.value,face);p.vw=p.vi.w;}
 fitted_text(p.name,name,face,room-p.vw-gap-brackets);p.ni=*ink_bounds(p.name,face);p.nw=p.ni.w;
 p.x=b.x+(b.w-p.nw-gap-p.vw-brackets)/2;p.vx=p.x+p.nw+gap+(p.adjust?14:0);
 p.left=p.vx-10;p.right=p.vx+p.vw-1+10;return p;
}
static int pair_origin(const struct RowPair*p,int y,int h){
 int top=p->ni.t,bottom=top+p->ni.h;
 if(p->vw){if(p->vi.t<top)top=p->vi.t;if(p->vi.t+p->vi.h>bottom)bottom=p->vi.t+p->vi.h;}
 return y+(h-bottom+top)/2-top;
}
/* Match the character body on the shared baseline, excluding accents and
 * descenders. The same reference is used for both adjustment directions. */
static int body_centre(int origin,int face){struct Ink cap=*ink_bounds("H",face);return origin+cap.t+(cap.h-1)/2;}
static int pair_centre_y(const struct RowPair*p,int y,int h,int face){return body_centre(pair_origin(p,y,h),face);}
static int box_text_origin(int y,int h,int face,const char*s){struct Ink m=*ink_bounds(s,face);return y+(h-m.h)/2-m.t;}
static void pair_draw(const struct RowPair*p,int y,int h,int face,u16 colour,u16 secondary,int left_enabled,int right_enabled){
 int origin=pair_origin(p,y,h);label(p->x-p->ni.l,origin,face,p->name,colour,0);
 if(p->vw)label(p->vx-p->vi.l,origin,face,p->value,secondary,0);
 if(p->adjust){int cy=body_centre(origin,face);menu_chevron(p->left,cy,-1,left_enabled?secondary:C_BORDER);menu_chevron(p->right,cy,1,right_enabled?secondary:C_BORDER);}
}
static int pair_text(struct HudBox b,int y,int h,int face,const char*name,const char*value,u16 colour,u16 secondary){
 struct RowPair p=pair_layout(b,face,name,value,0);pair_draw(&p,y,h,face,colour,secondary,1,1);return pair_centre_y(&p,y,h,face);
}
static void disclosure(struct HudBox b,int cy,u16 colour){menu_chevron(b.x+b.w-23,cy,1,colour);}

/* Canonical base letters for the existing Latin, Greek and Cyrillic keycaps.
 * Generated with Unicode canonical decomposition; the typed code point stays
 * precomposed. Native ASCII and accented keys share the same Noto font path. */
struct KeyCore {int code,base;};
static const struct KeyCore key_cores[]={
 {192,65},
 {193,65},
 {194,65},
 {195,65},
 {196,65},
 {199,67},
 {200,69},
 {201,69},
 {202,69},
 {203,69},
 {204,73},
 {205,73},
 {206,73},
 {207,73},
 {209,78},
 {210,79},
 {211,79},
 {212,79},
 {213,79},
 {214,79},
 {217,85},
 {218,85},
 {219,85},
 {220,85},
 {260,65},
 {262,67},
 {280,69},
 {286,71},
 {304,73},
 {323,78},
 {346,83},
 {350,83},
 {376,89},
 {377,90},
 {379,90},
 {902,913},
 {904,917},
 {905,919},
 {906,921},
 {908,927},
 {910,933},
 {911,937},
 {938,921},
 {939,933},
 {1025,1045},
 {1031,1030},
 {1049,1048},
};
struct KeyOrigin {int x,y;struct Ink ink;};
static int core_key(const char*s){return kbd_language<12&&!pk_symbols&&utf8_count(s)==1;}
static struct KeyOrigin key_origin(int x,int y,int w,int h,int face,const char*s){
 const char*p=s;int cp=utf8_code(&p),base=cp;for(unsigned i=0;i<sizeof(key_cores)/sizeof(key_cores[0]);i++)if(key_cores[i].code==cp){base=key_cores[i].base;break;}
 char b[8];kbd_encode(base,b);struct Ink core=*ink_bounds(b,face+16),ink=*ink_bounds(s,face+16);
 return (struct KeyOrigin){x+(w-core.w)/2-core.l,y+(h-core.h)/2-core.t,ink};
}
static void key_text(int x,int y,int w,int h,int face,const char*s,u16 colour){
 if(core_key(s)){struct KeyOrigin p=key_origin(x,y,w,h,face,s);
#if CLIO_UNICODE
 if(cliou_ready())cliou_draw(p.x,p.y,face,s,colour,0);else
#endif
 label(p.x,p.y,face,s,colour,0);}
 else box_text(x,y,w,h,face,s,colour);
}
