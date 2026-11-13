/* Area-filtered RGB565 frost with a wider, separable background blend.
 * Cache unchanged backdrop samples; preserve fractions until final rounding.
 * Screen-anchored dithering only at the final glass write; no full-screen blur. */
struct HudBox {int x,y,w,h,a;}; /* opacity in Q8 */
struct HudCard {
 struct HudBox box;
 int kind, flags, value;
 u16 ink;
 char title[193], detail[112], tag[24], badge[24], connection[96], feedback[112];
};
struct HudTween {
 struct HudBox from;
 struct HudCard target, previous;
 u32 position_at, content_at, fast_content_at;
 int seen;
};
enum {HC_STATUS,HC_CLOCK,HC_SPEED,HC_ROAD,HC_TURN,HC_ROAD_NAME,HC_COUNT};
static struct HudTween hud_tween[HC_COUNT];

static int hud_ease(u32 elapsed,u32 duration){return frame_ease(elapsed,duration);}

static int hud_lerp(int a,int b,int t){return a+((b-a)*t+128)/256;}
static int hud_box_eq(struct HudBox a,struct HudBox b){
 return a.x==b.x&&a.y==b.y&&a.w==b.w&&a.h==b.h&&a.a==b.a;
}
static int hud_content_eq(const struct HudCard*a,const struct HudCard*b){
 return a->kind==b->kind&&a->flags==b->flags&&a->value==b->value&&a->ink==b->ink&&
  streq(a->title,b->title)&&streq(a->detail,b->detail)&&streq(a->tag,b->tag)&&streq(a->badge,b->badge)&&
  streq(a->connection,b->connection)&&streq(a->feedback,b->feedback);
}
static int hud_same_status_except_volume(const struct HudCard*a,const struct HudCard*b){
 return a->kind==b->kind&&a->flags==b->flags&&a->value==b->value&&a->ink==b->ink&&streq(a->tag,b->tag)&&
  (a->kind==HUD_VOLUME||streq(a->title,b->title))&&streq(a->detail,b->detail)&&
  streq(a->connection,b->connection)&&streq(a->feedback,b->feedback);
}
/* Existing numeric channels may crossfade at the map cadence. New labels,
 * notification rows, symbols and geometry retain the faster transition clock. */
static int hud_value_update(int id,const struct HudCard*a,const struct HudCard*b){
 if(a->kind!=b->kind||a->flags!=b->flags||a->value!=b->value||a->ink!=b->ink||
    !streq(a->tag,b->tag)||!streq(a->connection,b->connection)||!streq(a->feedback,b->feedback))return 0;
 if(id==HC_STATUS)return hud_same_status_except_volume(a,b)&&!!a->badge[0]==!!b->badge[0]&&
   (a->kind!=HUD_VOLUME||!!a->title[0]==!!b->title[0]);
 if(id==HC_CLOCK)return !!a->title[0]==!!b->title[0]&&!!a->detail[0]==!!b->detail[0]&&streq(a->badge,b->badge);
 if(id==HC_SPEED||id==HC_TURN)return streq(a->detail,b->detail)&&streq(a->badge,b->badge);
 if(id==HC_ROAD)return streq(a->title,b->title)&&streq(a->detail,b->detail);
 return 0;
}
static struct HudBox hud_pose(const struct HudTween*t,u32 now){
 struct HudBox b=t->target.box,a=t->from;
 int q=hud_ease(now-t->position_at,320);
 if(q==256)return b;
 return (struct HudBox){hud_lerp(a.x,b.x,q),hud_lerp(a.y,b.y,q),hud_lerp(a.w,b.w,q),hud_lerp(a.h,b.h,q),hud_lerp(a.a,b.a,q)};
}
static void hud_set_card(int id,struct HudCard target,u32 now){
 struct HudTween*t=&hud_tween[id];
 if(!t->seen){t->seen=1;t->content_at=t->fast_content_at=now-220u;t->position_at=now-320u;}
 int dy=id==HC_STATUS?-6:6;
 if(id==HC_CLOCK)dy=-4;
 if(!target.box.a){
  if(!t->target.box.a)return;
  /* Keep the outgoing contents while fading and moving off their anchor. */
  t->from=hud_pose(t,now);t->target.box.a=0;t->target.box.y+=dy;t->position_at=now;
  return;
 }
 struct HudBox here=hud_pose(t,now);
 if(!here.a){
  t->from=target.box;t->from.a=0;t->position_at=now;
  t->from.y+=dy;
  t->previous=target;t->content_at=now-220u;
 }else{
  if(!hud_box_eq(t->target.box,target.box)){t->from=here;t->position_at=now;}
  if(!hud_content_eq(&t->target,&target)){
   if(!hud_value_update(id,&t->target,&target))t->fast_content_at=now;
   /* Rapid adjustments retarget the latest state instead of replaying the
    * card's entrance. Keep the more visible outgoing text as the handoff. */
   if(id!=HC_ROAD_NAME&&now-t->content_at<220u&&t->target.kind==target.kind&&t->target.flags==target.flags&&
      (id!=HC_STATUS||(hud_same_status_except_volume(&t->previous,&t->target)&&
                       hud_same_status_except_volume(&t->target,&target)))){
    t->previous=target;t->content_at=now-95u;
   }else{
    if(now-t->content_at>=95u)t->previous=t->target;
    t->content_at=now;
   }
  }
 }
 t->target=target;
}
static const struct HudCard* hud_ink_card(const struct HudTween*t,u32 now,int*alpha,int*offset){
 u32 age=now-t->content_at;
 *alpha=16;*offset=0;
 if(age>=220u)return &t->target;
 if(age<95u){
  int q=hud_ease(age,95);
  *alpha=16-(q*5>>8);*offset=-(q*2>>8);
  return &t->previous;
 }
 int q=hud_ease(age-95u,125);
 *alpha=11+(q*5>>8);*offset=2-(q*2>>8);
 return &t->target;
}

