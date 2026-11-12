/* Decoder equivalence and elapsed-time scheduling; no Bluetooth hardware needed.
 * Synthetic SBC payloads cover the advertised stereo/joint-stereo bitpool range.
 * CRC is not checked by this receiver; these are arithmetic/parser vectors,
 * not a claim of audible quality or physical-unit timing validation. */
#define HOST_TEST 1
#define main receiver_decoder_main
#include "../clio843/src/clioa2dp.c"
#undef main
#include <assert.h>

static u32 random_state=0x1658a729u;
static u32 random_u32(void){u32 x=random_state;x^=x<<13;x^=x>>17;x^=x<<5;return random_state=x;}
static u32 reference_get_bits(const u8*d,u32 pos,int n){u32 out=0;for(int i=0;i<n;i++,pos++)out=(out<<1)|((d[pos>>3]>>(7-(pos&7)))&1u);return out;}
static u32 reference_divide(u32 n,u32 d){return n/d;}
static int reference_unpack(const u8*d,u32 len,struct SbcFrame*f){
  if(len<4||d[0]!=0x9c)return -1;f->frequency=(d[1]>>6)&3;f->blocks=4+4*((d[1]>>4)&3);f->mode=(d[1]>>2)&3;f->channels=f->mode==SBC_MONO?1:2;f->allocation=(d[1]>>1)&1;f->subbands=(d[1]&1)?8:4;f->bitpool=d[2];
  if(f->frequency!=2||f->blocks!=16||f->subbands!=8||(f->mode!=SBC_STEREO&&f->mode!=SBC_JOINT)||f->bitpool<2||f->bitpool>53)return -2;
  u32 consumed=32;f->joint=0;if(f->mode==SBC_JOINT){if(len*8u<consumed+8)return-3;for(int sb=0;sb<7;sb++)if(reference_get_bits(d,consumed+sb,1))f->joint|=(u8)(1u<<sb);consumed+=8;}
  if(len*8u<consumed+64)return-3;for(int ch=0;ch<2;ch++)for(int sb=0;sb<8;sb++){f->scale[ch][sb]=(u8)reference_get_bits(d,consumed,4);consumed+=4;}
  int bits[2][8];sbc_bits(f,bits);
  for(int blk=0;blk<16;blk++)for(int ch=0;ch<2;ch++)for(int sb=0;sb<8;sb++){int nb=bits[ch][sb];if(!nb){f->sb[blk][ch][sb]=0;continue;}if(consumed+(u32)nb>len*8u)return-3;u32 a=reference_get_bits(d,consumed,nb);consumed+=(u32)nb;u32 levels=(1u<<nb)-1u;u32 num=((a<<1)|1u)<<f->scale[ch][sb];f->sb[blk][ch][sb]=(s32)reference_divide(num,levels)-(1<<f->scale[ch][sb]);}
  if(f->mode==SBC_JOINT)for(int blk=0;blk<16;blk++)for(int sb=0;sb<8;sb++)if(f->joint&(1u<<sb)){s32 a=f->sb[blk][0][sb],b=f->sb[blk][1][sb];f->sb[blk][0][sb]=a+b;f->sb[blk][1][sb]=a-b;}
  if(consumed&7u)consumed+=8u-(consumed&7u);return (int)(consumed>>3);
}

