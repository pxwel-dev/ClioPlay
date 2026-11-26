/* Bounded integer PCM cue, shared by the real receiver and native verification.
 * Kind 1: two soft tones (approach); Kind 2: three brisk tones (overspeed).
 * 15 ms envelopes and gradual music duck/release across 11/22/44 kHz. */
struct AudioChime {unsigned rate,pos,total,phase,inc1,inc2,ramp,kind;};
static void chime_start_kind(struct AudioChime*c,unsigned rate,unsigned kind){
 c->rate=rate;c->pos=0;c->phase=0;c->kind=kind==2u?2u:1u;
 c->total=c->kind==2u?(rate*3u)/5u:rate/2u;
 if(c->kind==2u){
  c->inc1=udiv32(988u*65536u,rate);c->inc2=udiv32(1244u*65536u,rate);
 }else{
  c->inc1=udiv32(880u*65536u,rate);c->inc2=udiv32(1174u*65536u,rate);
 }
 c->ramp=rate/64u;if(!c->ramp)c->ramp=1;
}
static void chime_start(struct AudioChime*c,unsigned rate){chime_start_kind(c,rate,1u);}
static int chime_active(const struct AudioChime*c){return c->pos<c->total;}
static int chime_sample(struct AudioChime*c,int*gain){
 static const signed short sine[32]={0,6393,12539,18204,23170,27245,30273,32137,32767,32137,30273,27245,23170,18204,12539,6393,0,-6393,-12539,-18204,-23170,-27245,-30273,-32137,-32767,-32137,-30273,-27245,-23170,-18204,-12539,-6393};
 if(!chime_active(c)){*gain=256;return 0;}
 unsigned p=c->pos++,r=c->rate,env=256;
 if(p<c->ramp)env=udiv32(p*256u,c->ramp);
 if(c->total-p<c->ramp)env=udiv32((c->total-p)*256u,c->ramp);
 *gain=256-(int)((env*176u)>>8);
 unsigned begin=0,end=0,inc=c->inc1;int tone=0;
 if(c->kind==2u){
  if(p<r*13u/100u){begin=0;end=r*13u/100u;inc=c->inc1;tone=1;}
  else if(p>=r*20u/100u&&p<r*33u/100u){begin=r*20u/100u;end=r*33u/100u;inc=c->inc2;tone=1;}
  else if(p>=r*40u/100u&&p<r*56u/100u){begin=r*40u/100u;end=r*56u/100u;inc=c->inc1;tone=1;}
 }else{
  if(p<r*14u/100u){begin=0;end=r*14u/100u;inc=c->inc1;tone=1;}
  else if(p>=r*22u/100u&&p<r*42u/100u){begin=r*22u/100u;end=r*42u/100u;inc=c->inc2;tone=1;}
 }
 if(!tone)return 0;
 unsigned a=p-begin,b=end-p;env=256;
 if(a<c->ramp)env=udiv32(a*256u,c->ramp);
 if(b<c->ramp)env=udiv32(b*256u,c->ramp);
 c->phase+=inc;
 unsigned i=(c->phase>>11)&31u,f=c->phase&2047u;
 int wave=sine[i]+((sine[(i+1u)&31u]-sine[i])*(int)f)/2048;
 return (wave*(int)env)>>11;
}
static int chime_mix(int sample,int gain,int tone){int v=(sample*gain)/256+tone;return v>32767?32767:v< -32768?-32768:v;}
