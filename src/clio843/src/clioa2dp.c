/*
 * CLIOPLAY clioa2dp 1.9 AUTORECONNECT2 EARLYBOOT AUDIOGUARD1 PAIRFIX26 DEBUGGATE - stable A2DP/AVRCP sink with persistent legacy-BlueZ bond storage.
 *
 * Target: ARMv4T / ARM7TDMI, freestanding old-Linux ABI, no libc at runtime.
 * Bluetooth control uses the kernel L2CAP socket ABI directly.  SDP publication
 * is performed by the stock BlueZ 2.x `sdptool add A2SNK` in btstart.sh.
 *
 * The SBC decoder below is a deliberately small 8-subband decoder derived from
 * the classic BlueZ SBC implementation (LGPL-2.1-or-later).  The A2DP endpoint
 * advertises the validated 44.1 kHz stereo/joint-stereo family:
 * 16 blocks, 8 subbands, loudness allocation, bitpool 2..53.
 */

typedef unsigned char u8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;

#ifndef HOST_TEST
static inline long sc0(long nr){ register long r0 __asm__("r0"); __asm__ volatile("svc %1":"=r"(r0):"i"(0x900000+nr):"memory","cc"); return r0; }
static inline long sc1(long nr,long a){ register long r0 __asm__("r0")=a; __asm__ volatile("svc %1":"+r"(r0):"i"(0x900000+nr):"memory","cc"); return r0; }
static inline long sc2(long nr,long a,long b){ register long r0 __asm__("r0")=a; register long r1 __asm__("r1")=b; __asm__ volatile("svc %2":"+r"(r0):"r"(r1),"i"(0x900000+nr):"memory","cc"); return r0; }
static inline long sc3(long nr,long a,long b,long c){ register long r0 __asm__("r0")=a; register long r1 __asm__("r1")=b; register long r2 __asm__("r2")=c; __asm__ volatile("svc %3":"+r"(r0):"r"(r1),"r"(r2),"i"(0x900000+nr):"memory","cc"); return r0; }
static inline long sc6(long nr,long a,long b,long c,long d,long e,long f){ register long r0 __asm__("r0")=a; register long r1 __asm__("r1")=b; register long r2 __asm__("r2")=c; register long r3 __asm__("r3")=d; register long r4 __asm__("r4")=e; register long r5 __asm__("r5")=f; __asm__ volatile("svc %6":"+r"(r0):"r"(r1),"r"(r2),"r"(r3),"r"(r4),"r"(r5),"i"(0x900000+nr):"memory","cc"); return r0; }
static long s_read(int fd,void*p,unsigned n){return sc3(3,fd,(long)p,(long)n);}
static long s_write(int fd,const void*p,unsigned n){return sc3(4,fd,(long)p,(long)n);}
static long s_open(const char*p,int f,int m){return sc3(5,(long)p,f,m);}
static long s_close(int fd){return sc1(6,fd);}
static long s_rename(const char*a,const char*b){return sc2(38,(long)a,(long)b);}
static long s_ioctl(int fd,u32 req,long arg){return sc3(54,fd,(long)req,arg);}
static long s_poll(void*p,u32 n,int timeout){return sc3(168,(long)p,(long)n,timeout);}
/* Legacy ARM Linux setpriority(2): PRIO_PROCESS=0, who=0 (self).  Best effort: no RT scheduling. */
static long s_audio_priority(void){return sc3(97,0,0,-4);}
static void*s_mmap2(void*addr,u32 len,int prot,int flags,int fd,u32 pgoff){long r=sc6(192,(long)addr,(long)len,prot,flags,fd,(long)pgoff);return (u32)r>=0xfffff001u?0:(void*)r;}
static long s_munmap(void*p,u32 len){return sc2(91,(long)p,(long)len);}
static long s_nanosleep(long sec,long ns){struct {long sec,nsec;}t={sec,ns};return sc2(162,(long)&t,0);}
struct timeval0{long tv_sec,tv_usec;};
static long s_gettimeofday(struct timeval0*t){return sc2(78,(long)t,0);}
__attribute__((noreturn)) static void s_exit(int c){sc1(1,c);for(;;){}}
static long s_socketcall(int call,unsigned long*a){return sc2(102,call,(long)a);}
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

enum { O_RDONLY_=0,O_WRONLY_=1,O_RDWR_=2,O_CREAT_=64,O_TRUNC_=512,O_APPEND_=1024,O_NONBLOCK_=2048 };
enum { PROT_READ_=1,PROT_WRITE_=2,MAP_SHARED_=1 };
#define SOUND_PATH "/dev/barc-snd"
#define BT_ACTIVE_PATH "/tmp/clbt"
#define CAMERA_ALERT_PATH "/tmp/clalert"
#define BT_STATUS_PATH "/tmp/clbtstat"
#define BT_LOG_PATH "/media/sdcard/clio843/bta2dp.log"
#define BT_EVENT_LOG_PATH "/media/sdcard/clio843/bt.log"
#define DEBUG_STATE_PATH "/tmp/cldebug"
#define PREF_PATH "/media/sdcard/clio843/prefs.ini"

static void memzero(void*p,u32 n){u8*d=(u8*)p;while(n--)*d++=0;}
static void memcopy(void*d0,const void*s0,u32 n){u8*d=(u8*)d0;const u8*s=(const u8*)s0;while(n--)*d++=*s++;}
static u32 slen(const char*s){u32 n=0;while(s&&s[n])n++;return n;}
static int streq(const char*a,const char*b){u32 i=0;while(a[i]&&b[i]){if(a[i]!=b[i])return 0;i++;}return a[i]==b[i];}
#include "utf8.h"
static void copystr(char*d,u32 cap,const char*s){utf8_copy(d,cap,s,0xffffffffu);}
static u32 udiv32(u32 n,u32 d){u32 q=0,r=0;if(!d)return 0;for(int i=31;i>=0;i--){r=(r<<1)|((n>>i)&1u);if(r>=d){r-=d;q|=1u<<i;}}return q;}
static s32 sdiv32(s32 n,s32 d){int neg=(n<0)^(d<0);u32 a=n<0?(u32)(-(n+1))+1u:(u32)n,b=d<0?(u32)(-(d+1))+1u:(u32)d;u32 q=udiv32(a,b);return neg?-(s32)q:(s32)q;}
#ifndef HOST_TEST
/* Prevent LLVM loop idiom lowering from creating libc dependencies. */
void *memset(void*d,int c,u32 n){u8*p=(u8*)d;while(n--)*p++=(u8)c;return d;}
void *memcpy(void*d,const void*s,u32 n){memcopy(d,s,n);return d;}
/* ARM EABI compiler helper: quotient in r0, remainder in r1. */
__attribute__((naked)) void __aeabi_uidivmod(void){__asm__ volatile(
  "cmp r1,#0\n"
  "beq 2f\n"
  "mov r2,#0\n"
  "mov r3,#0\n"
  "mov r12,#32\n"
  "1:\n"
  "mov r3,r3,lsl #1\n"
  "orr r3,r3,r0,lsr #31\n"
  "mov r0,r0,lsl #1\n"
  "cmp r3,r1\n"
  "subcs r3,r3,r1\n"
  "adc r2,r2,r2\n"
  "subs r12,r12,#1\n"
  "bne 1b\n"
  "mov r0,r2\n"
  "mov r1,r3\n"
  "bx lr\n"
  "2:\n"
  "mov r0,#0\n"
  "mov r1,#0\n"
  "bx lr\n");}
#endif

/* ---------------- SBC decoder (8 subbands, stereo/joint stereo) ----------- */
#define ASR32(v,b) ((s32)(v)>>(b))
#define SS8(v) ASR32((s32)(u32)(v),14)
#define SN8(v) ASR32((s32)(u32)(v),11)
#define SCALE8(v) ASR32((s32)(v),15)

static const s32 sbc_proto_8_80m0[40]={
SS8(0x00000000u),SS8(0xfe8d1970u),SS8(0xee979f00u),SS8(0x11686100u),SS8(0x0172e690u),SS8(0xfff5bd1au),SS8(0xfdf1c8d4u),SS8(0xeac182c0u),
SS8(0x0d9daee0u),SS8(0x00e530dau),SS8(0xffe9811du),SS8(0xfd52986cu),SS8(0xe7054ca0u),SS8(0x0a00d410u),SS8(0x006c1de4u),SS8(0xffdba705u),
SS8(0xfcbc98e8u),SS8(0xe3889d20u),SS8(0x06af2308u),SS8(0x000bb7dbu),SS8(0xffca00edu),SS8(0xfc3fbb68u),SS8(0xe071bc00u),SS8(0x03bf7948u),
SS8(0xffc4e05cu),SS8(0xffb54b3bu),SS8(0xfbedadc0u),SS8(0xdde26200u),SS8(0x0142291cu),SS8(0xff960e94u),SS8(0xff9f3e17u),SS8(0xfbd8f358u),
SS8(0xdbf79400u),SS8(0xff405e01u),SS8(0xff7d4914u),SS8(0xff8b1a31u),SS8(0xfc1417b8u),SS8(0xdac7bb40u),SS8(0xfdbb828cu),SS8(0xff762170u)};
static const s32 sbc_proto_8_80m1[40]={
SS8(0xff7c272cu),SS8(0xfcb02620u),SS8(0xda612700u),SS8(0xfcb02620u),SS8(0xff7c272cu),SS8(0xff762170u),SS8(0xfdbb828cu),SS8(0xdac7bb40u),
SS8(0xfc1417b8u),SS8(0xff8b1a31u),SS8(0xff7d4914u),SS8(0xff405e01u),SS8(0xdbf79400u),SS8(0xfbd8f358u),SS8(0xff9f3e17u),SS8(0xff960e94u),
SS8(0x0142291cu),SS8(0xdde26200u),SS8(0xfbedadc0u),SS8(0xffb54b3bu),SS8(0xffc4e05cu),SS8(0x03bf7948u),SS8(0xe071bc00u),SS8(0xfc3fbb68u),
SS8(0xffca00edu),SS8(0x000bb7dbu),SS8(0x06af2308u),SS8(0xe3889d20u),SS8(0xfcbc98e8u),SS8(0xffdba705u),SS8(0x006c1de4u),SS8(0x0a00d410u),
SS8(0xe7054ca0u),SS8(0xfd52986cu),SS8(0xffe9811du),SS8(0x00e530dau),SS8(0x0d9daee0u),SS8(0xeac182c0u),SS8(0xfdf1c8d4u),SS8(0xfff5bd1au)};
static const s32 synmatrix8[16][8]={
{SN8(0x05a82798u),SN8(0xfa57d868u),SN8(0xfa57d868u),SN8(0x05a82798u),SN8(0x05a82798u),SN8(0xfa57d868u),SN8(0xfa57d868u),SN8(0x05a82798u)},
{SN8(0x0471ced0u),SN8(0xf8275a10u),SN8(0x018f8b84u),SN8(0x06a6d988u),SN8(0xf9592678u),SN8(0xfe70747cu),SN8(0x07d8a5f0u),SN8(0xfb8e3130u)},
{SN8(0x030fbc54u),SN8(0xf89be510u),SN8(0x07641af0u),SN8(0xfcf043acu),SN8(0xfcf043acu),SN8(0x07641af0u),SN8(0xf89be510u),SN8(0x030fbc54u)},
{SN8(0x018f8b84u),SN8(0xfb8e3130u),SN8(0x06a6d988u),SN8(0xf8275a10u),SN8(0x07d8a5f0u),SN8(0xf9592678u),SN8(0x0471ced0u),SN8(0xfe70747cu)},
{0,0,0,0,0,0,0,0},
{SN8(0xfe70747cu),SN8(0x0471ced0u),SN8(0xf9592678u),SN8(0x07d8a5f0u),SN8(0xf8275a10u),SN8(0x06a6d988u),SN8(0xfb8e3130u),SN8(0x018f8b84u)},
{SN8(0xfcf043acu),SN8(0x07641af0u),SN8(0xf89be510u),SN8(0x030fbc54u),SN8(0x030fbc54u),SN8(0xf89be510u),SN8(0x07641af0u),SN8(0xfcf043acu)},
{SN8(0xfb8e3130u),SN8(0x07d8a5f0u),SN8(0xfe70747cu),SN8(0xf9592678u),SN8(0x06a6d988u),SN8(0x018f8b84u),SN8(0xf8275a10u),SN8(0x0471ced0u)},
{SN8(0xfa57d868u),SN8(0x05a82798u),SN8(0x05a82798u),SN8(0xfa57d868u),SN8(0xfa57d868u),SN8(0x05a82798u),SN8(0x05a82798u),SN8(0xfa57d868u)},
{SN8(0xf9592678u),SN8(0x018f8b84u),SN8(0x07d8a5f0u),SN8(0x0471ced0u),SN8(0xfb8e3130u),SN8(0xf8275a10u),SN8(0xfe70747cu),SN8(0x06a6d988u)},
{SN8(0xf89be510u),SN8(0xfcf043acu),SN8(0x030fbc54u),SN8(0x07641af0u),SN8(0x07641af0u),SN8(0x030fbc54u),SN8(0xfcf043acu),SN8(0xf89be510u)},
{SN8(0xf8275a10u),SN8(0xf9592678u),SN8(0xfb8e3130u),SN8(0xfe70747cu),SN8(0x018f8b84u),SN8(0x0471ced0u),SN8(0x06a6d988u),SN8(0x07d8a5f0u)},
{SN8(0xf8000000u),SN8(0xf8000000u),SN8(0xf8000000u),SN8(0xf8000000u),SN8(0xf8000000u),SN8(0xf8000000u),SN8(0xf8000000u),SN8(0xf8000000u)},
{SN8(0xf8275a10u),SN8(0xf9592678u),SN8(0xfb8e3130u),SN8(0xfe70747cu),SN8(0x018f8b84u),SN8(0x0471ced0u),SN8(0x06a6d988u),SN8(0x07d8a5f0u)},
{SN8(0xf89be510u),SN8(0xfcf043acu),SN8(0x030fbc54u),SN8(0x07641af0u),SN8(0x07641af0u),SN8(0x030fbc54u),SN8(0xfcf043acu),SN8(0xf89be510u)},
{SN8(0xf9592678u),SN8(0x018f8b84u),SN8(0x07d8a5f0u),SN8(0x0471ced0u),SN8(0xfb8e3130u),SN8(0xf8275a10u),SN8(0xfe70747cu),SN8(0x06a6d988u)}};
static const int sbc_offset8[4][8]={{-2,0,0,0,0,0,0,1},{-3,0,0,0,0,0,1,2},{-4,0,0,0,0,0,1,2},{-4,0,0,0,0,0,1,2}};

enum { SBC_MONO=0,SBC_DUAL=1,SBC_STEREO=2,SBC_JOINT=3 };
struct SbcFrame {int frequency,blocks,mode,channels,allocation,subbands,bitpool;u8 joint;u8 scale[2][8];s32 sb[16][2][8];s16 pcm[2][128];};
struct SbcState {s32 V[2][170];s16 offset[2][16];int initialized;};

