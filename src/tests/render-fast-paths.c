/* Pixel equivalence against the preceding scalar rasterizers. Reference
 * algorithms deliberately retain the branch-heavy glass and 64-bit bands. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static u16 backdrop[FB_PIX],expected[FB_PIX];
static void reference_glass(struct HudBox b,u16 tint,u16 edge,unsigned frost,int alert){
 if(b.a<=0||b.w<4||b.h<4||b.w>W||b.h>(HUD_GLASS_ROWS-1)*HUD_GLASS_STEP)return;
 const int r=10;
 int cols=(b.w-1)/HUD_GLASS_STEP+2,rows=(b.h-1)/HUD_GLASS_STEP+2;
 unsigned rim_strength=10u+(5u*(unsigned)alert+128u)/256u;
 hud_glass_sample(b,tint,frost,rows,cols);
 int oldj=-1;
 for(int yy=0;yy<b.h;yy++){
  int sy=b.y+yy;if(sy<0||sy>=H)continue;
  int j=yy/HUD_GLASS_STEP;
  if(j!=oldj){hud_glass_scan(hud_glass_row_a,j,b.w);hud_glass_scan(hud_glass_row_b,j+1,b.w);oldj=j;}
  unsigned fy=(yy&(HUD_GLASS_STEP-1))*2u;
  int dy=yy<b.h-1-yy?yy:b.h-1-yy;
  for(int xx=0;xx<b.w;xx++){
   int sx=b.x+xx;if(sx<0||sx>=W)continue;
   int dx=xx<b.w-1-xx?xx:b.w-1-xx;
   unsigned outer=hud_corner(dx,dy,r);if(!outer)continue;
   unsigned inner=dx&&dy?hud_corner(dx-1,dy-1,r-1):0;
   u16 colour=hud_mix16(hud_glass_row_a[xx],hud_glass_row_b[xx],fy);
   unsigned rim=outer-inner;
   if(rim)colour=hud_mix16(colour,edge,(rim*rim_strength+8u)>>4);
   else if(alert&&(dx==1||dy==1))colour=hud_mix16(colour,edge,(unsigned)(alert+16)/32u);
   unsigned q=(outer*(unsigned)b.a+128u)>>8;
   u16*p=&fb[sy*W+sx];*p=q==16?colour:hud_mix16(*p,colour,q);
  }
 }
}
static void reference_line(int x0,int y0,int x1,int y1,u16 c,int wid){
 if(!MAP_ANTIALIAS){line1(x0>>8,y0>>8,x1>>8,y1>>8,c,wid);return;}
 int soft=0;
#if MAP_ANTIALIAS
 /* Only the thinnest road class gets the wider reconstruction filter. */
 soft=wid==1&&map_mask_active&&map_mask_count==6&&map_mask_shift==0;