static inline u16 hud_mix16(u16 a,u16 b,unsigned q){
 u32 rb=(((a&0xf81fu)*(16u-q)+(b&0xf81fu)*q)>>4)&0xf81fu;
 u32 g=(((a&0x07e0u)*(16u-q)+(b&0x07e0u)*q)>>4)&0x07e0u;
 return (u16)(rb|g);
}
/* Four-by-four subpixel coverage, precomputed for the glass corner/rim. */
static const u8 hud_corner_9[9*9]={
 0,0,0,0,0,4,10,14,16,
 0,0,0,3,12,16,16,16,16,
 0,0,3,15,16,16,16,16,16,
 0,3,15,16,16,16,16,16,16,
 0,12,16,16,16,16,16,16,16,
 4,16,16,16,16,16,16,16,16,
 10,16,16,16,16,16,16,16,16,
 14,16,16,16,16,16,16,16,16,
 16,16,16,16,16,16,16,16,16
};
static const u8 hud_corner_10[10*10]={
 0,0,0,0,0,0,6,11,14,16,
 0,0,0,0,5,14,16,16,16,16,
 0,0,0,10,16,16,16,16,16,16,
 0,0,10,16,16,16,16,16,16,16,
 0,5,16,16,16,16,16,16,16,16,
 0,14,16,16,16,16,16,16,16,16,
 6,16,16,16,16,16,16,16,16,16,
 11,16,16,16,16,16,16,16,16,16,
 14,16,16,16,16,16,16,16,16,16,
 16,16,16,16,16,16,16,16,16,16
};
static unsigned hud_corner(int dx,int dy,int r){
 if(dx>=r||dy>=r)return 16;
 return r==10?hud_corner_10[dy*10+dx]:hud_corner_9[dy*9+dx];
}
/* Seven fractional bits per native channel retain colour detail through the
 * sampler, tint, reconstruction, rim and opacity. RGB565 is quantized only at
 * the final write. RB fields are 16 bits apart: 31*128*16 cannot carry. */