static u32 get_bits(const u8*d,u32 bitpos,int n){u32 v=0;for(int i=0;i<n;i++){v=(v<<1)|((d[bitpos>>3]>>(7-(bitpos&7)))&1u);bitpos++;}return v;}
static void sbc_bits(struct SbcFrame*f,int bits[2][8]){
  int bitneed[2][8],maxneed=0,bitcount=0,slicecount=0,bitslice,ch,sb;
  for(ch=0;ch<2;ch++)for(sb=0;sb<8;sb++){
    int n;if(f->allocation){n=f->scale[ch][sb];}else if(f->scale[ch][sb]==0)n=-5;else{int loud=f->scale[ch][sb]-sbc_offset8[f->frequency][sb];n=loud>0?loud/2:loud;}
    bitneed[ch][sb]=n;if(n>maxneed)maxneed=n;
  }
  bitslice=maxneed+1;
  do{bitslice--;bitcount+=slicecount;slicecount=0;for(ch=0;ch<2;ch++)for(sb=0;sb<8;sb++){int n=bitneed[ch][sb];if(n>bitslice+1&&n<bitslice+16)slicecount++;else if(n==bitslice+1)slicecount+=2;}}while(bitcount+slicecount<f->bitpool);
  if(bitcount+slicecount==f->bitpool){bitcount+=slicecount;bitslice--;}
  for(ch=0;ch<2;ch++)for(sb=0;sb<8;sb++){int n=bitneed[ch][sb];bits[ch][sb]=n<bitslice+2?0:n-bitslice;if(bits[ch][sb]>16)bits[ch][sb]=16;}
  ch=0;sb=0;while(bitcount<f->bitpool&&sb<8){if(bits[ch][sb]>=2&&bits[ch][sb]<16){bits[ch][sb]++;bitcount++;}else if(bitneed[ch][sb]==bitslice+1&&f->bitpool>bitcount+1){bits[ch][sb]=2;bitcount+=2;}if(ch){ch=0;sb++;}else ch=1;}
  ch=0;sb=0;while(bitcount<f->bitpool&&sb<8){if(bits[ch][sb]<16){bits[ch][sb]++;bitcount++;}if(ch){ch=0;sb++;}else ch=1;}
}
static int sbc_unpack(const u8*d,u32 len,struct SbcFrame*f){
  if(len<4||d[0]!=0x9c)return -1;f->frequency=(d[1]>>6)&3;f->blocks=4+4*((d[1]>>4)&3);f->mode=(d[1]>>2)&3;f->channels=f->mode==SBC_MONO?1:2;f->allocation=(d[1]>>1)&1;f->subbands=(d[1]&1)?8:4;f->bitpool=d[2];
  if(f->frequency!=2||f->blocks!=16||f->subbands!=8||(f->mode!=SBC_STEREO&&f->mode!=SBC_JOINT)||f->bitpool<2||f->bitpool>53)return -2;
  u32 consumed=32;f->joint=0;if(f->mode==SBC_JOINT){if(len*8u<consumed+8)return-3;for(int sb=0;sb<7;sb++)if(get_bits(d,consumed+sb,1))f->joint|=(u8)(1u<<sb);consumed+=8;}
  if(len*8u<consumed+64)return-3;for(int ch=0;ch<2;ch++)for(int sb=0;sb<8;sb++){f->scale[ch][sb]=(u8)get_bits(d,consumed,4);consumed+=4;}
  int bits[2][8];sbc_bits(f,bits);
  for(int blk=0;blk<16;blk++)for(int ch=0;ch<2;ch++)for(int sb=0;sb<8;sb++){int nb=bits[ch][sb];if(!nb){f->sb[blk][ch][sb]=0;continue;}if(consumed+(u32)nb>len*8u)return-3;u32 a=get_bits(d,consumed,nb);consumed+=(u32)nb;u32 levels=(1u<<nb)-1u;u32 num=((a<<1)|1u)<<f->scale[ch][sb];f->sb[blk][ch][sb]=(s32)udiv32(num,levels)-(1<<f->scale[ch][sb]);}
  if(f->mode==SBC_JOINT)for(int blk=0;blk<16;blk++)for(int sb=0;sb<8;sb++)if(f->joint&(1u<<sb)){s32 a=f->sb[blk][0][sb],b=f->sb[blk][1][sb];f->sb[blk][0][sb]=a+b;f->sb[blk][1][sb]=a-b;}
  if(consumed&7u)consumed+=8u-(consumed&7u);return (int)(consumed>>3);
}
static s16 clip16(s32 v){return v>32767?32767:(v<-32768?-32768:(s16)v);}
static s32 madd(s32 a,s32 b,s32 r){return (s32)((u32)(a*b)+(u32)r);} /* intentional fixed-point wrap */
static void sbc_state_init(struct SbcState*s){memzero(s,sizeof(*s));for(int ch=0;ch<2;ch++)for(int i=0;i<16;i++)s->offset[ch][i]=(s16)(10*i+10);s->initialized=1;}
static void sbc_synth8(struct SbcState*s,struct SbcFrame*f,int ch,int blk){
  s16*off=s->offset[ch];for(int i=0;i<16;i++){off[i]--;if(off[i]<0){off[i]=159;for(int j=0;j<9;j++)s->V[ch][j+160]=s->V[ch][j];}s32 r=synmatrix8[i][7]*f->sb[blk][ch][7];for(int j=6;j>=0;j--)r=madd(synmatrix8[i][j],f->sb[blk][ch][j],r);s->V[ch][off[i]]=SCALE8(r);}
  for(int i=0,idx=0;i<8;i++,idx+=5){int k=(i+8)&15;s32 r=s->V[ch][off[k]+9]*sbc_proto_8_80m1[idx+4];r=madd(s->V[ch][off[i]+8],sbc_proto_8_80m0[idx+4],r);r=madd(s->V[ch][off[k]+7],sbc_proto_8_80m1[idx+3],r);r=madd(s->V[ch][off[i]+6],sbc_proto_8_80m0[idx+3],r);r=madd(s->V[ch][off[k]+5],sbc_proto_8_80m1[idx+2],r);r=madd(s->V[ch][off[i]+4],sbc_proto_8_80m0[idx+2],r);r=madd(s->V[ch][off[k]+3],sbc_proto_8_80m1[idx+1],r);r=madd(s->V[ch][off[i]+2],sbc_proto_8_80m0[idx+1],r);r=madd(s->V[ch][off[k]+1],sbc_proto_8_80m1[idx],r);r=madd(s->V[ch][off[i]],sbc_proto_8_80m0[idx],r);f->pcm[ch][blk*8+i]=clip16(SCALE8(r));}
}
static int sbc_decode(const u8*d,u32 len,struct SbcState*s,struct SbcFrame*f){int n=sbc_unpack(d,len,f);if(n<0)return n;if(!s->initialized)sbc_state_init(s);for(int ch=0;ch<2;ch++)for(int blk=0;blk<16;blk++)sbc_synth8(s,f,ch,blk);return n;}

#ifndef HOST_TEST
/* -------------------------------------------------------------------------
 * clioa2dp 1.9 / A2DP3 STABLE + AVRCP SDP + MULTI-DEVICE BOND MANAGER
 *
 * Bluetooth ownership rules:
 *   - hcid/sdpd and the Renault HID hidd are owned by btstart.sh.
 *   - this daemon never resets hci0 and never touches the HID PSMs.
 *   - incoming AVDTP uses authenticated+encrypted L2CAP on PSM 0x19.
 *   - PAIRFIX25 retains the real AVRCP Controller SDP record, persistent profile-first reconnect, and stable remembered names.
 *   - Explicit PAIR uses the proven legacy PIN 0000. RECONNECT never enters pairing mode.
 *   - Phone link keys are captured from HCI LINK_KEY_NOTIFICATION and persisted by CLIOPLAY for up to eight remembered devices.
 *   - Before reconnect, the saved key is restored into TomTom's legacy hcid link-key database.
 *   - CONNECT never enables pairing mode; a failed remembered-bond reconnect reports REPAIR REQUIRED.
 *   - UNPAIR remains the only operation that clears bond state.
 *   - address-specific HCI actions (pair scan / disconnect / delkey) are
 *     requested through /tmp/clbthci and performed by btstart.sh's tiny helper.
 *
 * UI IPC matches clio040 DEV5.34 MEDIA1:
 *   /tmp/clbt       selected logical BLUETOOTH (native CD) source, 0/1
 *   /tmp/clbtstat   one-line status
 *   /tmp/clbtmeta   DEVICE/TITLE/ARTIST/ALBUM/PAIRED/CONNECTED/PLAYING/POS/DUR
 *   /tmp/clnav      validated CLP2 phone destination for clio040
 *   /tmp/clbtcmd    monotonically sequenced commands from clio040
 * ------------------------------------------------------------------------- */
#define BT_META_PATH "/tmp/clbtmeta"
#define BT_NAV_PATH "/tmp/clnav"
#define BT_NAV_TMP_PATH "/tmp/clnav.tmp"
#define BT_CMD_PATH "/tmp/clbtcmd"
#define BT_HCI_PATH "/tmp/clbthci"
#define BT_PHONE_PATH "/media/sdcard/clio843/btphone.dat"
#define BT_PIN_PATH "/tmp/clbtpin"
#define BT_LOCAL_PATH "/tmp/clbtlocal"
#define BT_BOOT_READY_PATH "/tmp/clbtrdy"
#define BT_BONDS_PATH "/media/sdcard/clio843/btbonds.dat"
#define BT_NAMES_PATH "/media/sdcard/clio843/btnames.dat"
#define BT_DEVLIST_PATH "/tmp/clbtdevices"
#define HCID_KEY_DB "/etc/bluetooth/link_key"

static long s_unlink(const char*p){return sc1(10,(long)p);}
static long s_fcntl(int fd,int cmd,long arg){return sc3(55,fd,cmd,arg);}

static char status_last[28];
static int meta_dirty=1,meta_paired=0,meta_connected=0,meta_playing=0,meta_pos=0,meta_dur=0;
static char meta_device[33],meta_title[193],meta_artist[161],meta_album[161];
static int hcifd=-1;static u8 live_peer[6];static int live_peer_valid=0;
static u32 command_seq_seen=0;

static void append_char(char*b,u32 cap,char c){u32 n=slen(b);if(n+1<cap){b[n]=c;b[n+1]=0;}}
static void append_str(char*b,u32 cap,const char*s){u32 n=slen(b),i=0;while(n+1<cap&&s&&s[i])b[n++]=s[i++];b[n]=0;}
static void append_uint(char*b,u32 cap,u32 v){char t[12];int n=0;if(!v){append_char(b,cap,'0');return;}while(v&&n<11){t[n++]=(char)('0'+v%10u);v=udiv32(v,10u);}while(n--)append_char(b,cap,t[n]);}
static void append_sint(char*b,u32 cap,s32 v){if(v<0){append_char(b,cap,'-');append_uint(b,cap,(u32)(-(v+1))+1u);}else append_uint(b,cap,(u32)v);}
static int hexval0(char c){if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return-1;}
static int parse_dec_span(const char*s,const char*e,s32*out){int neg=0;u32 v=0;if(s<e&&*s=='-'){neg=1;s++;}if(s>=e)return 0;for(;s<e;s++){if(*s<'0'||*s>'9')return 0;v=v*10u+(u32)(*s-'0');if(v>200000000u)return 0;}*out=neg?-(s32)v:(s32)v;return 1;}
static int parse_hex16_span(const char*s,const char*e,u16*out){u32 v=0;if(e-s!=4)return 0;for(;s<e;s++){int h=hexval0(*s);if(h<0)return 0;v=(v<<4)|(u32)h;}*out=(u16)v;return 1;}
static u16 crc16_ccitt_text(const char*s,u32 n){u16 crc=0xffffu;for(u32 i=0;i<n;i++){crc^=(u16)((u8)s[i]<<8);for(int bit=0;bit<8;bit++)crc=(u16)((crc&0x8000u)?((crc<<1)^0x1021u):(crc<<1));}return crc;}
static int nav_packet_parse(const char*t,char*action,s32*lat,s32*lon,u16*seq){if(!t||t[0]!='C'||t[1]!='L'||t[2]!='P'||t[3]!='2'||t[4]!=':'||(t[5]!='N'&&t[5]!='S')||t[6]!=':')return 0;const char*c1=t+6,*c2=0,*c3=0,*c4=0;for(const char*p=c1+1;*p;p++){if(*p==':'){if(!c2)c2=p;else if(!c3)c3=p;else if(!c4)c4=p;else return 0;}}if(!c2||!c3||!c4||!c4[1])return 0;s32 la=0,lo=0;u16 sq=0,want=0;if(!parse_dec_span(c1+1,c2,&la)||!parse_dec_span(c2+1,c3,&lo)||!parse_hex16_span(c3+1,c4,&sq)||!parse_hex16_span(c4+1,t+slen(t),&want))return 0;if(la<-9000000||la>9000000||lo<-18000000||lo>18000000)return 0;u16 got=crc16_ccitt_text(t,(u32)(c4-t));if(got!=want)return 0;*action=t[5];*lat=la;*lon=lo;*seq=sq;return 1;}
static void file_write(const char*path,const char*s);
static void bt_log_append(const char*s);
static u16 nav_last_seq=0;static int nav_last_seq_valid=0;
static void nav_publish(char action,s32 lat,s32 lon,u16 seq,const char*name){if(nav_last_seq_valid&&nav_last_seq==seq)return;char b[384];b[0]=0;append_str(b,sizeof(b),"SEQ=");append_uint(b,sizeof(b),(u32)seq);append_char(b,sizeof(b),'\n');append_str(b,sizeof(b),"ACTION=");append_char(b,sizeof(b),action);append_char(b,sizeof(b),'\n');append_str(b,sizeof(b),"LAT=");append_sint(b,sizeof(b),lat);append_char(b,sizeof(b),'\n');append_str(b,sizeof(b),"LON=");append_sint(b,sizeof(b),lon);append_char(b,sizeof(b),'\n');append_str(b,sizeof(b),"NAME=");append_str(b,sizeof(b),(name&&name[0])?name:"PHONE DESTINATION");append_char(b,sizeof(b),'\n');file_write(BT_NAV_TMP_PATH,b);if(s_rename(BT_NAV_TMP_PATH,BT_NAV_PATH)<0){file_write(BT_NAV_PATH,b);s_unlink(BT_NAV_TMP_PATH);}nav_last_seq=seq;nav_last_seq_valid=1;bt_log_append("NAV RX ACCEPTED\n");}
static int parse_uint0(const char*s,u32*out){u32 v=0;int any=0;while(*s>='0'&&*s<='9'){any=1;v=v*10u+(u32)(*s-'0');s++;}if(out)*out=v;return any;}
static int starts0(const char*s,const char*p){u32 i=0;while(p[i]){if(s[i]!=p[i])return 0;i++;}return 1;}
static void file_write(const char*path,const char*s){long fd=s_open(path,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd>=0){s_write((int)fd,s,slen(s));s_close((int)fd);}}
static void file_append(const char*path,const char*s){long fd=s_open(path,O_WRONLY_|O_CREAT_|O_APPEND_,0666);if(fd>=0){s_write((int)fd,s,slen(s));s_close((int)fd);}}
/* CLIOPLAY owns the user-facing Debug toggle.  clio040 mirrors it into /tmp so
 * this daemon can honour changes immediately without repeatedly reading the SD
 * prefs file.  Before clio040 has started, prefs.ini is a safe boot fallback. */