static void reference_synth8(struct SbcState*s,struct SbcFrame*f,int ch,int blk){
  s16*off=s->offset[ch];for(int i=0;i<16;i++){off[i]--;if(off[i]<0){off[i]=159;for(int j=0;j<9;j++)s->V[ch][j+160]=s->V[ch][j];}s32 r=madd(synmatrix8[i][7],f->sb[blk][ch][7],0);for(int j=6;j>=0;j--)r=madd(synmatrix8[i][j],f->sb[blk][ch][j],r);s->V[ch][off[i]]=SCALE8(r);}
  for(int i=0,idx=0;i<8;i++,idx+=5){int k=(i+8)&15;s32 r=madd(s->V[ch][off[k]+9],sbc_proto_8_80m1[idx+4],0);r=madd(s->V[ch][off[i]+8],sbc_proto_8_80m0[idx+4],r);r=madd(s->V[ch][off[k]+7],sbc_proto_8_80m1[idx+3],r);r=madd(s->V[ch][off[i]+6],sbc_proto_8_80m0[idx+3],r);r=madd(s->V[ch][off[k]+5],sbc_proto_8_80m1[idx+2],r);r=madd(s->V[ch][off[i]+4],sbc_proto_8_80m0[idx+2],r);r=madd(s->V[ch][off[k]+3],sbc_proto_8_80m1[idx+1],r);r=madd(s->V[ch][off[i]+2],sbc_proto_8_80m0[idx+1],r);r=madd(s->V[ch][off[k]+1],sbc_proto_8_80m1[idx],r);r=madd(s->V[ch][off[i]],sbc_proto_8_80m0[idx],r);f->pcm[ch][blk*8+i]=clip16(SCALE8(r));}
}