struct HudGlassColour {u32 rb,g;};
static inline __attribute__((always_inline)) struct HudGlassColour hud_glass_expand(u16 c){
 return (struct HudGlassColour){((u32)(c&0xf800u)<<12)|((u32)(c&31u)<<7),(u32)(c&0x07e0u)<<2};
}
static inline __attribute__((always_inline)) struct HudGlassColour hud_glass_mix(struct HudGlassColour a,struct HudGlassColour b,unsigned q){
 return (struct HudGlassColour){((a.rb*(16u-q)+b.rb*q)>>4)&0x0fff0fffu,(a.g*(16u-q)+b.g*q)>>4};
}
static inline __attribute__((always_inline)) u16 hud_glass_quantize_bias(struct HudGlassColour c,u32 bias){
 u32 rb=(c.rb+bias)>>7;
 return (u16)(((rb>>5)&0xf800u)|(rb&31u)|(((c.g+(bias>>16))>>2)&0x07e0u));
}
static inline __attribute__((always_inline)) u16 hud_glass_quantize(struct HudGlassColour c){
 return hud_glass_quantize_bias(c,0x00400040u);
}
static const u32*hud_glass_bias_row(int y){
 /* All 64 thresholds have mean 64 in Q7. Exact RGB565 colours (including
  * menu selection blue) remain exact. The fixed screen pattern never cycles
  * with time or follows a resizing card, and needs no diffusion buffers. */
#define HUD_GLASS_BIAS(n) ((1u+(n)*2u)*0x00010001u)
 static const u32 bias[8][8]={
  {HUD_GLASS_BIAS(0),HUD_GLASS_BIAS(32),HUD_GLASS_BIAS(8),HUD_GLASS_BIAS(40),HUD_GLASS_BIAS(2),HUD_GLASS_BIAS(34),HUD_GLASS_BIAS(10),HUD_GLASS_BIAS(42)},
  {HUD_GLASS_BIAS(48),HUD_GLASS_BIAS(16),HUD_GLASS_BIAS(56),HUD_GLASS_BIAS(24),HUD_GLASS_BIAS(50),HUD_GLASS_BIAS(18),HUD_GLASS_BIAS(58),HUD_GLASS_BIAS(26)},
  {HUD_GLASS_BIAS(12),HUD_GLASS_BIAS(44),HUD_GLASS_BIAS(4),HUD_GLASS_BIAS(36),HUD_GLASS_BIAS(14),HUD_GLASS_BIAS(46),HUD_GLASS_BIAS(6),HUD_GLASS_BIAS(38)},
  {HUD_GLASS_BIAS(60),HUD_GLASS_BIAS(28),HUD_GLASS_BIAS(52),HUD_GLASS_BIAS(20),HUD_GLASS_BIAS(62),HUD_GLASS_BIAS(30),HUD_GLASS_BIAS(54),HUD_GLASS_BIAS(22)},
  {HUD_GLASS_BIAS(3),HUD_GLASS_BIAS(35),HUD_GLASS_BIAS(11),HUD_GLASS_BIAS(43),HUD_GLASS_BIAS(1),HUD_GLASS_BIAS(33),HUD_GLASS_BIAS(9),HUD_GLASS_BIAS(41)},
  {HUD_GLASS_BIAS(51),HUD_GLASS_BIAS(19),HUD_GLASS_BIAS(59),HUD_GLASS_BIAS(27),HUD_GLASS_BIAS(49),HUD_GLASS_BIAS(17),HUD_GLASS_BIAS(57),HUD_GLASS_BIAS(25)},
  {HUD_GLASS_BIAS(15),HUD_GLASS_BIAS(47),HUD_GLASS_BIAS(7),HUD_GLASS_BIAS(39),HUD_GLASS_BIAS(13),HUD_GLASS_BIAS(45),HUD_GLASS_BIAS(5),HUD_GLASS_BIAS(37)},
  {HUD_GLASS_BIAS(63),HUD_GLASS_BIAS(31),HUD_GLASS_BIAS(55),HUD_GLASS_BIAS(23),HUD_GLASS_BIAS(61),HUD_GLASS_BIAS(29),HUD_GLASS_BIAS(53),HUD_GLASS_BIAS(21)}
 };
#undef HUD_GLASS_BIAS
 return bias[(unsigned)y&7u];
}
static inline __attribute__((always_inline)) u16 hud_glass_quantize_row(struct HudGlassColour c,const u32*row,int x){
 return hud_glass_quantize_bias(c,row[(unsigned)x&7u]);
}
#define HUD_GLASS_STEP 8
#define HUD_GLASS_RADIUS 12
#define HUD_GLASS_COLS (W/HUD_GLASS_STEP+2)
#define HUD_GLASS_ROWS (H/HUD_GLASS_STEP+1)
static struct HudGlassColour hud_glass_grid[HUD_GLASS_ROWS][HUD_GLASS_COLS];
static struct HudGlassColour hud_glass_row_a[W],hud_glass_row_b[W];
struct HudGlassSum {u32 rb,g;};
static struct HudGlassSum hud_glass_cells[2][(HUD_GLASS_COLS+2)*2];
static struct HudGlassSum hud_glass_blur[3][HUD_GLASS_COLS];
/* Only the clean-background compositor enables reuse. Reject a cached sample
 * when an earlier card may have painted into its filter footprint; insets must
 * still sample their current parent glass. Legacy/direct draws stay uncached. */