static int debug_enabled(void){char b[4]={0};long fd=s_open(DEBUG_STATE_PATH,O_RDONLY_,0);if(fd>=0){long n=s_read((int)fd,b,3);s_close((int)fd);return n>0&&b[0]=='1';}char p[512];fd=s_open(PREF_PATH,O_RDONLY_,0);if(fd<0)return 0;long n=s_read((int)fd,p,sizeof(p)-1);s_close((int)fd);if(n<=0)return 0;p[n]=0;for(long i=0;i<n;i++)if((i==0||p[i-1]=='\n'||p[i-1]=='\r')&&i+6<n&&p[i]=='d'&&p[i+1]=='e'&&p[i+2]=='b'&&p[i+3]=='u'&&p[i+4]=='g'&&p[i+5]=='=')return p[i+6]=='1';return 0;}
static void bt_log_append(const char*s){if(debug_enabled())file_append(BT_EVENT_LOG_PATH,s);}
static void status_set(const char*s){if(streq(status_last,s))return;copystr(status_last,sizeof(status_last),s);file_write(BT_STATUS_PATH,s);meta_dirty=1;}
static int source_enabled(void){char b[4]={0};long fd=s_open(BT_ACTIVE_PATH,O_RDONLY_,0);if(fd<0)return 0;long n=s_read((int)fd,b,3);s_close((int)fd);return n>0&&b[0]=='1';}
/* Refresh once per daemon iteration, never once per decoded SBC frame. */
static int source_active=0;
static void meta_text(char*d,u32 cap,const u8*s,u32 n){u32 at=0,out=0;while(at<n&&out+1<cap){unsigned c=s[at],bytes=c<128?1:c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;if(!bytes||bytes>n-at){at++;continue;}char tmp[5];for(unsigned i=0;i<bytes;i++)tmp[i]=(char)s[at+i];tmp[bytes]=0;if(!utf8_span(tmp)){at++;continue;}if(c<32||c==127){at+=bytes;continue;}if(out+bytes>=cap)break;for(unsigned i=0;i<bytes;i++)d[out++]=(char)s[at++];}d[out]=0;}
static void meta_clear_track(void){meta_title[0]=meta_artist[0]=meta_album[0]=0;meta_pos=meta_dur=0;meta_playing=0;meta_dirty=1;}
/* Mobile browsers can briefly expose the PWA/document title as AVRCP metadata
 * while the silent ClioPlay Send transport session is being activated.  That
 * title is transport plumbing, not user media, so never publish it to clio040. */
static int nav_sender_fallback_meta(void){return streq(meta_title,"ClioPlay Send");}
static void meta_hide_track(void){meta_title[0]=meta_artist[0]=meta_album[0]=0;meta_pos=meta_dur=0;meta_dirty=1;}
static void meta_write(void){char b[1024];b[0]=0;append_str(b,sizeof(b),"DEVICE=");append_str(b,sizeof(b),meta_device);append_char(b,sizeof(b),'\n');append_str(b,sizeof(b),"TITLE=");append_str(b,sizeof(b),meta_title);append_char(b,sizeof(b),'\n');append_str(b,sizeof(b),"ARTIST=");append_str(b,sizeof(b),meta_artist);append_char(b,sizeof(b),'\n');append_str(b,sizeof(b),"ALBUM=");append_str(b,sizeof(b),meta_album);append_char(b,sizeof(b),'\n');append_str(b,sizeof(b),"PAIRED=");append_char(b,sizeof(b),meta_paired?'1':'0');append_char(b,sizeof(b),'\n');append_str(b,sizeof(b),"CONNECTED=");append_char(b,sizeof(b),meta_connected?'1':'0');append_char(b,sizeof(b),'\n');append_str(b,sizeof(b),"PLAYING=");append_char(b,sizeof(b),meta_playing?'1':'0');append_char(b,sizeof(b),'\n');append_str(b,sizeof(b),"POS=");append_uint(b,sizeof(b),(u32)(meta_pos<0?0:meta_pos));append_char(b,sizeof(b),'\n');append_str(b,sizeof(b),"DUR=");append_uint(b,sizeof(b),(u32)(meta_dur<0?0:meta_dur));append_char(b,sizeof(b),'\n');file_write(BT_META_PATH,b);meta_dirty=0;}

/* Bluetooth addresses are stored in kernel byte order; display is reversed. */
static const char HX[]="0123456789ABCDEF";
static void append_hex8(char*b,u32 cap,u8 v){append_char(b,cap,HX[(v>>4)&15]);append_char(b,cap,HX[v&15]);}
static void addr_text(const u8 a[6],char out[18]){int p=0;for(int i=5;i>=0;i--){out[p++]=HX[(a[i]>>4)&15];out[p++]=HX[a[i]&15];if(i)out[p++]=':';}out[p]=0;}
static int hex0(char c){if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return-1;}
static int addr_parse(const char*s,u8 out[6]){u8 fwd[6];for(int i=0;i<6;i++){int a=hex0(s[i*3]),b=hex0(s[i*3+1]);if(a<0||b<0)return 0;fwd[i]=(u8)((a<<4)|b);if(i<5&&s[i*3+2]!=':')return 0;}for(int i=0;i<6;i++)out[i]=fwd[5-i];return 1;}
static int addr_eq(const u8*a,const u8*b);
static void hci_log(const char*tag,const u8*addr,int status);
static int hci_store_link_key(const u8 addr[6],const u8 key[16]);
#define MAX_DEVICES 10
struct DevNameRec {u8 addr[6];char name[33];};
static void device_list_write(void);
static int name_is_unknown(const char*n){return !n||!n[0]||streq(n,"UNKNOWN DEVICE")||streq(n,"PHONE");}
static int devnames_read(struct DevNameRec out[MAX_DEVICES]){char b[768];long fd=s_open(BT_NAMES_PATH,O_RDONLY_,0);if(fd<0)return 0;long n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);if(n<=0)return 0;b[n]=0;int c=0;char*ln=b;for(long i=0;i<=n&&c<MAX_DEVICES;i++){if(b[i]=='\r'||b[i]=='\n'||b[i]==0){char save=b[i];b[i]=0;if(slen(ln)>=18&&ln[17]=='|'&&addr_parse(ln,out[c].addr)){copystr(out[c].name,sizeof(out[c].name),ln+18);if(!out[c].name[0])copystr(out[c].name,sizeof(out[c].name),"UNKNOWN DEVICE");c++;}if(!save)break;ln=b+i+1;}}return c;}
static void devnames_write(struct DevNameRec r[MAX_DEVICES],int c){char b[768];b[0]=0;for(int i=0;i<c;i++){char a[18];addr_text(r[i].addr,a);append_str(b,sizeof(b),a);append_char(b,sizeof(b),'|');append_str(b,sizeof(b),r[i].name[0]?r[i].name:"UNKNOWN DEVICE");append_char(b,sizeof(b),'\n');}file_write(BT_NAMES_PATH,b);}
static int device_name_find(const u8 addr[6],char*out,u32 cap){struct DevNameRec r[MAX_DEVICES];int c=devnames_read(r);for(int i=0;i<c;i++)if(addr_eq(r[i].addr,addr)){if(out&&cap)copystr(out,cap,r[i].name);return 1;}return 0;}
static void device_name_store(const u8 addr[6],const char*name){if(name_is_unknown(name))return;struct DevNameRec r[MAX_DEVICES];int c=devnames_read(r),at=-1;for(int i=0;i<c;i++)if(addr_eq(r[i].addr,addr)){at=i;break;}if(at<0){if(c<MAX_DEVICES)at=c++;else{for(int i=1;i<MAX_DEVICES;i++)r[i-1]=r[i];at=MAX_DEVICES-1;}memcopy(r[at].addr,addr,6);}copystr(r[at].name,sizeof(r[at].name),name);devnames_write(r,c);}
static void device_name_remove(const u8 addr[6]){struct DevNameRec r[MAX_DEVICES],o[MAX_DEVICES];int c=devnames_read(r),n=0;for(int i=0;i<c;i++)if(!addr_eq(r[i].addr,addr))o[n++]=r[i];devnames_write(o,n);}
static void meta_device_set_addr(const u8 a[6]){char n[33];if(device_name_find(a,n,sizeof(n))&&!name_is_unknown(n))copystr(meta_device,sizeof(meta_device),n);else copystr(meta_device,sizeof(meta_device),"PHONE");meta_dirty=1;}
static u8 saved_peer[6];static int saved_peer_valid=0;
static void peer_save(const u8 a[6]){char s[24];addr_text(a,s);append_char(s,sizeof(s),'\n');file_write(BT_PHONE_PATH,s);memcopy(saved_peer,a,6);saved_peer_valid=1;meta_paired=1;meta_device_set_addr(a);device_list_write();}
static void peer_load(void){char b[24]={0};long fd=s_open(BT_PHONE_PATH,O_RDONLY_,0);if(fd<0)return;long n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);if(n<=0)return;b[n]=0;if(addr_parse(b,saved_peer)){saved_peer_valid=1;meta_paired=1;meta_device_set_addr(saved_peer);}}
static void peer_clear(void){memzero(saved_peer,6);saved_peer_valid=0;meta_paired=0;meta_device[0]=0;s_unlink(BT_PHONE_PATH);meta_clear_track();meta_dirty=1;}

/* ---------------- Persistent phone bond store ----------------------------
 * This TomTom image uses pre-2.16 BlueZ.  Its /etc/bluetooth/link_key is a
 * binary database of 36-byte records:
 *   local_bdaddr[6], remote_bdaddr[6], link_key[16], key_type[1], pad[3], time[4]
 * CLIOPLAY also keeps a compact multi-phone copy (23 bytes/record) on SD so
 * future UI can enumerate remembered phones without depending on hcid.
 */
#define MAX_BONDS 10
#define BOND_REC_SIZE 23
#define HCID_REC_SIZE 36
static u8 local_bdaddr[6];static int local_bdaddr_valid=0;
static int local_bdaddr_load(void){char b[24]={0};long fd=s_open(BT_LOCAL_PATH,O_RDONLY_,0);if(fd<0)return 0;long n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);if(n<=0)return 0;b[n]=0;if(!addr_parse(b,local_bdaddr))return 0;local_bdaddr_valid=1;return 1;}
static int bond_read(u8 out[MAX_BONDS][BOND_REC_SIZE]){long fd=s_open(BT_BONDS_PATH,O_RDONLY_,0);if(fd<0)return 0;long n=s_read((int)fd,out,MAX_BONDS*BOND_REC_SIZE);s_close((int)fd);if(n<=0)return 0;int c=(int)(n/BOND_REC_SIZE);if(c>MAX_BONDS)c=MAX_BONDS;return c;}
static void bond_write(u8 rec[MAX_BONDS][BOND_REC_SIZE],int count){long fd=s_open(BT_BONDS_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0600);if(fd<0)return;if(count>0)s_write((int)fd,rec,(u32)(count*BOND_REC_SIZE));s_close((int)fd);}
static int bond_find(const u8 addr[6],u8 key[16],u8*type){u8 r[MAX_BONDS][BOND_REC_SIZE];int c=bond_read(r);for(int i=0;i<c;i++)if(addr_eq(r[i],addr)){if(key)memcopy(key,r[i]+6,16);if(type)*type=r[i][22];return 1;}return 0;}
static void bond_store(const u8 addr[6],const u8 key[16],u8 type){u8 r[MAX_BONDS][BOND_REC_SIZE];int c=bond_read(r),at=-1;for(int i=0;i<c;i++)if(addr_eq(r[i],addr)){at=i;break;}if(at<0){if(c<MAX_BONDS)at=c++;else{for(int i=1;i<MAX_BONDS;i++)memcopy(r[i-1],r[i],BOND_REC_SIZE);at=MAX_BONDS-1;}}memcopy(r[at],addr,6);memcopy(r[at]+6,key,16);r[at][22]=type;bond_write(r,c);}
static void bond_remove(const u8 addr[6]){u8 r[MAX_BONDS][BOND_REC_SIZE],o[MAX_BONDS][BOND_REC_SIZE];int c=bond_read(r),n=0;for(int i=0;i<c;i++)if(!addr_eq(r[i],addr)){memcopy(o[n],r[i],BOND_REC_SIZE);n++;}bond_write(o,n);}
static void hcid_db_update(const u8 remote[6],const u8 key[16],u8 type,int add){if(!local_bdaddr_valid)return;u8 in[HCID_REC_SIZE*16],out[HCID_REC_SIZE*16];long fd=s_open(HCID_KEY_DB,O_RDONLY_,0);long nr=0;if(fd>=0){nr=s_read((int)fd,in,sizeof(in));s_close((int)fd);if(nr<0)nr=0;}int rc=(int)(nr/HCID_REC_SIZE),oc=0;for(int i=0;i<rc&&oc<15;i++){u8*p=in+i*HCID_REC_SIZE;if(addr_eq(p,local_bdaddr)&&addr_eq(p+6,remote))continue;memcopy(out+oc*HCID_REC_SIZE,p,HCID_REC_SIZE);oc++;}if(add&&oc<16){u8*p=out+oc*HCID_REC_SIZE;memzero(p,HCID_REC_SIZE);memcopy(p,local_bdaddr,6);memcopy(p+6,remote,6);memcopy(p+12,key,16);p[28]=type;struct timeval0 tv;memzero(&tv,sizeof(tv));s_gettimeofday(&tv);u32 ts=(u32)tv.tv_sec;memcopy(p+32,&ts,4);oc++;}fd=s_open(HCID_KEY_DB,O_WRONLY_|O_CREAT_|O_TRUNC_,0600);if(fd>=0){if(oc)s_write((int)fd,out,(u32)(oc*HCID_REC_SIZE));s_close((int)fd);}}
static int hcid_db_find(const u8 remote[6],u8 key[16],u8*type){if(!local_bdaddr_valid)return 0;u8 in[HCID_REC_SIZE*16];long fd=s_open(HCID_KEY_DB,O_RDONLY_,0);if(fd<0)return 0;long nr=s_read((int)fd,in,sizeof(in));s_close((int)fd);if(nr<=0)return 0;int rc=(int)(nr/HCID_REC_SIZE);for(int i=0;i<rc;i++){u8*p=in+i*HCID_REC_SIZE;if(addr_eq(p,local_bdaddr)&&addr_eq(p+6,remote)){memcopy(key,p+12,16);if(type)*type=p[28];return 1;}}return 0;}
static int bond_restore_hcid(const u8 addr[6]){u8 key[16],type=0;if(!bond_find(addr,key,&type)){if(!hcid_db_find(addr,key,&type))return 0;bond_store(addr,key,type);hci_log("PHONE BOND IMPORTED FROM HCID",addr,-1);}hcid_db_update(addr,key,type,1);hci_store_link_key(addr,key);hci_log("PHONE BOND RESTORED TO HCID",addr,-1);return 1;}
static void bond_capture(const u8*p){/* HCI Link Key Notification payload: addr6,key16,type1 */if(!p)return;bond_store(p,p+6,p[22]);hci_store_link_key(p,p+6);hci_log("PHONE BOND KEY STORED",p,-1);device_list_write();}
/* Persist the best live identity before a session can clear live_peer_valid.
 * meta_device is intentionally retained across an ordinary disconnect, so it
 * is also a safe recovery source for the currently selected remembered peer. */