static void bit_reader_checks(void){
 for(u32 len=1;len<=128;len++){
  u8*d=malloc(len);assert(d);for(u32 i=0;i<len;i++)d[i]=(u8)random_u32();
  for(u32 pos=0;pos<=len*8;pos++)for(int bits=0;bits<=16&&pos+(u32)bits<=len*8;bits++)
   assert(get_bits(d,pos,bits)==reference_get_bits(d,pos,bits));
  free(d);
 }
}
static void division_checks(void){
 for(int bits=1;bits<=16;bits++){
  u32 d=(1u<<bits)-1;
  /* Exhaustive small values, full-range randomized dividends and multiples. */
  for(u32 n=0;n<65536;n++)assert(sbc_levels_divide(n,bits)==n/d);
  assert(sbc_levels_divide(0xffffffffu,bits)==0xffffffffu/d);
  for(int i=0;i<100000;i++){
   u32 n=random_u32();assert(sbc_levels_divide(n,bits)==n/d);
   u32 multiple=(n/d)*d;assert(sbc_levels_divide(multiple,bits)==multiple/d);
   if(multiple)assert(sbc_levels_divide(multiple-1,bits)==(multiple-1)/d);
   if(multiple<0xffffffffu)assert(sbc_levels_divide(multiple+1,bits)==(multiple+1)/d);
  }
 }
}
static void matrix_checks(void){
 const u32 edge[]={0,1,0xffffffffu,0x80000000u,0x7fffffffu,0x80000001u,0x7ffffffeu};
 for(int trial=0;trial<100000;trial++){
  s32 x[8],out[16];for(int j=0;j<8;j++)x[j]=(s32)(trial<49?edge[(trial+j)%7]:random_u32());
  sbc_matrix8(x,out);
  for(int i=0;i<16;i++){u32 r=0;for(int j=0;j<8;j++)r+=(u32)synmatrix8[i][j]*(u32)x[j];assert((u32)out[i]==r);}
 }
}
static void frame_checks(void){
 struct SbcState old_state={0},new_state={0};
 for(int mode=0;mode<2;mode++)for(int pool=2;pool<=53;pool++)for(int trial=0;trial<12;trial++){
  u8 data[128];for(int i=0;i<128;i++)data[i]=(u8)random_u32();data[0]=0x9c;data[1]=(u8)(mode?0xbd:0xb9);data[2]=(u8)pool;
  struct SbcFrame old={0},cur={0};int length=reference_unpack(data,sizeof(data),&old);
  assert(length>0&&length<=(int)sizeof(data));
  /* Every truncation must reject before touching bytes beyond its allocation. */
  for(int n=0;n<=length;n++){
   u8*bounded=malloc(n?n:1);assert(bounded);memcpy(bounded,data,n);
   struct SbcFrame a={0},b={0};int x=reference_unpack(bounded,(u32)n,&a),y=sbc_unpack(bounded,(u32)n,&b);
   assert(x==y);if(x>0)assert(!memcmp(a.sb,b.sb,sizeof(a.sb)));free(bounded);
  }
  assert(sbc_decode(data,(u32)length,&new_state,&cur)==length);
  if(!old_state.initialized)sbc_state_init(&old_state);
  for(int ch=0;ch<2;ch++)for(int blk=0;blk<16;blk++)reference_synth8(&old_state,&old,ch,blk);
  assert(!memcmp(old.pcm,cur.pcm,sizeof(old.pcm)));
  u8 pcm[512];sbc_pcm_interleave(pcm,&cur);
  for(int i=0;i<128;i++)for(int ch=0;ch<2;ch++){
   u16 sample=(u16)cur.pcm[ch][i];assert(pcm[4*i+2*ch]==(u8)sample);assert(pcm[4*i+2*ch+1]==(u8)(sample>>8));
  }
 }
 u8 invalid[128]={0x9c,0xb9,53};struct SbcFrame f;
 invalid[1]=0x79;assert(sbc_unpack(invalid,sizeof(invalid),&f)==-2);
 invalid[1]=0xb9;invalid[2]=54;assert(sbc_unpack(invalid,sizeof(invalid),&f)==-2);
 invalid[0]=0;assert(sbc_unpack(invalid,sizeof(invalid),&f)==-1);
 /* Full signed 16-bit range, including clipping endpoints, packs unchanged. */
 for(int base=-32768;base<32768;base+=128){
  for(int i=0;i<128;i++){f.pcm[0][i]=(s16)(base+i);f.pcm[1][i]=(s16)(32767-(base+32768+i));}
  u8 pcm[512];sbc_pcm_interleave(pcm,&f);
  for(int i=0;i<128;i++)for(int ch=0;ch<2;ch++)assert((u16)(pcm[4*i+2*ch]|((u16)pcm[4*i+2*ch+1]<<8))==(u16)f.pcm[ch][i]);
 }
}
static void schedule_simulate(u32 packet_ms){
 struct AudioPeriodic maintenance={0},status={0},metadata={0};u32 runs=0,requests=0,tracks=0,ticks=0;
 int pair=600,reconnect=250;u32 pair_at=0,reconnect_at=0;
 for(u32 now=0;now<=60000;now=(now==60000?60001:(now+packet_ms>60000?60000:now+packet_ms))){
  u32 steps=audio_periodic(&maintenance,now,100);if(!steps)continue;
  runs++;ticks+=steps;
  if(audio_periodic(&status,now,1000))requests++;
  if(audio_periodic(&metadata,now,3000))tracks++;
  if(pair>0){pair=audio_countdown(pair,steps);if(!pair)pair_at=now;}
  if(reconnect>0){reconnect=audio_countdown(reconnect,steps);if(!reconnect)reconnect_at=now;}
 }
 assert(runs==601&&ticks==601&&requests==61&&tracks==21);
 assert(pair_at>=59900&&pair_at<59900+packet_ms);
 assert(reconnect_at>=24900&&reconnect_at<24900+packet_ms);
}
static void schedule_checks(void){
 schedule_simulate(1);schedule_simulate(2);schedule_simulate(3);schedule_simulate(7);schedule_simulate(5);schedule_simulate(10);schedule_simulate(100);
 struct AudioPeriodic p={0};assert(audio_periodic_wait(&p,100,100)==0);
 assert(audio_periodic(&p,100,100)==1);assert(audio_periodic_wait(&p,125,100)==75);
 assert(audio_periodic(&p,650,100)==5);assert(audio_periodic(&p,650,100)==0);
 assert(audio_periodic_wait(&p,650,100)==50);assert(audio_periodic(&p,700,100)==1);
 p.at=0xfffffff0u;p.initialized=1;assert(audio_periodic(&p,0x54u,100)==1);
 assert(audio_periodic(&p,0x55u,100)==0);assert(audio_periodic_wait(&p,0x55u,100)==99);
 assert(audio_countdown(10,50)==0&&audio_countdown(-1,50)==-1&&audio_countdown(0,50)==0);
}
int main(void){bit_reader_checks();division_checks();matrix_checks();frame_checks();schedule_checks();puts("Audio decoder: exact division/bit reads/folded synthesis/PCM, bounded frames and packet-independent maintenance passed.");return 0;}
