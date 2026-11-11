/* Playback position stays outside HudCard so clock ticks never retarget text.
 * Integer-only interpolation; stop extrapolating after two seconds without a
 * changed phone position. Unknown/live-stream durations leave the bar hidden. */
static struct {
 int valid,raw_pos,raw_dur,playing;
 u32 base_ms,sample_at,from,at;
 char title[193];
} hud_playback;
static void hud_playback_reset(void){memzero(&hud_playback,sizeof(hud_playback));}
static u32 hud_playback_ms(u32 now){
 u32 duration=(u32)hud_playback.raw_dur*1000u,ms=hud_playback.base_ms;
 if(hud_playback.playing){
  u32 elapsed=now-hud_playback.sample_at;if(elapsed>2000u)elapsed=2000u;
  ms=duration-ms<elapsed?duration:ms+elapsed;
 }
 return ms;
}
/* Q16 ratio by bounded long division, avoiding 64-bit divides on ARMv4T. */
static u32 hud_playback_ratio(u32 position,u32 duration){
 if(!duration)return 0;if(position>=duration)return 65536;
 u32 q=0,r=position;
 for(int i=0;i<16;i++){
  int carry=(r>>31)!=0;r<<=1;q<<=1;
  if(carry||r>=duration){r-=duration;q++;}
 }
 return q;
}
static u32 hud_playback_fraction(u32 now){
 u32 target=hud_playback_ratio(hud_playback_ms(now),(u32)hud_playback.raw_dur*1000u);
 return (u32)hud_lerp((int)hud_playback.from,(int)target,hud_ease(now-hud_playback.at,320));
}
static void hud_playback_update(u32 now){
 int valid=btui.connected&&bt_source_selected()&&reading_present(btui.title)&&
  btui.dur>0&&btui.dur<=4294967&&btui.pos>=0&&btui.pos<=btui.dur;
 if(!valid){
  if(hud_playback.valid){hud_playback.base_ms=hud_playback_ms(now);hud_playback.playing=0;}
  hud_playback.valid=0;return;
 }
 int track=!streq(hud_playback.title,btui.title);
 int playing=bt_transport_playing(now);
 int sample=btui.pos!=hud_playback.raw_pos||btui.dur!=hud_playback.raw_dur;
 if(!hud_playback.valid||track||sample||playing!=hud_playback.playing){
  u32 from=hud_playback.valid?hud_playback_fraction(now):0;
  u32 base=sample||track||!hud_playback.valid?(u32)btui.pos*1000u:hud_playback_ms(now);
  hud_playback.from=track?0:from;hud_playback.at=now;
  hud_playback.raw_pos=btui.pos;hud_playback.raw_dur=btui.dur;
  hud_playback.base_ms=base;hud_playback.sample_at=now;hud_playback.playing=playing;
  copystr(hud_playback.title,sizeof(hud_playback.title),btui.title);
 }
 hud_playback.valid=1;
}
/* Three-pixel capsule with fractional horizontal ends and 4x4 edge coverage. */
static void hud_playback_strip(int x,int y,int width_q8,u16 ink){
 if(width_q8<=0)return;
 int radius=width_q8<768?width_q8/2:384,cols=(width_q8+255)>>8;
 for(int j=0;j<3;j++)for(int i=0;i<cols;i++){
  int a=16;
  if(i<2||i*256>width_q8-768){
   a=0;
   for(int sy=32;sy<256;sy+=64)for(int sx=32;sx<256;sx+=64){
    int px=i*256+sx,py=j*256+sy;
    if(px>=width_q8)continue;
    int cx=px<radius?radius:px>width_q8-radius?width_q8-radius:px;
    int cy=py<radius?radius:py>768-radius?768-radius:py;
    int dx=px-cx,dy=py-cy;a+=dx*dx+dy*dy<=radius*radius;
   }
  }
  if(a)blend_pixel(x+i,y+j,ink,(a*15+8)>>4);
 }
}
static void hud_draw_playback(int x,int y,int width,u32 now){
 hud_playback_strip(x,y,width*256,C_BORDER);
 hud_playback_strip(x,y,(int)(hud_playback_fraction(now)*(u32)width>>8),C_CYAN);
}