static void remember_live_name(void){if(live_peer_valid&&!name_is_unknown(meta_device))device_name_store(live_peer,meta_device);}
static void device_list_write(void){u8 r[MAX_BONDS][BOND_REC_SIZE];int c=bond_read(r);char b[1280];b[0]=0;append_str(b,sizeof(b),"COUNT=");append_uint(b,sizeof(b),(u32)c);append_char(b,sizeof(b),'\n');for(int i=0;i<c;i++){char a[18],n[33];int live=(meta_connected&&live_peer_valid&&addr_eq(r[i],live_peer));int saved=(saved_peer_valid&&addr_eq(r[i],saved_peer));addr_text(r[i],a);if(!device_name_find(r[i],n,sizeof(n)))copystr(n,sizeof(n),"UNKNOWN DEVICE");/* A resolved device name can outlive the transport connection.  Recover it
 * for either the live peer or the currently remembered peer, and immediately
 * write it back so subsequent boots no longer depend on transient metadata. */if((live||saved)&&name_is_unknown(n)&&!name_is_unknown(meta_device)){copystr(n,sizeof(n),meta_device);device_name_store(r[i],n);}append_str(b,sizeof(b),"DEV=");append_str(b,sizeof(b),a);append_char(b,sizeof(b),'|');append_char(b,sizeof(b),live?'1':'0');append_char(b,sizeof(b),'|');append_str(b,sizeof(b),n);append_char(b,sizeof(b),'\n');}file_write(BT_DEVLIST_PATH,b);}
static void hci_remote_name_request(const u8 addr[6]){if(hcifd<0||!addr)return;u8 b[14];memzero(b,sizeof(b));b[0]=0x01;b[1]=0x19;b[2]=0x04;b[3]=10;memcopy(b+4,addr,6);b[10]=0x02;long n=s_write(hcifd,b,sizeof(b));hci_log(n==(long)sizeof(b)?"HCI REMOTE NAME REQUEST":"HCI REMOTE NAME REQUEST FAIL",addr,n==(long)sizeof(b)?-1:1);}
static void remote_name_store(const u8 addr[6],const u8*src,u32 n){char d[33],known[33];u32 j=0;for(u32 i=0;i<n&&j+1<sizeof(d);i++){u8 c=src[i];if(!c)break;if(c>=32&&c<127&&c!='|')d[j++]=(char)c;else if(c>=128){if(!j||d[j-1]!=' ')d[j++]=' ';}}while(j&&d[j-1]==' ')j--;d[j]=0;int target=(live_peer_valid&&addr_eq(addr,live_peer))||(saved_peer_valid&&addr_eq(addr,saved_peer));if(name_is_unknown(d)){/* A transient/blank Remote Name result is not new identity information. Preserve the last resolved name instead of exposing UNKNOWN DEVICE. */if(device_name_find(addr,known,sizeof(known))&&!name_is_unknown(known))copystr(d,sizeof(d),known);else if(target&&!name_is_unknown(meta_device))copystr(d,sizeof(d),meta_device);else copystr(d,sizeof(d),"PHONE");}else device_name_store(addr,d);if(target&&!name_is_unknown(d))copystr(meta_device,sizeof(meta_device),d);meta_dirty=1;device_list_write();char l[96];l[0]=0;append_str(l,sizeof(l),"PHONE NAME ");append_str(l,sizeof(l),d);append_char(l,sizeof(l),'\n');bt_log_append(l);}
static void hci_request(const char*verb,const u8*a){char b[64];b[0]=0;append_str(b,sizeof(b),verb);if(a){char t[18];addr_text(a,t);append_char(b,sizeof(b),' ');append_str(b,sizeof(b),t);}append_char(b,sizeof(b),'\n');file_write(BT_HCI_PATH,b);}

/* ---------------- Carminat continuous PCM ring -------------------------- */
#define SND_CHANNELS 2
#define SND_BITS 16
#define SND_FRAMES 16384
#define SND_RING 65536u
#define IO_SET_FORMAT 0x40145314u
#define IO_DESTROY 0x40045315u
#define IO_SELECT 0x40045316u
#define IO_START 0x40045317u
#define IO_PAUSE 0x40045318u
#define IO_GET_POS 0x4004531au
#define IO_RESET 0x4008531bu
#define IO_SET_STREAM 0x4014531du
#define IO_SET_REG 0x40045301u
#define IO_SET_VOL 0x40045305u
#define IO_SET_VOL_EXT 0x40045307u
#define IO_UNMUTE 0x00005304u
#define IO_MUTE_NAV 0x00005320u
#define IO_UNMUTE_NAV 0x00005321u
struct SndFmt{u32 rate,channels,bits,frames,mode;};struct SndPair{u32 handle,pos;};
/* AUDIOGUARD1: existing 64 KiB device ring, with an explicit 150 ms prefill
 * and byte-accurate validity tracking between successive hardware cursor reads.
 * The driver does not expose an underrun flag; clock elapsed since the last
 * cursor sample also guards against cursor wrap or freeze during starvation. */
#define SND_PREFILL_44K_BYTES 26460u /* 150 ms * 44100 samples/s * 4 bytes */
#define SND_RING_GUARD 4u
#define SND_UNDERRUN_MARGIN_MS 40u
static int sndfd=-1,snd_started=0,snd_mix_active=0,snd_div=1;
static u32 sndhandle=0,snd_write=0,snd_queued=0,snd_used=0,snd_read_pos=0;
static u32 snd_last_sample_ms=0,snd_output_bps=176400u;
static int snd_clock_valid=0;
static u32 snd_underruns=0,snd_pcm_drops=0;
static u8*sndring=0;
static u32 snd_now_ms(void){struct timeval0 tv;if(s_gettimeofday(&tv)<0)return 0;return (u32)tv.tv_sec*1000u+(u32)(tv.tv_usec/1000L);}
static void snd_reg(int reg,int value){u32 packed=((u32)reg<<9)|((u32)value&0x1ffu);s_ioctl(sndfd,IO_SET_REG,(long)packed);}
static void snd_mix_on(void){if(snd_mix_active)return;s_ioctl(sndfd,IO_MUTE_NAV,0);snd_reg(34,272);snd_reg(37,272);snd_reg(10,253);snd_reg(11,253);s_ioctl(sndfd,IO_SET_VOL,116);snd_mix_active=1;}
static void snd_mix_restore(void){if(!snd_mix_active||sndfd<0)return;s_ioctl(sndfd,IO_UNMUTE_NAV,0);snd_reg(34,144);snd_reg(37,144);s_ioctl(sndfd,IO_SET_VOL_EXT,118);snd_mix_active=0;}
static void snd_close(void){if(sndfd>=0){if(snd_started)s_ioctl(sndfd,IO_PAUSE,(long)sndhandle);struct SndPair p={sndhandle,0};s_ioctl(sndfd,IO_RESET,(long)&p);snd_mix_restore();if(sndring)s_munmap(sndring,SND_RING);if(sndhandle)s_ioctl(sndfd,IO_DESTROY,(long)sndhandle);s_close(sndfd);}sndfd=-1;sndring=0;sndhandle=0;snd_write=0;snd_queued=0;snd_used=0;snd_read_pos=0;snd_started=0;snd_mix_active=0;snd_div=1;snd_clock_valid=0;}
static int snd_try_rate(u32 rate,int div){sndfd=(int)s_open(SOUND_PATH,O_RDWR_,0);if(sndfd<0)return 0;struct SndFmt f={rate,SND_CHANNELS,SND_BITS,SND_FRAMES,1};long h=s_ioctl(sndfd,IO_SET_FORMAT,(long)&f);if(h<0){s_close(sndfd);sndfd=-1;return 0;}sndhandle=(u32)h;if(s_ioctl(sndfd,IO_SELECT,(long)sndhandle)<0){snd_close();return 0;}sndring=(u8*)s_mmap2(0,SND_RING,PROT_READ_|PROT_WRITE_,MAP_SHARED_,sndfd,0);if(!sndring){snd_close();return 0;}memzero(sndring,SND_RING);f.frames=1024;f.mode=3;if(s_ioctl(sndfd,IO_SET_STREAM,(long)&f)!=0){snd_close();return 0;}struct SndPair p={sndhandle,0};if(s_ioctl(sndfd,IO_RESET,(long)&p)<0){snd_close();return 0;}snd_write=snd_queued=snd_used=snd_read_pos=0;snd_started=snd_mix_active=snd_clock_valid=0;snd_div=div;snd_output_bps=rate*4u;return 1;}
static void bt_log_rate(const char*tag,u32 rate){char b[80];b[0]=0;append_str(b,sizeof(b),tag);append_char(b,sizeof(b),' ');append_uint(b,sizeof(b),rate);append_char(b,sizeof(b),'\n');bt_log_append(b);}
static int snd_open(void){if(sndfd>=0)return 1;/* Preserve the known-good device setup and sample-rate fallback. */if(snd_try_rate(44100,1)){bt_log_rate("BARC-SND OPEN",44100);return 1;}bt_log_rate("BARC-SND REJECT",44100);if(snd_try_rate(22050,2)){bt_log_rate("BARC-SND OPEN",22050);return 1;}bt_log_rate("BARC-SND REJECT",22050);if(snd_try_rate(11025,4)){bt_log_rate("BARC-SND OPEN",11025);return 1;}bt_log_rate("BARC-SND REJECT",11025);status_set("BT AUDIO ERR");return 0;}
/* Do not destroy/recreate the sound handle to recover: pause + reset the
 * selected stream, erase stale PCM, and prefill it again before IO_START. */
static void snd_reprime(void){
  if(sndfd<0||!snd_started)return;
  snd_underruns++;
  if(s_ioctl(sndfd,IO_PAUSE,(long)sndhandle)<0){snd_close();status_set("BT BUFFERING");return;}
  struct SndPair p={sndhandle,0};
  if(s_ioctl(sndfd,IO_RESET,(long)&p)<0){snd_close();status_set("BT BUFFERING");return;}
  memzero(sndring,SND_RING);
  snd_write=snd_queued=snd_used=snd_read_pos=0;
  snd_started=snd_clock_valid=0;
  status_set("BT BUFFERING");
}
/* A sample is taken ONCE per received media packet, not for each SBC frame.
 * The elapsed-time guard handles a wrapped or frozen playback cursor following
 * a long scheduling stall.  The hardware cursor is treated as valid only while
 * it advances within the bytes we actually submitted. */
static void snd_sample_playback(void){
  if(!snd_started||sndfd<0)return;
  long raw=s_ioctl(sndfd,IO_GET_POS,(long)sndhandle);
  if(raw<0)return; /* Unknown hardware position; do not invent an offset. */
  u32 now=snd_now_ms(),p=(u32)raw&(SND_RING-1u);
  u32 advance=(p-snd_read_pos)&(SND_RING-1u);
  if(snd_clock_valid&&now){
    u32 elapsed=now-snd_last_sample_ms;
    u32 buffered_ms=udiv32(snd_used*1000u,snd_output_bps);
    if(elapsed>buffered_ms+SND_UNDERRUN_MARGIN_MS){snd_reprime();return;}
  }
  if(advance>=snd_used){if(advance||snd_used==0){snd_reprime();return;}}
  snd_used-=advance;snd_read_pos=p;
  if(now){snd_last_sample_ms=now;snd_clock_valid=1;}
}
static int snd_push(const u8*data,u32 n){
  if(!snd_open())return 0;
  u32 used=snd_started?snd_used:snd_queued;
  if(n>SND_RING-SND_RING_GUARD-used){
    if(snd_started){snd_sample_playback();used=snd_started?snd_used:snd_queued;}
    if(n>SND_RING-SND_RING_GUARD-used){snd_pcm_drops++;return 0;}
  }
  u32 first=SND_RING-snd_write;if(first>n)first=n;
  memcopy(sndring+snd_write,data,first);
  if(n>first)memcopy(sndring,data+first,n-first);
  snd_write=(snd_write+n)&(SND_RING-1u);
  if(snd_started){snd_used+=n;return 1;}
  snd_queued+=n;
  /* 150 ms at 44.1k or at the lower driver fallback rate. */
  if(snd_queued>=udiv32(SND_PREFILL_44K_BYTES,(u32)snd_div)){
    snd_mix_on();
    if(s_ioctl(sndfd,IO_START,(long)sndhandle)<0){snd_close();return 0;}
    snd_started=1;snd_used=snd_queued;snd_read_pos=0;
    snd_last_sample_ms=snd_now_ms();snd_clock_valid=snd_last_sample_ms!=0;
  }
  return 1;
}

/* ---------------- Old Linux Bluetooth sockets --------------------------- */
#define AF_UNIX 1
#define AF_BLUETOOTH 31
#define SOCK_STREAM 1
#define SOCK_RAW 3
#define SOCK_SEQPACKET 5
#define BTPROTO_L2CAP 0
#define BTPROTO_HCI 1
#define SOL_HCI 0
#define HCI_FILTER 2
#define HCI_EVENT_PKT 0x04
#define HCI_COMMAND_PKT 0x01
#define EVT_CONN_COMPLETE 0x03
#define EVT_DISCONN_COMPLETE 0x05
#define EVT_AUTH_COMPLETE 0x06
#define EVT_ENCRYPT_CHANGE 0x08
#define EVT_CMD_COMPLETE 0x0e
#define EVT_CMD_STATUS 0x0f
#define EVT_REMOTE_NAME 0x07
#define EVT_PIN_CODE_REQ 0x16
#define EVT_LINK_KEY_REQ 0x17
#define EVT_LINK_KEY_NOTIFY 0x18
#define SOL_L2CAP 6
#define L2CAP_LM 3
#define L2CAP_LM_AUTH 0x0002
#define L2CAP_LM_ENCRYPT 0x0004
#define PSM_AVRCP 0x0017
#define PSM_AVDTP 0x0019
#define POLLIN 1
#define POLLOUT 4
#define POLLERR 8
#define POLLHUP 16
#define POLLNVAL 32
#define SOL_SOCKET 1
#define SO_ERROR 4
#define F_SETFL 4
struct pollfd0{int fd;short events,revents;};
struct sockaddr_l2_0{u16 family;u16 psm;u8 bdaddr[6];u16 cid;} __attribute__((packed));
enum{SC_SOCKET=1,SC_BIND=2,SC_CONNECT=3,SC_LISTEN=4,SC_ACCEPT=5,SC_SETSOCKOPT=14,SC_GETSOCKOPT=15};
struct sockaddr_hci_0{u16 family;u16 dev;} __attribute__((packed));
struct sockaddr_un_0{u16 family;char path[108];} __attribute__((packed));
struct hci_filter_0{u32 type_mask;u32 event_mask[2];u16 opcode;} __attribute__((packed));
static int hci_pair_armed=0,hci_pair_ticks=0;static u8 hci_pin_peer[6];static int hci_pin_peer_valid=0;
/* PAIRFIX7: use TomTom's stock blueserver pairing gate.  Raw HCI remains
 * diagnostic only; only stock hcid may answer PIN_CODE_REQUEST. */
