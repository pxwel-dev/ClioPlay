/* Bounded RGB565 glass compositor and state transitions.
 * Sample the map before drawing each card. A small interpolated grid softens
 * what shows through, without a second framebuffer or a full-screen blur. */
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
 u32 position_at, content_at;
 int seen;
};
enum {HC_STATUS,HC_CLOCK,HC_SPEED,HC_ROAD,HC_TURN,HC_COMPASS,HC_ROAD_NAME,HC_COUNT};
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
static struct HudBox hud_pose(const struct HudTween*t,u32 now){
 struct HudBox b=t->target.box,a=t->from;
 int q=hud_ease(now-t->position_at,320);
 if(q==256)return b;
 return (struct HudBox){hud_lerp(a.x,b.x,q),hud_lerp(a.y,b.y,q),hud_lerp(a.w,b.w,q),hud_lerp(a.h,b.h,q),hud_lerp(a.a,b.a,q)};
}
static void hud_set_card(int id,struct HudCard target,u32 now){
 struct HudTween*t=&hud_tween[id];
 if(!t->seen){t->seen=1;t->content_at=now-220u;t->position_at=now-320u;}
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
#define HUD_GLASS_STEP 8
#define HUD_GLASS_COLS (W/HUD_GLASS_STEP+2)
#define HUD_GLASS_ROWS 13
static u16 hud_glass_grid[HUD_GLASS_ROWS][HUD_GLASS_COLS];
static u16 hud_glass_row_a[W],hud_glass_row_b[W];
static u16 hud_sample(int x,int y){
 if(x<0)x=0;if(y<0)y=0;if(x>=W)x=W-1;if(y>=H)y=H-1;
 return fb[y*W+x];
}
static void hud_glass_sample(struct HudBox b,u16 tint,unsigned frost,int rows,int cols){
 for(int j=0;j<rows;j++)for(int i=0;i<cols;i++){
  int x=b.x+i*HUD_GLASS_STEP,y=b.y+j*HUD_GLASS_STEP;
  u16 a=hud_mix16(hud_sample(x-4,y-4),hud_sample(x+4,y-4),8);
  u16 c=hud_mix16(hud_sample(x-4,y+4),hud_sample(x+4,y+4),8);
  hud_glass_grid[j][i]=hud_mix16(hud_mix16(a,c,8),tint,frost);
 }
}
static void hud_glass_scan(u16*row,int j,int w){
 for(int x=0;x<w;x++){
  int i=x/HUD_GLASS_STEP;unsigned q=(x&(HUD_GLASS_STEP-1))*2u;
  row[x]=hud_mix16(hud_glass_grid[j][i],hud_glass_grid[j][i+1],q);
 }
}
static void hud_glass_surface(struct HudBox b,u16 tint,u16 edge,unsigned frost,int alert){
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
static void hud_glass(struct HudBox b,int alert){
 u16 tint=theme_resolved?rgb(13,21,33):rgb(247,251,255);
 u16 edge=hud_mix16(theme_resolved?rgb(125,152,184):rgb(128,151,174),rgb(255,43,59),(unsigned)(alert+8)/16u);
 hud_glass_surface(b,tint,edge,theme_resolved?12u:11u,alert);
}
