/* ClioPlay Global
 * Source-controlled framebuffer UI + offline CPM map renderer for Renault
 * Carminat TomTom (480x272 RGB565, ARMv4T Linux).
 *
 * Runtime inputs:
 *   /var/run/gpspipe       NMEA GPS stream
 *   /tmp/cfbus             volatile focusd event stream (source/volume)
 *   /tmp/cfstate           focusd vehicle state (temp/source mode)
 *   /dev/input/event0..7   TomTom remote (hot-discovered after HID bring-up)
 *   /proc/barcelona/onoff  power request latch
 *
 * CPM5 provides global tiles and directional road metadata. Device-side code uses
 * fixed-point integer maths only; no libc, malloc, SQLite, protobuf or libm.
 */
#include "release_version.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;
#include "frame_schedule.h"

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
static long s_unlink(const char*p){return sc1(10,(long)p);}
static long s_lseek(int fd,long o,int w){return sc3(19,fd,o,w);}
static long s_ioctl(int fd,u32 req,long arg){return sc3(54,fd,(long)req,arg);}
static long s_brk(long a){return sc1(45,a);}
static void*s_mmap2(void*addr,u32 len,int prot,int flags,int fd,u32 pgoff){long r=sc6(192,(long)addr,(long)len,prot,flags,fd,(long)pgoff);return (u32)r>=0xfffff001u?0:(void*)r;}
static long s_munmap(void*p,u32 len){return sc2(91,(long)p,(long)len);}
static long s_sync(void){return sc0(36);}
static long s_rename(const char*a,const char*b){return sc2(38,(long)a,(long)b);}
struct kts { long sec,nsec; };
struct ktv { long sec,usec; };
static long s_nanosleep(long sec,long ns){ struct kts t={sec,ns}; return sc2(162,(long)&t,0); }
/* Monotonic millisecond clock for the cooperative 10 Hz scheduler.  Old
 * Barcelona kernels normally provide clock_gettime(2); gettimeofday is a
 * compatibility fallback so routing never depends on libc. */
static u32 s_now_ms(void){struct kts t;if(sc2(263,1,(long)&t)>=0)return (u32)t.sec*1000u+(u32)(t.nsec/1000000L);struct ktv v;if(sc2(78,(long)&v,0)>=0)return (u32)v.sec*1000u+(u32)(v.usec/1000L);return 0;}
__attribute__((noreturn)) static void s_exit(int c){sc1(1,c);for(;;){}}
#else
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
static long s_read(int fd,void*p,unsigned n){return read(fd,p,n);}
static long s_write(int fd,const void*p,unsigned n){return write(fd,p,n);}
/* The runtime uses Linux flag values; native macOS tests need host flags. */
static long s_open(const char*p,int f,int m){
 int flags=f&3;if(f&64)flags|=O_CREAT;if(f&512)flags|=O_TRUNC;
 if(f&1024)flags|=O_APPEND;if(f&2048)flags|=O_NONBLOCK;
 return open(p,flags,m);
}
static long s_close(int fd){return close(fd);}
static long s_unlink(const char*p){return unlink(p);}
static long s_lseek(int fd,long o,int w){return lseek(fd,o,w);}
static long s_ioctl(int fd,u32 req,long arg){return ioctl(fd,(unsigned long)req,(void*)arg);}
static void*s_mmap2(void*addr,u32 len,int prot,int flags,int fd,u32 pgoff){if(flags&32)flags=(flags&~32)|MAP_ANON;void*p=mmap(addr,len,prot,flags,fd,(off_t)pgoff*4096);return p==MAP_FAILED?0:p;}
static long s_munmap(void*p,u32 len){return munmap(p,len);}
static long s_sync(void){return 0;}
static long s_rename(const char*a,const char*b){return rename(a,b);}
static long s_nanosleep(long sec,long ns){(void)sec;(void)ns;return 0;}
static u32 s_now_ms(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t)==0)return (u32)t.tv_sec*1000u+(u32)(t.tv_nsec/1000000L);return 0;}
__attribute__((noreturn)) static void s_exit(int c){exit(c);}
#endif

enum { O_RDONLY_=0,O_WRONLY_=1,O_RDWR_=2,O_CREAT_=64,O_TRUNC_=512,O_APPEND_=1024,O_NONBLOCK_=2048 };
enum { SEEK_SET_=0,SEEK_CUR_=1,SEEK_END_=2 };
enum { PROT_READ_=1,PROT_WRITE_=2,MAP_SHARED_=1,MAP_PRIVATE_=2,MAP_ANONYMOUS_=32 };

/* Local control I/O. Linux 2.6 ARM OABI; host paths use native POSIX types. */
struct ControlPoll {int fd;short events,revents;};
static long control_read(int fd,void*b,unsigned n){
 long rc=s_read(fd,b,n);
#ifdef HOST_TEST
 if(rc<0){if(errno==EAGAIN||errno==EWOULDBLOCK)return -11;if(errno==EINTR)return -4;return -errno;}
#endif
 return rc;
}
static int control_nonblock(int fd){
#ifdef HOST_TEST
 return fcntl(fd,F_SETFL,O_NONBLOCK);
#else
 return (int)sc3(55,fd,4,O_NONBLOCK_);
#endif
}
static int control_fifo(const char*path){
#ifdef HOST_TEST
 if(mkfifo(path,0666)<0&&errno!=EEXIST)return -1;
 int fd=open(path,O_RDWR|O_NONBLOCK|O_NOFOLLOW);struct stat st;
 if(fd>=0&&(fstat(fd,&st)<0||!S_ISFIFO(st.st_mode))){close(fd);fd=-1;}
 return fd;
#else
 long rc=sc3(14,(long)path,0010666,0);if(rc<0&&rc!=-17)return -1;
 int fd=(int)s_open(path,O_RDWR_|O_NONBLOCK_|0x20000,0);if(fd<0)return -1;
 /* ARM stat64 (Linux 2.6.13 asm-arm/stat.h): st_mode is at byte 16. */
 u8 st[128] __attribute__((aligned(8)));rc=sc2(197,fd,(long)st);
 if(rc<0){s_close(fd);return -1;}
 u32 mode=(u32)st[16]|(u32)st[17]<<8|(u32)st[18]<<16|(u32)st[19]<<24;
 if((mode&0170000)!=0010000){s_close(fd);return -1;}return fd;
#endif
}
static int control_socket(void){
#ifdef HOST_TEST
 int fd=socket(AF_UNIX,SOCK_SEQPACKET,0);
#else
 unsigned long a[3]={1,5,0};int fd=(int)sc2(102,1,(long)a);
#endif
 if(fd>=0&&control_nonblock(fd)<0){s_close(fd);fd=-1;}return fd;
}
static int control_address(int fd,const char*path,int connect_peer){
#ifdef HOST_TEST
 struct sockaddr_un a;memset(&a,0,sizeof(a));a.sun_family=AF_UNIX;
 unsigned n=0;while(path[n]&&n+1<sizeof(a.sun_path)){a.sun_path[n]=path[n];n++;}
 if(path[n])return -36;
 int rc=connect_peer?connect(fd,(struct sockaddr*)&a,sizeof(a)):bind(fd,(struct sockaddr*)&a,sizeof(a));
 return rc<0?-errno:0;
#else
 struct {u16 family;char path[108];}a;a.family=1;
 unsigned n=0;while(path[n]&&n+1<sizeof(a.path)){a.path[n]=path[n];n++;}a.path[n]=0;if(path[n])return -36;
 unsigned long v[3]={(unsigned long)fd,(unsigned long)&a,2+n+1};
 return (int)sc2(102,connect_peer?3:2,(long)v);
#endif
}
static int control_listen(int fd){
#ifdef HOST_TEST
 return listen(fd,2);
#else
 unsigned long a[2]={(unsigned long)fd,2};return (int)sc2(102,4,(long)a);
#endif
}
static int control_accept(int fd){
#ifdef HOST_TEST
 int c=accept(fd,0,0);
#else
 unsigned long a[3]={(unsigned long)fd,0,0};int c=(int)sc2(102,5,(long)a);
#endif
 if(c>=0&&control_nonblock(c)<0){s_close(c);c=-1;}return c;
}
static int control_poll(struct ControlPoll*p,unsigned n){
#ifdef HOST_TEST
 return poll((struct pollfd*)p,n,0);
#else
 return (int)sc3(168,(long)p,n,0);
#endif
}

#ifndef HOST_TEST
#define CSW_FIFO_PATH "/dev/input/CswEvent0"
#define CSW_MAP_PATH "./cswmap.ini"
#define CSW_MAP_ALT "./clio843/cswmap.ini"
#define CSW_LOG_PATH "./clio843/csw.log"
#define VSTAT_SOCKET_PREFIX "/var/run/PAC"
#define FB_PATH "/dev/fb"
#define FONT_PATH "./clio843/font.bin"
#define MAP_PATH "./clio843/maps/map.cpm"
#define FOCUS_BUS "/tmp/cfbus"
#define LAST_PATH "./clio843/audlast.dat"
#define CAR_PATH "/tmp/cfstate"
#define GPS_PATH "/var/run/gpspipe"
#define PREF_PATH "./clio843/prefs.ini"
#define THEME_STATE_PATH "./clio843/theme.dat"
#define LASTGPS_PATH "./clio843/gpslast.dat"
#define PREF_TMP_PATH "./clio843/prefs.tmp"
#define RECENT_PATH "./clio843/recent.dat"
#define FAVES_PATH "./clio843/faves.dat"
#define TRIP_PATH "./clio843/trip.dat"
#define ROUTE_PATH "./clio843/maps/route.cpr"
#define QADJ_PATH "./clio843/maps/route.qadj"
#define STATE_LOG "./clio843/state.log"
#define ROUTING_STATUS_PATH "./clio843/routing-status.txt"
#define KEY_LOG "./clio843/key.log"
#define FOCUS_LOG "./clio843/focus.log"
#define BT_LOG "./clio843/bt.log"
#define DEBUG_STATE_PATH "/tmp/cldebug"
#define BT_ACTIVE_PATH "/tmp/clbt"
#define CAMERA_ALERT_PATH "/tmp/clalert"
#define CAMERA_ALERT_TMP "/tmp/clalert.new"
#define BT_STATUS_PATH "/tmp/clbtstat"
#define BT_META_PATH "/tmp/clbtmeta"
#define BT_NAV_PATH "/tmp/clnav"
#define BT_DEVLIST_PATH "/tmp/clbtdevices"
#define BT_NAMES_PATH "./clio843/btnames.dat"
#define BT_CMD_PATH "/tmp/clbtcmd"
#define OLD_AUDIO_LOG "./clio843/cpaud.log"
#define OLD_GPS_LOG "./clio843/gps.log"
#define WD_PATH "/dev/watchdog"
#define POWER_PATH "/proc/barcelona/onoff"
#define BL_MAX "/sys/class/backlight/s3c/max_brightness"
#define BL_VAL "/sys/class/backlight/s3c/brightness"
#else
#ifndef CLIO_HOST_ROOT
#define CLIO_HOST_ROOT "/tmp/clioplay-ui-host/"
#endif
#define CSW_FIFO_PATH CLIO_HOST_ROOT "CswEvent0"
#define CSW_MAP_PATH CLIO_HOST_ROOT "cswmap.ini"
#define CSW_MAP_ALT CLIO_HOST_ROOT "clio843/cswmap.ini"
#define CSW_LOG_PATH CLIO_HOST_ROOT "clio843/csw.log"
#define VSTAT_SOCKET_PREFIX CLIO_HOST_ROOT "PAC"
#define CAMERA_ALERT_PATH CLIO_HOST_ROOT "clalert"
#define CAMERA_ALERT_TMP CLIO_HOST_ROOT "clalert.new"
#define FB_PATH CLIO_HOST_ROOT "clio040.fb"
#define FONT_PATH CLIO_HOST_ROOT "clio843/font.bin"
#define MAP_PATH CLIO_HOST_ROOT "clio843/maps/map.cpm"
#define FOCUS_BUS CLIO_HOST_ROOT "cfbus_test"
#define LAST_PATH CLIO_HOST_ROOT "audlast_test.dat"
#define CAR_PATH CLIO_HOST_ROOT "cfstate"
#define GPS_PATH CLIO_HOST_ROOT "gpspipe"
#define PREF_PATH CLIO_HOST_ROOT "clio843/prefs.ini"
#define THEME_STATE_PATH CLIO_HOST_ROOT "clio040_theme.dat"
#define LASTGPS_PATH CLIO_HOST_ROOT "clio040_gpslast.dat"
#define PREF_TMP_PATH CLIO_HOST_ROOT "clio843/prefs.tmp"
#define RECENT_PATH CLIO_HOST_ROOT "recent_test.dat"
#define FAVES_PATH CLIO_HOST_ROOT "faves_test.dat"
#define TRIP_PATH CLIO_HOST_ROOT "trip_test.dat"
#define ROUTE_PATH CLIO_HOST_ROOT "clio843/maps/route.cpr"
#define QADJ_PATH CLIO_HOST_ROOT "clio843/maps/route.qadj"
#define STATE_LOG CLIO_HOST_ROOT "clio040_state.log"
#define ROUTING_STATUS_PATH CLIO_HOST_ROOT "routing-status.txt"
#define KEY_LOG CLIO_HOST_ROOT "clio040_key.log"
#define FOCUS_LOG CLIO_HOST_ROOT "clio040_focus.log"
#define BT_LOG CLIO_HOST_ROOT "clio040_bt.log"
#define DEBUG_STATE_PATH CLIO_HOST_ROOT "cldebug_test"
#define BT_ACTIVE_PATH CLIO_HOST_ROOT "clbt_test"
#define BT_STATUS_PATH CLIO_HOST_ROOT "clbtstat_test"
#define BT_META_PATH CLIO_HOST_ROOT "clbtmeta_test"
#define BT_NAV_PATH CLIO_HOST_ROOT "clnav_test"
#define BT_DEVLIST_PATH CLIO_HOST_ROOT "clbtdevices_test"
#define BT_NAMES_PATH CLIO_HOST_ROOT "btnames_test.dat"
#define BT_CMD_PATH CLIO_HOST_ROOT "clbtcmd_test"
#define OLD_AUDIO_LOG CLIO_HOST_ROOT "clio040_cpaud.log"
#define OLD_GPS_LOG CLIO_HOST_ROOT "clio040_gps.log"
#define WD_PATH CLIO_HOST_ROOT "watchdog"
#define POWER_PATH CLIO_HOST_ROOT "onoff"
#define BL_MAX CLIO_HOST_ROOT "maxbrightness"
#define BL_VAL CLIO_HOST_ROOT "brightness"
#endif

#define W 480
#define H 272
#define FB_PIX (W*H)
#define FONT_BYTES 9120

#include "boot_theme.h"
#include "last_fix.h"
/* Persisted AUTO hint, used only when date or position is unavailable. */
static int read_theme_state_raw(void){char c=0;long fd=s_open(THEME_STATE_PATH,O_RDONLY_,0);if(fd<0)return-1;long n=s_read((int)fd,&c,1);s_close((int)fd);return n==1&&c=='0'?0:(n==1&&c=='1'?1:-1);}
static void write_theme_state_raw(int night){char c=night?'1':'0',old=0;long fd=s_open(THEME_STATE_PATH,O_RDONLY_,0);if(fd>=0){long n=s_read((int)fd,&old,1);s_close((int)fd);if(n==1&&old==c)return;}fd=s_open(THEME_STATE_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd>=0){s_write((int)fd,&c,1);s_close((int)fd);}}

/* Modern dual-theme palette, RGB565. Geometry stays shared between themes,
 * while surfaces/focus treatments are allowed to differ through palette roles.
 * C_WHITE remains the primary text role even though it is dark in DAY mode. */
static u16 C_BG=0x0082,C_PANEL=0x1105,C_PANEL2=0x1146,C_BORDER=0x2a09;
static u16 C_CYAN=0x64ff,C_WHITE=0xf7bf,C_BLUE=0x3c97,C_MUTED=0x9d76;
static u16 C_DIM=0x63af,C_WARN=0xf524,C_ROAD=0x326c,C_WATER=0x1a4c,C_GREEN=0x2b4a;
static u16 C_HEADER=0x08c3,C_ACTIVE=0x1a2b,C_ACTIVE_TEXT=0xf7ff,C_ACCENT=0x46b9,C_SHADOW=0x0041,C_HILITE=0x21a7;
static u16 C_CAR=0xf4e8; /* Vehicle body follows the map theme. */
static u16 C_ROUTE_SKIPPED=0xf64b,C_ROUTE_DIVERSION=0x66d8;
static u16 C_ROUTE=0xfb36; /* Day raspberry / night bright pink; separate from roads and vehicle. */
/* Map road colours are independent of text, focus and HUD accent colours.
 * Low-to-high coverage slots preserve the existing six-plane junction blend. */
#define ROAD_RGB(r,g,b) (((r)>>3)<<11|((g)>>2)<<5|((b)>>3))
static const u16 road_palette[2][6]={
 {ROAD_RGB(240,246,250),ROAD_RGB(231,241,247),ROAD_RGB(244,249,252),
  ROAD_RGB(255,255,255),ROAD_RGB(116,169,181),ROAD_RGB(78,144,193)},
 {ROAD_RGB(49,77,96),ROAD_RGB(73,103,123),ROAD_RGB(106,135,153),
  ROAD_RGB(158,181,197),ROAD_RGB(78,123,141),ROAD_RGB(85,141,190)}
};
static u16 road_colours[6]={ROAD_RGB(49,77,96),ROAD_RGB(73,103,123),ROAD_RGB(106,135,153),
 ROAD_RGB(158,181,197),ROAD_RGB(78,123,141),ROAD_RGB(85,141,190)};
static const u16 map_canvas[2]={ROAD_RGB(213,224,230),ROAD_RGB(9,20,29)};
static u16 C_MAP_BG=ROAD_RGB(9,20,29);
#undef ROAD_RGB
static int theme_resolved=1; /* 0 DAY, 1 NIGHT */

static void apply_palette(int night){
  int next=night?1:0,changed=(next!=theme_resolved);theme_resolved=next;
  if(night){
    C_CAR=0xf4e8;C_ROUTE=0xfb36;C_ROUTE_SKIPPED=0xf64b;C_ROUTE_DIVERSION=0x66d8;
    /* NIGHT: deep charcoal-blue surfaces with a restrained teal focus colour. */
    C_BG=0x0082; C_PANEL=0x1105; C_PANEL2=0x1146; C_BORDER=0x2a09;
    C_CYAN=0x46b9; C_WHITE=0xf7bf; C_BLUE=0x3c97; C_MUTED=0x9d76;
    C_DIM=0x63af; C_WARN=0xf524; C_ROAD=0x326c; C_WATER=0x1a4c; C_GREEN=0x2b4a;
    C_HEADER=0x08c3; C_PANEL=0x10e4; C_PANEL2=0x1926; C_BORDER=0x39e9; C_CYAN=0x64ff; C_ACTIVE=0x1a2b; C_ACTIVE_TEXT=0xffff; C_ACCENT=0x64ff; C_SHADOW=0x0041; C_HILITE=0x21a7;
  }else{
    C_CAR=0xeb83;C_ROUTE=0xe150;C_ROUTE_SKIPPED=0xb4a0;C_ROUTE_DIVERSION=0x0bf0;
    /* DAY: warm/cool automotive greys rather than plain white inversion. */
    C_BG=0xef7d; C_PANEL=0xf7be; C_PANEL2=0xdf3c; C_BORDER=0xb618;
    C_CYAN=0x03f1; C_WHITE=0x1146; C_BLUE=0x33f3; C_MUTED=0x534e;
    C_DIM=0x7c51; C_WARN=0xb340; C_ROAD=0xadb6; C_WATER=0xa6bc; C_GREEN=0x9e55;
    C_HEADER=0xe73c; C_PANEL=0xf7be; C_PANEL2=0xe71c; C_BORDER=0xb618; C_CYAN=0x039b; C_ACTIVE=0x039b; C_ACTIVE_TEXT=0xffff; C_ACCENT=0x039b; C_SHADOW=0xb618; C_HILITE=0xffff;
  }
  for(int i=0;i<6;i++)road_colours[i]=road_palette[next][i];
  C_ROAD=road_colours[0];C_MAP_BG=map_canvas[next];
  if(changed)write_theme_state_raw(theme_resolved);
}

static u16 display_fb[FB_PIX] __attribute__((aligned(4)));
/* Array indirection preserves sizeof(fb) for the native renderer/test tools. */
static u16 (*render_fb)[FB_PIX]=&display_fb;
#define fb (*render_fb)
static int render_background_dirty=1;
static void render_checkpoint(void);
static void popup_reset(void);
static void popup_settle_for_capture(u32 now);
static int clip_x=0,clip_y=0,clip_right=W,clip_bottom=H;
static u8 font[FONT_BYTES];
static int font_ok=0;

static unsigned slen(const char*s){unsigned n=0;while(s&&s[n])n++;return n;}
static int streq(const char*a,const char*b){if(!a||!b)return 0;while(*a&&*a==*b){a++;b++;}return *a==*b;}
static int starts(const char*s,const char*p){while(*p){if(*s++!=*p++)return 0;}return 1;}
static void memzero(void*p,unsigned n){u8*d=(u8*)p;while(n--)*d++=0;}
static int atoi0(const char*s){int neg=0,v=0;if(*s=='-'){neg=1;s++;}while(*s>='0'&&*s<='9'){v=v*10+(*s-'0');s++;}return neg?-v:v;}
#include "utf8.h"
#ifndef CLIO_UNICODE
#define CLIO_UNICODE 0
#endif
#if CLIO_UNICODE
#include "global_text.h"
#endif
static void copystr(char*d,unsigned cap,const char*s){utf8_copy(d,cap,s,0xffffffffu);}
static void copystr_limit(char*d,unsigned cap,const char*s,unsigned maxc){utf8_copy(d,cap,s,maxc);}
static void appendstr(char*d,unsigned cap,const char*s){unsigned n=slen(d);if(n<cap)utf8_copy(d+n,cap-n,s,0xffffffffu);}
static void append_int(char*d,unsigned cap,int v){char t[16];unsigned n=0;if(v<0){appendstr(d,cap,"-");v=-v;}do{t[n++]=(char)('0'+v%10);v/=10;}while(v&&n<sizeof(t));while(n){char q[2]={t[--n],0};appendstr(d,cap,q);}}

static void log_append(const char*path,const char*s){long fd=s_open(path,O_WRONLY_|O_CREAT_|O_APPEND_,0666);if(fd<0)return;s_write((int)fd,s,slen(s));s_write((int)fd,"\n",1);s_close((int)fd);}

static void px(int x,int y,u16 c){if((unsigned)x<W&&(unsigned)y<H)fb[y*W+x]=c;}
static void fill(int x,int y,int w,int h,u16 c){int yy,xx;if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}if(x+w>W)w=W-x;if(y+h>H)h=H-y;if(w<=0||h<=0)return;for(yy=y;yy<y+h;yy++)for(xx=x;xx<x+w;xx++)fb[yy*W+xx]=c;}
static void rect(int x,int y,int w,int h,u16 c){int i;for(i=0;i<w;i++){px(x+i,y,c);px(x+i,y+h-1,c);}for(i=0;i<h;i++){px(x,y+i,c);px(x+w-1,y+i,c);}}
/* The compact skin shares only font/shape primitives with Next. The original
 * map geometry, shared screens and input state machine remain the UI baseline. */
#define CLIO_UI_ALPHA 1
static int hud_ink_alpha=16;
#include "ui_raster.h"
/* Fit complete UTF-8 code points, adding an ellipsis inside the measured cell. */
static void compact_fit(int x,int y,int w,int face,const char*s,u16 c){
 if(!s||w<=0)return;
#if CLIO_UNICODE
 if(cliou_ready()&&utf8_native(s)){label(x,y,face,s,c,w);return;}
#endif
 if(width(s,face)<=w){label(x,y,face,s,c,w);return;}
 char b[160];int used=0,advance=0,reserve=width("…",face);const char*p=s;
 if(reserve>w)return;
 while(*p){const char*start=p;int cp=cp_code(&p),bytes=(int)(p-start);int step=cp_glyphs[face*98+cp_slot(cp)].advance;
  if(advance+step+reserve>w||used+bytes+4>=(int)sizeof(b))break;
  while(start<p)b[used++]=*start++;advance+=step;
 }b[used]=0;appendstr(b,sizeof(b),"…");label(x,y,face,b,c,w);
}
#if CLIO_UNICODE
int cliou_open(const char*path){return(int)s_open(path,O_RDONLY_,0);}
int cliou_size(int fd){return(int)s_lseek(fd,0,2);}
int cliou_read(int fd,unsigned off,void*data,unsigned len){if(s_lseek(fd,(long)off,SEEK_SET_)<0)return 0;return s_read(fd,data,len)==(long)len;}
void*cliou_map(int fd,unsigned off,unsigned len){unsigned pagesize=4096;
#ifdef HOST_TEST
 pagesize=(unsigned)getpagesize();
#endif
 unsigned page=off&~(pagesize-1),delta=off-page;if(len>0x7fffffffu-delta)return 0;void*p=s_mmap2(0,len+delta,1,2,fd,page>>12);return p?(u8*)p+delta:0;}
void cliou_pixel(int x,int y,unsigned short colour,int alpha){blend_pixel(x,y,colour,alpha);}
#endif
static void compact_center(int cx,int y,int w,int face,const char*s,u16 c){int tw=width(s,face);compact_fit(cx-(tw<w?tw:w)/2,y,w,face,s,c);}
static void ui_surface(int x,int y,int w,int h,u16 c){round_rect(x,y,w,h,4,c,15);}
static void ui_panel(int x,int y,int w,int h,int active){ui_surface(x,y,w,h,C_PANEL);if(active)round_rect(x+6,y,w-12,2,1,C_CYAN,15);}
static void ui_choice(int x,int y,int w,int h,int selected){ui_surface(x,y,w,h,selected?C_ACTIVE:C_PANEL2);}
/* Proportional list position; no indicator when the complete list fits. */
struct UiScroll {int top,height;};
static struct UiScroll ui_scroll_geometry(int y,int height,int first,int visible,int total){
 if(height<=0||visible<=0||total<=visible)return (struct UiScroll){0};
 int thumb=(int)((long long)height*visible/total);if(thumb<8)thumb=8;if(thumb>height)thumb=height;
 int last=total-visible;if(first<0)first=0;if(first>last)first=last;
 return (struct UiScroll){y+(int)((long long)(height-thumb)*first/last),thumb};
}
static void ui_scrollbar(int x,int y,int height,int first,int visible,int total){
 struct UiScroll b=ui_scroll_geometry(y,height,first,visible,total);if(!b.height)return;
 /* Translucent rounded strokes preserve the backing glass and caller fade. */
 round_rect(x,y,4,height,2,C_MUTED,4);
 round_rect(x,b.top,4,b.height,2,C_CYAN,12);
}
static u16 ui_choice_text(int selected){return selected?C_ACTIVE_TEXT:C_WHITE;}
static void line1(int x0,int y0,int x1,int y1,u16 c,int wid){int dx=x1>x0?x1-x0:x0-x1,sx=x0<x1?1:-1;int dy=-(y1>y0?y1-y0:y0-y1),sy=y0<y1?1:-1,err=dx+dy;for(;;){int a,b,r=wid/2;for(a=-r;a<=r;a++)for(b=-r;b<=r;b++)px(x0+a,y0+b,c);if(x0==x1&&y0==y1)break;int e2=2*err;if(e2>=dy){err+=dy;x0+=sx;}if(e2<=dx){err+=dx;y0+=sy;}}}

static int load_font(void){long fd=s_open(FONT_PATH,O_RDONLY_,0);if(fd<0)return 0;long n=s_read((int)fd,font,FONT_BYTES);s_close((int)fd);font_ok=(n==FONT_BYTES);return font_ok;}
static void text(int x,int y,const char*s,u16 c,int scale){
 label(x,y,scale<=1?0:1,s,c,0);
}
static int textw(const char*s,int scale){return width(s,scale<=1?0:1);}
static void text_center(int cx,int y,const char*s,u16 c,int scale){text(cx-textw(s,scale)/2,y,s,c,scale);}
/* Fixed-width font helper used by the global header.  Metadata is shortened
 * before drawing so malformed/very long AVRCP strings can never enter the
 * reserved volume/temperature/time cells. */
static void text_fit(int x,int y,int maxw,const char*s,u16 c){
  char b[96];unsigned n=s?slen(s):0,maxc=maxw>0?(unsigned)(maxw/8):0;if(!s||!s[0]||!maxc)return;
  if(n<=maxc){text(x,y,s,c,1);return;}
  if(maxc>=4){unsigned keep=maxc-3;if(keep>sizeof(b)-4)keep=sizeof(b)-4;copystr_limit(b,sizeof(b),s,keep);appendstr(b,sizeof(b),"...");}
  else copystr_limit(b,sizeof(b),s,maxc);
  text(x,y,b,c,1);
}
static void text_center_fit(int cx,int y,int maxw,const char*s,u16 c){
  char b[96];unsigned n=s?slen(s):0,maxc=maxw>0?(unsigned)(maxw/8):0;if(!s||!s[0]||!maxc)return;
  if(n<=maxc){text_center(cx,y,s,c,1);return;}
  if(maxc>=4){unsigned keep=maxc-3;if(keep>sizeof(b)-4)keep=sizeof(b)-4;copystr_limit(b,sizeof(b),s,keep);appendstr(b,sizeof(b),"...");}
  else copystr_limit(b,sizeof(b),s,maxc);
  text_center(cx,y,b,c,1);
}

#include "boot_transition.h"

static int fb_flush(void){long fd=s_open(FB_PATH,O_WRONLY_|O_CREAT_,0666);if(fd<0)return 0;s_lseek((int)fd,0,SEEK_SET_);long n=s_write((int)fd,fb,sizeof(fb));s_close((int)fd);return n==(long)sizeof(fb);}

enum { THEME_DAY=0, THEME_NIGHT=1, THEME_AUTO=2 };
struct Prefs {int bright,hoff,moff,autozoom,debug,theme,units,cameras,clock_auto,language,day_start,night_start,top_swap,bottom_swap,learn_history,destination_predictions,diversions;};
static struct Prefs pref={50,0,0,1,0,THEME_AUTO,0,1,1,0,CLIO_DAY_START,CLIO_NIGHT_START,0,0,0,1,1};
static int pref_keyboard_language=-1;
/* The TomTom hardware export is authoritative when present; unknown hardware
 * keeps Bluetooth visible when the hardware export is unavailable. */
static int hw_bluetooth_enabled=1;
#ifdef HOST_TEST
#define CLIO_HW_EXPORT_PATH CLIO_HOST_ROOT "proc/barcelona/export_hw_var"
#else
#define CLIO_HW_EXPORT_PATH "/proc/barcelona/export_hw_var"
#endif
static void hardware_load_bluetooth(void){
 char data[4096];long fd=s_open(CLIO_HW_EXPORT_PATH,O_RDONLY_,0);
 if(fd<0)return;long n=s_read((int)fd,data,sizeof(data)-1);s_close((int)fd);
 if(n<=0)return;data[n]=0;
 for(char*line=data;*line;){
  char*end=line;while(*end&&*end!='\r'&&*end!='\n')end++;
  char sep=*end;*end=0;char*p=line;
  while(*p==' '||*p=='\t')p++;
  if(starts(p,"export ")){p+=7;while(*p==' '||*p=='\t')p++;}
  if(starts(p,"hw_bluetooth=")){
   p+=13;while(*p==' '||*p=='\t'||*p=='\"'||*p=='\'')p++;
   if(*p=='0')hw_bluetooth_enabled=0;
   else if(*p=='1')hw_bluetooth_enabled=1;
  }
  if(!sep)break;line=end+1;while(*line=='\r'||*line=='\n')line++;
 }
}
#include "ui_translations.h"
/* Internal unnamed-place sentinels are translated only at presentation. */
static const char*place_display_name(const char*s){if(!s)return "";if(streq(s,"MAP POINT"))return ui_tr("Map point");if(streq(s,"PHONE DESTINATION"))return ui_tr("Phone destination");return s;}
/* The source badge is BT in every language. Page titles and status messages
 * retain their translated phone wording outside English. */
static const char*bt_label(void){return pref.language==0?"Bluetooth":ui_tr("Phone connection");}
static const char*bt_source_label(void){return "BT";}
static const char*bt_unavailable_label(void){return pref.language==0?"Bluetooth unavailable":ui_tr("Phone connection unavailable");}
static int manual_zoom=5;
static int prefs_dirty=0,prefs_delay=0;
static void parse_pref_line(char*ln){char*eq=ln;while(*eq&&*eq!='=')eq++;if(!*eq)return;*eq++=0;if(streq(ln,"daystart")){pref.day_start=clio_theme_time(eq,CLIO_DAY_START);return;}if(streq(ln,"nightstart")){pref.night_start=clio_theme_time(eq,CLIO_NIGHT_START);return;}int v=atoi0(eq);if(streq(ln,"bright"))pref.bright=v;else if(streq(ln,"hoff"))pref.hoff=v;else if(streq(ln,"moff"))pref.moff=v;else if(streq(ln,"autozoom"))pref.autozoom=v?1:0;else if(streq(ln,"debug"))pref.debug=v?1:0;else if(streq(ln,"theme"))pref.theme=v;else if(streq(ln,"units"))pref.units=v;else if(streq(ln,"cameras"))pref.cameras=v>=0&&v<=2?v:1;else if(streq(ln,"clockauto"))pref.clock_auto=v?1:0;else if(streq(ln,"language"))pref.language=v>=0&&v<UI_LANGUAGE_COUNT?v:0;else if(streq(ln,"manualzoom"))manual_zoom=v;else if(streq(ln,"keyboardlanguage"))pref_keyboard_language=v>=0&&v<UI_LANGUAGE_COUNT?v:-1;
 else if(streq(ln,"hudtopswap"))pref.top_swap=v?1:0;
 else if(streq(ln,"hudbottomswap"))pref.bottom_swap=v?1:0;
 else if(streq(ln,"learnhistory"))pref.learn_history=v?1:0;
 else if(streq(ln,"destpredictions"))pref.destination_predictions=v?1:0;
 else if(streq(ln,"diversions"))pref.diversions=v?1:0;}
static void load_prefs(void){pref.day_start=CLIO_DAY_START;pref.night_start=CLIO_NIGHT_START;char b[512],ln[96];unsigned u=0;long fd=s_open(PREF_PATH,O_RDONLY_,0);if(fd>=0){long n=s_read((int)fd,b,sizeof(b));s_close((int)fd);if(n>0){for(long i=0;i<n;i++){char c=b[i];if(c=='\r'||c=='\n'){if(u){ln[u]=0;parse_pref_line(ln);u=0;}}else if(u+1<sizeof(ln))ln[u++]=c;}if(u){ln[u]=0;parse_pref_line(ln);}}}if(pref.bright<10)pref.bright=10;if(pref.bright>100)pref.bright=100;if(pref.theme<THEME_DAY||pref.theme>THEME_AUTO)pref.theme=THEME_AUTO;if(pref.units<0||pref.units>2)pref.units=0;if(pref.hoff*60+pref.moff< -720||pref.hoff*60+pref.moff>840)pref.hoff=pref.moff=0;if(manual_zoom<0)manual_zoom=0;if(manual_zoom>6)manual_zoom=6;if(!clio_theme_schedule_valid(pref.day_start,pref.night_start)){pref.day_start=CLIO_DAY_START;pref.night_start=CLIO_NIGHT_START;}}
/* Shared with the splash; the definition follows the wall-clock helpers. */
static void seed_boot_theme(void);
static void save_prefs(void){char b[512];b[0]=0;appendstr(b,sizeof(b),"version=8\nbright=");append_int(b,sizeof(b),pref.bright);appendstr(b,sizeof(b),"\nhoff=");append_int(b,sizeof(b),pref.hoff);appendstr(b,sizeof(b),"\nmoff=");append_int(b,sizeof(b),pref.moff);appendstr(b,sizeof(b),"\nautozoom=");append_int(b,sizeof(b),pref.autozoom);appendstr(b,sizeof(b),"\ndebug=");append_int(b,sizeof(b),pref.debug);appendstr(b,sizeof(b),"\ntheme=");append_int(b,sizeof(b),pref.theme);appendstr(b,sizeof(b),"\nunits=");append_int(b,sizeof(b),pref.units);appendstr(b,sizeof(b),"\ncameras=");append_int(b,sizeof(b),pref.cameras);appendstr(b,sizeof(b),"\nclockauto=");append_int(b,sizeof(b),pref.clock_auto);appendstr(b,sizeof(b),"\nlanguage=");append_int(b,sizeof(b),pref.language);appendstr(b,sizeof(b),"\ndaystart=");append_int(b,sizeof(b),pref.day_start);appendstr(b,sizeof(b),"\nnightstart=");append_int(b,sizeof(b),pref.night_start);appendstr(b,sizeof(b),"\nmanualzoom=");append_int(b,sizeof(b),manual_zoom);appendstr(b,sizeof(b),"\nkeyboardlanguage=");append_int(b,sizeof(b),pref_keyboard_language);
 appendstr(b,sizeof(b),"\nhudtopswap=");append_int(b,sizeof(b),pref.top_swap);
 appendstr(b,sizeof(b),"\nhudbottomswap=");append_int(b,sizeof(b),pref.bottom_swap);
 appendstr(b,sizeof(b),"\nlearnhistory=");append_int(b,sizeof(b),pref.learn_history);
 appendstr(b,sizeof(b),"\ndestpredictions=");append_int(b,sizeof(b),pref.destination_predictions);
 appendstr(b,sizeof(b),"\ndiversions=");append_int(b,sizeof(b),pref.diversions);
 appendstr(b,sizeof(b),"\n");long fd=s_open(PREF_TMP_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd>=0){s_write((int)fd,b,slen(b));s_close((int)fd);s_sync();if(s_rename(PREF_TMP_PATH,PREF_PATH)<0){fd=s_open(PREF_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd>=0){s_write((int)fd,b,slen(b));s_close((int)fd);}}s_sync();}prefs_dirty=0;prefs_delay=0;}
static void prefs_mark_dirty(void){prefs_dirty=1;prefs_delay=10;}
static void prefs_service(void){if(!prefs_dirty)return;if(prefs_delay>0){prefs_delay--;return;}save_prefs();}

static void remove_stale_logs(void){
  s_unlink(ROUTING_STATUS_PATH);
  s_unlink(STATE_LOG); s_unlink(KEY_LOG); s_unlink(FOCUS_LOG);
  s_unlink(BT_LOG); s_unlink(CSW_LOG_PATH); s_unlink(OLD_AUDIO_LOG); s_unlink(OLD_GPS_LOG);
}
/* Shared volatile debug gate for helper daemons.  Publish the new state before
 * deleting logs so an independently-running producer cannot recreate them. */
static void debug_state_write(void){char b[2];b[0]=pref.debug?'1':'0';b[1]='\n';long fd=s_open(DEBUG_STATE_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd>=0){s_write((int)fd,b,2);s_close((int)fd);}}
static void debug_log(const char*path,const char*s){if(pref.debug)log_append(path,s);}

static int read_int_file(const char*p,int def){char b[24];long fd=s_open(p,O_RDONLY_,0);if(fd<0)return def;long n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);if(n<=0)return def;b[n]=0;return atoi0(b);}
static void write_int_file(const char*p,int v){char b[16];b[0]=0;append_int(b,sizeof(b),v);long fd=s_open(p,O_WRONLY_,0);if(fd>=0){s_write((int)fd,b,slen(b));s_close((int)fd);}}
static void apply_brightness(void){int mx=read_int_file(BL_MAX,100);if(mx<1)mx=100;write_int_file(BL_VAL,(pref.bright*mx+50)/100);}

static u32 udiv32(u32 n,u32 d);

struct GPS {int fix,sats; s32 lat,lon; int mph10,course; int hour,min,sec,day,mon,year;};
static struct GPS gps;static u32 gps_fix_ms=0;
#define DISPLAY_COURSE_UPDATE_MPH10 30 /* >=3.0 mph: GPS course is useful; below this freeze the last moving orientation. */

/* persist one tiny trusted last-fix record so a cold start can show
 * useful map context while the receiver reacquires satellites.  This is never
 * exposed as live GPS: speed/road/car marker remain suppressed until gps.fix. */
static int lastfix_valid=0,lastfix_live_seen=0;
static s32 lastfix_lat=0,lastfix_lon=0;static int lastfix_course=0;
static int load_last_fix(void){struct LastFixDisk d;if(!lastfix_read(LASTGPS_PATH,&d))return 0;lastfix_lat=d.lat;lastfix_lon=d.lon;lastfix_course=(int)d.course;lastfix_valid=1;return 1;}
#ifdef CLIO_THEME_TEST
static long theme_test_utc=-1;
#endif
static long theme_wall_clock_utc(void){
#ifdef CLIO_THEME_TEST
 return theme_test_utc;
#elif !defined(HOST_TEST)
 struct ktv v;return sc2(78,(long)&v,0)>=0?v.sec:-1;
#else
 time_t t=time(0);return t==(time_t)-1?-1:(long)t;
#endif
}
static void seed_boot_theme(void){apply_palette(clio_boot_night(pref.theme,theme_wall_clock_utc(),pref.hoff*60+pref.moff,pref.day_start,pref.night_start,read_theme_state_raw()));}
static int save_last_fix(void){if(!lastfix_valid)return 0;struct LastFixDisk d;d.magic[0]='L';d.magic[1]='G';d.magic[2]='P';d.magic[3]='1';d.lat=lastfix_lat;d.lon=lastfix_lon;d.course=(u16)(lastfix_course>=0&&lastfix_course<=359?lastfix_course:0);d.pad=0;d.check=lastfix_check(d.lat,d.lon,d.course);long fd=s_open(LASTGPS_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd<0)return-1;long n=s_write((int)fd,&d,sizeof(d));s_close((int)fd);return n==(long)sizeof(d)?1:-1;}
static void remember_live_fix(void){if(!gps.fix)return;lastfix_lat=gps.lat;lastfix_lon=gps.lon;/* Preserve the last trustworthy moving orientation instead of overwriting it with stationary GPS-course noise. */if(!lastfix_valid||gps.mph10>=DISPLAY_COURSE_UPDATE_MPH10)lastfix_course=gps.course;lastfix_valid=1;if(!lastfix_live_seen){save_last_fix();lastfix_live_seen=1;}}
static int parse_dec10(const char*s){int v=0,frac=0,neg=0;if(*s=='-'){neg=1;s++;}while(*s>='0'&&*s<='9'){v=v*10+(*s-'0');s++;}if(*s=='.'&&s[1]>='0'&&s[1]<='9')frac=s[1]-'0';return neg?-(v*10+frac):(v*10+frac);}
static s32 nmea_coord_e5(const char*s,int islon,char hemi){int deg=0;int digs=islon?3:2;for(int i=0;i<digs;i++){if(s[i]<'0'||s[i]>'9')return 0;deg=deg*10+s[i]-'0';}s+=digs;int min10k=0;int whole=0;while(*s>='0'&&*s<='9'){whole=whole*10+(*s-'0');s++;}min10k=whole*10000;if(*s=='.'){s++;int m=1000;while(*s>='0'&&*s<='9'&&m){min10k+=(*s-'0')*m;m/=10;s++;}}s32 e5=(s32)deg*100000+(s32)(min10k/6);if(hemi=='S'||hemi=='W')e5=-e5;return e5;}
static void parse_hhmmss(const char*s){
 for(int i=0;i<6;i++)if(s[i]<'0'||s[i]>'9')return;
 int hour=(s[0]-'0')*10+s[1]-'0',minute=(s[2]-'0')*10+s[3]-'0',second=(s[4]-'0')*10+s[5]-'0';
 if(hour>23||minute>59||second>60)return;
 gps.hour=hour;gps.min=minute;gps.sec=second;
}
static void parse_ddmmyy(const char*s){if(slen(s)<6)return;gps.day=(s[0]-'0')*10+s[1]-'0';gps.mon=(s[2]-'0')*10+s[3]-'0';gps.year=2000+(s[4]-'0')*10+s[5]-'0';}

/* Calendar helpers are shared with the offline local clock. */
static int leap_year(int y){return clio_leap_year(y);}
static int day_of_year0(void){return clio_day_of_year(gps.year,gps.mon,gps.day);}
#include "global_clock.h"
static int local_clock_minutes(void){if(gps.hour<0||gps.hour>23||gps.min<0||gps.min>59)return-1;int mt=gps.hour*60+gps.min+pref.hoff*60+pref.moff;while(mt<0)mt+=1440;while(mt>=1440)mt-=1440;return mt;}
static int auto_is_night(void){
 int minute=local_clock_minutes();
 if(minute<0)minute=clio_theme_local_minute(theme_wall_clock_utc(),pref.hoff*60+pref.moff);
 int night=clio_schedule_night(minute,pref.day_start,pref.night_start);
 return night>=0?night:theme_resolved;
}
static void refresh_theme(void){int night=pref.theme==THEME_NIGHT?1:(pref.theme==THEME_DAY?0:auto_is_night());if(night!=theme_resolved)apply_palette(night);}
static int split_csv(char*s,char**f,int max){int n=0;f[n++]=s;while(*s&&n<max){if(*s==','){*s=0;f[n++]=s+1;}s++;}return n;}
static u32 mob_epoch(int,int,int,int,int,int);
static void mob_rmc(u32,u32,int,s32,s32,int,int);
static int mobility_digits(const char*s,int n){for(int i=0;i<n;i++)if(!s[i]||s[i]<'0'||s[i]>'9')return 0;return 1;}
static void parse_nmea(char*ln){char*f[20];int n;if(starts(ln,"$GPRMC,")||starts(ln,"$GNRMC,")){n=split_csv(ln,f,20);if(n>9){parse_hhmmss(f[1]);if(f[2][0]=='A'){gps.fix=1;gps_fix_ms=s_now_ms();gps.lat=nmea_coord_e5(f[3],0,f[4][0]);gps.lon=nmea_coord_e5(f[5],1,f[6][0]);int k10=parse_dec10(f[7]);gps.mph10=(k10*1151+500)/1000;gps.course=parse_dec10(f[8])/10;if(gps.course<0)gps.course=0;if(gps.course>359)gps.course%=360;}else{gps.fix=0;gps.mph10=0;}parse_ddmmyy(f[9]);
 u32 epoch=0xffffffffu;if(mobility_digits(f[1],6)&&mobility_digits(f[9],6))epoch=mob_epoch(2000+(f[9][4]-'0')*10+f[9][5]-'0',(f[9][2]-'0')*10+f[9][3]-'0',(f[9][0]-'0')*10+f[9][1]-'0',(f[1][0]-'0')*10+f[1][1]-'0',(f[1][2]-'0')*10+f[1][3]-'0',(f[1][4]-'0')*10+f[1][5]-'0');
 int valid=gps.fix&&mobility_digits(f[3],4)&&mobility_digits(f[5],5)&&(f[4][0]=='N'||f[4][0]=='S')&&(f[6][0]=='E'||f[6][0]=='W');
 mob_rmc(s_now_ms(),epoch,valid,gps.lat,gps.lon,gps.mph10,gps.course);
 }}
else if(starts(ln,"$GPGGA,")||starts(ln,"$GNGGA,")){n=split_csv(ln,f,20);if(n>7){parse_hhmmss(f[1]);int q=atoi0(f[6]);gps.sats=atoi0(f[7]);if(q>0){gps.fix=1;gps.lat=nmea_coord_e5(f[2],0,f[3][0]);gps.lon=nmea_coord_e5(f[4],1,f[5][0]);}else gps.fix=0;}}
}

static char gpsbuf[1024],gpsline[256];static unsigned gpsused=0;static int gpsfd=-1;
static void poll_gps(void){if(gpsfd<0)gpsfd=(int)s_open(GPS_PATH,O_RDONLY_|O_NONBLOCK_,0);if(gpsfd<0)return;for(;;){long n=s_read(gpsfd,gpsbuf,sizeof(gpsbuf));if(n<=0)break;for(long i=0;i<n;i++){char c=gpsbuf[i];if(c=='\n'||c=='\r'){if(gpsused){gpsline[gpsused]=0;parse_nmea(gpsline);gpsused=0;}}else if(gpsused+1<sizeof(gpsline))gpsline[gpsused++]=c;else gpsused=0;}}}

/* One current transient per domain; repeated knob events replace the value
 * without replaying the entrance animation. Millisecond subtraction wraps. */
enum {HUD_NONE,HUD_SOURCE,HUD_VOLUME,HUD_MANUAL_ZOOM,HUD_JOURNEY,HUD_HEADING,HUD_FAILURE,HUD_MEDIA,HUD_CALC,HUD_NOTICE,HUD_CONNECTION,HUD_BT_FEEDBACK,HUD_SYSTEM,HUD_TRACK,HUD_EXPLORE,HUD_AUTO_ZOOM,HUD_TRAFFIC};
#ifdef HOST_TEST
static int hud_capture_clock;
static u32 hud_capture_ms;
static u32 hud_now_ms(void){return hud_capture_clock?hud_capture_ms:s_now_ms();}
#else
#define hud_now_ms s_now_ms
#endif
enum {HUD_INFO,HUD_SUCCESS,HUD_WARNING,HUD_ERROR};
static int hud_bt_level;
struct HudPulse {u32 since,changed,until;int kind;};
static struct HudPulse hud_audio,hud_source,hud_navigation,hud_connection,hud_bt_feedback;
static char hud_connected_name[33],hud_bt_text[96];
static int hud_pulse_live(const struct HudPulse*p,u32 now){return p->kind&&(s32)(p->until-now)>0;}
static void hud_pulse_at(struct HudPulse*p,int kind,u32 now,u32 ms){
 if(!hud_pulse_live(p,now))p->since=now;
 p->kind=kind;p->changed=now;p->until=now+ms;
}
static void hud_pulse(struct HudPulse*p,int kind,u32 ms){hud_pulse_at(p,kind,hud_now_ms(),ms);}
static void hud_bt_notice(const char*message){
 hud_bt_level=streq(message,ui_tr("Disconnected"))||streq(message,ui_tr("Reconnecting"))?HUD_WARNING:
  streq(message,ui_tr("Device forgotten"))?HUD_SUCCESS:HUD_INFO;
 copystr(hud_bt_text,sizeof(hud_bt_text),message);
 hud_pulse(&hud_bt_feedback,HUD_BT_FEEDBACK,2500);
}

struct Car {char source[32],volume[16];int temp,tempok;};static struct Car car;
static int car_temp_valid(int c){return c>=-60&&c<=85;}
static struct {int focus,focus_valid,vehicle,vehicle_valid,vehicle_connected;} temperature;
static void temperature_select(void){
 car.tempok=temperature.focus_valid||(temperature.vehicle_connected&&temperature.vehicle_valid);
 if(temperature.focus_valid)car.temp=temperature.focus;
 else if(car.tempok)car.temp=temperature.vehicle;
 static int last_source=-1,last_value;
 if(!pref.debug){last_source=-1;return;}
 int source=temperature.focus_valid?1:car.tempok?2:0;
 if(source!=last_source||(source&&car.temp!=last_value)){
  char b[80];copystr(b,sizeof(b),source==1?"TEMPERATURE FOCUS":source==2?"TEMPERATURE VEHICLESTATUS":"TEMPERATURE UNAVAILABLE");
  if(source){appendstr(b,sizeof(b)," ");append_int(b,sizeof(b),car.temp);}debug_log(STATE_LOG,b);
  last_source=source;last_value=car.temp;
 }
}
static int reading_present(const char*s){
 if(!s)return 0;
 while(*s==' '||*s=='\t'||*s=='-'||*s=='\r'||*s=='\n')s++;
 return *s!=0;
}

/* Direct Focus audio-state consumer. This is the former clioaudio parsing logic
 * folded into the main runtime: no helper process, /tmp/cpaudio, or state.log
 * shutdown handshake. focusd writes the volatile /tmp/cfbus stream; ClioPlay
 * reconstructs source/station/frequency/volume itself and caches the final
 * display state directly to audlast.dat on orderly power-down. */
struct AudioFeed {char band[8],freq[16],station[24];};static struct AudioFeed af,audio_presented;
static int audio_source_live=0;
static int bt_source_selected(void);
static int audio_source_detected(void){return audio_source_live&&reading_present(af.band);}
static int audiofd=-1;static char audiobuf[4096],audioline[512];static unsigned audioused=0;
static char upc(char c){return(c>='a'&&c<='z')?(char)(c-32):c;}
static void copy_trim(char*out,unsigned cap,const u8*p,unsigned n){unsigned a=0,b=n,k=0;while(a<b&&(p[a]==' '||p[a]=='\t'||p[a]==0))a++;while(b>a&&(p[b-1]==' '||p[b-1]=='\t'||p[b-1]==0))b--;while(a<b&&k+1<cap){u8 c=p[a++];out[k++]=(c>=32&&c<127)?(char)c:' ';}out[k]=0;}
static int setstrx(char*dst,unsigned cap,const char*src,int upper){char t[40];unsigned i=0;while(src[i]&&i+1<cap&&i+1<sizeof(t)){t[i]=upper?upc(src[i]):src[i];i++;}t[i]=0;if(streq(dst,t))return 0;copystr(dst,cap,t);return 1;}
static int set_band(const char*b){
 if(!reading_present(b)){
  int was_bt=bt_source_selected();
  audio_source_live=0;
  af.band[0]=af.freq[0]=af.station[0]=car.source[0]=0;
  hud_audio.kind=HUD_NONE;
  /* Missing radio data selects BT status, without restarting live track notices. */
  if(!was_bt)hud_pulse(&hud_source,HUD_SOURCE,2300);
  return 0;
 }
 int changed=setstrx(af.band,sizeof(af.band),b,1);
 if(!audio_source_live||changed)hud_pulse(&hud_source,HUD_SOURCE,2300);
 audio_source_live=1;
 if(changed)af.freq[0]=af.station[0]=0;
 return changed;
}
static int parse_uint_tail(const char*s){int v=-1,cur=0,seen=0;unsigned i=0;while(s[i]){if(s[i]>='0'&&s[i]<='9'){cur=cur*10+(s[i]-'0');seen=1;}else if(seen){v=cur;cur=0;seen=0;}i++;}if(seen)v=cur;return v;}
static int looks_tuning_label(const char*s){return s[0]=='<'||s[0]=='>'||s[0]=='[';}
static void rebuild_source(void){
 if(!reading_present(af.band))return;
 char out[32];copystr(out,sizeof(out),af.band);
 if(streq(af.band,"FM")||streq(af.band,"MW")||streq(af.band,"LW")){
  const char*detail=reading_present(af.station)?af.station:af.freq;
  if(reading_present(detail)){appendstr(out,sizeof(out)," ");appendstr(out,sizeof(out),detail);}
 }
 int changed=setstrx(car.source,sizeof(car.source),out,0);
 int radio=streq(af.band,"FM")||streq(af.band,"MW")||streq(af.band,"LW");
 int metadata=radio&&((reading_present(af.freq)&&!streq(af.freq,audio_presented.freq))||
                     (reading_present(af.station)&&!streq(af.station,audio_presented.station)));
 audio_presented=af;
 if((changed||metadata)&&audio_source_detected())hud_pulse(&hud_source,HUD_SOURCE,4200);
}
static void set_volume(int v){
 char out[16]="VOL ";
 if(v<0)v=0;if(v>99)v=99;
 append_int(out,sizeof(out),v);
 if(setstrx(car.volume,sizeof(car.volume),out,0)&&audio_source_detected())
  hud_pulse(&hud_audio,HUD_VOLUME,1800);
}
static int valid_saved_char(u8 c){return c==0||(c>=32&&c<127);}
static void infer_band_from_source(void){if(starts(car.source,"FM"))setstrx(af.band,sizeof(af.band),"FM",1);else if(starts(car.source,"MW"))setstrx(af.band,sizeof(af.band),"MW",1);else if(starts(car.source,"LW"))setstrx(af.band,sizeof(af.band),"LW",1);else if(starts(car.source,"CD"))setstrx(af.band,sizeof(af.band),"CD",1);else if(starts(car.source,"AUX"))setstrx(af.band,sizeof(af.band),"AUX",1);}
static int load_last_audio(void){u8 rec[48];unsigned i;long fd=s_open(LAST_PATH,O_RDONLY_,0);if(fd<0)return 0;long n=s_read((int)fd,rec,sizeof(rec));s_close((int)fd);if(n!=(long)sizeof(rec))return 0;for(i=0;i<sizeof(rec);i++)if(!valid_saved_char(rec[i]))return 0;for(i=0;i<31&&rec[i];i++)car.source[i]=(char)rec[i];car.source[i]=0;for(i=0;i<15&&rec[32+i];i++)car.volume[i]=(char)rec[32+i];car.volume[i]=0;if(!car.source[0]&&!car.volume[0])return 0;infer_band_from_source();return 1;}
static int save_last_audio(void){u8 rec[48];unsigned i;memzero(rec,sizeof(rec));for(i=0;i<31&&car.source[i];i++)rec[i]=(u8)car.source[i];for(i=0;i<15&&car.volume[i];i++)rec[32+i]=(u8)car.volume[i];if(!car.source[0]&&!car.volume[0])return 0;long fd=s_open(LAST_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd<0)return-1;long n=s_write((int)fd,rec,sizeof(rec));s_close((int)fd);return n==(long)sizeof(rec)?1:-1;}
static int hexv(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return-1;}
static unsigned decode_rx(const char*line,u8*out,unsigned cap){unsigned i=0,n=0;while(line[i]&&line[i]!=':')i++;if(line[i]!=':')return 0;i++;while(line[i]){while(line[i]==' '||line[i]=='\t')i++;int a=hexv(line[i]),b=hexv(line[i+1]);if(a<0||b<0)break;if(n<cap)out[n++]=(u8)((a<<4)|b);else break;i+=2;}return n;}
static int process_audio_source(const char*t){
 if(!t[0])return 0;
 if(!reading_present(t)||streq(t,"UNKNOWN")||streq(t,"unknown"))return set_band("");
 if(starts(t,"AUDIO VOLUME")||starts(t,"NAV VOLUME")){
  int v=parse_uint_tail(t);if(v>=0)set_volume(v);
  return 0;
 }
 if(streq(t,"FM RADIO")||streq(t,"RADIO FM"))return set_band("FM");
 if(streq(t,"MW RADIO")||streq(t,"RADIO MW"))return set_band("MW");
 if(streq(t,"LW RADIO")||streq(t,"RADIO LW"))return set_band("LW");
 if(streq(t,"MONO CD")||streq(t,"<NO CD>")||streq(t,"CD"))return set_band("CD");
 if(streq(t,"AUX")||streq(t,"AUXILIARY"))return set_band("AUX");
 return 0;
}
static void process_property(unsigned id,const u8*p,unsigned n,int*srcchg){
 char t[40];copy_trim(t,sizeof(t),p,n);
 if(id==100||id==40||id==60){*srcchg|=process_audio_source(t);return;}
 if(!reading_present(t))return;
 if(id==65)*srcchg|=setstrx(af.freq,sizeof(af.freq),t,0);
 if(id==66){
  if(looks_tuning_label(t)){if(af.station[0]){af.station[0]=0;*srcchg=1;}return;}
  *srcchg|=setstrx(af.station,8,t,1);
 }
}
static void process_rx_line(const char*line){u8 b[192];unsigned n=decode_rx(line,b,sizeof(b));if(n<5)return;unsigned i=0;int changed=0;while(i+5<=n){if((b[i]&0x7f)==13&&i+5<=n){unsigned len=b[i+4];if(i+5+len<=n){if(b[i+3]==1)process_property(b[i+2],b+i+5,len,&changed);i+=5+len;continue;}}i++;}if(changed)rebuild_source();}
static const char* after_text(const char*line){const char*k="text=\"";unsigned i=0,j;while(line[i]){j=0;while(k[j]&&line[i+j]==k[j])j++;if(!k[j])return line+i+j;i++;}return 0;}
static void process_decoded_line(const char*line){
 if(!(starts(line,"SOURCE_STATUS")||starts(line,"SOURCE_TEXT")||starts(line,"SOURCE_MODE")))return;
 const char*s=after_text(line);if(!s)return;
 unsigned n=0;while(n<39&&s[n]&&s[n]!='"')n++;
 char text[40];copy_trim(text,sizeof(text),(const u8*)s,n);
 if(process_audio_source(text))rebuild_source();
}
static void process_audio_line(char*line){if(pref.debug)log_append(FOCUS_LOG,line);if(line[0]=='R'&&line[1]=='X'&&line[2]=='#')process_rx_line(line);else process_decoded_line(line);}
static void poll_audio(void){if(audiofd<0){audiofd=(int)s_open(FOCUS_BUS,O_RDONLY_|O_NONBLOCK_,0);if(audiofd<0)return;}for(;;){long n=s_read(audiofd,audiobuf,sizeof(audiobuf));if(n<=0){if(n<0){s_close(audiofd);audiofd=-1;audioused=0;}break;}for(long i=0;i<n;i++){char c=audiobuf[i];if(c=='\r'||c=='\n'){if(audioused){audioline[audioused]=0;process_audio_line(audioline);audioused=0;}}else if(audioused+1<sizeof(audioline))audioline[audioused++]=c;else audioused=0;}}}
static int focus_temperature(const char*b,int*out){
 while(*b&&!starts(b,"temp=")){while(*b&&*b!='\n')b++;if(*b)b++;}
 if(!*b)return 0;b+=5;while(*b==' '||*b=='\t')b++;
 int sign=*b=='-'?-1:1;if(*b=='-'||*b=='+')b++;
 if(*b<'0'||*b>'9')return 0;
 int value=0;do{value=value*10+(*b++-'0');if(value>85)return 0;}while(*b>='0'&&*b<='9');
 while(*b==' '||*b=='\t')b++;
 if(*b&&*b!='\r'&&*b!='\n')return 0;value*=sign;
 if(!car_temp_valid(value))return 0;*out=value;return 1;
}
static void car_read_temperature(const char*b){
 temperature.focus_valid=focus_temperature(b,&temperature.focus);temperature_select();
}
static void poll_car(void){
 char b[256];long fd=s_open(CAR_PATH,O_RDONLY_,0);
 if(fd<0){temperature.focus_valid=0;temperature_select();return;}
 long n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);
 if(n<=0){temperature.focus_valid=0;temperature_select();return;}
 b[n]=0;car_read_temperature(b);
}

/* Bluetooth media UI/daemon boundary.
 *
 * The Renault head unit remains in its stable no-disc CD source internally,
 * but CLIOPLAY treats that source as BLUETOOTH everywhere. AUX stays wired AUX.
 * With no valid radio feed, Bluetooth is available independently of radio
 * source/volume telemetry. Audio ownership is
 * deliberately outside clio040: the clioa2dp daemon owns AVDTP/SBC, AVRCP and
 * /dev/barc-snd.  This process only publishes whether the Bluetooth source is
 * selected, consumes daemon state/metadata, and emits small idempotent commands.
 *
 * IPC is volatile (/tmp) and therefore cannot corrupt persistent settings:
 *   /tmp/clbt      : "1" for CD/BT or no readable radio source; else "0"
 *   /tmp/clbtstat  : one-line daemon status (BT CONNECTED/STREAMING/...)
 *   /tmp/clbtmeta  : KEY=VALUE lines (DEVICE/TITLE/ARTIST/ALBUM/...)
 *   /tmp/clbtcmd   : monotonically sequenced "<n> <COMMAND>" command record
 */
enum {SCR_MAP=0,SCR_MENU=1,SCR_TRIP=2,SCR_BT=3,SCR_SET=4,SCR_KEY=5,SCR_COORDINATES=6};static int screen=SCR_MAP;
enum {MENU_BODY_Y=42};
struct BtUi {
  char status[28],device[33],device_addr[18],title[193],artist[161],album[161];
  int paired,connected,playing,pos,dur;
};
struct BtDeviceUi {char addr[18],name[33];int connected;};
struct BtNameCache {char addr[18],name[33];};
#define BT_UI_MAX_DEVICES 10
static struct BtUi btui;
static struct BtDeviceUi btdev[BT_UI_MAX_DEVICES];
static struct BtNameCache btname_cache[BT_UI_MAX_DEVICES];
static int nbtdev=0,nbtname_cache=0;
static int bt_active_written=-1;
static u32 bt_cmd_seq=1;
static char bt_forget_addr[18];
static u32 bt_forget_at;
/* Temporary remote-control context used only by the normal navigation map. */
static int bt_control_mode=0;
enum {BT_TRANSPORT_PREVIOUS=1,BT_TRANSPORT_PLAY_PAUSE,BT_TRANSPORT_NEXT};
enum {BT_PRESS_MS=420,BT_PLAYBACK_REPLY_MS=1500};
static struct HudPulse bt_transport_press;
static int bt_playback_request=-1;
static u32 bt_playback_requested_at;

static int bt_source_selected(void){
 return hw_bluetooth_enabled&&(!audio_source_detected()||streq(af.band,"CD"));
}
static int bt_controls_available(void){return hw_bluetooth_enabled&&btui.connected&&bt_source_selected();}
static int bt_write_small(const char*path,const char*s){
 long fd=s_open(path,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);
 if(fd<0)return 0;
 unsigned length=slen(s);
 long written=s_write((int)fd,s,length);
 s_close((int)fd);
 return written==(long)length;
}
static void bt_source_service(void){
 int on=bt_source_selected();if(on==bt_active_written)return;
 if(!bt_write_small(BT_ACTIVE_PATH,on?"1\n":"0\n"))return;
 bt_active_written=on;
 if(on&&!audio_source_detected()&&!hud_pulse_live(&hud_source,hud_now_ms()))
  hud_pulse(&hud_source,HUD_SOURCE,2300);
 if(pref.debug)debug_log(STATE_LOG,!on?"BT SOURCE INACTIVE":audio_source_detected()?"BT SOURCE ACTIVE (CD)":"BT SOURCE ACTIVE (DEFAULT)");
}
static int bt_command(const char*cmd){
 char record[48]="";
 append_int(record,sizeof(record),(int)bt_cmd_seq++);
 appendstr(record,sizeof(record)," ");
 appendstr(record,sizeof(record),cmd);
 appendstr(record,sizeof(record),"\n");
 int written=bt_write_small(BT_CMD_PATH,record);
 if(pref.debug){
  char line[64]="BT CMD ";
  appendstr(line,sizeof(line),cmd);debug_log(STATE_LOG,line);
 }
 return written;
}
static int bt_transport_playing(u32 now){
 if(btui.connected&&bt_playback_request>=0&&now-bt_playback_requested_at<BT_PLAYBACK_REPLY_MS)
  return bt_playback_request;
 return btui.playing;
}
static void bt_transport_command(int button){
 u32 now=hud_now_ms();
 hud_pulse_at(&bt_transport_press,button,now,BT_PRESS_MS);
 if(button==BT_TRANSPORT_PLAY_PAUSE){
  int play=!bt_transport_playing(now);
  if(bt_command(play?"PLAY":"PAUSE")&&btui.connected){
   bt_playback_request=play;
   bt_playback_requested_at=now;
  }
 }else bt_command(button==BT_TRANSPORT_PREVIOUS?"PREV":"NEXT");
}
static void bt_command_arg(const char*cmd,const char*arg){char b[80];b[0]=0;append_int(b,sizeof(b),(int)bt_cmd_seq++);appendstr(b,sizeof(b)," ");appendstr(b,sizeof(b),cmd);if(arg&&arg[0]){appendstr(b,sizeof(b)," ");appendstr(b,sizeof(b),arg);}appendstr(b,sizeof(b),"\n");bt_write_small(BT_CMD_PATH,b);if(pref.debug){char l[96];l[0]=0;appendstr(l,sizeof(l),"BT CMD ");appendstr(l,sizeof(l),cmd);if(arg&&arg[0]){appendstr(l,sizeof(l)," ");appendstr(l,sizeof(l),arg);}debug_log(STATE_LOG,l);}}
static void bt_copy_value(char*d,unsigned cap,const char*s){char b[1024];unsigned i=0;while(s[i]&&s[i]!='\r'&&s[i]!='\n'&&i+1<sizeof(b)){b[i]=s[i];i++;}b[i]=0;copystr(d,cap,b);}
static int bt_name_unknown(const char*s){return !reading_present(s)||streq(s,"UNKNOWN DEVICE")||streq(s,"PHONE");}
static int bt_name_cache_idx(const char*addr){for(int i=0;i<nbtname_cache;i++)if(streq(btname_cache[i].addr,addr))return i;return-1;}
static const char* bt_name_cache_get(const char*addr){int i=bt_name_cache_idx(addr);return i>=0?btname_cache[i].name:0;}
static void bt_name_cache_remember(const char*addr,const char*name){if(!addr||!addr[0]||bt_name_unknown(name))return;int i=bt_name_cache_idx(addr);if(i<0){if(nbtname_cache>=BT_UI_MAX_DEVICES)return;i=nbtname_cache++;copystr(btname_cache[i].addr,sizeof(btname_cache[i].addr),addr);}copystr(btname_cache[i].name,sizeof(btname_cache[i].name),name);}
static void bt_name_cache_load(void){char b[768];long fd=s_open(BT_NAMES_PATH,O_RDONLY_,0);if(fd<0)return;long n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);if(n<=0)return;b[n]=0;char*ln=b;for(long i=0;i<=n;i++){if(b[i]=='\r'||b[i]=='\n'||b[i]==0){char save=b[i];b[i]=0;char*p=ln;while(*p&&*p!='|')p++;if(*p=='|'&&p-ln==17){*p=0;bt_name_cache_remember(ln,p+1);}if(!save)break;ln=b+i+1;}}}
/* AVRCP unknown values and malformed metadata must not masquerade as 0 s. */
static int bt_media_seconds(const char*s){
 if(!s||!*s)return-1;
 u32 value=0;
 for(;*s;s++){
  if(*s<'0'||*s>'9')return-1;
  unsigned digit=(unsigned)(*s-'0');if(value>(4294967u-digit)/10u)return-1;
  value=value*10u+digit;
 }
 return (int)value;
}
static void bt_parse_meta_line(char*ln){
  if(starts(ln,"DEVICE_ADDR=")){bt_copy_value(btui.device_addr,sizeof(btui.device_addr),ln+12);return;}
  if(starts(ln,"DEVICE=")){bt_copy_value(btui.device,sizeof(btui.device),ln+7);return;}
  if(starts(ln,"TITLE=")){bt_copy_value(btui.title,sizeof(btui.title),ln+6);return;}
  if(starts(ln,"ARTIST=")){bt_copy_value(btui.artist,sizeof(btui.artist),ln+7);return;}
  if(starts(ln,"ALBUM=")){bt_copy_value(btui.album,sizeof(btui.album),ln+6);return;}
  if(starts(ln,"PAIRED=")){btui.paired=atoi0(ln+7)?1:0;return;}
  if(starts(ln,"CONNECTED=")){btui.connected=atoi0(ln+10)?1:0;return;}
  if(starts(ln,"PLAYING=")){btui.playing=atoi0(ln+8)?1:0;return;}
  if(starts(ln,"POS=")){btui.pos=bt_media_seconds(ln+4);return;}
  if(starts(ln,"DUR=")){btui.dur=bt_media_seconds(ln+4);if(btui.dur<0)btui.dur=0;return;}
}
static int bt_meta_matches(const char*addr){
 return btui.connected&&addr&&addr[0]&&streq(addr,btui.device_addr)&&!bt_name_unknown(btui.device);
}
static int bt_device_address(char*a){
 if(slen(a)!=17)return 0;
 for(int i=0;i<17;i++){
  if(i%3==2){if(a[i]!=':')return 0;}
  else{a[i]=upc(a[i]);if(!((a[i]>='0'&&a[i]<='9')||(a[i]>='A'&&a[i]<='F')))return 0;}
 }return 1;
}
static int bt_poll_devices(void){
 char b[1280];long fd=s_open(BT_DEVLIST_PATH,O_RDONLY_,0);if(fd<0)return 0;
 long n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);
 if(n<=0||b[n-1]!='\n')return 0;b[n]=0;
 struct BtDeviceUi next[BT_UI_MAX_DEVICES];int count=0,expected=-1,connected=0;char*line=b;
 for(long i=0;i<n;i++)if(b[i]=='\n'){
  b[i]=0;if(i>0&&b[i-1]=='\r')b[i-1]=0;
  if(expected<0){
   if(!starts(line,"COUNT=")||!line[6])return 0;expected=0;
   for(char*p=line+6;*p;p++){if(*p<'0'||*p>'9')return 0;expected=expected*10+*p-'0';if(expected>BT_UI_MAX_DEVICES)return 0;}
  }else{
   if(!starts(line,"DEV=")||count>=expected)return 0;
   char*a=line+4,*state=a;while(*state&&*state!='|')state++;if(!*state)return 0;*state++=0;
   char*name=state;while(*name&&*name!='|')name++;if(!*name)return 0;*name++=0;
   if(!bt_device_address(a)||!(streq(state,"0")||streq(state,"1")))return 0;
   for(int j=0;j<count;j++)if(streq(next[j].addr,a))return 0;
   struct BtDeviceUi*row=&next[count++];copystr(row->addr,sizeof(row->addr),a);row->connected=streq(state,"1");
   if(row->connected&&++connected>1)return 0;copystr(row->name,sizeof(row->name),name);
  }
  line=b+i+1;
 }
 if(expected!=count)return 0;
 /* A name can only be cached against the address in its own DEV record. */
 for(int i=0;i<count;i++){
  struct BtDeviceUi*row=&next[i];
  if(bt_name_unknown(row->name)){const char*cached=bt_name_cache_get(row->addr);copystr(row->name,sizeof(row->name),cached?cached:"UNKNOWN DEVICE");}
  else bt_name_cache_remember(row->addr,row->name);
  if(!btui.connected)row->connected=0;
 }
 for(int i=0;i<count;i++)btdev[i]=next[i];nbtdev=count;return 1;
}
static int bt_connected_idx(void){for(int i=0;i<nbtdev;i++)if(btdev[i].connected)return i;return-1;}
static int bt_saved_count(void){int c=0;for(int i=0;i<nbtdev;i++)if(!btdev[i].connected)c++;return c;}
static int bt_saved_idx(int ord){for(int i=0;i<nbtdev;i++)if(!btdev[i].connected){if(!ord)return i;ord--;}return-1;}
static char bt_hud_peer[18];
static u32 bt_hud_link_at;
static int bt_hud_link,bt_hud_reported;
static void bt_hud_status(const char*before,u32 now,int devices_complete){
 int idx=bt_connected_idx();
 const char*peer=idx>=0?btdev[idx].addr:"";
 if(btui.connected){
  if(!bt_hud_link||(peer[0]&&bt_hud_peer[0]&&!streq(peer,bt_hud_peer))){
   bt_hud_link_at=now;bt_hud_reported=0;hud_connection.kind=HUD_NONE;
  }
  bt_hud_link=1;
  if(peer[0])copystr(bt_hud_peer,sizeof(bt_hud_peer),peer);
  const char*name=idx>=0?(!bt_name_unknown(btdev[idx].name)?btdev[idx].name:bt_meta_matches(btdev[idx].addr)?btui.device:""):btui.device;
  if(!bt_name_unknown(name)){
   if(bt_hud_reported<2){
    copystr(hud_connected_name,sizeof(hud_connected_name),name);
    hud_pulse_at(&hud_connection,HUD_CONNECTION,now,3200);bt_hud_reported=2;
   }
  }else if(!bt_hud_reported&&now-bt_hud_link_at>=1000u){
   hud_connected_name[0]=0;
   hud_pulse_at(&hud_connection,HUD_CONNECTION,now,3200);bt_hud_reported=1;
  }
 }else{
  if(bt_hud_link){hud_connection.kind=HUD_NONE;hud_bt_notice(ui_tr("Disconnected"));}
  bt_hud_link=0;bt_hud_peer[0]=0;
 }
 if(bt_forget_addr[0]){
  if(now-bt_forget_at>=5000u)bt_forget_addr[0]=0;
  else if(devices_complete&&streq(btui.status,"BT DEVICE FORGOTTEN")){
   int i=0;while(i<nbtdev&&!streq(btdev[i].addr,bt_forget_addr))i++;
   if(i==nbtdev){bt_forget_addr[0]=0;hud_bt_notice(ui_tr("Device forgotten"));}
  }
 }
 if(!streq(before,btui.status)){
  if((starts(before,"BT PAIRING")&&streq(hud_bt_text,ui_tr("Pairing phone")))||
     (streq(before,"BT RECONNECTING")&&streq(hud_bt_text,ui_tr("Reconnecting"))))hud_bt_feedback.kind=HUD_NONE;
  if(starts(btui.status,"BT PAIRING"))hud_bt_notice(ui_tr("Pairing phone"));
  else if(streq(btui.status,"BT RECONNECTING"))hud_bt_notice(ui_tr("Reconnecting"));
 }
}
static void bt_forget_device(const char*addr){
 copystr(bt_forget_addr,sizeof(bt_forget_addr),addr);bt_forget_at=hud_now_ms();
 bt_command_arg("UNPAIR",addr);
}
static void bt_poll_status(void){
  char before[sizeof(btui.status)];copystr(before,sizeof(before),btui.status);
  int was_playing=btui.playing,was_connected=btui.connected;
  char old_title[sizeof(btui.title)],old_artist[sizeof(btui.artist)],old_album[sizeof(btui.album)];
  copystr(old_title,sizeof(old_title),btui.title);copystr(old_artist,sizeof(old_artist),btui.artist);copystr(old_album,sizeof(old_album),btui.album);
  char b[1024];long fd=s_open(BT_STATUS_PATH,O_RDONLY_,0);
  if(fd>=0){
   long n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);
   if(n<=0)return;
   b[n]=0;bt_copy_value(btui.status,sizeof(btui.status),b);
  }else copystr(btui.status,sizeof(btui.status),"BT DAEMON OFFLINE");
  if(fd<0||!reading_present(btui.status)||streq(btui.status,"BT DISCONNECTED")){
   btui.connected=0;btui.playing=0;bt_playback_request=-1;
   btui.device_addr[0]=btui.device[0]=btui.title[0]=btui.artist[0]=btui.album[0]=0;btui.pos=btui.dur=0;
   for(int i=0;i<nbtdev;i++)btdev[i].connected=0;
   bt_hud_link=0;bt_hud_peer[0]=0;hud_connection.kind=hud_bt_feedback.kind=HUD_NONE;
   if(fd>=0&&was_connected&&streq(btui.status,"BT DISCONNECTED"))hud_bt_notice(ui_tr("Disconnected"));
   return;
  }
  if(starts(btui.status,"BT STREAMING")){btui.connected=1;btui.paired=1;btui.playing=1;}
  else if(starts(btui.status,"BT CONNECTED")||starts(btui.status,"BT OPEN")||starts(btui.status,"BT BUFFERING")||starts(btui.status,"BT PAUSED")){btui.connected=1;btui.paired=1;if(starts(btui.status,"BT PAUSED"))btui.playing=0;}
  else{btui.connected=0;btui.playing=0;btui.title[0]=0;btui.artist[0]=0;btui.album[0]=0;btui.pos=btui.dur=0;}
  fd=s_open(BT_META_PATH,O_RDONLY_,0);
  if(fd>=0){
   long n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);
   if(n>0){
    b[n]=0;btui.device_addr[0]=0;char*line=b;
    for(long i=0;i<=n;i++)if(b[i]=='\r'||b[i]=='\n'||b[i]==0){
     char end=b[i];b[i]=0;
     if(line[0])bt_parse_meta_line(line);
     if(!end)break;
     line=b+i+1;
    }
   }
  }
  int devices_complete=bt_poll_devices();
  if(!btui.connected)btui.device_addr[0]=btui.device[0]=0;
  u32 now=hud_now_ms();
  /* Hold the requested icon through stale reports, then trust the phone again. */
  if(!btui.connected||now-bt_playback_requested_at>=BT_PLAYBACK_REPLY_MS||
     (btui.playing!=was_playing&&btui.playing==bt_playback_request))bt_playback_request=-1;
  bt_hud_status(before,now,devices_complete);
  /* Track identity is independent of connection and playback position. Never
   * announce blank placeholders or a phone playing while another source is on. */
  if(btui.connected&&bt_source_selected()&&reading_present(btui.title)&&
     (!streq(old_title,btui.title)||
      (reading_present(btui.artist)&&!streq(old_artist,btui.artist))||
      (reading_present(btui.album)&&!streq(old_album,btui.album))))
   hud_pulse_at(&hud_source,HUD_TRACK,now,4200);
}
static const char* bt_action_label(void){if(btui.connected)return"DISCONNECT PHONE";if(btui.paired)return"RECONNECT PHONE";return"PAIR PHONE";}
static void bt_action_connection(void){if(btui.connected)bt_command("DISCONNECT");else if(btui.paired)bt_command("CONNECT");else bt_command("PAIR");}

/* CPM5 global map header, index and tile-local road metadata. */
struct CpmHeader {char magic[4];u16 ver;u8 minlvl,maxlvl;u32 flags,count,index_off,data_off; s32 minlat,minlon,maxlat,maxlon;};
struct CpmIndex {u8 lvl,flags;u16 reserved;s32 tx,ty;u32 off,len;u16 features,pad;};
static struct CpmHeader mh;static int mapfd=-1,mapok=0,mapfmt=0;
#define TILE_CACHE 96
#define TILE_MAX 32768
struct Tile {int valid;u8 lvl,page;s32 tx,ty;u32 len,age;u8 data[TILE_MAX];};
static struct Tile tc[TILE_CACHE];static u32 agec=1;
static u16 rd16(const u8*p){return(u16)(p[0]|((u16)p[1]<<8));}
static u32 rd32u(const u8*p){return(u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
#include "atlas_catalog.h"
static int load_map(void){mapok=0;mapfmt=0;if(mapfd>=0){s_close(mapfd);mapfd=-1;}load_atlas();mapfd=(int)s_open(atlas_ready?atlas_map_path:MAP_PATH,O_RDONLY_,0);if(mapfd<0)return 0;if(s_read(mapfd,&mh,sizeof(mh))!=(long)sizeof(mh)){s_close(mapfd);mapfd=-1;return 0;}if(mh.magic[0]=='C'&&mh.magic[1]=='P'&&mh.magic[2]=='M'&&mh.magic[3]=='5'&&mh.ver==5)mapfmt=5;else mapfmt=0;if(!mapfmt||mh.minlvl>mh.maxlvl||mh.maxlvl>6){s_close(mapfd);mapfd=-1;return 0;}mapok=1;manual_zoom=manual_zoom<(int)mh.minlvl?(int)mh.minlvl:manual_zoom;manual_zoom=manual_zoom>(int)mh.maxlvl?(int)mh.maxlvl:manual_zoom;return 1;}
static int keycmp(u8 l,s32 x,s32 y,const struct CpmIndex*e){if(l<e->lvl)return-1;if(l>e->lvl)return 1;if(x<e->tx)return-1;if(x>e->tx)return 1;if(y<e->ty)return-1;if(y>e->ty)return 1;return 0;}
static int find_index(u8 l,s32 x,s32 y,u8 page,struct CpmIndex*out){u32 lo=0,hi=mh.count;u32 size=24;while(lo<hi){u32 mid=lo+(hi-lo)/2;u8 b[24];s_lseek(mapfd,(long)(mh.index_off+mid*size),SEEK_SET_);if(s_read(mapfd,b,size)!=(long)size)return 0;struct CpmIndex e;e.lvl=b[0];e.flags=b[1];e.tx=(s32)rd32u(b+4);e.ty=(s32)rd32u(b+8);unsigned k=12;e.off=rd32u(b+k);e.len=rd32u(b+k+4);e.features=rd16(b+k+8);e.pad=rd16(b+k+10);int c=keycmp(l,x,y,&e);if(!c)c=page<e.flags?-1:(page>e.flags?1:0);if(c==0){*out=e;return 1;}if(c<0)hi=mid;else lo=mid+1;}return 0;}
static struct Tile* get_tile_page_raw(u8 l,s32 x,s32 y,u8 page){int i,best=0;u32 ba=0xffffffff;for(i=0;i<TILE_CACHE;i++){if(tc[i].valid&&tc[i].lvl==l&&tc[i].page==page&&tc[i].tx==x&&tc[i].ty==y){tc[i].age=agec++;return tc[i].valid==2?0:&tc[i];}if(!tc[i].valid){best=i;ba=0;break;}if(tc[i].age<ba){ba=tc[i].age;best=i;}}struct CpmIndex e;if(!find_index(l,x,y,page,&e)){tc[best].valid=2;tc[best].lvl=l;tc[best].page=page;tc[best].tx=x;tc[best].ty=y;tc[best].len=0;tc[best].age=agec++;return 0;}if(e.len>TILE_MAX)return 0;s_lseek(mapfd,(long)(mh.data_off+e.off),SEEK_SET_);if(s_read(mapfd,tc[best].data,e.len)!=(long)e.len)return 0;tc[best].valid=1;tc[best].lvl=l;tc[best].page=page;tc[best].tx=x;tc[best].ty=y;tc[best].len=e.len;tc[best].age=agec++;return &tc[best];}
static int atlas_map_fd[4]={-1,-1,-1,-1},atlas_map_id[4]={-1,-1,-1,-1};static struct CpmHeader atlas_map_header[4];static u32 atlas_map_slot=0;
static struct Tile* get_tile_page(u8 l,s32 x,s32 y,u8 page){
 if(!atlas_ready)return get_tile_page_raw(l,x,y,page);for(int i=0;i<TILE_CACHE;i++)if(tc[i].valid&&tc[i].lvl==l&&tc[i].page==page&&tc[i].tx==x&&tc[i].ty==y){tc[i].age=agec++;return tc[i].valid==2?0:&tc[i];}int source=l>=8&&l<=11?l-8:l;if(source>6)return 0;int span=16384>>source;int owner=atlas_owner(y*span+span/2,x*span+span/2);if(owner<0)return 0;int slot=-1;for(int i=0;i<4;i++)if(atlas_map_id[i]==owner){slot=i;break;}
 if(slot<0){slot=(int)(atlas_map_slot++&3u);if(atlas_map_fd[slot]>=0)s_close(atlas_map_fd[slot]);atlas_map_id[slot]=-1;char path[160];atlas_path(path,sizeof(path),(u32)owner,"map.cpm");int fd=(int)s_open(path,O_RDONLY_,0);if(fd<0)return 0;struct CpmHeader h;if(s_read(fd,&h,sizeof(h))!=(long)sizeof(h)||h.magic[0]!='C'||h.magic[1]!='P'||h.magic[2]!='M'||h.magic[3]!='5'||h.ver!=5||h.minlvl!=0||h.maxlvl!=6){s_close(fd);return 0;}atlas_map_fd[slot]=fd;atlas_map_header[slot]=h;atlas_map_id[slot]=owner;}
 int savefd=mapfd;struct CpmHeader save=mh;mapfd=atlas_map_fd[slot];mh=atlas_map_header[slot];struct Tile*t=get_tile_page_raw(l,x,y,page);mapfd=savefd;mh=save;return t;
}
static struct Tile* get_tile(u8 l,s32 x,s32 y){return get_tile_page(l,x,y,0);}
static int span_for_level(int z){if(z>=8&&z<=11)z-=8;static const int sp[7]={16384,8192,4096,2048,1024,512,256};if(z<0)z=0;if(z>6)z=6;return sp[z];}
static int clamp_map_zoom(int z){int lo=mapok?(int)mh.minlvl:0,hi=mapok?(int)mh.maxlvl:6;if(lo<0)lo=0;if(hi>6)hi=6;if(z<lo)z=lo;if(z>hi)z=hi;return z;}
static int tile_div(int a,int z){int sh=14-z;if(a>=0)return a>>sh;return -(((-a)+(1<<sh)-1)>>sh);}
static int coslat_q15(s32 lat){static s32 previous;static int cached,valid;if(valid&&lat==previous)return cached;previous=lat;valid=1;int d=lat<0?-lat:lat;d/=100;if(d>=90000){cached=0;return 0;}static const u16 c[91]={32767,32762,32747,32722,32687,32642,32587,32523,32448,32364,32269,32165,32051,31927,31794,31650,31498,31335,31163,30982,30791,30591,30381,30162,29934,29697,29451,29196,28932,28659,28377,28087,27788,27481,27165,26841,26509,26169,25821,25465,25101,24730,24351,23964,23571,23170,22762,22347,21925,21497,21062,20621,20173,19720,19260,18794,18323,17846,17364,16876,16384,15886,15383,14876,14364,13848,13328,12803,12275,11743,11207,10668,10126,9580,9032,8481,7927,7371,6813,6252,5690,5126,4560,3993,3425,2856,2286,1715,1144,572,0};int i=d/1000,f=d%1000;cached=c[i]+((int)c[i+1]-c[i])*f/1000;return cached;}
static s32 lon_delta(s32 d){if(d>18000000)d-=36000000;if(d< -18000000)d+=36000000;return d;}
static s32 lon_scale(s32 d,int cq){return(s32)(((long long)lon_delta(d)*cq)>>15);}
static u32 square_distance(s32 x,s32 y){if(x>32767||x< -32767||y>32767||y< -32767)return 0xffffffffu;return(u32)(x*x+y*y);}

static const s16 SIN36[36]={0,5690,11207,16384,21063,25080,28378,30791,32270,32767,32270,30791,28378,25080,21063,16384,11207,5690,0,-5690,-11207,-16384,-21063,-25080,-28378,-30791,-32270,-32767,-32270,-30791,-28378,-25080,-21063,-16384,-11207,-5690};
static const s16 COS36[36]={32767,32270,30791,28378,25080,21063,16384,11207,5690,0,-5690,-11207,-16384,-21063,-25080,-28378,-30791,-32270,-32767,-32270,-30791,-28378,-25080,-21063,-16384,-11207,-5690,0,5690,11207,16384,21063,25080,28378,30791,32270};
static u32 udiv32(u32 n,u32 d){u32 q=0,r=0;if(!d)return 0;for(int i=31;i>=0;i--){r=(r<<1)|((n>>i)&1u);if(r>=d){r-=d;q|=(1u<<i);}}return q;}

/* Smooth locked-camera position. GPS commonly updates at 1 Hz; the UI draws at
 * 5 Hz. Easing both the camera and marker avoids one-second map jumps. */
static s32 track_lat=0,track_lon=0;static int track_valid=0,track_course=-1;
/* UI1: map/car orientation has its own latch. It updates only while the car is
 * moving fast enough for GPS course to be trustworthy, then remains frozen
 * through crawling/stationary periods. This avoids the old <2 mph snap to
 * north-up and is intentionally independent of whether a route is active. */
static int display_course=-1,display_course_valid=0;
static void seed_boot_map_view(void){if(lastfix_valid){track_lat=lastfix_lat;track_lon=lastfix_lon;track_course=lastfix_course;track_valid=1;display_course=lastfix_course;display_course_valid=1;return;}if(mapok){track_lat=mh.minlat+(mh.maxlat-mh.minlat)/2;track_lon=mh.minlon+(mh.maxlon-mh.minlon)/2;track_course=0;track_valid=1;}}
static int iabs32(s32 v){return v<0?-v:v;}
static void update_smooth_track(void){if(!gps.fix)return;if(!track_valid||iabs32(gps.lat-track_lat)>5000||iabs32(gps.lon-track_lon)>5000){track_lat=gps.lat;track_lon=gps.lon;track_course=gps.course;track_valid=1;return;}s32 dl=gps.lat-track_lat,do_=gps.lon-track_lon;if(dl){s32 q=dl/4;if(!q)q=dl>0?1:-1;track_lat+=q;}if(do_){s32 q=do_/4;if(!q)q=do_>0?1:-1;track_lon+=q;}if(track_course<0)track_course=gps.course;else{int d=gps.course-track_course;if(d>180)d-=360;if(d<-180)d+=360;int q=d/4;if(!q&&d)q=d>0?1:-1;track_course+=q;if(track_course<0)track_course+=360;if(track_course>=360)track_course-=360;}}
static int shown_course(void){return track_course>=0?track_course:gps.course;}
static void update_display_course(void){if(!gps.fix)return;int c=shown_course();if(c<0||c>=360)return;if(!display_course_valid){/* A persisted heading is preferred, but on a first-ever boot this gives us a usable orientation until movement confirms it. */display_course=c;display_course_valid=1;return;}/* track_course is already smoothed; while moving, mirror it exactly so UI1 adds no extra steering lag. Below the threshold, deliberately do nothing and freeze. */if(gps.mph10>=DISPLAY_COURSE_UPDATE_MPH10)display_course=c;}
static int map_course(void){if(display_course_valid)return display_course;return shown_course();}

#define MAX_EXPLORE_ZOOM 9
#define FREECAM_ORIENT_ANIM_MS 500u
#define FC_ANIM_NONE 0
#define FC_ANIM_TO_NORTH 1
#define FC_ANIM_TO_CAR 2
static int freecam=0,freecam_zoom=5,auto_zoom_level=-1,zoom_override=-1;static s32 cam_lat=0,cam_lon=0;
/* Camera interpolation has its own elapsed-time clock, independent of map
 * and HUD frame counts. MAP toggles are guarded; confirming a selected point
 * may reverse an unfinished entry from its current pose. */
static int freecam_course=0,freecam_course_valid=0,freecam_anim_mode=FC_ANIM_NONE,freecam_anim_from=0;
static u32 freecam_anim_at;
static int freecam_anim_tilt_from=256;
/* 256=driving perspective, 0=top-down Explore; rotation and tilt share time. */
static int map_tilt_q8=256;

/* Driving autozoom now uses only Z6/Z5/Z4.  Hysteresis keeps the existing
 * 18/22 mph low-speed headroom and adds a 38/42 mph Z5/Z4 boundary. */
static int speed_auto_zoom(void){int s=gps.mph10;if(auto_zoom_level<4||auto_zoom_level>6){if(s>=400)auto_zoom_level=4;else if(s>=200)auto_zoom_level=5;else auto_zoom_level=6;return auto_zoom_level;}if(auto_zoom_level==6){if(s>=220)auto_zoom_level=5;}else if(auto_zoom_level==5){if(s<=180)auto_zoom_level=6;else if(s>=420)auto_zoom_level=4;}else{if(s<=380)auto_zoom_level=5;}return auto_zoom_level;}
static int active_zoom(void){int z;if(freecam){int lo=mapok?(int)mh.minlvl:0;z=freecam_zoom;if(z<lo)z=lo;if(z>MAX_EXPLORE_ZOOM)z=MAX_EXPLORE_ZOOM;return z;}if(pref.autozoom)z=zoom_override>=0?zoom_override:speed_auto_zoom();else z=manual_zoom;return clamp_map_zoom(z);}
static int span_for_view_zoom(int z){if(z<=6)return span_for_level(z);if(z>MAX_EXPLORE_ZOOM)z=MAX_EXPLORE_ZOOM;return 256>>(z-6);}
static s32 car_lat(void){return track_valid?track_lat:gps.lat;}static s32 car_lon(void){return track_valid?track_lon:gps.lon;}
static s32 view_lat(void){return freecam?cam_lat:car_lat();}static s32 view_lon(void){return freecam?cam_lon:car_lon();}
static int view_course_valid(void){if(freecam_anim_mode==FC_ANIM_TO_CAR)return freecam_course_valid;return freecam?freecam_course_valid:display_course_valid;}
static int view_course(void){if(freecam_anim_mode==FC_ANIM_TO_CAR&&freecam_course_valid)return freecam_course;return freecam&&freecam_course_valid?freecam_course:map_course();}
static int view_rotates(void){return view_course_valid();}
/* Continuous camera angles avoid the former ten-degree presentation steps.
 * Interpolated trig is normalized once per angle, then cached per vertex pass. */
static void view_trig(int*ss,int*c){
 static int previous,initialized,sine,cosine;
 int input=view_course();
 /* The camera angle is constant for a whole geometry pass. Check its raw
  * value before normalizing, avoiding a software modulo at every vertex. */
 if(!initialized||input!=previous){
  int angle=input;while(angle<0)angle+=360;angle%=360;
  int k=angle/10,f=angle%10,n=(k+1)%36;
  sine=(SIN36[k]*(10-f)+SIN36[n]*f)/10;
  cosine=(COS36[k]*(10-f)+COS36[n]*f)/10;
  int norm=(sine*sine+cosine*cosine)>>15,scale=(98304-norm)/2;
  sine=(sine*scale+16384)>>15;cosine=(cosine*scale+16384)>>15;
  if(sine>32767)sine=32767;if(cosine>32767)cosine=32767;
  if(sine< -32767)sine=-32767;if(cosine< -32767)cosine=-32767;
  previous=input;initialized=1;
 }
 *ss=sine;*c=cosine;
}
#define MAP_TOP 0
#define MAP_BOTTOM H
#define HUD_BOTTOM_Y 220
#define MAP_ANCHOR_X 240
#define FREECAM_ANCHOR_Y 160
#define DRIVE_ANCHOR_Y 211
/* Keep the car clear of the floating lower cards. The inverse coverage
 * calculation uses the same animated anchor. */
static int view_anchor_y(void){
  if(freecam_anim_mode!=FC_ANIM_NONE){
    int q=map_tilt_q8;
    if(q<0)q=0;if(q>256)q=256;
    return FREECAM_ANCHOR_Y+((DRIVE_ANCHOR_Y-FREECAM_ANCHOR_Y)*q+128)/256;
  }
  return freecam?FREECAM_ANCHOR_Y:DRIVE_ANCHOR_Y;
}
/* A fixed-point flat-ground perspective: screen scale falls with forward
 * distance, but lines stay straight, so the existing roads and route overlays
 * share one transform.  480/(480+2*d) puts the horizon ABOVE the map viewport
 * (y=-44 with driving anchor y=224), so every visible pixel has a finite
 * ground coordinate. Behind the car the existing top-down scale is kept.
 * Rebuild the tiny (2 KiB) table only on a tilt animation frame, never per
 * road vertex; ordinary map drawing needs just one indexed lookup. */
/* Keep the full projected footprint within the 96-entry tile cache even
 * after moving the camera 20 px lower.  A slightly higher horizon offset
 * avoids excessive far-field tile requests without losing the view ahead. */
/* A gentler far-field compression keeps the tile footprint close to the old
 * inset viewport after revealing the map behind the former header. */
#define PERSPECTIVE_BASE 620u
#define PERSPECTIVE_LUT_MAX 4096
#define PERSPECTIVE_LUT_STEP 4
#define PERSPECTIVE_LUT_SIZE (PERSPECTIVE_LUT_MAX/PERSPECTIVE_LUT_STEP+1)
static u16 perspective_scale_q12[PERSPECTIVE_LUT_SIZE];
static int perspective_lut_tilt=-1;
static void perspective_scale_update(void){
  if(perspective_lut_tilt==map_tilt_q8)return;
  if(map_tilt_q8<=0){perspective_lut_tilt=0;return;}
  const u32 base=PERSPECTIVE_BASE*256u, numerator=base*4096u;
  for(int i=0;i<PERSPECTIVE_LUT_SIZE;i++){
    u32 denom=base+2u*(u32)map_tilt_q8*(u32)(i*PERSPECTIVE_LUT_STEP);
    perspective_scale_q12[i]=(u16)udiv32(numerator,denom);
  }
  perspective_lut_tilt=map_tilt_q8;
}
static int perspective_scale_for_forward(int d){
  if(d<=0||map_tilt_q8<=0)return 4096;
  perspective_scale_update();
  if(d<=PERSPECTIVE_LUT_MAX)return perspective_scale_q12[d/PERSPECTIVE_LUT_STEP];
  return (int)udiv32(PERSPECTIVE_BASE*256u*4096u,PERSPECTIVE_BASE*256u+2u*(u32)map_tilt_q8*(u32)d);
}
/* Exact floor(local*span/4095), for 16-bit local coordinates and spans
 * up to 16384. The shift estimate is at most one low; correct its remainder. */
static int map_tile_offset(u32 local,u32 span){
 u32 value=local*span,q=(value+(value>>12))>>12;
 return (int)(q+(value-((q<<12)-q)>=4095u));
}
static void world_to_screen(s32 lat,s32 lon,int z,int*ox,int*oy){s32 vl=view_lat(),vo=view_lon();s32 dx=lon_delta(lon-vo),dy=lat-vl;int cq=coslat_q15(vl);s32 wx=lon_scale(dx,cq);int sh=14-z;long long xx=((long long)wx*256)>>sh,yy=-(((long long)dy*256)>>sh);s32 x=xx>16384?16384:xx< -16384?-16384:(s32)xx,y=yy>16384?16384:yy< -16384?-16384:(s32)yy;if(view_rotates()){int c,ss;view_trig(&ss,&c);int rx=(x*c+y*ss)>>15;int ry=(-x*ss+y*c)>>15;x=rx;y=ry;}
  if(y<0&&map_tilt_q8>0){int q=perspective_scale_for_forward(-y);x=(x*q)>>12;y=(y*q)>>12;}
  *ox=MAP_ANCHOR_X+x;*oy=view_anchor_y()+y;
}
/* PERSPECTIVE2: derive the exact visible world-space bounds from the inverse
 * camera rather than the old fixed +/-2-tile neighbourhood.  A projected map
 * viewport is a trapezoid; sample the two sides at its top, camera-height
 * crease and bottom. The anchor is animated in lockstep with the tilt.
 * The eight-pixel vertical and 24-pixel horizontal bleed
 * accommodate the map line widths and integer/lookup rounding.  Inverse
 * projection uses the same interpolated rotation as world_to_screen().
 * No floating point, dynamic allocations or per-road inverse calculations. */
static void screen_to_world_coverage(int sx,int sy,int z,s32*lat,s32*lon){
  int x=sx-MAP_ANCHOR_X,y=sy-view_anchor_y();
  if(y<0&&map_tilt_q8>0){
    u32 base=PERSPECTIVE_BASE*256u;
    u32 ds=(u32)(-y),sub=2u*(u32)map_tilt_q8*ds;
    /* PERSPECTIVE_BASE keeps the full viewport below the horizon. */
    u32 den=base>sub?base-sub:1u;
    x=(int)udiv32((u32)(x<0?-x:x)*base,den)*(x<0?-1:1);
    y=-(int)udiv32(ds*base,den);
  }
  if(view_rotates()){
    int c,ss;view_trig(&ss,&c);
    int rx=(x*c-y*ss)>>15,ry=(x*ss+y*c)>>15;x=rx;y=ry;
  }
  int sh=14-z,unit=1<<sh;
  s32 east=(s32)((x*unit)/256),north=-(s32)((y*unit)/256);
  int cq=coslat_q15(view_lat());
  u32 a=(u32)(east<0?-east:east);
  s32 dlon=(s32)udiv32((a<<15)+(u32)(cq/2),(u32)cq);
  if(east<0)dlon=-dlon;
  *lat=view_lat()+north;*lon=view_lon()+dlon;
}
static void map_visible_tile_bounds(int z,int sourcez,int*tx0,int*tx1,int*ty0,int*ty1){
  s32 minlat=0x7fffffff,maxlat=(-2147483647-1),minlon=0x7fffffff,maxlon=(-2147483647-1);
  const int xs[2]={-24,W+24},ys[3]={MAP_TOP-8,view_anchor_y(),MAP_BOTTOM+8};
  for(int i=0;i<3;i++)for(int j=0;j<2;j++){
    s32 lat,lon;screen_to_world_coverage(xs[j],ys[i],z,&lat,&lon);
    if(lat<minlat)minlat=lat;if(lat>maxlat)maxlat=lat;
    if(lon<minlon)minlon=lon;if(lon>maxlon)maxlon=lon;
  }
  /* Tile-space padding absorbs truncation and boundary-clipped line widths. */
  int unit=1<<(14-z),pad=unit/16+4;
  *tx0=tile_div(minlon-pad,sourcez);*tx1=tile_div(maxlon+pad,sourcez);
  *ty0=tile_div(minlat-pad,sourcez);*ty1=tile_div(maxlat+pad,sourcez);
}
/* Reject tiles wholly outside the physical map pack BEFORE expensive index and
 * SD-card reads.  Slightly expand the envelope to preserve edge-clipped roads. */
static int map_tile_in_pack(int z,int tx,int ty){
  if(atlas_ready){int span=span_for_level(z);return atlas_owner(ty*span+span/2,tx*span+span/2)>=0;}
  int minx=tile_div(mh.minlon,z)-1,maxx=tile_div(mh.maxlon,z)+1;
  int miny=tile_div(mh.minlat,z)-1,maxy=tile_div(mh.maxlat,z)+1;
  return tx>=minx&&tx<=maxx&&ty>=miny&&ty<=maxy;
}
/* Boot opens one child before GPS is ready. Its header describes that physical
 * file, not the entire atlas; saved/live fixes and Explore may be in any core.
 * Keep mh intact because tile index reads still need the child file's header. */
static int map_view_covered(s32 lat,s32 lon){
  if(atlas_ready)return atlas_owner(lat,lon)>=0;
  return lat>=mh.minlat-2&&lat<=mh.maxlat+2&&lon>=mh.minlon-2&&lon<=mh.maxlon+2;
}
static void road_style(int cls,int zoom,u16*col,int*wid){
 /* Zoom increases towards street detail. Service roads stay narrow even
  * when residential roads widen. No extra outlines or geometry passes. */
 static const u8 widths[4][6]={{1,1,2,2,3,4},{1,2,2,3,3,4},
  {1,2,3,3,4,5},{2,2,3,4,4,5}};
 int slot=cls>=1&&cls<=5?6-cls:0;
 int detail=zoom<=3?0:zoom==4?1:zoom==5?2:3;
 *col=road_colours[slot];*wid=cls>=7?1:widths[detail][slot];
}
static void geom_style(int layer,int style,int zoom,u16*col,int*wid){if(layer==0){road_style(style,zoom,col,wid);return;}if(layer==1){*col=C_WATER;*wid=style==1?2:1;return;}if(layer==2){*col=C_GREEN;*wid=1;return;}*col=C_BLUE;*wid=1;}

#include "map_raster.h"
/* Keep road/route vertices below pixel precision until rasterization. The
 * driving camera and route engine are unchanged; only drawing gains precision. */
static void map_project_q8(s32 lat,s32 lon,int z,int*ox,int*oy){
 if(!MAP_ANTIALIAS){world_to_screen(lat,lon,z,ox,oy);*ox*=256;*oy*=256;return;}
 s32 dx=lon_delta(lon-view_lon()),dy=lat-view_lat();int sh=14-z,cq=coslat_q15(view_lat());
 long long x=((long long)dx*cq)>>(sh-1),y=-(long long)dy*(65536>>sh);
 if(view_rotates()){int c,ss;view_trig(&ss,&c);long long rx=(x*c+y*ss)>>15,ry=(-x*ss+y*c)>>15;x=rx;y=ry;}
 if(y<0&&map_tilt_q8>0){int q;long long forward=-y;
  perspective_scale_update();
  if(forward<(long long)PERSPECTIVE_LUT_MAX*256){int i=(int)(forward>>10),f=(int)(forward&1023);q=(perspective_scale_q12[i]*(1024-f)+perspective_scale_q12[i+1]*f+512)>>10;}
  else q=perspective_scale_for_forward((int)(forward>>8));
  x=(x*q)>>12;y=(y*q)>>12;
 }
 if(x>4194304)x=4194304;if(x< -4194304)x=-4194304;if(y>4194304)y=4194304;if(y< -4194304)y=-4194304;*ox=MAP_ANCHOR_X*256+(int)x;*oy=view_anchor_y()*256+(int)y;
}

/* The top-centred journey/zoom card reserves only its actual footprint. */
static int nav_card_x=0,nav_card_y=0,nav_card_w=0,nav_card_h=0;
/* Existing CPM packs store boundary polylines. Only fill a complete closed
 * water/green ring; clipped/open fragments and river centre lines stay lines.
 * This deliberately needs no map rebuild and does not guess missing topology.
 * Rasterize at pixel centres with bounded stack storage and per-edge division,
 * then use integer additions for scanlines inside the 480x202 map viewport. */
#ifndef MAP_AREA_FILLS
#define MAP_AREA_FILLS 1
#endif
struct MapAreaEdge {int first,last,x,step,smooth;};
static void draw_closed_area(struct Tile*t,int span,int layer,int style,int np,const u8*p,int viewz){
  /* Only complete clipped polygons carry the area bit. */
  if(!(style&128))return;
  style&=127;
  if(!MAP_AREA_FILLS||np<4||np>255||(layer!=2&&!(layer==1&&(style==4||style==5))))return;
  if(rd16(p)!=rd16(p+(np-1)*4)||rd16(p+2)!=rd16(p+(np-1)*4+2))return;
  struct MapAreaEdge edges[254];int ne=0,lo=MAP_BOTTOM,hi=MAP_TOP;
  int x0=0,y0=0;
  for(int i=0;i<np;i++){
    s32 lon=(s32)t->tx*span+map_tile_offset(rd16(p+i*4),(u32)span);
    s32 lat=(s32)t->ty*span+map_tile_offset(rd16(p+i*4+2),(u32)span);
    int x1,y1;world_to_screen(lat,lon,viewz,&x1,&y1);
    if(i&&y0!=y1){
      int ax=x0,ay=y0,bx=x1,by=y1;
      if(ay>by){int tmp=ax;ax=bx;bx=tmp;tmp=ay;ay=by;by=tmp;}
      int first=ay<MAP_TOP?MAP_TOP:ay,last=by>MAP_BOTTOM?MAP_BOTTOM:by;
      if(first<last){
        int dy=by-ay,dx=bx-ax,numerator=dx*(2*first+1-2*ay),denominator=2*dy;
        int whole=numerator/denominator,remain=numerator%denominator;
        edges[ne].first=first;edges[ne].last=last;
        edges[ne].x=(ax+whole)*4096+(remain*4096)/denominator;
        edges[ne].step=(dx*4096)/dy;
        /* Tile clipping adds straight closure edges. Keep those hard so two
         * independently filled neighbours cannot leave a blended seam. */
        int ax0=rd16(p+(i-1)*4),ay0=rd16(p+(i-1)*4+2),ax1=rd16(p+i*4),ay1=rd16(p+i*4+2);
        edges[ne].smooth=!((ax0==ax1&&(ax0==0||ax0==4095))||(ay0==ay1&&(ay0==0||ay0==4095)));ne++;
        if(first<lo)lo=first;if(last>hi)hi=last;
      }
    }
    x0=x1;y0=y1;
  }
  u16 col=layer==1?C_WATER:C_GREEN;
  for(int y=lo;y<hi;y++){
    if(!(y&7))render_checkpoint();
    int cuts[254],steps[254],n=0;
    for(int i=0;i<ne;i++)if(y>=edges[i].first&&y<edges[i].last){
      int x=edges[i].x,step=edges[i].step,k=n;
      if(MAP_ANTIALIAS&&!edges[i].smooth){x=((x+2047)>>12)*4096;step=0;}
      while(k&&cuts[k-1]>x){cuts[k]=cuts[k-1];steps[k]=steps[k-1];k--;}
      cuts[k]=x;steps[k]=step;n++;
      edges[i].x+=edges[i].step;
    }
    for(int i=0;i+1<n;i+=2){
      if(MAP_ANTIALIAS)map_area_span(y,cuts[i],cuts[i+1],steps[i],steps[i+1],col);
      else{int left=(cuts[i]+2047)>>12,right=(cuts[i+1]+2047)>>12;
        if(left<0)left=0;if(right>W)right=W;if(left<right)fill(left,y,right-left,1,col);}

    }
  }
}
static void draw_path(struct Tile*t,int span,int layer,int style,int np,const u8**pp,const u8*end,int viewz){
 const u8*p=*pp;if(np<2||np>255||p+np*4>end){*pp=end;return;}u16 col;int wid;geom_style(layer,style,viewz,&col,&wid);
 map_mask_select(layer==0?((style>=1&&style<=5)?6-style:0):(layer==1?0:(layer==2?1:2)));
 int x[255],y[255];for(int i=0;i<np;i++){int lx=rd16(p),ly=rd16(p+2);p+=4;
  s32 lon=(s32)t->tx*span+map_tile_offset(lx,(u32)span),lat=(s32)t->ty*span+map_tile_offset(ly,(u32)span);
  map_project_q8(lat,lon,viewz,&x[i],&y[i]);
 }map_stroke_polyline(x,y,np,col,wid);*pp=p;
}
enum {MAP_DRAW_CONTEXT=0,MAP_DRAW_AREAS=1,MAP_DRAW_ROADS=2,MAP_DRAW_OCEAN=3};
static void draw_tile_geometry(struct Tile*t,int viewz,int pass){
  render_checkpoint();
  if(!t)return;const u8*p=t->data,*end=t->data+t->len;int span=span_for_level((int)t->lvl);

  int hs=8;if(t->len<(u32)hs)return;u16 ng=rd16(p);p+=hs;
  for(u16 f=0;f<ng&&p+4<=end;f++){
    if(!(f&7))render_checkpoint();
    int layer=*p++,style=*p++,np=*p++;p++;if(p+np*4>end)return;
    int ocean=layer==1&&style==(128|5);
    if(pass==MAP_DRAW_AREAS||pass==MAP_DRAW_OCEAN){
      if((pass==MAP_DRAW_OCEAN)==ocean)draw_closed_area(t,span,layer,style,np,p,viewz);p+=np*4;
    }
    else{
      int area=(style&128);
      int wanted=!area&&((pass==MAP_DRAW_ROADS&&layer==0)||(pass==MAP_DRAW_CONTEXT&&layer!=0));
      if(wanted)draw_path(t,span,layer,style,np,&p,end,viewz);else p+=np*4;
    }
  }
}

/* Freecam and car-locked road labels have separate layouts. Car-locked view first
 * displays high-priority names allowed by sparse layout rules, then fills
 * unused slots with visible names previously excluded by distant filtering.
 * Both passes rank by source priority before screen-space vicinity.  Candidate
 * pool is bounded; no heap, new map format or extra tile reads. */
#define DRIVE_LABEL_CANDIDATES 48
#define DRIVE_ROAD_LABELS 6
#define MAP_POI_LABELS 16
#define DRIVE_ROUTE_SEGS 128
struct DriveLabel {char name[113];s16 x,y;u8 kind,prio;int score;};
struct DriveRouteSeg {s16 x0,y0,x1,y1;};
static struct DriveLabel drive_labels[DRIVE_LABEL_CANDIDATES];
static struct DriveRouteSeg drive_route_segs[DRIVE_ROUTE_SEGS];
static int ndrive_labels=0,ndrive_route_segs=0;
static int drive_turn_valid=0,drive_turn_x=0,drive_turn_y=0;
static void drive_label_offer(int x,int y,int kind,int prio,const char*name);
static void drive_label_prepare(int z);
static void drive_label_render(void);
struct LBox{s16 x0,y0,x1,y1;};
#define LABEL_BOXES 40
static struct LBox lbox[LABEL_BOXES];static int nlbox=0;
#define LABEL_TEXTS 24
static char ltxt[LABEL_TEXTS][113];static int nltxt=0;
static int label_text_seen(const char*s){for(int i=0;i<nltxt;i++)if(streq(ltxt[i],s))return 1;return 0;}
static void label_text_add(const char*s){if(nltxt<LABEL_TEXTS)copystr(ltxt[nltxt++],29,s);}
static int label_space_free(int x0,int y0,int x1,int y1,int avoid_status){
 if(x0<4||x1>W-4||y1<MAP_TOP+3||y1>MAP_BOTTOM-3)return 0;
 if(x1>=350&&y0<=38)return 0; /* clock and temperature */
 if(!freecam&&y1>=HUD_BOTTOM_Y-2)return 0;
 if(freecam&&x1>=390&&y1>=HUD_BOTTOM_Y+5)return 0;
 if(avoid_status&&nav_card_w&&x1>=nav_card_x-3&&x0<=nav_card_x+nav_card_w+3&&y1>=nav_card_y-3&&y0<=nav_card_y+nav_card_h+3)return 0;
 for(int i=0;i<nlbox;i++)if(!(x1<lbox[i].x0||x0>lbox[i].x1||y1<lbox[i].y0||y0>lbox[i].y1))return 0;
 if(nlbox<LABEL_BOXES){lbox[nlbox].x0=x0;lbox[nlbox].y0=y0;lbox[nlbox].x1=x1;lbox[nlbox].y1=y1;nlbox++;}
 return 1;
}
static int label_free(int x0,int y0,int x1,int y1){return label_space_free(x0,y0,x1,y1,1);}
static u16 label_color(int kind){if(kind==3)return C_CYAN;if(kind==4)return C_WHITE;if(kind==5)return C_MUTED;if(kind>=6)return C_WARN;if(kind==1)return C_WHITE;return C_MUTED;}
#include "poi_icons.h"
static void poi_icon(int kind,int x,int y){
 if(kind<6||kind>8)return;
 int icon=kind==6?0:kind==7?1:2;
 u16 backing=theme_resolved?rgb(13,26,41):rgb(237,245,253);
 u16 rim=theme_resolved?rgb(83,111,143):rgb(158,182,208);
 u16 ink=icon==0?(theme_resolved?rgb(255,199,102):rgb(157,85,8)):
  icon==1?(theme_resolved?rgb(120,181,255):rgb(0,99,207)):
  (theme_resolved?rgb(97,218,187):rgb(0,116,96));
 /* Small translucent badges use precomputed coverage, not backdrop filtering.
  * Their full antialiased footprint stays inside the reserved 16x16 box. */
 for(int j=0;j<16;j++)for(int i=0;i<16;i++){
  int at=j*16+i,sx=x+i-8,sy=y+j-8;
  blend_pixel(sx,sy,backing,(poi_coverage[0][at]*13+7)/15);
  blend_pixel(sx,sy,rim,(poi_coverage[1][at]*10+7)/15);
  blend_pixel(sx,sy,ink,poi_coverage[2+icon][at]);
 }
}
#define POI_CANDIDATES 32
struct PoiLabel {int x,y,kind,prio;};
static struct PoiLabel poi_labels[POI_CANDIDATES];static int npoi,poi_visible;
static int poi_before(struct PoiLabel a,struct PoiLabel b){
 if(a.prio!=b.prio)return a.prio<b.prio;
 /* Stable ordering independent of tile/cache traversal. */
 if(a.y!=b.y)return a.y>b.y;return a.x<b.x;
}
static void poi_offer(int x,int y,int kind,int prio){
 if(x<12||x>W-12||y<MAP_TOP+12||y>HUD_BOTTOM_Y-11)return;
 for(int i=0;i<npoi;i++)if(poi_labels[i].kind==kind&&iabs32(poi_labels[i].x-x)<=2&&iabs32(poi_labels[i].y-y)<=2)return;
 struct PoiLabel p={x,y,kind,prio};int at=0;
 while(at<npoi&&!poi_before(p,poi_labels[at]))at++;
 if(at>=POI_CANDIDATES)return;
 if(npoi<POI_CANDIDATES)npoi++;
 for(int i=npoi-1;i>at;i--)poi_labels[i]=poi_labels[i-1];poi_labels[at]=p;
}
static void poi_reserve(void){
 int count=npoi;poi_visible=0;
 for(int i=0;i<count&&poi_visible<MAP_POI_LABELS;i++){
  struct PoiLabel p=poi_labels[i];
  if(label_space_free(p.x-8,p.y-8,p.x+8,p.y+8,0))poi_labels[poi_visible++]=p;
 }
}
static void poi_render(void){for(int i=0;i<poi_visible;i++)poi_icon(poi_labels[i].kind,poi_labels[i].x,poi_labels[i].y);}
static void draw_tile_labels(struct Tile*t,int viewz,int plo,int phi){if(!t)return;const u8*p=t->data,*end=t->data+t->len;int hs=8;if(t->len<(u32)hs)return;u16 ng=rd16(p),nl=rd16(p+2);p+=hs;for(u16 f=0;f<ng&&p+4<=end;f++){int np=p[2];p+=4+(unsigned)np*4;if(p>end)return;}int span=span_for_level((int)t->lvl);for(u16 i=0;i<nl&&p+8<=end;i++){if(!(i&15))render_checkpoint();int kind=p[0],prio=p[1];u16 lx=rd16(p+2),ly=rd16(p+4);int tl=p[6];p+=8;if(p+tl>end)return;if(((plo==256&&kind>=6)||(plo!=256&&kind<6&&prio>=plo&&prio<=phi))&&tl>0&&tl<113){char b[113];int n=tl;for(int j=0;j<n;j++)b[j]=(char)p[j];b[n]=0;s32 lon=(s32)t->tx*span+map_tile_offset(lx,(u32)span);s32 lat=(s32)t->ty*span+map_tile_offset(ly,(u32)span);int x,y;world_to_screen(lat,lon,viewz,&x,&y);if(kind>=6){if(plo==256)poi_offer(x,y,kind,prio);}else if(plo!=256){if(!freecam)drive_label_offer(x,y,kind,prio,b);else{int w=textw(b,1),x0=x-w/2,y0=y-6;if(!label_text_seen(b)&&label_free(x0-2,y0-2,x0+w+2,y0+14)){u16 c=label_color(kind);text(x0+1,y0+1,b,C_BG,1);text(x0,y0,b,c,1);label_text_add(b);}}}}p+=tl;}}

/* CPM5 directional speed/flag pairs and UTF-8 street names. */
struct RoadCtx {int valid,speed,reverse_speed;u8 speed_flags,reverse_flags;char street[113];};static struct RoadCtx roadctx;
/* road-name fallback is longitudinal, not radial.  The normal
 * nearest-road winner still owns speed/flags.  If that exact matched geometry
 * is unnamed, a bounded topology walk follows connected road geometry in both
 * directions and borrows the first confident name encountered.  The result is
 * cached across connected geometry fragments, so tracing runs only when the
 * matched road genuinely changes or continuity is lost. */
struct RoadGeomRef {
  int valid; s32 tx,ty; u16 gi; u8 cls,mi; int seg;
  s32 slat,slon,s2lat,s2lon,elat,elon,e2lat,e2lon;
};
struct RoadNameCache {int key_valid,name_valid;char name[113];struct RoadGeomRef last;};
static struct RoadNameCache roadname_cache;
/* Road context must not latch a bad startup GPS fix while stationary.
 * Moving vehicles refresh every service call; below 2 mph we refresh at
 * least every 3 calls, and immediately after a material GPS correction. */
static s32 roadctx_lat=0,roadctx_lon=0;static int roadctx_pos_valid=0,roadctx_slow_age=0;
static int roadctx_position_changed(s32 lat,s32 lon){if(!roadctx_pos_valid)return 1;s32 dy=lat-roadctx_lat,dx=lon-roadctx_lon;int cq=coslat_q15(lat);dx=lon_scale(dx,cq);if(dy<0)dy=-dy;if(dx<0)dx=-dx;return dy>=12||dx>=12;}
static const u8* cpm_meta_start(struct Tile*t,u16*nm){if(!t||t->len<8)return 0;const u8*p=t->data,*end=t?t->data+t->len:0;u16 ng=rd16(p),nl=rd16(p+2);*nm=rd16(p+4);p+=8;for(u16 i=0;i<ng;i++){if(p+4>end)return 0;int np=p[2];p+=4+(unsigned)np*4;if(p>end)return 0;}for(u16 i=0;i<nl;i++){if(p+8>end)return 0;int tl=p[6];p+=8+tl;if(p>end)return 0;}return p;}
static int read_road_meta(struct Tile*t,int idx,struct RoadCtx*out){u16 nm=0;const u8*p=cpm_meta_start(t,&nm),*end=t?t->data+t->len:0;if(!p||idx<0||idx>=nm)return 0;for(int i=0;i<nm;i++){int speed,flags=0,back=0,bflags=0,tl;if(p+5>end)return 0;speed=p[0];flags=p[1];back=p[2];bflags=p[3];tl=p[4];p+=5;if(p+tl>end)return 0;if(i==idx){out->speed=speed;out->speed_flags=(u8)flags;out->reverse_speed=back;out->reverse_flags=(u8)bflags;int n=tl;if(n>112)n=112;for(int j=0;j<n;j++)out->street[j]=(char)p[j];out->street[n]=0;out->valid=1;return 1;}p+=tl;}return 0;}
static int display_imperial(void);
static int display_limit(int value,int flags){if(!value)return 0;if(display_imperial())return flags&2?value:(value*100000+80467)/160934;return flags&2?(value*160934+50000)/100000:value;}
static void append_road_limit(char*b,unsigned cap,const struct RoadCtx*r){if(!r->valid){appendstr(b,cap,"--");return;}if(r->speed_flags&8){appendstr(b,cap,"VAR");return;}if(r->speed_flags&4){appendstr(b,cap,"INF");return;}if(r->speed<=0){appendstr(b,cap,"--");return;}append_int(b,cap,display_limit(r->speed,r->speed_flags));if(r->speed_flags&1)appendstr(b,cap,"?");}

static u32 seg_dist2(s32 ax,s32 ay,s32 bx,s32 by){int shift=0;while(ax>16383||ax< -16383||ay>16383||ay< -16383||bx>16383||bx< -16383||by>16383||by< -16383){ax>>=1;ay>>=1;bx>>=1;by>>=1;shift++;}s32 vx=bx-ax,vy=by-ay;u32 len2=(u32)(vx*vx+vy*vy);s32 dot=-(ax*vx+ay*vy),qx=ax,qy=ay;if(len2&&dot>0){if((u32)dot>=len2){qx=bx;qy=by;}else{u32 den=len2,num=(u32)dot;while(den>0xffffffu){den=(den+1)>>1;num>>=1;}u32 t=udiv32(num<<8,den);qx=ax+((vx*(s32)t)>>8);qy=ay+((vy*(s32)t)>>8);}}u32 d=square_distance(qx,qy);while(shift--){if(d>0x3fffffffu)return 0xffffffffu;d<<=2;}return d;}
static u32 road_seg_m(s32 la0,s32 lo0,s32 la1,s32 lo1){int cq=coslat_q15((la0+la1)/2);u32 dx=(u32)iabs32(lon_scale(lo1-lo0,cq)),dy=(u32)iabs32(la1-la0);u32 hi=dx>dy?dx:dy,lo=dx>dy?dy:dx;u32 q=hi+((lo*3u)>>3);return udiv32(q*111u+50u,100u);}
static void road_raw_point(struct Tile*t,const u8*p,s32*lat,s32*lon){int span=256;int lx=rd16(p),ly=rd16(p+2);*lon=(s32)t->tx*span+map_tile_offset(lx,(u32)span);*lat=(s32)t->ty*span+map_tile_offset(ly,(u32)span);}
static int road_ref_fill(struct Tile*t,u16 gi,int cls,int mi,const u8*p,int np,int seg,struct RoadGeomRef*r){if(!t||!p||np<2||!r)return 0;memzero(r,sizeof(*r));r->valid=1;r->tx=t->tx;r->ty=t->ty;r->gi=gi;r->cls=(u8)cls;r->mi=(u8)mi;r->seg=seg;road_raw_point(t,p,&r->slat,&r->slon);road_raw_point(t,p+4,&r->s2lat,&r->s2lon);road_raw_point(t,p+(np-1)*4,&r->elat,&r->elon);road_raw_point(t,p+(np-2)*4,&r->e2lat,&r->e2lon);return 1;}
static int road_ref_same(const struct RoadGeomRef*a,const struct RoadGeomRef*b){return a&&b&&a->valid&&b->valid&&a->tx==b->tx&&a->ty==b->ty&&a->gi==b->gi;}
static u32 road_join_d2(s32 alat,s32 alon,s32 blat,s32 blon){int cq=coslat_q15((alat+blat)/2);s32 dx=lon_scale(blon-alon,cq),dy=blat-alat;return square_distance(dx,dy);}
/* Straightness score: 1024 is straight, ~768 is about a 45-degree bend.
 * Reverse/U-turn candidates are rejected.  Same road class gets a small
 * preference but geometry remains the primary signal. */
static int road_cont_score(s32 clat,s32 clon,s32 inlat,s32 inlon,s32 outlat,s32 outlon,int same_cls,u32 join_d2){int cq=coslat_q15(clat);s32 ix=lon_scale(clon-inlon,cq),iy=clat-inlat,ox=lon_scale(outlon-clon,cq),oy=outlat-clat;s32 dot=ix*ox+iy*oy;if(dot<=0)return-1;u32 cr=(u32)iabs32(ix*oy-iy*ox),d=(u32)dot,sum=d+cr;if(!sum)return-1;int angle=1024-(int)udiv32(cr*512u,sum);int score=angle+(same_cls?80:0)-(int)(join_d2*8u);return score>=640?score:-1;}
#define ROAD_TRACE_JOIN_D2 16u       /* ~4 m endpoint tolerance after tile quantisation */
#define ROAD_TRACE_MAX_M 200u        /* bounded along-road search in each direction */
#define ROAD_TRACE_MAX_STEPS 12
#define ROAD_TRACE_AMBIG 48
static int road_refs_continuous(const struct RoadGeomRef*a,const struct RoadGeomRef*b){if(road_ref_same(a,b))return 1;if(!a||!b||!a->valid||!b->valid)return 0;int best=-1;u32 d;int s;
  d=road_join_d2(a->slat,a->slon,b->slat,b->slon);if(d<=ROAD_TRACE_JOIN_D2){s=road_cont_score(a->slat,a->slon,a->s2lat,a->s2lon,b->s2lat,b->s2lon,a->cls==b->cls,d);if(s>best)best=s;}
  d=road_join_d2(a->slat,a->slon,b->elat,b->elon);if(d<=ROAD_TRACE_JOIN_D2){s=road_cont_score(a->slat,a->slon,a->s2lat,a->s2lon,b->e2lat,b->e2lon,a->cls==b->cls,d);if(s>best)best=s;}
  d=road_join_d2(a->elat,a->elon,b->slat,b->slon);if(d<=ROAD_TRACE_JOIN_D2){s=road_cont_score(a->elat,a->elon,a->e2lat,a->e2lon,b->s2lat,b->s2lon,a->cls==b->cls,d);if(s>best)best=s;}
  d=road_join_d2(a->elat,a->elon,b->elat,b->elon);if(d<=ROAD_TRACE_JOIN_D2){s=road_cont_score(a->elat,a->elon,a->e2lat,a->e2lon,b->e2lat,b->e2lon,a->cls==b->cls,d);if(s>best)best=s;}
  return best>=700;
}
static const u8* road_feature_points(struct Tile*t,u16 gi,int*cls,int*mi,int*np){if(!t||t->lvl!=6||t->len<8)return 0;const u8*p=t->data,*end=t->data+t->len;u16 ng=rd16(p);p+=8;for(u16 g=0;g<ng&&p+4<=end;g++){int layer=p[0],style=p[1],n=p[2],m=p[3];p+=4;if(p+(unsigned)n*4>end)return 0;if(g==gi){if(layer!=0||m==255||n<2)return 0;if(cls)*cls=style;if(mi)*mi=m;if(np)*np=n;return p;}p+=(unsigned)n*4;}return 0;}
static u32 road_feature_length_m(const struct RoadGeomRef*r){struct Tile*t=r?get_tile(6,r->tx,r->ty):0;int np=0;const u8*p=t?road_feature_points(t,r->gi,0,0,&np):0;if(!p)return 0;u32 m=0;s32 la0,lo0,la1,lo1;road_raw_point(t,p,&la0,&lo0);for(int i=1;i<np;i++){road_raw_point(t,p+i*4,&la1,&lo1);u32 d=road_seg_m(la0,lo0,la1,lo1);if(0xffffffffu-m<d)return 0xffffffffu;m+=d;la0=la1;lo0=lo1;}return m;}
static u32 road_initial_side_m(const struct RoadGeomRef*r,int side){struct Tile*t=r?get_tile(6,r->tx,r->ty):0;int np=0;const u8*p=t?road_feature_points(t,r->gi,0,0,&np):0;if(!p||r->seg<0||r->seg>=np-1)return ROAD_TRACE_MAX_M+1;u32 m=0;s32 la0,lo0,la1,lo1;road_raw_point(t,p,&la0,&lo0);for(int j=0;j<np-1;j++){road_raw_point(t,p+(j+1)*4,&la1,&lo1);u32 d=road_seg_m(la0,lo0,la1,lo1);if((!side&&j<r->seg)||(side&&j>r->seg))m+=d;else if(j==r->seg)m+=d/2;la0=la1;lo0=lo1;}return m;}
static int road_trace_visited(const struct RoadGeomRef*v,int n,const struct RoadGeomRef*r){for(int i=0;i<n;i++)if(road_ref_same(&v[i],r))return 1;return 0;}
/* Pick the single most plausible road continuation at one endpoint.  We do not
 * favour named candidates here: topology is selected first, then metadata is
 * inspected, which prevents the trace from bending merely to obtain a name. */
static int road_trace_pick(const struct RoadGeomRef*cur,int exit_side,const struct RoadGeomRef*visited,int nvis,struct RoadGeomRef*out,int*enter_side){if(!cur||!cur->valid)return 0;s32 clat=exit_side?cur->elat:cur->slat,clon=exit_side?cur->elon:cur->slon,inlat=exit_side?cur->e2lat:cur->s2lat,inlon=exit_side?cur->e2lon:cur->s2lon;int tx=tile_div(clon,6),ty=tile_div(clat,6),best=-1,second=-1,benter=0;struct RoadGeomRef br;memzero(&br,sizeof(br));for(int y=ty-1;y<=ty+1;y++)for(int x=tx-1;x<=tx+1;x++){struct Tile*t=get_tile(6,x,y);if(!t||t->len<8)continue;const u8*p=t->data,*end=t->data+t->len;u16 ng=rd16(p);p+=8;for(u16 g=0;g<ng&&p+4<=end;g++){int layer=p[0],cls=p[1],np=p[2],mi=p[3];p+=4;if(p+(unsigned)np*4>end)break;if(layer==0&&mi!=255&&np>=2){struct RoadGeomRef cr;road_ref_fill(t,g,cls,mi,p,np,0,&cr);if(!road_ref_same(cur,&cr)&&!road_trace_visited(visited,nvis,&cr)){u32 d=road_join_d2(clat,clon,cr.slat,cr.slon);if(d<=ROAD_TRACE_JOIN_D2){int sc=road_cont_score(clat,clon,inlat,inlon,cr.s2lat,cr.s2lon,cur->cls==cr.cls,d);if(sc>=0){if(sc>best){second=best;best=sc;br=cr;benter=0;}else if(sc>second)second=sc;}}d=road_join_d2(clat,clon,cr.elat,cr.elon);if(d<=ROAD_TRACE_JOIN_D2){int sc=road_cont_score(clat,clon,inlat,inlon,cr.e2lat,cr.e2lon,cur->cls==cr.cls,d);if(sc>=0){if(sc>best){second=best;best=sc;br=cr;benter=1;}else if(sc>second)second=sc;}}}}p+=(unsigned)np*4;}}
  if(best<0||(second>=0&&best-second<ROAD_TRACE_AMBIG))return 0;*out=br;if(enter_side)*enter_side=benter;return 1;}
static int road_trace_dir(const struct RoadGeomRef*start,int side,char*out,unsigned cap,u32*found_m){u32 used=road_initial_side_m(start,side);if(used>ROAD_TRACE_MAX_M)return 0;struct RoadGeomRef cur=*start,visited[ROAD_TRACE_MAX_STEPS+1];int nvis=0;visited[nvis++]=cur;int exit_side=side;for(int step=0;step<ROAD_TRACE_MAX_STEPS&&used<=ROAD_TRACE_MAX_M;step++){struct RoadGeomRef nx;int enter=0;if(!road_trace_pick(&cur,exit_side,visited,nvis,&nx,&enter))return 0;struct Tile*t=get_tile(6,nx.tx,nx.ty);struct RoadCtx nr;memzero(&nr,sizeof(nr));if(t&&read_road_meta(t,nx.mi,&nr)&&nr.street[0]){copystr(out,cap,nr.street);if(found_m)*found_m=used;return 1;}u32 add=road_feature_length_m(&nx);if(!add||add==0xffffffffu||used+add>ROAD_TRACE_MAX_M)return 0;used+=add;cur=nx;if(nvis<ROAD_TRACE_MAX_STEPS+1)visited[nvis++]=cur;exit_side=enter?0:1;}return 0;}
static int road_trace_name(const struct RoadGeomRef*r,char*out,unsigned cap){char a[29],b[29];a[0]=b[0]=0;u32 am=0xffffffffu,bm=0xffffffffu;int ha=road_trace_dir(r,0,a,sizeof(a),&am),hb=road_trace_dir(r,1,b,sizeof(b),&bm);if(!ha&&!hb)return 0;if(ha&&hb&&!streq(a,b)){u32 dif=am>bm?am-bm:bm-am;if(dif<30u)return 0;}const char*s=ha&&(!hb||am<=bm)?a:b;copystr(out,cap,s);return out[0]!=0;}
static void scan_road_tile(struct Tile*t,s32 plat,s32 plon,u32*best,struct RoadGeomRef*winner){if(!t||t->lvl!=6||t->len<8)return;const u8*p=t->data,*end=t->data+t->len;u16 ng=rd16(p);p+=8;int span=256,cq=coslat_q15(plat);for(u16 g=0;g<ng&&p+4<=end;g++){int layer=p[0],cls=p[1],np=p[2],mi=p[3];p+=4;if(p+(unsigned)np*4>end)return;if(layer==0&&mi!=255&&np>=2){int lx=rd16(p),ly=rd16(p+2);s32 alon=(s32)t->tx*span+map_tile_offset(lx,(u32)span),alat=(s32)t->ty*span+map_tile_offset(ly,(u32)span);s32 ax=lon_scale(alon-plon,cq),ay=alat-plat;const u8*q=p+4;for(int i=1;i<np;i++,q+=4){lx=rd16(q);ly=rd16(q+2);s32 blon=(s32)t->tx*span+map_tile_offset(lx,(u32)span),blat=(s32)t->ty*span+map_tile_offset(ly,(u32)span);s32 bx=lon_scale(blon-plon,cq),by=blat-plat;u32 d=seg_dist2(ax,ay,bx,by);if(gps.mph10>=20){int k=((shown_course()+5)/10)%36;int hx=SIN36[k],hy=COS36[k];s32 vx=bx-ax,vy=by-ay;u32 norm=(u32)(iabs32(vx)+iabs32(vy));if(norm){s32 dp=vx*hx+vy*hy;u32 ad=(u32)(dp<0?-dp:dp);u32 lo=norm*12000u,mid=norm*22000u;if(ad<lo)d+=900;else if(ad<mid)d+=225;}}if(d<*best){*best=d;road_ref_fill(t,g,cls,mi,p,np,i-1,winner);}ax=bx;ay=by;}}p+=(unsigned)np*4;}}
static void roadname_cache_set(const char*s,const struct RoadGeomRef*r){if(!r||!r->valid){roadname_cache.key_valid=roadname_cache.name_valid=0;return;}roadname_cache.key_valid=1;roadname_cache.last=*r;roadname_cache.name_valid=s&&s[0]?1:0;roadname_cache.name[0]=0;if(roadname_cache.name_valid)copystr(roadname_cache.name,sizeof(roadname_cache.name),s);}
static void road_direction_select(struct Tile*t,const struct RoadGeomRef*r){int n=0;const u8*p=road_feature_points(t,r->gi,0,0,&n);if(!p||r->seg<0||r->seg+1>=n)return;s32 la,lo,lb,lob;road_raw_point(t,p+r->seg*4,&la,&lo);road_raw_point(t,p+(r->seg+1)*4,&lb,&lob);int k=((shown_course()+5)/10)%36;long long dot=(long long)lon_scale(lob-lo,coslat_q15(gps.lat))*SIN36[k]+(long long)(lb-la)*COS36[k];if(gps.mph10<30){if(roadctx.speed!=roadctx.reverse_speed||roadctx.speed_flags!=roadctx.reverse_flags){roadctx.speed=0;roadctx.speed_flags=0;}}else if(dot<0){roadctx.speed=roadctx.reverse_speed;roadctx.speed_flags=roadctx.reverse_flags;}}
static void road_estimate_missing(const struct RoadGeomRef*r);
static void update_road_context(void){if(!gps.fix){roadctx_slow_age=0;return;}int moved=roadctx_position_changed(gps.lat,gps.lon);if(gps.mph10<20&&roadctx.valid&&!moved){if(++roadctx_slow_age<3)return;}roadctx_slow_age=0;roadctx.valid=0;roadctx.speed=0;roadctx.speed_flags=0;roadctx.street[0]=0;if(!mapok||mh.maxlvl<6)return;s32 plat=gps.lat,plon=gps.lon;int tx=tile_div(plon,6),ty=tile_div(plat,6);u32 best=2500;struct RoadGeomRef winner;memzero(&winner,sizeof(winner));for(int y=ty-1;y<=ty+1;y++)for(int x=tx-1;x<=tx+1;x++)scan_road_tile(get_tile(6,x,y),plat,plon,&best,&winner);if(winner.valid){struct Tile*t=get_tile(6,winner.tx,winner.ty);if(t&&read_road_meta(t,winner.mi,&roadctx)){road_estimate_missing(&winner);t=get_tile(6,winner.tx,winner.ty);if(t)road_direction_select(t,&winner);if(roadctx.street[0])roadname_cache_set(roadctx.street,&winner);else if(roadname_cache.key_valid&&road_ref_same(&roadname_cache.last,&winner)){if(roadname_cache.name_valid)copystr(roadctx.street,sizeof(roadctx.street),roadname_cache.name);}else if(roadname_cache.key_valid&&roadname_cache.name_valid&&road_refs_continuous(&roadname_cache.last,&winner)){copystr(roadctx.street,sizeof(roadctx.street),roadname_cache.name);roadname_cache.last=winner;}else{char traced[113];traced[0]=0;if(road_trace_name(&winner,traced,sizeof(traced))){copystr(roadctx.street,sizeof(roadctx.street),traced);roadname_cache_set(traced,&winner);}else roadname_cache_set(0,&winner);}}}roadctx_lat=plat;roadctx_lon=plon;roadctx_pos_valid=1;}

#include "global_regions.h"
#include "speed_estimates.h"
static int display_imperial(void){if(pref.units)return pref.units==2;if(region_iso[0])return (region_flags&1)!=0;if(roadctx.valid&&roadctx.speed&&(roadctx.speed_flags&2))return 1;return (region_flags&1)!=0;}
static int display_speed(void){return display_imperial()?(gps.mph10+5)/10:(gps.mph10*160934+500000)/1000000;}

/* CPR2 v5 turn-aware compact contraction-hierarchy runtime.
 * Physical road geometry stays compact. Only OSM turn-restricted junctions
 * gain arrival-context routing states, so CH shortcuts cannot encode an
 * illegal OSM no/only turn manoeuvre. */
struct CprHeader {char magic[4];u16 ver,flags;u32 nodes,phys_nodes,base_edges,route_edges,tiles,tile_refs,ch_arcs,upf_count,upr_count,geom_points,src_refs,tile_off,tref_off,node_off,edge_off,base_off,src_off,arc_off,upf_off,upr_off,geom_off;s32 minlat,minlon,maxlat,maxlon;};
struct CprTile {s32 tx,ty;u32 first,count;};
struct CprNode {s32 lat,lon;u32 first_edge;u16 edge_count,flags;u32 phys,rank,comp,upf_first;u16 upf_count,upr_count;u32 upr_first;};
struct CprBase {u32 cost,geom_first,src_first,to_state;u16 geom_count,src_count;u8 cls,flags;u16 pad;};
struct CprArc {u32 from,to,cost,left,right;u8 cls,flags;u16 pad;};
struct CprPoint {s32 lat,lon;};
/* QAD7 rank-ordered node blocks and direct successor offsets. */
struct QAdj7QueryHeader {char magic[4];u16 ver,entry_size;u32 nodes,ch_arcs,upf_count,upr_count,route_edges,route_bytes,index_off,index_bytes,data_off,data_bytes,rmeta_off,rmeta_bytes,flags,reserved;};
/* QAD7 combines geometry and arithmetic metadata in a packet stream. */
struct QRec7 {u32 from,cost,left,right_geom,to_flags,geom_first,geom_info,packet_off;};
struct QAdj7Header {struct QAdj7QueryHeader q;u32 packet_off,packet_bytes,packet_points,packet_entry_size;};
struct QAdj7Entry {u32 other,cost,arc,next_off;};
#define QAD_RECON_META 2u
#define QAD_LOCAL_ARC_IDS 4u
#define QAD_DIRECT_INDEX 8u
#define QAD_RANK_ORDER 16u
#define QAD7_PACKET_GEOM 64u
#define QAD7_POINT_SIZE 20u
#define CPR_ARC_SHORTCUT 1u
#define CPR_ARC_GEOM_REVERSED 2u
static struct CprHeader rh;static struct QAdj7QueryHeader qh7_query;static struct QAdj7Header qh7;static int routefd=-1,qadjfd=-1,routeok=0,qadjok=0,qadjmode=0;
static u8*qadj_map=0;static u32 qadj_map_bytes=0;static int qadj_map_ok=0;
static u8*route_geom_map=0;static u32 route_geom_map_bytes=0,route_geom_map_delta=0,route_geom_map_fileoff=0;static int route_geom_map_ok=0;
/* Routing performs scattered reads from a ~GB-scale CPR2 file.
 * Larger contiguous caches trade only 256 KiB of static RAM for substantially
 * fewer SD-card seeks while CH is active. */
/* Profiling showed the 8 KiB node window was
 * almost never reused (16 hits / 3114 misses), while one global 8 KiB arc
 * window thrashed badly during bidirectional CH (about 60% hits).  Keep the
 * proven 8 KiB windows for base/ref/geometry, read nodes exactly, and retain
 * several small arc pages simultaneously.  No route.cpr format change. */
#define CPR_BASE_CACHE 8192
#define CPR_REF_CACHE 8192
#define CPR_GEOM_PAGE 4096
#define CPR_GEOM_SLOTS 16
#define CPR_TILE_PAGE 4096
#define CPR_TILE_SLOTS 8
#define CPR_ARC_PAGE 2048
#define CPR_ARC_SLOTS 16
static u32 cpr_bcache_w[CPR_BASE_CACHE/4],cpr_rcache_w[4][CPR_REF_CACHE/4];
static u32 cpr_gcache_w[CPR_GEOM_SLOTS][CPR_GEOM_PAGE/4],cpr_tcache_w[CPR_TILE_SLOTS][CPR_TILE_PAGE/4];
static u32 cpr_acache_w[CPR_ARC_SLOTS][CPR_ARC_PAGE/4];
static u32 cpr_bcache_first=0,cpr_bcache_count=0,cpr_rcache_first[4]={0,0,0,0},cpr_rcache_count[4]={0,0,0,0};
static u32 cpr_gcache_first[CPR_GEOM_SLOTS]={0},cpr_gcache_count[CPR_GEOM_SLOTS]={0},cpr_gcache_stamp[CPR_GEOM_SLOTS]={0},cpr_gcache_tick=1;
static u32 cpr_tcache_first[CPR_TILE_SLOTS]={0},cpr_tcache_count[CPR_TILE_SLOTS]={0},cpr_tcache_stamp[CPR_TILE_SLOTS]={0},cpr_tcache_tick=1;
static u32 cpr_acache_first[CPR_ARC_SLOTS]={0},cpr_acache_count[CPR_ARC_SLOTS]={0},cpr_acache_stamp[CPR_ARC_SLOTS]={0},cpr_acache_tick=1;
/* profiling: debug-gated counters only. No per-read logging and no
 * extra clock syscall in the CPR hot path. One route produces compact summary
 * lines in state.log, allowing cache/I/O tuning from measurements rather than
 * assumptions. */
static int route_prof_on=0;
static u32 route_prof_n_hit=0,route_prof_n_miss=0,route_prof_n_bytes=0;
static u32 route_prof_b_hit=0,route_prof_b_miss=0,route_prof_b_bytes=0;
static u32 route_prof_a_hit=0,route_prof_a_miss=0,route_prof_a_bytes=0;
static u32 route_prof_g_hit=0,route_prof_g_miss=0,route_prof_g_bytes=0;
static u32 route_prof_r_hit[4]={0,0,0,0},route_prof_r_miss[4]={0,0,0,0},route_prof_r_bytes[4]={0,0,0,0};
static u32 route_prof_tile_reads=0,route_prof_tile_bytes=0,route_prof_relax=0;
static u32 route_prof_qadj_reads=0,route_prof_qadj_bytes=0,route_prof_qidx_bytes=0,route_prof_strong_stop=0,route_prof_geom_reuse_arcs=0,route_prof_geom_reuse_pts=0;
static u32 route_prof_qrec_hit=0,route_prof_qrec_miss=0,route_prof_qrec_bytes=0;
static u32 route_prof_stall=0,route_prof_qmap_access=0,route_prof_gmap_access=0;
static u32 route_prof_div_fallback=0,route_prof_mfast=0,route_prof_packet_pts=0;
static u32 route_prof_build_phase_steps[16]={0};
static u32 route_prof_start_ms=0,route_prof_query_ms=0,route_prof_build_ms=0,route_prof_cpu_ms=0,route_prof_slice_ms=0;
static u32 route_prof_pre_io=0,route_prof_build_io=0;
#define RPROF_INC(x) do{if(route_prof_on)(x)++;}while(0)
#define RPROF_ADD(x,v) do{if(route_prof_on)(x)+=(u32)(v);}while(0)
static u32 route_prof_io_bytes(void){u32 n=route_prof_n_bytes+route_prof_b_bytes+route_prof_a_bytes+route_prof_g_bytes+route_prof_tile_bytes+route_prof_qadj_bytes+route_prof_qidx_bytes+route_prof_qrec_bytes;for(int i=0;i<4;i++)n+=route_prof_r_bytes[i];return n;}
static void route_prof_reset(void){route_prof_on=pref.debug?1:0;route_prof_n_hit=route_prof_n_miss=route_prof_n_bytes=0;route_prof_b_hit=route_prof_b_miss=route_prof_b_bytes=0;route_prof_a_hit=route_prof_a_miss=route_prof_a_bytes=0;route_prof_g_hit=route_prof_g_miss=route_prof_g_bytes=0;for(int i=0;i<4;i++)route_prof_r_hit[i]=route_prof_r_miss[i]=route_prof_r_bytes[i]=0;route_prof_tile_reads=route_prof_tile_bytes=route_prof_relax=0;route_prof_qadj_reads=route_prof_qadj_bytes=route_prof_qidx_bytes=route_prof_strong_stop=route_prof_geom_reuse_arcs=route_prof_geom_reuse_pts=0;route_prof_qrec_hit=route_prof_qrec_miss=route_prof_qrec_bytes=0;route_prof_stall=route_prof_qmap_access=route_prof_gmap_access=0;route_prof_div_fallback=route_prof_mfast=route_prof_packet_pts=0;memzero(route_prof_build_phase_steps,sizeof(route_prof_build_phase_steps));route_prof_query_ms=route_prof_build_ms=route_prof_cpu_ms=route_prof_slice_ms=0;route_prof_pre_io=route_prof_build_io=0;route_prof_start_ms=route_prof_on?s_now_ms():0;}
static void qadj_reset_caches(void);
static void qadj_unmap(void){qadj_reset_caches();if(qadj_map){s_munmap(qadj_map,qadj_map_bytes);qadj_map=0;}qadj_map_bytes=0;qadj_map_ok=0;}
static int qadj_try_map(u32 bytes){qadj_unmap();if(!bytes)return 0;void*p=s_mmap2(0,bytes,PROT_READ_,MAP_PRIVATE_,qadjfd,0);if(!p)return 0;qadj_map=(u8*)p;qadj_map_bytes=bytes;qadj_map_ok=1;return 1;}
static void route_geom_unmap(void){if(route_geom_map){s_munmap(route_geom_map,route_geom_map_bytes);route_geom_map=0;}route_geom_map_bytes=route_geom_map_delta=route_geom_map_fileoff=0;route_geom_map_ok=0;}
static int route_geom_try_map(void){route_geom_unmap();if(routefd<0||!rh.geom_points)return 0;u32 page=rh.geom_off&~4095u,delta=rh.geom_off-page;u32 gb=rh.geom_points*(u32)sizeof(struct CprPoint);if(gb/sizeof(struct CprPoint)!=rh.geom_points||delta>0xffffffffu-gb)return 0;u32 len=delta+gb;void*p=s_mmap2(0,len,PROT_READ_,MAP_PRIVATE_,routefd,page>>12);if(!p)return 0;route_geom_map=(u8*)p;route_geom_map_bytes=len;route_geom_map_delta=delta;route_geom_map_fileoff=page;route_geom_map_ok=1;return 1;}
static void load_qadj_data(void){
  qadjok=0;qadjmode=0;memzero(&qh7_query,sizeof(qh7_query));memzero(&qh7,sizeof(qh7));
  qadj_unmap();if(qadjfd>=0){s_close(qadjfd);qadjfd=-1;}qadjfd=(int)s_open(atlas_ready?atlas_qadj_path:QADJ_PATH,O_RDONLY_,0);if(qadjfd<0)return;
  char m[4];if(s_read(qadjfd,m,4)!=4)goto bad;s_lseek(qadjfd,0,SEEK_SET_);long rb=s_lseek(routefd,0,SEEK_END_);if(rb<=0)goto bad;
  if(m[0]=='Q'&&m[1]=='A'&&m[2]=='D'&&m[3]=='7'){
    u32 need=QAD_RECON_META|QAD_LOCAL_ARC_IDS|QAD_DIRECT_INDEX|QAD_RANK_ORDER|QAD7_PACKET_GEOM;
    if(s_read(qadjfd,&qh7,sizeof(qh7))!=(long)sizeof(qh7))goto bad;
    if(qh7.q.ver!=7||qh7.q.entry_size!=sizeof(struct QAdj7Entry)||(qh7.q.flags&need)!=need||qh7.q.nodes!=rh.nodes||qh7.q.ch_arcs!=rh.ch_arcs||qh7.q.upf_count!=rh.upf_count||qh7.q.upr_count!=rh.upr_count||qh7.q.route_edges!=rh.route_edges||(u32)rb!=qh7.q.route_bytes)goto bad;
    if(qh7.q.index_off!=sizeof(struct QAdj7Header)||qh7.q.index_bytes!=rh.nodes*4u||qh7.q.data_off!=qh7.q.index_off+qh7.q.index_bytes||qh7.q.rmeta_off!=qh7.q.data_off+qh7.q.data_bytes||qh7.q.rmeta_bytes!=rh.ch_arcs*(u32)sizeof(struct QRec7))goto bad;
    if(qh7.packet_off!=qh7.q.rmeta_off+qh7.q.rmeta_bytes||qh7.packet_entry_size!=QAD7_POINT_SIZE||qh7.packet_bytes!=qh7.packet_points*QAD7_POINT_SIZE)goto bad;
    long qb=s_lseek(qadjfd,0,SEEK_END_);if(qb<=0||(u32)qb!=qh7.packet_off+qh7.packet_bytes)goto bad;qh7_query=qh7.q;qadjmode=7;qadjok=1;qadj_try_map((u32)qb);return;
  }
bad:qadj_unmap();s_close(qadjfd);qadjfd=-1;qadjok=0;qadjmode=0;
}
static int load_route_data(void){route_geom_unmap();qadj_unmap();if(qadjfd>=0){s_close(qadjfd);qadjfd=-1;}qadjok=0;qadjmode=0;routeok=0;if(routefd>=0)s_close(routefd);routefd=(int)s_open(atlas_ready?atlas_route_path:ROUTE_PATH,O_RDONLY_,0);if(routefd<0)return 0;if(s_read(routefd,&rh,sizeof(rh))!=(long)sizeof(rh)){s_close(routefd);routefd=-1;return 0;}if(rh.magic[0]!='C'||rh.magic[1]!='P'||rh.magic[2]!='R'||rh.magic[3]!='2'||rh.ver!=5||!rh.nodes||!rh.base_edges||!rh.ch_arcs||!rh.geom_points){s_close(routefd);routefd=-1;return 0;}routeok=1;cpr_bcache_count=0;cpr_acache_tick=cpr_gcache_tick=cpr_tcache_tick=1;for(int i=0;i<CPR_ARC_SLOTS;i++){cpr_acache_first[i]=0;cpr_acache_count[i]=0;cpr_acache_stamp[i]=0;}for(int i=0;i<CPR_GEOM_SLOTS;i++){cpr_gcache_first[i]=0;cpr_gcache_count[i]=0;cpr_gcache_stamp[i]=0;}for(int i=0;i<CPR_TILE_SLOTS;i++){cpr_tcache_first[i]=0;cpr_tcache_count[i]=0;cpr_tcache_stamp[i]=0;}for(int i=0;i<4;i++){cpr_rcache_first[i]=0;cpr_rcache_count[i]=0;}route_geom_try_map();load_qadj_data();return 1;}
static int route_decode_tile(const u8*b,struct CprTile*out){out->tx=(s32)rd32u(b);out->ty=(s32)rd32u(b+4);u32 k=8;out->first=rd32u(b+k);out->count=rd32u(b+k+4);return 1;}
static int route_read_tile(u32 id,struct CprTile*out){if(!routeok||id>=rh.tiles||!out)return 0;u32 size=16;u32 per=CPR_TILE_PAGE/size,slot=CPR_TILE_SLOTS,old=0,oldst=0xffffffffu;for(u32 i=0;i<CPR_TILE_SLOTS;i++){if(cpr_tcache_count[i]&&id>=cpr_tcache_first[i]&&id<cpr_tcache_first[i]+cpr_tcache_count[i]){cpr_tcache_stamp[i]=++cpr_tcache_tick;return route_decode_tile((const u8*)cpr_tcache_w[i]+(id-cpr_tcache_first[i])*size,out);}if(!cpr_tcache_count[i]&&slot==CPR_TILE_SLOTS)slot=i;if(cpr_tcache_count[i]&&cpr_tcache_stamp[i]<oldst){oldst=cpr_tcache_stamp[i];old=i;}}if(slot==CPR_TILE_SLOTS)slot=old;u32 first=(id/per)*per,count=rh.tiles-first;if(count>per)count=per;s_lseek(routefd,(long)(rh.tile_off+first*size),SEEK_SET_);long n=s_read(routefd,cpr_tcache_w[slot],count*size);if(n<=0)return 0;RPROF_INC(route_prof_tile_reads);RPROF_ADD(route_prof_tile_bytes,n);cpr_tcache_first[slot]=first;cpr_tcache_count[slot]=(u32)n/size;cpr_tcache_stamp[slot]=++cpr_tcache_tick;return route_decode_tile((const u8*)cpr_tcache_w[slot]+(id-first)*size,out);}
static int route_find_tile(s32 tx,s32 ty,struct CprTile*out){u32 lo=0,hi=rh.tiles;while(lo<hi){u32 m=lo+(hi-lo)/2;struct CprTile t;if(!route_read_tile(m,&t))return 0;int c=tx<t.tx?-1:(tx>t.tx?1:(ty<t.ty?-1:(ty>t.ty?1:0)));if(!c){*out=t;return 1;}if(c<0)hi=m;else lo=m+1;}return 0;}
static int route_read_node(u32 id,struct CprNode*out){if(!routeok||id>=rh.nodes)return 0;RPROF_INC(route_prof_n_miss);s_lseek(routefd,(long)(rh.node_off+id*sizeof(struct CprNode)),SEEK_SET_);long n=s_read(routefd,out,sizeof(*out));if(n>0)RPROF_ADD(route_prof_n_bytes,n);return n==(long)sizeof(*out);}
static int route_read_base(u32 id,struct CprBase*out){if(!routeok||id>=rh.base_edges)return 0;u32 per=CPR_BASE_CACHE/(u32)sizeof(struct CprBase);if(!(id>=cpr_bcache_first&&id<cpr_bcache_first+cpr_bcache_count)){RPROF_INC(route_prof_b_miss);u32 first=(id/per)*per,count=rh.base_edges-first;if(count>per)count=per;s_lseek(routefd,(long)(rh.base_off+first*sizeof(struct CprBase)),SEEK_SET_);long n=s_read(routefd,cpr_bcache_w,count*sizeof(struct CprBase));if(n<=0)return 0;RPROF_ADD(route_prof_b_bytes,n);cpr_bcache_first=first;cpr_bcache_count=(u32)n/(u32)sizeof(struct CprBase);}else RPROF_INC(route_prof_b_hit);*out=((struct CprBase*)cpr_bcache_w)[id-cpr_bcache_first];return 1;}
static int route_read_arc(u32 id,struct CprArc*out){if(!routeok||id>=rh.ch_arcs)return 0;u32 per=CPR_ARC_PAGE/(u32)sizeof(struct CprArc),slot=CPR_ARC_SLOTS,oldest=0,oldstamp=0xffffffffu;for(u32 i=0;i<CPR_ARC_SLOTS;i++){if(cpr_acache_count[i]&&id>=cpr_acache_first[i]&&id<cpr_acache_first[i]+cpr_acache_count[i]){RPROF_INC(route_prof_a_hit);cpr_acache_stamp[i]=++cpr_acache_tick;*out=((struct CprArc*)cpr_acache_w[i])[id-cpr_acache_first[i]];return 1;}if(!cpr_acache_count[i]&&slot==CPR_ARC_SLOTS)slot=i;if(cpr_acache_count[i]&&cpr_acache_stamp[i]<oldstamp){oldstamp=cpr_acache_stamp[i];oldest=i;}}if(slot==CPR_ARC_SLOTS)slot=oldest;RPROF_INC(route_prof_a_miss);u32 first=(id/per)*per,count=rh.ch_arcs-first;if(count>per)count=per;s_lseek(routefd,(long)(rh.arc_off+first*sizeof(struct CprArc)),SEEK_SET_);long n=s_read(routefd,cpr_acache_w[slot],count*sizeof(struct CprArc));if(n<=0)return 0;RPROF_ADD(route_prof_a_bytes,n);cpr_acache_first[slot]=first;cpr_acache_count[slot]=(u32)n/(u32)sizeof(struct CprArc);cpr_acache_stamp[slot]=++cpr_acache_tick;*out=((struct CprArc*)cpr_acache_w[slot])[id-first];return 1;}
static int route_read_ref(int dir,u32 id,u32*out){u32 total=dir==3?rh.src_refs:(dir==2?rh.tile_refs:(dir?rh.upr_count:rh.upf_count)),off=dir==3?rh.src_off:(dir==2?rh.tref_off:(dir?rh.upr_off:rh.upf_off));if((unsigned)dir>=4u||id>=total)return 0;u32 per=CPR_REF_CACHE/4u;if(!(id>=cpr_rcache_first[dir]&&id<cpr_rcache_first[dir]+cpr_rcache_count[dir])){RPROF_INC(route_prof_r_miss[dir]);u32 first=(id/per)*per,count=total-first;if(count>per)count=per;s_lseek(routefd,(long)(off+first*4u),SEEK_SET_);long n=s_read(routefd,cpr_rcache_w[dir],count*4u);if(n<=0)return 0;RPROF_ADD(route_prof_r_bytes[dir],n);cpr_rcache_first[dir]=first;cpr_rcache_count[dir]=(u32)n/4u;}else RPROF_INC(route_prof_r_hit[dir]);*out=cpr_rcache_w[dir][id-cpr_rcache_first[dir]];return 1;}
static int route_read_upref(int dir,u32 id,u32*out){return route_read_ref(dir,id,out);}
#define QAD_INDEX_PAGE 4096u
#define QAD_INDEX_SLOTS 4u
#define QAD_BLOCK_BYTES 32768u
static u32 qadj7_buf_w[QAD_BLOCK_BYTES/4u];
static u32 qadj7_idx_w[QAD_INDEX_SLOTS][QAD_INDEX_PAGE/4u];static u32 qadj7_idx_first[QAD_INDEX_SLOTS]={0},qadj7_idx_count[QAD_INDEX_SLOTS]={0},qadj7_idx_stamp[QAD_INDEX_SLOTS]={0},qadj7_idx_tick=1;
/* Seed offsets belong to one regional file, including paged QAD7. */
static void qadj_reset_caches(void){memzero(qadj7_idx_count,sizeof(qadj7_idx_count));qadj7_idx_tick=1;}
static int qadj7_index_get(u32 node,u32*out){if(qadjmode!=7||!out||node>=rh.nodes)return 0;if(qadj_map_ok){u32 at=qh7_query.index_off+node*4u;if(at>qadj_map_bytes-4u)return 0;*out=rd32u(qadj_map+at);RPROF_INC(route_prof_qmap_access);return *out<qh7_query.data_bytes;}
 u32 per=QAD_INDEX_PAGE/4u,first=(node/per)*per,slot=QAD_INDEX_SLOTS,old=0,oldst=0xffffffffu;for(u32 i=0;i<QAD_INDEX_SLOTS;i++){if(qadj7_idx_count[i]&&node>=qadj7_idx_first[i]&&node<qadj7_idx_first[i]+qadj7_idx_count[i]){qadj7_idx_stamp[i]=++qadj7_idx_tick;*out=qadj7_idx_w[i][node-qadj7_idx_first[i]];return *out<qh7_query.data_bytes;}if(!qadj7_idx_count[i]&&slot==QAD_INDEX_SLOTS)slot=i;if(qadj7_idx_count[i]&&qadj7_idx_stamp[i]<oldst){oldst=qadj7_idx_stamp[i];old=i;}}if(slot==QAD_INDEX_SLOTS)slot=old;u32 count=rh.nodes-first;if(count>per)count=per;s_lseek(qadjfd,(long)(qh7_query.index_off+first*4u),SEEK_SET_);long n=s_read(qadjfd,qadj7_idx_w[slot],count*4u);if(n!=(long)(count*4u))return 0;RPROF_ADD(route_prof_qidx_bytes,n);qadj7_idx_first[slot]=first;qadj7_idx_count[slot]=count;qadj7_idx_stamp[slot]=++qadj7_idx_tick;*out=qadj7_idx_w[slot][node-first];return *out<qh7_query.data_bytes;}
static int qadj7_read_block_off(u32 off,u16*fc,u16*rc,struct QAdj7Entry**f,struct QAdj7Entry**r){if(qadjmode!=7||off>=qh7_query.data_bytes||!fc||!rc||!f||!r)return 0;u8*p=0;if(qadj_map_ok){u32 at=qh7_query.data_off+off;if(at>qadj_map_bytes-4u)return 0;p=qadj_map+at;RPROF_INC(route_prof_qmap_access);}else{u8 h[4];s_lseek(qadjfd,(long)(qh7_query.data_off+off),SEEK_SET_);long n=s_read(qadjfd,h,4);if(n!=4)return 0;*fc=rd16(h);*rc=rd16(h+2);u32 bytes=4u+((u32)*fc+(u32)*rc)*(u32)sizeof(struct QAdj7Entry);if(bytes>QAD_BLOCK_BYTES||bytes>qh7_query.data_bytes-off)return 0;s_lseek(qadjfd,(long)(qh7_query.data_off+off),SEEK_SET_);n=s_read(qadjfd,qadj7_buf_w,bytes);if(n!=(long)bytes)return 0;RPROF_INC(route_prof_qadj_reads);RPROF_ADD(route_prof_qadj_bytes,n);p=(u8*)qadj7_buf_w;}
 *fc=rd16(p);*rc=rd16(p+2);u32 bytes=4u+((u32)*fc+(u32)*rc)*(u32)sizeof(struct QAdj7Entry);if(bytes>QAD_BLOCK_BYTES||bytes>qh7_query.data_bytes-off)return 0;*f=(struct QAdj7Entry*)(p+4);*r=*f+*fc;return 1;}
static int qadj7_read_qrec(u32 id,struct QRec7*out){if(qadjmode!=7||!out||id>=rh.ch_arcs)return 0;u32 at=qh7.q.rmeta_off+id*(u32)sizeof(struct QRec7);if(qadj_map_ok){if(at>qadj_map_bytes-(u32)sizeof(struct QRec7))return 0;const u8*p=qadj_map+at;out->from=rd32u(p);out->cost=rd32u(p+4);out->left=rd32u(p+8);out->right_geom=rd32u(p+12);out->to_flags=rd32u(p+16);out->geom_first=rd32u(p+20);out->geom_info=rd32u(p+24);out->packet_off=rd32u(p+28);RPROF_INC(route_prof_qrec_hit);RPROF_INC(route_prof_qmap_access);return 1;}s_lseek(qadjfd,(long)at,SEEK_SET_);long n=s_read(qadjfd,out,sizeof(*out));if(n!=(long)sizeof(*out))return 0;RPROF_INC(route_prof_qrec_miss);RPROF_ADD(route_prof_qrec_bytes,n);return 1;}
static u32 qrec7_to(const struct QRec7*q){return q->to_flags&0x00ffffffu;}static u8 qrec7_flags(const struct QRec7*q){return(u8)(q->to_flags>>24);}static u16 qrec7_geom_count(const struct QRec7*q){return(u16)(q->geom_info&0xffffu);}static u8 qrec7_base_flags(const struct QRec7*q){return(u8)((q->geom_info>>16)&0xffu);}static u8 qrec7_base_cls(const struct QRec7*q){return(u8)(q->geom_info>>24);}
static int qadj7_packet_point(const struct QRec7*q,u32 raw,struct CprPoint*out,u32*qf,u32*qr,u16*mf,u16*mr){if(qadjmode!=7||!q||!out||!qf||!qr||!mf||!mr)return 0;u32 gc=qrec7_geom_count(q);if(raw>=gc||q->packet_off==0xffffffffu)return 0;u32 rel=q->packet_off+raw*QAD7_POINT_SIZE;if(rel>qh7.packet_bytes-QAD7_POINT_SIZE)return 0;u32 at=qh7.packet_off+rel;u8 tmp[QAD7_POINT_SIZE];const u8*p=0;if(qadj_map_ok){if(at>qadj_map_bytes-QAD7_POINT_SIZE)return 0;p=qadj_map+at;RPROF_INC(route_prof_qmap_access);}else{s_lseek(qadjfd,(long)at,SEEK_SET_);long n=s_read(qadjfd,tmp,QAD7_POINT_SIZE);if(n!=(long)QAD7_POINT_SIZE)return 0;p=tmp;RPROF_INC(route_prof_qadj_reads);RPROF_ADD(route_prof_qadj_bytes,QAD7_POINT_SIZE);}out->lat=(s32)rd32u(p);out->lon=(s32)rd32u(p+4);u16 fl=rd16(p+16);*qf=(fl&1u)?65536u:(u32)rd16(p+8);*qr=(fl&2u)?65536u:(u32)rd16(p+10);*mf=rd16(p+12);*mr=rd16(p+14);RPROF_INC(route_prof_packet_pts);return 1;}
static int route_read_srcref(u32 id,u32*out){return route_read_ref(3,id,out);}
/* Optional v5 flag2 extension; read lossless OSM way IDs without changing
 * BASE records or allocating an identity table in device RAM. */
static int route_read_way(u32 base,u32*lo,u32*hi){if(!routeok||!(rh.flags&2)||base>=rh.base_edges)return 0;u32 at=rh.geom_off+rh.geom_points*8u+base*8u;u8 b[8];if(at>0x7ffffff7u||s_lseek(routefd,(long)at,SEEK_SET_)<0||s_read(routefd,b,8)!=8)return 0;*lo=rd32u(b);*hi=rd32u(b+4);return 1;}
static int route_read_point(u32 id,struct CprPoint*out){if(!routeok||id>=rh.geom_points||!out)return 0;if(route_geom_map_ok){u32 at=route_geom_map_delta+id*(u32)sizeof(struct CprPoint);if(at>route_geom_map_bytes-(u32)sizeof(struct CprPoint))return 0;const u8*p=route_geom_map+at;out->lat=(s32)rd32u(p);out->lon=(s32)rd32u(p+4);RPROF_INC(route_prof_gmap_access);return 1;}u32 per=CPR_GEOM_PAGE/(u32)sizeof(struct CprPoint),slot=CPR_GEOM_SLOTS,old=0,oldst=0xffffffffu;for(u32 i=0;i<CPR_GEOM_SLOTS;i++){if(cpr_gcache_count[i]&&id>=cpr_gcache_first[i]&&id<cpr_gcache_first[i]+cpr_gcache_count[i]){RPROF_INC(route_prof_g_hit);cpr_gcache_stamp[i]=++cpr_gcache_tick;*out=((struct CprPoint*)cpr_gcache_w[i])[id-cpr_gcache_first[i]];return 1;}if(!cpr_gcache_count[i]&&slot==CPR_GEOM_SLOTS)slot=i;if(cpr_gcache_count[i]&&cpr_gcache_stamp[i]<oldst){oldst=cpr_gcache_stamp[i];old=i;}}if(slot==CPR_GEOM_SLOTS)slot=old;RPROF_INC(route_prof_g_miss);u32 first=(id/per)*per,count=rh.geom_points-first;if(count>per)count=per;s_lseek(routefd,(long)(rh.geom_off+first*sizeof(struct CprPoint)),SEEK_SET_);long n=s_read(routefd,cpr_gcache_w[slot],count*sizeof(struct CprPoint));if(n<=0)return 0;RPROF_ADD(route_prof_g_bytes,n);cpr_gcache_first[slot]=first;cpr_gcache_count[slot]=(u32)n/(u32)sizeof(struct CprPoint);cpr_gcache_stamp[slot]=++cpr_gcache_tick;*out=((struct CprPoint*)cpr_gcache_w[slot])[id-first];return 1;}
static int route_base_point(const struct CprBase*a,u32 pos,struct CprPoint*out){if(!a||!a->geom_count||pos>=a->geom_count)return 0;u32 raw=(a->flags&CPR_ARC_GEOM_REVERSED)?(a->geom_count-1u-pos):pos;return route_read_point(a->geom_first+raw,out);}
#define ROUTE_SNAP_D2 22500u
#define ROUTE_DEST_SNAP_D2 160000u
#define ROUTE_DEST_CAND_MAX 12
/* exact edge snapping.  CPR2 v5 still stores compact topology, but a
 * snap now retains the exact projected position inside a geometry segment.
 * frac_q16 is 0..65536 along geom_seg -> geom_seg+1 in logical arc order.
 * along_u/total_u are metric-proportional polyline units used for exact
 * partial-edge cost and for distributing ETA cost along compressed geometry. */
struct RouteSnapCandidate {u32 node,arc,d2,score,extra,geom_seg,frac_q16,along_u,total_u;s32 snap_lat,snap_lon;};
/* snapping is geometric first.  Road class must not pull a POI off
 * the physically nearest residential/service access road onto a farther
 * major road.  Access legality is already encoded by which base arcs exist;
 * connectivity is checked separately before a CH query begins. */
static u32 route_snap_class_penalty_base(const struct CprBase*a){(void)a;return 0u;}
static u32 route_ratio_q16(u32 num,u32 den){if(!den||!num)return 0;if(num>=den)return 65536u;while(den>65535u){den=(den+1u)>>1;num=(num+1u)>>1;if(num>=den)return 65536u;}return udiv32(num<<16,den);}
static u32 route_mul_q16(u32 v,u32 q){if(q>=65536u)return v;u32 hi=v>>16,lo=v&65535u;return hi*q+((lo*q+32768u)>>16);}
static s32 route_lerp_q16(s32 a,s32 b,u32 q){if(!q)return a;if(q>=65536u)return b;s32 d=b-a;u32 ad=(u32)(d<0?-d:d),hi=ad>>16,lo=ad&65535u;u32 z=hi*q+((lo*q+32768u)>>16);return d<0?a-(s32)z:a+(s32)z;}
static u32 route_geom_seg_units(const struct CprPoint*a,const struct CprPoint*b){int cq=coslat_q15((a->lat+b->lat)/2);u32 dx=(u32)iabs32(lon_scale(b->lon-a->lon,cq)),dy=(u32)iabs32(b->lat-a->lat);u32 hi=dx>dy?dx:dy,lo=dx>dy?dy:dx;return hi+((lo*3u)>>3);}
static u32 route_project_q16(s32 ax,s32 ay,s32 bx,s32 by,u32*outd){s32 vx=bx-ax,vy=by-ay;s32 dot=-(ax*vx+ay*vy);u32 len2=(u32)(vx*vx+vy*vy),q=0;if(!len2){if(outd)*outd=square_distance(ax,ay);return 0;}if(dot<=0)q=0;else if((u32)dot>=len2)q=65536u;else q=route_ratio_q16((u32)dot,len2);s32 qx=ax+route_lerp_q16(0,vx,q),qy=ay+route_lerp_q16(0,vy,q);if(outd)*outd=(u32)(qx*qx+qy*qy);return q;}
static int route_base_nearest(const struct CprBase*a,s32 lat,s32 lon,struct RouteSnapCandidate*out){if(!a||!out||a->geom_count<2)return 0;int cq=coslat_q15(lat);u32 best=0xffffffffu,bseg=0,bfrac=0,bprefix=0,bseg_u=0,total=0;struct CprPoint p0,p1,bp0,bp1;if(!route_base_point(a,0,&p0))return 0;for(u32 i=1;i<a->geom_count;i++){if(!route_base_point(a,i,&p1))return 0;s32 ax=lon_scale(p0.lon-lon,cq),ay=p0.lat-lat,bx=lon_scale(p1.lon-lon,cq),by=p1.lat-lat;u32 d=0,q=route_project_q16(ax,ay,bx,by,&d),su=route_geom_seg_units(&p0,&p1);if(d<best){best=d;bseg=i-1u;bfrac=q;bprefix=total;bseg_u=su;bp0=p0;bp1=p1;}total=total>0xffffffffu-su?0xffffffffu:total+su;p0=p1;}if(best==0xffffffffu)return 0;u32 within=route_mul_q16(bseg_u,bfrac),along=bprefix>0xffffffffu-within?0xffffffffu:bprefix+within;if(along>total)along=total;out->d2=best;out->geom_seg=bseg;out->frac_q16=bfrac;out->along_u=along;out->total_u=total;out->snap_lat=route_lerp_q16(bp0.lat,bp1.lat,bfrac);out->snap_lon=route_lerp_q16(bp0.lon,bp1.lon,bfrac);return 1;}
static void route_candidate_insert(struct RouteSnapCandidate*out,int cap,int*count,const struct RouteSnapCandidate*c){
    /* The same base arc can be referenced by several spatial tiles, so
     * deduplicate that exact arc.  Do NOT deduplicate by topology node: two
     * different roads can share a junction and are distinct snap choices. */
    int old=-1;for(int i=0;i<*count;i++)if(out[i].arc==c->arc){old=i;break;}
    if(old>=0){if(out[old].score<c->score||(out[old].score==c->score&&out[old].d2<=c->d2))return;for(int i=old;i+1<*count;i++)out[i]=out[i+1];(*count)--;}
    int pos=*count;for(int i=0;i<*count;i++)if(c->score<out[i].score||(c->score==out[i].score&&c->d2<out[i].d2)){pos=i;break;}
    if(pos>=cap)return;int n=*count;if(n<cap)n++;for(int i=n-1;i>pos;i--)out[i]=out[i-1];out[pos]=*c;*count=n;
}
static u32 route_cost_for_units(u32 cost,u32 units,u32 total){return route_mul_q16(cost,route_ratio_q16(units,total));}
static int route_snap_candidates(s32 lat,s32 lon,u32 maxd2,int tile_rad,int dest_mode,struct RouteSnapCandidate*out,int cap){
    if(!routeok||!out||cap<1||lat<rh.minlat-500||lat>rh.maxlat+500||lon<rh.minlon-500||lon>rh.maxlon+500)return 0;
    int tx=tile_div(lon,6),ty=tile_div(lat,6),count=0;u32 nearest_d=0xffffffffu,nearest_arc=0xffffffffu;int have=0;
    for(int y=ty-tile_rad;y<=ty+tile_rad;y++)for(int x=tx-tile_rad;x<=tx+tile_rad;x++){
        struct CprTile t;if(!route_find_tile(x,y,&t))continue;
        for(u32 j=0;j<t.count;j++){
            u32 aid;if(!route_read_ref(2,t.first+j,&aid)||aid>=rh.base_edges)continue;
            struct CprBase a;if(!route_read_base(aid,&a))continue;
            struct RouteSnapCandidate c;memzero(&c,sizeof(c));if(!route_base_nearest(&a,lat,lon,&c)||c.d2>=maxd2)continue;
            c.node=a.to_state;c.arc=aid;c.score=c.d2+route_snap_class_penalty_base(&a);
            u32 units=dest_mode?c.along_u:(c.total_u>c.along_u?c.total_u-c.along_u:0);c.extra=route_cost_for_units(a.cost,units,c.total_u);
            if(c.d2<nearest_d){nearest_d=c.d2;nearest_arc=aid;have=1;}route_candidate_insert(out,cap,&count,&c);
        }
    }
    if(!have)return 0;int at=-1;for(int i=0;i<count;i++)if(out[i].arc==nearest_arc&&out[i].d2==nearest_d){at=i;break;}if(at>0){struct RouteSnapCandidate c=out[at];for(int i=at;i>0;i--)out[i]=out[i-1];out[0]=c;}return count;
}
static int route_snap_node(s32 lat,s32 lon,struct RouteSnapCandidate*out){struct RouteSnapCandidate c[8];/* the live car should normally lie on a road indexed by its current 256-unit Z6 tile. Avoid a 3x3 scan unless that exact tile cannot produce a legal nearby road. */int n=route_snap_candidates(lat,lon,ROUTE_SNAP_D2,0,0,c,8);if(n<1)n=route_snap_candidates(lat,lon,ROUTE_SNAP_D2,1,0,c,8);if(n<1)return 0;if(out)*out=c[0];return 1;}

#define ROUTE_PATH_MAX 16384
#define ROUTE_INF 0xffffffffu
#define ROUTE_NOARC 0xffffffffu
#define ROUTE_MEM_RESERVE_KB 12288u
#define ROUTE_MEM_MAX_KB 32768u
#define ROUTE_MEM_FALLBACK_KB 8192u
#define ROUTE_MEM_ALLOC_STEP (1024u*1024u)
struct ChState {u32 node,df,db,pf,pb,hash_next,posf,posb,qoff;u8 sf,sb;u16 pad;};
static struct ChState*chst=0;static u32*chhf=0,*chhb=0,*chbucket=0;
static u32 chst_cap=0,chst_n=0,chhf_n=0,chhb_n=0,chheap_cap=0,chbucket_n=0,chbucket_mask=0,chheap_peak=0;
static void*route_arena=0;static u32 route_arena_bytes=0,route_workspace_bytes=0,route_mem_avail_kb=0;
#ifndef HOST_TEST
static long route_brk_base=0;
#endif
static s32 route_lat[ROUTE_PATH_MAX],route_lon[ROUTE_PATH_MAX];
/* route_seg_arc[i] identifies the CPR2 base road used by the segment
 * route[i-1] -> route[i].  This distinguishes a nearby parallel
 * road from the actual planned road instead of relying on proximity alone. */
static u32 route_node[ROUTE_PATH_MAX],route_seg_cost[ROUTE_PATH_MAX],route_seg_arc[ROUTE_PATH_MAX],route_seg_phys[ROUTE_PATH_MAX],route_seg_m[ROUTE_PATH_MAX];
static u32 route_emit_arc=ROUTE_NOARC,route_emit_phys=ROUTE_NOARC;
static u32 ch_arc_path[ROUTE_PATH_MAX],ch_arc_tmp[ROUTE_PATH_MAX],ch_unpack_stack[ROUTE_PATH_MAX];static int route_n=0,route_head=0;static u32 route_result_m=0;
enum {R_IDLE=0,R_CALC=1,R_READY=2,R_NOFILE=3,R_NOROAD=4,R_UNREACHABLE=5,R_MEMORY=6,R_ERROR=7};
#define ROUTE_CALC_STALE_D2 10000u
#define ROUTE_FINISH_STALE_D2 2500u
#define ROUTE_PROGRESS_D2 3600u
#define ROUTE_OFFROUTE_CHECKS 2
#define ROUTE_PROGRESS_LOOKAHEAD 256
#define ROUTE_DRAW_MAX_SEGMENTS 2048
#define ROUTE_ARC_CHECK_TICKS 3
#define ROUTE_ARC_PROGRESS_D2 900u
#define ROUTE_ADHERE_TIE_D2 64u /* ~8-9 m when nearest snap is on-road: tolerate junction/GPS ambiguity only */
static int route_status=R_IDLE;static u32 route_car_node=0,route_dest_comp=0,route_result_cost=0,route_expanded=0,route_best=ROUTE_INF,route_meet=ROUTE_INF;
static s32 route_query_lat=0,route_query_lon=0,route_target_lat=0,route_target_lon=0;static int route_tick=0,route_offroute_checks=0,route_rerouting=0,route_arc_check_tick=0;static u32 ui_anim_tick=0;
/* compact next-manoeuvre guidance is derived entirely from the
 * reconstructed route already held in RAM. Physical-polyline transitions mark
 * junction candidates; near-straight transitions are suppressed so CPR2 road
 * segmentation does not become visual noise. */
enum {MAN_NONE=0,MAN_SLIGHT_LEFT,MAN_LEFT,MAN_SHARP_LEFT,MAN_SLIGHT_RIGHT,MAN_RIGHT,MAN_SHARP_RIGHT,MAN_UTURN,MAN_ROUNDABOUT};
#define ROUTE_MAN_SCAN_POINTS 4096
#define ROUTE_MAN_RESCAN_POINTS 128
#define ROUTE_MAN_SHOW_M 3000u
static int nav_man_type=MAN_NONE,nav_man_idx=-1,nav_man_scan_head=0,nav_man_exit=0;static u32 nav_man_m=0;
static void nav_maneuver_find(void);
static void nav_maneuver_reset(void){nav_man_exit=0;nav_man_type=MAN_NONE;nav_man_idx=-1;nav_man_m=0;nav_man_scan_head=route_head;}
static struct RouteSnapCandidate route_start_snap,route_dest_cand[ROUTE_DEST_CAND_MAX];static int route_dest_count=0,route_dest_idx=0;

/* local CH repair.
 *
 * A deviation does not immediately throw away a long route.  Future turn-aware
 * states on the existing route are treated as a virtual destination: each is
 * backward-seeded with its already-known suffix cost (normalised by the cheapest
 * suffix so CH's two queues stay well-scaled).  The winning query therefore
 * minimises:
 *
 *      cost(car -> rejoin) + old_route_cost(rejoin -> real destination)
 *
 * across all sampled future rejoin states in one CH search.  The chosen old
 * suffix is then spliced behind the newly reconstructed repair prefix.  No
 * route.cpr format change is required.
 *
 * Multiple plausible directed start arcs are forward-seeded.  A start arc whose
 * local direction is effectively a U-turn against the live GPS heading gets a
 * soft choice penalty; the penalty influences which route wins but is removed
 * from the displayed ETA.  If local repair is unavailable, exceeds the bounded
 * repair budget, or encounters an error, the existing full CH reroute remains
 * the authoritative fallback. */
#define ROUTE_REPAIR_CAND_MAX 64
#define ROUTE_REPAIR_START_MAX 8
#define ROUTE_REPAIR_WINDOW_DS 18000u       /* 30 min of the old route ahead */
#define ROUTE_REPAIR_MIN_AHEAD_DS 100u      /* do not rejoin the exact miss point */
#define ROUTE_REPAIR_DENSE_UNTIL_DS 3000u   /* dense candidates for first 5 min */
#define ROUTE_REPAIR_DENSE_STEP_DS 200u     /* ~20 s spacing */
#define ROUTE_REPAIR_SPARSE_STEP_DS 600u    /* ~1 min spacing thereafter */
#define ROUTE_REPAIR_MAX_PREFIX_DS 7200u    /* >12 min local detour => full CH */
#define ROUTE_REPAIR_UTURN_PENALTY_DS 1800u /* soft 3 min immediate reversal cost */
#define ROUTE_REPAIR_START_TIE_D2 256u      /* only plausible nearby directed arcs */
/* repair-splice guard.  Fast local repair is allowed to reuse the old
 * route only when the newly reconstructed prefix lands on the exact saved
 * turn-aware state/coordinate.  Around that splice we can safely erase a
 * positive-cost cycle only if it returns to the identical routing state.  A
 * short return to the same physical junction under a different turn-context
 * state is deliberately not guessed away: reject the local splice and let the
 * existing full CH fallback decide the globally legal path. */
#define ROUTE_REPAIR_SPLICE_SCAN_BACK 32
#define ROUTE_REPAIR_SPLICE_SCAN_AHEAD 512
#define ROUTE_REPAIR_SPLICE_EARLY_A 64
#define ROUTE_REPAIR_SPLICE_LOOP_MAX_DS 3000u /* only police short <=5 min join-local loops */

static int route_repair_active=0,route_repair_cand_n=0,route_repair_start_n=0;
static int route_repair_old_n=0,route_repair_old_head=0,route_repair_join_old_idx=-1;
static int route_repair_chosen_start=-1,route_repair_suffix_n=0,route_repair_suffix_copy=0;
static int route_repair_prefix_anchor=-1,route_repair_splice_reject=0;
static u32 route_repair_old_cost=0,route_repair_suffix_base=0,route_repair_chosen_suffix=0;
static u32 route_repair_start_penalty=0,route_repair_prefix_cost=0,route_repair_begin_ms=0;
static u32 route_repair_trim_points=0,route_repair_trim_cost=0;
static u32 route_repair_cand_node[ROUTE_REPAIR_CAND_MAX],route_repair_cand_suffix[ROUTE_REPAIR_CAND_MAX];
static int route_repair_cand_idx[ROUTE_REPAIR_CAND_MAX];
static struct RouteSnapCandidate route_repair_start[ROUTE_REPAIR_START_MAX];
static u32 route_repair_start_seed[ROUTE_REPAIR_START_MAX],route_repair_start_pen[ROUTE_REPAIR_START_MAX];

/* Only the winning suffix is copied, immediately before route reconstruction.
 * Static storage costs <400 KiB and avoids keeping a second 32 MiB CH arena or
 * relying on overwritten route[] memory while the prefix is being emitted. */
static s32 route_repair_lat[ROUTE_PATH_MAX],route_repair_lon[ROUTE_PATH_MAX];
static u32 route_repair_node[ROUTE_PATH_MAX],route_repair_seg_cost[ROUTE_PATH_MAX];
static u32 route_repair_seg_arc[ROUTE_PATH_MAX],route_repair_seg_phys[ROUTE_PATH_MAX],route_repair_seg_m[ROUTE_PATH_MAX];

static int nav_heading10(s32 la0,s32 lo0,s32 la1,s32 lo1);
static void route_repair_reset(void){
  route_repair_active=0;route_repair_cand_n=route_repair_start_n=0;
  route_repair_old_n=route_repair_old_head=0;route_repair_join_old_idx=-1;
  route_repair_chosen_start=-1;route_repair_suffix_n=route_repair_suffix_copy=0;
  route_repair_prefix_anchor=-1;route_repair_splice_reject=0;
  route_repair_old_cost=route_repair_suffix_base=route_repair_chosen_suffix=0;
  route_repair_start_penalty=route_repair_prefix_cost=route_repair_begin_ms=0;
  route_repair_trim_points=route_repair_trim_cost=0;
}

/* Cooperative route finalisation state.  Search, CH parent tracing, shortcut
 * unpacking and base-geometry reconstruction all advance in small steps so
 * none of them owns the UI thread for an entire long-route build. */
enum {RB_IDLE=0,RB_TRACE_F,RB_COPY_F,RB_TRACE_B,RB_START_INIT,RB_START_POINTS,RB_UNPACK,RB_BASE_NEXT,RB_BASE_TOTAL,RB_BASE_OUTPUT,RB_DEST_INIT,RB_DEST_POINTS,RB_REPAIR_SUFFIX,RB_DONE};
static int route_build_phase=RB_IDLE,route_build_nf=0,route_build_nb=0,route_build_copy=-1,route_build_arc_idx=0,route_build_sp=0,route_build_after_base=RB_UNPACK;
static u32 route_build_u=0,route_build_i=0,route_build_total=0,route_build_cum=0,route_build_prev_cost=0;
static struct QRec7 route_base_meta[ROUTE_PATH_MAX];static int route_base_meta_n=0,route_base_meta_idx=0;
static struct CprArc route_build_arc;static struct CprBase route_build_base;static struct CprPoint route_build_prev_point;
#define ROUTE_GEOM_TMP_MAX 256u
static struct CprPoint route_geom_tmp[ROUTE_GEOM_TMP_MAX];static u32 route_geom_cum_tmp[ROUTE_GEOM_TMP_MAX];static int route_build_geom_cached=0;
/* Routing now consumes only spare deadline budget in <=5 ms chunks. */
#define ROUTE_CLOCK_CHECK_MASK 31u
static u32 route_sched_slices=0,route_sched_yields=0,route_build_steps=0;
static int time_before_u32(u32 a,u32 b){return (s32)(a-b)<0;}
static u32 sat_add32(u32 a,u32 b){return a>ROUTE_INF-b?ROUTE_INF:a+b;}
static u32 next_pow2(u32 v){u32 p=1;if(v<=1)return 1;while(p<v&&p<0x80000000u)p<<=1;return p;}
static u32 meminfo_value_kb(const char*b,const char*key){const char*p=b;unsigned kl=slen(key);while(*p){if(starts(p,key)){p+=kl;while(*p==' '||*p=='\t'||*p==':')p++;return(u32)(atoi0(p)<0?0:atoi0(p));}while(*p&&*p!='\n')p++;if(*p)p++;}return 0;}
static u32 route_memory_budget(void){char b[4096];long fd=s_open("/proc/meminfo",O_RDONLY_,0);u32 total=0,avail=0,fr=0,buf=0,cached=0;if(fd>=0){long n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);if(n>0){b[n]=0;total=meminfo_value_kb(b,"MemTotal");avail=meminfo_value_kb(b,"MemAvailable");fr=meminfo_value_kb(b,"MemFree");buf=meminfo_value_kb(b,"Buffers");cached=meminfo_value_kb(b,"Cached");}}if(!avail)avail=fr+buf+((cached>>2)*3u);route_mem_avail_kb=avail;u32 kb=avail>ROUTE_MEM_RESERVE_KB+2048u?avail-ROUTE_MEM_RESERVE_KB:(avail?avail>>1:ROUTE_MEM_FALLBACK_KB);if(total>16384u&&kb>total-16384u)kb=total-16384u;if(kb>ROUTE_MEM_MAX_KB)kb=ROUTE_MEM_MAX_KB;if(kb<1024u)kb=1024u;return kb*1024u;}
static void route_arena_release(void){if(!route_arena)return;
#ifdef HOST_TEST
 free(route_arena);
#else
 if(route_brk_base>0)s_brk(route_brk_base);route_brk_base=0;
#endif
 route_arena=0;route_arena_bytes=route_workspace_bytes=0;chst=0;chhf=chhb=chbucket=0;chst_cap=chst_n=chhf_n=chhb_n=chheap_cap=chbucket_n=chbucket_mask=0;}
static int route_arena_alloc(u32 bytes){bytes=(bytes+4095u)&~4095u;
#ifdef HOST_TEST
 route_arena=malloc(bytes);if(!route_arena)return 0;
#else
 long base=s_brk(0);if(base<=0)return 0;long want=base+(long)bytes;long got=s_brk(want);if(got<want){s_brk(base);return 0;}route_brk_base=base;route_arena=(void*)base;
#endif
 route_arena_bytes=bytes;return 1;}
static int route_partition_arena(void){if(!route_arena||route_arena_bytes<512u*1024u)return 0;u32 mincap=rh.nodes<1024u?rh.nodes:1024u,guess=route_arena_bytes/52u;if(guess>rh.nodes)guess=rh.nodes;if(guess<mincap)guess=mincap;u32 buckets=next_pow2((guess+1u)>>1);if(buckets<1024u)buckets=1024u;u32 cap=0;for(int it=0;it<4;it++){u32 bb=buckets*4u;if(bb+4096u>=route_arena_bytes)return 0;cap=(route_arena_bytes-bb-8u)/(u32)(sizeof(struct ChState)+8u);if(cap>rh.nodes)cap=rh.nodes;if(cap<mincap)return 0;u32 want=next_pow2((cap+1u)>>1);if(want<1024u)want=1024u;if(want==buckets)break;buckets=want;}u32 bb=buckets*4u;cap=(route_arena_bytes-bb-8u)/(u32)(sizeof(struct ChState)+8u);if(cap>rh.nodes)cap=rh.nodes;if(cap<mincap)return 0;u8*p=(u8*)route_arena;chbucket=(u32*)p;p+=bb;chst=(struct ChState*)p;p+=cap*(u32)sizeof(struct ChState);chhf=(u32*)p;p+=(cap+1u)*4u;chhb=(u32*)p;p+=(cap+1u)*4u;chst_cap=chheap_cap=cap;chbucket_n=buckets;chbucket_mask=buckets-1u;route_workspace_bytes=(u32)(p-(u8*)route_arena);return route_workspace_bytes<=route_arena_bytes;}
static int route_arena_prepare(void){route_arena_release();u32 want=route_memory_budget();while(want>=1024u*1024u){if(route_arena_alloc(want)){if(route_partition_arena())return 1;route_arena_release();}if(want<=ROUTE_MEM_ALLOC_STEP)break;want-=ROUTE_MEM_ALLOC_STEP;}return 0;}
static u32 ch_hash(u32 n){return(n*2654435761u)&chbucket_mask;}
static struct ChState*ch_state(u32 node,int create){if(!chst||!chbucket_n)return 0;u32 b=ch_hash(node),x=chbucket[b];while(x){struct ChState*s=&chst[x-1u];if(s->node==node)return s;x=s->hash_next;}if(!create||chst_n>=chst_cap)return 0;struct ChState*s=&chst[chst_n];s->node=node;s->df=s->db=ROUTE_INF;s->pf=s->pb=ROUTE_NOARC;s->posf=s->posb=0;s->qoff=ROUTE_NOARC;s->sf=s->sb=0;s->hash_next=chbucket[b];chbucket[b]=++chst_n;return s;}
static u32 ch_slot(struct ChState*s){return(u32)(s-chst);}static u32 ch_dist(struct ChState*s,int dir){return dir?s->db:s->df;}static u32*ch_posp(struct ChState*s,int dir){return dir?&s->posb:&s->posf;}static u32*ch_heap(int dir){return dir?chhb:chhf;}static u32*ch_heapnp(int dir){return dir?&chhb_n:&chhf_n;}
static int ch_heap_less(int dir,u32 a,u32 b){return ch_dist(&chst[a],dir)<ch_dist(&chst[b],dir);}static void ch_heap_swap(int dir,u32 a,u32 b){u32*h=ch_heap(dir);u32 x=h[a];h[a]=h[b];h[b]=x;*ch_posp(&chst[h[a]],dir)=a;*ch_posp(&chst[h[b]],dir)=b;}static void ch_heap_up(int dir,u32 p){u32*h=ch_heap(dir);while(p>1){u32 q=p>>1;if(!ch_heap_less(dir,h[p],h[q]))break;ch_heap_swap(dir,p,q);p=q;}}static void ch_heap_down(int dir,u32 p){u32*h=ch_heap(dir),n=*ch_heapnp(dir);for(;;){u32 l=p<<1,r=l+1,b=p;if(l<=n&&ch_heap_less(dir,h[l],h[b]))b=l;if(r<=n&&ch_heap_less(dir,h[r],h[b]))b=r;if(b==p)break;ch_heap_swap(dir,p,b);p=b;}}
static int ch_heap_insert(int dir,struct ChState*s){u32*pos=ch_posp(s,dir),*h=ch_heap(dir),*np=ch_heapnp(dir);if(*pos){ch_heap_up(dir,*pos);return 1;}if(*np>=chheap_cap)return 0;h[++(*np)]=ch_slot(s);*pos=*np;if(*np>chheap_peak)chheap_peak=*np;ch_heap_up(dir,*np);return 1;}static struct ChState*ch_heap_pop(int dir){u32*h=ch_heap(dir),*np=ch_heapnp(dir);if(!*np)return 0;struct ChState*s=&chst[h[1]];*ch_posp(s,dir)=0;if(*np>1){h[1]=h[*np];*ch_posp(&chst[h[1]],dir)=1;}(*np)--;if(*np)ch_heap_down(dir,1);return s;}static u32 ch_heap_min(int dir){u32 n=dir?chhb_n:chhf_n;if(!n)return ROUTE_INF;return ch_dist(&chst[ch_heap(dir)[1]],dir);}

static void ch_reset(void);
static void ch_update_meet(struct ChState*s);
static void route_mark_query_start(void);
static u32 geo_seg_m(s32 la0,s32 lo0,s32 la1,s32 lo1);
static int route_append_coord_cost(s32 lat,s32 lon,u32 node,u32 cost);
static int route_append_coord_cost_m(s32 lat,s32 lon,u32 node,u32 cost,u32 segm);
static int route_start(s32 dest_lat,s32 dest_lon);
static int route_full_reroute(const char*why);

static int route_repair_candidate_for_node(u32 node){
  int best=-1;u32 bc=ROUTE_INF;
  for(int i=0;i<route_repair_cand_n;i++)if(route_repair_cand_node[i]==node&&route_repair_cand_suffix[i]<bc){bc=route_repair_cand_suffix[i];best=i;}
  return best;
}
static int route_repair_start_for_node(u32 node){
  int best=-1;u32 bc=ROUTE_INF;
  for(int i=0;i<route_repair_start_n;i++)if(route_repair_start[i].node==node&&route_repair_start_seed[i]<bc){bc=route_repair_start_seed[i];best=i;}
  return best;
}
static void route_repair_add_candidate(int idx,u32 suffix){
  if(idx<=route_repair_old_head||idx>=route_repair_old_n||route_node[idx]==ROUTE_NOARC)return;
  u32 node=route_node[idx];
  for(int i=0;i<route_repair_cand_n;i++)if(route_repair_cand_node[i]==node){
    if(suffix<route_repair_cand_suffix[i]){route_repair_cand_suffix[i]=suffix;route_repair_cand_idx[i]=idx;}
    return;
  }
  if(route_repair_cand_n>=ROUTE_REPAIR_CAND_MAX)return;
  int k=route_repair_cand_n++;route_repair_cand_node[k]=node;route_repair_cand_suffix[k]=suffix;route_repair_cand_idx[k]=idx;
}
static int route_repair_collect_candidates(void){
  route_repair_old_n=route_n;route_repair_old_head=route_head;route_repair_old_cost=route_result_cost;route_repair_cand_n=0;
  if(route_repair_old_n-route_repair_old_head<3||!route_repair_old_cost)return 0;
  u32 spent=0,next=ROUTE_REPAIR_MIN_AHEAD_DS;int last=-1;
  for(int i=route_repair_old_head+1;i<route_repair_old_n;i++){
    spent=sat_add32(spent,route_seg_cost[i]);if(spent>ROUTE_REPAIR_WINDOW_DS)break;
    if(route_node[i]==ROUTE_NOARC)continue;last=i;
    if(spent<next)continue;
    u32 suffix=route_repair_old_cost>spent?route_repair_old_cost-spent:0;route_repair_add_candidate(i,suffix);
    next=spent+(spent<ROUTE_REPAIR_DENSE_UNTIL_DS?ROUTE_REPAIR_DENSE_STEP_DS:ROUTE_REPAIR_SPARSE_STEP_DS);
  }
  if(last>route_repair_old_head){
    u32 spent2=0;for(int i=route_repair_old_head+1;i<=last;i++)spent2=sat_add32(spent2,route_seg_cost[i]);
    route_repair_add_candidate(last,route_repair_old_cost>spent2?route_repair_old_cost-spent2:0);
  }
  if(!route_repair_cand_n)return 0;route_repair_suffix_base=ROUTE_INF;
  for(int i=0;i<route_repair_cand_n;i++)if(route_repair_cand_suffix[i]<route_repair_suffix_base)route_repair_suffix_base=route_repair_cand_suffix[i];
  return route_repair_suffix_base!=ROUTE_INF;
}
static u32 route_repair_heading_penalty(const struct RouteSnapCandidate*c){
  if(!c||!gps.fix||gps.mph10<50)return 0;struct CprBase b;struct CprPoint p0,p1;
  if(!route_read_base(c->arc,&b)||c->geom_seg+1u>=b.geom_count||!route_base_point(&b,c->geom_seg,&p0)||!route_base_point(&b,c->geom_seg+1u,&p1))return 0;
  int h=nav_heading10(p0.lat,p0.lon,p1.lat,p1.lon),course=shown_course();if(h<0)return 0;int d=h-course;while(d>180)d-=360;while(d<-180)d+=360;if(d<0)d=-d;
  return d>=145?ROUTE_REPAIR_UTURN_PENALTY_DS:0;
}
static int route_repair_collect_starts(u32 comp){
  struct RouteSnapCandidate c[ROUTE_REPAIR_START_MAX];int n=route_snap_candidates(car_lat(),car_lon(),ROUTE_SNAP_D2,1,0,c,ROUTE_REPAIR_START_MAX);route_repair_start_n=0;if(n<1)return 0;u32 nearest=c[0].d2;
  for(int i=0;i<n&&route_repair_start_n<ROUTE_REPAIR_START_MAX;i++){
    if(c[i].d2>nearest+ROUTE_REPAIR_START_TIE_D2)continue;struct CprNode rn;if(!route_read_node(c[i].node,&rn)||rn.comp!=comp)continue;
    u32 pen=route_repair_heading_penalty(&c[i]),seed=sat_add32(c[i].extra,pen);int k=route_repair_start_n++;
    route_repair_start[k]=c[i];route_repair_start_pen[k]=pen;route_repair_start_seed[k]=seed;
  }
  return route_repair_start_n>0;
}
static int route_repair_seed_query(void){
  int seeded_f=0,seeded_b=0;
  for(int i=0;i<route_repair_start_n;i++){struct ChState*x=ch_state(route_repair_start[i].node,1);if(!x)return 0;if(route_repair_start_seed[i]<x->df){x->df=route_repair_start_seed[i];x->pf=ROUTE_NOARC;if(!ch_heap_insert(0,x))return 0;}seeded_f=1;ch_update_meet(x);}
  for(int i=0;i<route_repair_cand_n;i++){struct ChState*x=ch_state(route_repair_cand_node[i],1);if(!x)return 0;u32 d=route_repair_cand_suffix[i]-route_repair_suffix_base;if(d<x->db){x->db=d;x->pb=ROUTE_NOARC;if(!ch_heap_insert(1,x))return 0;}seeded_b=1;ch_update_meet(x);}
  route_mark_query_start();return seeded_f&&seeded_b;
}
static void route_repair_log_start(void){
  if(!pref.debug)return;char b[192];b[0]=0;appendstr(b,sizeof(b),"REPAIR START CAND=");append_int(b,sizeof(b),route_repair_cand_n);appendstr(b,sizeof(b)," STARTS=");append_int(b,sizeof(b),route_repair_start_n);appendstr(b,sizeof(b)," WINDOWMIN=");append_int(b,sizeof(b),(int)udiv32(ROUTE_REPAIR_WINDOW_DS,600u));appendstr(b,sizeof(b)," OLDREM=");append_int(b,sizeof(b),(int)udiv32(route_repair_old_cost,10u));appendstr(b,sizeof(b),"s");debug_log(STATE_LOG,b);
}
static void route_repair_log_fallback(const char*why){
  if(!pref.debug)return;char b[192];b[0]=0;appendstr(b,sizeof(b),"REPAIR FALLBACK ");appendstr(b,sizeof(b),why);appendstr(b,sizeof(b)," EXP=");append_int(b,sizeof(b),(int)route_expanded);appendstr(b,sizeof(b)," WALLMS=");append_int(b,sizeof(b),(int)(s_now_ms()-route_repair_begin_ms));debug_log(STATE_LOG,b);
}
static void route_repair_log_ready(void){
  if(!pref.debug)return;char b[256];u32 ahead=route_repair_old_cost>route_repair_chosen_suffix?route_repair_old_cost-route_repair_chosen_suffix:0;b[0]=0;appendstr(b,sizeof(b),"REPAIR READY JOINAHEAD=");append_int(b,sizeof(b),(int)udiv32(ahead,10u));appendstr(b,sizeof(b),"s PREFIX=");append_int(b,sizeof(b),(int)udiv32(route_repair_prefix_cost,10u));appendstr(b,sizeof(b),"s PEN=");append_int(b,sizeof(b),(int)udiv32(route_repair_start_penalty,10u));appendstr(b,sizeof(b),"s SUFFIXPTS=");append_int(b,sizeof(b),route_repair_suffix_n);appendstr(b,sizeof(b)," TRIMPTS=");append_int(b,sizeof(b),(int)route_repair_trim_points);appendstr(b,sizeof(b)," TRIM=");append_int(b,sizeof(b),(int)udiv32(route_repair_trim_cost,10u));appendstr(b,sizeof(b),"s WALLMS=");append_int(b,sizeof(b),(int)(s_now_ms()-route_repair_begin_ms));debug_log(STATE_LOG,b);
}
static void route_repair_log_splice(const char*kind,int a,int bidx,u32 cost){
  if(!pref.debug)return;char b[224];b[0]=0;appendstr(b,sizeof(b),"REPAIR SPLICE ");appendstr(b,sizeof(b),kind);appendstr(b,sizeof(b)," A=");append_int(b,sizeof(b),a);appendstr(b,sizeof(b)," B=");append_int(b,sizeof(b),bidx);appendstr(b,sizeof(b)," COST=");append_int(b,sizeof(b),(int)udiv32(cost,10u));appendstr(b,sizeof(b),"s");if(a>=0&&a<route_n&&bidx>=0&&bidx<route_n){appendstr(b,sizeof(b)," NA=");append_int(b,sizeof(b),(int)route_node[a]);appendstr(b,sizeof(b)," NB=");append_int(b,sizeof(b),(int)route_node[bidx]);}debug_log(STATE_LOG,b);
}
/* The repair prefix must terminate at exactly the old-route state selected as
 * the virtual destination.  Anything else means the suffix cannot be spliced
 * without inventing geometry, so fail closed into the normal full-CH reroute. */
static int route_repair_validate_anchor(void){
  if(route_n<1||route_repair_suffix_n<1)return 0;int p=route_n-1;
  if(route_repair_node[0]==ROUTE_NOARC||route_node[p]!=route_repair_node[0]||route_lat[p]!=route_repair_lat[0]||route_lon[p]!=route_repair_lon[0]){route_repair_splice_reject=1;route_repair_log_splice("ANCHOR-REJECT",p,0,0);return 0;}
  route_repair_prefix_anchor=p;return 1;
}
static void route_repair_remove_cycle(int a,int bidx,u32 cost){
  int cut=bidx-a;if(cut<=0)return;u32 cutm=0;for(int i=a+1;i<=bidx;i++)cutm=sat_add32(cutm,route_seg_m[i]);for(int d=a+1,sr=bidx+1;sr<route_n;d++,sr++){route_lat[d]=route_lat[sr];route_lon[d]=route_lon[sr];route_node[d]=route_node[sr];route_seg_cost[d]=route_seg_cost[sr];route_seg_arc[d]=route_seg_arc[sr];route_seg_phys[d]=route_seg_phys[sr];route_seg_m[d]=route_seg_m[sr];}route_n-=cut;route_result_m=route_result_m>cutm?route_result_m-cutm:0;route_repair_trim_points=sat_add32(route_repair_trim_points,(u32)cut);route_repair_trim_cost=sat_add32(route_repair_trim_cost,cost);if(route_repair_prefix_anchor>bidx)route_repair_prefix_anchor-=cut;else if(route_repair_prefix_anchor>a)route_repair_prefix_anchor=a;
}
/* Normalise only the repair join neighbourhood.  Returning to the identical
 * routing state is a provably dominated positive-cost cycle and can be erased
 * with no legality trade-off.  Returning to the same coordinates with a
 * different turn-aware state may encode a real restriction; never guess: make
 * this repair fail and let a fresh global CH query arbitrate it. */
static int route_repair_sanitize_splice(void){
  if(route_repair_prefix_anchor<0||route_repair_prefix_anchor>=route_n)return 0;
restart:
  {int anchor=route_repair_prefix_anchor,lo=anchor-ROUTE_REPAIR_SPLICE_SCAN_BACK;if(lo<0)lo=0;int amax=anchor+ROUTE_REPAIR_SPLICE_EARLY_A;if(amax>route_n-2)amax=route_n-2;int hi=anchor+ROUTE_REPAIR_SPLICE_SCAN_AHEAD;if(hi>route_n-1)hi=route_n-1;
   for(int a=lo;a<=amax;a++){if(route_node[a]==ROUTE_NOARC)continue;u32 cost=0;for(int b=a+1;b<=hi;b++){cost=sat_add32(cost,route_seg_cost[b]);if(cost>ROUTE_REPAIR_SPLICE_LOOP_MAX_DS)break;if(route_node[b]==ROUTE_NOARC)continue;if(route_node[a]==route_node[b]){route_repair_log_splice("TRIM-CYCLE",a,b,cost);route_repair_remove_cycle(a,b,cost);goto restart;}if(route_lat[a]==route_lat[b]&&route_lon[a]==route_lon[b]){route_repair_splice_reject=1;route_repair_log_splice("CONTEXT-REJECT",a,b,cost);return 0;}}}}
  if(route_repair_trim_cost){route_result_cost=route_result_cost>route_repair_trim_cost?route_result_cost-route_repair_trim_cost:0;}return 1;
}
static int route_repair_trace_roots(int*start_idx,int*cand_idx){
  u32 u=route_meet;int guard=0,si=-1,ci=-1;
  for(;;){if(++guard>ROUTE_PATH_MAX)return 0;struct ChState*x=ch_state(u,0);if(!x)return 0;if(x->pf==ROUTE_NOARC){si=route_repair_start_for_node(u);break;}if(qadjmode==7){struct QRec7 q;if(!qadj7_read_qrec(x->pf,&q))return 0;u=q.from;}else{struct CprArc a;if(!route_read_arc(x->pf,&a))return 0;u=a.from;}}
  u=route_meet;guard=0;
  for(;;){if(++guard>ROUTE_PATH_MAX)return 0;struct ChState*x=ch_state(u,0);if(!x)return 0;if(x->pb==ROUTE_NOARC){ci=route_repair_candidate_for_node(u);break;}if(qadjmode==7){struct QRec7 q;if(!qadj7_read_qrec(x->pb,&q))return 0;u=qrec7_to(&q);}else{struct CprArc a;if(!route_read_arc(x->pb,&a))return 0;u=a.to;}}
  if(si<0||ci<0)return 0;*start_idx=si;*cand_idx=ci;return 1;
}
static int route_repair_prepare_finish(void){
  int si=-1,ci=-1;if(!route_repair_trace_roots(&si,&ci))return 0;route_repair_chosen_start=si;route_repair_start_penalty=route_repair_start_pen[si];
  route_repair_join_old_idx=route_repair_cand_idx[ci];route_repair_chosen_suffix=route_repair_cand_suffix[ci];
  u32 total=sat_add32(route_best,route_repair_suffix_base);if(total<route_repair_start_penalty)return 0;total-=route_repair_start_penalty;
  route_repair_prefix_cost=total>route_repair_chosen_suffix?total-route_repair_chosen_suffix:0;if(route_repair_prefix_cost>ROUTE_REPAIR_MAX_PREFIX_DS)return 0;
  if(route_repair_join_old_idx<0||route_repair_join_old_idx>=route_repair_old_n)return 0;route_repair_suffix_n=route_repair_old_n-route_repair_join_old_idx;if(route_repair_suffix_n<1||route_repair_suffix_n>ROUTE_PATH_MAX)return 0;
  for(int k=0;k<route_repair_suffix_n;k++){int i=route_repair_join_old_idx+k;route_repair_lat[k]=route_lat[i];route_repair_lon[k]=route_lon[i];route_repair_node[k]=route_node[i];route_repair_seg_cost[k]=route_seg_cost[i];route_repair_seg_arc[k]=route_seg_arc[i];route_repair_seg_phys[k]=route_seg_phys[i];route_repair_seg_m[k]=route_seg_m[i];}
  route_repair_suffix_copy=1;return 1;
}
static int route_repair_suffix_step(void){
  if(route_repair_suffix_copy>=route_repair_suffix_n){u32 total=sat_add32(route_best,route_repair_suffix_base);route_result_cost=total>=route_repair_start_penalty?total-route_repair_start_penalty:total;if(!route_repair_sanitize_splice())return 0;route_build_phase=RB_DONE;return route_n>0?2:0;}
  int k=route_repair_suffix_copy++;route_emit_arc=route_repair_seg_arc[k];route_emit_phys=route_repair_seg_phys[k];
  return route_append_coord_cost_m(route_repair_lat[k],route_repair_lon[k],route_repair_node[k],route_repair_seg_cost[k],route_repair_seg_m[k])?1:0;
}
static int route_repair_reseed_starts(void){
  if(!route_repair_active||route_build_phase!=RB_IDLE||!route_repair_collect_starts(route_dest_comp))return 0;
  chhf_n=0;route_best=route_meet=ROUTE_INF;route_n=0;route_head=0;route_result_cost=0;route_result_m=0;nav_maneuver_reset();route_expanded=0;route_tick=0;chheap_peak=chhb_n;
  route_emit_arc=route_emit_phys=ROUTE_NOARC;route_build_phase=RB_IDLE;route_build_nf=route_build_nb=route_build_arc_idx=route_build_sp=0;route_build_copy=-1;route_build_after_base=RB_UNPACK;route_base_meta_n=route_base_meta_idx=0;route_build_u=route_build_i=route_build_total=route_build_cum=route_build_prev_cost=0;route_build_steps=0;
  for(u32 i=0;i<chst_n;i++){chst[i].df=ROUTE_INF;chst[i].pf=ROUTE_NOARC;chst[i].posf=0;chst[i].sf=0;}
  int ok=0;for(int i=0;i<route_repair_start_n;i++){struct ChState*x=ch_state(route_repair_start[i].node,1);if(!x)return 0;if(route_repair_start_seed[i]<x->df){x->df=route_repair_start_seed[i];x->pf=ROUTE_NOARC;if(!ch_heap_insert(0,x))return 0;}ok=1;ch_update_meet(x);}
  if(ok){route_start_snap=route_repair_start[0];route_mark_query_start();if(pref.debug)debug_log(STATE_LOG,"REPAIR MOVING-START RESEED");}return ok;
}
static int route_repair_begin(const struct RouteSnapCandidate*nearest){
  if(!nearest||route_status!=R_READY||route_n-route_head<3)return 0;route_repair_reset();route_prof_reset();route_repair_begin_ms=s_now_ms();
  if(!route_repair_collect_candidates())return 0;if(!route_arena_prepare())return 0;if(!route_repair_collect_starts(route_dest_comp)){route_arena_release();return 0;}
  route_repair_active=1;if(route_prof_on&&!route_prof_query_ms){route_prof_query_ms=s_now_ms();route_prof_pre_io=route_prof_io_bytes();}
  ch_reset();if(!route_repair_seed_query()){route_repair_active=0;route_arena_release();return 0;}
  route_start_snap=*nearest;route_car_node=nearest->node;route_offroute_checks=0;route_arc_check_tick=0;route_rerouting=1;route_status=R_CALC;route_repair_log_start();return 1;
}

static void traffic_invalidate(void);
static void ch_reset(void){traffic_invalidate();if(chbucket&&chbucket_n)memzero(chbucket,chbucket_n*4u);chst_n=chhf_n=chhb_n=0;chheap_peak=0;route_expanded=0;route_best=route_meet=ROUTE_INF;route_n=0;route_head=0;route_result_cost=0;route_result_m=0;route_tick=0;route_arc_check_tick=0;route_offroute_checks=0;route_emit_arc=route_emit_phys=ROUTE_NOARC;route_build_phase=RB_IDLE;route_build_nf=route_build_nb=route_build_arc_idx=route_build_sp=0;route_build_copy=-1;route_build_after_base=RB_UNPACK;route_base_meta_n=route_base_meta_idx=0;route_sched_slices=route_sched_yields=route_build_steps=0;nav_maneuver_reset();}
static void ch_update_meet(struct ChState*s){if(!s||s->df==ROUTE_INF||s->db==ROUTE_INF)return;u32 v=sat_add32(s->df,s->db);if(v<route_best){route_best=v;route_meet=s->node;}}
static u32 route_exact_from=ROUTE_NOARC,route_exact_to=ROUTE_NOARC;
static void route_mark_query_start(void){route_query_lat=route_exact_from==ROUTE_NOARC?(atlas_probe?atlas_origin_lat:car_lat()):route_start_snap.snap_lat;route_query_lon=route_exact_from==ROUTE_NOARC?(atlas_probe?atlas_origin_lon:car_lon()):route_start_snap.snap_lon;}
static int ch_seed_destination(const struct RouteSnapCandidate*dest,u32 comp){if(dest&&dest->arc==ROUTE_NOARC){struct CprNode n;if(!route_read_node(dest->node,&n)||n.comp!=comp)return 0;struct ChState*s=ch_state(dest->node,1);if(!s)return 0;s->db=0;s->pb=ROUTE_NOARC;return ch_heap_insert(1,s);}struct CprBase b;if(!dest||!route_read_base(dest->arc,&b)||!b.src_count)return 0;int seeded=0;for(u32 i=0;i<b.src_count;i++){u32 node;if(!route_read_srcref(b.src_first+i,&node))return 0;struct CprNode rn;if(!route_read_node(node,&rn))return 0;if(rn.comp!=comp)continue;struct ChState*s=ch_state(node,1);if(!s)return 0;if(dest->extra<s->db){s->db=dest->extra;s->pb=ROUTE_NOARC;if(!ch_heap_insert(1,s))return 0;}seeded=1;}return seeded;}
static int route_dest_has_component(const struct RouteSnapCandidate*dest,u32 comp){if(dest&&dest->arc==ROUTE_NOARC){struct CprNode n;return route_read_node(dest->node,&n)&&n.comp==comp;}struct CprBase b;if(!dest||!route_read_base(dest->arc,&b))return 0;for(u32 i=0;i<b.src_count;i++){u32 node;struct CprNode n;if(!route_read_srcref(b.src_first+i,&node)||!route_read_node(node,&n))return 0;if(n.comp==comp)return 1;}return 0;}
static int ch_begin(const struct RouteSnapCandidate*start,const struct RouteSnapCandidate*dest){if(route_prof_on&&!route_prof_query_ms){route_prof_query_ms=s_now_ms();route_prof_pre_io=route_prof_io_bytes();}ch_reset();struct ChState*a=ch_state(start->node,1);if(!a)return 0;a->df=start->extra;if(!ch_heap_insert(0,a)||!ch_seed_destination(dest,route_dest_comp))return 0;route_mark_query_start();ch_update_meet(a);return 1;}
static int ch_retarget_start(const struct RouteSnapCandidate*start){traffic_invalidate();if(!chst||!chbucket)return 0;chhf_n=0;route_best=route_meet=ROUTE_INF;route_n=0;route_head=0;route_result_cost=0;route_result_m=0;nav_maneuver_reset();route_expanded=0;route_tick=0;chheap_peak=chhb_n;/* A completed route leaves the cooperative finaliser in RB_DONE.
     * A start-only CH retarget must reset every start/path-build field just as
     * a fresh CH query would. Leaving RB_DONE set after clearing route_n
     * makes the next routing slice finalise an empty path and raise R_ERROR. */route_emit_arc=route_emit_phys=ROUTE_NOARC;route_build_phase=RB_IDLE;route_build_nf=route_build_nb=route_build_arc_idx=route_build_sp=0;route_build_copy=-1;route_build_after_base=RB_UNPACK;route_base_meta_n=route_base_meta_idx=0;route_build_u=route_build_i=route_build_total=route_build_cum=route_build_prev_cost=0;route_build_steps=0;for(u32 i=0;i<chst_n;i++){chst[i].df=ROUTE_INF;chst[i].pf=ROUTE_NOARC;chst[i].posf=0;chst[i].sf=0;}struct ChState*a=ch_state(start->node,1);if(!a)return 0;a->df=start->extra;if(!ch_heap_insert(0,a))return 0;route_mark_query_start();ch_update_meet(a);return 1;}
static int ch_qad7_ensure_off(struct ChState*s){if(!s||qadjmode!=7)return 1;if(s->qoff!=ROUTE_NOARC)return s->qoff<qh7_query.data_bytes;u32 off=0;if(!qadj7_index_get(s->node,&off))return 0;s->qoff=off;return 1;}
static int ch_qad7_stall(int dir,u32 du,struct QAdj7Entry*f,u16 fc,struct QAdj7Entry*r,u16 rc){struct QAdj7Entry*e=dir?f:r;u32 n=dir?fc:rc;for(u32 i=0;i<n;i++){if(e[i].other>=rh.nodes)continue;struct ChState*t=ch_state(e[i].other,0);if(!t)continue;u32 d=dir?t->db:t->df;if(d!=ROUTE_INF&&sat_add32(d,e[i].cost)<du)return 1;}return 0;}
static int ch_relax_entries7(int dir,u32 du,struct QAdj7Entry*qe,u32 cnt){for(u32 j=0;j<cnt;j++){u32 v=qe[j].other,aid=qe[j].arc,off=qe[j].next_off;if(v>=rh.nodes||aid>=rh.ch_arcs||off>=qh7_query.data_bytes)return-1;struct ChState*t=ch_state(v,1);if(!t)return-2;if(t->qoff==ROUTE_NOARC)t->qoff=off;else if(t->qoff!=off)return-1;u32 nd=sat_add32(du,qe[j].cost);u32*dist=dir?&t->db:&t->df;if(nd<*dist){*dist=nd;if(dir)t->pb=aid;else t->pf=aid;if(!ch_heap_insert(dir,t))return-2;ch_update_meet(t);}}return 1;}
static u32 route_diag_qad7_expanded=0,route_diag_cpr_expanded=0;
static int ch_expand_one(void){
  u32 mf=ch_heap_min(0),mb=ch_heap_min(1);
  if(route_best!=ROUTE_INF&&mf>=route_best&&mb>=route_best)return 2;
  if(mf==ROUTE_INF&&mb==ROUTE_INF)return route_best==ROUTE_INF?0:2;int dir=(mf>mb);struct ChState*s=ch_heap_pop(dir);if(!s)return 0;if((dir?s->sb:s->sf))return 1;if(dir)s->sb=1;else s->sf=1;ch_update_meet(s);u32 du=dir?s->db:s->df;
  if(qadjmode==7){route_diag_qad7_expanded++;if(!ch_qad7_ensure_off(s))return-1;u16 fc=0,rc=0;struct QAdj7Entry*f=0,*r=0;if(!qadj7_read_block_off(s->qoff,&fc,&rc,&f,&r))return-1;if(ch_qad7_stall(dir,du,f,fc,r,rc)){RPROF_INC(route_prof_stall);return 1;}route_expanded++;u32 cnt=dir?rc:fc;RPROF_ADD(route_prof_relax,cnt);if(!cnt)return 1;return ch_relax_entries7(dir,du,dir?r:f,cnt);}
  route_expanded++;


  if(!qadjmode)route_diag_cpr_expanded++;struct CprNode n;if(!route_read_node(s->node,&n))return-1;u32 first=dir?n.upr_first:n.upf_first;u32 cnt=dir?n.upr_count:n.upf_count;RPROF_ADD(route_prof_relax,cnt);for(u32 i=0;i<cnt;i++){u32 aid;if(!route_read_upref(dir,first+i,&aid))return-1;struct CprArc a;if(!route_read_arc(aid,&a))return-1;u32 v=dir?a.from:a.to;struct ChState*t=ch_state(v,1);if(!t)return-2;u32 nd=sat_add32(du,a.cost);u32*dist=dir?&t->db:&t->df;if(nd<*dist){*dist=nd;if(dir)t->pb=aid;else t->pf=aid;if(!ch_heap_insert(dir,t))return-2;ch_update_meet(t);}}return 1;
}
static int route_append_coord_cost_m(s32 lat,s32 lon,u32 node,u32 cost,u32 segm){if(route_n&&route_lat[route_n-1]==lat&&route_lon[route_n-1]==lon){route_seg_cost[route_n-1]=sat_add32(route_seg_cost[route_n-1],cost);if(route_emit_arc!=ROUTE_NOARC){route_seg_arc[route_n-1]=route_emit_arc;route_seg_phys[route_n-1]=route_emit_phys;}if(node!=ROUTE_NOARC)route_node[route_n-1]=node;return 1;}if(route_n>=ROUTE_PATH_MAX)return 0;route_lat[route_n]=lat;route_lon[route_n]=lon;route_node[route_n]=node;route_seg_cost[route_n]=route_n?cost:0;route_seg_m[route_n]=route_n?segm:0;route_seg_arc[route_n]=route_n?route_emit_arc:ROUTE_NOARC;route_seg_phys[route_n]=route_n?route_emit_phys:ROUTE_NOARC;if(route_n)route_result_m=sat_add32(route_result_m,segm);route_n++;return 1;}
static int route_append_coord_cost(s32 lat,s32 lon,u32 node,u32 cost){u32 m=route_n?geo_seg_m(route_lat[route_n-1],route_lon[route_n-1],lat,lon):0;RPROF_INC(route_prof_div_fallback);return route_append_coord_cost_m(lat,lon,node,cost,m);}
static int route_append_node_cost(u32 node,u32 cost){struct CprNode n;if(!route_read_node(node,&n))return 0;return route_append_coord_cost(n.lat,n.lon,node,cost);}
static int ch_finish_begin(void){if(route_best==ROUTE_INF||route_meet==ROUTE_INF)return 0;if(route_prof_on&&!route_prof_build_ms){route_prof_build_ms=s_now_ms();route_prof_build_io=route_prof_io_bytes();}route_build_phase=RB_TRACE_F;route_build_nf=route_build_nb=route_build_arc_idx=route_build_sp=0;route_build_copy=-1;route_build_after_base=RB_UNPACK;route_base_meta_n=route_base_meta_idx=0;route_build_u=route_meet;route_n=0;route_head=0;route_result_m=0;route_build_steps=0;return 1;}
static int route_build_start_init(const struct RouteSnapCandidate*c){if(c->arc==ROUTE_NOARC){route_n=route_head=0;route_result_m=0;route_emit_arc=route_emit_phys=ROUTE_NOARC;return route_append_node_cost(c->node,0);}if(!route_read_base(c->arc,&route_build_base)||route_build_base.geom_count<2||c->geom_seg+1u>=route_build_base.geom_count)return 0;route_n=0;route_head=0;route_result_m=0;route_emit_arc=route_emit_phys=ROUTE_NOARC;if(!route_append_coord_cost(route_query_lat,route_query_lon,ROUTE_NOARC,0))return 0;if(!route_append_coord_cost(c->snap_lat,c->snap_lon,ROUTE_NOARC,0))return 0;route_emit_arc=c->arc;route_emit_phys=route_build_base.geom_first;route_build_i=c->geom_seg+1u;route_build_total=c->total_u>c->along_u?c->total_u-c->along_u:0;route_build_cum=route_build_prev_cost=0;route_build_prev_point.lat=c->snap_lat;route_build_prev_point.lon=c->snap_lon;return 1;}
static int route_build_start_point(const struct RouteSnapCandidate*c){if(c->arc==ROUTE_NOARC)return 2;if(route_build_i>=route_build_base.geom_count)return 2;struct CprPoint p;if(!route_base_point(&route_build_base,route_build_i,&p))return 0;route_build_cum=sat_add32(route_build_cum,route_geom_seg_units(&route_build_prev_point,&p));u32 cc=(route_build_i+1u==route_build_base.geom_count)?c->extra:route_cost_for_units(c->extra,route_build_cum,route_build_total),cost=cc>=route_build_prev_cost?cc-route_build_prev_cost:0;u32 node=(route_build_i+1u==route_build_base.geom_count)?c->node:ROUTE_NOARC;if(!route_append_coord_cost(p.lat,p.lon,node,cost))return 0;route_build_prev_cost=cc;route_build_prev_point=p;route_build_i++;return route_build_i>=route_build_base.geom_count?2:1;}
static int route_build_base_begin(u32 aid,const struct CprArc*a){(void)aid;if(!a||a->flags&CPR_ARC_SHORTCUT||a->left>=rh.base_edges||!route_read_base(a->left,&route_build_base)||route_build_base.geom_count<2)return 0;route_emit_arc=a->left;route_emit_phys=route_build_base.geom_first;route_build_arc=*a;route_build_i=1;route_build_total=route_build_cum=route_build_prev_cost=0;route_build_geom_cached=route_build_base.geom_count<=ROUTE_GEOM_TMP_MAX;if(!route_base_point(&route_build_base,0,&route_build_prev_point))return 0;if(route_build_geom_cached){route_geom_tmp[0]=route_build_prev_point;route_geom_cum_tmp[0]=0;}route_build_phase=RB_BASE_TOTAL;return 1;}
static struct QRec7 route_build_q7;
static int route_build_base_begin_qrec7(const struct QRec7*q){if(!q||qrec7_flags(q)&CPR_ARC_SHORTCUT||q->left>=rh.base_edges)return 0;u16 gc=qrec7_geom_count(q);if(gc<2||q->packet_off==0xffffffffu)return 0;route_build_q7=*q;memzero(&route_build_base,sizeof(route_build_base));route_build_base.geom_first=q->geom_first;route_build_base.geom_count=gc;route_build_base.flags=qrec7_base_flags(q);route_build_base.cls=qrec7_base_cls(q);route_build_base.to_state=qrec7_to(q);route_emit_arc=q->left;route_emit_phys=q->geom_first;memzero(&route_build_arc,sizeof(route_build_arc));route_build_arc.cost=q->cost;route_build_arc.to=qrec7_to(q);route_build_arc.left=q->left;route_build_i=1;route_build_total=q->right_geom;route_build_cum=route_build_prev_cost=0;route_build_geom_cached=0;u32 raw=(route_build_base.flags&CPR_ARC_GEOM_REVERSED)?gc-1u:0u,qf=0,qr=0;u16 mf=0,mr=0;if(!qadj7_packet_point(q,raw,&route_build_prev_point,&qf,&qr,&mf,&mr))return 0;route_build_phase=RB_BASE_OUTPUT;return 1;}
static int route_build_base_total_step(void){if(route_build_i>=route_build_base.geom_count){route_build_i=1;route_build_cum=route_build_prev_cost=0;if(route_build_geom_cached){route_build_prev_point=route_geom_tmp[0];RPROF_INC(route_prof_geom_reuse_arcs);RPROF_ADD(route_prof_geom_reuse_pts,route_build_base.geom_count);}else if(!route_base_point(&route_build_base,0,&route_build_prev_point))return 0;route_build_phase=RB_BASE_OUTPUT;return 1;}struct CprPoint p;if(!route_base_point(&route_build_base,route_build_i,&p))return 0;route_build_total=sat_add32(route_build_total,route_geom_seg_units(&route_build_prev_point,&p));if(route_build_geom_cached){route_geom_tmp[route_build_i]=p;route_geom_cum_tmp[route_build_i]=route_build_total;}route_build_prev_point=p;route_build_i++;return 1;}
static int route_build_finish_bases(void){if(route_repair_active){if(!route_repair_validate_anchor())return 0;route_repair_suffix_copy=1;route_build_phase=RB_REPAIR_SUFFIX;return 1;}route_build_phase=RB_DEST_INIT;return 1;}
static int route_build_base_output_step(void){if(route_build_i>=route_build_base.geom_count){route_build_phase=route_build_after_base;return 1;}struct CprPoint p;u32 cc=0,segm=0;int fast=0;if(qadjmode==7){u32 raw=(route_build_base.flags&CPR_ARC_GEOM_REVERSED)?(route_build_base.geom_count-1u-route_build_i):route_build_i,qf=0,qr=0;u16 mf=0,mr=0;if(qadj7_packet_point(&route_build_q7,raw,&p,&qf,&qr,&mf,&mr)){u32 q=(route_build_base.flags&CPR_ARC_GEOM_REVERSED)?qr:qf;segm=(route_build_base.flags&CPR_ARC_GEOM_REVERSED)?mr:mf;cc=(route_build_i+1u==route_build_base.geom_count)?route_build_arc.cost:route_mul_q16(route_build_arc.cost,q);fast=1;}}else if(!route_base_point(&route_build_base,route_build_i,&p))return 0;
 if(!fast){if(qadjmode==7){if(!route_base_point(&route_build_base,route_build_i,&p))return 0;}route_build_cum=sat_add32(route_build_cum,route_geom_seg_units(&route_build_prev_point,&p));cc=(route_build_i+1u==route_build_base.geom_count)?route_build_arc.cost:route_cost_for_units(route_build_arc.cost,route_build_cum,route_build_total);segm=geo_seg_m(route_build_prev_point.lat,route_build_prev_point.lon,p.lat,p.lon);RPROF_INC(route_prof_div_fallback);}else{RPROF_INC(route_prof_mfast);}u32 cost=cc>=route_build_prev_cost?cc-route_build_prev_cost:0;u32 node=(route_build_i+1u==route_build_base.geom_count)?route_build_arc.to:ROUTE_NOARC;if(!route_append_coord_cost_m(p.lat,p.lon,node,cost,segm))return 0;route_build_prev_cost=cc;route_build_prev_point=p;route_build_i++;if(route_build_i>=route_build_base.geom_count)route_build_phase=route_build_after_base;return 1;}
static int route_build_dest_init(const struct RouteSnapCandidate*c){if(c->arc==ROUTE_NOARC){route_emit_arc=route_emit_phys=ROUTE_NOARC;return 1;}if(!route_read_base(c->arc,&route_build_base)||route_build_base.geom_count<2||c->geom_seg+1u>=route_build_base.geom_count)return 0;route_emit_arc=c->arc;route_emit_phys=route_build_base.geom_first;route_build_i=1;route_build_total=c->along_u;route_build_cum=route_build_prev_cost=0;if(!route_base_point(&route_build_base,0,&route_build_prev_point))return 0;return 1;}
static int route_build_dest_point(const struct RouteSnapCandidate*c){if(c->arc==ROUTE_NOARC)return route_append_node_cost(c->node,0)?2:0;if(route_build_i<=c->geom_seg&&route_build_i<route_build_base.geom_count){struct CprPoint p;if(!route_base_point(&route_build_base,route_build_i,&p))return 0;route_build_cum=sat_add32(route_build_cum,route_geom_seg_units(&route_build_prev_point,&p));u32 cc=route_cost_for_units(c->extra,route_build_cum,route_build_total),cost=cc>=route_build_prev_cost?cc-route_build_prev_cost:0;if(!route_append_coord_cost(p.lat,p.lon,ROUTE_NOARC,cost))return 0;route_build_prev_cost=cc;route_build_prev_point=p;route_build_i++;return 1;}u32 cost=c->extra>=route_build_prev_cost?c->extra-route_build_prev_cost:0;if(!route_append_coord_cost(c->snap_lat,c->snap_lon,ROUTE_NOARC,cost))return 0;return 2;}
/* One bounded unit of CH path finalisation. Destination-side CH is multi-seeded
 * from every legal arrival-context state that may enter the snapped final road.
 * Therefore backward tracing stops at whichever seed has no parent. */
static int ch_finish_step(const struct RouteSnapCandidate*start,const struct RouteSnapCandidate*dest){route_build_steps++;if(route_prof_on&&route_build_phase>=0&&route_build_phase<15)route_prof_build_phase_steps[route_build_phase]++;switch(route_build_phase){
case RB_TRACE_F:{
  struct ChState*s=ch_state(route_build_u,0);
  if(route_repair_active){
    if(!s)return 0;
    if(s->pf==ROUTE_NOARC){int si=route_repair_start_for_node(route_build_u);if(si<0)return 0;route_repair_chosen_start=si;route_build_copy=route_build_nf-1;route_build_phase=RB_COPY_F;return 1;}
  }else if(route_build_u==start->node){route_build_copy=route_build_nf-1;route_build_phase=RB_COPY_F;return 1;}
  if(route_build_nf>=ROUTE_PATH_MAX||!s||s->pf==ROUTE_NOARC)return 0;u32 fr=0;if(qadjmode==7){struct QRec7 q;if(!qadj7_read_qrec(s->pf,&q))return 0;fr=q.from;}else{struct CprArc a;if(!route_read_arc(s->pf,&a))return 0;fr=a.from;}ch_arc_tmp[route_build_nf++]=s->pf;route_build_u=fr;return 1;}
case RB_COPY_F:if(route_build_copy<0){route_build_u=route_meet;route_build_phase=RB_TRACE_B;return 1;}if(route_build_nb>=ROUTE_PATH_MAX)return 0;ch_arc_path[route_build_nb++]=ch_arc_tmp[route_build_copy--];return 1;
case RB_TRACE_B:{struct ChState*s=ch_state(route_build_u,0);if(!s||s->db==ROUTE_INF)return 0;if(s->pb==ROUTE_NOARC){route_build_phase=RB_START_INIT;return 1;}if(route_build_nb>=ROUTE_PATH_MAX)return 0;u32 to=0;if(qadjmode==7){struct QRec7 q;if(!qadj7_read_qrec(s->pb,&q))return 0;to=qrec7_to(&q);}else{struct CprArc a;if(!route_read_arc(s->pb,&a))return 0;to=a.to;}ch_arc_path[route_build_nb++]=s->pb;route_build_u=to;}return 1;
case RB_START_INIT:{const struct RouteSnapCandidate*rs=route_repair_active?(route_repair_chosen_start>=0&&route_repair_chosen_start<route_repair_start_n?&route_repair_start[route_repair_chosen_start]:0):start;if(!rs||!route_build_start_init(rs))return 0;route_build_phase=RB_START_POINTS;return 1;}
case RB_START_POINTS:{const struct RouteSnapCandidate*rs=route_repair_active?&route_repair_start[route_repair_chosen_start]:start;int r=route_build_start_point(rs);if(!r)return 0;if(r==2)route_build_phase=RB_UNPACK;return 1;}
case RB_UNPACK:
  if(route_build_sp){u32 x=ch_unpack_stack[--route_build_sp];if(qadjmode==7){struct QRec7 q;if(!qadj7_read_qrec(x,&q))return 0;if(qrec7_flags(&q)&CPR_ARC_SHORTCUT){if(route_build_sp+2>ROUTE_PATH_MAX)return 0;ch_unpack_stack[route_build_sp++]=q.right_geom;ch_unpack_stack[route_build_sp++]=q.left;return 1;}if(route_base_meta_n>=ROUTE_PATH_MAX)return 0;route_base_meta[route_base_meta_n++]=q;return 1;}else{struct CprArc a;if(!route_read_arc(x,&a))return 0;if(a.flags&CPR_ARC_SHORTCUT){if(route_build_sp+2>ROUTE_PATH_MAX)return 0;ch_unpack_stack[route_build_sp++]=a.right;ch_unpack_stack[route_build_sp++]=a.left;return 1;}route_build_after_base=RB_UNPACK;return route_build_base_begin(x,&a)?1:0;}}
  if(route_build_arc_idx<route_build_nb){ch_unpack_stack[route_build_sp++]=ch_arc_path[route_build_arc_idx++];return 1;}
  if(qadjmode==7){route_base_meta_idx=0;route_build_phase=RB_BASE_NEXT;return 1;}
  return route_build_finish_bases();
case RB_BASE_NEXT:if(route_base_meta_idx<route_base_meta_n){route_build_after_base=RB_BASE_NEXT;return route_build_base_begin_qrec7(&route_base_meta[route_base_meta_idx++])?1:0;}return route_build_finish_bases();
case RB_BASE_TOTAL:return route_build_base_total_step()?1:0;
case RB_BASE_OUTPUT:{int n=qadjmode==7?16:1;while(n--){if(!route_build_base_output_step())return 0;if(route_build_phase!=RB_BASE_OUTPUT)break;}return 1;}
case RB_DEST_INIT:if(!route_build_dest_init(dest))return 0;route_build_phase=RB_DEST_POINTS;return 1;
case RB_DEST_POINTS:{int r=route_build_dest_point(dest);if(!r)return 0;if(r==2){route_result_cost=route_best;route_build_phase=RB_DONE;return route_n>0?2:0;}return 1;}
case RB_REPAIR_SUFFIX:return route_repair_suffix_step();
case RB_DONE:return route_n>0?2:0;default:return 0;}}

static u32 route_used_bytes(void){return chbucket_n*4u+chst_n*(u32)sizeof(struct ChState)+(chhf_n+chhb_n+2u)*4u;}
static void route_debug_summary(const char*tag){if(!pref.debug)return;char b[256];b[0]=0;appendstr(b,sizeof(b),tag);appendstr(b,sizeof(b)," ALG=CH4 STATES=");append_int(b,sizeof(b),(int)chst_n);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)chst_cap);appendstr(b,sizeof(b)," EXP=");append_int(b,sizeof(b),(int)route_expanded);appendstr(b,sizeof(b)," HEAP=");append_int(b,sizeof(b),(int)chheap_peak);appendstr(b,sizeof(b)," USEDKB=");append_int(b,sizeof(b),(int)(route_used_bytes()>>10));appendstr(b,sizeof(b)," ARENAKB=");append_int(b,sizeof(b),(int)(route_arena_bytes>>10));appendstr(b,sizeof(b)," AVAILKB=");append_int(b,sizeof(b),(int)route_mem_avail_kb);appendstr(b,sizeof(b)," SLICES=");append_int(b,sizeof(b),(int)route_sched_slices);appendstr(b,sizeof(b)," YIELD=");append_int(b,sizeof(b),(int)route_sched_yields);appendstr(b,sizeof(b)," BUILD=");append_int(b,sizeof(b),(int)route_build_steps);debug_log(STATE_LOG,b);if(!route_prof_on)return;u32 now=s_now_ms(),wall=now-route_prof_start_ms,pre=route_prof_query_ms?route_prof_query_ms-route_prof_start_ms:wall,query=route_prof_query_ms?(route_prof_build_ms?route_prof_build_ms-route_prof_query_ms:now-route_prof_query_ms):0,build=route_prof_build_ms?now-route_prof_build_ms:0,cpu=route_prof_cpu_ms;if(route_prof_slice_ms)cpu+=now-route_prof_slice_ms;u32 io=route_prof_io_bytes(),preio=route_prof_pre_io,qio=route_prof_build_io>=preio?route_prof_build_io-preio:0,bio=io>=route_prof_build_io?io-route_prof_build_io:0;b[0]=0;appendstr(b,sizeof(b),"ROUTE PROF WALLMS=");append_int(b,sizeof(b),(int)wall);appendstr(b,sizeof(b)," PREMS=");append_int(b,sizeof(b),(int)pre);appendstr(b,sizeof(b)," QUERYMS=");append_int(b,sizeof(b),(int)query);appendstr(b,sizeof(b)," BUILDMS=");append_int(b,sizeof(b),(int)build);appendstr(b,sizeof(b)," CPUMS=");append_int(b,sizeof(b),(int)cpu);appendstr(b,sizeof(b)," RELAX=");append_int(b,sizeof(b),(int)route_prof_relax);appendstr(b,sizeof(b)," PATH=");append_int(b,sizeof(b),route_n);appendstr(b,sizeof(b)," PREKB=");append_int(b,sizeof(b),(int)(preio>>10));appendstr(b,sizeof(b)," QKB=");append_int(b,sizeof(b),(int)(qio>>10));appendstr(b,sizeof(b)," BKB=");append_int(b,sizeof(b),(int)(bio>>10));debug_log(STATE_LOG,b);b[0]=0;appendstr(b,sizeof(b),"CPR PROF N=");append_int(b,sizeof(b),(int)route_prof_n_hit);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)route_prof_n_miss);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)(route_prof_n_bytes>>10));appendstr(b,sizeof(b)," B=");append_int(b,sizeof(b),(int)route_prof_b_hit);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)route_prof_b_miss);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)(route_prof_b_bytes>>10));appendstr(b,sizeof(b)," A=");append_int(b,sizeof(b),(int)route_prof_a_hit);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)route_prof_a_miss);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)(route_prof_a_bytes>>10));appendstr(b,sizeof(b)," G=");append_int(b,sizeof(b),(int)route_prof_g_hit);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)route_prof_g_miss);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)(route_prof_g_bytes>>10));appendstr(b,sizeof(b)," T=");append_int(b,sizeof(b),(int)route_prof_tile_reads);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)(route_prof_tile_bytes>>10));debug_log(STATE_LOG,b);b[0]=0;appendstr(b,sizeof(b),"CPR REF F=");for(int i=0;i<4;i++){if(i)appendstr(b,sizeof(b),i==1?" R=":(i==2?" T=":" S="));append_int(b,sizeof(b),(int)route_prof_r_hit[i]);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)route_prof_r_miss[i]);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)(route_prof_r_bytes[i]>>10));}debug_log(STATE_LOG,b);b[0]=0;appendstr(b,sizeof(b),"QADJ PROF MODE=");append_int(b,sizeof(b),qadjmode);appendstr(b,sizeof(b)," READS=");append_int(b,sizeof(b),(int)route_prof_qadj_reads);appendstr(b,sizeof(b)," KB=");append_int(b,sizeof(b),(int)(route_prof_qadj_bytes>>10));appendstr(b,sizeof(b)," VIDXKB=");append_int(b,sizeof(b),(int)((qadjmode==7?qh7_query.index_bytes:0u)>>10));appendstr(b,sizeof(b)," MMAP=");append_int(b,sizeof(b),qadj_map_ok);appendstr(b,sizeof(b)," STALL=");append_int(b,sizeof(b),(int)route_prof_stall);appendstr(b,sizeof(b)," QMAP=");append_int(b,sizeof(b),(int)route_prof_qmap_access);appendstr(b,sizeof(b)," IDXIOKB=");append_int(b,sizeof(b),(int)(route_prof_qidx_bytes>>10));debug_log(STATE_LOG,b);b[0]=0;appendstr(b,sizeof(b),"QREC PROF H=");append_int(b,sizeof(b),(int)route_prof_qrec_hit);appendstr(b,sizeof(b)," M=");append_int(b,sizeof(b),(int)route_prof_qrec_miss);appendstr(b,sizeof(b)," KB=");append_int(b,sizeof(b),(int)(route_prof_qrec_bytes>>10));debug_log(STATE_LOG,b);b[0]=0;appendstr(b,sizeof(b),"BUILD PROF TF=");append_int(b,sizeof(b),(int)route_prof_build_phase_steps[RB_TRACE_F]);appendstr(b,sizeof(b)," TB=");append_int(b,sizeof(b),(int)route_prof_build_phase_steps[RB_TRACE_B]);appendstr(b,sizeof(b)," UNP=");append_int(b,sizeof(b),(int)route_prof_build_phase_steps[RB_UNPACK]);appendstr(b,sizeof(b)," BASE=");append_int(b,sizeof(b),route_base_meta_n);appendstr(b,sizeof(b)," GT=");append_int(b,sizeof(b),(int)route_prof_build_phase_steps[RB_BASE_TOTAL]);appendstr(b,sizeof(b)," GO=");append_int(b,sizeof(b),(int)route_prof_build_phase_steps[RB_BASE_OUTPUT]);appendstr(b,sizeof(b)," SP=");append_int(b,sizeof(b),(int)route_prof_build_phase_steps[RB_START_POINTS]);appendstr(b,sizeof(b)," DP=");append_int(b,sizeof(b),(int)route_prof_build_phase_steps[RB_DEST_POINTS]);debug_log(STATE_LOG,b);b[0]=0;appendstr(b,sizeof(b),"BUILD IO MMAPG=");append_int(b,sizeof(b),route_geom_map_ok);appendstr(b,sizeof(b)," GMAP=");append_int(b,sizeof(b),(int)route_prof_gmap_access);debug_log(STATE_LOG,b);b[0]=0;appendstr(b,sizeof(b),"BUILD ARITH FASTM=");append_int(b,sizeof(b),(int)route_prof_mfast);appendstr(b,sizeof(b)," FALLBACK=");append_int(b,sizeof(b),(int)route_prof_div_fallback);appendstr(b,sizeof(b)," ROUTEM=");append_int(b,sizeof(b),(int)route_result_m);appendstr(b,sizeof(b)," PKTPTS=");append_int(b,sizeof(b),(int)route_prof_packet_pts);debug_log(STATE_LOG,b);}
static u32 geo_seg_m(s32 la0,s32 lo0,s32 la1,s32 lo1);
static u32 route_point_d2(s32 lat,s32 lon,s32 qlat,s32 qlon){int cq=coslat_q15(lat);s32 dx=lon_scale(qlon-lon,cq),dy=qlat-lat;return square_distance(dx,dy);}
static int route_snap_car_component(u32 comp,struct RouteSnapCandidate*out){struct RouteSnapCandidate c[8];int n=route_snap_candidates(car_lat(),car_lon(),ROUTE_SNAP_D2,1,0,c,8);for(int i=0;i<n;i++){struct CprNode rn;if(route_read_node(c[i].node,&rn)&&rn.comp==comp){if(out)*out=c[i];return 1;}}return 0;}
static u32 route_compute_m(void){u32 m=0;int first=route_head;if(first<0)first=0;for(int i=first+1;i<route_n;i++){u32 d=geo_seg_m(route_lat[i-1],route_lon[i-1],route_lat[i],route_lon[i]);if(0xffffffffu-m<d)return 0xffffffffu;m+=d;}return m;}
static int route_path_index_for_car(u32 snapped){if(route_n-route_head<2)return-1;int cq=coslat_q15(car_lat());int end=route_n-1;if(end>route_head+ROUTE_PROGRESS_LOOKAHEAD)end=route_head+ROUTE_PROGRESS_LOOKAHEAD;for(int i=route_head;i<=end;i++)if(route_node[i]==snapped){s32 dx=lon_scale(route_lon[i]-car_lon(),cq),dy=route_lat[i]-car_lat();u32 d=square_distance(dx,dy);if(d<ROUTE_PROGRESS_D2)return i;}u32 best=ROUTE_PROGRESS_D2;int bi=-1;for(int i=route_head;i<end;i++){s32 ax=lon_scale(route_lon[i]-car_lon(),cq),ay=route_lat[i]-car_lat();s32 bx=lon_scale(route_lon[i+1]-car_lon(),cq),by=route_lat[i+1]-car_lat();u32 d=seg_dist2(ax,ay,bx,by);if(d<best){best=d;bi=i;}}return bi;}
static void route_trim_prefix(int idx){if(idx>route_head&&idx<route_n){u32 spent=0,spentm=0;for(int i=route_head+1;i<=idx;i++){spent=sat_add32(spent,route_seg_cost[i]);spentm=sat_add32(spentm,route_seg_m[i]);}route_result_cost=route_result_cost>spent?route_result_cost-spent:0;route_result_m=route_result_m>spentm?route_result_m-spentm:0;route_head=idx;if(route_node[route_head]!=ROUTE_NOARC)route_car_node=route_node[route_head];if(nav_man_idx>=0){if(route_head>=nav_man_idx){nav_maneuver_reset();if(route_status==R_READY)nav_maneuver_find();}else nav_man_m=nav_man_m>spentm?nav_man_m-spentm:0;}else if(route_status==R_READY&&route_head-nav_man_scan_head>=ROUTE_MAN_RESCAN_POINTS)nav_maneuver_find();}}
/* stores the physical-polyline key (geom_first) beside each route
 * segment.  Adherence checks are therefore RAM-only over the route window;
 * they no longer perform two CPR2 base-record reads for every segment. */
static int route_path_index_for_snap(const struct RouteSnapCandidate*c){if(!c||route_n-route_head<2)return-1;struct CprBase cb;if(!route_read_base(c->arc,&cb))return-1;u32 phys=cb.geom_first;int cq=coslat_q15(c->snap_lat);int end=route_n-1;if(end>route_head+ROUTE_PROGRESS_LOOKAHEAD)end=route_head+ROUTE_PROGRESS_LOOKAHEAD;u32 best=ROUTE_ARC_PROGRESS_D2;int bi=-1;for(int i=route_head;i<end;i++){if(route_seg_phys[i+1]!=phys)continue;s32 ax=lon_scale(route_lon[i]-c->snap_lon,cq),ay=route_lat[i]-c->snap_lat;s32 bx=lon_scale(route_lon[i+1]-c->snap_lon,cq),by=route_lat[i+1]-c->snap_lat;u32 d=seg_dist2(ax,ay,bx,by);if(d<best){best=d;bi=i;}}return bi;}
/* For adherence, do not blindly trust one nearest snap.  At a junction GPS
 * can make a crossing/parallel arc win by a few metres even while the car is
 * still on the planned road.  If a route-matching candidate is essentially
 * tied with the nearest component candidate, keep the route. */
static int route_snap_car_adherence(u32 comp,struct RouteSnapCandidate*out,int*path_idx){struct RouteSnapCandidate c[8];int n=route_snap_candidates(car_lat(),car_lon(),ROUTE_SNAP_D2,1,0,c,8),have=0;struct RouteSnapCandidate nearest;u32 nearest_d=0xffffffffu;int match=-1;struct RouteSnapCandidate matchc;for(int i=0;i<n;i++){struct CprNode rn;if(!route_read_node(c[i].node,&rn)||rn.comp!=comp)continue;if(!have||c[i].d2<nearest_d){nearest=c[i];nearest_d=c[i].d2;have=1;}int idx=route_path_index_for_snap(&c[i]);if(idx>=0&&c[i].d2<=nearest_d+ROUTE_ADHERE_TIE_D2){match=idx;matchc=c[i];}}if(!have)return 0;if(match>=0){if(out)*out=matchc;if(path_idx)*path_idx=match;return 1;}if(out)*out=nearest;if(path_idx)*path_idx=-1;return 1;}
static int route_retarget_car(const struct RouteSnapCandidate*c){
  /* Repair the current regional leg in its original state/arc namespace.
   * Its saved suffix still ends at the exact gateway (or final destination),
   * so the remaining atlas plan stays valid. Crossing into another core or
   * failing any repair check falls back to a fresh end-to-end atlas plan. */
  if(atlas_active&&!atlas_probe&&atlas_owner(car_lat(),car_lon())!=atlas_route_region)return route_full_reroute("ATLAS REGION CHANGE");
  int was_ready=route_status==R_READY;struct CprNode n;if(!c||!route_read_node(c->node,&n)||n.comp!=route_dest_comp)return 0;
  if(was_ready){
    if(route_repair_begin(c)){route_exact_from=ROUTE_NOARC;return 1;}
    return route_full_reroute("LOCAL UNAVAILABLE");
  }
  int ok=route_repair_active?route_repair_reseed_starts():ch_retarget_start(c);
  if(!ok){if(route_repair_active)return route_full_reroute("MOVING RESEED");route_status=R_MEMORY;return 0;}
  route_start_snap=*c;route_car_node=c->node;route_offroute_checks=0;route_arc_check_tick=0;route_status=R_CALC;
  if(pref.debug)debug_log(STATE_LOG,route_repair_active?"CH4 REPAIR MOVING-START":"CH4 MOVING-START REQUERY");return 1;
}
static int route_begin_candidate(int idx){if(idx<0||idx>=route_dest_count)return 0;route_dest_idx=idx;struct CprNode car;if(!route_read_node(route_start_snap.node,&car)){route_status=R_ERROR;return 0;}if(!route_dest_has_component(&route_dest_cand[idx],car.comp))return 0;route_dest_comp=car.comp;route_rerouting=0;if(!ch_begin(&route_start_snap,&route_dest_cand[idx])){route_status=R_MEMORY;return 0;}route_status=R_CALC;return 1;}
static int route_retry_destination(void){for(int next=route_dest_idx+1;next<route_dest_count;next++){if(route_begin_candidate(next))return 1;}return 0;}
static int route_exact_snap(u32 node,struct RouteSnapCandidate*c){struct CprNode n;if(node>=rh.phys_nodes||!route_read_node(node,&n))return 0;memzero(c,sizeof(*c));c->node=node;c->arc=ROUTE_NOARC;c->snap_lat=n.lat;c->snap_lon=n.lon;return 1;}
static int route_same_endpoint(const struct RouteSnapCandidate*a,const struct RouteSnapCandidate*b){if(a->extra||a->snap_lat!=b->snap_lat||a->snap_lon!=b->snap_lon)return 0;struct CprNode source,dest;if(!route_read_node(a->node,&source)||source.lat!=a->snap_lat||source.lon!=a->snap_lon)return 0;if(a->arc==b->arc)return 1;struct CprBase base;if(!route_read_base(b->arc,&base))return 0;u32 node=ROUTE_NOARC;if(b->along_u==b->total_u)node=base.to_state;else if(!b->along_u&&!route_read_srcref(base.src_first,&node))return 0;if(node==ROUTE_NOARC||!route_read_node(node,&dest))return 0;return source.phys==dest.phys;}
/* An atlas request freezes both endpoints while it compares gateways. Snap
 * each coordinate once per regional namespace, rather than scanning the same
 * CPR geometry again for every trial. Final legs still snap the live car. */
static struct RouteSnapCandidate atlas_start_snap,atlas_dest_snap[ROUTE_DEST_CAND_MAX];
static int atlas_start_snap_region=-2,atlas_dest_snap_region=-2,atlas_start_snap_ok=0,atlas_dest_snap_count=0;
static int route_query_start_snap(struct RouteSnapCandidate*out){
 if(!atlas_probe)return route_snap_node(car_lat(),car_lon(),out);
 if(atlas_start_snap_region!=atlas_route_region){atlas_start_snap_region=atlas_route_region;atlas_start_snap_ok=route_snap_node(atlas_origin_lat,atlas_origin_lon,&atlas_start_snap);}
 if(atlas_start_snap_ok)*out=atlas_start_snap;return atlas_start_snap_ok;
}
static int route_query_dest_snaps(s32 lat,s32 lon){
 if(!atlas_probe)return route_snap_candidates(lat,lon,ROUTE_DEST_SNAP_D2,1,1,route_dest_cand,ROUTE_DEST_CAND_MAX);
 if(atlas_dest_snap_region!=atlas_route_region){atlas_dest_snap_region=atlas_route_region;atlas_dest_snap_count=route_snap_candidates(lat,lon,ROUTE_DEST_SNAP_D2,1,1,atlas_dest_snap,ROUTE_DEST_CAND_MAX);}
 for(int i=0;i<atlas_dest_snap_count;i++)route_dest_cand[i]=atlas_dest_snap[i];return atlas_dest_snap_count;
}
static int regional_route_start(s32 dest_lat,s32 dest_lon){route_repair_reset();route_prof_reset();route_arena_release();route_dest_count=route_dest_idx=0;route_dest_comp=0;route_offroute_checks=0;route_arc_check_tick=0;route_rerouting=0;route_target_lat=dest_lat;route_target_lon=dest_lon;if(!routeok){route_status=R_NOFILE;return 0;}if(!(route_exact_from!=ROUTE_NOARC?route_exact_snap(route_exact_from,&route_start_snap):route_query_start_snap(&route_start_snap))){route_status=R_NOROAD;return 0;}route_car_node=route_start_snap.node;/* at Z6 a CPR tile spans 256 coordinate units, larger than the 150-unit destination snap radius. A 3x3 neighbourhood therefore covers every geometrically legal candidate; the old 5x5 sweep only added I/O. */route_dest_count=route_exact_to!=ROUTE_NOARC?route_exact_snap(route_exact_to,&route_dest_cand[0]):route_query_dest_snaps(dest_lat,dest_lon);if(route_dest_count<1){route_status=R_NOROAD;return 0;}if(route_exact_to==ROUTE_NOARC&&route_same_endpoint(&route_start_snap,&route_dest_cand[0])){struct RouteSnapCandidate stop;memzero(&stop,sizeof(stop));stop.arc=ROUTE_NOARC;stop.node=route_start_snap.node;stop.snap_lat=route_start_snap.snap_lat;stop.snap_lon=route_start_snap.snap_lon;route_dest_cand[0]=stop;route_dest_count=1;}if(!route_arena_prepare()){route_status=R_MEMORY;return 0;}if(!route_begin_candidate(0)&&!route_retry_destination()){route_status=R_UNREACHABLE;return 0;}return 1;}
static int route_start(s32 lat,s32 lon){if(atlas_ready)return atlas_request(lat,lon);route_exact_from=route_exact_to=ROUTE_NOARC;return regional_route_start(lat,lon);}
static int route_full_reroute(const char*why){
  if(atlas_active)return atlas_request(atlas_dest_lat,atlas_dest_lon);
  s32 lat=route_target_lat,lon=route_target_lon;if(route_repair_begin_ms)route_repair_log_fallback(why);else if(pref.debug){char b[96];b[0]=0;appendstr(b,sizeof(b),"FULL REROUTE ");appendstr(b,sizeof(b),why);debug_log(STATE_LOG,b);}
  int ok=route_start(lat,lon);if(ok&&route_status==R_CALC)route_rerouting=1;return ok;
}
static int route_recover_reroute_error(void){if(atlas_active)return atlas_request(atlas_dest_lat,atlas_dest_lon);if(!route_rerouting)return 0;s32 lat=route_target_lat,lon=route_target_lon;if(pref.debug)debug_log(STATE_LOG,"CH4 REROUTE RECOVERY: FRESH QUERY");/* A fresh route is known-good and is cheap under CPR2.  If cached-side
     * rerouting ever encounters inconsistent transient state, recover
     * automatically instead of surfacing a trip/routing-data error.
     * Return 1 once recovery was attempted even if route_start selects a
     * meaningful NO ROAD/NO ROUTE/MEMORY state; do not overwrite it with the
     * generic R_ERROR in the caller. */(void)route_start(lat,lon);return 1;}
static void route_cancel(void){atlas_cancel();route_exact_from=route_exact_to=ROUTE_NOARC;route_repair_reset();ch_reset();route_arena_release();route_dest_count=route_dest_idx=0;route_dest_comp=0;route_offroute_checks=0;route_arc_check_tick=0;route_rerouting=0;route_status=R_IDLE;}
static int route_accept_finished_path(void){if(atlas_probe||route_exact_from!=ROUTE_NOARC)return 1;struct RouteSnapCandidate c;int idx=-1;if(!route_snap_car_adherence(route_dest_comp,&c,&idx))return 1;/* Prefer road identity when the car moved during calculation, while allowing only a very small nearest-road tie for junction/GPS ambiguity. */if(idx>=0){route_trim_prefix(idx);route_offroute_checks=0;return 1;}if(route_point_d2(route_query_lat,route_query_lon,car_lat(),car_lon())>=ROUTE_FINISH_STALE_D2){if(route_repair_active)return route_full_reroute("STALE BUILD")?0:-1;return route_retarget_car(&c)?0:-1;}idx=route_path_index_for_car(c.node);if(idx>=0){route_trim_prefix(idx);route_offroute_checks=0;}return 1;}
static void route_progress_service(void){
    /* Geometry-tail trimming is deliberately every 100 ms frame.  It is
     * CPU-only and keeps the magenta line attached to the car immediately. */
    int gidx=route_path_index_for_car(0xfffffffeu); /* impossible node => geometry-only */
    if(gidx>=0)route_trim_prefix(gidx);
    /* Road-identity snapping touches CPR2/SD data, so do it at ~3.3 Hz.  A
     * sustained mismatch for two checks (~0.6 s) is enough to reroute now
     * that CH queries are cheap; proximity to a parallel road no longer
     * suppresses recalculation. */
    if(++route_arc_check_tick<ROUTE_ARC_CHECK_TICKS)return;route_arc_check_tick=0;
    struct RouteSnapCandidate c;int idx=-1;if(!route_snap_car_adherence(route_dest_comp,&c,&idx))return;
    if(idx>=0){route_trim_prefix(idx);route_offroute_checks=0;return;}
    if(gps.mph10<20){route_offroute_checks=0;return;}
    if(++route_offroute_checks>=ROUTE_OFFROUTE_CHECKS){route_offroute_checks=0;route_retarget_car(&c);}
}
static void route_calc_moving_start_service(void){if(atlas_probe||route_exact_from!=ROUTE_NOARC)return;/* A parked car cannot meaningfully stale the start. Avoid periodic CPR2 snapping (and cache pollution) while stationary. */if(gps.mph10<20){route_tick=0;return;}/* Once a repair prefix is being reconstructed the old route[] storage is no longer pristine. Finish the bounded build, then the normal stale-finish check can fall back safely if needed. */if(route_repair_active&&route_build_phase!=RB_IDLE)return;if(++route_tick<5)return;route_tick=0;struct RouteSnapCandidate c;if(!route_snap_car_component(route_dest_comp,&c))return;if(c.node==route_start_snap.node&&c.arc==route_start_snap.arc&&c.geom_seg==route_start_snap.geom_seg&&c.frac_q16==route_start_snap.frac_q16)return;if(route_point_d2(route_query_lat,route_query_lon,car_lat(),car_lon())>=ROUTE_CALC_STALE_D2)route_retarget_car(&c);}
static void regional_service_until(u32 deadline_ms){
  if(route_status!=R_CALC&&route_status!=R_READY)return;
  if(route_status==R_READY){if(!atlas_probe)route_progress_service();return;}
  route_calc_moving_start_service();if(route_status!=R_CALC)return;route_sched_slices++;unsigned ops=0;
  while(route_status==R_CALC){
    if((ops++&ROUTE_CLOCK_CHECK_MASK)==0u&&!time_before_u32(s_now_ms(),deadline_ms)){route_sched_yields++;break;}
    if(route_build_phase!=RB_IDLE){
      int b=ch_finish_step(&route_start_snap,&route_dest_cand[route_dest_idx]);
      if(b==2){
        int was_repair=route_repair_active,a=route_accept_finished_path();
        if(a>0){
          route_status=R_READY;route_rerouting=0;nav_maneuver_find();
          if(was_repair){route_repair_log_ready();route_debug_summary("ROUTE REPAIRED");route_repair_reset();}else route_debug_summary("ROUTE READY");
          route_arena_release();
        }else if(a<0){if(!route_recover_reroute_error()){route_status=R_ERROR;route_arena_release();}}
        break;
      }
      if(!b){
        if(route_repair_active){(void)route_full_reroute("BUILD ERROR");break;}
        if(!route_recover_reroute_error()){route_status=R_ERROR;route_arena_release();}break;
      }
      continue;
    }
    int r=ch_expand_one();
    if(r==2){
      /* Atlas alternatives need an exact CH cost, not a drawable route.
       * Building every trial's geometry defeats QAD7's fast query path.
       * atlas_probe==2 requests distance only for the selected target leg. */
      if(atlas_probe==1){route_result_cost=route_best;route_result_m=0;route_status=R_READY;route_debug_summary("ATLAS COST READY");route_arena_release();break;}
      if(route_repair_active&&!route_repair_prepare_finish()){(void)route_full_reroute("LOCAL TOO COSTLY");break;}
      if(!ch_finish_begin()){
        if(route_repair_active){(void)route_full_reroute("FINISH INIT");break;}
        if(!route_recover_reroute_error()){route_status=R_ERROR;route_arena_release();}break;
      }
      continue;
    }
    if(r==0){
      if(route_repair_active){(void)route_full_reroute("NO LOCAL PATH");break;}
      if(route_retry_destination())break;route_status=R_UNREACHABLE;route_debug_summary("ROUTE UNREACHABLE");route_arena_release();break;
    }
    if(r==-2){
      if(route_repair_active){(void)route_full_reroute("LOCAL MEMORY");break;}
      route_status=R_MEMORY;route_debug_summary("ROUTE MEMORY");route_arena_release();break;
    }
    if(r<0){
      if(route_repair_active){(void)route_full_reroute("LOCAL DATA");break;}
      if(!route_recover_reroute_error()){route_status=R_ERROR;route_arena_release();}break;
    }
  }
}
#include "atlas_router.h"
static void route_service_until(u32 deadline){if(atlas_ready){atlas_service_until(deadline);if(atlas_active||route_status>=R_NOFILE)atlas_report();}else regional_service_until(deadline);}
static void route_service(void){route_service_until(s_now_ms()+20u);}
static u32 geo_seg_m(s32 la0,s32 lo0,s32 la1,s32 lo1){int cq=coslat_q15((la0+la1)/2);u32 dx=(u32)iabs32(lon_scale(lo1-lo0,cq)),dy=(u32)iabs32(la1-la0);u32 hi=dx>dy?dx:dy,lo=dx>dy?dy:dx;u32 q=hi+((lo*3u)>>3);return udiv32(q*111u+50u,100u);}
/* Cheap navigation-only segment length. The route's authoritative remaining
 * distance still uses geo_seg_m(); this approximation is only used while
 * scanning RAM for an upcoming manoeuvre, avoiding thousands of software
 * divisions on ARMv4T. Error is comfortably below what the guidance display
 * rounds away. */
static u32 nav_seg_m(s32 la0,s32 lo0,s32 la1,s32 lo1){int cq=coslat_q15((la0+la1)/2);u32 dx=(u32)iabs32(lon_scale(lo1-lo0,cq)),dy=(u32)iabs32(la1-la0);u32 hi=dx>dy?dx:dy,lo=dx>dy?dy:dx;u32 q=hi+((lo*3u)>>3);return q+(q>>3)-(q>>6);}
static int nav_heading10(s32 la0,s32 lo0,s32 la1,s32 lo1){int cq=coslat_q15((la0+la1)/2);s32 x=lon_scale(lo1-lo0,cq),y=la1-la0;if(!x&&!y)return-1;int bi=0;s32 bd=(s32)0x80000000u;for(int k=0;k<36;k++){s32 d=x*(s32)SIN36[k]+y*(s32)COS36[k];if(d>bd){bd=d;bi=k;}}return bi*10;}
static int nav_turn_delta(int i){int a=i-1,b=i+1;u32 m=nav_seg_m(route_lat[a],route_lon[a],route_lat[i],route_lon[i]);while(a>route_head&&m<30u){m+=nav_seg_m(route_lat[a-1],route_lon[a-1],route_lat[a],route_lon[a]);a--;}m=nav_seg_m(route_lat[i],route_lon[i],route_lat[b],route_lon[b]);while(b+1<route_n&&m<30u){m+=nav_seg_m(route_lat[b],route_lon[b],route_lat[b+1],route_lon[b+1]);b++;}int h0=nav_heading10(route_lat[a],route_lon[a],route_lat[i],route_lon[i]),h1=nav_heading10(route_lat[i],route_lon[i],route_lat[b],route_lon[b]);if(h0<0||h1<0)return 0;int d=h1-h0;while(d>180)d-=360;while(d<-180)d+=360;return d;}
static int nav_turn_type(int d){int a=d<0?-d:d;if(a<25)return MAN_NONE;if(a>=160)return MAN_UTURN;if(d<0)return a>=110?MAN_SHARP_LEFT:(a>=45?MAN_LEFT:MAN_SLIGHT_LEFT);return a>=110?MAN_SHARP_RIGHT:(a>=45?MAN_RIGHT:MAN_SLIGHT_RIGHT);}
/* BASE flag 4 identifies roundabout arcs; pad marks an outgoing public exit
 * at the directed arc endpoint. Count junctions, not geometry points/lanes. */
static int nav_round_info(int i,struct CprBase*b){return i>=0&&i<route_n&&route_seg_arc[i]!=ROUTE_NOARC&&route_read_base(route_seg_arc[i],b);}
static int nav_round_exit_legal(int i,const struct CprBase*b){if(i<0||i>=route_n||route_node[i]==ROUTE_NOARC)return b->pad!=0;u32 state=route_node[i];struct CprNode n;struct CprTile tile;if(!route_read_node(state,&n)||!route_find_tile(tile_div(n.lon,6),tile_div(n.lat,6),&tile))return 0;for(u32 j=0;j<tile.count;j++){u32 id;struct CprBase a;if(!route_read_ref(2,tile.first+j,&id)||!route_read_base(id,&a))return 0;if(a.flags&4)continue;for(u32 k=0;k<a.src_count;k++){u32 source;if(!route_read_srcref(a.src_first+k,&source))return 0;if(source==state)return 1;}}return 0;}
static int nav_round_scan(int entry,u32 distance,int joined_midway){int exits=0;int end=entry+ROUTE_MAN_SCAN_POINTS;if(end>route_n-1)end=route_n-1;for(int j=entry+1;j<end;j++){if(route_seg_arc[j]==route_seg_arc[j+1])continue;struct CprBase in,out;if(!nav_round_info(j,&in)||!nav_round_info(j+1,&out)||!(in.flags&4))return 0;if(nav_round_exit_legal(j,&in))exits++;if(!(out.flags&4)){nav_man_type=MAN_ROUNDABOUT;nav_man_exit=joined_midway?0:(exits?exits:1);nav_man_idx=j;nav_man_m=distance;return 1;}}return 0;}
static void nav_maneuver_find(void){nav_maneuver_reset();if(route_status!=R_READY||route_n-route_head<3)return;nav_man_scan_head=route_head;u32 dist=0;int end=route_head+ROUTE_MAN_SCAN_POINTS;if(end>route_n-1)end=route_n-1;struct CprBase first;if(nav_round_info(route_head+1,&first)&&(first.flags&4)&&nav_round_scan(route_head,0,1))return;for(int i=route_head+1;i<end;i++){dist=sat_add32(dist,nav_seg_m(route_lat[i-1],route_lon[i-1],route_lat[i],route_lon[i]));u32 in=route_seg_phys[i],out=route_seg_phys[i+1];if(in==ROUTE_NOARC||out==ROUTE_NOARC)continue;if(route_seg_arc[i]!=route_seg_arc[i+1]){struct CprBase a,b;if(nav_round_info(i,&a)&&nav_round_info(i+1,&b)){if(!(a.flags&4)&&(b.flags&4)&&nav_round_scan(i,dist,0))return;if((a.flags|b.flags)&4)continue;}}int t=nav_turn_type(nav_turn_delta(i));if(in==out&&t!=MAN_UTURN)continue;if(t!=MAN_NONE){nav_man_type=t;nav_man_idx=i;nav_man_m=dist;return;}}}
#include "mobility.h"
#include "traffic.h"
static const char* nav_man_text(void){static char exit_text[64];if(nav_man_type==MAN_ROUNDABOUT){if(!nav_man_exit)return ui_tr("Roundabout");copystr(exit_text,sizeof(exit_text),ui_tr("Take exit "));append_int(exit_text,sizeof(exit_text),nav_man_exit);return exit_text;}switch(nav_man_type){case MAN_SLIGHT_LEFT:return ui_tr("Slight left");case MAN_LEFT:return ui_tr("Turn left");case MAN_SHARP_LEFT:return ui_tr("Sharp left");case MAN_SLIGHT_RIGHT:return ui_tr("Slight right");case MAN_RIGHT:return ui_tr("Turn right");case MAN_SHARP_RIGHT:return ui_tr("Sharp right");case MAN_UTURN:return ui_tr("U-turn");default:return"";}}
static void append_nav_distance(char*b,unsigned cap,u32 m){if(!display_imperial()){append_metric_distance(b,cap,m,1);return;}if(m<70u){appendstr(b,cap,ui_tr("Now"));return;}if(m<1000u){u32 yd=udiv32(m*1094u+500u,1000u);yd=udiv32(yd+5u,10u)*10u;append_int(b,cap,(int)yd);appendstr(b,cap," yd");return;}u32 mi10=udiv32(m*10u+804u,1609u);append_int(b,cap,(int)(mi10/10u));appendstr(b,cap,".");char d[2]={(char)('0'+mi10%10u),0};appendstr(b,cap,d);appendstr(b,cap," mi");}
static void nav_arrow_head(int x,int y,int dx,int dy,u16 c){/* Tiny HUD arrow head. */int px=-dy,py=dx;line1(x,y,x-dx*4+px*3,y-dy*4+py*3,c,1);line1(x,y,x-dx*4-px*3,y-dy*4-py*3,c,1);}
#include "nav_roundabout.h"
static void draw_roundabout_icon(int cx,int cy){
 int clockwise=(region_flags&4)!=0;
 for(int y=0;y<23;y++)for(int x=0;x<23;x++){
  int alpha=nav_roundabout_alpha[y*23+(clockwise?x:22-x)];
  if(alpha)blend_pixel(cx+x-11,cy+y-11,C_CYAN,alpha);
 }
 /* A high-contrast count stays upright when the circulation glyph mirrors.
  * No count is invented when entering a roundabout midway. */
 if(nav_man_exit>0&&nav_man_exit<10){char n[2]={(char)('0'+nav_man_exit),0};compact_center(cx,cy-7,13,0,n,C_WHITE);}
 else if(nav_man_exit>=10&&nav_man_exit<100){
  int digits[2]={nav_man_exit/10,nav_man_exit%10};
  for(int i=0;i<2;i++)for(int y=0;y<7;y++)for(int x=0;x<5;x++){
   int a=nav_roundabout_digits[digits[i]][y*5+x];if(a)blend_pixel(cx-5+i*6+x,cy-5+y,C_WHITE,a);
  }
 }
}
static void draw_manoeuvre_icon(int cx,int cy,int t){u16 c=C_CYAN;if(t==MAN_ROUNDABOUT){draw_roundabout_icon(cx,cy);return;}if(t==MAN_UTURN){int side=(region_flags&4)?-1:1;line1(cx+side*4,cy+7,cx+side*4,cy-4,c,2);line1(cx+side*4,cy-4,cx+side,cy-7,c,2);line1(cx+side,cy-7,cx-side*5,cy-7,c,2);line1(cx-side*5,cy-7,cx-side*7,cy-4,c,2);line1(cx-side*7,cy-4,cx-side*7,cy+3,c,2);nav_arrow_head(cx-side*7,cy+3,0,1,c);return;}int left=t==MAN_SLIGHT_LEFT||t==MAN_LEFT||t==MAN_SHARP_LEFT;int slight=t==MAN_SLIGHT_LEFT||t==MAN_SLIGHT_RIGHT;int sharp=t==MAN_SHARP_LEFT||t==MAN_SHARP_RIGHT;line1(cx,cy+8,cx,cy,c,2);if(slight){int ex=cx+(left?-8:8),ey=cy-7;line1(cx,cy,ex,ey,c,2);nav_arrow_head(ex,ey,left?-1:1,-1,c);}else if(sharp){int mx=cx+(left?-4:4),my=cy-1,ex=cx+(left?-8:8),ey=cy+4;line1(cx,cy,mx,my,c,2);line1(mx,my,ex,ey,c,2);nav_arrow_head(ex,ey,left?-1:1,1,c);}else{int ex=cx+(left?-9:9),ey=cy;line1(cx,cy,ex,ey,c,2);nav_arrow_head(ex,ey,left?-1:1,0,c);}}
static void draw_manoeuvre_card(void){if(nav_man_type==MAN_NONE||nav_man_idx<0||nav_man_m>ROUTE_MAN_SHOW_M)return;char d[20];d[0]=0;append_nav_distance(d,sizeof(d),nav_man_m);/* Compact raised navigation card. */int mw=textw(d,1)+38;if(mw<62)mw=62;if(mw>92)mw=92;ui_surface(8,48,mw,24,C_PANEL2);fill(9,48,mw-2,2,C_CYAN);draw_manoeuvre_icon(22,60,nav_man_type);text(39,54,d,C_WHITE,1);}
static u32 route_path_m(void){return sat_add32(route_result_m,atlas_future_m);}
static void append_miles(char*b,unsigned cap,u32 m){if(!display_imperial()){append_metric_distance(b,cap,m,0);return;}u32 mi10=udiv32(m*10u+804u,1609u);append_int(b,cap,(int)(mi10/10u));appendstr(b,cap,".");char d[2]={(char)('0'+mi10%10u),0};appendstr(b,cap,d);appendstr(b,cap," mi");}
static void append_eta(char*b,unsigned cap,u32 ds){u32 min=udiv32(ds+300u,600u);if(min>=60u){append_int(b,cap,(int)(min/60u));appendstr(b,cap,"h ");append_int(b,cap,(int)(min%60u));appendstr(b,cap,"min");}else{append_int(b,cap,(int)min);appendstr(b,cap," min");}}
static void append_arrival_clock(char*b,unsigned cap,u32 ds){int mt=local_clock_minutes();if(mt<0){appendstr(b,cap,"--:--");return;}u32 add=udiv32(ds+300u,600u);mt=(mt+(int)(add%1440u))%1440;char t[6];t[0]=(char)('0'+mt/600);t[1]=(char)('0'+(mt/60)%10);t[2]=':';t[3]=(char)('0'+(mt%60)/10);t[4]=(char)('0'+mt%10);t[5]=0;appendstr(b,cap,t);}
static void route_draw_window(int*ps,int*pe){int first=route_head+1,last=route_n-1;if(first>last){*ps=1;*pe=0;return;}if(last-first+1<=ROUTE_DRAW_MAX_SEGMENTS){*ps=first;*pe=last;return;}if(!freecam){*ps=first;*pe=first+ROUTE_DRAW_MAX_SEGMENTS-1;if(*pe>last)*pe=last;return;}s32 vl=view_lat(),vo=view_lon();int cq=coslat_q15(vl),best=first;u32 bd=0xffffffffu;for(int i=first;i<=last;i+=64){s32 dx=lon_scale(route_lon[i]-vo,cq),dy=route_lat[i]-vl;u32 d=square_distance(dx,dy);if(d<bd){bd=d;best=i;}}{s32 dx=lon_scale(route_lon[last]-vo,cq),dy=route_lat[last]-vl;u32 d=square_distance(dx,dy);if(d<bd)best=last;}int half=ROUTE_DRAW_MAX_SEGMENTS/2,s=best-half;if(s<first)s=first;int e=s+ROUTE_DRAW_MAX_SEGMENTS-1;if(e>last){e=last;s=e-ROUTE_DRAW_MAX_SEGMENTS+1;if(s<first)s=first;}*ps=s;*pe=e;}
/* Segment i runs from vertex i-1 to i. Highlight only the bypassed interval. */
static int route_overlay_slot(int i,int offered){return offered&&i>traffic_begin_idx&&i<=traffic_join_idx&&!traffic_main_shared(i)?1:0;}
static void draw_route_overlay(int z){
 if(route_status!=R_READY||route_n-route_head<2)return;
 int s,e;route_draw_window(&s,&e);if(s>e)return;
 int offered=traffic_offer_valid(),slot=route_overlay_slot(s,offered);
 int x[255],y[255],n=1;
 if(s==route_head+1)map_project_q8(car_lat(),car_lon(),z,&x[0],&y[0]);
 else map_project_q8(route_lat[s-1],route_lon[s-1],z,&x[0],&y[0]);
 map_mask_begin(2);map_mask_select(slot);
 for(int i=s;i<=e;i++){
  if(!(i&31))render_checkpoint();
  int next=route_overlay_slot(i,offered);
  if(next!=slot){
   if(n>1)map_stroke_polyline(x,y,n,slot?C_ROUTE_SKIPPED:C_ROUTE,3);
   x[0]=x[n-1];y[0]=y[n-1];n=1;slot=next;map_mask_select(slot);
  }
  map_project_q8(route_lat[i],route_lon[i],z,&x[n],&y[n]);
  int visible=!((x[n-1]<-20*256&&x[n]<-20*256)||(x[n-1]>(W+20)*256&&x[n]>(W+20)*256)||(y[n-1]<(MAP_TOP-3)*256&&y[n]<(MAP_TOP-3)*256)||(y[n-1]>(MAP_BOTTOM+3)*256&&y[n]>(MAP_BOTTOM+3)*256));
  if(visible&&!freecam&&ndrive_route_segs<DRIVE_ROUTE_SEGS&&x[n-1]>-2048*256&&x[n-1]<2048*256&&y[n-1]>-2048*256&&y[n-1]<2048*256&&x[n]>-2048*256&&x[n]<2048*256&&y[n]>-2048*256&&y[n]<2048*256){
   struct DriveRouteSeg*r=&drive_route_segs[ndrive_route_segs++];r->x0=(s16)(x[n-1]>>8);r->y0=(s16)(y[n-1]>>8);r->x1=(s16)(x[n]>>8);r->y1=(s16)(y[n]>>8);
  }
  n++;if(n==255){map_stroke_polyline(x,y,n,slot?C_ROUTE_SKIPPED:C_ROUTE,3);x[0]=x[n-1];y[0]=y[n-1];n=1;}
 }
 if(n>1)map_stroke_polyline(x,y,n,slot?C_ROUTE_SKIPPED:C_ROUTE,3);
 if(offered){
  int phase=0,px,py;map_mask_select(2);
  map_project_q8(traffic_path[0].lat,traffic_path[0].lon,z,&px,&py);
  for(int i=1;i<traffic_offer_n;i++){
   if(!(i&31))render_checkpoint();int nx,ny;
   map_project_q8(traffic_path[i].lat,traffic_path[i].lon,z,&nx,&ny);
   if(!traffic_shared_alt[i])map_dash_segment(px,py,nx,ny,C_ROUTE_DIVERSION,&phase);else phase=0;
   px=nx;py=ny;
  }
 }
 map_mask_end();
}
/* Labels remain a *screen-space overlay* over the perspective road drawing;
 * unlike map geometry the bitmap font is never stretched or rotated. */
static int drive_iabs(int x){return x<0?-x:x;}
static int drive_label_eq(const char*a,const char*b){while(*a&&*b&&upc(*a)==upc(*b)){a++;b++;}return *a==*b;}
static void drive_label_prepare(int z){
  ndrive_labels=ndrive_route_segs=0;drive_turn_valid=0;
  if(!freecam&&route_status==R_READY&&nav_man_type!=MAN_NONE&&
     nav_man_idx>=route_head&&nav_man_idx<route_n){
    world_to_screen(route_lat[nav_man_idx],route_lon[nav_man_idx],z,&drive_turn_x,&drive_turn_y);
    drive_turn_valid=drive_turn_x>=0&&drive_turn_x<W&&drive_turn_y>=76&&drive_turn_y<215;
  }
}
static int drive_label_score(int x,int y,int kind,int prio){
  /* A one-step map-priority advantage always beats a position advantage.
   * Within equal priority, favour names nearer the useful driving area. */
  int s=prio*1024 + drive_iabs(y-184)*2 + drive_iabs(x-MAP_ANCHOR_X);
  if(kind==3)s-=34;else if(kind==4)s-=18;
  if(drive_turn_valid&&drive_iabs(x-drive_turn_x)<92&&drive_iabs(y-drive_turn_y)<40)s-=76;
  return s;
}
static int drive_label_primary(const struct DriveLabel*d){
  /* Uncluttered far-field criteria are first-pass preferences,
   * not irreversible rejections.  Remaining road names can fill vacancies. */
  return d->prio<=159 && (d->y>=124||d->prio<=31) &&
         (d->y>=151||d->prio<=79);
}
static void drive_label_offer(int x,int y,int kind,int prio,const char*name){
  if(freecam||!name||!name[0])return;
  if(kind>=6){poi_offer(x,y,kind,prio);return;}else{
    int w=textw(name,1),x0=x-w/2;
    if(x0<8||x0+w>W-8||y<80||y>202)return;
    if(roadctx.valid&&roadctx.street[0]&&drive_label_eq(name,roadctx.street))return;
    if(drive_iabs(x-MAP_ANCHOR_X)<31&&y>view_anchor_y()-29)return; /* car/near road */
  }
  int score=drive_label_score(x,y,kind,prio);
  /* Different tiles can repeat a road name. Keep the most useful position. */
  if(kind<6)for(int i=0;i<ndrive_labels;i++){
    if(drive_labels[i].kind<6&&drive_label_eq(drive_labels[i].name,name)){
      if(score>=drive_labels[i].score)return;
      drive_labels[i].x=x;drive_labels[i].y=y;
      drive_labels[i].kind=(u8)kind;drive_labels[i].prio=(u8)prio;
      drive_labels[i].score=score;return;
    }
  }
  int at=ndrive_labels;
  if(at>=DRIVE_LABEL_CANDIDATES){
    at=0;for(int i=1;i<DRIVE_LABEL_CANDIDATES;i++)
      if(drive_labels[i].score>drive_labels[at].score)at=i;
    if(score>=drive_labels[at].score)return;
  }else ndrive_labels++;
  struct DriveLabel*d=&drive_labels[at];
  copystr(d->name,sizeof(d->name),name);
  d->x=x;d->y=y;d->kind=(u8)kind;d->prio=(u8)prio;d->score=score;
}
static int drive_label_hits_route(int x0,int y0,int x1,int y1){
  /* Segment rectangles are recorded while the route is already being drawn,
   * avoiding a second route projection pass or map-data access. */
  for(int i=0;i<ndrive_route_segs;i++){
    const struct DriveRouteSeg*r=&drive_route_segs[i];
    int rx0=r->x0<r->x1?r->x0:r->x1;
    int rx1=r->x0>r->x1?r->x0:r->x1;
    int ry0=r->y0<r->y1?r->y0:r->y1;
    int ry1=r->y0>r->y1?r->y0:r->y1;
    if(x1+5>=rx0&&x0-5<=rx1&&y1+5>=ry0&&y0-5<=ry1)return 1;
  }
  return 0;
}
static void drive_label_render(void){
  if(freecam)return;
  int road_count=0,order[DRIVE_LABEL_CANDIDATES];
  for(int i=0;i<ndrive_labels;i++)order[i]=i;
  /* A small, bounded insertion sort; no allocations or tile re-reads. */
  for(int i=1;i<ndrive_labels;i++){
    int idx=order[i],j=i;
    while(j>0&&drive_labels[idx].score<drive_labels[order[j-1]].score){
      order[j]=order[j-1];j--;
    }
    order[j]=idx;
  }
  nltxt=0;
  for(int pass=0;pass<2&&road_count<DRIVE_ROAD_LABELS;pass++){
    /* Pass 0: unobtrusive names, ordered by road priority/vicinity.
     * Pass 1: only if under six, admit distant/minor names into free space. */
    for(int k=0;k<ndrive_labels&&road_count<DRIVE_ROAD_LABELS;k++){
      struct DriveLabel*d=&drive_labels[order[k]];
      if(d->kind>=6||drive_label_primary(d)!=(pass==0))continue;
      int w=textw(d->name,1),x0=d->x-w/2,y0=d->y-6;
      if(drive_label_hits_route(x0,y0,x0+w,y0+12))continue;
      if(!label_text_seen(d->name)&&label_free(x0-3,y0-3,x0+w+3,y0+15)){
        u16 c=label_color(d->kind);
        text(x0+1,y0+1,d->name,C_BG,1);
        text(x0,y0,d->name,c,1);
        label_text_add(d->name);road_count++;
      }
    }
  }

}
static void append_miles_hud(char*b,unsigned cap,u32 m){if(!display_imperial()){append_metric_distance(b,cap,m,0);return;}u32 mi10=udiv32(m*10u+804u,1609u);if(mi10>=100u){append_int(b,cap,(int)udiv32(mi10+5u,10u));appendstr(b,cap,"mi");}else{append_int(b,cap,(int)(mi10/10u));appendstr(b,cap,".");char d[2]={(char)('0'+mi10%10u),0};appendstr(b,cap,d);appendstr(b,cap,"mi");}}
static void append_eta_hud(char*b,unsigned cap,u32 ds){u32 min=udiv32(ds+300u,600u);if(min>=60u){append_int(b,cap,(int)(min/60u));appendstr(b,cap,"h");if(min%60u<10u)appendstr(b,cap,"0");append_int(b,cap,(int)(min%60u));}else{append_int(b,cap,(int)min);appendstr(b,cap,"min");}}
static int freecam_transition_active(void){return freecam_anim_mode!=FC_ANIM_NONE;}
static void freecam_orientation_anim_update(u32 now);
/* Hard recenter is reserved for programmatic state changes (arrival/trip/etc.).
 * User MAP toggling uses leave_freecam() below so it gets the reverse animation. */
static void recenter_map(void){freecam=0;map_tilt_q8=256;freecam_course_valid=0;freecam_anim_mode=FC_ANIM_NONE;freecam_anim_at=0;freecam_anim_tilt_from=256;zoom_override=-1;if(pref.autozoom)auto_zoom_level=-1;}
static void enter_freecam(void){if(freecam||freecam_transition_active()||(!gps.fix&&!track_valid))return;int z=active_zoom();int c=map_course();if(c<0||c>=360)c=0;freecam_course=c;freecam_course_valid=1;/* Tilt still animates even when already facing north. */freecam_anim_mode=FC_ANIM_TO_NORTH;freecam_anim_from=c;freecam_anim_at=hud_now_ms();freecam_anim_tilt_from=256;map_tilt_q8=256;freecam=1;freecam_zoom=z;zoom_override=-1;/* MAP entry begins exactly at the car's current/last tracked position. */cam_lat=car_lat();cam_lon=car_lon();}
static void leave_freecam(void){
 if(!freecam)return;
 /* Confirmation may arrive before Explore has finished entering. Reverse from
  * the current camera pose instead of snapping to north-up or driving tilt. */
 u32 now=hud_now_ms();freecam_orientation_anim_update(now);
 freecam=0;freecam_course_valid=1;freecam_anim_mode=FC_ANIM_TO_CAR;
 freecam_anim_from=freecam_course;freecam_anim_tilt_from=map_tilt_q8;
 freecam_anim_at=now;zoom_override=-1;if(pref.autozoom)auto_zoom_level=-1;
}
static void toggle_freecam(void){if(freecam_transition_active())return;if(freecam)leave_freecam();else enter_freecam();}
/* Presentation samples the same camera trajectory at any cadence. */
static void freecam_orientation_anim_update(u32 now){
 if(freecam_anim_mode==FC_ANIM_NONE)return;
 int mode=freecam_anim_mode,target=mode==FC_ANIM_TO_NORTH?0:map_course();
 if(target<0||target>=360){target=0;freecam_course_valid=0;}
 int d=target-freecam_anim_from;while(d>180)d-=360;while(d<-180)d+=360;
 u32 elapsed=now-freecam_anim_at;int q=frame_ease(elapsed,FREECAM_ORIENT_ANIM_MS);
 if(elapsed<FREECAM_ORIENT_ANIM_MS&&q==256)q=255;
 freecam_course=(freecam_anim_from+(d*q+(d>=0?128:-128))/256+360)%360;
 map_tilt_q8=freecam_anim_tilt_from+(((mode==FC_ANIM_TO_NORTH?0:256)-freecam_anim_tilt_from)*q)/256;
 if(q==256){freecam_course=target;map_tilt_q8=mode==FC_ANIM_TO_NORTH?0:256;
  if(mode==FC_ANIM_TO_CAR)freecam_course_valid=0;freecam_anim_mode=FC_ANIM_NONE;}
}
static int pan_step_for_zoom(int z){if(z<=6)return 80<<(6-z);z-=6;return 80>>z;}
/* During the short north-up transition, arrow presses must remain relative to
 * the screen rather than suddenly becoming world-north/world-east controls. */
static s32 freecam_east_to_lon(s32 east,int cq){if(!east)return 0;if(cq<=0)return east;u32 a=(u32)(east<0?-east:east);u32 q=udiv32((a<<15)+(u32)(cq>>1),(u32)cq);return east<0?-(s32)q:(s32)q;}
static void pan_freecam(int code){if(!gps.fix&&!freecam&&!track_valid)return;enter_freecam();if(!freecam)return;int z=active_zoom();int step=pan_step_for_zoom(z);if(step<1)step=1;int ss,c;view_trig(&ss,&c);s32 de=0,dn=0;if(code==103){de=(s32)(((s32)step*ss)>>15);dn=(s32)(((s32)step*c)>>15);}else if(code==108){de=-(s32)(((s32)step*ss)>>15);dn=-(s32)(((s32)step*c)>>15);}else if(code==106){de=(s32)(((s32)step*c)>>15);dn=-(s32)(((s32)step*ss)>>15);}else if(code==105){de=-(s32)(((s32)step*c)>>15);dn=(s32)(((s32)step*ss)>>15);}int cq=coslat_q15(cam_lat);cam_lat+=dn;if(cam_lat>8500000)cam_lat=8500000;if(cam_lat< -8500000)cam_lat=-8500000;cam_lon=lon_delta(cam_lon+freecam_east_to_lon(de,cq));}
#include "vehicle_icon.h"
/* The ground pointer flattens as the camera tilts. Use the camera's live tilt,
 * not the freecam flag: that flag changes at the START of entry/exit. Rotate
 * first, then foreshorten along screen depth, so all headings lie on the map. */
static int vehicle_scale_q8(void){
 int tilt=map_tilt_q8;if(tilt<0)tilt=0;if(tilt>256)tilt=256;
 return 256-((100*tilt+128)>>8);
}
static int vehicle_source_edge(int row,int scale){
 int n=(row-VEHICLE_ICON_SIZE/2)*65536,q=n/scale;
 if(n<0&&n%scale)q--;
 return q+VEHICLE_ICON_SIZE*128;
}
/* Pixel-area row filtering preserves coverage when compressing the footprint.
 * At most three source rows contribute. No image buffer or mask rebuild. */
static unsigned vehicle_row_mask(const u16*coverage,int col,int first,int count,const unsigned*weights){
 unsigned sums[4]={0,0,0,0};
 for(int i=0;i<count;i++){
  int row=first+i;if(row<0||row>=VEHICLE_ICON_SIZE)continue;
  unsigned mask=coverage[row*VEHICLE_ICON_SIZE+col];
  for(int layer=0;layer<4;layer++)sums[layer]+=((mask>>(layer*4))&15)*weights[i];
 }
 unsigned mask=0;for(int layer=0;layer<4;layer++)mask|=((sums[layer]+2048)>>12)<<(layer*4);
 return mask;
}
static void vehicle_icon(int x,int y,int heading){
 int k=((heading%360+360)%360+5)/10%VEHICLE_ICON_HEADINGS;
 const u16*coverage=vehicle_coverage[k];int scale=vehicle_scale_q8();
 u16 rim=theme_resolved?rgb(255,211,156):rgb(255,244,224);
 u16 facet=theme_resolved?rgb(255,203,135):rgb(255,187,106);
 int start=vehicle_source_edge(0,scale);
 for(int row=0;row<VEHICLE_ICON_SIZE;row++){
  int end=vehicle_source_edge(row+1,scale),first=start>>8;
  int count=((end-1)>>8)-first+1;unsigned weights[3]={0,0,0},used=0;
  if(scale!=256){
   for(int i=0;i<count;i++){
    int lo=start>=(first+i)*256?start:(first+i)*256;
    int hi=end<=(first+i+1)*256?end:(first+i+1)*256;
    weights[i]=i==count-1?4096-used:udiv32((u32)(hi-lo)*4096u+(u32)(end-start)/2u,(u32)(end-start));
    used+=weights[i];
   }
  }
  start=end;
  int sy=y+row-VEHICLE_ICON_SIZE/2;if(sy<MAP_TOP||sy>=MAP_BOTTOM)continue;
  if(scale!=256&&(first>=VEHICLE_ICON_SIZE||first+count<=0))continue;
  for(int col=0;col<VEHICLE_ICON_SIZE;col++){
   unsigned mask=scale==256?coverage[row*VEHICLE_ICON_SIZE+col]:vehicle_row_mask(coverage,col,first,count,weights);
   if(!mask)continue;
   int sx=x+col-VEHICLE_ICON_SIZE/2;
   int a=mask&15;if(a)blend_pixel(sx,sy,rgb(8,20,34),a);
   a=(mask>>4)&15;if(a)blend_pixel(sx,sy,rim,a);
   a=(mask>>8)&15;if(a)blend_pixel(sx,sy,C_CAR,a);
   a=(mask>>12)&15;if(a)blend_pixel(sx,sy,facet,a);
  }
 }
}
static void draw_car_marker(int z){
 if(!gps.fix)return;
 int cx,cy;world_to_screen(car_lat(),car_lon(),z,&cx,&cy);
 if(cx< -VEHICLE_ICON_SIZE/2||cx>W+VEHICLE_ICON_SIZE/2||cy<MAP_TOP-VEHICLE_ICON_SIZE/2||cy>MAP_BOTTOM+VEHICLE_ICON_SIZE/2)return;
 int rel=map_course();if(view_rotates())rel-=view_course();
 vehicle_icon(cx,cy,rel);
}
static void draw_explore_crosshair(void){if(!freecam)return;int x=240,y=160;map_line(x-12,y,x-4,y,C_DIM,1);map_line(x+4,y,x+12,y,C_DIM,1);map_line(x,y-12,x,y-4,C_DIM,1);map_line(x,y+4,x,y+12,C_DIM,1);rect(x-2,y-2,5,5,C_DIM);}
static void draw_map_geometry(int z){npoi=poi_visible=0;nlbox=nltxt=0;fill(0,MAP_TOP,W,MAP_BOTTOM-MAP_TOP,C_MAP_BG);if(!mapok||(!gps.fix&&!freecam&&!track_valid))return;s32 vl=view_lat(),vo=view_lon();if(!map_view_covered(vl,vo))return;int sourcez=z>6?6:clamp_map_zoom(z),tx0,tx1,ty0,ty1;
  map_visible_tile_bounds(z,sourcez,&tx0,&tx1,&ty0,&ty1);
  /* Ocean polygons retain close-zoom precision in shared Z0..Z3 tiles.
   * Reuse Z3 at closer zooms to avoid millions of redundant offshore tiles.
   * Finish the ocean before all local areas, strokes and roads. */
  if(MAP_AREA_FILLS&&(mh.flags&4)){
    int oz=sourcez<3?sourcez:3,a,b,c,d;
    map_visible_tile_bounds(z,oz,&a,&b,&c,&d);
    for(int ty=c;ty<=d;ty++)for(int tx=a;tx<=b;tx++)
      if(map_tile_in_pack(oz,tx,ty))for(int page=0;page<256;page++){
        struct Tile*t=get_tile_page((u8)(oz+8),tx,ty,(u8)page);
        if(!t)break;draw_tile_geometry(t,z,MAP_DRAW_OCEAN);
      }
  }
  /* Finish each layer across ALL tiles before moving to the next: fills,
   * context strokes (including river/coast outlines), then roads. A later
   * tile's water stroke must never paint over an earlier tile's bridge. */
  int first=MAP_AREA_FILLS?0:1;
  for(int phase=first;phase<3;phase++){
    if(phase)map_mask_begin(phase==1?0:1);
    for(int ty=ty0;ty<=ty1;ty++)for(int tx=tx0;tx<=tx1;tx++)
      if(map_tile_in_pack(sourcez,tx,ty))draw_tile_geometry(get_tile((u8)sourcez,tx,ty),z,
        phase==0?MAP_DRAW_AREAS:(phase==1?MAP_DRAW_CONTEXT:MAP_DRAW_ROADS));
    if(phase)map_mask_end();
  }
  {
    for(int ty=ty0;ty<=ty1;ty++)for(int tx=tx0;tx<=tx1;tx++)
      if(map_tile_in_pack(sourcez,tx,ty))draw_tile_labels(get_tile((u8)sourcez,tx,ty),z,256,256);
    poi_reserve();
    static const int cuts[5]={0,16,32,48,256};
    for(int b=0;b<4;b++)for(int ty=ty0;ty<=ty1;ty++)for(int tx=tx0;tx<=tx1;tx++)
      if(map_tile_in_pack(sourcez,tx,ty))draw_tile_labels(get_tile((u8)sourcez,tx,ty),z,cuts[b],cuts[b+1]-1);
  }}

#include "global_cameras.h"

/* Naming and coordinate entry share the existing remote controls and renderer;
 * on-device routing needs no search database or network service. */
static int coordinates_active=0;
static const char*coordinates_title(void);
static void coordinates_name_done(void);
static char kbd_text[161];static int kbd_focus=0;
static void add_fave_place(const char*name,s32 lat,s32 lon);
static void rename_fave(int idx,const char*name);
enum {KBD_NAME_FAVE=0,KBD_RENAME_FAVE=1,KBD_NAME_COORDINATES=2};static int kbd_action=KBD_NAME_FAVE;
static s32 kbd_place_lat=0,kbd_place_lon=0;static int kbd_fave_idx=-1;
#define K_ESC 1
#define K_BACK 13
#define K_SR 14
#define K_ENTER 28
#define K_UP 103
#define K_LEFT 105
#define K_RIGHT 106
#define K_DOWN 108
#define K_MAP 67
#define K_MENU 68
#define K_ROTCW 191
#define K_ROTCCW 192
#define K_CONSOLE_INFO 4096
#define K_CONSOLE_TRIP 4097
#define K_CONSOLE_THEME 4098
static int menu_blocks_phone(void);
static void menu_reset(u32 now);
#include "keyboard.h"

/* Trip navigation workspace. The Trip page is a
 * split workspace: left = destination sources, right = ordered itinerary. The router
 * calculates one active leg at a time. For multiple stops this means
 * CAR->STOP1 first; only after arrival/continue do we calculate STOP1->STOP2,
 * then the next leg, until the final destination is reached. */
#define RECENT_MAX 25
#define FAVE_MAX 25
#define TRIP_MAX_TARGETS 10
#define ARRIVAL_RADIUS_M 55u
#define ARRIVAL_TICKS 20
struct SavedPlace {s32 lat,lon;char name[161];};
struct TripTarget {s32 lat,lon;char name[161];};
struct TripState {u8 active,count,active_idx,paused;struct TripTarget target[TRIP_MAX_TARGETS];};
static struct SavedPlace recents[RECENT_MAX],faves[FAVE_MAX];static int nrecents=0,nfaves=0;
static struct TripState trip;static int trip_need_route=0,trip_route_defer=0,arrival_ticks=0;

/* All Yes/No confirmations share the established popup font face. */
enum {CONFIRMATION_FACE=1};
enum {MOD_NONE=0,MOD_ROUTE_POINT=1,MOD_RECENT=2,MOD_FAVE=3,MOD_CANCEL_ROUTE=4,MOD_TARGET=5,MOD_REMOVE_STOP=6,MOD_ARRIVED_STOP=7,MOD_ARRIVED_DEST=8,MOD_RESUME_TRIP=9,MOD_SOURCE_PICK=10};
static int modal=MOD_NONE,modal_choice=0,modal_index=-1;static s32 pending_lat=0,pending_lon=0;static int pending_point_stop=0;
enum {DEST_PICK_NONE=0,DEST_PICK_ADD_STOP=1,DEST_PICK_CHANGE_DEST=2};
static int dest_pick_action=DEST_PICK_NONE,trip_pick_mode=DEST_PICK_NONE;
struct PhoneDestination {int valid;u16 seq;char action;s32 lat,lon;char name[161];};
static struct PhoneDestination phone_dest;static int phone_seq_seen_valid=0;static u16 phone_seq_seen=0;
#define ARRIVAL_MODAL_MS 5000u
static u32 arrival_modal_deadline=0;
#define NOTICE_MS 1800u
static char notice_text[40]; /* Latest action, also used by native checks. */

#include "hud.h"

/* A single anti-aliased saving badge belongs beside the real decision
 * point, not halfway along the alternative. Only place it in clear map
 * space, outside the car, driving instruments and existing label boxes. */
static void diversion_saving_text(char*b,unsigned cap){
 copystr(b,cap,ui_tr("Save ~"));append_eta(b,cap,traffic_saving);
}
static int diversion_saving_box(int z,int w,struct HudBox*b){
 int qx,qy;map_project_q8(traffic_path[0].lat,traffic_path[0].lon,z,&qx,&qy);
 int x=qx>>8,y=qy>>8;
 if(x<6||x>W-6||y<6||y>H-6)return 0;
 int carx,cary,joinx,joiny;
 world_to_screen(car_lat(),car_lon(),z,&carx,&cary);
 world_to_screen(traffic_path[traffic_offer_n-1].lat,traffic_path[traffic_offer_n-1].lon,z,&joinx,&joiny);
 const int h=20,gap=9;
 /* Try the sides first; keep all alternatives immediately adjacent. */
 const int xs[]={x+gap,x-w-gap,x+20,x-w-20,x-w/2,x-w/2,x+gap,x-w-gap};
 const int ys[]={y-h/2,y-h/2,y-h/2,y-h/2,y-h-gap,y+gap,y-h-gap,y-h-gap};
 for(unsigned i=0;i<sizeof(xs)/sizeof(xs[0]);i++){
  int l=xs[i],t=ys[i];
  if(t<40||t+h>HUD_BOTTOM_Y-8||l<4||l+w>W-4)continue;
  if(l<carx+19&&l+w>carx-19&&t<cary+19&&t+h>cary-19)continue;
  if(l<joinx+6&&l+w>joinx-6&&t<joiny+6&&t+h>joiny-6)continue;
  if(!label_free(l-2,t-2,l+w+2,t+h+2))continue;
  *b=hud_box(l,t,w,h);return 1;
 }
 return 0;
}
static void box_text(int x,int y,int w,int h,int face,const char*s,u16 colour);
static void draw_diversion_markers(int z){
 if(!traffic_offer_valid())return;
 for(int k=0;k<2;k++){
  int i=k?traffic_offer_n-1:0,x,y;
  map_project_q8(traffic_path[i].lat,traffic_path[i].lon,z,&x,&y);
  if(x>=6*256&&x<=(W-6)*256&&y>=6*256&&y<=(H-6)*256){
   map_dot_q8(x,y,C_ROUTE_DIVERSION,5);map_dot_q8(x,y,C_MAP_BG,3);
  }
 }
 char text[112];diversion_saving_text(text,sizeof(text));
 struct HudBox badge;int w=width(text,0)+18;
 if(w<=164&&diversion_saving_box(z,w,&badge)){
  int old=hud_ink_alpha;hud_ink_alpha=16;
  /* Sample the clean map framebuffer using the same frosted glass, rim
   * and anti-aliased corners as the primary HUD. This background pass
   * must not consume or reuse a foreground compositor cache slot. */
  int glass_active=hud_glass_frame_active;hud_glass_frame_active=0;
  hud_glass(badge,0);hud_glass_frame_active=glass_active;
  box_text(badge.x+6,badge.y,badge.w-12,badge.h,0,text,hud_status_colour(HUD_SUCCESS,C_WHITE));
  hud_ink_alpha=old;
 }
}
static void draw_map_background(void){
 int z=active_zoom();perspective_scale_update();
 if(!freecam)drive_label_prepare(z);
 draw_map_geometry(z);draw_route_overlay(z);if(!freecam)drive_label_render();poi_render();draw_diversion_markers(z);
 draw_car_marker(z);draw_explore_crosshair();
}
static void draw_map(void){
 u32 now=hud_now_ms();freecam_orientation_anim_update(now);hud_prepare(active_zoom(),now,1);hud_driver_prepare(now);
 draw_map_background();hud_draw_all(hud_now_ms(),1);
}

/* CLP2 is the only saved-place layout. Reject unsupported or truncated files. */
static int read_places(const char*path,struct SavedPlace*a,int cap){
 long fd=s_open(path,O_RDONLY_,0);if(fd<0)return 0;u32 magic=0,count=0;int got=0;
 if(s_read((int)fd,&magic,4)==4&&magic==0x32504c43u&&s_read((int)fd,&count,4)==4&&count<=(u32)cap){
  for(u32 i=0;i<count;i++){
   if(s_read((int)fd,&a[i],sizeof(a[i]))!=(long)sizeof(a[i])){got=0;break;}
   a[i].name[160]=0;char n[161];copystr(n,sizeof(n),a[i].name);copystr(a[i].name,sizeof(a[i].name),n);got++;
  }
 }
 s_close((int)fd);return got;
}
static void write_places(const char*path,const struct SavedPlace*a,int n){long fd=s_open(path,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd<0)return;u32 magic=0x32504c43u,count=(u32)n;s_write((int)fd,&magic,4);s_write((int)fd,&count,4);if(n>0)s_write((int)fd,a,(unsigned)n*sizeof(*a));s_close((int)fd);}
static int same_place(const struct SavedPlace*a,s32 lat,s32 lon){return iabs32(a->lat-lat)<=8&&iabs32(a->lon-lon)<=8;}
static void add_recent(const char*name,s32 lat,s32 lon){int found=-1;for(int i=0;i<nrecents;i++)if(same_place(&recents[i],lat,lon)){found=i;break;}struct SavedPlace p;p.lat=lat;p.lon=lon;copystr(p.name,sizeof(p.name),name&&name[0]?name:"MAP POINT");if(found>=0){for(int i=found;i>0;i--)recents[i]=recents[i-1];recents[0]=p;}else{if(nrecents<RECENT_MAX)nrecents++;for(int i=nrecents-1;i>0;i--)recents[i]=recents[i-1];recents[0]=p;}write_places(RECENT_PATH,recents,nrecents);}
static int fave_contains(s32 lat,s32 lon){for(int i=0;i<nfaves;i++)if(same_place(&faves[i],lat,lon))return i;return-1;}
static void notice_success(const char*s);
static void add_fave_place(const char*name,s32 lat,s32 lon){struct SavedPlace p;p.lat=lat;p.lon=lon;copystr(p.name,sizeof(p.name),name&&name[0]?name:"MAP POINT");if(fave_contains(lat,lon)>=0)return;if(nfaves<FAVE_MAX)faves[nfaves++]=p;else{for(int i=1;i<FAVE_MAX;i++)faves[i-1]=faves[i];faves[FAVE_MAX-1]=p;}write_places(FAVES_PATH,faves,nfaves);notice_success(ui_tr("Favourite saved"));}
static int fave_needs_name(const char*name){return !name||!name[0]||streq(name,"MAP POINT");}
static void add_fave_or_name(const char*name,s32 lat,s32 lon){if(fave_needs_name(name)){keyboard_open_fave_name(lat,lon);screen=SCR_KEY;}else add_fave_place(name,lat,lon);}
static void add_fave(const struct SavedPlace*p){if(p)add_fave_or_name(p->name,p->lat,p->lon);}
static void remove_fave(int idx){if(idx<0||idx>=nfaves)return;for(int i=idx+1;i<nfaves;i++)faves[i-1]=faves[i];nfaves--;write_places(FAVES_PATH,faves,nfaves);notice_success(ui_tr("Favourite removed"));}
static void rename_fave(int idx,const char*name){if(idx<0||idx>=nfaves||!name||!name[0])return;char next[sizeof(faves[0].name)];copystr_limit(next,sizeof(next),name,40);if(streq(faves[idx].name,next))return;copystr(faves[idx].name,sizeof(faves[idx].name),next);write_places(FAVES_PATH,faves,nfaves);notice_success(ui_tr("Favourite renamed"));}
static void keyboard_open_fave_rename(int idx){if(idx<0||idx>=nfaves)return;kbd_action=KBD_RENAME_FAVE;kbd_fave_idx=idx;kbd_place_lat=faves[idx].lat;kbd_place_lon=faves[idx].lon;copystr_limit(kbd_text,sizeof(kbd_text),faves[idx].name,40);keyboard_begin();}

/* clioa2dp publishes validated CLP2 packets here. The packet carries a default
 * action (N = route as new, S = add as stop). If the user explicitly entered
 * ADD STOP / CHANGE DESTINATION -> SEND FROM PHONE on the Carminat, that local
 * action takes precedence for the next received destination. */
static void notice_set(const char*s,int is_error){
 copystr(notice_text,sizeof(notice_text),s&&s[0]?s:ui_tr("Done"));
 hud_status_notice(notice_text,is_error?HUD_ERROR:HUD_SUCCESS);
}
static void notice_success(const char*s){notice_set(s,0);}
static void notice_fail(const char*s){notice_set(s,1);}
static void poll_phone_destination(void){if(!hw_bluetooth_enabled)return;char b[384];long fd=s_open(BT_NAV_PATH,O_RDONLY_,0);if(fd<0)return;long n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);if(n<=0)return;b[n]=0;int have_seq=0,have_action=0,have_lat=0,have_lon=0;int seqi=0;s32 lat=0,lon=0;char action=0,name[161];copystr(name,sizeof(name),"PHONE DESTINATION");char*ln=b;for(long i=0;i<=n;i++){if(b[i]=='\r'||b[i]=='\n'||b[i]==0){char save=b[i];b[i]=0;if(starts(ln,"SEQ=")){seqi=atoi0(ln+4);have_seq=seqi>=0&&seqi<=65535;}else if(starts(ln,"ACTION=")){action=ln[7];have_action=(action=='N'||action=='S')&&ln[8]==0;}else if(starts(ln,"LAT=")){lat=(s32)atoi0(ln+4);have_lat=lat>=-9000000&&lat<=9000000;}else if(starts(ln,"LON=")){lon=(s32)atoi0(ln+4);have_lon=lon>=-18000000&&lon<=18000000;}else if(starts(ln,"NAME=")){copystr_limit(name,sizeof(name),ln+5,40);}if(!save)break;ln=b+i+1;}}
  if(!have_seq||!have_action||!have_lat||!have_lon)return;u16 seq=(u16)seqi;if(phone_seq_seen_valid&&phone_seq_seen==seq)return;phone_seq_seen=seq;phone_seq_seen_valid=1;phone_dest.valid=1;phone_dest.seq=seq;phone_dest.action=action;phone_dest.lat=lat;phone_dest.lon=lon;copystr(phone_dest.name,sizeof(phone_dest.name),name[0]?name:"PHONE DESTINATION");}
static void phone_destination_clear(void){phone_dest.valid=0;}

static struct TripTarget* trip_current(void){if(!trip.active||trip.count<1||trip.active_idx>=trip.count)return 0;return &trip.target[trip.active_idx];}
static int trip_current_is_destination(void){return trip.active&&trip.count&&trip.active_idx==trip.count-1;}
/* CTR4 stores up to ten targets. Other file versions are not supported. */
static void save_trip(void){long fd=s_open(TRIP_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd<0)return;u32 magic=0x34525443u;s_write((int)fd,&magic,4);s_write((int)fd,&trip,sizeof(trip));s_close((int)fd);}
static void load_trip(void){
 memzero(&trip,sizeof(trip));long fd=s_open(TRIP_PATH,O_RDONLY_,0);if(fd<0)return;
 u32 magic=0;struct TripState saved;
 int ok=s_read((int)fd,&magic,4)==4&&magic==0x34525443u&&s_read((int)fd,&saved,sizeof(saved))==(long)sizeof(saved);
 s_close((int)fd);
 if(!ok||saved.count>TRIP_MAX_TARGETS||saved.active>1||saved.paused>1||(!saved.count&&saved.active)||(saved.count&&saved.active_idx>=saved.count))return;
 trip=saved;for(int i=0;i<trip.count;i++)trip.target[i].name[160]=0;trip.paused=0;
}
static void trip_schedule_route(void){arrival_ticks=0;trip_need_route=1;trip_route_defer=1;route_cancel();}
static void trip_request_route(void){struct TripTarget*t=trip_current();arrival_ticks=0;if(!t||trip.paused){trip_need_route=0;trip_route_defer=0;return;}if(!gps.fix){trip_need_route=1;route_cancel();return;}trip_need_route=0;trip_route_defer=0;route_start(t->lat,t->lon);}
static void set_destination(const char*name,s32 lat,s32 lon){memzero(&trip,sizeof(trip));trip.active=1;trip.count=1;trip.active_idx=0;trip.target[0].lat=lat;trip.target[0].lon=lon;copystr(trip.target[0].name,sizeof(trip.target[0].name),name&&name[0]?name:"MAP POINT");save_trip();add_recent(trip.target[0].name,lat,lon);trip_schedule_route();notice_success(ui_tr("Route started"));}
static void mobility_append_coord(char*b,unsigned cap,s32 value){
 if(value<0){appendstr(b,cap,"-");value=-value;}append_int(b,cap,value/100000);appendstr(b,cap,".");
 char digits[6];for(int i=4;i>=0;i--){digits[i]=(char)('0'+value%10);value/=10;}digits[5]=0;appendstr(b,cap,digits);
}
static int suggested_place(struct SavedPlace*out){
 if(!pref.learn_history||!pref.destination_predictions||!mob_suggest_valid||trip.active)return 0;
 struct MobPoint p=mob_suggest_point;u32 best=101;const struct SavedPlace*chosen=0;
 for(int i=0;i<nfaves;i++){u32 d=mob_dist(p,(struct MobPoint){faves[i].lat,faves[i].lon});if(d<best){best=d;chosen=&faves[i];}}
 if(!chosen)for(int i=0;i<nrecents;i++){u32 d=mob_dist(p,(struct MobPoint){recents[i].lat,recents[i].lon});if(d<best){best=d;chosen=&recents[i];}}
 if(chosen)*out=*chosen;
 else{memzero(out,sizeof(*out));out->lat=p.lat;out->lon=p.lon;mobility_append_coord(out->name,sizeof(out->name),p.lat);appendstr(out->name,sizeof(out->name),", ");mobility_append_coord(out->name,sizeof(out->name),p.lon);}
 return 1;
}
static int suggested_available(void){struct SavedPlace p;return suggested_place(&p);}
static int suggested_start(void){struct SavedPlace p;if(!suggested_place(&p))return 0;set_destination(p.name,p.lat,p.lon);return 1;}
static void mobility_service(u32 now){
 if(!pref.learn_history){if(mob_recording){mob_stop_recording();traffic_learning_reset();}return;}
 if(!mob_recording){mob_session_begin(now,lastfix_valid,lastfix_lat,lastfix_lon);traffic_learning_reset();}
 if(!mob_recording)return;
 int fresh=mob_sample(now);
 if(fresh&&mob_started){
  if(atlas_ready&&!atlas_active&&route_status==R_IDLE&&atlas_owner(mob_rmc_point.lat,mob_rmc_point.lon)!=atlas_route_region){
   int region=atlas_owner(mob_rmc_point.lat,mob_rmc_point.lon);if(region>=0)(void)atlas_open_route(region);
  }
  traffic_learn_sample();
 }
 if(pref.destination_predictions&&!trip.active){
  u32 epoch=mob_time(now);
  static u32 last_minute=MOB_UNKNOWN;static int last_origin,last_started;
  u32 minute=epoch==MOB_UNKNOWN?MOB_UNKNOWN:epoch/60u;
  if(!mob_predict_ms||last_origin!=mob_origin_valid||last_started!=mob_started||(!mob_started&&minute!=last_minute)||(mob_started&&now-mob_predict_ms>=5000u)){
   last_origin=mob_origin_valid;last_started=mob_started;
   int old=mob_suggest_valid;struct MobPoint oldp=mob_suggest_point;
   mob_predict_ms=now;last_minute=minute;(void)mob_predict(now);
   if(old!=mob_suggest_valid||oldp.lat!=mob_suggest_point.lat||oldp.lon!=mob_suggest_point.lon)render_background_dirty=1;
  }
 }else mob_suggest_valid=0;
 int prev_offer=traffic_offer_valid(),prev_warn=traffic_warn;u32 prev_saving=traffic_saving;
 traffic_service(now);
 if(prev_offer!=traffic_offer_valid()||prev_warn!=traffic_warn||prev_saving!=traffic_saving)render_background_dirty=1;
}
static void clear_trip(void){memzero(&trip,sizeof(trip));save_trip();arrival_ticks=0;trip_need_route=trip_route_defer=0;route_cancel();}
static int trip_add_stop(const char*name,s32 lat,s32 lon){if(!trip.active||trip.count<1||trip.count>=TRIP_MAX_TARGETS)return 0;int ins=trip.count-1;for(int i=trip.count;i>ins;i--)trip.target[i]=trip.target[i-1];trip.target[ins].lat=lat;trip.target[ins].lon=lon;copystr(trip.target[ins].name,sizeof(trip.target[ins].name),name&&name[0]?name:"MAP POINT");trip.count++;if(ins==trip.active_idx)trip_schedule_route();save_trip();notice_success(ui_tr("Stop added"));return 1;}
static void trip_change_destination(const char*name,s32 lat,s32 lon){if(!trip.active||!trip.count){set_destination(name,lat,lon);return;}int i=trip.count-1;int changed=trip.target[i].lat!=lat||trip.target[i].lon!=lon||!streq(trip.target[i].name,name);trip.target[i].lat=lat;trip.target[i].lon=lon;copystr(trip.target[i].name,sizeof(trip.target[i].name),name&&name[0]?name:"MAP POINT");add_recent(trip.target[i].name,lat,lon);save_trip();if(i==trip.active_idx)trip_schedule_route();if(changed)notice_success(ui_tr("Destination changed"));}
static void phone_destination_apply(void){if(menu_blocks_phone())return;if(coordinates_active&&screen!=SCR_COORDINATES&&!(screen==SCR_KEY&&kbd_action==KBD_NAME_COORDINATES))coordinates_active=0;if(!phone_dest.valid||coordinates_active)return;int action=phone_dest.action=='S'?DEST_PICK_ADD_STOP:DEST_PICK_NONE;if(modal!=MOD_NONE)return;if(action==DEST_PICK_ADD_STOP){if(!trip.active||!trip.count)notice_fail(ui_tr("No active route"));else if(trip.count>=TRIP_MAX_TARGETS)notice_fail(ui_tr("Trip full - stop not added"));else if(trip_add_stop(phone_dest.name,phone_dest.lat,phone_dest.lon)){add_recent(phone_dest.name,phone_dest.lat,phone_dest.lon);}else notice_fail(ui_tr("Stop not added"));}
  else{set_destination(phone_dest.name,phone_dest.lat,phone_dest.lon);screen=SCR_MAP;recenter_map();}
  phone_destination_clear();}
static void trip_remove_stop(int idx){if(!trip.active||idx<trip.active_idx||idx<0||idx>=trip.count-1)return;int was_current=idx==trip.active_idx;for(int i=idx+1;i<trip.count;i++)trip.target[i-1]=trip.target[i];trip.count--;save_trip();if(was_current)trip_schedule_route();notice_success(ui_tr("Stop removed"));}
static void trip_move_stop(int idx,int dir){if(!trip.active||idx<trip.active_idx||idx>=trip.count-1)return;int j=idx+dir;if(j<trip.active_idx||j>=trip.count-1)return;struct TripTarget t=trip.target[idx];trip.target[idx]=trip.target[j];trip.target[j]=t;save_trip();if(idx==trip.active_idx||j==trip.active_idx)trip_schedule_route();notice_success(ui_tr("Stop moved"));}
static void trip_continue_after_stop(void){if(!trip.active||trip.active_idx+1>=trip.count)return;trip.active_idx++;trip.paused=0;arrival_ticks=0;save_trip();trip_schedule_route();}
static void trip_service(void){if(trip_need_route&&gps.fix&&!trip.paused){if(trip_route_defer>0)trip_route_defer--;else trip_request_route();}if(atlas_active&&(atlas_phase!=AP_LEG||route_exact_to!=ROUTE_NOARC)){arrival_ticks=0;return;}if(!trip.active||trip.paused||!gps.fix)return;struct TripTarget*t=trip_current();if(!t)return;u32 d=geo_seg_m(car_lat(),car_lon(),t->lat,t->lon);/* the drivable route ends at the exact road-access snap, not at a fabricated straight connector to the POI centroid.  Count reaching that access point as arrival too. */if(route_status==R_READY&&!atlas_future_cost&&route_exact_to==ROUTE_NOARC&&route_dest_idx>=0&&route_dest_idx<route_dest_count){u32 sd=geo_seg_m(car_lat(),car_lon(),route_dest_cand[route_dest_idx].snap_lat,route_dest_cand[route_dest_idx].snap_lon);if(sd<d)d=sd;}if(d<=ARRIVAL_RADIUS_M&&gps.mph10<=100){if(arrival_ticks<ARRIVAL_TICKS)arrival_ticks++;}else arrival_ticks=0;if(arrival_ticks>=ARRIVAL_TICKS){arrival_ticks=0;trip.paused=1;route_cancel();save_trip();modal_choice=0;modal=trip_current_is_destination()?MOD_ARRIVED_DEST:MOD_ARRIVED_STOP;arrival_modal_deadline=s_now_ms()+ARRIVAL_MODAL_MS;}}
static void arrival_modal_service(void){if(modal!=MOD_ARRIVED_STOP&&modal!=MOD_ARRIVED_DEST){arrival_modal_deadline=0;return;}if(!arrival_modal_deadline)arrival_modal_deadline=s_now_ms()+ARRIVAL_MODAL_MS;if(time_before_u32(s_now_ms(),arrival_modal_deadline))return;arrival_modal_deadline=0;if(modal==MOD_ARRIVED_STOP){modal=MOD_NONE;trip_continue_after_stop();screen=SCR_MAP;recenter_map();}else{modal=MOD_NONE;clear_trip();screen=SCR_MAP;recenter_map();}}
static int menu_focus=0;
static void menurow(int y,const char*l,const char*sub,int f){ui_choice(18,y,444,60,f);label(34,y+9,3,l,ui_choice_text(f),408);label(34,y+33,0,sub,f?C_ACTIVE_TEXT:C_MUTED,408);}
static void draw_menu(void){fill(0,0,W,H,C_BG);menurow(MENU_BODY_Y,ui_tr("Trip"),ui_tr("Destinations and itinerary"),menu_focus==0);menurow(MENU_BODY_Y+66,bt_label(),ui_tr("Connected and saved phones"),menu_focus==1);menurow(MENU_BODY_Y+132,ui_tr("Settings"),ui_tr("Display and navigation"),menu_focus==2);centered(240,254,0,"(c) OpenStreetMap contributors",C_MUTED);}

static int trip_pane=0,trip_tab=0,trip_focus=0,trip_right_focus=0;
#include "coordinates.h"
static void focus_rect(int x,int y,int w,int h,int active){ui_panel(x,y,w,h,active);}
static void trip_item(int x,int y,int w,const char*s,int f){char name[161];copystr_limit(name,sizeof(name),s,40);ui_choice(x,y-3,w,19,f);compact_fit(x+8,y-2,w-16,1,name,ui_choice_text(f));}
static const char* active_route_word(void){if(route_status==R_READY)return ui_tr("Ready");if(trip_need_route||route_status==R_CALC)return ui_tr("Calc");if(route_status==R_NOFILE)return ui_tr("No data");if(route_status==R_NOROAD)return ui_tr("No road");if(route_status==R_UNREACHABLE)return ui_tr("No route");if(route_status==R_MEMORY)return ui_tr("Mem full");if(route_status==R_ERROR)return ui_tr("Error");return ui_tr("Wait");}
static void trip_target_row(int y,int idx,int selected){
 ui_choice(249,y-3,220,20,selected);label(256,y-1,0,idx==trip.count-1?"D":"S",selected?C_ACTIVE_TEXT:C_MUTED,12);
 const char*st=idx<trip.active_idx?ui_tr("Done"):(idx==trip.active_idx?active_route_word():ui_tr("Wait"));int sw=width(st,0);if(sw<44)sw=44;if(sw>104)sw=104;int sx=462-sw;
 char name[161];copystr_limit(name,sizeof(name),trip.target[idx].name[0]?place_display_name(trip.target[idx].name):ui_tr("Map point"),40);compact_fit(272,y-2,sx-282,1,name,ui_choice_text(selected));
 u16 sc=selected?C_ACTIVE_TEXT:(idx==trip.active_idx?(route_status==R_READY?C_CYAN:C_WARN):C_MUTED);compact_fit(sx,y,sw,0,st,sc);
}
static void draw_trip(void){
 const int panel_y=MENU_BODY_Y,route_y=panel_y+32,route_step=22;
 fill(0,0,W,H,C_BG);
 focus_rect(4,panel_y,231,267-panel_y,trip_pane==0);focus_rect(244,panel_y,232,267-panel_y,trip_pane==1);
 const char*title=trip_pick_mode==DEST_PICK_ADD_STOP?ui_tr("Add stop"):(trip_pick_mode==DEST_PICK_CHANGE_DEST?ui_tr("Change destination"):ui_tr("Destinations"));
 compact_fit(12,panel_y+4,214,1,title,trip_pane==0?C_CYAN:C_WHITE);label(253,panel_y+4,1,ui_tr("Current route"),trip_pane==1?C_CYAN:C_WHITE,210);
 ui_choice(10,panel_y+20,104,20,trip_tab==0);centered(62,panel_y+23,0,ui_tr("Recents"),trip_tab==0?C_ACTIVE_TEXT:C_MUTED);
 ui_choice(118,panel_y+20,110,20,trip_tab==1);centered(173,panel_y+23,0,ui_tr("Favourites"),trip_tab==1?C_ACTIVE_TEXT:C_MUTED);
 const int rows=9;
 int count=trip_tab?nfaves:nrecents,first=trip_focus>=rows?trip_focus-rows+1:0;
 if(first+rows>count)first=count>rows?count-rows:0;
 if(!count)centered(119,122,1,trip_tab?ui_tr("No favourites yet"):ui_tr("No recent places"),C_MUTED);
 for(int row=0;row<rows&&first+row<count;row++){int i=first+row;trip_item(10,panel_y+48+row*20,218,place_display_name(trip_tab?faves[i].name:recents[i].name),trip_pane==0&&trip_focus==i);}
 ui_scrollbar(230,panel_y+45,179,first,rows,count);
 if(trip.active){int rem=trip.count-trip.active_idx;
  /* Legacy full-screen path also needs a bounded viewport for long trips. */
  int visible=(232-route_y)/route_step-2;if(visible<1)visible=1;if(visible>rem)visible=rem;
  int first=trip_right_focus>=visible?trip_right_focus-visible+1:0;if(first+visible>rem)first=rem-visible;
  for(int r=0;r<visible;r++)trip_target_row(route_y+r*route_step,trip.active_idx+first+r,trip_pane==1&&trip_right_focus==first+r);
  ui_scrollbar(470,route_y-3,visible*route_step,first,visible,rem);
  trip_item(249,route_y+visible*route_step,220,ui_tr("+ Add stop"),trip_pane==1&&trip_right_focus==rem);
  trip_item(249,route_y+(visible+1)*route_step,220,ui_tr("Cancel route"),trip_pane==1&&trip_right_focus==rem+1);
 }else{
  centered(359,110,1,ui_tr("Choose a destination"),C_WHITE);centered(359,132,0,ui_tr("from your saved places"),C_MUTED);
  trip_item(253,198,214,ui_tr("Coordinates"),trip_pane==1);
 }
 if(trip.active&&route_status==R_READY){
  char m[64]="";append_miles(m,sizeof(m),route_path_m());appendstr(m,sizeof(m),"  ~");
  append_eta(m,sizeof(m),sat_add32(traffic_remaining_cost(),atlas_future_cost));compact_fit(253,243,214,0,m,C_MUTED);
 }
}

static int bt_page_focus=0,bt_page_connected_choice=0,bt_page_dialog=-1,bt_page_dialog_choice=0;
static const char* bt_display_name(int idx);
static void bt_page_button(int x,int y,int w,const char*l,int f){ui_choice(x,y,w,22,f);compact_center(x+w/2,y+4,w-12,0,l,ui_choice_text(f));}
static int bt_page_total(void){int ci=bt_connected_idx();return(ci>=0?1:0)+bt_saved_count()+1;}
static void bt_page_clamp(void){int t=bt_page_total();if(t<1)t=1;if(bt_page_focus<0)bt_page_focus=0;if(bt_page_focus>=t)bt_page_focus=t-1;}
static const char* bt_display_name(int idx){if(idx<0||idx>=nbtdev)return ui_tr("Unknown phone");if(bt_name_unknown(btdev[idx].name)){const char*cn=bt_name_cache_get(btdev[idx].addr);if(cn)return cn;if(btdev[idx].connected&&bt_meta_matches(btdev[idx].addr))return btui.device;return ui_tr("Unknown phone");}return btdev[idx].name;}
static const char*bt_status_text(void){if(pref.debug&&reading_present(btui.status))return btui.status;if(starts(btui.status,"BT DAEMON")||starts(btui.status,"BT L2CAP")||starts(btui.status,"BT AUDIO ERR"))return bt_unavailable_label();if(starts(btui.status,"BT PAIR"))return ui_tr("Pairing phone");if(starts(btui.status,"BT WAIT"))return ui_tr("Waiting for phone");return ui_tr("Ready to connect");}
static void draw_bluetooth(void){
  fill(0,0,W,H,C_BG);bt_page_clamp();
  int ci=bt_connected_idx();int conn_selected=(ci>=0&&bt_page_focus==0);
  ui_surface(18,MENU_BODY_Y,444,48,C_PANEL2);
  if(ci>=0){
    label(30,48,0,ui_tr("Connected"),C_MUTED,216);compact_fit(30,66,216,1,bt_display_name(ci),C_WHITE);
    bt_page_button(258,55,92,ui_tr("Disconnect"),conn_selected&&bt_page_connected_choice==0);bt_page_button(358,55,92,ui_tr("Forget"),conn_selected&&bt_page_connected_choice==1);
  }else{
    label(30,48,1,ui_tr("No phone connected"),C_MUTED,410);compact_fit(30,68,410,0,bt_status_text(),C_MUTED);
  }
  label(24,92,0,ui_tr("Saved phones"),C_MUTED,420);
  const int rows=8;
  int base=ci>=0?1:0,sc=bt_saved_count(),lf=bt_page_focus-base,listn=sc+1;
  int first=lf>=rows?lf-rows+1:0;if(first+rows>listn)first=listn>rows?listn-rows:0;
  for(int row=0;row<rows&&first+row<listn;row++){
    int ord=first+row,y=111+row*20,f=bt_page_focus==base+ord;ui_choice(24,y-3,432,19,f);
    if(ord<sc){int di=bt_saved_idx(ord);compact_fit(36,y-2,326,1,di>=0?bt_display_name(di):ui_tr("Unknown phone"),ui_choice_text(f));label(392,y,0,ui_tr("Saved"),f?C_ACTIVE_TEXT:C_MUTED,52);}
    else{label(36,y-2,1,ui_tr("+ Pair new phone"),ui_choice_text(f),326);label(388,y,0,"0000",f?C_ACTIVE_TEXT:C_MUTED,56);}
  }
  ui_scrollbar(466,108,159,first,rows,listn);
}

static int set_focus=0;
static void settingrow(int y,const char*l,const char*v,int f){int vw=width(v,1);if(vw<76)vw=76;if(vw>180)vw=180;int vx=446-vw;ui_choice(18,y-5,444,24,f);compact_fit(30,y-2,vx-42,1,l,ui_choice_text(f));compact_fit(vx,y-2,vw,1,v,f?C_ACTIVE_TEXT:C_MUTED);}
static const char* theme_value(void){if(pref.theme==THEME_DAY)return"DAY";if(pref.theme==THEME_NIGHT)return"NIGHT";return"AUTO";}
enum {SETTING_COUNT=16,SETTING_VISIBLE_ROWS=8,SETTING_ROW_STEP=26};
static int theme_time_part;
static const char* setting_names[]={"Brightness","Theme","Units","Auto zoom","Camera alerts","Clock","Debug log","Language","Day starts","Night starts","Top pill order","Bottom pill order","Learn driving history","Destination suggestions","Intelligent diversions","Clear driving history"};
static const char* units_value(void){return pref.units==1?ui_tr("Metric"):pref.units==2?ui_tr("Imperial"):ui_tr("Auto");}
static const char* setting_value(int row,char b[96]){
 switch(row){
 case 0:
  b[0]=0;append_int(b,96,pref.bright);appendstr(b,96,"%");return b;
 case 1:return pref.theme==THEME_DAY?ui_tr("Day"):pref.theme==THEME_NIGHT?ui_tr("Night"):ui_tr("Auto");
 case 2:return units_value();
 case 3:return ui_tr(pref.autozoom?"On":"Off");
 case 4:return hw_bluetooth_enabled&&pref.cameras==2?ui_tr("Phone audio"):pref.cameras?ui_tr("Visual"):ui_tr("Off");
 case 5:{
  copystr(b,96,pref.clock_auto?ui_tr("Auto"):"UTC");appendstr(b,96," ");
  if(ui_language_rtl())appendstr(b,96,"\xe2\x81\xa6");
  int mins=pref.hoff*60+pref.moff;
  appendstr(b,96,mins<0?"-":"+");if(mins<0)mins=-mins;
  append_int(b,96,mins/60);appendstr(b,96,":");if(mins%60<10)appendstr(b,96,"0");
  append_int(b,96,mins%60);if(ui_language_rtl())appendstr(b,96,"\xe2\x81\xa9");return b;
 }
 case 6:return ui_tr(pref.debug?"On":"Off");
 case 10:return ui_tr(pref.top_swap?"Clock left":"Status left");
 case 11:return ui_tr(pref.bottom_swap?"Direction left":"Speed left");
  case 12:return ui_tr(pref.learn_history?"On":"Off");
  case 13:return ui_tr(pref.destination_predictions?"On":"Off");
  case 14:return ui_tr(pref.diversions?"On":"Off");
  case 15:copystr(b,96,ui_tr("Journeys: "));append_int(b,96,(int)mob.n);return b;
 case 8:case 9:{
  int minute=row==8?pref.day_start:pref.night_start;
  b[0]='0'+minute/60/10;b[1]='0'+minute/60%10;b[2]=':';
  b[3]='0'+minute%60/10;b[4]='0'+minute%60%10;b[5]=0;
  return b;
 }
 default:return ui_language_name();
 }
}
static void draw_settings(void){
 fill(0,0,W,H,C_BG);
 int first=set_focus>=SETTING_VISIBLE_ROWS?set_focus-SETTING_VISIBLE_ROWS+1:0;
 for(int row=0;row<SETTING_VISIBLE_ROWS;row++){
  int i=first+row,y=MENU_BODY_Y+5+row*SETTING_ROW_STEP;
  char b[96];const char*value=setting_value(i,b);
  settingrow(y,ui_tr(setting_names[i]),value,set_focus==i);
  if(i==set_focus&&(i==8||i==9)){
   char prefix[4]={b[0],b[1],':',0},part[3]={b[theme_time_part?3:0],b[theme_time_part?4:1],0};
   int x=446-76+(theme_time_part?width(prefix,1):0);
   round_rect(x,y+16,width(part,1),1,0,C_ACTIVE_TEXT,15);
  }
 }
 ui_scrollbar(468,MENU_BODY_Y,SETTING_VISIBLE_ROWS*SETTING_ROW_STEP-2,first,SETTING_VISIBLE_ROWS,SETTING_COUNT);
 centered(240,254,0,"ClioPlay " CLIOPLAY_VERSION,C_MUTED);
}
static void settings_key(int code){
 if(code==K_UP){if(set_focus>0)set_focus--;theme_time_part=0;return;}
 if(code==K_DOWN){if(set_focus<SETTING_COUNT-1)set_focus++;theme_time_part=0;return;}
 if(code==K_ENTER&&(set_focus==8||set_focus==9)){theme_time_part=!theme_time_part;return;}
 if(code==K_ENTER&&set_focus==5){
  pref.clock_auto=!pref.clock_auto;clock_next_ms=0;clock_service();
 }else if(code==K_ENTER&&set_focus==7)pref.language=(pref.language+1)%UI_LANGUAGE_COUNT;
 else if(code==K_LEFT||code==K_RIGHT){
  int d=code==K_RIGHT?1:-1;
  switch(set_focus){
  case 0:{
   int next=pref.bright+d*10;if(next<10)next=10;if(next>100)next=100;
   if(next==pref.bright)return;
   pref.bright=next;apply_brightness();break;
  }
  case 1:
   pref.theme+=d;if(pref.theme<THEME_DAY)pref.theme=THEME_AUTO;if(pref.theme>THEME_AUTO)pref.theme=THEME_DAY;
   refresh_theme();break;
  case 2:pref.units=(pref.units+d+3)%3;break;
  case 3:
   if(pref.autozoom){manual_zoom=active_zoom();pref.autozoom=0;}
   else{pref.autozoom=1;auto_zoom_level=-1;}
   zoom_override=-1;break;
  case 4:pref.cameras=(pref.cameras+d+(hw_bluetooth_enabled?3:2))%(hw_bluetooth_enabled?3:2);break;
  case 5:{
   pref.clock_auto=0;int m=pref.hoff*60+pref.moff+d*15;
   if(m< -720)m=840;if(m>840)m=-720;pref.hoff=m/60;pref.moff=m%60;break;
  }
  case 6:pref.debug=!pref.debug;debug_state_write();if(!pref.debug)remove_stale_logs();break;
  case 7:pref.language=(pref.language+d+UI_LANGUAGE_COUNT)%UI_LANGUAGE_COUNT;break;
  case 10:pref.top_swap=!pref.top_swap;break;
  case 11:pref.bottom_swap=!pref.bottom_swap;break;
   case 12:pref.learn_history=!pref.learn_history;if(!pref.learn_history){mob_stop_recording();traffic_invalidate();traffic_learning_reset();}break;
   case 13:pref.destination_predictions=!pref.destination_predictions;if(!pref.destination_predictions)mob_suggest_valid=0;mob_predict_ms=0;break;
   case 14:pref.diversions=!pref.diversions;traffic_invalidate();break;
   case 15:return;
  case 8:case 9:{
   int*value=set_focus==8?&pref.day_start:&pref.night_start;
   int other=set_focus==8?pref.night_start:pref.day_start;
   int step=theme_time_part?1:60;
   int next=(*value+d*step+1440)%1440;
   if(next==other)next=(next+d*step+1440)%1440;
   *value=next;refresh_theme();break;
  }
  }
 }else return;
 prefs_mark_dirty();
}

static int modal_action_count(void){if(modal==MOD_RECENT)return trip.active?3:2;if(modal==MOD_FAVE)return trip.active?4:3;if(modal==MOD_TARGET){if(modal_index>=0&&modal_index<trip.count-1)return 3;return 2;}if(modal==MOD_SOURCE_PICK)return 4;return 0;}
#include "popups.h"
#include "menu_keyboard.h"
#include "menu_layout.h"
#include "menu_sheet.h"

static void draw(void){
 u32 now=hud_now_ms();freecam_orientation_anim_update(now);clock_service();region_service();camera_service();refresh_theme();
 hud_prepare(active_zoom(),now,1);hud_driver_prepare(now);menu_prepare(now);draw_map_background();
 hud_glass_frame_begin();menu_draw(hud_now_ms());draw_modal();hud_glass_frame_end();boot_fade_apply(hud_now_ms());fb_flush();
}

struct input_event16 {s32 sec,usec;u16 type,code;s32 value;};
#define EV_KEY 1
#define INPUT_MAX 8
#define EAGAIN_NEG (-11)
static int infd[INPUT_MAX]={-1,-1,-1,-1,-1,-1,-1,-1};
static int input_lock=-1;
static int known_remote_key(int code){return code==K_ESC||code==K_BACK||code==K_SR||code==K_ENTER||code==K_UP||code==K_LEFT||code==K_RIGHT||code==K_DOWN||code==K_MAP||code==K_MENU||code==K_ROTCW||code==K_ROTCCW;}
static void input_log2(const char*tag,int ev,int code){if(!pref.debug)return;char b[72];b[0]=0;appendstr(b,sizeof(b),tag);appendstr(b,sizeof(b)," event");append_int(b,sizeof(b),ev);if(code>=0){appendstr(b,sizeof(b)," code=");append_int(b,sizeof(b),code);}log_append(KEY_LOG,b);}
static void scan_inputs(void){char p[]="/dev/input/event0";for(int i=0;i<INPUT_MAX;i++){if(infd[i]>=0)continue;p[16]=(char)('0'+i);long fd=s_open(p,O_RDONLY_|O_NONBLOCK_,0);if(fd>=0){infd[i]=(int)fd;input_log2("INPUT OPEN",i,-1);}}}
static void open_inputs(void){for(int i=0;i<INPUT_MAX;i++)infd[i]=-1;input_lock=-1;if(pref.debug)log_append(KEY_LOG,"--- CLIOPLAY GLOBAL " CLIOPLAY_VERSION " MEDIA INPUT ---");scan_inputs();}
static void keydiag(int code){if(!pref.debug)return;char b[48]="KEY code=";append_int(b,sizeof(b),code);log_append(KEY_LOG,b);}
static void key_handle_state(int code){render_background_dirty=1;keydiag(code);
/* Arrival/resume prompts cannot be silently escaped with MAP/BACK. */
if(code==K_MAP&&screen!=SCR_MAP&&modal!=MOD_RESUME_TRIP&&modal!=MOD_ARRIVED_STOP&&modal!=MOD_ARRIVED_DEST){coordinates_active=0;modal=MOD_NONE;dest_pick_action=DEST_PICK_NONE;trip_pick_mode=DEST_PICK_NONE;screen=SCR_MAP;recenter_map();return;}
if(modal!=MOD_NONE){
  int yesno=modal==MOD_ROUTE_POINT||modal==MOD_CANCEL_ROUTE||modal==MOD_REMOVE_STOP||modal==MOD_RESUME_TRIP;
  if(code==K_BACK&&modal!=MOD_ARRIVED_STOP&&modal!=MOD_ARRIVED_DEST&&modal!=MOD_RESUME_TRIP){if(modal==MOD_SOURCE_PICK)dest_pick_action=DEST_PICK_NONE;modal=MOD_NONE;return;}
  if(yesno&&(code==K_LEFT||code==K_RIGHT)){modal_choice=modal_choice?0:1;return;}
  if(!yesno&&modal!=MOD_ARRIVED_STOP&&modal!=MOD_ARRIVED_DEST){int n=modal_action_count();if((code==K_UP||code==K_LEFT)&&modal_choice>0){modal_choice--;return;}if((code==K_DOWN||code==K_RIGHT)&&modal_choice+1<n){modal_choice++;return;}}
  if(code==K_ENTER){
    if(modal==MOD_ROUTE_POINT){
      if(modal_choice==0){
        int accepted=1;
        if(pending_point_stop&&trip.active){
          accepted=trip_add_stop("MAP POINT",pending_lat,pending_lon);
          if(accepted)add_recent("MAP POINT",pending_lat,pending_lon);else notice_fail(ui_tr("Stop not added"));
        }else set_destination("MAP POINT",pending_lat,pending_lon);
        if(accepted){
          screen=SCR_MAP;trip_pane=1;trip_right_focus=0;leave_freecam();
          if(hud_navigation.kind==HUD_EXPLORE)hud_navigation.kind=HUD_NONE;
        }
      }
      modal=MOD_NONE;return;
    }
    if(modal==MOD_CANCEL_ROUTE){if(modal_choice==0&&trip.active){clear_trip();notice_success(ui_tr("Route cancelled"));}modal=MOD_NONE;return;}
    if(modal==MOD_REMOVE_STOP){if(modal_choice==0)trip_remove_stop(modal_index);modal=MOD_NONE;return;}
    if(modal==MOD_RESUME_TRIP){if(modal_choice==0){trip.paused=0;trip_need_route=1;trip_route_defer=1;notice_success(ui_tr("Trip resumed"));}else if(trip.active){clear_trip();notice_success(ui_tr("Route cancelled"));}modal=MOD_NONE;return;}
    if(modal==MOD_ARRIVED_STOP){arrival_modal_deadline=0;modal=MOD_NONE;trip_continue_after_stop();screen=SCR_MAP;recenter_map();return;}
    if(modal==MOD_ARRIVED_DEST){arrival_modal_deadline=0;modal=MOD_NONE;clear_trip();screen=SCR_MAP;recenter_map();return;}
    if(modal==MOD_SOURCE_PICK){if(modal_choice==0||modal_choice==1){trip_pick_mode=dest_pick_action;trip_tab=modal_choice;trip_pane=0;trip_focus=0;dest_pick_action=DEST_PICK_NONE;modal=MOD_NONE;screen=SCR_TRIP;}else if(modal_choice==2){int action=dest_pick_action;dest_pick_action=DEST_PICK_NONE;coordinates_open(action);}else{dest_pick_action=DEST_PICK_NONE;modal=MOD_NONE;}return;}

    if(modal==MOD_RECENT&&modal_index>=0&&modal_index<nrecents){struct SavedPlace*p=&recents[modal_index];if(trip.active){if(modal_choice==0){if(trip_add_stop(p->name,p->lat,p->lon))add_recent(p->name,p->lat,p->lon);}else if(modal_choice==1){set_destination(p->name,p->lat,p->lon);screen=SCR_TRIP;trip_pane=1;trip_right_focus=0;}else add_fave(p);}else{if(modal_choice==0){set_destination(p->name,p->lat,p->lon);screen=SCR_TRIP;trip_pane=1;trip_right_focus=0;}else add_fave(p);}modal=MOD_NONE;return;}
    if(modal==MOD_FAVE&&modal_index>=0&&modal_index<nfaves){struct SavedPlace p=faves[modal_index];if(trip.active){if(modal_choice==0){if(trip_add_stop(p.name,p.lat,p.lon))add_recent(p.name,p.lat,p.lon);}else if(modal_choice==1){set_destination(p.name,p.lat,p.lon);screen=SCR_TRIP;trip_pane=1;trip_right_focus=0;}else if(modal_choice==2){keyboard_open_fave_rename(modal_index);screen=SCR_KEY;modal=MOD_NONE;return;}else remove_fave(modal_index);}else{if(modal_choice==0){set_destination(p.name,p.lat,p.lon);screen=SCR_TRIP;trip_pane=1;trip_right_focus=0;}else if(modal_choice==1){keyboard_open_fave_rename(modal_index);screen=SCR_KEY;modal=MOD_NONE;return;}else remove_fave(modal_index);}modal=MOD_NONE;return;}
    if(modal==MOD_TARGET&&modal_index>=trip.active_idx&&modal_index<trip.count){if(modal_index<trip.count-1){if(modal_choice==0){modal=MOD_REMOVE_STOP;modal_choice=1;return;}if(modal_choice==1)trip_move_stop(modal_index,-1);else trip_move_stop(modal_index,1);}else{if(modal_choice==0){dest_pick_action=DEST_PICK_CHANGE_DEST;modal=MOD_SOURCE_PICK;modal_choice=0;return;}else{int fi=fave_contains(trip.target[modal_index].lat,trip.target[modal_index].lon);if(fi>=0)remove_fave(fi);else add_fave_or_name(trip.target[modal_index].name,trip.target[modal_index].lat,trip.target[modal_index].lon);}}modal=MOD_NONE;return;}
  }
  return;
}
if(code==K_MENU){coordinates_active=0;bt_control_mode=0;trip_pick_mode=DEST_PICK_NONE;screen=SCR_MENU;return;}
if(screen==SCR_BT&&bt_page_dialog>=0&&code==K_BACK){bt_page_dialog=-1;return;}
if(screen==SCR_KEY&&kbd_picker&&code==K_BACK){keyboard_handle(code);return;}
if(screen==SCR_COORDINATES){coordinates_handle(code);return;}
if(screen==SCR_KEY&&kbd_action==KBD_NAME_COORDINATES&&code==K_BACK){coordinates_save_name();coordinates.step=COORD_LON;coordinates.error=0;coordinates.focus=0;screen=SCR_COORDINATES;return;}
if(code==K_BACK){
  if(screen==SCR_TRIP&&trip_pick_mode!=DEST_PICK_NONE){trip_pick_mode=DEST_PICK_NONE;trip_pane=1;trip_right_focus=0;return;}
  if(screen==SCR_MAP&&!freecam&&(bt_control_mode||bt_controls_available())){
    bt_control_mode=!bt_control_mode;hud_diversion_invited=0;bt_transport_press.kind=0;return;
  }
  if(screen==SCR_KEY){screen=SCR_TRIP;return;}if(screen==SCR_MENU)screen=SCR_MAP;else if(screen!=SCR_MAP)screen=SCR_MENU;return;
}
if(screen==SCR_KEY){keyboard_handle(code);return;}
if(screen==SCR_MAP){
  /* BACK enters this explicit TomTom-remote Bluetooth context.  It is kept
   * separate from Renault radio/stalk input: K_UP/K_DOWN remain navigation
   * directions and are deliberately ignored while BT control is active. */
  if(bt_control_mode&&!bt_controls_available())bt_control_mode=0;
  if(bt_control_mode){
    if(code==K_LEFT){bt_transport_command(BT_TRANSPORT_PREVIOUS);return;}
    if(code==K_RIGHT){bt_transport_command(BT_TRANSPORT_NEXT);return;}
    if(code==K_ENTER){bt_transport_command(BT_TRANSPORT_PLAY_PAUSE);return;}
    if(code==K_UP||code==K_DOWN)return;
  }
  if(code==K_MAP){bt_control_mode=0;
    if(!freecam&&pref.autozoom&&zoom_override>=0){zoom_override=-1;auto_zoom_level=-1;hud_pulse(&hud_navigation,HUD_AUTO_ZOOM,1800);return;}
    int was=freecam;toggle_freecam();if(was!=freecam)hud_pulse(&hud_navigation,freecam?HUD_EXPLORE:(pref.autozoom?HUD_AUTO_ZOOM:HUD_MANUAL_ZOOM),1800);return;}
  if(code==K_ENTER){if(freecam){pending_lat=cam_lat;pending_lon=cam_lon;pending_point_stop=trip.active?1:0;modal=MOD_ROUTE_POINT;modal_choice=0;}else hud_pulse(&hud_navigation,route_status==R_READY?HUD_JOURNEY:HUD_HEADING,route_status==R_READY?4200:2300);return;}
  if(code==K_UP||code==K_DOWN||code==K_LEFT||code==K_RIGHT){int was=freecam;pan_freecam(code);if(!was&&freecam)hud_pulse(&hud_navigation,HUD_EXPLORE,1800);return;}
  if(code==K_ESC||code==K_SR){
    int z=active_zoom(),up=code==K_SR,lo=mapok?(int)mh.minlvl:0;
    int hi=freecam?MAX_EXPLORE_ZOOM:(mapok?(int)mh.maxlvl:6);if(!freecam&&hi>6)hi=6;
    int next=z+(up?1:-1);
    if(next>=lo&&next<=hi){
      if(freecam)freecam_zoom=next;
      else if(pref.autozoom)zoom_override=next;
      else{manual_zoom=next;prefs_mark_dirty();}
    }
    if(!freecam&&next>=lo&&next<=hi)hud_pulse(&hud_navigation,HUD_MANUAL_ZOOM,1800);return;
  }
  return;}
if(screen==SCR_MENU){if(code==K_UP&&menu_focus>0)menu_focus--;else if(code==K_DOWN&&menu_focus<2)menu_focus++;else if(code==K_ENTER)screen=menu_focus==0?SCR_TRIP:(menu_focus==1?SCR_BT:SCR_SET);return;}
if(screen==SCR_TRIP){
  if(!trip.active&&(trip_right_focus<0||trip_right_focus>1))trip_right_focus=0;
  if(code==K_ESC){if(trip_pick_mode!=DEST_PICK_NONE){trip_pick_mode=DEST_PICK_NONE;trip_pane=1;trip_right_focus=0;}else trip_pane=0;return;}if(code==K_SR){if(trip_pick_mode!=DEST_PICK_NONE){trip_pick_mode=DEST_PICK_NONE;}trip_pane=1;trip_right_focus=0;return;}
  if(trip_pane==0){int maxf=trip_tab==0?(nrecents?nrecents-1:0):(nfaves?nfaves-1:0);if(code==K_LEFT||code==K_RIGHT){trip_tab=trip_tab?0:1;trip_focus=0;return;}if(code==K_UP&&trip_focus>0)trip_focus--;else if(code==K_DOWN&&trip_focus<maxf)trip_focus++;else if(code==K_ENTER){struct SavedPlace p;int ok=0;if(trip_tab==0&&trip_focus<nrecents){p=recents[trip_focus];ok=1;}else if(trip_tab==1&&trip_focus<nfaves){p=faves[trip_focus];ok=1;}if(ok&&trip_pick_mode!=DEST_PICK_NONE){if(trip_pick_mode==DEST_PICK_ADD_STOP){if(trip_add_stop(p.name,p.lat,p.lon)){add_recent(p.name,p.lat,p.lon);}else notice_fail(ui_tr("Stop not added"));}else{trip_change_destination(p.name,p.lat,p.lon);}trip_pick_mode=DEST_PICK_NONE;trip_pane=1;trip_right_focus=0;return;}if(ok){modal=trip_tab==0?MOD_RECENT:MOD_FAVE;modal_index=trip_focus;modal_choice=0;}return;}}
  else if(trip.active){int rem=trip.count-trip.active_idx,total=rem+2;if(code==K_UP&&trip_right_focus>0)trip_right_focus--;else if(code==K_DOWN&&trip_right_focus+1<total)trip_right_focus++;else if(code==K_ENTER){if(trip_right_focus<rem){modal=MOD_TARGET;modal_index=trip.active_idx+trip_right_focus;modal_choice=0;}else if(trip_right_focus==rem){if(trip.count<TRIP_MAX_TARGETS){dest_pick_action=DEST_PICK_ADD_STOP;modal=MOD_SOURCE_PICK;modal_choice=0;}}else{modal=MOD_CANCEL_ROUTE;modal_choice=1;}return;}}
  else{trip_right_focus=0;if(code==K_ENTER)coordinates_open(DEST_PICK_NONE);}
  return;}
if(screen==SCR_BT){if(code==K_ESC){bt_page_dialog=-1;return;}if(bt_page_dialog>=0){if(code==K_LEFT){bt_page_dialog_choice=0;return;}if(code==K_RIGHT){bt_page_dialog_choice=1;return;}if(code==K_ENTER){if(bt_page_dialog>=nbtdev){bt_page_dialog=-1;return;}if(bt_page_dialog_choice==0)bt_command_arg("CONNECT",btdev[bt_page_dialog].addr);else bt_forget_device(btdev[bt_page_dialog].addr);bt_page_dialog=-1;return;}return;}bt_page_clamp();int ci=bt_connected_idx(),base=ci>=0?1:0,sc=bt_saved_count(),total=base+sc+1;if(code==K_UP&&bt_page_focus>0){bt_page_focus--;return;}if(code==K_DOWN&&bt_page_focus+1<total){bt_page_focus++;return;}if(ci>=0&&bt_page_focus==0){if(code==K_LEFT){bt_page_connected_choice=0;return;}if(code==K_RIGHT){bt_page_connected_choice=1;return;}if(code==K_ENTER){if(bt_page_connected_choice==0)bt_command_arg("DISCONNECT",btdev[ci].addr);else bt_forget_device(btdev[ci].addr);return;}return;}if(code==K_ENTER){int ord=bt_page_focus-base;if(ord>=0&&ord<sc){int di=bt_saved_idx(ord);if(di>=0){bt_page_dialog=di;bt_page_dialog_choice=0;}return;}if(ord==sc){bt_command("PAIR");return;}}return;}
if(screen==SCR_SET)settings_key(code);}


/* Popup-only input changes presentation, not the underlying map/menu image. */
static int control_key(int code){
 if(code!=K_ROTCW&&code!=K_ROTCCW)return code;
 int forward=code==K_ROTCW;
 if(modal!=MOD_NONE){int choice=modal==MOD_ROUTE_POINT||modal==MOD_CANCEL_ROUTE||modal==MOD_REMOVE_STOP||modal==MOD_RESUME_TRIP;return choice?(forward?K_RIGHT:K_LEFT):(forward?K_DOWN:K_UP);}
 if(screen==SCR_KEY&&!kbd_picker){
  int next=kbd_focus+(forward?1:-1);while(next>=0&&next<48&&!keyboard_valid_focus(next))next+=forward?1:-1;
  if(next>=0&&next<48){kbd_focus=next;kbd_column=keyboard_col(next);render_background_dirty=1;}return 0;
 }
 if(screen==SCR_COORDINATES&&coordinates.step!=COORD_REVIEW){
  coordinate_move(code);render_background_dirty=1;return 0;
 }
 if(screen==SCR_MAP)return bt_control_mode?(forward?K_RIGHT:K_LEFT):(forward?K_SR:K_ESC);
 if(screen==SCR_BT&&(bt_page_dialog>=0||(bt_connected_idx()>=0&&bt_page_focus==0)))return forward?K_RIGHT:K_LEFT;
 return forward?K_DOWN:K_UP;
}
static void key_handle(int code){
 boot_fade_cancel();
 if(code==K_CONSOLE_THEME){pref.theme=(pref.theme+1)%3;prefs_mark_dirty();refresh_theme();render_background_dirty=1;return;}
 if(code==K_CONSOLE_INFO){if(screen==SCR_MAP&&modal==MOD_NONE)hud_pulse(&hud_navigation,route_status==R_READY?HUD_JOURNEY:HUD_HEADING,route_status==R_READY?4200:2300);return;}
 if(code==K_CONSOLE_TRIP){if(modal!=MOD_NONE)return;coordinates_active=bt_control_mode=0;trip_pick_mode=DEST_PICK_NONE;menu_open(P_TRIP,trip.active_idx,hud_now_ms());return;}
 menu_sync(hud_now_ms());
 if(modal==MOD_NONE&&(code==K_MENU||screen!=SCR_MAP)){
  keydiag(code);if(code==K_MENU&&screen==SCR_MAP)menu_open(P_HOME,0,hud_now_ms());else menu_dispatch(code,hud_now_ms());return;
 }
 if(code==K_ENTER&&hud_diversion_shortcut_ready()){
  hud_diversion_invited=0;bt_control_mode=0;menu_open_diversion(1,hud_now_ms());return;
 }
 code=control_key(code);if(!code)return;
 int dirty=render_background_dirty,old_screen=screen,old_popup=popup_requested_key();
 key_handle_state(code);
 int new_popup=popup_requested_key();
 /* Opening a keyboard picker can commit a live Pinyin composition. */
 int popup_only=old_screen==screen&&((!old_popup&&new_popup&&screen!=SCR_KEY)||
  (old_popup&&(code==K_LEFT||code==K_RIGHT||code==K_UP||code==K_DOWN||code==K_BACK||code==K_ESC)));
 if(popup_only)render_background_dirty=dirty;
}


static int csw_live(void);
static void poll_keys(void){for(int i=0;i<INPUT_MAX;i++){if(infd[i]<0)continue;for(;;){struct input_event16 e;long n=control_read(infd[i],&e,sizeof(e));if(n==(long)sizeof(e)){if(e.type==EV_KEY&&e.value==1&&known_remote_key(e.code)){if(e.code==K_ROTCW||e.code==K_ROTCCW){if(!csw_live())key_handle(e.code);continue;}if(input_lock<0){input_lock=i;input_log2("INPUT LOCK",i,e.code);}if(input_lock==i)key_handle(e.code);}continue;}if(n==0||(n<0&&n!=EAGAIN_NEG)){s_close(infd[i]);infd[i]=-1;if(input_lock==i){input_log2("INPUT LOST",i,-1);input_lock=-1;}}break;}}}

/* Console protocol/mapping adapted from bobczan/Renatka (MIT), PR #2,
 * d7701fbe60566d3afa8c556ebf7fe7da3a2fcf6e; checked against Navcore's
 * CswParser. No helper process or injected evdev events are involved. */
#define CSW_RECORD 28
#define CSW_QUEUE 32
static struct {
 int fd,verbose,active;u8 partial[CSW_RECORD],used,head,queued,seen[9],pressed,dir;
 u16 map[17];struct {u16 key;u8 count;}queue[CSW_QUEUE];
 u32 button_at[9],direction_at,press_at,retry_at,config_at,config_hash,records,invalid,dropped;
} csw={.fd=-1};
/* An open idle FIFO is not evidence of a console. Only valid received records
 * take precedence over a duplicate evdev knob path. */
static int csw_live(void){return csw.fd>=0&&csw.active;}
static u32 control_u32(const u8*b){return (u32)b[0]|(u32)b[1]<<8|(u32)b[2]<<16|(u32)b[3]<<24;}
static void control_number(char*b,unsigned cap,int value){
 if(value==(s32)0x80000000u)appendstr(b,cap,"-2147483648");else append_int(b,cap,value);
}
static void control_log(const char*tag,int source,int value){
 if(!pref.debug)return;
 long fd=s_open(CSW_LOG_PATH,O_WRONLY_|O_CREAT_|O_APPEND_,0666);if(fd<0)return;
 if(s_lseek((int)fd,0,SEEK_END_)<98304){
  char b[128];b[0]=0;appendstr(b,sizeof(b),tag);appendstr(b,sizeof(b)," src=");control_number(b,sizeof(b),source);
  appendstr(b,sizeof(b)," value=");control_number(b,sizeof(b),value);appendstr(b,sizeof(b),"\n");s_write((int)fd,b,slen(b));
 }s_close((int)fd);
}
static void csw_defaults(u16*map){
 const u16 defaults[17]={K_ESC,K_BACK,K_SR,K_CONSOLE_INFO,K_MENU,K_CONSOLE_THEME,K_CONSOLE_INFO,K_CONSOLE_TRIP,K_MAP,
  K_UP,K_DOWN,K_RIGHT,K_LEFT,K_ROTCCW,K_ROTCW,K_ENTER,0};
 for(int i=0;i<17;i++)map[i]=defaults[i];
}
static int csw_action(const char*name){
 static const char*names[]={"UP","DOWN","LEFT","RIGHT","OK","ENTER","BACK","ESC","ZOOMOUT","ZOOMIN","SR","MAP","MENU","ROTNEXT","ROTPREV","KNOB+","KNOB-","INFO","TRIP","THEME","NONE","IGNORE"};
 static const u16 codes[]={K_UP,K_DOWN,K_LEFT,K_RIGHT,K_ENTER,K_ENTER,K_BACK,K_ESC,K_ESC,K_SR,K_SR,K_MAP,K_MENU,K_ROTCW,K_ROTCCW,K_ROTCW,K_ROTCCW,K_CONSOLE_INFO,K_CONSOLE_TRIP,K_CONSOLE_THEME,0,0};
 for(unsigned i=0;i<sizeof(codes)/sizeof(codes[0]);i++)if(streq(name,names[i]))return codes[i];return -1;
}
/* Parse into a temporary map. A bad/truncated edit cannot half-apply bindings. */
static int csw_parse_config(char*b,u16*map,int*verbose){
 csw_defaults(map);*verbose=0;char*line=b;
 while(*line){
  char*end=line;while(*end&&*end!='\n'&&*end!='\r')end++;char saved=*end;*end=0;
  char*p=line;while(*p==' '||*p=='\t')p++;
  if(*p&&*p!='#'&&*p!=';'&&*p!='['){
   char name[24],value[24];unsigned n=0;
   while(*p&&*p!='='&&*p!=' '&&*p!='\t'){if(n+1>=sizeof(name))return 0;name[n++]=upc(*p++);}name[n]=0;
   while(*p==' '||*p=='\t')p++;if(*p++!='=')return 0;while(*p==' '||*p=='\t')p++;
   n=0;while(*p&&*p!=' '&&*p!='\t'&&*p!='#'&&*p!=';'){if(n+1>=sizeof(value))return 0;value[n++]=upc(*p++);}value[n]=0;
   while(*p==' '||*p=='\t')p++;if(*p&&*p!='#'&&*p!=';')return 0;
   int slot=-1;
   if(starts(name,"BTN")&&name[3]>='1'&&name[3]<='9'&&!name[4])slot=name[3]-'1';
   else if(starts(name,"DIR")&&name[3]>='1'&&name[3]<='4'&&!name[4])slot=9+name[3]-'1';
   else if(streq(name,"ROT1"))slot=13;else if(streq(name,"ROT2"))slot=14;
   else if(streq(name,"PUSH"))slot=15;else if(streq(name,"LONGPUSH"))slot=16;
   else if(streq(name,"VERBOSE")){if(!streq(value,"0")&&!streq(value,"1"))return 0;*verbose=value[0]=='1';}
   else return 0;
   if(slot>=0){int key=csw_action(value);if(key<0)return 0;map[slot]=(u16)key;}
  }
  if(!saved)break;line=end+1;
 }return 1;
}
static void csw_config_load(void){
 char b[2049];int fd=(int)s_open(CSW_MAP_PATH,O_RDONLY_|O_NONBLOCK_,0);
 if(fd<0)fd=(int)s_open(CSW_MAP_ALT,O_RDONLY_|O_NONBLOCK_,0);
 long n=0;if(fd>=0){n=control_read(fd,b,sizeof(b));s_close(fd);if(n<0||n>2048){control_log("CSW CONFIG TOO LARGE/UNREADABLE",0,(int)n);return;}}
 u32 hash=2166136261u;for(long i=0;i<n;i++){if(!b[i]){control_log("CSW CONFIG CONTAINS NUL",0,0);return;}hash=(hash^(u8)b[i])*16777619u;}
 if(hash==csw.config_hash)return;
 b[n]=0;u16 map[17];int verbose;
 if(!csw_parse_config(b,map,&verbose)){control_log("CSW CONFIG INVALID; KEEPING BINDINGS",0,0);return;}
 for(int i=0;i<17;i++)csw.map[i]=map[i];csw.verbose=verbose;csw.config_hash=hash;
 csw.head=csw.queued=0;control_log("CSW CONFIG LOADED",0,0);
}
static void csw_enqueue(int key,unsigned count){
 if(key<=0||!count)return;
 if(csw.queued==CSW_QUEUE){csw.dropped++;return;}
 unsigned at=(csw.head+csw.queued)%CSW_QUEUE;csw.queue[at].key=(u16)key;csw.queue[at].count=(u8)count;csw.queued++;
}
static int csw_next(void){
 if(!csw.queued)return 0;int key=csw.queue[csw.head].key;
 if(!--csw.queue[csw.head].count){csw.head=(csw.head+1)%CSW_QUEUE;csw.queued--;}return key;
}
static void csw_record(const u8*b,u32 now){
 u32 src=control_u32(b);csw.records++;
 if(csw.verbose){
  control_log("CSW SOURCE",(int)src,0);
  if(!src){for(int i=1;i<6;i++)control_log("CSW FIELD",i,(int)control_u32(b+i*4));control_log("CSW STEPS",0,b[24]);}
 }
 if(src>=1&&src<=9){
  /* No unrelated button fields are initialized reliably by the producer. */
  csw.active=1;
  unsigned i=src-1;int duplicate=csw.seen[i]&&now-csw.button_at[i]<250u;csw.button_at[i]=now;csw.seen[i]=1;if(duplicate)return;csw_enqueue(csw.map[i],1);return;
 }
 if(src){csw.invalid++;control_log("CSW UNKNOWN SOURCE",(int)src,0);return;}
 u32 direction=control_u32(b+4),rotation=control_u32(b+8),pressed=control_u32(b+16);
 if(direction<1||direction>5||rotation<1||rotation>3||pressed>1){csw.invalid++;return;}
 csw.active=1;
 if(direction>=1&&direction<=4){
  if(direction!=csw.dir||now-csw.direction_at>=150u){csw_enqueue(csw.map[9+direction-1],1);csw.direction_at=now;}
  csw.dir=(u8)direction;
 }else if(direction==5)csw.dir=0;
 if(rotation==1||rotation==2){unsigned count=b[24];if(count>=1&&count<=14)csw_enqueue(csw.map[rotation==1?13:14],count);else csw.invalid++;}
 /* Neutral direction/rotation is NOT an OK press. Use the real pressed
  * edge; elapsed UI/rendering time must never manufacture a second OK. */
 if(pressed==1){
  if(!csw.pressed){
   int key=csw.map[15];if(control_u32(b+20)==2&&csw.map[16])key=csw.map[16];
   csw_enqueue(key,1);
  }
  csw.pressed=1;csw.press_at=now;
 }else if(pressed==0)csw.pressed=0;
}
static void csw_feed(const u8*b,unsigned n,u32 now){
 for(unsigned i=0;i<n;i++){csw.partial[csw.used++]=b[i];if(csw.used==CSW_RECORD){csw_record(csw.partial,now);csw.used=0;}}
}
static void csw_init(void){
 csw.active=0;csw_defaults(csw.map);
 csw.fd=control_fifo(CSW_FIFO_PATH);csw.retry_at=s_now_ms();csw.config_at=csw.retry_at;csw_config_load();
}
static void poll_csw(void){
 u32 now=s_now_ms();
 if(now-csw.config_at>=5000u){csw.config_at=now;csw_config_load();}
 if(csw.fd<0){if(now-csw.retry_at<1000u)return;csw.retry_at=now;csw.fd=control_fifo(CSW_FIFO_PATH);if(csw.fd<0){control_log("CSW FIFO UNAVAILABLE",0,0);return;}}
 /* At most 16 input actions and two bounded reads per slice. Backpressure
  * keeps detents queued in the FIFO instead of dropping a rapid rotation. */
 unsigned budget=16;int key;
 while(budget&&((key=csw_next())!=0)){key_handle(key);budget--;}
 for(unsigned attempt=0;attempt<2&&csw.queued<=8;attempt++){
  u8 b[CSW_RECORD*8];long n=control_read(csw.fd,b,sizeof(b));
  if(n>0)csw_feed(b,(unsigned)n,now);else{if(n==0)csw.used=0;break;}
 }
 while(budget&&((key=csw_next())!=0)){key_handle(key);budget--;}
}

/* VehicleStatus alternative to Focus. Endpoint facts from Renatka PR #2;
 * fixed storage, nonblocking sequenced packets, no replies or raw/VIN logs.
 * Available hardware supplies records at runtime; no SD-card marker is needed. */
static const unsigned short vehicle_ports[9]={3400,3401,3700,3800,4000,4001,5600,5700,5800};
static struct {int enabled,listen[9],client[16];u8 port[16];u32 retry_at;} vehicle;
static void vehicle_path(int port,char*out){
 copystr(out,108,VSTAT_SOCKET_PREFIX);char digits[6];for(int i=4;i>=0;i--){digits[i]=(char)('0'+port%10);port/=10;}digits[5]=0;appendstr(out,108,digits);
}
static int vehicle_listener(int port){
 char path[108];vehicle_path(port,path);int fd=control_socket();if(fd<0)return -1;
 int rc=control_address(fd,path,0);
 if(rc==-98||rc==-48){
  /* Never remove a live endpoint owned by another service. Reclaim only a
   * socket that refuses a connection (a prior crashed instance). */
  int probe=control_socket();int status=probe<0?0:control_address(probe,path,1);if(probe>=0)s_close(probe);
  if(status==-111||status==-61){s_unlink(path);rc=control_address(fd,path,0);}
 }
 if(rc<0||control_listen(fd)<0){s_close(fd);return -1;}return fd;
}
static void vehicle_disconnect(void){temperature.vehicle_connected=temperature.vehicle_valid=0;temperature_select();}
static void vehicle_packet(const u8*b,unsigned n){
 if(n!=11||b[0]!=37||b[1]!=0||b[2]!=1||control_u32(b+3)!=4)return;
 int value=(s32)control_u32(b+7);temperature.vehicle_connected=1;
 temperature.vehicle_valid=car_temp_valid(value);if(temperature.vehicle_valid)temperature.vehicle=value;
 temperature_select();
}
static void vehicle_init(void){
 vehicle.enabled=1;
 for(int i=0;i<9;i++)vehicle.listen[i]=-1;
 for(int i=0;i<16;i++)vehicle.client[i]=-1;
 for(int i=0;i<9;i++)vehicle.listen[i]=vehicle_listener(vehicle_ports[i]);
 vehicle.retry_at=s_now_ms();
}
static void poll_vehicle(void){
 if(!vehicle.enabled)return;u32 now=s_now_ms();
 if(now-vehicle.retry_at>=5000u){vehicle.retry_at=now;for(int i=0;i<9;i++)if(vehicle.listen[i]<0)vehicle.listen[i]=vehicle_listener(vehicle_ports[i]);}
 struct ControlPoll p[25];int map[25];unsigned count=0;
 for(int i=0;i<9;i++)if(vehicle.listen[i]>=0){p[count]=(struct ControlPoll){vehicle.listen[i],1,0};map[count++]=i;}
 for(int i=0;i<16;i++)if(vehicle.client[i]>=0){p[count]=(struct ControlPoll){vehicle.client[i],1,0};map[count++]=9+i;}
 if(!count||control_poll(p,count)<=0)return;
 unsigned budget=16;
 for(unsigned k=0;k<count&&budget;k++)if(p[k].revents){
  if(map[k]<9){
   int port=map[k],fd=control_accept(p[k].fd);if(fd<0)continue;
   int slot=-1;
   /* One current VehicleStatus producer: replacing a connection resets the
    * old sample until the new producer publishes a complete valid update. */
   if(vehicle_ports[port]==3700)for(int j=0;j<16;j++)if(vehicle.client[j]>=0&&vehicle.port[j]==port){s_close(vehicle.client[j]);vehicle.client[j]=-1;vehicle_disconnect();}
   for(int j=0;j<16;j++)if(vehicle.client[j]<0){slot=j;break;}
   if(slot<0&&vehicle_ports[port]==3700){
    for(int j=0;j<16;j++)if(vehicle.port[j]!=port){s_close(vehicle.client[j]);vehicle.client[j]=-1;slot=j;break;}
   }
   if(slot<0)s_close(fd);else{vehicle.client[slot]=fd;vehicle.port[slot]=(u8)port;if(vehicle_ports[port]==3700)vehicle_disconnect();}
  }else{
   int slot=map[k]-9;if(vehicle.client[slot]!=p[k].fd)continue;u8 b[512];long n=control_read(p[k].fd,b,sizeof(b));
   if(n>0&&vehicle_ports[vehicle.port[slot]]==3700)vehicle_packet(b,(unsigned)n);
   else if(n==0||(n<0&&n!=-11&&n!=-4)){
    s_close(vehicle.client[slot]);vehicle.client[slot]=-1;
    if(vehicle_ports[vehicle.port[slot]]==3700)vehicle_disconnect();
   }
  }budget--;
 }
}
static void vehicle_close(void){
 if(!vehicle.enabled)return;
 for(int i=0;i<16;i++)if(vehicle.client[i]>=0){s_close(vehicle.client[i]);vehicle.client[i]=-1;}
 for(int i=0;i<9;i++)if(vehicle.listen[i]>=0){char path[108];s_close(vehicle.listen[i]);vehicle.listen[i]=-1;vehicle_path(vehicle_ports[i],path);s_unlink(path);}
 vehicle_disconnect();vehicle.enabled=0;
}

#include "frame_renderer.h"

static int power_requested(void){char b[24];long fd=s_open(POWER_PATH,O_RDWR_,0);if(fd<0)return 0;long n=s_read((int)fd,b,sizeof(b)-1);if(n<=0){s_close((int)fd);return 0;}b[n]=0;int v=atoi0(b),r=0;if(v&1){s_lseek((int)fd,0,SEEK_SET_);s_write((int)fd,"0",1);r=1;}s_close((int)fd);return r;}

#ifndef HOST_TEST
static void run(void){
  boot_fade_load();
  csw_init();vehicle_init();
  memzero(&gps,sizeof(gps));gps.hour=-1;gps.min=0;memzero(&car,sizeof(car));memzero(&af,sizeof(af));memzero(&roadctx,sizeof(roadctx));
  load_font();
#if CLIO_UNICODE
#ifdef HOST_TEST
 cliou_load(CLIO_HOST_ROOT "clio843/maps/fonts.cpf");
#else
 cliou_load("./clio843/maps/fonts.cpf");
#endif
#endif
 hardware_load_bluetooth();load_prefs();debug_state_write();load_last_fix();mob_load();if(pref.learn_history)mob_session_begin(s_now_ms(),lastfix_valid,lastfix_lat,lastfix_lon);seed_boot_theme();if(!pref.debug)remove_stale_logs();load_last_audio();nrecents=read_places(RECENT_PATH,recents,RECENT_MAX);nfaves=read_places(FAVES_PATH,faves,FAVE_MAX);load_trip();apply_brightness();load_map();load_timezones();load_regions();load_cameras();seed_boot_map_view();load_route_data();s_unlink(BT_CMD_PATH);bt_write_small(BT_ACTIVE_PATH,"0\n");bt_active_written=0;bt_name_cache_load();bt_poll_status();if(trip.active){modal=MOD_RESUME_TRIP;modal_choice=0;}open_inputs();
 control_log("CONSOLE READER",0,csw.fd);
 if(vehicle.enabled)for(int i=0;i<9;i++)control_log("VEHICLE STATUS LISTENER",vehicle_ports[i],vehicle.listen[i]);
  int wdfd=(int)s_open(WD_PATH,O_WRONLY_,0);debug_log(STATE_LOG,"CLIOPLAY GLOBAL " CLIOPLAY_VERSION " START");debug_log(STATE_LOG,qadjmode==7?(qadj_map_ok?"QAD7 PACKET+MMAP QUERY ENABLED":"QAD7 PAGED QUERY ENABLED"):"QADJ FAST QUERY FALLBACK CPR2");{char mb[96];mb[0]=0;appendstr(mb,sizeof(mb),"MMAP QAD=");append_int(mb,sizeof(mb),qadj_map_ok);appendstr(mb,sizeof(mb)," GEOM=");append_int(mb,sizeof(mb),route_geom_map_ok);debug_log(STATE_LOG,mb);}u32 tick=0,next_model=s_now_ms(),next_input=next_model;render_runtime_reset(next_model);
  for(;;){
    u32 now=s_now_ms();int model_due=frame_due(now,next_model);
    if(power_requested()){boot_fade_cancel();debug_log(STATE_LOG,"POWER REQUEST");bt_write_small(BT_ACTIVE_PATH,"0\n");if(pref.learn_history)(void)mob_finish(s_now_ms());if(prefs_dirty)save_prefs();save_last_audio();if(gps.fix)remember_live_fix();save_last_fix();save_trip();render_background_dirty=1;render_runtime_step(hud_now_ms(),1);s_sync();s_nanosleep(0,400000000);vehicle_close();if(wdfd>=0)s_close(wdfd);for(int i=0;i<INPUT_MAX;i++)if(infd[i]>=0)s_close(infd[i]);if(gpsfd>=0)s_close(gpsfd);if(audiofd>=0)s_close(audiofd);if(mapfd>=0)s_close(mapfd);if(qadjfd>=0)s_close(qadjfd);if(routefd>=0)s_close(routefd);return;}
    if(frame_due(now,next_input)){poll_csw();poll_keys();next_input=frame_after(next_input,now,33u);}
    if(model_due){
      next_model=frame_after(next_model,now,FRAME_MODEL_MS);
      if((tick%5)==0)scan_inputs();int had_fix=gps.fix;poll_gps();if(gps.fix){if(!had_fix){track_valid=0;track_course=-1;}remember_live_fix();}
      update_smooth_track();update_display_course();
      if((tick%2)==0){poll_audio();poll_car();poll_vehicle();bt_source_service();}
      if((tick%5)==0){bt_poll_status();poll_phone_destination();}
      if((tick%10)==0)update_road_context();
      phone_destination_apply();trip_service();arrival_modal_service();
      if(bt_control_mode&&(screen!=SCR_MAP||modal!=MOD_NONE||freecam||!bt_controls_available()))bt_control_mode=0;
      prefs_service();ui_anim_tick=tick;if((tick%10)==0&&wdfd>=0)s_write(wdfd,"W",1);
      clock_service();region_service();camera_service();refresh_theme();mobility_service(now);
      if(screen!=SCR_MAP&&!(tick&1u))render_background_dirty=1;
      tick++;
    }
    render_runtime_step(s_now_ms(),model_due);
    now=s_now_ms();u32 wake=frame_earlier(next_model,next_input);
    wake=frame_earlier(wake,render_runtime_deadline(now));
    u32 budget=frame_route_budget(now,wake);
    if(route_status==R_CALC&&budget){
      route_prof_slice_ms=route_prof_on?now:0;route_service_until(now+budget);
      u32 elapsed=s_now_ms()-now;render_stats.route_ms+=elapsed;
      if(route_prof_on){route_prof_cpu_ms+=elapsed;route_prof_slice_ms=0;}
      continue;
    }
    if(model_due&&route_status==R_READY)route_service_until(now);
    render_profile_service(s_now_ms());
    now=s_now_ms();if(!frame_due(now,wake)){u32 wait=wake-now;s_nanosleep((long)(wait/1000u),(long)(wait%1000u)*1000000L);}
  }
}
__attribute__((noreturn))void _start(void){run();s_exit(0);}
#else
int main(void){memzero(&gps,sizeof(gps));gps.fix=1;gps.sats=8;gps.lat=5498200;gps.lon=-165638;gps.mph10=380;gps.course=70;gps.hour=13;gps.min=8;gps.day=15;gps.mon=9;gps.year=2026;copystr(car.source,sizeof(car.source),"FM BBC R1");copystr(car.volume,sizeof(car.volume),"VOL 12");car.temp=18;car.tempok=1;load_font();hardware_load_bluetooth();load_prefs();load_last_fix();
#ifdef PREVIEW_FORCE_DAY
 pref.theme=THEME_DAY;apply_palette(0);
#elif defined(PREVIEW_FORCE_NIGHT)
 pref.theme=THEME_NIGHT;apply_palette(1);
#else
 seed_boot_theme();
#endif
 load_map();nrecents=read_places(RECENT_PATH,recents,RECENT_MAX);nfaves=read_places(FAVES_PATH,faves,FAVE_MAX);load_trip();load_route_data();update_smooth_track();update_road_context();if(trip.active){modal=MOD_RESUME_TRIP;modal_choice=0;}copystr(af.band,sizeof(af.band),"FM");copystr(af.station,sizeof(af.station),"BBC R1");copystr(af.freq,sizeof(af.freq),"98.8");
#ifdef PREVIEW_DEMO
 nrecents=10;recents[0].lat=5496810;recents[0].lon=-161770;copystr(recents[0].name,sizeof(recents[0].name),"NEWCASTLE CENTRAL");recents[1].lat=5501450;recents[1].lon=-169420;copystr(recents[1].name,sizeof(recents[1].name),"KINGSTON PARK");recents[2].lat=5499000;recents[2].lon=-160000;copystr(recents[2].name,sizeof(recents[2].name),"HOME");recents[3].lat=5497700;recents[3].lon=-161100;copystr(recents[3].name,sizeof(recents[3].name),"QUAYSIDE");recents[4].lat=5497400;recents[4].lon=-162900;copystr(recents[4].name,sizeof(recents[4].name),"ELDON SQUARE");recents[5].lat=5499500;recents[5].lon=-159500;copystr(recents[5].name,sizeof(recents[5].name),"JESMOND");recents[6].lat=5500200;recents[6].lon=-165000;copystr(recents[6].name,sizeof(recents[6].name),"GOSFORTH");recents[7].lat=5496400;recents[7].lon=-158400;copystr(recents[7].name,sizeof(recents[7].name),"GATESHEAD");recents[8].lat=5497200;recents[8].lon=-157900;copystr(recents[8].name,sizeof(recents[8].name),"BALTIC CENTRE");recents[9].lat=5498300;recents[9].lon=-163500;copystr(recents[9].name,sizeof(recents[9].name),"LEAZES PARK");
 nfaves=8;faves[0]=recents[2];faves[1]=recents[0];faves[2]=recents[1];faves[3]=recents[5];faves[4]=recents[3];faves[5]=recents[6];faves[6]=recents[4];faves[7]=recents[9];
 memzero(&trip,sizeof(trip));trip.active=1;trip.count=2;trip.active_idx=0;trip.target[0].lat=5501450;trip.target[0].lon=-169420;copystr(trip.target[0].name,sizeof(trip.target[0].name),"KINGSTON PARK");trip.target[1].lat=5496810;trip.target[1].lon=-161770;copystr(trip.target[1].name,sizeof(trip.target[1].name),"NEWCASTLE CENTRAL");trip_need_route=1;route_status=R_IDLE;modal=MOD_NONE;
#ifdef PREVIEW_ROUTE_READY
 trip_need_route=0;route_status=R_READY;trip_pane=1;trip_right_focus=0;
#endif
#endif
#ifdef PREVIEW_BT
copystr(af.band,sizeof(af.band),"CD");copystr(btui.title,sizeof(btui.title),"Hotel California");copystr(btui.artist,sizeof(btui.artist),"Eagles");copystr(btui.album,sizeof(btui.album),"Hotel California (2013 Remaster)");copystr(btui.device,sizeof(btui.device),"JOHN'S IPHONE");btui.connected=1;btui.paired=1;btui.playing=1;btui.pos=151;btui.dur=390;nbtdev=10;copystr(btdev[0].name,sizeof(btdev[0].name),"JOHN'S IPHONE");copystr(btdev[0].addr,sizeof(btdev[0].addr),"00:11:22:33:44:55");btdev[0].connected=1;copystr(btdev[1].name,sizeof(btdev[1].name),"PIXEL 10");copystr(btdev[1].addr,sizeof(btdev[1].addr),"00:11:22:33:44:66");copystr(btdev[2].name,sizeof(btdev[2].name),"MUM'S IPHONE");copystr(btdev[2].addr,sizeof(btdev[2].addr),"00:11:22:33:44:77");copystr(btdev[3].name,sizeof(btdev[3].name),"WORK PHONE");copystr(btdev[3].addr,sizeof(btdev[3].addr),"00:11:22:33:44:88");copystr(btdev[4].name,sizeof(btdev[4].name),"PIXEL 8");copystr(btdev[4].addr,sizeof(btdev[4].addr),"00:11:22:33:44:99");copystr(btdev[5].name,sizeof(btdev[5].name),"SARAH'S IPHONE");copystr(btdev[5].addr,sizeof(btdev[5].addr),"00:11:22:33:44:AA");copystr(btdev[6].name,sizeof(btdev[6].name),"GALAXY S25");copystr(btdev[6].addr,sizeof(btdev[6].addr),"00:11:22:33:44:BB");copystr(btdev[7].name,sizeof(btdev[7].name),"OLD IPHONE");copystr(btdev[7].addr,sizeof(btdev[7].addr),"00:11:22:33:44:CC");copystr(btdev[8].name,sizeof(btdev[8].name),"TABLET");copystr(btdev[8].addr,sizeof(btdev[8].addr),"00:11:22:33:44:DD");copystr(btdev[9].name,sizeof(btdev[9].name),"SPARE PHONE");copystr(btdev[9].addr,sizeof(btdev[9].addr),"00:11:22:33:44:EE");
#endif
#ifdef PREVIEW_BT_EMPTY
copystr(af.band,sizeof(af.band),"CD");btui.connected=1;btui.paired=1;btui.playing=1;copystr(btui.device,sizeof(btui.device),"JOHN'S IPHONE");
#endif
#ifdef PREVIEW_BTCTRL
bt_control_mode=1;
#endif
#ifdef PREVIEW_BT_SCROLL
bt_page_focus=10;
#endif

#ifdef PREVIEW_PHONE_DEST
screen=SCR_TRIP;notice_success(ui_tr("Stop added"));
#endif
#ifdef PREVIEW_SOURCE_PICK
dest_pick_action=DEST_PICK_CHANGE_DEST;modal=MOD_SOURCE_PICK;modal_choice=0;screen=SCR_TRIP;
#endif
#ifdef PREVIEW_TRIP_FAVES
trip_tab=1;trip_focus=0;trip_pane=0;screen=SCR_TRIP;
#endif
#ifdef PREVIEW_TRIP_SCROLL
trip_tab=0;trip_focus=8;trip_pane=0;screen=SCR_TRIP;
#endif
#ifdef PREVIEW_NAV_IDLE
memzero(&trip,sizeof(trip));route_status=R_IDLE;nav_maneuver_reset();
#endif
#ifdef PREVIEW_MAP_NOTICE_ERROR
screen=SCR_MAP;modal=MOD_NONE;notice_fail(ui_tr("No active route"));
#endif
#ifdef PREVIEW_MAP_NOTICE_SUCCESS
screen=SCR_MAP;modal=MOD_NONE;notice_success(ui_tr("Stop added"));
#endif
#ifdef PREVIEW_KEYBOARD
kbd_action=KBD_NAME_FAVE;copystr(kbd_text,sizeof(kbd_text),"HOME");kbd_focus=7;
#endif
#ifdef PREVIEW_MENU
screen=SCR_MENU;
#elif defined(PREVIEW_BLUETOOTH) || defined(PREVIEW_MEDIA_BT)
screen=SCR_BT;
#elif defined(PREVIEW_SETTINGS)
screen=SCR_SET;
#elif defined(PREVIEW_KEYBOARD)
screen=SCR_KEY;
#elif defined(PREVIEW_TRIP)
screen=SCR_TRIP;
#else
screen=SCR_MAP;
#endif
draw();return 0;}
#endif
#ifndef HOST_TEST
/* Freestanding ARM EABI compiler-runtime shims for the device build. */
void *memcpy(void *d0,const void *s0,unsigned n){u8*d=(u8*)d0;const u8*s=(const u8*)s0;while(n--)*d++=*s++;return d0;}
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
__attribute__((naked)) void __aeabi_uidiv(void){__asm__ volatile("b __aeabi_uidivmod\n");}
__attribute__((naked)) void __aeabi_idivmod(void){__asm__ volatile(
  "push {r4,r5,lr}\n"
  "mov r4,r0\n"
  "eor r5,r0,r1\n"
  "cmp r0,#0\n"
  "rsblt r0,r0,#0\n"
  "cmp r1,#0\n"
  "rsblt r1,r1,#0\n"
  "bl __aeabi_uidivmod\n"
  "cmp r5,#0\n"
  "rsblt r0,r0,#0\n"
  "cmp r4,#0\n"
  "rsblt r1,r1,#0\n"
  "pop {r4,r5,pc}\n");}
__attribute__((naked)) void __aeabi_idiv(void){__asm__ volatile("b __aeabi_idivmod\n");}
#endif