static int pair_policy_on=0,pair_policy_fallback=0;
static char pair_pin[5]="0000";
static int reconnect_wait_ticks=-1;
/* AUTORECONNECT2 EARLYBOOT: boot-only, asynchronous, bounded and user-preemptible.
 * The bootstrap marks /tmp/clbtrdy only after the Renault HID quiet window,
 * HCI/SDP services, A2DP registration and the phone-command helper are ready.
 * Reconnect deliberately does NOT wait for clio040, /tmp/clbt, GPS or a GPS fix.
 * /tmp/clbt remains only the later audio-source gate: a connected phone may be
 * established early while PCM output stays closed until CD/BLUETOOTH is selected. */
enum {AUTO_WAIT_READY=0,AUTO_SETTLE=1,AUTO_CONNECTING=2,AUTO_RETRY_WAIT=3,AUTO_DONE=4};
static int auto_boot_state=AUTO_WAIT_READY,auto_attempts=0,auto_acl_fallback=0;
static u32 auto_next_tick=0,auto_started_tick=0;
static int avr_sdpfd=-1;
static u32 avrcp_sdp_retry_at=0;
static int avrcp_listen_warned=0;
static void hci_log(const char*tag,const u8*addr,int status){char b[112];b[0]=0;append_str(b,sizeof(b),tag);if(addr){char a[18];addr_text(addr,a);append_char(b,sizeof(b),' ');append_str(b,sizeof(b),a);}if(status>=0){append_str(b,sizeof(b)," STATUS=0x");append_hex8(b,sizeof(b),(u8)status);}append_char(b,sizeof(b),'\n');bt_log_append(b);}
static void hci_log_opcode(const char*tag,u16 op,int status){char b[96];b[0]=0;append_str(b,sizeof(b),tag);append_str(b,sizeof(b)," OP=0x");append_hex8(b,sizeof(b),(u8)(op>>8));append_hex8(b,sizeof(b),(u8)op);if(status>=0){append_str(b,sizeof(b)," STATUS=0x");append_hex8(b,sizeof(b),(u8)status);}append_char(b,sizeof(b),'\n');bt_log_append(b);}
static int hci_raw_open(void){unsigned long a[3]={AF_BLUETOOTH,SOCK_RAW,BTPROTO_HCI};int fd=(int)s_socketcall(SC_SOCKET,a);if(fd<0)return-1;struct sockaddr_hci_0 sa;sa.family=AF_BLUETOOTH;sa.dev=0;unsigned long bv[3]={(unsigned long)fd,(unsigned long)&sa,sizeof(sa)};if(s_socketcall(SC_BIND,bv)<0){s_close(fd);return-1;}struct hci_filter_0 f;memzero(&f,sizeof(f));f.type_mask=1u<<HCI_EVENT_PKT;int evs[]={EVT_CONN_COMPLETE,EVT_DISCONN_COMPLETE,EVT_AUTH_COMPLETE,EVT_REMOTE_NAME,EVT_ENCRYPT_CHANGE,EVT_CMD_COMPLETE,EVT_CMD_STATUS,EVT_PIN_CODE_REQ,EVT_LINK_KEY_REQ,EVT_LINK_KEY_NOTIFY};for(unsigned i=0;i<sizeof(evs)/sizeof(evs[0]);i++){int e=evs[i];f.event_mask[e>>5]|=1u<<(e&31);}unsigned long sv[5]={(unsigned long)fd,SOL_HCI,HCI_FILTER,(unsigned long)&f,sizeof(f)};long fr=s_socketcall(SC_SETSOCKOPT,sv);s_fcntl(fd,F_SETFL,O_NONBLOCK_);hci_log(fr<0?"HCI MONITOR READY FILTER-WARN":"HCI MONITOR READY",0,-1);return fd;}
static int hci_store_link_key(const u8 addr[6],const u8 key[16]){if(hcifd<0)return 0;u8 b[27];memzero(b,sizeof(b));b[0]=HCI_COMMAND_PKT;b[1]=0x11;b[2]=0x0c;b[3]=23;b[4]=1;memcopy(b+5,addr,6);memcopy(b+11,key,16);long n=s_write(hcifd,b,sizeof(b));hci_log(n==(long)sizeof(b)?"HCI STORED KEY LOAD":"HCI STORED KEY LOAD FAIL",addr,n==(long)sizeof(b)?-1:1);return n==(long)sizeof(b);}
static void pair_pin_prepare(void){copystr(pair_pin,sizeof(pair_pin),"0000");file_write(BT_PIN_PATH,pair_pin);}
static void pair_pin_status(void){char b[28];b[0]=0;append_str(b,sizeof(b),"BT PAIRING PIN ");append_str(b,sizeof(b),pair_pin);status_set(b);}
static int blueserver_cmd(const char*cmd){
  unsigned long a[3]={AF_UNIX,SOCK_STREAM,0};int fd=(int)s_socketcall(SC_SOCKET,a);if(fd<0)return 0;
  struct sockaddr_un_0 sa;memzero(&sa,sizeof(sa));sa.family=AF_UNIX;copystr(sa.path,sizeof(sa.path),"/var/run/bluesock");
  u32 plen=slen(sa.path)+1u;unsigned long cv[3]={(unsigned long)fd,(unsigned long)&sa,(unsigned long)(2u+plen)};
  if(s_socketcall(SC_CONNECT,cv)<0){s_close(fd);return 0;}
  long n=s_write(fd,cmd,slen(cmd));s_close(fd);return n==(long)slen(cmd);
}
static int pairing_policy_set(int enable){
  char cmd[32];cmd[0]=0;if(enable){append_str(cmd,sizeof(cmd),"PAIRING ENABLE|");append_str(cmd,sizeof(cmd),pair_pin);}else append_str(cmd,sizeof(cmd),"PAIRING DISABLE");int ok=blueserver_cmd(cmd);
  if(ok){if(enable){char l[40];l[0]=0;append_str(l,sizeof(l),"PAIRING POLICY ENABLE PIN ");append_str(l,sizeof(l),pair_pin);hci_log(l,0,-1);}else hci_log("PAIRING POLICY DISABLE SENT",0,-1);pair_policy_on=enable;pair_policy_fallback=0;return 1;}
  hci_log(enable?"PAIRING POLICY ENABLE SOCKET FAIL":"PAIRING POLICY DISABLE SOCKET FAIL",0,-1);return 0;
}
static void pairing_policy_finish(void){
  if(pair_policy_fallback){hci_request("PAIR_END_FALLBACK",0);pair_policy_fallback=0;pair_policy_on=0;s_unlink(BT_PIN_PATH);return;}
  if(pair_policy_on){if(!pairing_policy_set(0)){hci_request("PAIR_END_FALLBACK",0);pair_policy_on=0;s_unlink(BT_PIN_PATH);return;}}
  hci_request("PAIR_END",0);pair_policy_on=0;s_unlink(BT_PIN_PATH);
}
static void hci_event_service(void){if(hcifd<0)return;for(;;){u8 b[320];long n=s_read(hcifd,b,sizeof(b));if(n<=0)return;if(n<3||b[0]!=HCI_EVENT_PKT)continue;u8 evt=b[1],ln=b[2];if((long)(3+ln)>n)continue;const u8*p=b+3;
  if(evt==EVT_PIN_CODE_REQ&&ln>=6){hci_log("HCI PIN REQUEST",p,-1);if(hci_pair_armed){memcopy(hci_pin_peer,p,6);hci_pin_peer_valid=1;hci_log("HCI PIN STOCK POLICY ACTIVE",p,-1);}else{hci_log("HCI PIN REQUEST IGNORED",p,-1);if(saved_peer_valid&&addr_eq(p,saved_peer)){status_set("BT REPAIR REQUIRED");reconnect_wait_ticks=-1;}}continue;}
  if(evt==EVT_CMD_COMPLETE&&ln>=4){u16 op=(u16)p[1]|((u16)p[2]<<8);int st=p[3];if(op==0x040b)hci_log_opcode("HCI LINK KEY POS CMD COMPLETE STOCK",op,st);else if(op==0x040c)hci_log_opcode("HCI LINK KEY NEG CMD COMPLETE STOCK",op,st);else if(op==0x040d)hci_log_opcode("HCI PIN POS CMD COMPLETE STOCK",op,st);else if(op==0x040e)hci_log_opcode("HCI PIN NEG CMD COMPLETE STOCK",op,st);else if(op==0x0c11)hci_log_opcode("HCI STORED KEY WRITE COMPLETE",op,st);continue;}
  if(evt==EVT_CMD_STATUS&&ln>=4){u16 op=(u16)p[2]|((u16)p[3]<<8);if(op==0x040d||op==0x040e)hci_log_opcode("HCI PIN CMD STATUS",op,p[0]);continue;}
  if(evt==EVT_AUTH_COMPLETE&&ln>=3){hci_log("HCI AUTH COMPLETE",hci_pin_peer_valid?hci_pin_peer:0,p[0]);if(p[0]==0){hci_pair_armed=0;hci_pair_ticks=0;pairing_policy_finish();if(hci_pin_peer_valid){peer_save(hci_pin_peer);meta_paired=1;meta_dirty=1;hci_remote_name_request(hci_pin_peer);}}continue;}
  if(evt==EVT_REMOTE_NAME&&ln>=7){if(p[0]==0){remote_name_store(p+1,p+7,(u32)(ln-7));hci_log("HCI REMOTE NAME OK",p+1,-1);}else hci_log("HCI REMOTE NAME FAIL",p+1,p[0]);continue;}
  if(evt==EVT_ENCRYPT_CHANGE&&ln>=4){hci_log("HCI ENCRYPT CHANGE",hci_pin_peer_valid?hci_pin_peer:0,p[0]);continue;}
  if(evt==EVT_LINK_KEY_REQ&&ln>=6){hci_log("HCI LINK KEY REQUEST",p,-1);continue;}
  if(evt==EVT_LINK_KEY_NOTIFY&&ln>=23){hci_log("HCI LINK KEY",p,-1);if(hci_pair_armed||hci_pin_peer_valid||(saved_peer_valid&&addr_eq(p,saved_peer))){bond_capture(p);peer_save(p);meta_paired=1;meta_dirty=1;hci_remote_name_request(p);hci_pair_armed=0;hci_pair_ticks=0;pairing_policy_finish();}continue;}
  if(evt==EVT_CONN_COMPLETE&&ln>=11){hci_log("HCI CONN COMPLETE",p+3,p[0]);continue;}
  if(evt==EVT_DISCONN_COMPLETE&&ln>=4){hci_log("HCI DISCONNECT",hci_pin_peer_valid?hci_pin_peer:0,p[3]);continue;}
 }}
static int bt_socket(void){unsigned long a[3]={AF_BLUETOOTH,SOCK_SEQPACKET,BTPROTO_L2CAP};return(int)s_socketcall(SC_SOCKET,a);}
static int bt_secure(int fd){int lm=L2CAP_LM_AUTH|L2CAP_LM_ENCRYPT;unsigned long a[5]={(unsigned long)fd,SOL_L2CAP,L2CAP_LM,(unsigned long)&lm,sizeof(lm)};return(int)s_socketcall(SC_SETSOCKOPT,a);}
static int bt_bind_psm(int fd,u16 psm){struct sockaddr_l2_0 a;memzero(&a,sizeof(a));a.family=AF_BLUETOOTH;a.psm=psm;unsigned long v[3]={(unsigned long)fd,(unsigned long)&a,sizeof(a)};return(int)s_socketcall(SC_BIND,v);}
static int bt_listen(int fd){unsigned long a[2]={(unsigned long)fd,4};return(int)s_socketcall(SC_LISTEN,a);}
static int bt_accept_peer(int fd,u8 peer[6]){struct sockaddr_l2_0 a;u32 alen=sizeof(a);memzero(&a,sizeof(a));unsigned long v[3]={(unsigned long)fd,(unsigned long)&a,(unsigned long)&alen};int c=(int)s_socketcall(SC_ACCEPT,v);if(c>=0&&peer)memcopy(peer,a.bdaddr,6);return c;}
static int bt_connect_nb(int fd,u16 psm,const u8 peer[6]){struct sockaddr_l2_0 a;memzero(&a,sizeof(a));a.family=AF_BLUETOOTH;a.psm=psm;memcopy(a.bdaddr,peer,6);s_fcntl(fd,F_SETFL,O_NONBLOCK_);unsigned long v[3]={(unsigned long)fd,(unsigned long)&a,sizeof(a)};return(int)s_socketcall(SC_CONNECT,v);}
static int bt_so_error(int fd){int e=0;u32 l=sizeof(e);unsigned long a[5]={(unsigned long)fd,SOL_SOCKET,SO_ERROR,(unsigned long)&e,(unsigned long)&l};if(s_socketcall(SC_GETSOCKOPT,a)<0)return-1;return e;}
static int addr_eq(const u8*a,const u8*b){for(int i=0;i<6;i++)if(a[i]!=b[i])return 0;return 1;}


/* ---------------- Local AVRCP Controller SDP registration ----------------
 * TomTom's old sdptool lacks the AVRCT template even though sdpd itself
 * supports normal local service registration.  Register a minimal AVRCP 1.3
 * Controller record directly over BlueZ's /var/run/sdp compatibility socket.
 * Keep avr_sdpfd open: non-persistent records are collected when this local
 * registration connection closes. */