#endif
 int dx=x1-x0,dy=y1-y0,steep=(dy<0?-dy:dy)>(dx<0?-dx:dx);
 if(steep){int t=x0;x0=y0;y0=t;t=x1;x1=y1;y1=t;}
 if(x0>x1){int t=x0;x0=x1;x1=t;t=y0;y0=y1;y1=t;}
 dx=x1-x0;dy=y1-y0;
 int stroke_width=(wid/2)*2+1,cap=stroke_width/2+1;
 if(!dx&&soft){int cx=x0>>8,cy=y0>>8;
  for(int j=-3;j<=3;j++)for(int i=-3;i<=3;i++){
   int a=map_minor_cap((cx+i)*256-x0,(cy+j)*256-y0);
   if(!a)continue;
   if(steep){if((unsigned)(cy+j)<W&&cx+i>=MAP_TOP&&cx+i<MAP_BOTTOM)map_minor_store(cy+j,cx+i,a);}
   else if((unsigned)(cx+i)<W&&cy+j>=MAP_TOP&&cy+j<MAP_BOTTOM)map_minor_store(cx+i,cy+j,a);
  }return;
 }
 if(!dx){if(steep)map_dot_q8(y0,x0,c,stroke_width);else map_dot_q8(x0,y0,c,stroke_width);return;}
 /* Axis-aligned bodies remain fast spans; only their round caps need coverage. */
 if(!soft&&!dy&&!((x0|x1|y0)&255)){int r=stroke_width/2;
  if(steep){map_solid((y0>>8)-r,x0>>8,stroke_width,(dx>>8)+1,c);map_dot_q8(y0,x0,c,stroke_width);map_dot_q8(y1,x1,c,stroke_width);}
  else{map_solid(x0>>8,(y0>>8)-r,(dx>>8)+1,stroke_width,c);map_dot_q8(x0,y0,c,stroke_width);map_dot_q8(x1,y1,c,stroke_width);}return;
 }
 int first=(x0>>8)-(soft?2:cap),last=(x1>>8)+(soft?2:cap),majorlo=steep?MAP_TOP:0,majorhi=steep?MAP_BOTTOM-1:W-1;
 if(first<majorlo)first=majorlo;if(last>majorhi)last=majorhi;if(first>last)return;
 int slope=map_ratio_q16((u32)(dy<0?-dy:dy),(u32)dx),step=dy<0?-slope:slope;
 static const u16 norm[17]={256,256,257,260,263,268,273,279,286,293,301,310,320,329,340,350,362};
 int idx=slope>>12,length=norm[idx];if(idx<16)length+=((norm[idx+1]-length)*(slope&4095))>>12;
 /* Preserve the old nominal odd stroke sizes (1,3,5), but correct thickness
  * for slope instead of repeatedly stamping overlapping square pixels. */
 int half=stroke_width*length*128;
 long long pos=(long long)y0*256+(((long long)first*256-x0)*step>>8);
 int minorlo=steep?0:MAP_TOP,minorhi=steep?W-1:MAP_BOTTOM-1;
 /* One-pixel roads need the area on both axes: a centre-column sample
  * overstates diagonal centres and snaps their shoulders into visible steps.
  * Minor roads additionally use a broader, isotropic reconstruction filter:
  * the softer shoulders reduce visible stair steps instead of merely making
  * coverage more accurate. Wider roads retain their fast opaque spans. */
 int thin_idx=(slope+1024)>>11,thin_limit=soft?177:113;
 const u8*thin=soft?map_minor_coverage[thin_idx]:map_thin_coverage[thin_idx];
 int thin_radius=soft?((int)map_minor_radius[thin_idx]*1024+1024):half+(slope>>1)+32768+1024;
 long long thin_len=(long long)dx*dx+(long long)dy*dy,thin_dotstep=(long long)dy*256;
 for(int major=first;major<=last;major++,pos+=step){
  if(stroke_width==1){
   if(pos<(long long)minorlo*65536-thin_radius||pos>(long long)minorhi*65536+thin_radius)continue;
   int center=(int)pos;
   int begin=(center-thin_radius+65535)>>16,end=(center+thin_radius)>>16;
   if(begin<minorlo)begin=minorlo;if(end>minorhi)end=minorhi;
   int endpoint=major*256<x0+(soft?512:256)||major*256>x1-(soft?512:256);
   long long dot=endpoint?((long long)major*256-x0)*dx+((long long)begin*256-y0)*dy:0;
   for(int minor=begin;minor<=end;minor++,dot+=thin_dotstep){
    int delta=minor*65536-center;if(delta<0)delta=-delta;
    unsigned d=(unsigned)((delta+512)>>10);int cover=d<(unsigned)thin_limit?thin[d]:0;
    if(!cover)continue;
    if(endpoint){int round=32;
     if(dot<0)round=soft?map_minor_cap(major*256-x0,minor*256-y0):map_round_coverage_q8(major*256-x0,minor*256-y0,1);
     else if(dot>thin_len)round=soft?map_minor_cap(major*256-x1,minor*256-y1):map_round_coverage_q8(major*256-x1,minor*256-y1,1);
     if(round<cover)cover=round;
    }
    if(soft){if(steep)map_minor_store(minor,major,cover);else map_minor_store(major,minor,cover);}
    else if(steep)map_blend(minor,major,c,cover);else map_blend(major,minor,c,cover);
   }continue;
  }
  long long low=pos-half+32768,high=pos+half+32768;
  int lo=(int)(low>>16),hi=(int)(high>>16);
  int a=(int)((65536-(low-(long long)lo*65536)+1024)>>11);
  int b=(int)(((high-(long long)hi*65536)+1024)>>11);
  /* Near endpoints, clamp the band to a round cap rather than letting a
   * square or diagonal corner poke out beyond the road's radius. */
  if(major*256<x0+cap*256||major*256>x1-cap*256){
   int begin=lo,end=hi;if(begin<minorlo)begin=minorlo;if(end>minorhi)end=minorhi;
   for(int minor=begin;minor<=end;minor++){
    int cover=minor==lo?a:(minor==hi?b:32),round=32;
    long long startdot=((long long)major*256-x0)*dx+((long long)minor*256-y0)*dy;
    long long enddot=((long long)major*256-x1)*dx+((long long)minor*256-y1)*dy;
    if(startdot<0)round=map_round_coverage_q8(major*256-x0,minor*256-y0,stroke_width);
    else if(enddot>0)round=map_round_coverage_q8(major*256-x1,minor*256-y1,stroke_width);
    if(round<cover)cover=round;if(steep)map_blend(minor,major,c,cover);else map_blend(major,minor,c,cover);
   }continue;
  }
  if(lo==hi){int cover=(int)((high-low+1024)>>11);if(steep)map_blend(lo,major,c,cover);else map_blend(major,lo,c,cover);continue;}
  if(steep){map_blend(lo,major,c,a);map_blend(hi,major,c,b);}
  else{map_blend(major,lo,c,a);map_blend(major,hi,c,b);}
  int begin=lo+1,end=hi;if(begin<minorlo)begin=minorlo;if(end>minorhi+1)end=minorhi+1;
  if(begin<end){if(steep)map_solid(begin,major,end-begin,1,c);else map_solid(major,begin,1,end-begin,c);}

 }
}
static void reference_view_trig(int*ss,int*c){
 static int previous=-1,sine,cosine;
 int angle=view_course();while(angle<0)angle+=360;angle%=360;
 if(angle!=previous){
  int k=angle/10,f=angle%10,n=(k+1)%36;
  sine=(SIN36[k]*(10-f)+SIN36[n]*f)/10;
  cosine=(COS36[k]*(10-f)+COS36[n]*f)/10;
  int norm=(sine*sine+cosine*cosine)>>15,scale=(98304-norm)/2;
  sine=(sine*scale+16384)>>15;cosine=(cosine*scale+16384)>>15;
  if(sine>32767)sine=32767;if(cosine>32767)cosine=32767;
  if(sine< -32767)sine=-32767;if(cosine< -32767)cosine=-32767;
  previous=angle;
 }
 *ss=sine;*c=cosine;
}
static int reference_coslat(s32 lat){int d=lat<0?-lat:lat;d/=100;if(d>=90000)return 0;static const u16 c[91]={32767,32762,32747,32722,32687,32642,32587,32523,32448,32364,32269,32165,32051,31927,31794,31650,31498,31335,31163,30982,30791,30591,30381,30162,29934,29697,29451,29196,28932,28659,28377,28087,27788,27481,27165,26841,26509,26169,25821,25465,25101,24730,24351,23964,23571,23170,22762,22347,21925,21497,21062,20621,20173,19720,19260,18794,18323,17846,17364,16876,16384,15886,15383,14876,14364,13848,13328,12803,12275,11743,11207,10668,10126,9580,9032,8481,7927,7371,6813,6252,5690,5126,4560,3993,3425,2856,2286,1715,1144,572,0};int i=d/1000,f=d%1000;return c[i]+((int)c[i+1]-c[i])*f/1000;}
static unsigned rng=0x8bd13827u;
static unsigned random_word(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void glass_checks(void){
 const int widths[]={4,11,19,20,21,62,128,282,480},heights[]={4,7,27,43,80,96};
 for(int i=0;i<=256;i++){
  int w=widths[i%9],h=heights[(i/3)%6];
  const int xs[]={-15,0,8,W-w,W-w+13},ys[]={-7,0,8,H-h,H-h+9};
  struct HudBox b={xs[(i/5)%5],ys[(i/7)%5],w,h,i};
  u16 tint=(u16)random_word(),edge=(u16)random_word();unsigned frost=(unsigned)i%17;int alert=i*37%257;
  memcpy(fb,backdrop,sizeof backdrop);reference_glass(b,tint,edge,frost,alert);memcpy(expected,fb,sizeof expected);
  memcpy(fb,backdrop,sizeof backdrop);hud_glass_surface(b,tint,edge,frost,alert);
  if(memcmp(expected,fb,sizeof expected)){fprintf(stderr,"Glass mismatch case%d: %d,%d %dx%d alpha%d alert%d\n",i,b.x,b.y,w,h,i,alert);assert(0);}
 }
}
static void line_checks(void){
 for(int i=0;i<600;i++){
  int x0=((int)(random_word()%800)-160)*256+(int)(random_word()%256);
  int y0=((int)(random_word()%600)-160)*256+(int)(random_word()%256);
  int x1=((int)(random_word()%800)-160)*256+(int)(random_word()%256);
  int y1=((int)(random_word()%600)-160)*256+(int)(random_word()%256);
  if(!(i%25)){x0=-100000*256;x1=100000*256;}
  if(!(i%31)){y0=-100000*256;y1=100000*256;}
  int kind=i%3==0?2:1,slot=i%6,width=1+i%4;u16 colour=(u16)random_word();
  memcpy(fb,backdrop,sizeof backdrop);map_mask_begin(kind);map_mask_select(kind==2?0:slot);
  reference_line(x0,y0,x1,y1,colour,width);map_mask_end();memcpy(expected,fb,sizeof expected);
  memcpy(fb,backdrop,sizeof backdrop);map_mask_begin(kind);map_mask_select(kind==2?0:slot);
  map_line_q8(x0,y0,x1,y1,colour,width);map_mask_end();
  if(memcmp(expected,fb,sizeof expected)){fprintf(stderr,"Line mismatch case%d\n",i);assert(0);}
 }
}
static void trig_checks(void){
 freecam=1;freecam_course_valid=1;freecam_anim_mode=FC_ANIM_NONE;
 for(int angle=-720;angle<=1080;angle++)for(int repeat=0;repeat<2;repeat++){
  int a,b,c,d;freecam_course=angle;reference_view_trig(&a,&b);view_trig(&c,&d);assert(a==c&&b==d);
 }
 freecam=0;freecam_course_valid=0;
 for(int lat=-9000000;lat<=9000000;lat+=3337)for(int repeat=0;repeat<2;repeat++)assert(coslat_q15(lat)==reference_coslat(lat));
}
static void copy_checks(void){
 static u16 src[FB_PIX+8] __attribute__((aligned(8))),dst[FB_PIX+8] __attribute__((aligned(8))),want[FB_PIX+8];
 for(unsigned i=0;i<FB_PIX+8;i++)src[i]=(u16)random_word();
 for(int s=0;s<4;s++)for(int d=0;d<4;d++){
  for(unsigned i=0;i<FB_PIX+8;i++)dst[i]=want[i]=0xa55a;
  memcpy(want+d,src+s,FB_PIX*sizeof(u16));render_copy(dst+d,src+s);
  assert(!memcmp(dst,want,sizeof dst));
 }
}
int main(void){
 hud_capture_clock=1;hud_capture_ms=10000;render_fb=&display_fb;renderer.building=0;
 for(unsigned i=0;i<FB_PIX;i++)backdrop[i]=(u16)((i*641u+i/W*331u)^(i>>3));
 glass_checks();line_checks();trig_checks();copy_checks();
 puts("Render fast paths: all 257 glass opacities with corners/clipping/alerts, 600 scalar-reference AA lines including extreme endpoints, all camera angle/latitude cache cases, and 16 aligned/unaligned guarded framebuffer copies are pixel identical.");
}