#define HUD_GLASS_CACHE_COUNT 8
static struct {
 struct HudBox box;u32 background;u16 tint;unsigned frost;
 struct HudGlassColour grid[HUD_GLASS_ROWS][HUD_GLASS_COLS];
} hud_glass_cache[HUD_GLASS_CACHE_COUNT];
static struct HudBox hud_glass_painted[HUD_GLASS_CACHE_COUNT];
static u32 hud_glass_background=1;
static unsigned hud_glass_calls;
static int hud_glass_frame_active;
static void hud_glass_background_changed(void){
 hud_glass_frame_active=0;
 if(!++hud_glass_background){
  hud_glass_background=1;
  for(unsigned i=0;i<HUD_GLASS_CACHE_COUNT;i++)hud_glass_cache[i].background=0;
 }
}
static void hud_glass_frame_begin(void){hud_glass_calls=0;hud_glass_frame_active=1;}
static void hud_glass_frame_end(void){hud_glass_frame_active=0;}
static int hud_glass_cache_slot(struct HudBox b,int rows,int cols){
 if(!hud_glass_frame_active||hud_glass_calls>=HUD_GLASS_CACHE_COUNT)return -1;
 unsigned slot=hud_glass_calls++;int clear=1;
 /* The blended footprint covers -12 through +11; its exclusive end is +12.
  * Clamp to the source pixels actually read at the display edge. */
 int left=b.x-HUD_GLASS_RADIUS,top=b.y-HUD_GLASS_RADIUS;
 int right=b.x+(cols-1)*HUD_GLASS_STEP+HUD_GLASS_RADIUS,bottom=b.y+(rows-1)*HUD_GLASS_STEP+HUD_GLASS_RADIUS;
 if(left<0)left=0;if(left>=W)left=W-1;if(top<0)top=0;if(top>=H)top=H-1;
 if(right<1)right=1;if(right>W)right=W;if(bottom<1)bottom=1;if(bottom>H)bottom=H;
 for(unsigned i=0;i<slot;i++){
  struct HudBox p=hud_glass_painted[i];
  if(left<p.x+p.w&&right>p.x&&top<p.y+p.h&&bottom>p.y)clear=0;
 }
 hud_glass_painted[slot]=b;
 return clear?(int)slot:-1;
}
static u16 hud_sample(int x,int y){
 if(x<0)x=0;if(y<0)y=0;if(x>=W)x=W-1;if(y>=H)y=H-1;
 return fb[y*W+x];
}
/* Combine 4x4 sums to cover EVERY pixel of each 8x8 filter footprint.
 * Each source pixel is read only once per card, with clamping at screen edges.
 * Up to 64 blue values fit below the packed red field, so RB can share a sum. */