#define AF_UNIX_ 1
#define SDP_SVC_REGISTER_REQ_ 0x75
#define SDP_SVC_REGISTER_RSP_ 0x76
static const u8 avrcp_sdp_record[]={
  0x35,0x58,
  0x09,0x00,0x00, 0x0a,0xff,0xff,0xff,0xff,
  0x09,0x00,0x01, 0x35,0x06,0x19,0x11,0x0e,0x19,0x11,0x0f,
  0x09,0x00,0x04, 0x35,0x10,0x35,0x06,0x19,0x01,0x00,0x09,0x00,0x17,
                    0x35,0x06,0x19,0x00,0x17,0x09,0x01,0x03,
  0x09,0x00,0x05, 0x35,0x03,0x19,0x10,0x02,
  0x09,0x00,0x09, 0x35,0x08,0x35,0x06,0x19,0x11,0x0e,0x09,0x01,0x03,
  0x09,0x01,0x00, 0x25,0x10,'A','V','R','C','P',' ','C','o','n','t','r','o','l','l','e','r',
  0x09,0x03,0x11, 0x09,0x00,0x01
};
static int avrcp_sdp_register(void){
  unsigned long a[3]={AF_UNIX_,SOCK_STREAM,0};int fd=(int)s_socketcall(SC_SOCKET,a);if(fd<0){hci_log("AVRCP SDP SOCKET FAIL",0,(int)(-fd));return 0;}
  struct sockaddr_un_0 sa;memzero(&sa,sizeof(sa));sa.family=AF_UNIX_;copystr(sa.path,sizeof(sa.path),"/var/run/sdp");u32 plen=slen(sa.path)+1u;unsigned long cv[3]={(unsigned long)fd,(unsigned long)&sa,(unsigned long)(2u+plen)};
  if(s_socketcall(SC_CONNECT,cv)<0){copystr(sa.path,sizeof(sa.path),"/var/run/bluetooth/sdp");plen=slen(sa.path)+1u;cv[2]=(unsigned long)(2u+plen);if(s_socketcall(SC_CONNECT,cv)<0){hci_log("AVRCP SDP CONNECT FAIL",0,-1);s_close(fd);return 0;}}
  u8 req[128];u32 rn=0;req[rn++]=SDP_SVC_REGISTER_REQ_;req[rn++]=0;req[rn++]=1;u16 pay=(u16)(1u+sizeof(avrcp_sdp_record));req[rn++]=(u8)(pay>>8);req[rn++]=(u8)pay;req[rn++]=0;memcopy(req+rn,avrcp_sdp_record,sizeof(avrcp_sdp_record));rn+=(u32)sizeof(avrcp_sdp_record);
  if(s_write(fd,req,rn)!=(long)rn){hci_log("AVRCP SDP WRITE FAIL",0,-1);s_close(fd);return 0;}u8 rsp[32];long n=s_read(fd,rsp,sizeof(rsp));if(n<9||rsp[0]!=SDP_SVC_REGISTER_RSP_){hci_log("AVRCP SDP REGISTER FAIL",0,(int)(n<0?-n:0));s_close(fd);return 0;}avr_sdpfd=fd;hci_log("AVRCP SDP CONTROLLER REGISTERED",0,-1);return 1;
}

/* ---------------- AVDTP / SBC Sink -------------------------------------- */
static int listenfd=-1,ctrllistenfd=-1,sigfd=-1,mediafd=-1;
static int cfg_ok=0,av_opened=0,av_started=0,manual_block=0;
static int avrcp_delay_ticks=-1,reconnect_profile_ticks=-1;
static struct SbcState decst;static struct SbcFrame decfr;static u8 pcm_out[512];
/* Capability is a mask: 44.1 kHz + Stereo + Joint Stereo.  A SET_CONFIGURATION
 * selects exactly one channel mode, so accept 0x22 (Stereo) or 0x21 (Joint). */
static const u8 SBC_CAP0=0x23;
static const u8 SBC_CAP1=0x15; /* 16 blocks + 8 subbands + loudness */
static u8 sbc_sel0=0x22,sbc_sel1=0x15,sbc_sel_min=2,sbc_sel_max=53;
static u32 media_packets=0,media_bytes=0,media_frames=0,media_decode_errors=0;
struct AvSbcCfg{u8 c0,c1,mn,mx;int found;};
static const char*av_sig_name(int sig){switch(sig){case 1:return"DISCOVER";case 2:return"GET_CAPABILITIES";case 3:return"SET_CONFIGURATION";case 4:return"GET_CONFIGURATION";case 5:return"RECONFIGURE";case 6:return"OPEN";case 7:return"START";case 8:return"CLOSE";case 9:return"SUSPEND";case 10:return"ABORT";case 11:return"SECURITY_CONTROL";case 12:return"GET_ALL_CAPABILITIES";case 13:return"DELAY_REPORT";default:return"UNKNOWN";}}
static void av_log_msg(const char*dir,int sig,int tl,int n){char b[128];b[0]=0;append_str(b,sizeof(b),"AVDTP ");append_str(b,sizeof(b),dir);append_char(b,sizeof(b),' ');append_str(b,sizeof(b),av_sig_name(sig));append_str(b,sizeof(b)," TL=");append_uint(b,sizeof(b),(u32)tl);append_str(b,sizeof(b)," LEN=");append_uint(b,sizeof(b),(u32)(n<0?0:n));append_str(b,sizeof(b)," STATE=");append_str(b,sizeof(b),av_started?"STREAMING":(av_opened?"OPEN":(cfg_ok?"CONFIGURED":"IDLE")));append_char(b,sizeof(b),'\n');bt_log_append(b);}
static void av_log_reject(int sig,int err){char b[112];b[0]=0;append_str(b,sizeof(b),"AVDTP TX REJECT ");append_str(b,sizeof(b),av_sig_name(sig));append_str(b,sizeof(b)," ERR=0x");append_hex8(b,sizeof(b),(u8)err);append_char(b,sizeof(b),'\n');bt_log_append(b);}
static void av_log_sbc(const char*tag,const struct AvSbcCfg*c){char b[144];b[0]=0;append_str(b,sizeof(b),tag);append_str(b,sizeof(b)," C0=0x");append_hex8(b,sizeof(b),c->c0);append_str(b,sizeof(b)," C1=0x");append_hex8(b,sizeof(b),c->c1);append_str(b,sizeof(b)," MODE=");append_str(b,sizeof(b),(c->c0&1)?"JOINT":((c->c0&2)?"STEREO":"OTHER"));append_str(b,sizeof(b)," MIN=");append_uint(b,sizeof(b),c->mn);append_str(b,sizeof(b)," MAX=");append_uint(b,sizeof(b),c->mx);append_char(b,sizeof(b),'\n');bt_log_append(b);}
static void av_log_media(void){char b[144];b[0]=0;append_str(b,sizeof(b),"SBC MEDIA RX PACKETS=");append_uint(b,sizeof(b),media_packets);append_str(b,sizeof(b)," BYTES=");append_uint(b,sizeof(b),media_bytes);append_str(b,sizeof(b)," FRAMES=");append_uint(b,sizeof(b),media_frames);append_str(b,sizeof(b)," DECODE_ERR=");append_uint(b,sizeof(b),media_decode_errors);append_str(b,sizeof(b)," SOURCE=");append_char(b,sizeof(b),source_active?'1':'0');append_char(b,sizeof(b),'\n');bt_log_append(b);}
static int av_send(int fd,const u8*req,u8 sig,const u8*payload,int plen,int accept){u8 b[40];if(plen>38)return 0;b[0]=(req[0]&0xf0)|(accept?2:3);b[1]=sig&0x3f;for(int i=0;i<plen;i++)b[2+i]=payload[i];return s_write(fd,b,(u32)(2+plen))==2+plen;}
static void av_caps(u8 p[10]){p[0]=1;p[1]=0;/* Media Transport */p[2]=7;p[3]=6;p[4]=0;p[5]=0;/* audio/SBC */p[6]=SBC_CAP0;p[7]=SBC_CAP1;p[8]=2;p[9]=53;}
static void av_selected(u8 p[10]){p[0]=1;p[1]=0;p[2]=7;p[3]=6;p[4]=0;p[5]=0;p[6]=sbc_sel0;p[7]=sbc_sel1;p[8]=sbc_sel_min;p[9]=sbc_sel_max;}
static int av_parse_sbc(const u8*b,int n,int start,struct AvSbcCfg*out){memzero(out,sizeof(*out));int i=start;while(i+2<=n){int cat=b[i],ln=b[i+1];if(i+2+ln>n)return 0;if(cat==7&&ln>=6){if(b[i+2]!=0||b[i+3]!=0)return 0;out->c0=b[i+4];out->c1=b[i+5];out->mn=b[i+6];out->mx=b[i+7];out->found=1;}i+=2+ln;}return out->found;}
static int av_sbc_supported(const struct AvSbcCfg*c){if(!c->found)return 0;if(c->c0!=0x21&&c->c0!=0x22)return 0;if(c->c1!=SBC_CAP1)return 0;if(c->mn<2||c->mn>53||c->mx<c->mn||c->mx>53)return 0;return 1;}
static int av_seid_is_ours(const u8*b,int n){return n>=3&&((b[2]>>2)&0x3f)==1;}
static void av_store_cfg(const struct AvSbcCfg*c){sbc_sel0=c->c0;sbc_sel1=c->c1;sbc_sel_min=c->mn;sbc_sel_max=c->mx;cfg_ok=1;av_opened=av_started=0;}
static void av_reset_stream(void){cfg_ok=av_opened=av_started=0;sbc_sel0=0x22;sbc_sel1=0x15;sbc_sel_min=2;sbc_sel_max=53;media_packets=media_bytes=media_frames=media_decode_errors=0;memzero(&decst,sizeof(decst));}
static int av_signal(int fd,const u8*b,int n){if(n<2)return 1;int tl=(b[0]>>4)&15,ptype=(b[0]>>2)&3,msg=b[0]&3,sig=b[1]&0x3f;if(ptype!=0){hci_log("AVDTP FRAGMENT UNSUPPORTED",live_peer_valid?live_peer:0,ptype);return 1;}if(msg!=0){/* We are an acceptor for A2DP; ignore peer responses to commands we never originate. */return 1;}u8 p[16];av_log_msg("RX",sig,tl,n);
  if(sig==1){p[0]=1u<<2;p[1]=1u<<3;av_send(fd,b,sig,p,2,1);av_log_msg("TX ACCEPT",sig,tl,4);return 1;} /* SEID 1, Sink */
  if(sig==2||sig==12){if(!av_seid_is_ours(b,n)){p[0]=0x11;av_send(fd,b,sig,p,1,0);av_log_reject(sig,0x11);return 1;}av_caps(p);av_send(fd,b,sig,p,10,1);av_log_msg("TX ACCEPT CAPS",sig,tl,12);return 1;}
  if(sig==3){struct AvSbcCfg c;int ok=n>=12&&av_seid_is_ours(b,n)&&av_parse_sbc(b,n,4,&c)&&av_sbc_supported(&c);if(c.found)av_log_sbc("AVDTP SBC SET",&c);if(ok){av_store_cfg(&c);av_send(fd,b,sig,0,0,1);av_log_msg("TX ACCEPT",sig,tl,2);status_set("BT CONFIGURED");}else{p[0]=7;p[1]=0x29;av_send(fd,b,sig,p,2,0);av_log_reject(sig,0x29);}return 1;}
  if(sig==4){if(!cfg_ok||!av_seid_is_ours(b,n)){p[0]=cfg_ok?0x11:0x13;av_send(fd,b,sig,p,1,0);av_log_reject(sig,p[0]);return 1;}av_selected(p);av_send(fd,b,sig,p,10,1);av_log_msg("TX ACCEPT CONFIG",sig,tl,12);return 1;}
  if(sig==5){struct AvSbcCfg c;int ok=cfg_ok&&av_seid_is_ours(b,n)&&av_parse_sbc(b,n,3,&c)&&av_sbc_supported(&c);if(c.found)av_log_sbc("AVDTP SBC RECONFIG",&c);if(ok){av_store_cfg(&c);av_send(fd,b,sig,0,0,1);av_log_msg("TX ACCEPT",sig,tl,2);}else{p[0]=7;p[1]=0x29;av_send(fd,b,sig,p,2,0);av_log_reject(sig,0x29);}return 1;}
  if(sig==6){if(cfg_ok&&av_seid_is_ours(b,n)){av_opened=1;av_send(fd,b,sig,0,0,1);av_log_msg("TX ACCEPT",sig,tl,2);status_set("BT OPEN");}else{p[0]=!cfg_ok?0x13:0x11;av_send(fd,b,sig,p,1,0);av_log_reject(sig,p[0]);}return 1;}
  if(sig==7){if(av_opened&&av_seid_is_ours(b,n)){av_started=1;meta_playing=1;meta_dirty=1;av_send(fd,b,sig,0,0,1);av_log_msg("TX ACCEPT",sig,tl,2);status_set(source_enabled()?"BT BUFFERING":"BT CONNECTED");avrcp_delay_ticks=10;}else{p[0]=0x31;av_send(fd,b,sig,p,1,0);av_log_reject(sig,0x31);}return 1;}
  if(sig==9){if((av_opened||av_started)&&av_seid_is_ours(b,n)){av_started=0;meta_playing=0;meta_dirty=1;snd_close();av_send(fd,b,sig,0,0,1);av_log_msg("TX ACCEPT",sig,tl,2);status_set("BT PAUSED");}else{p[0]=0x31;av_send(fd,b,sig,p,1,0);av_log_reject(sig,0x31);}return 1;}
  if(sig==8){if((cfg_ok||av_opened)&&av_seid_is_ours(b,n)){av_started=0;av_opened=0;cfg_ok=0;meta_playing=0;meta_dirty=1;snd_close();av_send(fd,b,sig,0,0,1);av_log_msg("TX ACCEPT",sig,tl,2);status_set("BT CONNECTED");}else{p[0]=0x31;av_send(fd,b,sig,p,1,0);av_log_reject(sig,0x31);}return 1;}
  if(sig==10){av_started=0;av_opened=0;cfg_ok=0;meta_playing=0;meta_dirty=1;snd_close();av_send(fd,b,sig,0,0,1);av_log_msg("TX ACCEPT",sig,tl,2);status_set("BT CONNECTED");return 1;}
  if(sig==13){av_send(fd,b,sig,0,0,1);av_log_msg("TX ACCEPT",sig,tl,2);return 1;} /* tolerate delay report */
  p[0]=0x19;av_send(fd,b,sig,p,1,0);av_log_reject(sig,0x19);return 1;
}
#include "audio_chime.h"
static struct AudioChime camera_chime;
static u32 alert_seq=0,alert_idle_until=0,alert_last_media=0;
static u32 alert_now_ms(void){struct {long sec,nsec;}t;if(sc2(263,1,(long)&t)>=0)return (u32)t.sec*1000u+(u32)(t.nsec/1000000L);return snd_now_ms();}
static void alert_service(void){u32 now=alert_now_ms(),msg[4];long f=s_open(CAMERA_ALERT_PATH,O_RDONLY_,0);
 if(f>=0){long n=s_read((int)f,msg,sizeof(msg));s_close((int)f);if(n==sizeof(msg)&&msg[0]==0x4c414c43u&&msg[2]!=alert_seq){alert_seq=msg[2];if(!msg[3]){camera_chime.total=0;alert_idle_until=0;if(sndfd>=0&&now-alert_last_media>200)snd_close();}else if(source_active&&(s32)(msg[2]-now)>=0&&(s32)(msg[2]-now)<=1500&&snd_open()){chime_start(&camera_chime,udiv32(44100u,(u32)snd_div));alert_idle_until=now+1100u;}}}
 if(!source_active){camera_chime.total=0;alert_idle_until=0;return;}
 if(!alert_idle_until)return;
 if((s32)(now-alert_idle_until)>=0){alert_idle_until=0;if(now-alert_last_media>200&&sndfd>=0)snd_close();return;}
 if(now-alert_last_media<=200)return; /* Incoming PCM owns the same ring. */
 if(!snd_open())return;if(snd_started)snd_sample_playback();u32 target=udiv32(snd_output_bps,5u);
 /* At most 250ms of PCM per poll, including the initial 150ms prefill. */
 for(unsigned block=0;block<88&&(snd_started?snd_used:snd_queued)<target;block++){
  for(int i=0;i<128;i++){int gain,tone=chime_sample(&camera_chime,&gain);s16 v=(s16)tone;pcm_out[4*i]=(u8)v;pcm_out[4*i+1]=(u8)((u16)v>>8);pcm_out[4*i+2]=(u8)v;pcm_out[4*i+3]=(u8)((u16)v>>8);}if(!snd_push(pcm_out,512))break;
 }
}
static void pcm_pack(void){int q=0,d=snd_div;if(d<1)d=1;for(int i=0;i<128;i+=d){s32 l=0,r=0;for(int j=0;j<d;j++){l+=decfr.pcm[0][i+j];r+=decfr.pcm[1][i+j];}if(d==2){l>>=1;r>>=1;}else if(d==4){l>>=2;r>>=2;}int gain,tone=chime_sample(&camera_chime,&gain);s16 ls=(s16)chime_mix(l,gain,tone),rs=(s16)chime_mix(r,gain,tone);pcm_out[q++]=(u8)ls;pcm_out[q++]=(u8)((u16)ls>>8);pcm_out[q++]=(u8)rs;pcm_out[q++]=(u8)((u16)rs>>8);}if(!snd_push(pcm_out,(u32)q))status_set("BT BUFFERING");else status_set(snd_started?"BT STREAMING":"BT BUFFERING");}
static void media_packet(const u8*b,int n){if(!av_started||n<14)return;alert_last_media=alert_now_ms();if(source_active&&snd_started)snd_sample_playback();media_packets++;media_bytes+=(u32)n;int cc=b[0]&15,off=12+4*cc;if(b[0]&0x10){if(off+4>n){media_decode_errors++;return;}int ext=((int)b[off+2]<<8)|b[off+3];off+=4+ext*4;}if(off>=n){media_decode_errors++;return;}u8 ph=b[off++];if(ph&0xe0){media_decode_errors++;return;}int frames=ph&15;if(frames<1){media_decode_errors++;return;}int decoded=0;for(int k=0;k<frames&&off<n;k++){int used=sbc_decode(b+off,(u32)(n-off),&decst,&decfr);if(used<=0){media_decode_errors++;break;}off+=used;decoded++;media_frames++;if(!source_active)continue;if(sndfd<0&&!snd_open())return;pcm_pack();}if(media_packets==1||media_packets==10||(media_packets%100u)==0||decoded!=frames)av_log_media();}

