/* Real receiver playback helpers with an in-memory old-driver ABI mock.
 * No radio, Bluetooth stack, kernel timing or physical audio is validated. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static unsigned mock_now=10000,mock_rate_cap=44100,mock_rate,mock_chunk,mock_chunks;
static unsigned mock_cursor_at,mock_starts,mock_pauses,mock_formats,mock_closes,mock_logs;
static int mock_running,mock_source=1,mock_debug=0,mock_alert=0,mock_shutdown=0;
static unsigned mock_alert_words[4];
static unsigned char mock_ring[65536] __attribute__((aligned(4)));
static char mock_nav[512];
static char mock_nav_ack[128];
static unsigned char mock_avrcp_tx[32][80];
static unsigned mock_avrcp_tx_len[32],mock_avrcp_tx_n;
static struct MockFile{const char*path;unsigned char data[1280];unsigned size;} mock_files[]={
 {"./clio843/btnames.dat",{0},0},{"./clio843/btbonds.dat",{0},0},
 {"/tmp/clbtdevices",{0},0},{"/tmp/clbtdevices.tmp",{0},0},
 {"/tmp/clbtmeta",{0},0},{"/tmp/clbtmeta.tmp",{0},0}};
static int mock_file(const char*p){for(unsigned i=0;i<sizeof(mock_files)/sizeof(mock_files[0]);i++)if(!strcmp(p,mock_files[i].path))return (int)i;return-1;}
static long sc1(long nr,long a){(void)nr;(void)a;return 0;}
static long sc2(long nr,long a,long b){
 if(nr==263){long*t=(long*)b;t[0]=mock_now/1000;t[1]=(mock_now%1000)*1000000L;return 0;}
 (void)a;return -38;
}
static long sc3(long nr,long a,long b,long c){(void)nr;(void)a;(void)b;(void)c;return 0;}
static long s_open(const char*p,int flags,int mode){
 (void)mode;
 int i=mock_file(p);if(i>=0){if(flags&512)mock_files[i].size=0;else if(!mock_files[i].size&&!(flags&64))return-2;return 20+i;}
 if(!strcmp(p,"/dev/barc-snd")){mock_formats++;return 3;}
 if(!strcmp(p,"/tmp/clbt"))return 4;
 if(!strcmp(p,"/tmp/cldebug"))return 5;
 if(!strcmp(p,"/tmp/clalert"))return mock_alert?6:-2;
 if(!strcmp(p,"/tmp/clshutdown"))return mock_shutdown?7:-2;
 if(!strcmp(p,"/tmp/clnav_ack"))return mock_nav_ack[0]?12:-2;
 if(flags&64)return !strcmp(p,"/media/sdcard/clio843/bt.log")?11:!strcmp(p,"/tmp/clnav.tmp")?9:8;
 return -2;
}
static long s_read(int fd,void*p,unsigned n){
 if(fd>=20&&fd<26){struct MockFile*f=&mock_files[fd-20];if(n>f->size)n=f->size;memcpy(p,f->data,n);return n;}
 if(fd==4||fd==5){if(n)((char*)p)[0]=(fd==4?mock_source:mock_debug)?'1':'0';return n?1:0;}
 if(fd==6){unsigned size=sizeof(mock_alert_words);if(n<size)size=n;memcpy(p,mock_alert_words,size);return size;}
 if(fd==12){unsigned len=(unsigned)strlen(mock_nav_ack);if(n>len)n=len;memcpy(p,mock_nav_ack,n);return n;}
 return -2;
}
static long s_write(int fd,const void*p,unsigned n){
 if(fd>=20&&fd<26){struct MockFile*f=&mock_files[fd-20];assert(n<sizeof(f->data));memcpy(f->data,p,n);f->data[n]=0;f->size=n;return n;}
 if(fd==30){assert(mock_avrcp_tx_n<32&&n<=sizeof(mock_avrcp_tx[0]));memcpy(mock_avrcp_tx[mock_avrcp_tx_n],p,n);mock_avrcp_tx_len[mock_avrcp_tx_n]=n;mock_avrcp_tx_n++;return n;}
 if(fd==9){unsigned size=n<sizeof(mock_nav)-1?n:sizeof(mock_nav)-1;memcpy(mock_nav,p,size);mock_nav[size]=0;}
 if(fd==11)mock_logs++;
 return n;
}
static long s_close(int fd){if(fd==3){mock_closes++;mock_running=0;}return 0;}
static long s_rename(const char*a,const char*b){int i=mock_file(a),j=mock_file(b);if(i>=0&&j>=0){memcpy(mock_files[j].data,mock_files[i].data,mock_files[i].size+1);mock_files[j].size=mock_files[i].size;mock_files[i].size=0;}return 0;}
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
 mock_starts=mock_pauses=mock_closes=mock_formats=mock_logs=0;mock_nav[0]=0;mock_nav_ack[0]=0;
 source_active=0;av_reset_stream();cfg_ok=av_opened=av_started=1;
 meta_connected=meta_playing=1;meta_dirty=0;meta_title[0]=meta_artist[0]=meta_album[0]=0;
 nav_last_seq_valid=0;nav_ack_pending=0;nav_last_ack_valid=0;nav_resume_music=0;real_media_was_playing=0;nav_last_activity_ms=0;
 camera_chime.total=0;alert_idle_until=alert_last_media=alert_seq=0;
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
 assert(media_frames>0&&snd_started);u32 frames=media_frames,packets=media_packets;
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
static void bluetooth_identity_checks(void){
 u8 a[6]={1,2,3,4,5,6},b[6]={2,2,3,4,5,6},key[16]={0};char addr_a[18],addr_b[18],name[33];
 addr_text(a,addr_a);addr_text(b,addr_b);bond_store(a,key,0);bond_store(b,key,0);
 live_peer_valid=1;memcopy(live_peer,a,6);meta_connected=1;saved_peer_valid=1;memcopy(saved_peer,b,6);
 remote_name_store(a,(const u8*)"First phone",11);
 assert(streq(meta_device,"First phone")&&streq(meta_device_addr,addr_a));assert(!device_name_find(b,name,sizeof(name)));
 remote_name_store(b,(const u8*)"Second phone",12);
 assert(streq(meta_device,"First phone")&&streq(meta_device_addr,addr_a));
 assert(device_name_find(a,name,sizeof(name))&&streq(name,"First phone"));assert(device_name_find(b,name,sizeof(name))&&streq(name,"Second phone"));
 device_list_write();assert(strstr((char*)mock_files[2].data,"DEV=06:05:04:03:02:01|1|First phone\n"));assert(strstr((char*)mock_files[2].data,"DEV=06:05:04:03:02:02|0|Second phone\n"));
 meta_write();assert(strstr((char*)mock_files[4].data,"DEVICE_ADDR=06:05:04:03:02:01\nDEVICE=First phone\n"));
 memcopy(live_peer,b,6);meta_device_set_addr(b);remote_name_store(a,(const u8*)"First renamed",13);
 assert(streq(meta_device,"Second phone")&&streq(meta_device_addr,addr_b));
 assert(device_name_find(a,name,sizeof(name))&&streq(name,"First renamed"));assert(device_name_find(b,name,sizeof(name))&&streq(name,"Second phone"));
 remote_name_store(b,(const u8*)"",0);assert(streq(meta_device,"Second phone"));device_list_write();
 assert(strstr((char*)mock_files[2].data,"DEV=06:05:04:03:02:02|1|Second phone\n"));
 assert(strstr((char*)mock_files[2].data,"DEV=06:05:04:03:02:01|0|First renamed\n"));
}
static void avrcp_notify_checks(void){
 reset(44100);avrcp_close();avrcp_tl=0;mock_avrcp_tx_n=0;
 avrcpfd=30;avrcp_state=2;
 avrcp_on_ready();
 /* Expect initial GetPlayStatus (0x30), GetElementAttributes (0x20),
  * RegisterNotification(PLAYBACK=1) and RegisterNotification(TRACK=2). */
 assert(mock_avrcp_tx_n==4);
 assert(mock_avrcp_tx[0][3]==0x01&&mock_avrcp_tx[0][9]==0x30);
 assert(mock_avrcp_tx[1][3]==0x01&&mock_avrcp_tx[1][9]==0x20);
 assert(mock_avrcp_tx[2][3]==0x03&&mock_avrcp_tx[2][9]==0x31&&mock_avrcp_tx_len[2]==18&&mock_avrcp_tx[2][13]==AVRCP_EVT_PLAYBACK);
 assert(mock_avrcp_tx[3][3]==0x03&&mock_avrcp_tx[3][9]==0x31&&mock_avrcp_tx_len[3]==18&&mock_avrcp_tx[3][13]==AVRCP_EVT_TRACK);
 assert(avrcp_notify_state[AVRCP_EVT_PLAYBACK]==1&&avrcp_notify_state[AVRCP_EVT_TRACK]==1);
 u8 tl_pb=(u8)(mock_avrcp_tx[2][0]>>4),tl_tr=(u8)(mock_avrcp_tx[3][0]>>4);
 /* INTERIM arms playback & updates meta_playing immediately. */
 mock_avrcp_tx_n=0;meta_playing=1;meta_dirty=0;
 u8 interim_pb[]={(u8)((tl_pb<<4)|2),0x11,0x0e,0x0f,0x48,0,0,0x19,0x58,0x31,0,0,2,AVRCP_EVT_PLAYBACK,2};
 avrcp_parse(interim_pb,sizeof(interim_pb));
 assert(avrcp_notify_state[AVRCP_EVT_PLAYBACK]==2&&!meta_playing&&meta_dirty&&mock_avrcp_tx_n==0);
 u8 interim_tr[]={(u8)((tl_tr<<4)|2),0x11,0x0e,0x0f,0x48,0,0,0x19,0x58,0x31,0,0,9,AVRCP_EVT_TRACK,0,0,0,0,0,0,0,1};
 avrcp_parse(interim_tr,sizeof(interim_tr));
 assert(avrcp_notify_state[AVRCP_EVT_TRACK]==2&&mock_avrcp_tx_n==0);
 /* Duplicate registration while armed is suppressed. */
 avrcp_register_notify(AVRCP_EVT_TRACK);assert(mock_avrcp_tx_n==0);
 /* TRACK_CHANGED fires immediate metadata + status fetch and re-arms track notify. */
 u8 changed_tr[]={(u8)((tl_tr<<4)|2),0x11,0x0e,0x0d,0x48,0,0,0x19,0x58,0x31,0,0,9,AVRCP_EVT_TRACK,0,0,0,0,0,0,0,2};
 avrcp_parse(changed_tr,sizeof(changed_tr));
 assert(mock_avrcp_tx_n==3);
 assert(mock_avrcp_tx[0][9]==0x20&&mock_avrcp_tx[1][9]==0x30&&mock_avrcp_tx[2][3]==0x03&&mock_avrcp_tx[2][9]==0x31&&mock_avrcp_tx[2][13]==AVRCP_EVT_TRACK);
 assert(avrcp_notify_state[AVRCP_EVT_TRACK]==1);
 /* PLAYBACK_STATUS_CHANGED updates playing flag, requests status, and re-arms playback notify. */
 mock_avrcp_tx_n=0;meta_dirty=0;
 u8 changed_pb[]={(u8)((tl_pb<<4)|2),0x11,0x0e,0x0d,0x48,0,0,0x19,0x58,0x31,0,0,2,AVRCP_EVT_PLAYBACK,1};
 avrcp_parse(changed_pb,sizeof(changed_pb));
 assert(meta_playing&&meta_dirty&&mock_avrcp_tx_n==2);
 assert(mock_avrcp_tx[0][9]==0x30&&mock_avrcp_tx[1][3]==0x03&&mock_avrcp_tx[1][9]==0x31&&mock_avrcp_tx[1][13]==AVRCP_EVT_PLAYBACK);
 /* REJECTED / NOT_IMPLEMENTED marks event unsupported using transaction label and suppresses retry spam. */
 u8 tl_pb2=(u8)(mock_avrcp_tx[1][0]>>4);
 mock_avrcp_tx_n=0;
 u8 rejected_pb[]={(u8)((tl_pb2<<4)|2),0x11,0x0e,0x0a,0x48,0,0,0x19,0x58,0x31,0,0,1,0x01};
 avrcp_parse(rejected_pb,sizeof(rejected_pb));
 assert(avrcp_notify_state[AVRCP_EVT_PLAYBACK]==3);
 avrcp_register_notify(AVRCP_EVT_PLAYBACK);assert(mock_avrcp_tx_n==0);
 /* REJECTED status response does not clobber valid metadata. */
 meta_dur=210;meta_pos=42;meta_playing=1;
 u8 rejected_status[]={2,0x11,0x0e,0x0a,0x48,0,0,0x19,0x58,0x30,0,0,1,0x00};
 avrcp_parse(rejected_status,sizeof(rejected_status));
 assert(meta_dur==210&&meta_pos==42&&meta_playing==1);
 /* Closing AVRCP resets notification state so reconnects re-negotiate cleanly. */
 avrcp_close();
 assert(avrcp_notify_state[AVRCP_EVT_PLAYBACK]==0&&avrcp_notify_state[AVRCP_EVT_TRACK]==0);
}
static void shutdown_checks(void){
 reset(44100);mock_shutdown=0;assert(!shutdown_requested());
 u8 p[2048];int n=packet(p,5);pump(p,n,16,0);
 assert(sndfd>=0&&mock_running);
 listenfd=40;ctrllistenfd=41;avr_sdpfd=42;hcifd=43;sigfd=44;mediafd=45;avrcpfd=46;
 mock_shutdown=1;assert(shutdown_requested());
 unsigned closes_before=mock_closes;
 audio_shutdown_cleanup();
 assert(sndfd<0&&!mock_running&&mock_closes==closes_before+1);
 assert(listenfd<0&&ctrllistenfd<0&&avr_sdpfd<0&&hcifd<0&&sigfd<0&&mediafd<0&&avrcpfd<0);
 mock_shutdown=0;
}
static void feed_avrcp_meta(const char*title,const char*artist,const char*album){
 u8 meta[384]={2,0x11,0x0e,0x0c,0x48,0,0,0x19,0x58,0x20};int off=14;meta[13]=3;
 const char*values[]={title,artist,album};
 const unsigned ids[]={1,2,3};
 for(int i=0;i<3;i++){unsigned len=(unsigned)strlen(values[i]);put_be32(meta+off,ids[i]);meta[off+4]=0;meta[off+5]=106;meta[off+6]=(u8)(len>>8);meta[off+7]=(u8)len;memcpy(meta+off+8,values[i],len);off+=8+(int)len;}
 meta[11]=(u8)((off-13)>>8);meta[12]=(u8)(off-13);avrcp_parse(meta,off);
}
static void clp3_signed(char*out,unsigned cap,const char*body){
 u16 crc=crc16_ccitt_text(body,(u32)strlen(body));
 snprintf(out,cap,"%s:%04X",body,crc);
}
static void clp3_link_checks(void){
 reset(44100);avrcpfd=30;avrcp_state=2;mock_avrcp_tx_n=0;
 /* Suppress ClioPlay Link fallback title while silent transport activates. */
 feed_avrcp_meta("ClioPlay Link","","");
 assert(meta_title[0]==0&&meta_album[0]==0);
 /* Establish real music playing prior to CLP3 transfer for smart resume. */
 meta_playing=1;feed_avrcp_meta("Midnight City","M83","Hurry Up");
 assert(real_media_was_playing&&!nav_resume_music);
 /* Atomic trip stop 0 of 3 with instant ACK + chime kind=3. */
 char pkt[128];clp3_signed(pkt,sizeof(pkt),"CLP3:T:0:3:2A:5497032:-160208:0101");
 feed_avrcp_meta(pkt,"Tesco Extra","CLIOPLAY-NAV");
 assert(nav_ack_pending&&nav_ack_seq==0x0101&&nav_resume_music);
 assert(strstr(mock_nav,"VER=3\n")&&strstr(mock_nav,"ACTION=T\n")&&strstr(mock_nav,"A=0\n")&&strstr(mock_nav,"B=3\n")&&strstr(mock_nav,"C=42\n"));
 snprintf(mock_nav_ack,sizeof(mock_nav_ack),"SEQ=%u\nRES=OK\nCHIME=3\n",0x0101u);
 mock_now+=50;nav_ack_service(mock_now);
 assert(!nav_ack_pending&&mock_avrcp_tx_n==2);
 assert(mock_avrcp_tx[0][5]==0x7c&&mock_avrcp_tx[0][6]==0x4b&&mock_avrcp_tx[1][6]==(0x4b|0x80));
 assert(camera_chime.kind==3u&&chime_active(&camera_chime));
 /* Retry with same seq immediately re-emits cached ACK without re-publishing. */
 mock_avrcp_tx_n=0;feed_avrcp_meta(pkt,"Tesco Extra","CLIOPLAY-NAV");
 assert(mock_avrcp_tx_n==2&&mock_avrcp_tx[0][6]==0x4b);
 /* NACK returns 0x4c PREV pass-through. */
 mock_avrcp_tx_n=0;clp3_signed(pkt,sizeof(pkt),"CLP3:N:5150740:-12780:0102");
 feed_avrcp_meta(pkt,"Outside Map","CLIOPLAY-NAV");
 assert(nav_ack_pending&&nav_ack_seq==0x0102);
 snprintf(mock_nav_ack,sizeof(mock_nav_ack),"SEQ=%u\nRES=NACK\nCHIME=0\n",0x0102u);
 mock_now+=40;nav_ack_service(mock_now);
 assert(!nav_ack_pending&&mock_avrcp_tx_n==2&&mock_avrcp_tx[0][6]==0x4c&&mock_avrcp_tx[1][6]==(0x4c|0x80));
 /* Parse every remaining CLP3 packet kind: F, P, C(X/K/T), Q(S/A/D), H. */
 struct NavPacket np;
 clp3_signed(pkt,sizeof(pkt),"CLP3:F:1:5497032:-160208:0103");
 assert(nav_packet_parse_ex(pkt,&np)&&np.ver==3&&np.action=='F'&&np.a==1);
 clp3_signed(pkt,sizeof(pkt),"CLP3:P:013209030200A40174043C00:0104");
 assert(nav_packet_parse_ex(pkt,&np)&&np.action=='P'&&streq(np.hex,"013209030200A40174043C00"));
 clp3_signed(pkt,sizeof(pkt),"CLP3:C:X:0105");
 assert(nav_packet_parse_ex(pkt,&np)&&np.action=='C'&&np.sub=='X');
 clp3_signed(pkt,sizeof(pkt),"CLP3:C:K:0106");
 assert(nav_packet_parse_ex(pkt,&np)&&np.action=='C'&&np.sub=='K');
 clp3_signed(pkt,sizeof(pkt),"CLP3:C:T:69F01234:60:0107");
 assert(nav_packet_parse_ex(pkt,&np)&&np.action=='C'&&np.sub=='T'&&np.a==0x69f01234u&&np.b==60);
 clp3_signed(pkt,sizeof(pkt),"CLP3:Q:S:0108");
 assert(nav_packet_parse_ex(pkt,&np)&&np.action=='Q'&&np.sub=='S');
 clp3_signed(pkt,sizeof(pkt),"CLP3:Q:A:3:0109");
 assert(nav_packet_parse_ex(pkt,&np)&&np.action=='Q'&&np.sub=='A'&&np.a==3);
 clp3_signed(pkt,sizeof(pkt),"CLP3:Q:D:010A");
 assert(nav_packet_parse_ex(pkt,&np)&&np.action=='Q'&&np.sub=='D');
 clp3_signed(pkt,sizeof(pkt),"CLP3:H:3:010B");
 assert(nav_packet_parse_ex(pkt,&np)&&np.action=='H'&&np.a==3);
 /* Bad CRC must be rejected. */
 pkt[strlen(pkt)-1]^=1;
 assert(!nav_packet_parse_ex(pkt,&np));
 /* Smart Music Resume fires 0x44 PLAY once silent transfer finishes and settles. */
 mock_avrcp_tx_n=0;av_started=0;meta_playing=0;
 mock_now+=950;nav_ack_service(mock_now);
 assert(mock_avrcp_tx_n==2&&mock_avrcp_tx[0][6]==0x44&&mock_avrcp_tx[1][6]==(0x44|0x80)&&!nav_resume_music);
 /* Active QR sync (Q:S / Q:A) enables fast metadata polling and defers Smart Music Resume until Q:D. */
 meta_playing=1;feed_avrcp_meta("Midnight City","M83","Hurry Up");
 assert(real_media_was_playing&&!nav_qr_active);
 clp3_signed(pkt,sizeof(pkt),"CLP3:Q:S:010D");
 feed_avrcp_meta(pkt,"QR Sync","CLIOPLAY-NAV");
 assert(nav_qr_active&&!nav_resume_music);
 snprintf(mock_nav_ack,sizeof(mock_nav_ack),"SEQ=%u\nRES=OK\nCHIME=0\n",0x010Du);
 mock_now+=40;nav_ack_service(mock_now);
 mock_avrcp_tx_n=0;av_started=0;meta_playing=0;
 mock_now+=1200;nav_ack_service(mock_now);
 assert(mock_avrcp_tx_n==0&&nav_qr_active); /* Must NOT fire PLAY during active QR scan! */
 clp3_signed(pkt,sizeof(pkt),"CLP3:Q:D:010E");
 feed_avrcp_meta(pkt,"QR Done","CLIOPLAY-NAV");
 assert(!nav_qr_active&&nav_resume_music);
 snprintf(mock_nav_ack,sizeof(mock_nav_ack),"SEQ=%u\nRES=OK\nCHIME=3\n",0x010Eu);
 mock_now+=40;nav_ack_service(mock_now);
 mock_avrcp_tx_n=0;av_started=0;meta_playing=0;
 mock_now+=950;nav_ack_service(mock_now);
 assert(mock_avrcp_tx_n==2&&mock_avrcp_tx[0][6]==0x44&&!nav_resume_music);
 /* Legacy CLP2 must never arm AVRCP ACK. */
 mock_avrcp_tx_n=0;clp3_signed(pkt,sizeof(pkt),"CLP2:N:5497032:-160208:010C");
 feed_avrcp_meta(pkt,"Legacy Send","CLIOPLAY-NAV");
 assert(!nav_ack_pending&&mock_avrcp_tx_n==0);
}
int main(void){bluetooth_identity_checks();avrcp_notify_checks();clp3_link_checks();rate_checks();source_checks();idle_checks();cue_checks();copy_checks();shutdown_checks();snd_close();puts("Playback: three rates, gated decode/fresh resume, AVRCP notifications/CLP3 bidirectional ACK+NACK+resume, bounded idle drain, transport suspend, paused cues, cooperative shutdown cleanup and PCM copies passed.");return 0;}