static void hud_glass_cell_row(struct HudGlassSum*out,int x,int y,int count){
 const u16*line[4];
 for(int k=0;k<4;k++){int sy=y+k;if(sy<0)sy=0;if(sy>=H)sy=H-1;line[k]=fb+sy*W;}
 for(int i=0;i<count;i++,x+=4){
  u32 rb=0,g=0;
  if(x>=0&&x+3<W){
   for(int k=0;k<4;k++){
    const u16*p=line[k]+x;u32 a=p[0],b=p[1],c=p[2],d=p[3];
    rb+=(a&0xf81fu)+(b&0xf81fu)+(c&0xf81fu)+(d&0xf81fu);
    g+=(a&0x07e0u)+(b&0x07e0u)+(c&0x07e0u)+(d&0x07e0u);
   }
  }else{
   for(int k=0;k<4;k++)for(int n=0;n<4;n++){
    int sx=x+n;if(sx<0)sx=0;if(sx>=W)sx=W-1;
    u32 c=line[k][sx];rb+=c&0xf81fu;g+=c&0x07e0u;
   }
  }
  out[i]=(struct HudGlassSum){rb,g};
 }
}
static struct HudGlassSum hud_glass_area_sum(int k){
 const struct HudGlassSum*a=hud_glass_cells[0],*b=hud_glass_cells[1];
 u32 rb=a[k].rb+a[k+1].rb+b[k].rb+b[k+1].rb;
 /* Separate R/B by 16 bits before weighting: even the final sum fits. */
 return (struct HudGlassSum){((rb>>11)<<16)|(rb&0x07ffu),a[k].g+a[k+1].g+b[k].g+b[k+1].g};
}
static void hud_glass_filter_row(struct HudGlassSum*out,int x,int y,int cols){
 hud_glass_cell_row(hud_glass_cells[0],x-HUD_GLASS_RADIUS,y-4,(cols+2)*2);
 hud_glass_cell_row(hud_glass_cells[1],x-HUD_GLASS_RADIUS,y,(cols+2)*2);
 struct HudGlassSum left=hud_glass_area_sum(0),middle=hud_glass_area_sum(2);
 for(int i=0;i<cols;i++){
  struct HudGlassSum right=hud_glass_area_sum(i*2+4);
  out[i]=(struct HudGlassSum){left.rb+middle.rb*2u+right.rb,left.g+middle.g*2u+right.g};
  left=middle;middle=right;
 }
}
static void hud_glass_sample(struct HudBox b,u16 tint,unsigned frost,int rows,int cols){
 struct HudGlassColour wash=hud_glass_expand(tint);
 /* Fully tinted selections keep the exact menu blue and need no backdrop. */
 if(frost==16u){for(int j=0;j<rows;j++)for(int i=0;i<cols;i++)hud_glass_grid[j][i]=wash;return;}
 struct HudGlassSum*a=hud_glass_blur[0],*c=hud_glass_blur[1],*d=hud_glass_blur[2];
 hud_glass_filter_row(a,b.x,b.y-HUD_GLASS_STEP,cols);
 hud_glass_filter_row(c,b.x,b.y,cols);
 /* A [1 2 1] blend in each axis combines 3x3 complete 8x8 areas. Read
  * one padded row at a time; retain exact sums until conversion to Q7. */
 for(int j=0;j<rows;j++){
  hud_glass_filter_row(d,b.x,b.y+(j+1)*HUD_GLASS_STEP,cols);
  for(int i=0;i<cols;i++){
   u32 rb=a[i].rb+c[i].rb*2u+d[i].rb,g=a[i].g+c[i].g*2u+d[i].g;
   /* 64 pixels * 16 total weight; Q7 is weighted native sum / 8. */
   struct HudGlassColour average={(rb>>3)&0x0fff0fffu,g>>8};
   hud_glass_grid[j][i]=hud_glass_mix(average,wash,frost);
  }
  struct HudGlassSum*swap=a;a=c;c=d;d=swap;
 }
}
static void hud_glass_scan(struct HudGlassColour*row,int j,int w){
 for(int x=0;x<w;x++){
  int i=x/HUD_GLASS_STEP;unsigned q=(x&(HUD_GLASS_STEP-1))*(16u/HUD_GLASS_STEP);
  row[x]=hud_glass_mix(hud_glass_grid[j][i],hud_glass_grid[j][i+1],q);
 }
}
static void hud_glass_surface(struct HudBox b,u16 tint,u16 edge,unsigned frost,int alert){
 if(b.a<=0||b.w<4||b.h<4||b.w>W||b.h>(HUD_GLASS_ROWS-1)*HUD_GLASS_STEP)return;
 const int r=10;
 int cols=(b.w-1)/HUD_GLASS_STEP+2,rows=(b.h-1)/HUD_GLASS_STEP+2;
 unsigned rim_strength=10u+(5u*(unsigned)alert+128u)/256u;
 int slot=hud_glass_cache_slot(b,rows,cols);
 int reuse=slot>=0&&hud_glass_cache[slot].background==hud_glass_background&&
  hud_glass_cache[slot].box.x==b.x&&hud_glass_cache[slot].box.y==b.y&&
  hud_glass_cache[slot].box.w==b.w&&hud_glass_cache[slot].box.h==b.h&&
  hud_glass_cache[slot].tint==tint&&hud_glass_cache[slot].frost==frost;
 if(reuse){
  for(int j=0;j<rows;j++)for(int i=0;i<cols;i++)hud_glass_grid[j][i]=hud_glass_cache[slot].grid[j][i];
 }else{
  hud_glass_sample(b,tint,frost,rows,cols);
  if(slot>=0){
   hud_glass_cache[slot].background=hud_glass_background;hud_glass_cache[slot].box=b;
   hud_glass_cache[slot].tint=tint;hud_glass_cache[slot].frost=frost;
   for(int j=0;j<rows;j++)for(int i=0;i<cols;i++)hud_glass_cache[slot].grid[j][i]=hud_glass_grid[j][i];
  }
 }
 struct HudGlassColour edge_colour=hud_glass_expand(edge);
 struct HudGlassColour*row_a=hud_glass_row_a,*row_b=hud_glass_row_b;
 int oldj=-1;
 for(int yy=0;yy<b.h;yy++){
  int sy=b.y+yy;if(sy<clip_y||sy>=clip_bottom)continue;
  int j=yy/HUD_GLASS_STEP;
  if(j!=oldj){
   if(oldj>=0&&j==oldj+1){struct HudGlassColour*swap=row_a;row_a=row_b;row_b=swap;hud_glass_scan(row_b,j+1,b.w);}
   else{hud_glass_scan(row_a,j,b.w);hud_glass_scan(row_b,j+1,b.w);}
   oldj=j;
  }
  const u32*bias_row=hud_glass_bias_row(sy);
  unsigned fy=(yy&(HUD_GLASS_STEP-1))*(16u/HUD_GLASS_STEP);
  int dy=yy<b.h-1-yy?yy:b.h-1-yy;
  int first=r,last=b.w-r;
  if(first<clip_x-b.x)first=clip_x-b.x;if(last>clip_right-b.x)last=clip_right-b.x;
  if(dy<2||first>=last)first=last=0;
  for(int xx=0;xx<b.w;xx++){
   if(xx==first&&first<last){
    u16*dest=&fb[sy*W+b.x+first];unsigned q=(16u*(unsigned)b.a+128u)>>8;
    if(q==16){for(int i=first;i<last;i++,dest++)*dest=hud_glass_quantize_row(hud_glass_mix(row_a[i],row_b[i],fy),bias_row,b.x+i);}
    else if(q){for(int i=first;i<last;i++,dest++)*dest=hud_glass_quantize_row(hud_glass_mix(hud_glass_expand(*dest),hud_glass_mix(row_a[i],row_b[i],fy),q),bias_row,b.x+i);}
    xx=last-1;continue;
   }
   int sx=b.x+xx;if(sx<clip_x||sx>=clip_right)continue;
   int dx=xx<b.w-1-xx?xx:b.w-1-xx;
   unsigned outer=hud_corner(dx,dy,r);if(!outer)continue;
   unsigned inner=dx&&dy?hud_corner(dx-1,dy-1,r-1):0;
   struct HudGlassColour colour=hud_glass_mix(row_a[xx],row_b[xx],fy);
   unsigned rim=outer-inner;
   if(rim)colour=hud_glass_mix(colour,edge_colour,(rim*rim_strength+8u)>>4);
   else if(alert&&(dx==1||dy==1))colour=hud_glass_mix(colour,edge_colour,(unsigned)(alert+16)/32u);
   unsigned q=(outer*(unsigned)b.a+128u)>>8;
   u16*p=&fb[sy*W+sx];if(q<16)colour=hud_glass_mix(hud_glass_expand(*p),colour,q);
   *p=hud_glass_quantize_row(colour,bias_row,sx);
  }
 }
}
static void hud_glass(struct HudBox b,int alert){
 u16 tint=theme_resolved?rgb(13,21,33):rgb(247,251,255);
 u16 edge=hud_mix16(theme_resolved?rgb(125,152,184):rgb(128,151,174),rgb(255,43,59),(unsigned)(alert+8)/16u);
 hud_glass_surface(b,tint,edge,theme_resolved?12u:11u,alert);
}