/* ---------------- AVRCP Controller: metadata + 4 transport commands ----- */
static int avrcpfd=-1,avrcp_state=0;/*0 none,1 connecting,2 ready*/static u8 avrcp_tl=0;static u32 avrcp_retry_at=0;
static u16 be16(const u8*p){return(u16)(((u16)p[0]<<8)|p[1]);}static u32 be32(const u8*p){return((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];}
static void put_be32(u8*p,u32 v){p[0]=(u8)(v>>24);p[1]=(u8)(v>>16);p[2]=(u8)(v>>8);p[3]=(u8)v;}
static void avrcp_close(void){if(avrcpfd>=0)s_close(avrcpfd);avrcpfd=-1;avrcp_state=0;}
static void reconnect_profile_ready(const u8*peer){if(reconnect_wait_ticks<0)return;if(peer){memcopy(live_peer,peer,6);live_peer_valid=1;}meta_connected=1;meta_dirty=1;reconnect_wait_ticks=-1;reconnect_profile_ticks=-1;status_set("BT CONNECTED");hci_log("PHONE RECONNECT PROFILE READY",peer,-1);}
static void avrcp_open(u32 tick){if(avrcp_state||manual_block||tick<avrcp_retry_at)return;const u8*peer=live_peer_valid?live_peer:(saved_peer_valid?saved_peer:0);if(!peer)return;hci_log("AVRCP CONNECT TRY",peer,-1);int fd=bt_socket();if(fd<0){hci_log("AVRCP SOCKET FAIL",peer,(int)(-fd));avrcp_retry_at=tick+50;return;}/* ACL is already authenticated/encrypted by A2DP.  Do not trigger a second legacy security negotiation here. */int r=bt_connect_nb(fd,PSM_AVRCP,peer);if(r==0){avrcpfd=fd;avrcp_state=2;avrcp_retry_at=tick+50;hci_log("AVRCP CONNECTED",peer,-1);reconnect_profile_ready(peer);return;}/* EINPROGRESS/EALREADY on old Linux. */if(r==-115||r==-114||r==-11){avrcpfd=fd;avrcp_state=1;hci_log("AVRCP CONNECT PENDING",peer,(int)(-r));return;}hci_log("AVRCP CONNECT IMMEDIATE FAIL",peer,(int)(-r));s_close(fd);avrcp_retry_at=tick+50;}
static void avrcp_connect_finish(u32 tick){if(avrcp_state!=1||avrcpfd<0)return;int e=bt_so_error(avrcpfd);const u8*peer=live_peer_valid?live_peer:(saved_peer_valid?saved_peer:0);if(e==0){avrcp_state=2;avrcp_retry_at=tick+50;hci_log("AVRCP CONNECTED",peer,-1);reconnect_profile_ready(peer);}else if(e==11||e==114||e==115){return;}else{hci_log("AVRCP CONNECT FAIL",peer,e<0?-e:e);avrcp_close();avrcp_retry_at=tick+50;if(reconnect_wait_ticks>=0)reconnect_profile_ticks=20;}}
static int avrcp_write(const u8*b,u32 n){return avrcp_state==2&&avrcpfd>=0&&s_write(avrcpfd,b,n)==(long)n;}
static void avrcp_passthrough(u8 op){if(avrcp_state!=2)return;for(int rel=0;rel<2;rel++){u8 b[8];u8 tl=avrcp_tl++&15;b[0]=(u8)(tl<<4);b[1]=0x11;b[2]=0x0e;b[3]=0x00;b[4]=0x48;b[5]=0x7c;b[6]=(u8)(op|(rel?0x80:0));b[7]=0;avrcp_write(b,sizeof(b));}}
static void avrcp_vendor(u8 pdu,const u8*params,u16 plen){u8 b[80];if(plen>67||avrcp_state!=2)return;u8 tl=avrcp_tl++&15;b[0]=(u8)(tl<<4);b[1]=0x11;b[2]=0x0e;b[3]=0x01;/* STATUS */b[4]=0x48;b[5]=0x00;b[6]=0x00;b[7]=0x19;b[8]=0x58;b[9]=pdu;b[10]=0;b[11]=(u8)(plen>>8);b[12]=(u8)plen;for(u16 i=0;i<plen;i++)b[13+i]=params[i];avrcp_write(b,(u32)(13+plen));}
static void avrcp_get_status(void){avrcp_vendor(0x30,0,0);}
static void avrcp_get_meta(void){u8 p[25];memzero(p,sizeof(p));p[8]=4;put_be32(p+9,1);put_be32(p+13,2);put_be32(p+17,3);put_be32(p+21,7);avrcp_vendor(0x20,p,sizeof(p));}
static void avrcp_parse(const u8*b,int n){if(n<6||b[1]!=0x11||b[2]!=0x0e)return;if((b[0]&2)==0)return;/* only AVCTP responses */if(b[5]==0x7c)return;if(b[5]!=0x00||n<13)return;if(b[6]!=0||b[7]!=0x19||b[8]!=0x58)return;u8 pdu=b[9],pkt=b[10];u16 plen=be16(b+11);if(13+(int)plen>n)plen=(u16)(n-13);if(pkt!=0)return;const u8*p=b+13;
  if(pdu==0x30&&plen>=9){u32 dur=be32(p),pos=be32(p+4);int play=p[8]==1;meta_dur=(int)udiv32(dur,1000);meta_pos=(pos==0xffffffffu)?0:(int)udiv32(pos,1000);meta_playing=play;meta_dirty=1;return;}
  if(pdu==0x20&&plen>=1){u32 off=1,count=p[0];meta_title[0]=meta_artist[0]=meta_album[0]=0;for(u32 k=0;k<count&&off+8<=plen;k++){u32 id=be32(p+off);u16 cs=be16(p+off+4),ln=be16(p+off+6);off+=8;if(off+ln>plen)break;if(cs==106){if(id==1)meta_text(meta_title,sizeof(meta_title),p+off,ln);else if(id==2)meta_text(meta_artist,sizeof(meta_artist),p+off,ln);else if(id==3)meta_text(meta_album,sizeof(meta_album),p+off,ln);else if(id==7){u32 ms=0;for(u16 i=0;i<ln&&p[off+i]>='0'&&p[off+i]<='9';i++)ms=ms*10u+(u32)(p[off+i]-'0');if(ms)meta_dur=(int)udiv32(ms,1000);}}off+=ln;}if(streq(meta_album,"CLIOPLAY-NAV")){char action=0;s32 lat=0,lon=0;u16 seq=0;if(nav_packet_parse(meta_title,&action,&lat,&lon,&seq))nav_publish(action,lat,lon,seq,meta_artist);else bt_log_append("NAV RX REJECTED\n");meta_hide_track();return;}if(nav_sender_fallback_meta()){bt_log_append("NAV SENDER FALLBACK METADATA HIDDEN\n");meta_hide_track();return;}meta_dirty=1;return;}
}

/* The daemon shares no auto-connect command file with the UI.  That means
 * manual CONNECT/PAIR/DISCONNECT/UNPAIR always wins over a pending boot attempt. */
static void auto_boot_cancel(void){auto_boot_state=AUTO_DONE;}
static void auto_boot_preempt(void){
  if(auto_boot_state==AUTO_CONNECTING){
    reconnect_wait_ticks=-1;reconnect_profile_ticks=-1;avrcp_close();
  }
  auto_boot_cancel();
}
static int auto_path_exists(const char*path){long fd=s_open(path,O_RDONLY_,0);if(fd<0)return 0;s_close((int)fd);return 1;}
static void auto_boot_service(u32 tick){
  if(auto_boot_state==AUTO_DONE)return;
  if(!saved_peer_valid||manual_block||hci_pair_armed||meta_connected||live_peer_valid||sigfd>=0||
     (auto_boot_state!=AUTO_CONNECTING&&avrcpfd>=0)){
    auto_boot_cancel();return;
  }
  if(auto_boot_state==AUTO_CONNECTING){
    /* If an authentication error or session teardown cancelled the underlying
     * reconnect, do not silently start another attempt. */
    if(reconnect_wait_ticks<0){reconnect_profile_ticks=-1;avrcp_close();auto_boot_cancel();return;}
    /* Profile-first is preferable on this old BlueZ stack.  One ACL fallback
     * is allowed if the phone has not answered after the initial grace. */
    if(!auto_acl_fallback&&tick-auto_started_tick>=80u&&avrcp_state!=2){
      auto_acl_fallback=1;hci_request("CONNECT",saved_peer);
      hci_log("AUTO RECONNECT ACL FALLBACK",saved_peer,-1);
    }
    return;
  }
  if(auto_boot_state==AUTO_WAIT_READY){
    if(tick>=600u){hci_log("AUTO RECONNECT BOOT READINESS TIMEOUT",0,-1);auto_boot_cancel();return;}
    if((tick%10u)!=0u)return; /* SD-free, low-frequency startup check. */
    /* Bluetooth readiness is the only boot barrier.  GPS and the framebuffer UI
     * start independently and must never delay restoration of the last phone. */
    if(hcifd<0||listenfd<0||ctrllistenfd<0||avr_sdpfd<0||!local_bdaddr_valid||
       !auto_path_exists(BT_BOOT_READY_PATH))return;
    auto_next_tick=tick+10u;auto_boot_state=AUTO_SETTLE;
    hci_log("AUTO RECONNECT BT CORE READY - EARLY SETTLE",saved_peer,-1);
    return;
  }
  if(tick<auto_next_tick)return;
  if(avrcpfd>=0){auto_boot_cancel();return;} /* incoming phone owns the link */
  if(!bond_restore_hcid(saved_peer)){
    hci_log("AUTO RECONNECT SAVED BOND MISSING",saved_peer,-1);
    status_set("BT REPAIR REQUIRED");auto_boot_cancel();return;
  }
  auto_attempts++;auto_acl_fallback=0;auto_started_tick=tick;
  reconnect_profile_ticks=0;reconnect_wait_ticks=250;avrcp_retry_at=0;
  auto_boot_state=AUTO_CONNECTING;
  status_set("BT RECONNECTING");
  hci_log(auto_attempts==1?"AUTO RECONNECT PROFILE-FIRST":"AUTO RECONNECT SECOND TRY",saved_peer,-1);
}
static void session_close(int keep_status){remember_live_name();snd_close();avrcp_close();avrcp_delay_ticks=-1;reconnect_profile_ticks=-1;reconnect_wait_ticks=-1;if(mediafd>=0)s_close(mediafd);if(sigfd>=0)s_close(sigfd);mediafd=sigfd=-1;av_reset_stream();live_peer_valid=0;meta_connected=0;meta_playing=0;meta_dirty=1;if(!keep_status)status_set(meta_paired?"BT WAIT PHONE":"BT WAIT PHONE");}

/* clio040 command file is a single latest-command record; sequence prevents repeats. */
static void process_command(void){char b[96]={0};long fd=s_open(BT_CMD_PATH,O_RDONLY_,0);if(fd<0)return;long n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);if(n<=0)return;b[n]=0;u32 seq=0;if(!parse_uint0(b,&seq)||seq==command_seq_seen)return;command_seq_seen=seq;char*p=b;while(*p>='0'&&*p<='9')p++;while(*p==' '||*p=='\t')p++;
  if(starts0(p,"PLAY")){avrcp_passthrough(0x44);return;}if(starts0(p,"PAUSE")){avrcp_passthrough(0x46);return;}if(starts0(p,"NEXT")){avrcp_passthrough(0x4b);return;}if(starts0(p,"PREV")){avrcp_passthrough(0x4c);return;}
  if(starts0(p,"PAIR")){auto_boot_preempt();if(hci_pair_armed){pair_pin_status();return;}manual_block=0;pair_pin_prepare();hci_pair_armed=1;hci_pair_ticks=600;hci_pin_peer_valid=0;{char l[40];l[0]=0;append_str(l,sizeof(l),"HCI PAIR ARMED 60S PIN ");append_str(l,sizeof(l),pair_pin);hci_log(l,0,-1);}if(pairing_policy_set(1)){hci_request("PAIR",0);}else{pair_policy_fallback=1;pair_policy_on=1;hci_request("PAIR_FALLBACK",0);}pair_pin_status();return;}
  if(starts0(p,"CONNECT")){auto_boot_preempt();u8 target[6];const u8*t=0;char*q=p+7;while(*q==' '||*q=='\t')q++;if(addr_parse(q,target))t=target;else if(saved_peer_valid)t=saved_peer;manual_block=0;if(!t){status_set("BT NO SAVED DEVICE");return;}if(hci_pair_armed){hci_pair_armed=0;hci_pair_ticks=0;pairing_policy_finish();}hci_pin_peer_valid=0;if(!bond_restore_hcid(t)){status_set("BT REPAIR REQUIRED");hci_log("PHONE RECONNECT NO SAVED KEY",t,-1);return;}peer_save(t);hci_remote_name_request(t);avrcp_close();reconnect_profile_ticks=0;reconnect_wait_ticks=250;avrcp_retry_at=0;hci_log("PHONE RECONNECT PROFILE-FIRST+BOND",t,-1);status_set("BT RECONNECTING");return;}
  if(starts0(p,"DISCONNECT")){auto_boot_preempt();u8 target[6];const u8*t=live_peer_valid?live_peer:(saved_peer_valid?saved_peer:0);char*q=p+10;while(*q==' '||*q=='\t')q++;if(addr_parse(q,target))t=target;manual_block=1;if(hci_pair_armed){hci_pair_armed=0;hci_pair_ticks=0;pairing_policy_finish();}if(t)hci_request("DISCONNECT",t);if(!t||(live_peer_valid&&addr_eq(t,live_peer)))session_close(1);status_set("BT DISCONNECTED");device_list_write();return;}
  if(starts0(p,"UNPAIR")){auto_boot_preempt();u8 target[6];const u8*t=saved_peer_valid?saved_peer:0;char*q=p+6;while(*q==' '||*q=='\t')q++;if(addr_parse(q,target))t=target;manual_block=1;if(hci_pair_armed){hci_pair_armed=0;hci_pair_ticks=0;pairing_policy_finish();}if(t){u8 oldpeer[6];memcopy(oldpeer,t,6);bond_remove(oldpeer);device_name_remove(oldpeer);hcid_db_update(oldpeer,0,0,0);hci_request("UNPAIR",oldpeer);if(live_peer_valid&&addr_eq(oldpeer,live_peer))session_close(1);if(saved_peer_valid&&addr_eq(oldpeer,saved_peer)){u8 r[MAX_BONDS][BOND_REC_SIZE];int c=bond_read(r);if(c>0)peer_save(r[0]);else peer_clear();}}device_list_write();status_set("BT DEVICE FORGOTTEN");return;}
}

