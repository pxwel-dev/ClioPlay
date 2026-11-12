/* Real receiver playback helpers with an in-memory old-driver ABI mock.
 * No radio, Bluetooth stack, kernel timing or physical audio is validated. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static unsigned mock_now=10000,mock_rate_cap=44100,mock_rate,mock_chunk,mock_chunks;
static unsigned mock_cursor_at,mock_starts,mock_pauses,mock_formats,mock_closes,mock_logs;
static int mock_running,mock_source=1,mock_debug=0,mock_alert=0;
static unsigned mock_alert_words[4];
static unsigned char mock_ring[65536] __attribute__((aligned(4)));
static char mock_nav[512];
static long sc1(long nr,long a){(void)nr;(void)a;return 0;}
static long sc2(long nr,long a,long b){
 if(nr==263){long*t=(long*)b;t[0]=mock_now/1000;t[1]=(mock_now%1000)*1000000L;return 0;}
 (void)a;return -38;
}
static long sc3(long nr,long a,long b,long c){(void)nr;(void)a;(void)b;(void)c;return 0;}
static long s_open(const char*p,int flags,int mode){
 (void)mode;
 if(!strcmp(p,"/dev/barc-snd")){mock_formats++;return 3;}
 if(!strcmp(p,"/tmp/clbt"))return 4;
 if(!strcmp(p,"/tmp/cldebug"))return 5;
 if(!strcmp(p,"/tmp/clalert"))return mock_alert?6:-2;
 if(flags&64)return !strcmp(p,"/media/sdcard/clio843/bt.log")?11:!strcmp(p,"/tmp/clnav.tmp")?9:8;
 return -2;
}
static long s_read(int fd,void*p,unsigned n){
 if(fd==4||fd==5){if(n)((char*)p)[0]=(fd==4?mock_source:mock_debug)?'1':'0';return n?1:0;}
 if(fd==6){unsigned size=sizeof(mock_alert_words);if(n<size)size=n;memcpy(p,mock_alert_words,size);return size;}
 return -2;
}
static long s_write(int fd,const void*p,unsigned n){
 if(fd==9){unsigned size=n<sizeof(mock_nav)-1?n:sizeof(mock_nav)-1;memcpy(mock_nav,p,size);mock_nav[size]=0;}
 if(fd==11)mock_logs++;
 return n;
}
static long s_close(int fd){if(fd==3){mock_closes++;mock_running=0;}return 0;}
static long s_rename(const char*a,const char*b){(void)a;(void)b;return 0;}
static long s_ioctl(int fd,unsigned req,long arg){
 (void)fd;
 if(req==0x40145314u){unsigned*f=(unsigned*)arg;if(f[0]>mock_rate_cap)return -22;mock_rate=f[0];return 1;}
 if(req==0x4014531du){unsigned*f=(unsigned*)arg;mock_chunk=f[3];mock_chunks=f[4];return 0;}
 if(req==0x40045317u){mock_running=1;mock_cursor_at=mock_now;mock_starts++;return 0;}
 if(req==0x40045318u){mock_running=0;mock_pauses++;return 0;}
 if(req==0x4008531bu){mock_running=0;return 0;}
 if(req==0x4004531au){
  unsigned chunk=mock_chunk*4;
  return mock_running?(long)((chunk*mock_chunks+((uint64_t)(mock_now-mock_cursor_at)*mock_rate/1000/mock_chunk)*chunk)&65535):0;
 }
 return 0;
}
static long s_poll(void*p,unsigned n,int timeout){(void)p;(void)n;(void)timeout;return 0;}
static long s_audio_priority(void){return 0;}
static void*s_mmap2(void*a,unsigned len,int prot,int flags,int fd,unsigned off){
 (void)a;(void)prot;(void)flags;(void)fd;(void)off;assert(len==sizeof(mock_ring));return mock_ring;
}
static long s_munmap(void*p,unsigned len){(void)p;(void)len;return 0;}
static long s_nanosleep(long s,long ns){(void)s;(void)ns;return 0;}
struct timeval0{long tv_sec,tv_usec;};
static long s_gettimeofday(void*p){long*t=p;t[0]=mock_now/1000;t[1]=(mock_now%1000)*1000L;return 0;}
__attribute__((noreturn)) static void s_exit(int n){exit(n);}
static long s_socketcall(int n,unsigned long*a){(void)n;(void)a;return -97;}
#define HOST_TEST 1
#define AUDIO_RUNTIME_TEST 1
#include "../clio843/src/clioa2dp.c"

static void reset(unsigned rate){
 snd_close();mock_now=10000;mock_rate_cap=rate;mock_running=mock_alert=mock_debug=0;mock_source=1;
 mock_starts=mock_pauses=mock_closes=mock_formats=mock_logs=0;mock_nav[0]=0;
 source_active=0;av_reset_stream();cfg_ok=av_opened=av_started=1;
 meta_connected=meta_playing=1;meta_dirty=0;meta_title[0]=meta_artist[0]=meta_album[0]=0;
 nav_last_seq_valid=0;camera_chime.total=0;alert_idle_until=alert_last_media=alert_seq=0;
 snd_underruns=snd_pcm_drops=0;status_last[0]=0;
 audio_source_refresh();
}
static int packet(u8*out,int frames){
 u32 r=0x12345;int at=13;memset(out,0,2048);out[0]=0x80;out[1]=0x60;out[12]=(u8)frames;
 for(int k=0;k<frames;k++){
  u8 d[128];for(int i=0;i<128;i++){r^=r<<13;r^=r>>17;r^=r<<5;d[i]=(u8)r;}
  d[0]=0x9c;d[1]=0xbd;d[2]=53;struct SbcFrame f;int n=sbc_unpack(d,sizeof(d),&f);assert(n>0);
  memcpy(out+at,d,n);at+=n;
 }
 return at;
}
static void pump(const u8*p,int n,int count,int first){
 for(int i=first;i<first+count;i++){
  mock_now=10000+(unsigned)((uint64_t)i*5*128*1000/44100);
  media_packet(p,n);audio_playback_service(mock_now);
 }
}
static void rate_checks(void){
 u8 p[2048];int n=packet(p,5);
 const unsigned rates[]={44100,22050,11025};
 for(unsigned i=0;i<3;i++){
  reset(rates[i]);pump(p,n,345,0);
  assert(mock_rate==rates[i]&&mock_chunk==audio_primary_frames((int)(44100/rates[i]))&&mock_chunks==3);
  assert(mock_starts==1&&snd_underruns==0&&snd_pcm_drops==0&&snd_started);
  assert(media_frames==1725&&mock_formats==i+1);
 }
}
static void source_checks(void){
 u8 p[2048];int n=packet(p,5);reset(44100);pump(p,n,30,0);
 assert(decst.initialized&&snd_started);u32 frames=media_frames,packets=media_packets;
 mock_source=0;audio_source_refresh();assert(sndfd<0&&sbc_resume_reset);
 pump(p,n,20,30);assert(media_packets==packets+20&&media_frames==frames);
 /* Malformed media is still bounded while muted; it cannot reopen output. */
 p[12]=0xe5;media_packet(p,n);assert(sndfd<0);p[12]=5;
 mock_source=1;audio_source_refresh();assert(sbc_resume_reset);
 /* A resumed frame must use fresh synthesis history, never old radio-era PCM. */
 struct SbcState fresh={0};struct SbcFrame expected;int pos=13;
 for(int i=0;i<5;i++){int used=sbc_decode(p+pos,(u32)(n-pos),&fresh,&expected);assert(used>0);pos+=used;}
 mock_now+=20;media_packet(p,n);
 assert(!sbc_resume_reset&&!memcmp(expected.pcm,decfr.pcm,sizeof(expected.pcm)));
 /* AVRCP still accepts metadata and destination messages with output disabled. */
 mock_source=0;audio_source_refresh();
 u8 status[22]={2,0x11,0x0e,0x0c,0x48,0,0,0x19,0x58,0x30,0,0,9};
 put_be32(status+13,180000);put_be32(status+17,65000);status[21]=1;
 avrcp_parse(status,sizeof(status));assert(meta_playing&&meta_pos==65&&meta_dur==180);
 char title[96]="CLP2:N:5497032:-160208:1234";u16 crc=crc16_ccitt_text(title,slen(title));char tail[6];
 snprintf(tail,sizeof(tail),":%04X",crc);strcat(title,tail);
 u8 meta[384]={2,0x11,0x0e,0x0c,0x48,0,0,0x19,0x58,0x20};int off=14;meta[13]=3;
 const char*values[]={title,"Muted destination","CLIOPLAY-NAV"};
 const unsigned ids[]={1,2,3};
 for(int i=0;i<3;i++){unsigned len=(unsigned)strlen(values[i]);put_be32(meta+off,ids[i]);meta[off+4]=0;meta[off+5]=106;meta[off+6]=(u8)(len>>8);meta[off+7]=(u8)len;memcpy(meta+off+8,values[i],len);off+=8+(int)len;}
 meta[11]=(u8)((off-13)>>8);meta[12]=(u8)(off-13);avrcp_parse(meta,off);
 assert(nav_last_seq_valid&&nav_last_seq==0x1234&&strstr(mock_nav,"NAME=Muted destination"));
 assert(!source_active&&sndfd<0);
}
static void idle_checks(void){
 u8 p[2048];int n=packet(p,5);reset(44100);pump(p,n,30,0);u32 at=mock_now,queued=snd_feed_bytes;
 mock_now=at+100;audio_playback_service(mock_now);assert(snd_started);
 mock_now=at+1000;audio_playback_service(mock_now);
 assert(sndfd<0&&media_audio_idle&&av_started&&sbc_resume_reset&&meta_connected);
 unsigned closes=mock_closes;audio_source_refresh();audio_playback_service(mock_now+1000);assert(mock_closes==closes&&streq(status_last,"BT CONNECTED"));
 mock_now+=20;media_packet(p,n);assert(sndfd>=0&&!media_audio_idle&&!sbc_resume_reset);
 /* A nearly full slow-rate ring gets enough time to drain, not a fixed 250 ms. */
 assert(!audio_idle_expired(999,1000,0,176400));
 assert(!audio_idle_expired(1000,0,65532,44100));assert(audio_idle_expired(2000,0,65532,44100));
 assert(!audio_idle_expired(0x30,0xfffffff0u,queued,176400));
 assert(audio_idle_expired(0x500,0xfffffff0u,queued,176400));
 /* A legitimate AVDTP suspend/close still releases output immediately. */
 const u8 suspend[]={0,9,4};av_signal(-1,suspend,3);assert(!av_started&&sndfd<0&&meta_connected);
}
static void cue_checks(void){
 reset(44100);mock_now=20000;mock_alert=1;
 mock_alert_words[0]=0x4c414c43u;mock_alert_words[1]=1;mock_alert_words[2]=mock_now+500;mock_alert_words[3]=1;
 alert_service();assert(snd_started&&alert_idle_until);
 for(int i=1;i<=10;i++){mock_now=20000+(unsigned)i*100;alert_service();audio_playback_service(mock_now);assert(sndfd>=0);}
 mock_now=21200;alert_service();audio_playback_service(mock_now);assert(sndfd<0&&!alert_idle_until);
 assert(!mock_logs); /* no diagnostics are written with Debug off */
}
static void copy_checks(void){
 unsigned char from[1040] __attribute__((aligned(4))),to[1040] __attribute__((aligned(4))),expected[1040];
 for(unsigned i=0;i<sizeof(from);i++)from[i]=(u8)(i*73u);
 for(unsigned so=0;so<4;so++)for(unsigned dest=0;dest<4;dest++)for(unsigned n=0;n<=1024;n++){
  memset(to,0xa5,sizeof(to));memset(expected,0xa5,sizeof(expected));memcpy(expected+dest,from+so,n);
  audio_pcm_copy(to+dest,from+so,n);assert(!memcmp(expected,to,sizeof(to)));
 }
}
int main(void){rate_checks();source_checks();idle_checks();cue_checks();copy_checks();snd_close();puts("Playback: three rates, gated decode/fresh resume, AVRCP/destination handoff, bounded idle drain, transport suspend, paused cues and PCM copies passed.");return 0;}
