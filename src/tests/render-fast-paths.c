/* Optimized surfaces versus scalar references, plus independent high-precision
 * glass checks. Map references retain their preceding 64-bit bands. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static u16 backdrop[FB_PIX],expected[FB_PIX];
static void reference_glass(struct HudBox b,u16 tint,u16 edge,unsigned frost,int alert){
 if(b.a<=0||b.w<4||b.h<4||b.w>W||b.h>(HUD_GLASS_ROWS-1)*HUD_GLASS_STEP)return;
 int cols=(b.w-1)/8+2,rows=(b.h-1)/8+2;unsigned rim_strength=10u+(5u*(unsigned)alert+128u)/256u;
 hud_glass_sample(b,tint,frost,rows,cols);int oldj=-1;
 for(int y=0;y<b.h;y++){
  int sy=b.y+y;if(sy<0||sy>=H)continue;int j=y/8;
  if(j!=oldj){hud_glass_scan(hud_glass_row_a,j,b.w);hud_glass_scan(hud_glass_row_b,j+1,b.w);oldj=j;}
  unsigned fy=(y&7)*2u;int dy=y<b.h-1-y?y:b.h-1-y;
  for(int x=0;x<b.w;x++){
   int sx=b.x+x;if(sx<0||sx>=W)continue;int dx=x<b.w-1-x?x:b.w-1-x;
   unsigned outer=hud_corner(dx,dy,10);if(!outer)continue;
   unsigned inner=dx&&dy?hud_corner(dx-1,dy-1,9):0,rim=outer-inner;
   struct HudGlassColour c=hud_glass_mix(hud_glass_row_a[x],hud_glass_row_b[x],fy);
   if(rim)c=hud_glass_mix(c,hud_glass_expand(edge),(rim*rim_strength+8u)>>4);
   else if(alert&&(dx==1||dy==1))c=hud_glass_mix(c,hud_glass_expand(edge),(unsigned)(alert+16)/32u);
   unsigned q=(outer*(unsigned)b.a+128u)>>8;
   u16*p=&fb[sy*W+sx];if(q<16)c=hud_glass_mix(hud_glass_expand(*p),c,q);
   *p=hud_glass_quantize(c);
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
/* Verify packed Q7 arithmetic independently with separate scalar channels. */
static void glass_precision_checks(void){
 for(unsigned c=0;c<65536;c++)assert(hud_glass_quantize(hud_glass_expand((u16)c))==c);
 for(int n=0;n<8192;n++){
  unsigned ar=random_word()%3969,ag=random_word()%8065,ab=random_word()%3969;
  unsigned br=random_word()%3969,bg=random_word()%8065,bb=random_word()%3969;
  struct HudGlassColour a={(ar<<16)|ab,ag},b={(br<<16)|bb,bg};
  for(unsigned q=0;q<=16;q++){
   struct HudGlassColour c=hud_glass_mix(a,b,q);
   unsigned r=(ar*(16-q)+br*q)>>4,g=(ag*(16-q)+bg*q)>>4,blue=(ab*(16-q)+bb*q)>>4;
   assert(c.rb==((r<<16)|blue)&&c.g==g);
   u16 expected=(u16)((((r+64)>>7)<<11)|(((g+64)>>7)<<5)|((blue+64)>>7));
   assert(hud_glass_quantize(c)==expected);
  }
 }
}
/* Independent complete-area reference, including screen-edge clamping and tint. */
static void glass_sample_checks(void){
 const int widths[]={4,21,128,282,480},heights[]={4,27,80,96};
 for(int n=0;n<40;n++){
  int w=widths[n%5],h=heights[n%4];
  const int xs[]={-15,0,8,W-w+13},ys[]={-7,0,8,H-h+9};
  struct HudBox box={xs[n%4],ys[(n/4)%4],w,h,256};
  int cols=(w-1)/HUD_GLASS_STEP+2,rows=(h-1)/HUD_GLASS_STEP+2;
  u16 tint=(u16)random_word();unsigned frost=n%17;
  memcpy(fb,backdrop,sizeof backdrop);hud_glass_sample(box,tint,frost,rows,cols);
  for(int j=0;j<rows;j++)for(int i=0;i<cols;i++){
   unsigned r=0,g=0,b=0;
   for(int iy=0;iy<8;iy++)for(int ix=0;ix<8;ix++){
    int x=box.x+i*HUD_GLASS_STEP+ix-4,y=box.y+j*HUD_GLASS_STEP+iy-4;
    if(x<0)x=0;if(x>=W)x=W-1;if(y<0)y=0;if(y>=H)y=H-1;
    u16 c=backdrop[y*W+x];r+=c>>11;g+=(c>>5)&63;b+=c&31;
   }
   r=(r*2*(16-frost)+((tint>>11)<<7)*frost)>>4;
   g=(g*2*(16-frost)+(((tint>>5)&63)<<7)*frost)>>4;
   b=(b*2*(16-frost)+((tint&31)<<7)*frost)>>4;
   assert(hud_glass_grid[j][i].rb==((r<<16)|b)&&hud_glass_grid[j][i].g==g);
  }
 }
 /* A thin road must contribute equally at all eight source offsets. */
 unsigned reference=0;
 for(int phase=0;phase<8;phase++){
  memset(fb,0,FB_PIX*sizeof(u16));for(int y=0;y<H;y++)fb[y*W+160+phase]=0xffff;
  struct HudBox box={80,80,240,64,256};int rows=(box.h-1)/8+2,cols=(box.w-1)/8+2;
  hud_glass_sample(box,0,0,rows,cols);unsigned sum=0;
  for(int j=0;j<rows;j++)for(int i=0;i<cols;i++){
   struct HudGlassColour c=hud_glass_grid[j][i];sum+=(c.rb>>16)+c.g+(c.rb&0xfffu);
  }
  assert(sum>0);if(!phase)reference=sum;else assert(sum==reference);
 }
}
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
static void glass_cache_checks(void){
 const struct HudBox boxes[]={{8,8,240,80,256},{180,30,128,48,192},{350,8,120,48,256}};
 for(int pass=0;pass<24;pass++){
  if(!(pass%4)){
   for(int i=0;i<FB_PIX;i++)backdrop[i]=(u16)random_word();
   hud_glass_background_changed();
  }
  memcpy(fb,backdrop,sizeof backdrop);
  for(int i=0;i<3;i++){
   struct HudBox b=boxes[i];b.a=(pass*17+i*53)%257;if(pass&1)b.x+=3;
   hud_glass_surface(b,(u16)(0x1234+pass*139+i*13),(u16)(0x5678+i*33),(unsigned)(pass+i)%17,(pass*13)%257);
  }
  memcpy(expected,fb,sizeof expected);
  memcpy(fb,backdrop,sizeof backdrop);hud_glass_frame_begin();
  for(int i=0;i<3;i++){
   struct HudBox b=boxes[i];b.a=(pass*17+i*53)%257;if(pass&1)b.x+=3;
   hud_glass_surface(b,(u16)(0x1234+pass*139+i*13),(u16)(0x5678+i*33),(unsigned)(pass+i)%17,(pass*13)%257);
  }
  hud_glass_frame_end();assert(!memcmp(expected,fb,sizeof expected));
  /* Replay on the same backdrop: unchanged root cards may reuse their grids,
   * but overlapping cards/insets must still see newly painted glass. */
  memcpy(fb,backdrop,sizeof backdrop);hud_glass_frame_begin();
  for(int i=0;i<3;i++){
   struct HudBox b=boxes[i];b.a=(pass*17+i*53)%257;if(pass&1)b.x+=3;
   hud_glass_surface(b,(u16)(0x1234+pass*139+i*13),(u16)(0x5678+i*33),(unsigned)(pass+i)%17,(pass*13)%257);
  }
  hud_glass_frame_end();assert(!memcmp(expected,fb,sizeof expected));
 }
 /* More surfaces than cache slots always retain the uncached result. */
 for(int enabled=0;enabled<2;enabled++){
  memcpy(fb,backdrop,sizeof backdrop);if(enabled)hud_glass_frame_begin();
  for(int i=0;i<12;i++)hud_glass_surface((struct HudBox){8+i*35,8,32,40,256},0x1111,0x8888,8,0);
  hud_glass_frame_end();if(!enabled)memcpy(expected,fb,sizeof expected);else assert(!memcmp(expected,fb,sizeof expected));
 }
 /* The final +3 pixel is outside the painted pill, but still contributes.
  * A previous card touching precisely that point must prevent cached reuse. */
 for(int axis=0;axis<2;axis++){
  struct HudBox previous=axis?(struct HudBox){68,123,64,32,256}:(struct HudBox){123,68,32,64,256};
  struct HudBox target=axis?(struct HudBox){80,80,32,40,256}:(struct HudBox){80,80,40,32,256};
  hud_glass_background_changed();
  for(int pass=0;pass<2;pass++){
   u16 edge=pass?0xf800:0x001f;
   memcpy(fb,backdrop,sizeof backdrop);
   hud_glass_surface(previous,0x18c3,edge,8,0);hud_glass_surface(target,0x2222,0x8888,8,0);
   memcpy(expected,fb,sizeof expected);
   memcpy(fb,backdrop,sizeof backdrop);hud_glass_frame_begin();
   hud_glass_surface(previous,0x18c3,edge,8,0);hud_glass_surface(target,0x2222,0x8888,8,0);
   hud_glass_frame_end();assert(!memcmp(expected,fb,sizeof expected));
  }
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
 glass_precision_checks();glass_sample_checks();glass_checks();glass_cache_checks();line_checks();trig_checks();copy_checks();
 puts("Glass: Q7 colour/quantization, clamped full-area reference and background-cache parity passed. Render fast paths: all 257 glass opacities with corners/clipping/alerts, 600 scalar-reference AA lines including extreme endpoints, all camera angle/latitude cache cases, and 16 aligned/unaligned guarded framebuffer copies are pixel identical.");
}
