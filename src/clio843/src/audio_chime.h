/* Bounded integer PCM cue, shared by the real receiver and native verification.
 * Two soft tones, 15 ms envelopes and a gradual music duck/release. */
struct AudioChime {unsigned rate,pos,total,phase,inc1,inc2,ramp;};
static void chime_start(struct AudioChime*c,unsigned rate){c->rate=rate;c->pos=0;c->total=rate/2;c->phase=0;c->inc1=udiv32(880u*65536u,rate);c->inc2=udiv32(1174u*65536u,rate);c->ramp=rate/64;if(!c->ramp)c->ramp=1;}
static int chime_active(const struct AudioChime*c){return c->pos<c->total;}
static int chime_sample(struct AudioChime*c,int*gain){
 static const signed short sine[32]={0,6393,12539,18204,23170,27245,30273,32137,32767,32137,30273,27245,23170,18204,12539,6393,0,-6393,-12539,-18204,-23170,-27245,-30273,-32137,-32767,-32137,-30273,-27245,-23170,-18204,-12539,-6393};
 if(!chime_active(c)){*gain=256;return 0;}unsigned p=c->pos++,r=c->rate,env=256;
 if(p<c->ramp)env=udiv32(p*256,c->ramp);
 if(c->total-p<c->ramp)env=udiv32((c->total-p)*256,c->ramp);
 *gain=256-(int)((env*176)>>8);unsigned begin=0,end=r*14/100;int tone=p<end;
 if(p>=r*22/100&&p<r*42/100){begin=r*22/100;end=r*42/100;tone=1;}
 if(!tone)return 0;unsigned a=p-begin,b=end-p;env=256;
 if(a<c->ramp)env=udiv32(a*256,c->ramp);if(b<c->ramp)env=udiv32(b*256,c->ramp);
 c->phase+=begin?c->inc2:c->inc1;unsigned i=(c->phase>>11)&31,f=c->phase&2047;int wave=sine[i]+((sine[(i+1)&31]-sine[i])*(int)f)/2048;return (wave*(int)env)>>11;
}
static int chime_mix(int sample,int gain,int tone){int v=(sample*gain)/256+tone;return v>32767?32767:v< -32768?-32768:v;}