static int make_listener_psm(u16 psm){int l=bt_socket();if(l<0)return-1;bt_secure(l);if(bt_bind_psm(l,psm)<0||bt_listen(l)<0){s_close(l);return-1;}return l;}
static void accept_avdtp(void){u8 peer[6];int c=bt_accept_peer(listenfd,peer);if(c<0)return;
  /* Security was configured on the listening PSM before listen().  Do not
   * re-apply L2CAP_LM to an already accepted channel on this old kernel. */
  if(manual_block){hci_log("AVDTP ACCEPT BLOCKED",peer,-1);s_close(c);return;}
  if(sigfd<0){sigfd=c;memcopy(live_peer,peer,6);live_peer_valid=1;peer_save(peer);meta_connected=1;meta_dirty=1;reconnect_profile_ticks=-1;reconnect_wait_ticks=-1;if(hci_pair_armed){hci_pair_armed=0;hci_pair_ticks=0;pairing_policy_finish();}av_reset_stream();status_set("BT CONNECTED");hci_remote_name_request(peer);device_list_write();hci_log("AVDTP SIGNAL ACCEPT",peer,-1);return;}
  if(mediafd<0&&av_opened&&live_peer_valid&&addr_eq(peer,live_peer)){mediafd=c;hci_log("AVDTP MEDIA ACCEPT",peer,-1);return;}
  hci_log("AVDTP EXTRA CHANNEL REJECT",peer,av_opened?1:0);s_close(c);
}
static void accept_avrcp(void){u8 peer[6];int c=bt_accept_peer(ctrllistenfd,peer);if(c<0)return;if(manual_block){s_close(c);return;}int known=bond_find(peer,0,0);int ok=(live_peer_valid&&addr_eq(peer,live_peer))||(!live_peer_valid&&((saved_peer_valid&&addr_eq(peer,saved_peer))||known));if(!ok){s_close(c);return;}if(!live_peer_valid){memcopy(live_peer,peer,6);live_peer_valid=1;peer_save(peer);meta_connected=1;meta_dirty=1;}hci_remote_name_request(peer);if(avrcpfd>=0)s_close(avrcpfd);avrcpfd=c;s_fcntl(avrcpfd,F_SETFL,O_NONBLOCK_);avrcp_state=2;avrcp_retry_at=0;hci_log("AVRCP ACCEPTED",peer,-1);reconnect_profile_ready(peer);}

static void serve(void){long prio=s_audio_priority();peer_load();local_bdaddr_load();meta_write();status_set("BT WAIT PHONE");hcifd=hci_raw_open();if(hcifd<0)hci_log("HCI MONITOR OPEN FAIL",0,-1);if(saved_peer_valid)bond_restore_hcid(saved_peer);device_list_write();hci_log("AVRCP SDP+MULTI-DEVICE KEYVAULT+PIN0000 AUDIOGUARD1 AUTORECONNECT2-EARLYBOOT",0,-1);hci_log(prio<0?"AUDIO PRIORITY REQUEST DENIED (NORMAL SCHEDULING)":"AUDIO PRIORITY -4 SET",0,-1);u32 tick=0;for(;;){
    if(listenfd<0){listenfd=make_listener_psm(PSM_AVDTP);if(listenfd<0){status_set("BT L2CAP ERR");s_nanosleep(1,0);continue;}}
    if(ctrllistenfd<0){ctrllistenfd=make_listener_psm(PSM_AVRCP);if(ctrllistenfd>=0){hci_log("AVRCP LISTEN PSM17 READY",0,-1);avrcp_listen_warned=0;}else if(!avrcp_listen_warned){hci_log("AVRCP LISTEN PSM17 FAIL",0,-1);avrcp_listen_warned=1;}}
    if(ctrllistenfd>=0&&avr_sdpfd<0&&tick>=avrcp_sdp_retry_at){if(!avrcp_sdp_register())avrcp_sdp_retry_at=tick+50;}
    process_command();
    auto_boot_service(tick);
    if(reconnect_profile_ticks>0)reconnect_profile_ticks--;else if(reconnect_profile_ticks==0&&!manual_block&&saved_peer_valid&&!meta_connected){avrcp_open(tick);if(avrcp_state==2){reconnect_profile_ticks=-1;}else if(avrcp_state==1){reconnect_profile_ticks=-1;hci_log("AVRCP RECONNECT PROFILE PENDING",saved_peer,-1);}else reconnect_profile_ticks=20;}
    if(reconnect_wait_ticks>0)reconnect_wait_ticks--;else if(reconnect_wait_ticks==0&&!meta_connected){
      reconnect_wait_ticks=-1;reconnect_profile_ticks=-1;
      if(auto_boot_state==AUTO_CONNECTING){
        avrcp_close();
        if(auto_attempts<2){auto_boot_state=AUTO_RETRY_WAIT;auto_next_tick=tick+120u;
          hci_log("AUTO RECONNECT PHONE NOT READY - ONE DELAYED RETRY",saved_peer,-1);}
        else{auto_boot_cancel();hci_log("AUTO RECONNECT STOPPED - PHONE UNAVAILABLE",saved_peer,-1);}
        status_set("BT WAIT PHONE");
      }else{status_set("BT REPAIR REQUIRED");hci_log("PHONE RECONNECT PROFILE TIMEOUT REPAIR REQUIRED",saved_peer_valid?saved_peer:0,-1);}
    }
    if(avrcp_delay_ticks>0)avrcp_delay_ticks--;else if(avrcp_delay_ticks==0&&meta_connected&&!manual_block){avrcp_open(tick);if(avrcp_state)avrcp_delay_ticks=-1;}
    else if(avrcp_delay_ticks<0&&meta_connected&&avrcp_state==0&&av_started&&!manual_block&&tick>=avrcp_retry_at)avrcp_open(tick);
    struct pollfd0 pf[6];int np=0,ih=-1,il=-1,ir=-1,is=-1,im=-1,ic=-1;
    if(hcifd>=0){ih=np;pf[np].fd=hcifd;pf[np].events=POLLIN;pf[np].revents=0;np++;}
    il=np;pf[np].fd=listenfd;pf[np].events=POLLIN;pf[np].revents=0;np++;if(ctrllistenfd>=0){ir=np;pf[np].fd=ctrllistenfd;pf[np].events=POLLIN;pf[np].revents=0;np++;}
    if(sigfd>=0){is=np;pf[np].fd=sigfd;pf[np].events=POLLIN;pf[np].revents=0;np++;}
    if(mediafd>=0){im=np;pf[np].fd=mediafd;pf[np].events=POLLIN;pf[np].revents=0;np++;}
    if(avrcpfd>=0){ic=np;pf[np].fd=avrcpfd;pf[np].events=POLLIN|(avrcp_state==1?POLLOUT:0);pf[np].revents=0;np++;}
    long pr=s_poll(pf,(u32)np,100);tick++;
    if(hci_pair_ticks>0){hci_pair_ticks--;if(hci_pair_ticks==0&&hci_pair_armed){hci_pair_armed=0;hci_log("HCI PAIR/RECONNECT WINDOW EXPIRED",0,-1);pairing_policy_finish();}}
    int en=source_enabled();source_active=en;if(!en&&sndfd>=0)snd_close();if(av_started){if(!en)status_set("BT CONNECTED");else status_set(snd_started?"BT STREAMING":"BT BUFFERING");}
    alert_service();
    if(meta_connected&&avrcp_state==2){if((tick%10u)==0)avrcp_get_status();if((tick%30u)==1)avrcp_get_meta();}
    if(meta_dirty||(tick%10u)==0){meta_write();device_list_write();}
    if(pr<0){hci_log("POLL ERROR",0,(int)(-pr));s_nanosleep(0,50000000);continue;}if(pr==0)continue;
    if(ih>=0&&(pf[ih].revents&POLLIN))hci_event_service();
    if(il>=0&&(pf[il].revents&POLLIN))accept_avdtp();if(ir>=0&&ctrllistenfd>=0&&(pf[ir].revents&POLLIN))accept_avrcp();
    if(is>=0&&sigfd>=0&&(pf[is].revents&POLLIN)){u8 b[512];long n=s_read(sigfd,b,sizeof(b));if(n<=0){hci_log("AVDTP SIGNAL READ CLOSE",live_peer_valid?live_peer:0,(int)(n<0?-n:0));session_close(0);continue;}av_signal(sigfd,b,(int)n);}
    if(is>=0&&sigfd>=0&&(pf[is].revents&(POLLERR|POLLHUP|POLLNVAL))){hci_log("AVDTP SIGNAL POLL CLOSE",live_peer_valid?live_peer:0,(int)pf[is].revents);session_close(0);continue;}
    if(im>=0&&mediafd>=0&&(pf[im].revents&POLLIN)){u8 b[2048];long n=s_read(mediafd,b,sizeof(b));if(n<=0){hci_log("AVDTP MEDIA READ CLOSE",live_peer_valid?live_peer:0,(int)(n<0?-n:0));s_close(mediafd);mediafd=-1;av_started=0;snd_close();meta_playing=0;meta_dirty=1;status_set("BT CONNECTED");}else media_packet(b,(int)n);}
    if(im>=0&&mediafd>=0&&(pf[im].revents&(POLLERR|POLLHUP|POLLNVAL))){hci_log("AVDTP MEDIA POLL CLOSE",live_peer_valid?live_peer:0,(int)pf[im].revents);s_close(mediafd);mediafd=-1;av_started=0;snd_close();meta_playing=0;meta_dirty=1;status_set("BT CONNECTED");}
    if(ic>=0&&avrcpfd>=0){
      if(avrcp_state==1&&(pf[ic].revents&POLLOUT))avrcp_connect_finish(tick);
      if(avrcp_state==2&&(pf[ic].revents&POLLIN)){u8 b[512];long n=s_read(avrcpfd,b,sizeof(b));if(n<=0){hci_log("AVRCP READ CLOSE",live_peer_valid?live_peer:0,(int)(n<0?-n:0));avrcp_close();avrcp_retry_at=tick+50;}else avrcp_parse(b,(int)n);}
      if(avrcpfd>=0&&(pf[ic].revents&(POLLERR|POLLHUP|POLLNVAL))){hci_log("AVRCP POLL CLOSE",live_peer_valid?live_peer:0,(int)pf[ic].revents);avrcp_close();avrcp_retry_at=tick+50;}
    }
  }}

__attribute__((noreturn)) void _start(void){memzero(status_last,sizeof(status_last));s_unlink(CAMERA_ALERT_PATH);s_unlink(BT_STATUS_PATH);s_unlink(BT_META_PATH);s_unlink(BT_NAV_PATH);s_unlink(BT_NAV_TMP_PATH);s_unlink(BT_HCI_PATH);serve();s_exit(0);}

#else
/* Host decoder validation: read the entire SBC stream, not just one chunk. */
int main(int argc,char**argv){if(argc!=3)return 2;FILE*fi=fopen(argv[1],"rb"),*fo=fopen(argv[2],"wb");if(!fi||!fo)return 3;u8 in[8192];u32 have=0;struct SbcState s;struct SbcFrame f;memzero(&s,sizeof(s));for(;;){size_t got=fread(in+have,1,sizeof(in)-have,fi);have+=(u32)got;u32 off=0;while(off<have){int u=sbc_decode(in+off,have-off,&s,&f);if(u>0){off+=(u32)u;for(int i=0;i<128;i++){fwrite(&f.pcm[0][i],2,1,fo);fwrite(&f.pcm[1][i],2,1,fo);}continue;}if(u==-1){off++;continue;}break;}if(off){for(u32 i=off;i<have;i++)in[i-off]=in[i];have-=off;}if(!got)break;if(have==sizeof(in)){/* invalid/unsupported frame cannot wedge validation */for(u32 i=1;i<have;i++)in[i-1]=in[i];have--;}}fclose(fi);fclose(fo);return 0;}
#endif
