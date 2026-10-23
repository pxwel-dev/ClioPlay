/* ClioPlay Global 1.1.0-rc.1
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
 * CPM1/2 remain readable for rollback. CPM3/4 add Z6 road metadata used for
 * current-street and OSM-derived speed-limit context. CPM4 adds a metadata
 * flags byte; bit 0 marks weak/estimated speed limits. Device-side code uses
 * fixed-point integer maths only; no libc, malloc, SQLite, protobuf or libm.
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
static long s_read(int fd,void*p,unsigned n){return read(fd,p,n);} 
static long s_write(int fd,const void*p,unsigned n){return write(fd,p,n);} 
static long s_open(const char*p,int f,int m){return open(p,f,m);} 
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

#ifndef HOST_TEST
#define FB_PATH "/dev/fb"
#define FONT_PATH "/mnt/sdcard/clio843/font.bin"
#define MAP_PATH "/mnt/sdcard/clio843/maps/map.cpm"
#define FOCUS_BUS "/tmp/cfbus"
#define LAST_PATH "/media/sdcard/clio843/audlast.dat"
#define CAR_PATH "/tmp/cfstate"
#define GPS_PATH "/var/run/gpspipe"
#define PREF_PATH "/media/sdcard/clio843/prefs.ini"
#define THEME_STATE_PATH "/media/sdcard/clio843/theme.dat"
#define LASTGPS_PATH "/media/sdcard/clio843/gpslast.dat"
#define PREF_TMP_PATH "/media/sdcard/clio843/prefs.tmp"
#define RECENT_PATH "/media/sdcard/clio843/recent.dat"
#define FAVES_PATH "/media/sdcard/clio843/faves.dat"
#define TRIP_PATH "/media/sdcard/clio843/trip.dat"
#define ROUTE_PATH "/mnt/sdcard/clio843/maps/route.cpr"
#define QADJ_PATH "/mnt/sdcard/clio843/maps/route.qadj"
#define STATE_LOG "/media/sdcard/clio843/state.log"
#define ROUTING_STATUS_PATH "/media/sdcard/clio843/routing-status.txt"
#define KEY_LOG "/media/sdcard/clio843/key.log"
#define FOCUS_LOG "/media/sdcard/clio843/focus.log"
#define BT_LOG "/media/sdcard/clio843/bt.log"
#define DEBUG_STATE_PATH "/tmp/cldebug"
#define BT_ACTIVE_PATH "/tmp/clbt"
#define CAMERA_ALERT_PATH "/tmp/clalert"
#define CAMERA_ALERT_TMP "/tmp/clalert.new"
#define BT_STATUS_PATH "/tmp/clbtstat"
#define BT_META_PATH "/tmp/clbtmeta"
#define BT_NAV_PATH "/tmp/clnav"
#define BT_DEVLIST_PATH "/tmp/clbtdevices"
#define BT_NAMES_PATH "/media/sdcard/clio843/btnames.dat"
#define BT_CMD_PATH "/tmp/clbtcmd"
#define OLD_AUDIO_LOG "/media/sdcard/clio843/cpaud.log"
#define OLD_GPS_LOG "/media/sdcard/clio843/gps.log"
#define WD_PATH "/dev/watchdog"
#define POWER_PATH "/proc/barcelona/onoff"
#define BL_MAX "/sys/class/backlight/s3c/max_brightness"
#define BL_VAL "/sys/class/backlight/s3c/brightness"
#else
#ifndef CLIO_HOST_ROOT
#define CLIO_HOST_ROOT "/tmp/clioplay-ui-host/"
#endif
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

/* Tiny persisted AUTO-theme hint shared with cliospl. It is only a fallback
 * when neither live GPS time nor a valid wall clock is available at boot. */
static int read_theme_state_raw(void){char c=0;long fd=s_open(THEME_STATE_PATH,O_RDONLY_,0);if(fd<0)return-1;long n=s_read((int)fd,&c,1);s_close((int)fd);return n==1&&c=='0'?0:(n==1&&c=='1'?1:-1);}
static void write_theme_state_raw(int night){char c=night?'1':'0',old=0;long fd=s_open(THEME_STATE_PATH,O_RDONLY_,0);if(fd>=0){long n=s_read((int)fd,&old,1);s_close((int)fd);if(n==1&&old==c)return;}fd=s_open(THEME_STATE_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd>=0){s_write((int)fd,&c,1);s_close((int)fd);}}

/* Modern dual-theme palette, RGB565. Geometry stays shared between themes,
 * while surfaces/focus treatments are allowed to differ through palette roles.
 * C_WHITE remains the primary text role even though it is dark in DAY mode. */
static u16 C_BG=0x0082,C_PANEL=0x1105,C_PANEL2=0x1146,C_BORDER=0x2a09;
static u16 C_CYAN=0x64ff,C_WHITE=0xf7bf,C_BLUE=0x3c97,C_MUTED=0x9d76;
static u16 C_DIM=0x63af,C_WARN=0xf524,C_ROAD=0x532d,C_WATER=0x1a4c,C_GREEN=0x2b4a;
static u16 C_HEADER=0x08c3,C_ACTIVE=0x1a2b,C_ACTIVE_TEXT=0xf7ff,C_ACCENT=0x46b9,C_SHADOW=0x0041,C_HILITE=0x21a7;
static const u16 C_CAR=0xfc60; /* fixed orange in both themes */
static const u16 C_ROUTE=0xf81f; /* fixed vivid magenta for map contrast */
static int theme_resolved=1; /* 0 DAY, 1 NIGHT */

static void apply_palette(int night){
  int next=night?1:0,changed=(next!=theme_resolved);theme_resolved=next;
  if(night){
    /* NIGHT: deep charcoal-blue surfaces with a restrained teal focus colour. */
    C_BG=0x0082; C_PANEL=0x1105; C_PANEL2=0x1146; C_BORDER=0x2a09;
    C_CYAN=0x46b9; C_WHITE=0xf7bf; C_BLUE=0x3c97; C_MUTED=0x9d76;
    C_DIM=0x63af; C_WARN=0xf524; C_ROAD=0x532d; C_WATER=0x1a4c; C_GREEN=0x2b4a;
    C_HEADER=0x08c3; C_PANEL=0x10e4; C_PANEL2=0x1926; C_BORDER=0x39e9; C_CYAN=0x64ff; C_ACTIVE=0x1a2b; C_ACTIVE_TEXT=0xffff; C_ACCENT=0x64ff; C_SHADOW=0x0041; C_HILITE=0x21a7;
  }else{
    /* DAY: warm/cool automotive greys rather than plain white inversion. */
    C_BG=0xef7d; C_PANEL=0xf7be; C_PANEL2=0xdf3c; C_BORDER=0xb618;
    C_CYAN=0x03f1; C_WHITE=0x1146; C_BLUE=0x33f3; C_MUTED=0x534e;
    C_DIM=0x7c51; C_WARN=0xb340; C_ROAD=0xadb6; C_WATER=0xa6bc; C_GREEN=0x9e55;
    C_HEADER=0xe73c; C_PANEL=0xf7be; C_PANEL2=0xe71c; C_BORDER=0xb618; C_CYAN=0x039b; C_ACTIVE=0x039b; C_ACTIVE_TEXT=0xffff; C_ACCENT=0x039b; C_SHADOW=0xb618; C_HILITE=0xffff;
  }
  if(changed)write_theme_state_raw(theme_resolved);
}

static u16 fb[FB_PIX];
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
 * map geometry, six screens and input state machine remain the UI baseline. */
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
/* Lightweight HMI surfaces: one-pixel cut corners, a top highlight and bottom
 * shadow give depth without alpha blending or expensive effects. */
static void ui_surface(int x,int y,int w,int h,u16 c){round_rect(x,y,w,h,4,c,15);}
static void ui_panel(int x,int y,int w,int h,int active){ui_surface(x,y,w,h,C_PANEL);if(active)fill(x+6,y,w-12,2,C_CYAN);}
static void ui_choice(int x,int y,int w,int h,int selected){ui_surface(x,y,w,h,selected?C_ACTIVE:C_PANEL2);if(selected)fill(x+2,y+4,2,h-8,C_CYAN);}
static u16 ui_choice_text(int selected){return selected?C_ACTIVE_TEXT:C_WHITE;}
static void ui_modal(int x,int y,int w,int h){round_rect(x-1,y-1,w+2,h+2,6,C_BORDER,15);round_rect(x,y,w,h,5,C_PANEL,15);}
static void line1(int x0,int y0,int x1,int y1,u16 c,int wid){int dx=x1>x0?x1-x0:x0-x1,sx=x0<x1?1:-1;int dy=-(y1>y0?y1-y0:y0-y1),sy=y0<y1?1:-1,err=dx+dy;for(;;){int a,b,r=wid/2;for(a=-r;a<=r;a++)for(b=-r;b<=r;b++)px(x0+a,y0+b,c);if(x0==x1&&y0==y1)break;int e2=2*err;if(e2>=dy){err+=dy;x0+=sx;}if(e2<=dx){err+=dx;y0+=sy;}}}

static int load_font(void){long fd=s_open(FONT_PATH,O_RDONLY_,0);if(fd<0)return 0;long n=s_read((int)fd,font,FONT_BYTES);s_close((int)fd);font_ok=(n==FONT_BYTES);return font_ok;}
static void glyph(int x,int y,char ch,u16 c,int scale){int gx,gy,sx,sy;unsigned idx;if(ch<32||ch>126)ch='?';idx=(unsigned)(ch-32)*96;if(!font_ok)return;for(gy=0;gy<12;gy++)for(gx=0;gx<8;gx++){if(font[idx+(unsigned)gy*8u+(unsigned)gx]>64){for(sy=0;sy<scale;sy++)for(sx=0;sx<scale;sx++)px(x+gx*scale+sx,y+gy*scale+sy,c);}}}
static void text(int x,int y,const char*s,u16 c,int scale){
#if CLIO_UNICODE
 if(cliou_ready()&&utf8_native(s)){cliou_draw(x,y,scale<=1?0:1,s,c,0);return;}
#endif
 int adv=8*scale;while(*s){glyph(x,y,*s++,c,scale);x+=adv;}}
static int textw(const char*s,int scale){
#if CLIO_UNICODE
 if(cliou_ready()&&utf8_native(s))return cliou_width(s,scale<=1?0:1);
#endif
 return(int)slen(s)*8*scale;}
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

static int fb_flush(void){long fd=s_open(FB_PATH,O_WRONLY_|O_CREAT_,0666);if(fd<0)return 0;s_lseek((int)fd,0,SEEK_SET_);long n=s_write((int)fd,fb,sizeof(fb));s_close((int)fd);return n==(long)sizeof(fb);}

enum { THEME_DAY=0, THEME_NIGHT=1, THEME_AUTO=2 };
struct Prefs {int bright,hoff,moff,autozoom,debug,theme,units,cameras,clock_auto,language;};
static struct Prefs pref={50,0,0,1,0,THEME_AUTO,0,1,1,0};
static int pref_keyboard_language=-1;
#include "ui_translations.h"
/* The source badge is BT in every language. Page titles and status messages
 * retain their translated phone wording outside English. */
static const char*bt_label(void){return pref.language==0?"Bluetooth":ui_tr("Phone connection");}
static const char*bt_source_label(void){return "BT";}
static const char*bt_unavailable_label(void){return pref.language==0?"Bluetooth unavailable":ui_tr("Phone connection unavailable");}
static int manual_zoom=5;
static int prefs_dirty=0,prefs_delay=0;
static void parse_pref_line(char*ln){char*eq=ln;while(*eq&&*eq!='=')eq++;if(!*eq)return;*eq++=0;int v=atoi0(eq);if(streq(ln,"bright"))pref.bright=v;else if(streq(ln,"hoff"))pref.hoff=v;else if(streq(ln,"moff"))pref.moff=v;else if(streq(ln,"autozoom"))pref.autozoom=v?1:0;else if(streq(ln,"debug"))pref.debug=v?1:0;else if(streq(ln,"theme"))pref.theme=v;else if(streq(ln,"units"))pref.units=v;else if(streq(ln,"cameras"))pref.cameras=v>=0&&v<=2?v:1;else if(streq(ln,"clockauto"))pref.clock_auto=v?1:0;else if(streq(ln,"language"))pref.language=v>=0&&v<UI_LANGUAGE_COUNT?v:0;else if(streq(ln,"manualzoom"))manual_zoom=v;else if(streq(ln,"keyboardlanguage"))pref_keyboard_language=v>=0&&v<UI_LANGUAGE_COUNT?v:-1;}
static void load_prefs(void){char b[512],ln[96];unsigned u=0;long fd=s_open(PREF_PATH,O_RDONLY_,0);if(fd>=0){long n=s_read((int)fd,b,sizeof(b));s_close((int)fd);if(n>0){for(long i=0;i<n;i++){char c=b[i];if(c=='\r'||c=='\n'){if(u){ln[u]=0;parse_pref_line(ln);u=0;}}else if(u+1<sizeof(ln))ln[u++]=c;}if(u){ln[u]=0;parse_pref_line(ln);}}}if(pref.bright<10)pref.bright=10;if(pref.bright>100)pref.bright=100;if(pref.theme<THEME_DAY||pref.theme>THEME_AUTO)pref.theme=THEME_AUTO;if(pref.units<0||pref.units>2)pref.units=0;if(pref.hoff*60+pref.moff< -720||pref.hoff*60+pref.moff>840)pref.hoff=pref.moff=0;if(manual_zoom<0)manual_zoom=0;if(manual_zoom>6)manual_zoom=6;}
/* Seed AUTO before the first NMEA sentence arrives, so cliospl -> first UI frame
 * never falls through the old hard-coded NIGHT default. GPS later refines this
 * with the normal solar calculation. */
static int boot_wall_clock_minutes(void){long sec=-1;
#ifndef HOST_TEST
 struct ktv v;if(sc2(78,(long)&v,0)>=0&&v.sec>=1577836800L)sec=v.sec;
#else
 time_t t=time(0);if(t!=(time_t)-1&&t>=1577836800L)sec=(long)t;
#endif
 if(sec<0)return-1;sec%=86400L;if(sec<0)sec+=86400L;int mt=(int)(sec/60L)+pref.hoff*60+pref.moff;while(mt<0)mt+=1440;while(mt>=1440)mt-=1440;return mt;}
static void seed_boot_theme(void){int night;if(pref.theme==THEME_NIGHT)night=1;else if(pref.theme==THEME_DAY)night=0;else{int mt=boot_wall_clock_minutes();if(mt>=0)night=mt<420||mt>=1140;else{int saved=read_theme_state_raw();night=saved>=0?saved:1;}}apply_palette(night);}
static void save_prefs(void){char b[384];b[0]=0;appendstr(b,sizeof(b),"version=6\nbright=");append_int(b,sizeof(b),pref.bright);appendstr(b,sizeof(b),"\nhoff=");append_int(b,sizeof(b),pref.hoff);appendstr(b,sizeof(b),"\nmoff=");append_int(b,sizeof(b),pref.moff);appendstr(b,sizeof(b),"\nautozoom=");append_int(b,sizeof(b),pref.autozoom);appendstr(b,sizeof(b),"\ndebug=");append_int(b,sizeof(b),pref.debug);appendstr(b,sizeof(b),"\ntheme=");append_int(b,sizeof(b),pref.theme);appendstr(b,sizeof(b),"\nunits=");append_int(b,sizeof(b),pref.units);appendstr(b,sizeof(b),"\ncameras=");append_int(b,sizeof(b),pref.cameras);appendstr(b,sizeof(b),"\nclockauto=");append_int(b,sizeof(b),pref.clock_auto);appendstr(b,sizeof(b),"\nlanguage=");append_int(b,sizeof(b),pref.language);appendstr(b,sizeof(b),"\nmanualzoom=");append_int(b,sizeof(b),manual_zoom);appendstr(b,sizeof(b),"\nkeyboardlanguage=");append_int(b,sizeof(b),pref_keyboard_language);appendstr(b,sizeof(b),"\n");long fd=s_open(PREF_TMP_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd>=0){s_write((int)fd,b,slen(b));s_close((int)fd);s_sync();if(s_rename(PREF_TMP_PATH,PREF_PATH)<0){fd=s_open(PREF_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd>=0){s_write((int)fd,b,slen(b));s_close((int)fd);}}s_sync();}prefs_dirty=0;prefs_delay=0;}
static void prefs_mark_dirty(void){prefs_dirty=1;prefs_delay=10;}
static void prefs_service(void){if(!prefs_dirty)return;if(prefs_delay>0){prefs_delay--;return;}save_prefs();}

static void remove_stale_logs(void){
  s_unlink(ROUTING_STATUS_PATH);
  s_unlink(STATE_LOG); s_unlink(KEY_LOG); s_unlink(FOCUS_LOG);
  s_unlink(BT_LOG); s_unlink(OLD_AUDIO_LOG); s_unlink(OLD_GPS_LOG);
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

/* DEV5.24: persist one tiny trusted last-fix record so a cold start can show
 * useful map context while the receiver reacquires satellites.  This is never
 * exposed as live GPS: speed/road/car marker remain suppressed until gps.fix. */
struct LastFixDisk {char magic[4];s32 lat,lon;u16 course,pad;u32 check;};
static int lastfix_valid=0,lastfix_live_seen=0;
static s32 lastfix_lat=0,lastfix_lon=0;static int lastfix_course=0;
static u32 lastfix_check(s32 lat,s32 lon,u16 course){return 0x43504731u^(u32)lat^((u32)lon*2654435761u)^((u32)course<<16);}
static int load_last_fix(void){struct LastFixDisk d;long fd=s_open(LASTGPS_PATH,O_RDONLY_,0);if(fd<0)return 0;long n=s_read((int)fd,&d,sizeof(d));s_close((int)fd);if(n!=(long)sizeof(d))return 0;if(d.magic[0]!='L'||d.magic[1]!='G'||d.magic[2]!='P'||d.magic[3]!='1')return 0;if(d.lat<-9000000||d.lat>9000000||d.lon<-18000000||d.lon>18000000||d.course>359)return 0;if(!d.lat&&!d.lon)return 0;if(d.check!=lastfix_check(d.lat,d.lon,d.course))return 0;lastfix_lat=d.lat;lastfix_lon=d.lon;lastfix_course=(int)d.course;lastfix_valid=1;return 1;}
static int save_last_fix(void){if(!lastfix_valid)return 0;struct LastFixDisk d;d.magic[0]='L';d.magic[1]='G';d.magic[2]='P';d.magic[3]='1';d.lat=lastfix_lat;d.lon=lastfix_lon;d.course=(u16)(lastfix_course>=0&&lastfix_course<=359?lastfix_course:0);d.pad=0;d.check=lastfix_check(d.lat,d.lon,d.course);long fd=s_open(LASTGPS_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd<0)return-1;long n=s_write((int)fd,&d,sizeof(d));s_close((int)fd);return n==(long)sizeof(d)?1:-1;}
static void remember_live_fix(void){if(!gps.fix)return;lastfix_lat=gps.lat;lastfix_lon=gps.lon;/* Preserve the last trustworthy moving orientation instead of overwriting it with stationary GPS-course noise. */if(!lastfix_valid||gps.mph10>=DISPLAY_COURSE_UPDATE_MPH10)lastfix_course=gps.course;lastfix_valid=1;if(!lastfix_live_seen){save_last_fix();lastfix_live_seen=1;}}
static int parse_dec10(const char*s){int v=0,frac=0,neg=0;if(*s=='-'){neg=1;s++;}while(*s>='0'&&*s<='9'){v=v*10+(*s-'0');s++;}if(*s=='.'&&s[1]>='0'&&s[1]<='9')frac=s[1]-'0';return neg?-(v*10+frac):(v*10+frac);}
static s32 nmea_coord_e5(const char*s,int islon,char hemi){int deg=0;int digs=islon?3:2;for(int i=0;i<digs;i++){if(s[i]<'0'||s[i]>'9')return 0;deg=deg*10+s[i]-'0';}s+=digs;int min10k=0;int whole=0;while(*s>='0'&&*s<='9'){whole=whole*10+(*s-'0');s++;}min10k=whole*10000;if(*s=='.'){s++;int m=1000;while(*s>='0'&&*s<='9'&&m){min10k+=(*s-'0')*m;m/=10;s++;}}s32 e5=(s32)deg*100000+(s32)(min10k/6);if(hemi=='S'||hemi=='W')e5=-e5;return e5;}
static void parse_hhmmss(const char*s){if(slen(s)<6)return;gps.hour=(s[0]-'0')*10+s[1]-'0';gps.min=(s[2]-'0')*10+s[3]-'0';gps.sec=(s[4]-'0')*10+s[5]-'0';}
static void parse_ddmmyy(const char*s){if(slen(s)<6)return;gps.day=(s[0]-'0')*10+s[1]-'0';gps.mon=(s[2]-'0')*10+s[3]-'0';gps.year=2000+(s[4]-'0')*10+s[5]-'0';}

/* Offline AUTO theme. SUN_HALF contains sunrise/sunset half-day lengths in
 * minutes for UK latitude bands 49..61N at 15-day anchors. Runtime linearly
 * interpolates by date, then shifts solar noon by longitude. GPS time/date is
 * UTC, so daylight detection is independent of DST and the display offset. */
#include "global_solar.h"
static int leap_year(int y){return(y%4)==0&&((y%100)!=0||(y%400)==0);}
static int day_of_year0(void){static const u16 cum[12]={0,31,59,90,120,151,181,212,243,273,304,334};if(gps.mon<1||gps.mon>12||gps.day<1||gps.day>31)return-1;int d=cum[gps.mon-1]+gps.day-1;if(gps.mon>2&&leap_year(gps.year))d++;return d;}
#include "global_clock.h"
static int solar_day_index=0,solar_day_frac=0,solar_day_span=15;
static int solar_interp(const u16*row){int a=row[solar_day_index],b=row[solar_day_index+1];return a+(b-a)*solar_day_frac/solar_day_span;}
static int sun_half_minutes(void){int d=day_of_year0();if(d<0)return-1;if(d>365)d=365;int i=d/15;if(i>24)i=24;solar_day_index=i;solar_day_frac=d-i*15;solar_day_span=i==24?5:15;int latitude=gps.lat+8500000;if(latitude<0)latitude=0;if(latitude>=17000000)latitude=16999999;int row=latitude/500000,f=latitude%500000;int a=solar_interp(GLOBAL_SUN_HALF[row]),b=solar_interp(GLOBAL_SUN_HALF[row+1]);return a+(b-a)*(f/100)/5000;}
static int local_clock_minutes(void){if(gps.hour<0||gps.hour>23||gps.min<0||gps.min>59)return-1;int mt=gps.hour*60+gps.min+pref.hoff*60+pref.moff;while(mt<0)mt+=1440;while(mt>=1440)mt-=1440;return mt;}
static int auto_is_night(void){
  if(gps.hour<0||gps.hour>23||gps.min<0||gps.min>59)return theme_resolved;
  /* Preferred AUTO path: GPS UTC date + position gives an offline solar
   * sunrise/sunset decision. If the receiver has supplied time but not yet an
   * RMC date/position, fall back to a deterministic local-clock window rather
   * than freezing AUTO in whichever palette happened to be active at boot. */
  if(gps.year>=2020&&(gps.fix||lastfix_valid)){int half=sun_half_minutes();if(half>=0){if(half<=0)return 1;if(half>=720)return 0;int lon4=gps.lon/25000;int eot=GLOBAL_EOT[solar_day_index]+(GLOBAL_EOT[solar_day_index+1]-GLOBAL_EOT[solar_day_index])*solar_day_frac/solar_day_span;int now=(gps.hour*60+gps.min+lon4+eot+1440)%1440;return now<720-half||now>=720+half;}}
  {int mt=local_clock_minutes();if(mt<0)return theme_resolved;return mt<420||mt>=1140;} /* 07:00..19:00 fallback */
}
static void refresh_theme(void){int night=pref.theme==THEME_NIGHT?1:(pref.theme==THEME_DAY?0:auto_is_night());if(night!=theme_resolved)apply_palette(night);}
static int split_csv(char*s,char**f,int max){int n=0;f[n++]=s;while(*s&&n<max){if(*s==','){*s=0;f[n++]=s+1;}s++;}return n;}
static void parse_nmea(char*ln){char*f[20];int n;if(starts(ln,"$GPRMC,")||starts(ln,"$GNRMC,")){n=split_csv(ln,f,20);if(n>9){parse_hhmmss(f[1]);if(f[2][0]=='A'){gps.fix=1;gps_fix_ms=s_now_ms();gps.lat=nmea_coord_e5(f[3],0,f[4][0]);gps.lon=nmea_coord_e5(f[5],1,f[6][0]);int k10=parse_dec10(f[7]);gps.mph10=(k10*1151+500)/1000;gps.course=parse_dec10(f[8])/10;if(gps.course<0)gps.course=0;if(gps.course>359)gps.course%=360;}else{gps.fix=0;gps.mph10=0;}parse_ddmmyy(f[9]);}}
else if(starts(ln,"$GPGGA,")||starts(ln,"$GNGGA,")){n=split_csv(ln,f,20);if(n>7){parse_hhmmss(f[1]);int q=atoi0(f[6]);gps.sats=atoi0(f[7]);if(q>0){gps.fix=1;gps.lat=nmea_coord_e5(f[2],0,f[3][0]);gps.lon=nmea_coord_e5(f[4],1,f[5][0]);}else gps.fix=0;}}
}

static char gpsbuf[1024],gpsline[256];static unsigned gpsused=0;static int gpsfd=-1;
static void poll_gps(void){if(gpsfd<0)gpsfd=(int)s_open(GPS_PATH,O_RDONLY_|O_NONBLOCK_,0);if(gpsfd<0)return;for(;;){long n=s_read(gpsfd,gpsbuf,sizeof(gpsbuf));if(n<=0)break;for(long i=0;i<n;i++){char c=gpsbuf[i];if(c=='\n'||c=='\r'){if(gpsused){gpsline[gpsused]=0;parse_nmea(gpsline);gpsused=0;}}else if(gpsused+1<sizeof(gpsline))gpsline[gpsused++]=c;else gpsused=0;}}}

struct Car {char source[32],volume[16];int temp,tempok;};static struct Car car;

/* Direct Focus audio-state consumer. This is the former clioaudio parsing logic
 * folded into the main runtime: no helper process, /tmp/cpaudio, or state.log
 * shutdown handshake. focusd writes the volatile /tmp/cfbus stream; ClioPlay
 * reconstructs source/station/frequency/volume itself and caches the final
 * display state directly to audlast.dat on orderly power-down. */
struct AudioFeed {char band[8],freq[16],station[24];};static struct AudioFeed af;
static int audio_source_live=0;
static int audiofd=-1;static char audiobuf[4096],audioline[512];static unsigned audioused=0;
static char upc(char c){return(c>='a'&&c<='z')?(char)(c-32):c;}
static void copy_trim(char*out,unsigned cap,const u8*p,unsigned n){unsigned a=0,b=n,k=0;while(a<b&&(p[a]==' '||p[a]=='\t'||p[a]==0))a++;while(b>a&&(p[b-1]==' '||p[b-1]=='\t'||p[b-1]==0))b--;while(a<b&&k+1<cap){u8 c=p[a++];out[k++]=(c>=32&&c<127)?(char)c:' ';}out[k]=0;}
static int setstrx(char*dst,unsigned cap,const char*src,int upper){char t[40];unsigned i=0;while(src[i]&&i+1<cap&&i+1<sizeof(t)){t[i]=upper?upc(src[i]):src[i];i++;}t[i]=0;if(streq(dst,t))return 0;copystr(dst,cap,t);return 1;}
static int set_band(const char*b){audio_source_live=1;if(streq(af.band,b))return 0;setstrx(af.band,sizeof(af.band),b,1);af.freq[0]=0;af.station[0]=0;return 1;}
static int parse_uint_tail(const char*s){int v=-1,cur=0,seen=0;unsigned i=0;while(s[i]){if(s[i]>='0'&&s[i]<='9'){cur=cur*10+(s[i]-'0');seen=1;}else if(seen){v=cur;cur=0;seen=0;}i++;}if(seen)v=cur;return v;}
static int looks_tuning_label(const char*s){return s[0]=='<'||s[0]=='>'||s[0]=='[';}
static void rebuild_source(void){char out[32];unsigned k=0,i=0;if(!af.band[0])return;while(af.band[i]&&k+1<sizeof(out))out[k++]=af.band[i++];if((streq(af.band,"FM")||streq(af.band,"MW")||streq(af.band,"LW"))&&k+1<sizeof(out)){const char*d=af.station[0]?af.station:af.freq;if(d[0]){out[k++]=' ';i=0;while(d[i]&&k+1<sizeof(out)){out[k++]=af.station[0]?upc(d[i]):d[i];i++;}}}out[k]=0;setstrx(car.source,sizeof(car.source),out,0);}
static void set_volume(int v){char out[16];unsigned k=0;const char*p="VOL ";while(*p)out[k++]=*p++;if(v<0)v=0;if(v>99)v=99;if(v>=10)out[k++]=(char)('0'+v/10);out[k++]=(char)('0'+v%10);out[k]=0;setstrx(car.volume,sizeof(car.volume),out,0);}
static int valid_saved_char(u8 c){return c==0||(c>=32&&c<127);}
static void infer_band_from_source(void){if(starts(car.source,"FM"))setstrx(af.band,sizeof(af.band),"FM",1);else if(starts(car.source,"MW"))setstrx(af.band,sizeof(af.band),"MW",1);else if(starts(car.source,"LW"))setstrx(af.band,sizeof(af.band),"LW",1);else if(starts(car.source,"CD"))setstrx(af.band,sizeof(af.band),"CD",1);else if(starts(car.source,"AUX"))setstrx(af.band,sizeof(af.band),"AUX",1);}
static int load_last_audio(void){u8 rec[48];unsigned i;long fd=s_open(LAST_PATH,O_RDONLY_,0);if(fd<0)return 0;long n=s_read((int)fd,rec,sizeof(rec));s_close((int)fd);if(n!=(long)sizeof(rec))return 0;for(i=0;i<sizeof(rec);i++)if(!valid_saved_char(rec[i]))return 0;for(i=0;i<31&&rec[i];i++)car.source[i]=(char)rec[i];car.source[i]=0;for(i=0;i<15&&rec[32+i];i++)car.volume[i]=(char)rec[32+i];car.volume[i]=0;if(!car.source[0]&&!car.volume[0])return 0;infer_band_from_source();return 1;}
static int save_last_audio(void){u8 rec[48];unsigned i;memzero(rec,sizeof(rec));for(i=0;i<31&&car.source[i];i++)rec[i]=(u8)car.source[i];for(i=0;i<15&&car.volume[i];i++)rec[32+i]=(u8)car.volume[i];if(!car.source[0]&&!car.volume[0])return 0;long fd=s_open(LAST_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd<0)return-1;long n=s_write((int)fd,rec,sizeof(rec));s_close((int)fd);return n==(long)sizeof(rec)?1:-1;}
static int hexv(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return-1;}
static unsigned decode_rx(const char*line,u8*out,unsigned cap){unsigned i=0,n=0;while(line[i]&&line[i]!=':')i++;if(line[i]!=':')return 0;i++;while(line[i]){while(line[i]==' '||line[i]=='\t')i++;int a=hexv(line[i]),b=hexv(line[i+1]);if(a<0||b<0)break;if(n<cap)out[n++]=(u8)((a<<4)|b);else break;i+=2;}return n;}
static void process_property(unsigned id,const u8*p,unsigned n,int*srcchg){char t[40];copy_trim(t,sizeof(t),p,n);if(!t[0])return;if(id==100||id==40){if(starts(t,"AUDIO VOLUME")||starts(t,"NAV VOLUME")){int v=parse_uint_tail(t);if(v>=0)set_volume(v);return;}if(streq(t,"FM RADIO")||streq(t,"RADIO FM")){*srcchg|=set_band("FM");return;}if(streq(t,"MW RADIO")||streq(t,"RADIO MW")){*srcchg|=set_band("MW");return;}if(streq(t,"LW RADIO")||streq(t,"RADIO LW")){*srcchg|=set_band("LW");return;}if(streq(t,"MONO CD")||streq(t,"<NO CD>")||streq(t,"CD")){*srcchg|=set_band("CD");return;}if(streq(t,"AUX")||streq(t,"AUXILIARY")){*srcchg|=set_band("AUX");return;}}if(id==60){if(streq(t,"RADIO FM")){*srcchg|=set_band("FM");return;}if(streq(t,"RADIO MW")){*srcchg|=set_band("MW");return;}if(streq(t,"RADIO LW")){*srcchg|=set_band("LW");return;}}if(id==65){*srcchg|=setstrx(af.freq,sizeof(af.freq),t,0);return;}if(id==66){if(looks_tuning_label(t)){if(af.station[0]){af.station[0]=0;*srcchg=1;}return;}*srcchg|=setstrx(af.station,8,t,1);}}
static void process_rx_line(const char*line){u8 b[192];unsigned n=decode_rx(line,b,sizeof(b));if(n<5)return;unsigned i=0;int changed=0;while(i+5<=n){if((b[i]&0x7f)==13&&i+5<=n){unsigned len=b[i+4];if(i+5+len<=n){if(b[i+3]==1)process_property(b[i+2],b+i+5,len,&changed);i+=5+len;continue;}}i++;}if(changed)rebuild_source();}
static const char* after_text(const char*line){const char*k="text=\"";unsigned i=0,j;while(line[i]){j=0;while(k[j]&&line[i+j]==k[j])j++;if(!k[j])return line+i+j;i++;}return 0;}
static void process_decoded_line(const char*line){if(!(starts(line,"SOURCE_STATUS")||starts(line,"SOURCE_TEXT")||starts(line,"SOURCE_MODE")))return;const char*s=after_text(line);if(!s)return;char t[40],z[40];unsigned k=0;while(s[k]&&s[k]!='\"'&&k+1<sizeof(t)){t[k]=s[k];k++;}t[k]=0;copy_trim(z,sizeof(z),(const u8*)t,k);if(starts(z,"AUDIO VOLUME")||starts(z,"NAV VOLUME")){int v=parse_uint_tail(z);if(v>=0)set_volume(v);return;}int changed=0;if(streq(z,"FM RADIO")||streq(z,"RADIO FM"))changed=set_band("FM");else if(streq(z,"MW RADIO")||streq(z,"RADIO MW"))changed=set_band("MW");else if(streq(z,"LW RADIO")||streq(z,"RADIO LW"))changed=set_band("LW");else if(streq(z,"MONO CD")||streq(z,"<NO CD>")||streq(z,"CD"))changed=set_band("CD");else if(streq(z,"AUX")||streq(z,"AUXILIARY"))changed=set_band("AUX");if(changed)rebuild_source();}
static void process_audio_line(char*line){if(pref.debug)log_append(FOCUS_LOG,line);if(line[0]=='R'&&line[1]=='X'&&line[2]=='#')process_rx_line(line);else process_decoded_line(line);}
static void poll_audio(void){if(audiofd<0){audiofd=(int)s_open(FOCUS_BUS,O_RDONLY_|O_NONBLOCK_,0);if(audiofd<0)return;}for(;;){long n=s_read(audiofd,audiobuf,sizeof(audiobuf));if(n<=0){if(n<0){s_close(audiofd);audiofd=-1;audioused=0;}break;}for(long i=0;i<n;i++){char c=audiobuf[i];if(c=='\r'||c=='\n'){if(audioused){audioline[audioused]=0;process_audio_line(audioline);audioused=0;}}else if(audioused+1<sizeof(audioline))audioline[audioused++]=c;else audioused=0;}}}
static void poll_car(void){char b[256];long fd=s_open(CAR_PATH,O_RDONLY_,0);if(fd<0)return;long n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);if(n<=0)return;b[n]=0;for(long i=0;i+5<n;i++)if(b[i]=='t'&&b[i+1]=='e'&&b[i+2]=='m'&&b[i+3]=='p'&&b[i+4]=='='){car.temp=atoi0(b+i+5);car.tempok=1;break;}}

/* DEV5.34 Bluetooth media UI/daemon boundary.
 *
 * The Renault head unit remains in its stable no-disc CD source internally,
 * but CLIOPLAY treats that source as BLUETOOTH everywhere.  Audio ownership is
 * deliberately outside clio040: the clioa2dp daemon owns AVDTP/SBC, AVRCP and
 * /dev/barc-snd.  This process only publishes whether the Bluetooth source is
 * selected, consumes daemon state/metadata, and emits small idempotent commands.
 *
 * IPC is volatile (/tmp) and therefore cannot corrupt persistent settings:
 *   /tmp/clbt      : "1" while live CD/BLUETOOTH is selected, otherwise "0"
 *   /tmp/clbtstat  : one-line daemon status (BT CONNECTED/STREAMING/...)
 *   /tmp/clbtmeta  : KEY=VALUE lines (DEVICE/TITLE/ARTIST/ALBUM/...)
 *   /tmp/clbtcmd   : monotonically sequenced "<n> <COMMAND>" command record
 */
struct BtUi {
  char status[28],device[33],title[193],artist[161],album[161];
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
/* Temporary remote-control context used only by the normal navigation map. */
static int bt_control_mode=0;

static int bt_source_selected(void){return audio_source_live&&streq(af.band,"CD");}
static void bt_write_small(const char*path,const char*s){long fd=s_open(path,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd>=0){s_write((int)fd,s,slen(s));s_close((int)fd);}}
static void bt_source_service(void){int on=bt_source_selected();if(on==bt_active_written)return;bt_write_small(BT_ACTIVE_PATH,on?"1\n":"0\n");bt_active_written=on;if(pref.debug)debug_log(STATE_LOG,on?"BT SOURCE ACTIVE (CD)":"BT SOURCE INACTIVE");}
static void bt_command(const char*cmd){char b[48];b[0]=0;append_int(b,sizeof(b),(int)bt_cmd_seq++);appendstr(b,sizeof(b)," ");appendstr(b,sizeof(b),cmd);appendstr(b,sizeof(b),"\n");bt_write_small(BT_CMD_PATH,b);if(pref.debug){char l[64];l[0]=0;appendstr(l,sizeof(l),"BT CMD ");appendstr(l,sizeof(l),cmd);debug_log(STATE_LOG,l);}}
static void bt_command_arg(const char*cmd,const char*arg){char b[80];b[0]=0;append_int(b,sizeof(b),(int)bt_cmd_seq++);appendstr(b,sizeof(b)," ");appendstr(b,sizeof(b),cmd);if(arg&&arg[0]){appendstr(b,sizeof(b)," ");appendstr(b,sizeof(b),arg);}appendstr(b,sizeof(b),"\n");bt_write_small(BT_CMD_PATH,b);if(pref.debug){char l[96];l[0]=0;appendstr(l,sizeof(l),"BT CMD ");appendstr(l,sizeof(l),cmd);if(arg&&arg[0]){appendstr(l,sizeof(l)," ");appendstr(l,sizeof(l),arg);}debug_log(STATE_LOG,l);}}
static void bt_copy_value(char*d,unsigned cap,const char*s){char b[1024];unsigned i=0;while(s[i]&&s[i]!='\r'&&s[i]!='\n'&&i+1<sizeof(b)){b[i]=s[i];i++;}b[i]=0;copystr(d,cap,b);}
static int bt_name_unknown(const char*s){return !s||!s[0]||streq(s,"UNKNOWN DEVICE")||streq(s,"PHONE");}
static int bt_name_cache_idx(const char*addr){for(int i=0;i<nbtname_cache;i++)if(streq(btname_cache[i].addr,addr))return i;return-1;}
static const char* bt_name_cache_get(const char*addr){int i=bt_name_cache_idx(addr);return i>=0?btname_cache[i].name:0;}
static void bt_name_cache_remember(const char*addr,const char*name){if(!addr||!addr[0]||bt_name_unknown(name))return;int i=bt_name_cache_idx(addr);if(i<0){if(nbtname_cache>=BT_UI_MAX_DEVICES)return;i=nbtname_cache++;copystr(btname_cache[i].addr,sizeof(btname_cache[i].addr),addr);}copystr(btname_cache[i].name,sizeof(btname_cache[i].name),name);}
static void bt_name_cache_load(void){char b[768];long fd=s_open(BT_NAMES_PATH,O_RDONLY_,0);if(fd<0)return;long n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);if(n<=0)return;b[n]=0;char*ln=b;for(long i=0;i<=n;i++){if(b[i]=='\r'||b[i]=='\n'||b[i]==0){char save=b[i];b[i]=0;char*p=ln;while(*p&&*p!='|')p++;if(*p=='|'&&p-ln==17){*p=0;bt_name_cache_remember(ln,p+1);}if(!save)break;ln=b+i+1;}}}
static void bt_parse_meta_line(char*ln){
  if(starts(ln,"DEVICE=")){bt_copy_value(btui.device,sizeof(btui.device),ln+7);return;}
  if(starts(ln,"TITLE=")){bt_copy_value(btui.title,sizeof(btui.title),ln+6);return;}
  if(starts(ln,"ARTIST=")){bt_copy_value(btui.artist,sizeof(btui.artist),ln+7);return;}
  if(starts(ln,"ALBUM=")){bt_copy_value(btui.album,sizeof(btui.album),ln+6);return;}
  if(starts(ln,"PAIRED=")){btui.paired=atoi0(ln+7)?1:0;return;}
  if(starts(ln,"CONNECTED=")){btui.connected=atoi0(ln+10)?1:0;return;}
  if(starts(ln,"PLAYING=")){btui.playing=atoi0(ln+8)?1:0;return;}
  if(starts(ln,"POS=")){btui.pos=atoi0(ln+4);if(btui.pos<0)btui.pos=0;return;}
  if(starts(ln,"DUR=")){btui.dur=atoi0(ln+4);if(btui.dur<0)btui.dur=0;return;}
}
static void bt_poll_devices(void){nbtdev=0;char b[1280];long fd=s_open(BT_DEVLIST_PATH,O_RDONLY_,0);if(fd<0)return;long n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);if(n<=0)return;b[n]=0;char*ln=b;for(long i=0;i<=n&&nbtdev<BT_UI_MAX_DEVICES;i++){if(b[i]=='\r'||b[i]=='\n'||b[i]==0){char save=b[i];b[i]=0;if(starts(ln,"DEV=")){char*p=ln+4,*a=p,*c=0,*nm=0;while(*p&&*p!='|')p++;if(*p=='|'){*p=0;c=p+1;p=c;while(*p&&*p!='|')p++;if(*p=='|'){*p=0;nm=p+1;bt_copy_value(btdev[nbtdev].addr,sizeof(btdev[nbtdev].addr),a);btdev[nbtdev].connected=atoi0(c)?1:0;bt_copy_value(btdev[nbtdev].name,sizeof(btdev[nbtdev].name),nm);if(bt_name_unknown(btdev[nbtdev].name)){const char*cn=bt_name_cache_get(btdev[nbtdev].addr);if(cn)copystr(btdev[nbtdev].name,sizeof(btdev[nbtdev].name),cn);else if(btdev[nbtdev].connected&&!bt_name_unknown(btui.device)){copystr(btdev[nbtdev].name,sizeof(btdev[nbtdev].name),btui.device);bt_name_cache_remember(btdev[nbtdev].addr,btui.device);}}else bt_name_cache_remember(btdev[nbtdev].addr,btdev[nbtdev].name);if(!btdev[nbtdev].name[0])copystr(btdev[nbtdev].name,sizeof(btdev[nbtdev].name),"UNKNOWN DEVICE");nbtdev++;}}}if(!save)break;ln=b+i+1;}}}
static int bt_connected_idx(void){for(int i=0;i<nbtdev;i++)if(btdev[i].connected)return i;return-1;}
static int bt_saved_count(void){int c=0;for(int i=0;i<nbtdev;i++)if(!btdev[i].connected)c++;return c;}
static int bt_saved_idx(int ord){for(int i=0;i<nbtdev;i++)if(!btdev[i].connected){if(!ord)return i;ord--;}return-1;}
static void bt_poll_status(void){
  char b[1024];long fd=s_open(BT_STATUS_PATH,O_RDONLY_,0);if(fd<0){copystr(btui.status,sizeof(btui.status),"BT DAEMON OFFLINE");btui.connected=0;btui.playing=0;return;}long n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);if(n<=0){copystr(btui.status,sizeof(btui.status),"BT WAIT PHONE");return;}b[n]=0;bt_copy_value(btui.status,sizeof(btui.status),b);
  if(starts(btui.status,"BT STREAMING")){btui.connected=1;btui.paired=1;btui.playing=1;}
  else if(starts(btui.status,"BT CONNECTED")||starts(btui.status,"BT OPEN")||starts(btui.status,"BT BUFFERING")||starts(btui.status,"BT PAUSED")){btui.connected=1;btui.paired=1;if(starts(btui.status,"BT PAUSED"))btui.playing=0;}
  else{btui.connected=0;btui.playing=0;btui.title[0]=0;btui.artist[0]=0;btui.album[0]=0;btui.pos=btui.dur=0;}
  fd=s_open(BT_META_PATH,O_RDONLY_,0);if(fd>=0){n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);if(n>0){b[n]=0;char*ln=b;for(long i=0;i<=n;i++){if(b[i]=='\r'||b[i]=='\n'||b[i]==0){char save=b[i];b[i]=0;if(ln[0])bt_parse_meta_line(ln);if(!save)break;ln=b+i+1;}}}}bt_poll_devices();
}
static const char* bt_action_label(void){if(btui.connected)return"DISCONNECT PHONE";if(btui.paired)return"RECONNECT PHONE";return"PAIR PHONE";}
static void bt_action_connection(void){if(btui.connected)bt_command("DISCONNECT");else if(btui.paired)bt_command("CONNECT");else bt_command("PAIR");}

/* CPM1/CPM2/CPM3/CPM4 map formats. Header/index layout is shared. CPM3/4
 * keep geometry/labels seekable and add a tile-local metadata table referenced
 * by Z6 road geometry. CPM4 extends each road-metadata record with a flags byte. */
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
static int load_map(void){load_atlas();mapfd=(int)s_open(atlas_ready?atlas_map_path:MAP_PATH,O_RDONLY_,0);if(mapfd<0)return 0;if(s_read(mapfd,&mh,sizeof(mh))!=(long)sizeof(mh)){s_close(mapfd);mapfd=-1;return 0;}if(mh.magic[0]=='C'&&mh.magic[1]=='P'&&mh.magic[2]=='M'&&mh.magic[3]=='1'&&mh.ver==1)mapfmt=1;else if(mh.magic[0]=='C'&&mh.magic[1]=='P'&&mh.magic[2]=='M'&&mh.magic[3]=='2'&&mh.ver==2)mapfmt=2;else if(mh.magic[0]=='C'&&mh.magic[1]=='P'&&mh.magic[2]=='M'&&mh.magic[3]=='3'&&mh.ver==3)mapfmt=3;else if(mh.magic[0]=='C'&&mh.magic[1]=='P'&&mh.magic[2]=='M'&&mh.magic[3]=='4'&&mh.ver==4)mapfmt=4;else if(mh.magic[0]=='C'&&mh.magic[1]=='P'&&mh.magic[2]=='M'&&mh.magic[3]=='5'&&mh.ver==5)mapfmt=5;else mapfmt=0;if(!mapfmt||mh.minlvl>mh.maxlvl||mh.maxlvl>6){s_close(mapfd);mapfd=-1;return 0;}mapok=1;manual_zoom=manual_zoom<(int)mh.minlvl?(int)mh.minlvl:manual_zoom;manual_zoom=manual_zoom>(int)mh.maxlvl?(int)mh.maxlvl:manual_zoom;return 1;}
static int keycmp(u8 l,s32 x,s32 y,const struct CpmIndex*e){if(l<e->lvl)return-1;if(l>e->lvl)return 1;if(x<e->tx)return-1;if(x>e->tx)return 1;if(y<e->ty)return-1;if(y>e->ty)return 1;return 0;}
static int find_index(u8 l,s32 x,s32 y,u8 page,struct CpmIndex*out){u32 lo=0,hi=mh.count;u32 size=mapfmt>=5?24:20;while(lo<hi){u32 mid=lo+(hi-lo)/2;u8 b[24];s_lseek(mapfd,(long)(mh.index_off+mid*size),SEEK_SET_);if(s_read(mapfd,b,size)!=(long)size)return 0;struct CpmIndex e;e.lvl=b[0];e.flags=b[1];e.tx=mapfmt>=5?(s32)rd32u(b+4):(s16)rd16(b+2);e.ty=mapfmt>=5?(s32)rd32u(b+8):(s16)rd16(b+4);unsigned k=mapfmt>=5?12:8;e.off=rd32u(b+k);e.len=rd32u(b+k+4);e.features=rd16(b+k+8);e.pad=rd16(b+k+10);int c=keycmp(l,x,y,&e);if(!c)c=page<e.flags?-1:(page>e.flags?1:0);if(c==0){*out=e;return 1;}if(c<0)hi=mid;else lo=mid+1;}return 0;}
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
static int coslat_q15(s32 lat){int d=lat<0?-lat:lat;d/=100;if(d>=90000)return 0;static const u16 c[91]={32767,32762,32747,32722,32687,32642,32587,32523,32448,32364,32269,32165,32051,31927,31794,31650,31498,31335,31163,30982,30791,30591,30381,30162,29934,29697,29451,29196,28932,28659,28377,28087,27788,27481,27165,26841,26509,26169,25821,25465,25101,24730,24351,23964,23571,23170,22762,22347,21925,21497,21062,20621,20173,19720,19260,18794,18323,17846,17364,16876,16384,15886,15383,14876,14364,13848,13328,12803,12275,11743,11207,10668,10126,9580,9032,8481,7927,7371,6813,6252,5690,5126,4560,3993,3425,2856,2286,1715,1144,572,0};int i=d/1000,f=d%1000;return c[i]+((int)c[i+1]-c[i])*f/1000;}
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
#define FREECAM_ORIENT_ANIM_FRAMES 5 /* Five rendered frames ~= 0.5 s at the normal 10 Hz map cadence. */
#define FC_ANIM_NONE 0
#define FC_ANIM_TO_NORTH 1
#define FC_ANIM_TO_CAR 2
static int freecam=0,freecam_zoom=5,auto_zoom_level=-1,zoom_override=-1;static s32 cam_lat=0,cam_lon=0;
/* freecam_course is also the temporary view orientation while snapping back to
 * the car.  Mode changes are locked until the current five-frame transition
 * has completed, so entry/exit can never reverse halfway through. */
static int freecam_course=0,freecam_course_valid=0,freecam_anim_mode=FC_ANIM_NONE,freecam_anim_frames=0,freecam_anim_hold=0;
/* PERSPECTIVE1: 256=the fixed Waze-style driving tilt, 0=top-down Freecam.
 * The existing five rendered orientation frames also interpolate the tilt. */
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
#define MAP_ANCHOR_X 240
#define FREECAM_ANCHOR_Y 160
#define DRIVE_ANCHOR_Y 224
/* The extra 24 px freed by REV14 makes room for a lower driving anchor.
 * In REV16 the car sits at y=224, still 18 px above the y=242 driver strip.
 * During the existing five-frame Freecam orientation/tilt transition, also
 * interpolate the camera's vertical anchor: no position snap on mode changes.
 * The inverse tile-coverage calculation shares this same animated anchor. */
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
#define PERSPECTIVE_BASE 536u
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
static void world_to_screen(s32 lat,s32 lon,int z,int*ox,int*oy){s32 vl=view_lat(),vo=view_lon();s32 dx=lon_delta(lon-vo),dy=lat-vl;int cq=coslat_q15(vl);s32 wx=lon_scale(dx,cq);int sh=14-z;long long xx=((long long)wx*256)>>sh,yy=-(((long long)dy*256)>>sh);s32 x=xx>16384?16384:xx< -16384?-16384:(s32)xx,y=yy>16384?16384:yy< -16384?-16384:(s32)yy;if(view_rotates()){int k=((view_course()+5)/10)%36;int c=COS36[k],ss=SIN36[k];int rx=(x*c+y*ss)>>15;int ry=(-x*ss+y*c)>>15;x=rx;y=ry;}
  if(y<0&&map_tilt_q8>0){int q=perspective_scale_for_forward(-y);x=(x*q)>>12;y=(y*q)>>12;}
  *ox=MAP_ANCHOR_X+x;*oy=view_anchor_y()+y;
}
/* PERSPECTIVE2: derive the exact visible world-space bounds from the inverse
 * camera rather than the old fixed +/-2-tile neighbourhood.  A projected map
 * viewport is a trapezoid; sample the two sides at its top, camera-height
 * crease and bottom. The anchor is animated in lockstep with the tilt.
 * The eight-pixel vertical and 24-pixel horizontal bleed
 * accommodate the map line widths and integer/lookup rounding.  Inverse
 * projection uses the same quantised 10-degree rotation as world_to_screen().
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
    int k=((view_course()+5)/10)%36,c=COS36[k],ss=SIN36[k];
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
  const int xs[2]={-24,W+24},ys[3]={32,view_anchor_y(),256};
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
static void road_style(int cls,u16*col,int*wid){switch(cls){case 1:*col=C_CYAN;*wid=4;break;case 2:*col=C_BLUE;*wid=3;break;case 3:*col=C_WHITE;*wid=3;break;case 4:*col=C_MUTED;*wid=2;break;case 5:*col=C_DIM;*wid=2;break;default:*col=C_ROAD;*wid=1;break;}}
static void geom_style(int layer,int style,u16*col,int*wid){if(layer==0){road_style(style,col,wid);return;}if(layer==1){*col=C_WATER;*wid=style==1?2:1;return;}if(layer==2){*col=C_GREEN;*wid=1;return;}*col=C_BLUE;*wid=1;}
#define MAP_STRIP_Y 242
#include "map_raster.h"
/* Keep road/route vertices below pixel precision until rasterization. The
 * driving camera and route engine are unchanged; only drawing gains precision. */
static void map_project_q8(s32 lat,s32 lon,int z,int*ox,int*oy){
 if(!MAP_ANTIALIAS){world_to_screen(lat,lon,z,ox,oy);*ox*=256;*oy*=256;return;}
 s32 dx=lon_delta(lon-view_lon()),dy=lat-view_lat();int sh=14-z,cq=coslat_q15(view_lat());
 long long x=((long long)dx*cq)>>(sh-1),y=-(long long)dy*(65536>>sh);
 if(view_rotates()){int k=((view_course()+5)/10)%36,c=COS36[k],ss=SIN36[k];long long rx=(x*c+y*ss)>>15,ry=(-x*ss+y*c)>>15;x=rx;y=ry;}
 if(y<0&&map_tilt_q8>0){int q;long long forward=-y;
  perspective_scale_update();
  if(forward<(long long)PERSPECTIVE_LUT_MAX*256){int i=(int)(forward>>10),f=(int)(forward&1023);q=(perspective_scale_q12[i]*(1024-f)+perspective_scale_q12[i+1]*f+512)>>10;}
  else q=perspective_scale_for_forward((int)(forward>>8));
  x=(x*q)>>12;y=(y*q)>>12;
 }
 if(x>4194304)x=4194304;if(x< -4194304)x=-4194304;if(y>4194304)y=4194304;if(y< -4194304)y=-4194304;*ox=MAP_ANCHOR_X*256+(int)x;*oy=view_anchor_y()*256+(int)y;
}

/* The top-centred journey/zoom card reserves only its actual footprint. */
static int nav_card_x=0,nav_card_w=0,nav_card_h=0;
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
  /* Rebuilt CPM4 packs mark complete clipped polygons with style bit 7.
   * Their bridged hole contours use even/odd filling; old boundary fragments
   * must not independently refill an island. Unmarked legacy packs still use
   * the closed-outline compatibility path. */
  if((mh.flags&2)&&!(style&128))return;
  style&=127;
  if(!MAP_AREA_FILLS||np<4||np>255||(layer!=2&&!(layer==1&&(style==4||style==5))))return;
  if(rd16(p)!=rd16(p+(np-1)*4)||rd16(p+2)!=rd16(p+(np-1)*4+2))return;
  struct MapAreaEdge edges[254];int ne=0,lo=MAP_STRIP_Y,hi=40;
  int x0=0,y0=0;
  for(int i=0;i<np;i++){
    s32 lon=(s32)t->tx*span+((s32)rd16(p+i*4)*span)/4095;
    s32 lat=(s32)t->ty*span+((s32)rd16(p+i*4+2)*span)/4095;
    int x1,y1;world_to_screen(lat,lon,viewz,&x1,&y1);
    if(i&&y0!=y1){
      int ax=x0,ay=y0,bx=x1,by=y1;
      if(ay>by){int tmp=ax;ax=bx;bx=tmp;tmp=ay;ay=by;by=tmp;}
      int first=ay<40?40:ay,last=by>MAP_STRIP_Y?MAP_STRIP_Y:by;
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
 const u8*p=*pp;if(np<2||np>255||p+np*4>end){*pp=end;return;}u16 col;int wid;geom_style(layer,style,&col,&wid);
 map_mask_select(layer==0?((style>=1&&style<=5)?6-style:0):(layer==1?0:(layer==2?1:2)));
 int x[255],y[255];for(int i=0;i<np;i++){int lx=rd16(p),ly=rd16(p+2);p+=4;
  s32 lon=(s32)t->tx*span+((s32)lx*span)/4095,lat=(s32)t->ty*span+((s32)ly*span)/4095;
  map_project_q8(lat,lon,viewz,&x[i],&y[i]);
 }map_stroke_polyline(x,y,np,col,wid);*pp=p;
}
enum {MAP_DRAW_CONTEXT=0,MAP_DRAW_AREAS=1,MAP_DRAW_ROADS=2,MAP_DRAW_OCEAN=3};
static void draw_tile_geometry(struct Tile*t,int viewz,int pass){
  if(!t)return;const u8*p=t->data,*end=t->data+t->len;int span=span_for_level((int)t->lvl);
  if(mapfmt==1){
    if(pass!=MAP_DRAW_ROADS||t->len<2)return;u16 nf=rd16(p);p+=2;
    for(u16 f=0;f<nf&&p+2<=end;f++){int cls=*p++,np=*p++;draw_path(t,span,0,cls,np,&p,end,viewz);}return;
  }
  int hs=mapfmt>=3?8:4;if(t->len<(u32)hs)return;u16 ng=rd16(p);p+=hs;
  for(u16 f=0;f<ng&&p+4<=end;f++){
    int layer=*p++,style=*p++,np=*p++;p++;if(p+np*4>end)return;
    int ocean=layer==1&&style==(128|5);
    if(pass==MAP_DRAW_AREAS||pass==MAP_DRAW_OCEAN){
      if((pass==MAP_DRAW_OCEAN)==ocean)draw_closed_area(t,span,layer,style,np,p,viewz);p+=np*4;
    }
    else{
      int area=mapfmt>=4&&(style&128);
      int wanted=!area&&((pass==MAP_DRAW_ROADS&&layer==0)||(pass==MAP_DRAW_CONTEXT&&layer!=0));
      if(wanted)draw_path(t,span,layer,style,np,&p,end,viewz);else p+=np*4;
    }
  }
}

/* REV13: keep the Freecam label renderer unchanged.  Car-locked view first
 * displays high-priority names allowed by REV12's sparse rules, then fills
 * unused slots with visible names previously excluded by distant filtering.
 * Both passes rank by source priority before screen-space vicinity.  Candidate
 * pool is bounded; no heap, new map format or extra tile reads. */
#define DRIVE_LABEL_CANDIDATES 48
#define DRIVE_ROAD_LABELS 6
#define DRIVE_POI_LABELS 2
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
static int label_free(int x0,int y0,int x1,int y1){if(x0<4||x1>W-4||y1<43||y1>MAP_STRIP_Y-3)return 0;if(nav_card_w>0&&x1>=nav_card_x-3&&x0<=nav_card_x+nav_card_w+3&&y1>=45&&y0<=48+nav_card_h+3)return 0;for(int i=0;i<nlbox;i++)if(!(x1<lbox[i].x0||x0>lbox[i].x1||y1<lbox[i].y0||y0>lbox[i].y1))return 0;if(nlbox<LABEL_BOXES){lbox[nlbox].x0=x0;lbox[nlbox].y0=y0;lbox[nlbox].x1=x1;lbox[nlbox].y1=y1;nlbox++;}return 1;}
static u16 label_color(int kind){if(kind==3)return C_CYAN;if(kind==4)return C_WHITE;if(kind==5)return C_MUTED;if(kind>=6)return C_WARN;if(kind==1)return C_WHITE;return C_MUTED;}
static void poi_icon(int kind,int x,int y){u16 c=kind==6?C_WARN:(kind==8?C_WHITE:C_MUTED);if(kind==6){fill(x-4,y-5,7,9,C_PANEL2);rect(x-4,y-5,7,9,c);fill(x-2,y-3,3,2,c);line1(x+3,y-2,x+6,y+1,c,1);line1(x+6,y+1,x+6,y+5,c,1);}else if(kind==8){fill(x-5,y-1,11,3,c);fill(x-1,y-5,3,11,c);}else{rect(x-5,y-6,11,12,c);text(x-3,y-5,"P",c,1);}}
static void draw_tile_labels(struct Tile*t,int viewz,int plo,int phi){if(mapfmt<2||!t)return;const u8*p=t->data,*end=t->data+t->len;int hs=mapfmt>=3?8:4;if(t->len<(u32)hs)return;u16 ng=rd16(p),nl=rd16(p+2);p+=hs;for(u16 f=0;f<ng&&p+4<=end;f++){int np=p[2];p+=4+(unsigned)np*4;if(p>end)return;}int span=span_for_level((int)t->lvl);for(u16 i=0;i<nl&&p+8<=end;i++){int kind=p[0],prio=p[1];u16 lx=rd16(p+2),ly=rd16(p+4);int tl=p[6];p+=8;if(p+tl>end)return;if(prio>=plo&&prio<=phi&&tl>0&&tl<113){char b[113];int n=tl;for(int j=0;j<n;j++)b[j]=(char)p[j];b[n]=0;s32 lon=(s32)t->tx*span+((s32)lx*span)/4095;s32 lat=(s32)t->ty*span+((s32)ly*span)/4095;int x,y;world_to_screen(lat,lon,viewz,&x,&y);if(!freecam)drive_label_offer(x,y,kind,prio,b);else if(kind>=6){if(label_free(x-8,y-8,x+8,y+8))poi_icon(kind,x,y);}else{int w=textw(b,1),x0=x-w/2,y0=y-6;if(!label_text_seen(b)&&label_free(x0-2,y0-2,x0+w+2,y0+14)){u16 c=label_color(kind);text(x0+1,y0+1,b,C_BG,1);text(x0,y0,b,c,1);label_text_add(b);}}}p+=tl;}}

/* Current road context: at Z6 each road geometry may reference tile-local
 * metadata. CPM3 records are [speed_mph, text_len, text]; CPM4 records are
 * [speed_mph, flags, text_len, text]. Bit 0 means weak/estimated speed. */
struct RoadCtx {int valid,speed,reverse_speed;u8 speed_flags,reverse_flags;char street[113];};static struct RoadCtx roadctx;
/* DEV5.36.5: road-name fallback is longitudinal, not radial.  The normal
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
static int read_road_meta(struct Tile*t,int idx,struct RoadCtx*out){u16 nm=0;const u8*p=cpm_meta_start(t,&nm),*end=t?t->data+t->len:0;if(!p||idx<0||idx>=nm||mapfmt<3)return 0;for(int i=0;i<nm;i++){int speed,flags=0,back=0,bflags=0,tl;if(mapfmt>=5){if(p+5>end)return 0;speed=p[0];flags=p[1];back=p[2];bflags=p[3];tl=p[4];p+=5;}else if(mapfmt>=4){if(p+3>end)return 0;speed=p[0];flags=p[1]|2;back=speed;bflags=flags;tl=p[2];p+=3;}else{if(p+2>end)return 0;speed=p[0];back=speed;flags=bflags=2;tl=p[1];p+=2;}if(p+tl>end)return 0;if(i==idx){out->speed=speed;out->speed_flags=(u8)flags;out->reverse_speed=back;out->reverse_flags=(u8)bflags;int n=tl;if(n>112)n=112;for(int j=0;j<n;j++)out->street[j]=(char)p[j];out->street[n]=0;out->valid=1;return 1;}p+=tl;}return 0;}
static int display_imperial(void);
static int display_limit(int value,int flags){if(!value)return 0;if(display_imperial())return flags&2?value:(value*100000+80467)/160934;return flags&2?(value*160934+50000)/100000:value;}
static void append_road_limit(char*b,unsigned cap,const struct RoadCtx*r){if(!r->valid){appendstr(b,cap,"--");return;}if(r->speed_flags&8){appendstr(b,cap,"VAR");return;}if(r->speed_flags&4){appendstr(b,cap,"INF");return;}if(r->speed<=0){appendstr(b,cap,"--");return;}append_int(b,cap,display_limit(r->speed,r->speed_flags));if(r->speed_flags&1)appendstr(b,cap,"?");}

static u32 seg_dist2(s32 ax,s32 ay,s32 bx,s32 by){int shift=0;while(ax>16383||ax< -16383||ay>16383||ay< -16383||bx>16383||bx< -16383||by>16383||by< -16383){ax>>=1;ay>>=1;bx>>=1;by>>=1;shift++;}s32 vx=bx-ax,vy=by-ay;u32 len2=(u32)(vx*vx+vy*vy);s32 dot=-(ax*vx+ay*vy),qx=ax,qy=ay;if(len2&&dot>0){if((u32)dot>=len2){qx=bx;qy=by;}else{u32 den=len2,num=(u32)dot;while(den>0xffffffu){den=(den+1)>>1;num>>=1;}u32 t=udiv32(num<<8,den);qx=ax+((vx*(s32)t)>>8);qy=ay+((vy*(s32)t)>>8);}}u32 d=square_distance(qx,qy);while(shift--){if(d>0x3fffffffu)return 0xffffffffu;d<<=2;}return d;}
static u32 road_seg_m(s32 la0,s32 lo0,s32 la1,s32 lo1){int cq=coslat_q15((la0+la1)/2);u32 dx=(u32)iabs32(lon_scale(lo1-lo0,cq)),dy=(u32)iabs32(la1-la0);u32 hi=dx>dy?dx:dy,lo=dx>dy?dy:dx;u32 q=hi+((lo*3u)>>3);return udiv32(q*111u+50u,100u);}
static void road_raw_point(struct Tile*t,const u8*p,s32*lat,s32*lon){int span=256;int lx=rd16(p),ly=rd16(p+2);*lon=(s32)t->tx*span+((s32)lx*span)/4095;*lat=(s32)t->ty*span+((s32)ly*span)/4095;}
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
static void scan_road_tile(struct Tile*t,s32 plat,s32 plon,u32*best,struct RoadGeomRef*winner){if(!t||t->lvl!=6||t->len<8)return;const u8*p=t->data,*end=t->data+t->len;u16 ng=rd16(p);p+=8;int span=256,cq=coslat_q15(plat);for(u16 g=0;g<ng&&p+4<=end;g++){int layer=p[0],cls=p[1],np=p[2],mi=p[3];p+=4;if(p+(unsigned)np*4>end)return;if(layer==0&&mi!=255&&np>=2){int lx=rd16(p),ly=rd16(p+2);s32 alon=(s32)t->tx*span+((s32)lx*span)/4095,alat=(s32)t->ty*span+((s32)ly*span)/4095;s32 ax=lon_scale(alon-plon,cq),ay=alat-plat;const u8*q=p+4;for(int i=1;i<np;i++,q+=4){lx=rd16(q);ly=rd16(q+2);s32 blon=(s32)t->tx*span+((s32)lx*span)/4095,blat=(s32)t->ty*span+((s32)ly*span)/4095;s32 bx=lon_scale(blon-plon,cq),by=blat-plat;u32 d=seg_dist2(ax,ay,bx,by);if(gps.mph10>=20){int k=((shown_course()+5)/10)%36;int hx=SIN36[k],hy=COS36[k];s32 vx=bx-ax,vy=by-ay;u32 norm=(u32)(iabs32(vx)+iabs32(vy));if(norm){s32 dp=vx*hx+vy*hy;u32 ad=(u32)(dp<0?-dp:dp);u32 lo=norm*12000u,mid=norm*22000u;if(ad<lo)d+=900;else if(ad<mid)d+=225;}}if(d<*best){*best=d;road_ref_fill(t,g,cls,mi,p,np,i-1,winner);}ax=bx;ay=by;}}p+=(unsigned)np*4;}}
static void roadname_cache_set(const char*s,const struct RoadGeomRef*r){if(!r||!r->valid){roadname_cache.key_valid=roadname_cache.name_valid=0;return;}roadname_cache.key_valid=1;roadname_cache.last=*r;roadname_cache.name_valid=s&&s[0]?1:0;roadname_cache.name[0]=0;if(roadname_cache.name_valid)copystr(roadname_cache.name,sizeof(roadname_cache.name),s);}
static void road_direction_select(struct Tile*t,const struct RoadGeomRef*r){if(mapfmt<5)return;int n=0;const u8*p=road_feature_points(t,r->gi,0,0,&n);if(!p||r->seg<0||r->seg+1>=n)return;s32 la,lo,lb,lob;road_raw_point(t,p+r->seg*4,&la,&lo);road_raw_point(t,p+(r->seg+1)*4,&lb,&lob);int k=((shown_course()+5)/10)%36;long long dot=(long long)lon_scale(lob-lo,coslat_q15(gps.lat))*SIN36[k]+(long long)(lb-la)*COS36[k];if(gps.mph10<30){if(roadctx.speed!=roadctx.reverse_speed||roadctx.speed_flags!=roadctx.reverse_flags){roadctx.speed=0;roadctx.speed_flags=0;}}else if(dot<0){roadctx.speed=roadctx.reverse_speed;roadctx.speed_flags=roadctx.reverse_flags;}}
static void road_estimate_missing(const struct RoadGeomRef*r);
static void update_road_context(void){if(!gps.fix){roadctx_slow_age=0;return;}int moved=roadctx_position_changed(gps.lat,gps.lon);if(gps.mph10<20&&roadctx.valid&&!moved){if(++roadctx_slow_age<3)return;}roadctx_slow_age=0;roadctx.valid=0;roadctx.speed=0;roadctx.speed_flags=0;roadctx.street[0]=0;if(mapfmt<3||!mapok||mh.maxlvl<6)return;s32 plat=gps.lat,plon=gps.lon;int tx=tile_div(plon,6),ty=tile_div(plat,6);u32 best=2500;struct RoadGeomRef winner;memzero(&winner,sizeof(winner));for(int y=ty-1;y<=ty+1;y++)for(int x=tx-1;x<=tx+1;x++)scan_road_tile(get_tile(6,x,y),plat,plon,&best,&winner);if(winner.valid){struct Tile*t=get_tile(6,winner.tx,winner.ty);if(t&&read_road_meta(t,winner.mi,&roadctx)){road_estimate_missing(&winner);t=get_tile(6,winner.tx,winner.ty);if(t)road_direction_select(t,&winner);if(roadctx.street[0])roadname_cache_set(roadctx.street,&winner);else if(roadname_cache.key_valid&&road_ref_same(&roadname_cache.last,&winner)){if(roadname_cache.name_valid)copystr(roadctx.street,sizeof(roadctx.street),roadname_cache.name);}else if(roadname_cache.key_valid&&roadname_cache.name_valid&&road_refs_continuous(&roadname_cache.last,&winner)){copystr(roadctx.street,sizeof(roadctx.street),roadname_cache.name);roadname_cache.last=winner;}else{char traced[113];traced[0]=0;if(road_trace_name(&winner,traced,sizeof(traced))){copystr(roadctx.street,sizeof(roadctx.street),traced);roadname_cache_set(traced,&winner);}else roadname_cache_set(0,&winner);}}}roadctx_lat=plat;roadctx_lon=plon;roadctx_pos_valid=1;}

#include "global_regions.h"
#include "speed_estimates.h"
static int display_imperial(void){if(pref.units)return pref.units==2;if(region_iso[0])return (region_flags&1)!=0;if(roadctx.valid&&mapfmt>=5&&roadctx.speed&&(roadctx.speed_flags&2))return 1;return (region_flags&1)!=0;}
static int display_speed(void){return display_imperial()?(gps.mph10+5)/10:(gps.mph10*160934+500000)/1000000;}

/* CPR2 v4 turn-aware compact contraction-hierarchy runtime.
 * Physical road geometry stays compact. Only OSM turn-restricted junctions
 * gain arrival-context routing states, so CH shortcuts cannot encode an
 * illegal OSM no/only turn manoeuvre. */
struct CprHeader {char magic[4];u16 ver,flags;u32 nodes,phys_nodes,base_edges,route_edges,tiles,tile_refs,ch_arcs,upf_count,upr_count,geom_points,src_refs,tile_off,tref_off,node_off,edge_off,base_off,src_off,arc_off,upf_off,upr_off,geom_off;s32 minlat,minlon,maxlat,maxlon;};
struct CprTile {s32 tx,ty;u32 first,count;};
struct CprNode {s32 lat,lon;u32 first_edge;u16 edge_count,flags;u32 phys,rank,comp,upf_first;u16 upf_count,upr_count;u32 upr_first;};
struct CprBase {u32 cost,geom_first,src_first,to_state;u16 geom_count,src_count;u8 cls,flags;u16 pad;};
struct CprArc {u32 from,to,cost,left,right;u8 cls,flags;u16 pad;};
struct CprPoint {s32 lat,lon;};
/* DEV5.25 QAD1 / DEV5.26 QAD2: optional query-only adjacency sidecar.  Each upward reference
 * is materialised as (other state, cost, original CPR2 arc id).  The CH query
 * can therefore relax a node with one small sequential read instead of reading
 * a u32 ref and then chasing a scattered 24-byte CprArc.  route.cpr remains
 * authoritative for snapping, shortcut unpacking and geometry reconstruction. */
struct QAdjHeader {char magic[4];u16 ver,entry_size;u32 nodes,ch_arcs,upf_count,upr_count,route_edges,route_bytes,fwd_off,rev_off;};
/* DEV5.26 QAD2: node-blocked adjacency.  One u32 offset per route state is
 * loaded into the temporary route arena; each expanded state then needs one
 * sidecar read containing counts + both direction adjacency lists. QAD1 stays
 * accepted for backwards compatibility. */
struct QAdj2Header {char magic[4];u16 ver,entry_size;u32 nodes,ch_arcs,upf_count,upr_count,route_edges,route_bytes,index_off,data_off,data_bytes,flags;};
/* PERF4/QAD3: 64-node-ish group blocks shrink the resident directory from the
 * QAD2 per-node 27 MiB index to a sub-MiB group index.  Reconstruction metadata
 * lives in the same optional sidecar: every CH arc stores only the fields the
 * finaliser needs, and base arcs replace their unused right-child field with a
 * builder-computed geometry total.  CPR2-v4 remains the source of truth. */
struct QAdj3Header {char magic[4];u16 ver,entry_size;u32 nodes,ch_arcs,upf_count,upr_count,route_edges,route_bytes,group_shift,group_count,gindex_off,data_off,data_bytes,rmeta_off,rmeta_bytes,flags;};
struct QAdjEntry {u32 other,cost,arc;};
struct QRec {u32 from,cost,left,right_geom,to_flags;};
/* PERF5/QAD4 keeps the QAD3 group-query layout, but uses sidecar-local CH arc
 * IDs ordered for shortcut-tree locality.  Base records additionally carry
 * their physical geometry descriptor, eliminating CPR base reads while
 * reconstructing the winning CH path. */
struct QRec4 {u32 from,cost,left,right_geom,to_flags,geom_first,geom_info;};
/* PERF6/QAD5: query node blocks are physically written in CH-rank order.
 * A mapped node->block offset index is needed only for seed states; every
 * QAD5 adjacency entry carries the target block offset so discovered CH states
 * never perform another index lookup. Reconstruction records remain QAD4-
 * compatible sidecar-local IDs. */
struct QAdj5Header {char magic[4];u16 ver,entry_size;u32 nodes,ch_arcs,upf_count,upr_count,route_edges,route_bytes,index_off,index_bytes,data_off,data_bytes,rmeta_off,rmeta_bytes,flags,reserved;};
/* PERF7/QAD6 appends one compact arithmetic record per stored CPR geometry
 * point.  Intermediate cumulative fractions are precomputed in both physical
 * directions and segment metres use the exact runtime integer formula. */
struct QAdj6Header {struct QAdj5Header q;u32 gmeta_off,gmeta_bytes,geom_points,gmeta_entry_size;};
/* DEV5.31 builder accidentally serialized QAD6 geometry fields before the inherited
 * QAD5 flags/reserved words.  DEV5.32 accepts that one released layout so existing
 * sidecars remain usable; corrected builders write QAdj6Header layout above. */
struct QAdj6LegacyHeader {char magic[4];u16 ver,entry_size;u32 nodes,ch_arcs,upf_count,upr_count,route_edges,route_bytes,index_off,index_bytes,data_off,data_bytes,rmeta_off,rmeta_bytes,gmeta_off,gmeta_bytes,geom_points,flags,reserved0,reserved1;};
/* PERF8/QAD7 fuses complete physical geometry with its arithmetic metadata into
 * one reconstruction-local packet stream.  The first 28 bytes of QRec7 remain
 * QRec4-compatible; packet_off is relative to packet_off in QAdj7Header. */
struct QRec7 {u32 from,cost,left,right_geom,to_flags,geom_first,geom_info,packet_off;};
struct QAdj7Header {struct QAdj5Header q;u32 packet_off,packet_bytes,packet_points,packet_entry_size;};
struct QAdj5Entry {u32 other,cost,arc,next_off;};
#define QAD2_NODE_BLOCKS 1u
#define QAD3_GROUP_BLOCKS 1u
#define QAD3_RECON_META 2u
#define QAD4_LOCAL_ARC_IDS 4u
#define QAD5_DIRECT_INDEX 8u
#define QAD5_RANK_ORDER 16u
#define QAD6_GEOM_META 32u
#define QAD6_GMETA_SIZE 12u
#define QAD7_PACKET_GEOM 64u
#define QAD7_POINT_SIZE 20u
#define CPR_ARC_SHORTCUT 1u
#define CPR_ARC_GEOM_REVERSED 2u
static struct CprHeader rh;static struct QAdjHeader qh;static struct QAdj2Header qh2;static struct QAdj3Header qh3;static struct QAdj5Header qh5;static struct QAdj6Header qh6;static struct QAdj7Header qh7;static int routefd=-1,qadjfd=-1,routeok=0,qadjok=0,qadjmode=0;static int qadj6_legacy_header=0;
static u8*qadj_map=0;static u32 qadj_map_bytes=0;static int qadj_map_ok=0;
static u8*route_geom_map=0;static u32 route_geom_map_bytes=0,route_geom_map_delta=0,route_geom_map_fileoff=0;static int route_geom_map_ok=0;
/* dev5.8: routing performs scattered reads from a ~GB-scale CPR2 file.
 * Larger contiguous caches trade only 256 KiB of static RAM for substantially
 * fewer SD-card seeks while CH is active. */
/* DEV5.23 CACHELAB: the DEV5.22 profile showed the 8 KiB node window was
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
/* DEV5.21 profiling: debug-gated counters only. No per-read logging and no
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
static u32 route_prof_prefetch_pages=0,route_prof_prefetch_reads=0,route_prof_prefetch_bytes=0;
static u32 route_prof_gmeta=0,route_prof_div_fallback=0,route_prof_mfast=0,route_prof_packet_pts=0;
static u32 route_prof_build_phase_steps[16]={0};
static u32 route_prof_start_ms=0,route_prof_query_ms=0,route_prof_build_ms=0,route_prof_cpu_ms=0,route_prof_slice_ms=0;
static u32 route_prof_pre_io=0,route_prof_build_io=0;
#define RPROF_INC(x) do{if(route_prof_on)(x)++;}while(0)
#define RPROF_ADD(x,v) do{if(route_prof_on)(x)+=(u32)(v);}while(0)
static u32 route_prof_io_bytes(void){u32 n=route_prof_n_bytes+route_prof_b_bytes+route_prof_a_bytes+route_prof_g_bytes+route_prof_tile_bytes+route_prof_qadj_bytes+route_prof_qidx_bytes+route_prof_qrec_bytes+route_prof_prefetch_bytes;for(int i=0;i<4;i++)n+=route_prof_r_bytes[i];return n;}
static void route_prof_reset(void){route_prof_on=pref.debug?1:0;route_prof_n_hit=route_prof_n_miss=route_prof_n_bytes=0;route_prof_b_hit=route_prof_b_miss=route_prof_b_bytes=0;route_prof_a_hit=route_prof_a_miss=route_prof_a_bytes=0;route_prof_g_hit=route_prof_g_miss=route_prof_g_bytes=0;for(int i=0;i<4;i++)route_prof_r_hit[i]=route_prof_r_miss[i]=route_prof_r_bytes[i]=0;route_prof_tile_reads=route_prof_tile_bytes=route_prof_relax=0;route_prof_qadj_reads=route_prof_qadj_bytes=route_prof_qidx_bytes=route_prof_strong_stop=route_prof_geom_reuse_arcs=route_prof_geom_reuse_pts=0;route_prof_qrec_hit=route_prof_qrec_miss=route_prof_qrec_bytes=0;route_prof_stall=route_prof_qmap_access=route_prof_gmap_access=0;route_prof_prefetch_pages=route_prof_prefetch_reads=route_prof_prefetch_bytes=0;route_prof_gmeta=route_prof_div_fallback=route_prof_mfast=route_prof_packet_pts=0;memzero(route_prof_build_phase_steps,sizeof(route_prof_build_phase_steps));route_prof_query_ms=route_prof_build_ms=route_prof_cpu_ms=route_prof_slice_ms=0;route_prof_pre_io=route_prof_build_io=0;route_prof_start_ms=route_prof_on?s_now_ms():0;}
static int qadj3_load_index(void);
static void qadj_reset_caches(void);
static void qadj_unmap(void){qadj_reset_caches();if(qadj_map){s_munmap(qadj_map,qadj_map_bytes);qadj_map=0;}qadj_map_bytes=0;qadj_map_ok=0;}
static int qadj_try_map(u32 bytes){qadj_unmap();if(!bytes)return 0;void*p=s_mmap2(0,bytes,PROT_READ_,MAP_PRIVATE_,qadjfd,0);if(!p)return 0;qadj_map=(u8*)p;qadj_map_bytes=bytes;qadj_map_ok=1;return 1;}
static void route_geom_unmap(void){if(route_geom_map){s_munmap(route_geom_map,route_geom_map_bytes);route_geom_map=0;}route_geom_map_bytes=route_geom_map_delta=route_geom_map_fileoff=0;route_geom_map_ok=0;}
static int route_geom_try_map(void){route_geom_unmap();if(routefd<0||!rh.geom_points)return 0;u32 page=rh.geom_off&~4095u,delta=rh.geom_off-page;u32 gb=rh.geom_points*(u32)sizeof(struct CprPoint);if(gb/sizeof(struct CprPoint)!=rh.geom_points||delta>0xffffffffu-gb)return 0;u32 len=delta+gb;void*p=s_mmap2(0,len,PROT_READ_,MAP_PRIVATE_,routefd,page>>12);if(!p)return 0;route_geom_map=(u8*)p;route_geom_map_bytes=len;route_geom_map_delta=delta;route_geom_map_fileoff=page;route_geom_map_ok=1;return 1;}
static void load_qadj_data(void){
  qadjok=0;qadjmode=0;qadj6_legacy_header=0;memzero(&qh,sizeof(qh));memzero(&qh2,sizeof(qh2));memzero(&qh3,sizeof(qh3));memzero(&qh5,sizeof(qh5));memzero(&qh6,sizeof(qh6));memzero(&qh7,sizeof(qh7));
  qadj_unmap();if(qadjfd>=0){s_close(qadjfd);qadjfd=-1;}qadjfd=(int)s_open(atlas_ready?atlas_qadj_path:QADJ_PATH,O_RDONLY_,0);if(qadjfd<0)return;
  char m[4];if(s_read(qadjfd,m,4)!=4)goto bad;s_lseek(qadjfd,0,SEEK_SET_);long rb=s_lseek(routefd,0,SEEK_END_);if(rb<=0)goto bad;
  if(m[0]=='Q'&&m[1]=='A'&&m[2]=='D'&&m[3]=='7'){
    u32 need=QAD3_RECON_META|QAD4_LOCAL_ARC_IDS|QAD5_DIRECT_INDEX|QAD5_RANK_ORDER|QAD7_PACKET_GEOM;
    if(s_read(qadjfd,&qh7,sizeof(qh7))!=(long)sizeof(qh7))goto bad;
    if(qh7.q.ver!=7||qh7.q.entry_size!=sizeof(struct QAdj5Entry)||(qh7.q.flags&need)!=need||qh7.q.nodes!=rh.nodes||qh7.q.ch_arcs!=rh.ch_arcs||qh7.q.upf_count!=rh.upf_count||qh7.q.upr_count!=rh.upr_count||qh7.q.route_edges!=rh.route_edges||(u32)rb!=qh7.q.route_bytes)goto bad;
    if(qh7.q.index_off!=sizeof(struct QAdj7Header)||qh7.q.index_bytes!=rh.nodes*4u||qh7.q.data_off!=qh7.q.index_off+qh7.q.index_bytes||qh7.q.rmeta_off!=qh7.q.data_off+qh7.q.data_bytes||qh7.q.rmeta_bytes!=rh.ch_arcs*(u32)sizeof(struct QRec7))goto bad;
    if(qh7.packet_off!=qh7.q.rmeta_off+qh7.q.rmeta_bytes||qh7.packet_entry_size!=QAD7_POINT_SIZE||qh7.packet_bytes!=qh7.packet_points*QAD7_POINT_SIZE)goto bad;
    long qb=s_lseek(qadjfd,0,SEEK_END_);if(qb<=0||(u32)qb!=qh7.packet_off+qh7.packet_bytes)goto bad;qh5=qh7.q;qadjmode=7;qadjok=1;qadj_try_map((u32)qb);return;
  }
  if(m[0]=='Q'&&m[1]=='A'&&m[2]=='D'&&m[3]=='6'){
    u32 need=QAD3_RECON_META|QAD4_LOCAL_ARC_IDS|QAD5_DIRECT_INDEX|QAD5_RANK_ORDER|QAD6_GEOM_META;int h6ok=0;
    if(s_read(qadjfd,&qh6,sizeof(qh6))==(long)sizeof(qh6)){
      if(qh6.q.ver==6&&qh6.q.entry_size==sizeof(struct QAdj5Entry)&&(qh6.q.flags&need)==need&&qh6.q.nodes==rh.nodes&&qh6.q.ch_arcs==rh.ch_arcs&&qh6.q.upf_count==rh.upf_count&&qh6.q.upr_count==rh.upr_count&&qh6.q.route_edges==rh.route_edges&&(u32)rb==qh6.q.route_bytes&&qh6.q.index_off==sizeof(struct QAdj6Header)&&qh6.q.index_bytes==rh.nodes*4u&&qh6.q.data_off==qh6.q.index_off+qh6.q.index_bytes&&qh6.q.rmeta_off==qh6.q.data_off+qh6.q.data_bytes&&qh6.q.rmeta_bytes==rh.ch_arcs*(u32)sizeof(struct QRec4)&&qh6.gmeta_off==qh6.q.rmeta_off+qh6.q.rmeta_bytes&&qh6.geom_points==rh.geom_points&&qh6.gmeta_entry_size==QAD6_GMETA_SIZE&&qh6.gmeta_bytes==rh.geom_points*QAD6_GMETA_SIZE)h6ok=1;
    }
    if(!h6ok){
      struct QAdj6LegacyHeader l;memzero(&l,sizeof(l));s_lseek(qadjfd,0,SEEK_SET_);
      if(s_read(qadjfd,&l,sizeof(l))!=(long)sizeof(l))goto bad;
      if(l.ver!=6||l.entry_size!=sizeof(struct QAdj5Entry)||(l.flags&need)!=need||l.nodes!=rh.nodes||l.ch_arcs!=rh.ch_arcs||l.upf_count!=rh.upf_count||l.upr_count!=rh.upr_count||l.route_edges!=rh.route_edges||(u32)rb!=l.route_bytes||l.index_off!=sizeof(struct QAdj6LegacyHeader)||l.index_bytes!=rh.nodes*4u||l.data_off!=l.index_off+l.index_bytes||l.rmeta_off!=l.data_off+l.data_bytes||l.rmeta_bytes!=rh.ch_arcs*(u32)sizeof(struct QRec4)||l.gmeta_off!=l.rmeta_off+l.rmeta_bytes||l.geom_points!=rh.geom_points||l.gmeta_bytes!=rh.geom_points*QAD6_GMETA_SIZE)goto bad;
      memzero(&qh6,sizeof(qh6));qh6.q.magic[0]='Q';qh6.q.magic[1]='A';qh6.q.magic[2]='D';qh6.q.magic[3]='6';qh6.q.ver=l.ver;qh6.q.entry_size=l.entry_size;qh6.q.nodes=l.nodes;qh6.q.ch_arcs=l.ch_arcs;qh6.q.upf_count=l.upf_count;qh6.q.upr_count=l.upr_count;qh6.q.route_edges=l.route_edges;qh6.q.route_bytes=l.route_bytes;qh6.q.index_off=l.index_off;qh6.q.index_bytes=l.index_bytes;qh6.q.data_off=l.data_off;qh6.q.data_bytes=l.data_bytes;qh6.q.rmeta_off=l.rmeta_off;qh6.q.rmeta_bytes=l.rmeta_bytes;qh6.q.flags=l.flags;qh6.q.reserved=0;qh6.gmeta_off=l.gmeta_off;qh6.gmeta_bytes=l.gmeta_bytes;qh6.geom_points=l.geom_points;qh6.gmeta_entry_size=QAD6_GMETA_SIZE;qadj6_legacy_header=1;h6ok=1;
    }
    if(!h6ok)goto bad;long qb=s_lseek(qadjfd,0,SEEK_END_);if(qb<=0||(u32)qb!=qh6.gmeta_off+qh6.gmeta_bytes)goto bad;qh5=qh6.q;qadjmode=6;qadjok=1;qadj_try_map((u32)qb);return;
  }
  if(m[0]=='Q'&&m[1]=='A'&&m[2]=='D'&&m[3]=='5'){
    if(s_read(qadjfd,&qh5,sizeof(qh5))!=(long)sizeof(qh5))goto bad;
    u32 need=QAD3_RECON_META|QAD4_LOCAL_ARC_IDS|QAD5_DIRECT_INDEX|QAD5_RANK_ORDER;
    if(qh5.ver!=5||qh5.entry_size!=sizeof(struct QAdj5Entry)||(qh5.flags&need)!=need)goto bad;
    if(qh5.nodes!=rh.nodes||qh5.ch_arcs!=rh.ch_arcs||qh5.upf_count!=rh.upf_count||qh5.upr_count!=rh.upr_count||qh5.route_edges!=rh.route_edges||(u32)rb!=qh5.route_bytes)goto bad;
    if(qh5.index_off!=sizeof(struct QAdj5Header)||qh5.index_bytes!=rh.nodes*4u||qh5.data_off!=qh5.index_off+qh5.index_bytes||qh5.rmeta_off!=qh5.data_off+qh5.data_bytes||qh5.rmeta_bytes!=rh.ch_arcs*(u32)sizeof(struct QRec4))goto bad;
    long qb=s_lseek(qadjfd,0,SEEK_END_);if(qb<=0||(u32)qb!=qh5.rmeta_off+qh5.rmeta_bytes)goto bad;qadjmode=5;qadjok=1;qadj_try_map((u32)qb);return;
  }
  if(m[0]=='Q'&&m[1]=='A'&&m[2]=='D'&&m[3]=='4'){
    if(s_read(qadjfd,&qh3,sizeof(qh3))!=(long)sizeof(qh3))goto bad;
    if(qh3.ver!=4||qh3.entry_size!=sizeof(struct QAdjEntry)||(qh3.flags&(QAD3_GROUP_BLOCKS|QAD3_RECON_META|QAD4_LOCAL_ARC_IDS))!=(QAD3_GROUP_BLOCKS|QAD3_RECON_META|QAD4_LOCAL_ARC_IDS))goto bad;
    if(qh3.nodes!=rh.nodes||qh3.ch_arcs!=rh.ch_arcs||qh3.upf_count!=rh.upf_count||qh3.upr_count!=rh.upr_count||qh3.route_edges!=rh.route_edges||(u32)rb!=qh3.route_bytes||qh3.group_shift<3||qh3.group_shift>8)goto bad;
    u32 want_groups=(rh.nodes+(1u<<qh3.group_shift)-1u)>>qh3.group_shift;if(qh3.group_count!=want_groups||qh3.gindex_off!=sizeof(struct QAdj3Header)||qh3.data_off!=qh3.gindex_off+(qh3.group_count+1u)*4u||qh3.rmeta_off!=qh3.data_off+qh3.data_bytes||qh3.rmeta_bytes!=rh.ch_arcs*(u32)sizeof(struct QRec4))goto bad;
    long qb=s_lseek(qadjfd,0,SEEK_END_);if(qb<=0||(u32)qb!=qh3.rmeta_off+qh3.rmeta_bytes)goto bad;qadjmode=4;qadjok=1;qadj_try_map((u32)qb);return;
  }
  if(m[0]=='Q'&&m[1]=='A'&&m[2]=='D'&&m[3]=='3'){
    if(s_read(qadjfd,&qh3,sizeof(qh3))!=(long)sizeof(qh3))goto bad;
    if(qh3.ver!=3||qh3.entry_size!=sizeof(struct QAdjEntry)||(qh3.flags&(QAD3_GROUP_BLOCKS|QAD3_RECON_META))!=(QAD3_GROUP_BLOCKS|QAD3_RECON_META))goto bad;
    if(qh3.nodes!=rh.nodes||qh3.ch_arcs!=rh.ch_arcs||qh3.upf_count!=rh.upf_count||qh3.upr_count!=rh.upr_count||qh3.route_edges!=rh.route_edges||(u32)rb!=qh3.route_bytes||qh3.group_shift<3||qh3.group_shift>8)goto bad;
    u32 want_groups=(rh.nodes+(1u<<qh3.group_shift)-1u)>>qh3.group_shift;if(qh3.group_count!=want_groups||qh3.gindex_off!=sizeof(struct QAdj3Header)||qh3.data_off!=qh3.gindex_off+(qh3.group_count+1u)*4u||qh3.rmeta_off!=qh3.data_off+qh3.data_bytes||qh3.rmeta_bytes!=rh.ch_arcs*(u32)sizeof(struct QRec))goto bad;
    long qb=s_lseek(qadjfd,0,SEEK_END_);if(qb<=0||(u32)qb!=qh3.rmeta_off+qh3.rmeta_bytes)goto bad;qadjmode=3;qadjok=1;qadj_try_map((u32)qb);return;
  }
  if(m[0]=='Q'&&m[1]=='A'&&m[2]=='D'&&m[3]=='2'){
    if(s_read(qadjfd,&qh2,sizeof(qh2))!=(long)sizeof(qh2))goto bad;if(qh2.ver!=2||qh2.entry_size!=sizeof(struct QAdjEntry)||!(qh2.flags&QAD2_NODE_BLOCKS))goto bad;
    if(qh2.nodes!=rh.nodes||qh2.ch_arcs!=rh.ch_arcs||qh2.upf_count!=rh.upf_count||qh2.upr_count!=rh.upr_count||qh2.route_edges!=rh.route_edges||(u32)rb!=qh2.route_bytes)goto bad;u32 ibytes=(rh.nodes+1u)*4u;if(qh2.index_off!=sizeof(struct QAdj2Header)||qh2.data_off!=qh2.index_off+ibytes)goto bad;long qb=s_lseek(qadjfd,0,SEEK_END_);if(qb<=0||(u32)qb!=qh2.data_off+qh2.data_bytes)goto bad;qadjmode=2;qadjok=1;return;
  }
  if(m[0]=='Q'&&m[1]=='A'&&m[2]=='D'&&m[3]=='1'){
    if(s_read(qadjfd,&qh,sizeof(qh))!=(long)sizeof(qh))goto bad;if(qh.ver!=1||qh.entry_size!=sizeof(struct QAdjEntry))goto bad;
    if(qh.nodes!=rh.nodes||qh.ch_arcs!=rh.ch_arcs||qh.upf_count!=rh.upf_count||qh.upr_count!=rh.upr_count||qh.route_edges!=rh.route_edges||(u32)rb!=qh.route_bytes)goto bad;u32 want_rev=qh.fwd_off+qh.upf_count*(u32)sizeof(struct QAdjEntry);if(qh.fwd_off!=sizeof(struct QAdjHeader)||qh.rev_off!=want_rev)goto bad;long qb=s_lseek(qadjfd,0,SEEK_END_);if(qb<=0||(u32)qb!=qh.rev_off+qh.upr_count*(u32)sizeof(struct QAdjEntry))goto bad;qadjmode=1;qadjok=1;return;
  }
bad:qadj_unmap();s_close(qadjfd);qadjfd=-1;qadjok=0;qadjmode=0;
}
static int load_route_data(void){route_geom_unmap();routeok=0;if(routefd>=0)s_close(routefd);routefd=(int)s_open(atlas_ready?atlas_route_path:ROUTE_PATH,O_RDONLY_,0);if(routefd<0)return 0;if(s_read(routefd,&rh,sizeof(rh))!=(long)sizeof(rh)){s_close(routefd);routefd=-1;return 0;}if(rh.magic[0]!='C'||rh.magic[1]!='P'||rh.magic[2]!='R'||rh.magic[3]!='2'||(rh.ver!=4&&rh.ver!=5)||!rh.nodes||!rh.base_edges||!rh.ch_arcs||!rh.geom_points){s_close(routefd);routefd=-1;return 0;}routeok=1;cpr_bcache_count=0;cpr_acache_tick=cpr_gcache_tick=cpr_tcache_tick=1;for(int i=0;i<CPR_ARC_SLOTS;i++){cpr_acache_first[i]=0;cpr_acache_count[i]=0;cpr_acache_stamp[i]=0;}for(int i=0;i<CPR_GEOM_SLOTS;i++){cpr_gcache_first[i]=0;cpr_gcache_count[i]=0;cpr_gcache_stamp[i]=0;}for(int i=0;i<CPR_TILE_SLOTS;i++){cpr_tcache_first[i]=0;cpr_tcache_count[i]=0;cpr_tcache_stamp[i]=0;}for(int i=0;i<4;i++){cpr_rcache_first[i]=0;cpr_rcache_count[i]=0;}route_geom_try_map();load_qadj_data();if(qadjmode>=3&&qadjmode<5&&!qadj3_load_index()){qadjok=0;qadjmode=0;}return 1;}
static int route_decode_tile(const u8*b,struct CprTile*out){out->tx=rh.ver>=5?(s32)rd32u(b):(s16)rd16(b);out->ty=rh.ver>=5?(s32)rd32u(b+4):(s16)rd16(b+2);u32 k=rh.ver>=5?8:4;out->first=rd32u(b+k);out->count=rd32u(b+k+4);return 1;}
static int route_read_tile(u32 id,struct CprTile*out){if(!routeok||id>=rh.tiles||!out)return 0;u32 size=rh.ver>=5?16:12;u32 per=CPR_TILE_PAGE/size,slot=CPR_TILE_SLOTS,old=0,oldst=0xffffffffu;for(u32 i=0;i<CPR_TILE_SLOTS;i++){if(cpr_tcache_count[i]&&id>=cpr_tcache_first[i]&&id<cpr_tcache_first[i]+cpr_tcache_count[i]){cpr_tcache_stamp[i]=++cpr_tcache_tick;return route_decode_tile((const u8*)cpr_tcache_w[i]+(id-cpr_tcache_first[i])*size,out);}if(!cpr_tcache_count[i]&&slot==CPR_TILE_SLOTS)slot=i;if(cpr_tcache_count[i]&&cpr_tcache_stamp[i]<oldst){oldst=cpr_tcache_stamp[i];old=i;}}if(slot==CPR_TILE_SLOTS)slot=old;u32 first=(id/per)*per,count=rh.tiles-first;if(count>per)count=per;s_lseek(routefd,(long)(rh.tile_off+first*size),SEEK_SET_);long n=s_read(routefd,cpr_tcache_w[slot],count*size);if(n<=0)return 0;RPROF_INC(route_prof_tile_reads);RPROF_ADD(route_prof_tile_bytes,n);cpr_tcache_first[slot]=first;cpr_tcache_count[slot]=(u32)n/size;cpr_tcache_stamp[slot]=++cpr_tcache_tick;return route_decode_tile((const u8*)cpr_tcache_w[slot]+(id-first)*size,out);}
static int route_find_tile(s32 tx,s32 ty,struct CprTile*out){u32 lo=0,hi=rh.tiles;while(lo<hi){u32 m=lo+(hi-lo)/2;struct CprTile t;if(!route_read_tile(m,&t))return 0;int c=tx<t.tx?-1:(tx>t.tx?1:(ty<t.ty?-1:(ty>t.ty?1:0)));if(!c){*out=t;return 1;}if(c<0)hi=m;else lo=m+1;}return 0;}
static int route_read_node(u32 id,struct CprNode*out){if(!routeok||id>=rh.nodes)return 0;RPROF_INC(route_prof_n_miss);s_lseek(routefd,(long)(rh.node_off+id*sizeof(struct CprNode)),SEEK_SET_);long n=s_read(routefd,out,sizeof(*out));if(n>0)RPROF_ADD(route_prof_n_bytes,n);return n==(long)sizeof(*out);}
static int route_read_base(u32 id,struct CprBase*out){if(!routeok||id>=rh.base_edges)return 0;u32 per=CPR_BASE_CACHE/(u32)sizeof(struct CprBase);if(!(id>=cpr_bcache_first&&id<cpr_bcache_first+cpr_bcache_count)){RPROF_INC(route_prof_b_miss);u32 first=(id/per)*per,count=rh.base_edges-first;if(count>per)count=per;s_lseek(routefd,(long)(rh.base_off+first*sizeof(struct CprBase)),SEEK_SET_);long n=s_read(routefd,cpr_bcache_w,count*sizeof(struct CprBase));if(n<=0)return 0;RPROF_ADD(route_prof_b_bytes,n);cpr_bcache_first=first;cpr_bcache_count=(u32)n/(u32)sizeof(struct CprBase);}else RPROF_INC(route_prof_b_hit);*out=((struct CprBase*)cpr_bcache_w)[id-cpr_bcache_first];return 1;}
static int route_read_arc(u32 id,struct CprArc*out){if(!routeok||id>=rh.ch_arcs)return 0;u32 per=CPR_ARC_PAGE/(u32)sizeof(struct CprArc),slot=CPR_ARC_SLOTS,oldest=0,oldstamp=0xffffffffu;for(u32 i=0;i<CPR_ARC_SLOTS;i++){if(cpr_acache_count[i]&&id>=cpr_acache_first[i]&&id<cpr_acache_first[i]+cpr_acache_count[i]){RPROF_INC(route_prof_a_hit);cpr_acache_stamp[i]=++cpr_acache_tick;*out=((struct CprArc*)cpr_acache_w[i])[id-cpr_acache_first[i]];return 1;}if(!cpr_acache_count[i]&&slot==CPR_ARC_SLOTS)slot=i;if(cpr_acache_count[i]&&cpr_acache_stamp[i]<oldstamp){oldstamp=cpr_acache_stamp[i];oldest=i;}}if(slot==CPR_ARC_SLOTS)slot=oldest;RPROF_INC(route_prof_a_miss);u32 first=(id/per)*per,count=rh.ch_arcs-first;if(count>per)count=per;s_lseek(routefd,(long)(rh.arc_off+first*sizeof(struct CprArc)),SEEK_SET_);long n=s_read(routefd,cpr_acache_w[slot],count*sizeof(struct CprArc));if(n<=0)return 0;RPROF_ADD(route_prof_a_bytes,n);cpr_acache_first[slot]=first;cpr_acache_count[slot]=(u32)n/(u32)sizeof(struct CprArc);cpr_acache_stamp[slot]=++cpr_acache_tick;*out=((struct CprArc*)cpr_acache_w[slot])[id-first];return 1;}
static int route_read_ref(int dir,u32 id,u32*out){u32 total=dir==3?rh.src_refs:(dir==2?rh.tile_refs:(dir?rh.upr_count:rh.upf_count)),off=dir==3?rh.src_off:(dir==2?rh.tref_off:(dir?rh.upr_off:rh.upf_off));if((unsigned)dir>=4u||id>=total)return 0;u32 per=CPR_REF_CACHE/4u;if(!(id>=cpr_rcache_first[dir]&&id<cpr_rcache_first[dir]+cpr_rcache_count[dir])){RPROF_INC(route_prof_r_miss[dir]);u32 first=(id/per)*per,count=total-first;if(count>per)count=per;s_lseek(routefd,(long)(off+first*4u),SEEK_SET_);long n=s_read(routefd,cpr_rcache_w[dir],count*4u);if(n<=0)return 0;RPROF_ADD(route_prof_r_bytes[dir],n);cpr_rcache_first[dir]=first;cpr_rcache_count[dir]=(u32)n/4u;}else RPROF_INC(route_prof_r_hit[dir]);*out=cpr_rcache_w[dir][id-cpr_rcache_first[dir]];return 1;}
static int route_read_upref(int dir,u32 id,u32*out){return route_read_ref(dir,id,out);}
#define QADJ_CHUNK 256u
#define QAD2_BLOCK_BYTES 8192u
#define QAD2_INDEX_PAGE 4096u
#define QAD2_INDEX_SLOTS 4u
#define QAD3_GROUP_BYTES 32768u
#define QAD3_GROUP_SLOTS 4u
#define QREC_PAGE 1024u
#define QREC_SLOTS 64u
#define QREC_PER_PAGE (QREC_PAGE/(u32)sizeof(struct QRec))
/* PERF5/QAD4: reconstruction is latency-bound, not bandwidth-bound.  Keep a
 * 256 KiB, 4 KiB-page LRU for locality-renumbered reconstruction records. */
#define QREC4_PAGE 4096u
#define QREC4_SLOTS 64u
#define QREC4_PER_PAGE (QREC4_PAGE/(u32)sizeof(struct QRec4))
static struct QAdjEntry qadj_buf[QADJ_CHUNK];
static u32 qadj2_buf_w[QAD2_BLOCK_BYTES/4u];
static u32 qadj5_buf_w[QAD3_GROUP_BYTES/4u];
static u32 qadj5_idx_w[QAD2_INDEX_SLOTS][QAD2_INDEX_PAGE/4u];static u32 qadj5_idx_first[QAD2_INDEX_SLOTS]={0},qadj5_idx_count[QAD2_INDEX_SLOTS]={0},qadj5_idx_stamp[QAD2_INDEX_SLOTS]={0},qadj5_idx_tick=1;
static u32 qadj2_idx_w[QAD2_INDEX_SLOTS][QAD2_INDEX_PAGE/4u];static u32 qadj2_idx_first[QAD2_INDEX_SLOTS]={0},qadj2_idx_count[QAD2_INDEX_SLOTS]={0},qadj2_idx_stamp[QAD2_INDEX_SLOTS]={0},qadj2_idx_tick=1;
static u32*qadj_index=0;static u32 qadj_index_bytes=0;
static u32 qadj3_group_w[QAD3_GROUP_SLOTS][QAD3_GROUP_BYTES/4u];static u32 qadj3_group_id[QAD3_GROUP_SLOTS]={0},qadj3_group_len[QAD3_GROUP_SLOTS]={0},qadj3_group_stamp[QAD3_GROUP_SLOTS]={0},qadj3_group_tick=1;
static struct QRec qrec_cache[QREC_SLOTS][QREC_PER_PAGE];static u32 qrec_first[QREC_SLOTS]={0},qrec_count[QREC_SLOTS]={0},qrec_stamp[QREC_SLOTS]={0},qrec_tick=1;
static struct QRec4 qrec4_cache[QREC4_SLOTS][QREC4_PER_PAGE];static u32 qrec4_first[QREC4_SLOTS]={0},qrec4_count[QREC4_SLOTS]={0},qrec4_stamp[QREC4_SLOTS]={0},qrec4_tick=1;
/* Local state/arc IDs and directory offsets belong to one regional file.
 * In particular, a paged QAD7 must never reuse another pack's seed offsets.
 * Callers release the query arena before releasing the older QAD3 directory. */
static void qadj_reset_caches(void){
 if(qadj_index&&!qadj_map_ok){
#ifdef HOST_TEST
  free(qadj_index);
#else
  s_brk((long)qadj_index);
#endif
 }
 qadj_index=0;qadj_index_bytes=0;
 memzero(qadj5_idx_count,sizeof(qadj5_idx_count));memzero(qadj2_idx_count,sizeof(qadj2_idx_count));
 memzero(qadj3_group_len,sizeof(qadj3_group_len));memzero(qrec_count,sizeof(qrec_count));memzero(qrec4_count,sizeof(qrec4_count));
 qadj5_idx_tick=qadj2_idx_tick=qadj3_group_tick=qrec_tick=qrec4_tick=1;
}
static int qadj_read_chunk(int dir,u32 first,u32 count,struct QAdjEntry**out){if(qadjmode!=1||!out||!count||count>QADJ_CHUNK)return 0;u32 total=dir?qh.upr_count:qh.upf_count;if(first>=total||count>total-first)return 0;u32 base=dir?qh.rev_off:qh.fwd_off;s_lseek(qadjfd,(long)(base+first*(u32)sizeof(struct QAdjEntry)),SEEK_SET_);u32 bytes=count*(u32)sizeof(struct QAdjEntry);long n=s_read(qadjfd,qadj_buf,bytes);if(n>0){RPROF_INC(route_prof_qadj_reads);RPROF_ADD(route_prof_qadj_bytes,n);}if(n!=(long)bytes)return 0;*out=qadj_buf;return 1;}
static int qadj2_index_get(u32 idx,u32*out){if(qadjmode!=2||!out||idx>rh.nodes)return 0;u32 per=QAD2_INDEX_PAGE/4u,first=(idx/per)*per,slot=QAD2_INDEX_SLOTS,old=0,oldst=0xffffffffu;for(u32 i=0;i<QAD2_INDEX_SLOTS;i++){if(qadj2_idx_count[i]&&idx>=qadj2_idx_first[i]&&idx<qadj2_idx_first[i]+qadj2_idx_count[i]){qadj2_idx_stamp[i]=++qadj2_idx_tick;*out=qadj2_idx_w[i][idx-qadj2_idx_first[i]];return 1;}if(!qadj2_idx_count[i]&&slot==QAD2_INDEX_SLOTS)slot=i;if(qadj2_idx_count[i]&&qadj2_idx_stamp[i]<oldst){oldst=qadj2_idx_stamp[i];old=i;}}if(slot==QAD2_INDEX_SLOTS)slot=old;u32 total=rh.nodes+1u,count=total-first;if(count>per)count=per;s_lseek(qadjfd,(long)(qh2.index_off+first*4u),SEEK_SET_);long n=s_read(qadjfd,qadj2_idx_w[slot],count*4u);if(n!=(long)(count*4u))return 0;RPROF_ADD(route_prof_qidx_bytes,n);qadj2_idx_first[slot]=first;qadj2_idx_count[slot]=count;qadj2_idx_stamp[slot]=++qadj2_idx_tick;*out=qadj2_idx_w[slot][idx-first];return 1;}
static int qadj2_read_block(u32 node,u16*fc,u16*rc,struct QAdjEntry**f,struct QAdjEntry**r){u32 a=0,b=0;if(qadjmode!=2||node>=rh.nodes||!fc||!rc||!f||!r||!qadj2_index_get(node,&a)||!qadj2_index_get(node+1u,&b)||b<a||b-a<4u)return 0;u32 bytes=b-a;if(bytes<=QAD2_BLOCK_BYTES){s_lseek(qadjfd,(long)(qh2.data_off+a),SEEK_SET_);long n=s_read(qadjfd,qadj2_buf_w,bytes);if(n>0){RPROF_INC(route_prof_qadj_reads);RPROF_ADD(route_prof_qadj_bytes,n);}if(n!=(long)bytes)return 0;u8*p=(u8*)qadj2_buf_w;*fc=(u16)(p[0]|((u16)p[1]<<8));*rc=(u16)(p[2]|((u16)p[3]<<8));if(4u+((u32)*fc+(u32)*rc)*(u32)sizeof(struct QAdjEntry)!=bytes)return 0;*f=(struct QAdjEntry*)(p+4);*r=*f+*fc;return 1;}u8 h[4];s_lseek(qadjfd,(long)(qh2.data_off+a),SEEK_SET_);long n=s_read(qadjfd,h,4);if(n>0){RPROF_INC(route_prof_qadj_reads);RPROF_ADD(route_prof_qadj_bytes,n);}if(n!=4)return 0;*fc=(u16)(h[0]|((u16)h[1]<<8));*rc=(u16)(h[2]|((u16)h[3]<<8));if(4u+((u32)*fc+(u32)*rc)*(u32)sizeof(struct QAdjEntry)!=bytes)return 0;*f=*r=0;return 2;}
static int qadj2_read_entries(u32 node,int dir,u16 fc,u16 rc,u32 pos,u32 count,struct QAdjEntry**out){u32 a=0;if(!out||!count||count>QADJ_CHUNK||qadjmode!=2||node>=rh.nodes||!qadj2_index_get(node,&a))return 0;u32 cnt=dir?rc:fc;if(pos>=cnt||count>cnt-pos)return 0;u32 rel=a+4u+(dir?(u32)fc*(u32)sizeof(struct QAdjEntry):0u)+pos*(u32)sizeof(struct QAdjEntry);s_lseek(qadjfd,(long)(qh2.data_off+rel),SEEK_SET_);u32 bytes=count*(u32)sizeof(struct QAdjEntry);long n=s_read(qadjfd,qadj_buf,bytes);if(n>0){RPROF_INC(route_prof_qadj_reads);RPROF_ADD(route_prof_qadj_bytes,n);}if(n!=(long)bytes)return 0;*out=qadj_buf;return 1;}
static int qadj3_load_index(void){if(qadjmode<3||qadjmode>=5)return 1;if(qadj_index)return 1;u32 bytes=(qh3.group_count+1u)*4u;if(bytes<8u||bytes>2u*1024u*1024u)return 0;
 if(qadj_map_ok){if(qh3.gindex_off>qadj_map_bytes||bytes>qadj_map_bytes-qh3.gindex_off)return 0;qadj_index=(u32*)(qadj_map+qh3.gindex_off);qadj_index_bytes=bytes;}
 else{
#ifdef HOST_TEST
  qadj_index=(u32*)malloc(bytes);if(!qadj_index)return 0;
#else
  long base=s_brk(0);if(base<=0)return 0;long want=base+(long)((bytes+4095u)&~4095u),got=s_brk(want);if(got<want){s_brk(base);return 0;}qadj_index=(u32*)base;
#endif
  qadj_index_bytes=bytes;s_lseek(qadjfd,(long)qh3.gindex_off,SEEK_SET_);long n=s_read(qadjfd,qadj_index,bytes);if(n!=(long)bytes)goto bad;RPROF_ADD(route_prof_qidx_bytes,n);
 }
 if(qadj_index[0]!=0||qadj_index[qh3.group_count]!=qh3.data_bytes)goto bad;for(u32 i=0;i<qh3.group_count;i++)if(qadj_index[i]>qadj_index[i+1]||qadj_index[i+1]>qh3.data_bytes||qadj_index[i+1]-qadj_index[i]>QAD3_GROUP_BYTES)goto bad;return 1;
bad:
 if(!qadj_map_ok){
#ifdef HOST_TEST
  free(qadj_index);
#else
  s_brk((long)qadj_index);
#endif
 }
 qadj_index=0;qadj_index_bytes=0;return 0;}
static int qadj3_read_block(u32 node,u16*fc,u16*rc,struct QAdjEntry**f,struct QAdjEntry**r){if(qadjmode<3||qadjmode>=5||!qadj_index||node>=rh.nodes||!fc||!rc||!f||!r)return 0;u32 gid=node>>qh3.group_shift;if(gid>=qh3.group_count)return 0;u32 a=qadj_index[gid],b=qadj_index[gid+1u],bytes=b-a;if(!bytes||bytes>QAD3_GROUP_BYTES)return 0;u8*p=0;
 if(qadj_map_ok){if(qh3.data_off+a>qadj_map_bytes||bytes>qadj_map_bytes-(qh3.data_off+a))return 0;p=qadj_map+qh3.data_off+a;RPROF_INC(route_prof_qmap_access);}
 else{u32 slot=QAD3_GROUP_SLOTS,old=0,oldst=0xffffffffu;for(u32 i=0;i<QAD3_GROUP_SLOTS;i++){if(qadj3_group_len[i]&&qadj3_group_id[i]==gid){qadj3_group_stamp[i]=++qadj3_group_tick;slot=i;break;}if(!qadj3_group_len[i]&&slot==QAD3_GROUP_SLOTS)slot=i;if(qadj3_group_len[i]&&qadj3_group_stamp[i]<oldst){oldst=qadj3_group_stamp[i];old=i;}}if(slot==QAD3_GROUP_SLOTS)slot=old;if(!qadj3_group_len[slot]||qadj3_group_id[slot]!=gid){s_lseek(qadjfd,(long)(qh3.data_off+a),SEEK_SET_);long n=s_read(qadjfd,qadj3_group_w[slot],bytes);if(n!=(long)bytes)return 0;RPROF_INC(route_prof_qadj_reads);RPROF_ADD(route_prof_qadj_bytes,n);qadj3_group_id[slot]=gid;qadj3_group_len[slot]=bytes;qadj3_group_stamp[slot]=++qadj3_group_tick;}p=(u8*)qadj3_group_w[slot];}
 u16 nc=(u16)(p[0]|((u16)p[1]<<8));u32 local=node-(gid<<qh3.group_shift);if(local>=nc)return 0;u32 gh=(4u+2u*((u32)nc+1u)+3u)&~3u;if(gh>bytes)return 0;u16 na=(u16)(p[4+local*2u]|((u16)p[5+local*2u]<<8)),nb=(u16)(p[4+(local+1u)*2u]|((u16)p[5+(local+1u)*2u]<<8));if(nb<na||gh+nb>bytes||nb-na<4u)return 0;u8*nptr=p+gh+na;*fc=(u16)(nptr[0]|((u16)nptr[1]<<8));*rc=(u16)(nptr[2]|((u16)nptr[3]<<8));if(4u+((u32)*fc+(u32)*rc)*(u32)sizeof(struct QAdjEntry)!=(u32)(nb-na))return 0;*f=(struct QAdjEntry*)(nptr+4);*r=*f+*fc;return 1;}
static int qadj5_index_get(u32 node,u32*out){if((qadjmode!=5&&qadjmode!=6&&qadjmode!=7)||!out||node>=rh.nodes)return 0;if(qadj_map_ok){u32 at=qh5.index_off+node*4u;if(at>qadj_map_bytes-4u)return 0;*out=rd32u(qadj_map+at);RPROF_INC(route_prof_qmap_access);return *out<qh5.data_bytes;}
 u32 per=QAD2_INDEX_PAGE/4u,first=(node/per)*per,slot=QAD2_INDEX_SLOTS,old=0,oldst=0xffffffffu;for(u32 i=0;i<QAD2_INDEX_SLOTS;i++){if(qadj5_idx_count[i]&&node>=qadj5_idx_first[i]&&node<qadj5_idx_first[i]+qadj5_idx_count[i]){qadj5_idx_stamp[i]=++qadj5_idx_tick;*out=qadj5_idx_w[i][node-qadj5_idx_first[i]];return *out<qh5.data_bytes;}if(!qadj5_idx_count[i]&&slot==QAD2_INDEX_SLOTS)slot=i;if(qadj5_idx_count[i]&&qadj5_idx_stamp[i]<oldst){oldst=qadj5_idx_stamp[i];old=i;}}if(slot==QAD2_INDEX_SLOTS)slot=old;u32 count=rh.nodes-first;if(count>per)count=per;s_lseek(qadjfd,(long)(qh5.index_off+first*4u),SEEK_SET_);long n=s_read(qadjfd,qadj5_idx_w[slot],count*4u);if(n!=(long)(count*4u))return 0;RPROF_ADD(route_prof_qidx_bytes,n);qadj5_idx_first[slot]=first;qadj5_idx_count[slot]=count;qadj5_idx_stamp[slot]=++qadj5_idx_tick;*out=qadj5_idx_w[slot][node-first];return *out<qh5.data_bytes;}
static int qadj5_read_block_off(u32 off,u16*fc,u16*rc,struct QAdj5Entry**f,struct QAdj5Entry**r){if((qadjmode!=5&&qadjmode!=6&&qadjmode!=7)||off>=qh5.data_bytes||!fc||!rc||!f||!r)return 0;u8*p=0;if(qadj_map_ok){u32 at=qh5.data_off+off;if(at>qadj_map_bytes-4u)return 0;p=qadj_map+at;RPROF_INC(route_prof_qmap_access);}else{u8 h[4];s_lseek(qadjfd,(long)(qh5.data_off+off),SEEK_SET_);long n=s_read(qadjfd,h,4);if(n!=4)return 0;*fc=rd16(h);*rc=rd16(h+2);u32 bytes=4u+((u32)*fc+(u32)*rc)*(u32)sizeof(struct QAdj5Entry);if(bytes>QAD3_GROUP_BYTES||bytes>qh5.data_bytes-off)return 0;s_lseek(qadjfd,(long)(qh5.data_off+off),SEEK_SET_);n=s_read(qadjfd,qadj5_buf_w,bytes);if(n!=(long)bytes)return 0;RPROF_INC(route_prof_qadj_reads);RPROF_ADD(route_prof_qadj_bytes,n);p=(u8*)qadj5_buf_w;}
 *fc=rd16(p);*rc=rd16(p+2);u32 bytes=4u+((u32)*fc+(u32)*rc)*(u32)sizeof(struct QAdj5Entry);if(bytes>QAD3_GROUP_BYTES||bytes>qh5.data_bytes-off)return 0;*f=(struct QAdj5Entry*)(p+4);*r=*f+*fc;return 1;}
static int qadj5_read_block_node(u32 node,u32*qoff,u16*fc,u16*rc,struct QAdj5Entry**f,struct QAdj5Entry**r){u32 off=0;if(!qadj5_index_get(node,&off))return 0;if(qoff)*qoff=off;return qadj5_read_block_off(off,fc,rc,f,r);}
static int qadj3_read_qrec(u32 id,struct QRec*out){if(qadjmode!=3||!out||id>=rh.ch_arcs)return 0;u32 per=QREC_PER_PAGE,slot=QREC_SLOTS,old=0,oldst=0xffffffffu;for(u32 i=0;i<QREC_SLOTS;i++){if(qrec_count[i]&&id>=qrec_first[i]&&id<qrec_first[i]+qrec_count[i]){RPROF_INC(route_prof_qrec_hit);qrec_stamp[i]=++qrec_tick;*out=qrec_cache[i][id-qrec_first[i]];return 1;}if(!qrec_count[i]&&slot==QREC_SLOTS)slot=i;if(qrec_count[i]&&qrec_stamp[i]<oldst){oldst=qrec_stamp[i];old=i;}}if(slot==QREC_SLOTS)slot=old;RPROF_INC(route_prof_qrec_miss);u32 first=(id/per)*per,count=rh.ch_arcs-first;if(count>per)count=per;s_lseek(qadjfd,(long)(qh3.rmeta_off+first*(u32)sizeof(struct QRec)),SEEK_SET_);long n=s_read(qadjfd,qrec_cache[slot],count*sizeof(struct QRec));if(n!=(long)(count*sizeof(struct QRec)))return 0;RPROF_ADD(route_prof_qrec_bytes,n);qrec_first[slot]=first;qrec_count[slot]=count;qrec_stamp[slot]=++qrec_tick;*out=qrec_cache[slot][id-first];return 1;}
static int qadj4_read_qrec(u32 id,struct QRec4*out){if((qadjmode!=4&&qadjmode!=5&&qadjmode!=6)||!out||id>=rh.ch_arcs)return 0;u32 roff=qadjmode>=5?qh5.rmeta_off:qh3.rmeta_off;if(qadj_map_ok){u32 at=roff+id*(u32)sizeof(struct QRec4);if(at>qadj_map_bytes-(u32)sizeof(struct QRec4))return 0;const u8*p=qadj_map+at;out->from=rd32u(p);out->cost=rd32u(p+4);out->left=rd32u(p+8);out->right_geom=rd32u(p+12);out->to_flags=rd32u(p+16);out->geom_first=rd32u(p+20);out->geom_info=rd32u(p+24);RPROF_INC(route_prof_qrec_hit);RPROF_INC(route_prof_qmap_access);return 1;}u32 per=QREC4_PER_PAGE,slot=QREC4_SLOTS,old=0,oldst=0xffffffffu;for(u32 i=0;i<QREC4_SLOTS;i++){if(qrec4_count[i]&&id>=qrec4_first[i]&&id<qrec4_first[i]+qrec4_count[i]){RPROF_INC(route_prof_qrec_hit);qrec4_stamp[i]=++qrec4_tick;*out=qrec4_cache[i][id-qrec4_first[i]];return 1;}if(!qrec4_count[i]&&slot==QREC4_SLOTS)slot=i;if(qrec4_count[i]&&qrec4_stamp[i]<oldst){oldst=qrec4_stamp[i];old=i;}}if(slot==QREC4_SLOTS)slot=old;RPROF_INC(route_prof_qrec_miss);u32 first=(id/per)*per,count=rh.ch_arcs-first;if(count>per)count=per;s_lseek(qadjfd,(long)(roff+first*(u32)sizeof(struct QRec4)),SEEK_SET_);long n=s_read(qadjfd,qrec4_cache[slot],count*sizeof(struct QRec4));if(n!=(long)(count*sizeof(struct QRec4)))return 0;RPROF_ADD(route_prof_qrec_bytes,n);qrec4_first[slot]=first;qrec4_count[slot]=count;qrec4_stamp[slot]=++qrec4_tick;*out=qrec4_cache[slot][id-first];return 1;}
static u32 qrec_to(const struct QRec*q){return q->to_flags&0x00ffffffu;}static u8 qrec_flags(const struct QRec*q){return(u8)(q->to_flags>>24);}
static u32 qrec4_to(const struct QRec4*q){return q->to_flags&0x00ffffffu;}static u8 qrec4_flags(const struct QRec4*q){return(u8)(q->to_flags>>24);}static u16 qrec4_geom_count(const struct QRec4*q){return(u16)(q->geom_info&0xffffu);}static u8 qrec4_base_flags(const struct QRec4*q){return(u8)((q->geom_info>>16)&0xffu);}static u8 qrec4_base_cls(const struct QRec4*q){return(u8)(q->geom_info>>24);}
static int qadj7_read_qrec(u32 id,struct QRec7*out){if(qadjmode!=7||!out||id>=rh.ch_arcs)return 0;u32 at=qh7.q.rmeta_off+id*(u32)sizeof(struct QRec7);if(qadj_map_ok){if(at>qadj_map_bytes-(u32)sizeof(struct QRec7))return 0;const u8*p=qadj_map+at;out->from=rd32u(p);out->cost=rd32u(p+4);out->left=rd32u(p+8);out->right_geom=rd32u(p+12);out->to_flags=rd32u(p+16);out->geom_first=rd32u(p+20);out->geom_info=rd32u(p+24);out->packet_off=rd32u(p+28);RPROF_INC(route_prof_qrec_hit);RPROF_INC(route_prof_qmap_access);return 1;}s_lseek(qadjfd,(long)at,SEEK_SET_);long n=s_read(qadjfd,out,sizeof(*out));if(n!=(long)sizeof(*out))return 0;RPROF_INC(route_prof_qrec_miss);RPROF_ADD(route_prof_qrec_bytes,n);return 1;}
static u32 qrec7_to(const struct QRec7*q){return q->to_flags&0x00ffffffu;}static u8 qrec7_flags(const struct QRec7*q){return(u8)(q->to_flags>>24);}static u16 qrec7_geom_count(const struct QRec7*q){return(u16)(q->geom_info&0xffffu);}static u8 qrec7_base_flags(const struct QRec7*q){return(u8)((q->geom_info>>16)&0xffu);}static u8 qrec7_base_cls(const struct QRec7*q){return(u8)(q->geom_info>>24);}
static void qrec7_from4(struct QRec7*d,const struct QRec4*s){d->from=s->from;d->cost=s->cost;d->left=s->left;d->right_geom=s->right_geom;d->to_flags=s->to_flags;d->geom_first=s->geom_first;d->geom_info=s->geom_info;d->packet_off=0xffffffffu;}
static int qadj7_packet_point(const struct QRec7*q,u32 raw,struct CprPoint*out,u32*qf,u32*qr,u16*mf,u16*mr){if(qadjmode!=7||!q||!out||!qf||!qr||!mf||!mr)return 0;u32 gc=qrec7_geom_count(q);if(raw>=gc||q->packet_off==0xffffffffu)return 0;u32 rel=q->packet_off+raw*QAD7_POINT_SIZE;if(rel>qh7.packet_bytes-QAD7_POINT_SIZE)return 0;u32 at=qh7.packet_off+rel;u8 tmp[QAD7_POINT_SIZE];const u8*p=0;if(qadj_map_ok){if(at>qadj_map_bytes-QAD7_POINT_SIZE)return 0;p=qadj_map+at;RPROF_INC(route_prof_qmap_access);}else{s_lseek(qadjfd,(long)at,SEEK_SET_);long n=s_read(qadjfd,tmp,QAD7_POINT_SIZE);if(n!=(long)QAD7_POINT_SIZE)return 0;p=tmp;RPROF_INC(route_prof_qadj_reads);RPROF_ADD(route_prof_qadj_bytes,QAD7_POINT_SIZE);}out->lat=(s32)rd32u(p);out->lon=(s32)rd32u(p+4);u16 fl=rd16(p+16);*qf=(fl&1u)?65536u:(u32)rd16(p+8);*qr=(fl&2u)?65536u:(u32)rd16(p+10);*mf=rd16(p+12);*mr=rd16(p+14);RPROF_INC(route_prof_packet_pts);return 1;}
static int qadj6_gmeta(u32 raw,u32*qf,u32*qr,u16*mf,u16*mr){if(qadjmode!=6||!qadj_map_ok||raw>=qh6.geom_points||!qf||!qr||!mf||!mr)return 0;u32 at=qh6.gmeta_off+raw*QAD6_GMETA_SIZE;if(at>qadj_map_bytes-QAD6_GMETA_SIZE)return 0;const u8*p=qadj_map+at;u16 fl=rd16(p+8);*qf=(fl&1u)?65536u:(u32)rd16(p);*qr=(fl&2u)?65536u:(u32)rd16(p+2);*mf=rd16(p+4);*mr=rd16(p+6);RPROF_INC(route_prof_gmeta);return 1;}
static int route_read_srcref(u32 id,u32*out){return route_read_ref(3,id,out);}
/* Optional v5 flag2 extension; read lossless OSM way IDs without changing
 * legacy BASE records or allocating an identity table in device RAM. */
static int route_read_way(u32 base,u32*lo,u32*hi){if(!routeok||!(rh.flags&2)||base>=rh.base_edges)return 0;u32 at=rh.geom_off+rh.geom_points*8u+base*8u;u8 b[8];if(at>0x7ffffff7u||s_lseek(routefd,(long)at,SEEK_SET_)<0||s_read(routefd,b,8)!=8)return 0;*lo=rd32u(b);*hi=rd32u(b+4);return 1;}
static int route_read_point(u32 id,struct CprPoint*out){if(!routeok||id>=rh.geom_points||!out)return 0;if(route_geom_map_ok){u32 at=route_geom_map_delta+id*(u32)sizeof(struct CprPoint);if(at>route_geom_map_bytes-(u32)sizeof(struct CprPoint))return 0;const u8*p=route_geom_map+at;out->lat=(s32)rd32u(p);out->lon=(s32)rd32u(p+4);RPROF_INC(route_prof_gmap_access);return 1;}u32 per=CPR_GEOM_PAGE/(u32)sizeof(struct CprPoint),slot=CPR_GEOM_SLOTS,old=0,oldst=0xffffffffu;for(u32 i=0;i<CPR_GEOM_SLOTS;i++){if(cpr_gcache_count[i]&&id>=cpr_gcache_first[i]&&id<cpr_gcache_first[i]+cpr_gcache_count[i]){RPROF_INC(route_prof_g_hit);cpr_gcache_stamp[i]=++cpr_gcache_tick;*out=((struct CprPoint*)cpr_gcache_w[i])[id-cpr_gcache_first[i]];return 1;}if(!cpr_gcache_count[i]&&slot==CPR_GEOM_SLOTS)slot=i;if(cpr_gcache_count[i]&&cpr_gcache_stamp[i]<oldst){oldst=cpr_gcache_stamp[i];old=i;}}if(slot==CPR_GEOM_SLOTS)slot=old;RPROF_INC(route_prof_g_miss);u32 first=(id/per)*per,count=rh.geom_points-first;if(count>per)count=per;s_lseek(routefd,(long)(rh.geom_off+first*sizeof(struct CprPoint)),SEEK_SET_);long n=s_read(routefd,cpr_gcache_w[slot],count*sizeof(struct CprPoint));if(n<=0)return 0;RPROF_ADD(route_prof_g_bytes,n);cpr_gcache_first[slot]=first;cpr_gcache_count[slot]=(u32)n/(u32)sizeof(struct CprPoint);cpr_gcache_stamp[slot]=++cpr_gcache_tick;*out=((struct CprPoint*)cpr_gcache_w[slot])[id-first];return 1;}
static int route_base_point(const struct CprBase*a,u32 pos,struct CprPoint*out){if(!a||!a->geom_count||pos>=a->geom_count)return 0;u32 raw=(a->flags&CPR_ARC_GEOM_REVERSED)?(a->geom_count-1u-pos):pos;return route_read_point(a->geom_first+raw,out);}
#define ROUTE_SNAP_D2 22500u
#define ROUTE_DEST_SNAP_D2 160000u
#define ROUTE_DEST_CAND_MAX 12
/* dev5.5+ exact edge snapping.  CPR2 v4 still stores compact topology, but a
 * snap now retains the exact projected position inside a geometry segment.
 * frac_q16 is 0..65536 along geom_seg -> geom_seg+1 in logical arc order.
 * along_u/total_u are metric-proportional polyline units used for exact
 * partial-edge cost and for distributing ETA cost along compressed geometry. */
struct RouteSnapCandidate {u32 node,arc,d2,score,extra,geom_seg,frac_q16,along_u,total_u;s32 snap_lat,snap_lon;};
/* dev5.6: snapping is geometric first.  Road class must not pull a POI off
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
static int route_snap_node(s32 lat,s32 lon,struct RouteSnapCandidate*out){struct RouteSnapCandidate c[8];/* PERF5: the live car should normally lie on a road indexed by its current 256-unit Z6 tile. Avoid a 3x3 scan unless that exact tile cannot produce a legal nearby road. */int n=route_snap_candidates(lat,lon,ROUTE_SNAP_D2,0,0,c,8);if(n<1)n=route_snap_candidates(lat,lon,ROUTE_SNAP_D2,1,0,c,8);if(n<1)return 0;if(out)*out=c[0];return 1;}

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
 * route[i-1] -> route[i].  This lets dev5.9 distinguish a nearby parallel
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
/* DEV5.17: compact next-manoeuvre guidance is derived entirely from the
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

/* DEV5.22 local CH repair.
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
/* DEV5.29 repair-splice guard.  Fast local repair is allowed to reuse the old
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
enum {RB_IDLE=0,RB_TRACE_F,RB_COPY_F,RB_TRACE_B,RB_START_INIT,RB_START_POINTS,RB_UNPACK,RB_PREFETCH,RB_BASE_NEXT,RB_BASE_TOTAL,RB_BASE_OUTPUT,RB_DEST_INIT,RB_DEST_POINTS,RB_REPAIR_SUFFIX,RB_DONE};
static int route_build_phase=RB_IDLE,route_build_nf=0,route_build_nb=0,route_build_copy=-1,route_build_arc_idx=0,route_build_sp=0,route_build_after_base=RB_UNPACK;
static u32 route_build_u=0,route_build_i=0,route_build_total=0,route_build_cum=0,route_build_prev_cost=0;
static struct QRec7 route_base_meta[ROUTE_PATH_MAX];static int route_base_meta_n=0,route_base_meta_idx=0;
static u32 route_prefetch_page[ROUTE_PATH_MAX];static int route_prefetch_n=0,route_prefetch_idx=0;
static u32 route_prefetch_scratch[8192];
static struct CprArc route_build_arc;static struct CprBase route_build_base;static struct CprPoint route_build_prev_point;
#define ROUTE_GEOM_TMP_MAX 256u
static struct CprPoint route_geom_tmp[ROUTE_GEOM_TMP_MAX];static u32 route_geom_cum_tmp[ROUTE_GEOM_TMP_MAX];static int route_build_geom_cached=0;
#define ROUTE_FRAME_MS 100u
#define ROUTE_DEADLINE_GUARD_MS 6u
/* PERF2: guarantee useful routing work without returning to DEV5.7 freezes.
 * Stationary initial calculations may consume most of a frame; moving/re-route
 * work remains deliberately more conservative. */
#define ROUTE_INITIAL_SLICE_MS 80u
#define ROUTE_MOVING_SLICE_MS 55u
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
  for(;;){if(++guard>ROUTE_PATH_MAX)return 0;struct ChState*x=ch_state(u,0);if(!x)return 0;if(x->pf==ROUTE_NOARC){si=route_repair_start_for_node(u);break;}if(qadjmode==7){struct QRec7 q;if(!qadj7_read_qrec(x->pf,&q))return 0;u=q.from;}else if(qadjmode>=4){struct QRec4 q;if(!qadj4_read_qrec(x->pf,&q))return 0;u=q.from;}else if(qadjmode==3){struct QRec q;if(!qadj3_read_qrec(x->pf,&q))return 0;u=q.from;}else{struct CprArc a;if(!route_read_arc(x->pf,&a))return 0;u=a.from;}}
  u=route_meet;guard=0;
  for(;;){if(++guard>ROUTE_PATH_MAX)return 0;struct ChState*x=ch_state(u,0);if(!x)return 0;if(x->pb==ROUTE_NOARC){ci=route_repair_candidate_for_node(u);break;}if(qadjmode==7){struct QRec7 q;if(!qadj7_read_qrec(x->pb,&q))return 0;u=qrec7_to(&q);}else if(qadjmode>=4){struct QRec4 q;if(!qadj4_read_qrec(x->pb,&q))return 0;u=qrec4_to(&q);}else if(qadjmode==3){struct QRec q;if(!qadj3_read_qrec(x->pb,&q))return 0;u=qrec_to(&q);}else{struct CprArc a;if(!route_read_arc(x->pb,&a))return 0;u=a.to;}}
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
  route_emit_arc=route_emit_phys=ROUTE_NOARC;route_build_phase=RB_IDLE;route_build_nf=route_build_nb=route_build_arc_idx=route_build_sp=0;route_build_copy=-1;route_build_after_base=RB_UNPACK;route_base_meta_n=route_base_meta_idx=route_prefetch_n=route_prefetch_idx=0;route_build_u=route_build_i=route_build_total=route_build_cum=route_build_prev_cost=0;route_build_steps=0;
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

static void ch_reset(void){if(chbucket&&chbucket_n)memzero(chbucket,chbucket_n*4u);chst_n=chhf_n=chhb_n=0;chheap_peak=0;route_expanded=0;route_best=route_meet=ROUTE_INF;route_n=0;route_head=0;route_result_cost=0;route_result_m=0;route_tick=0;route_arc_check_tick=0;route_offroute_checks=0;route_emit_arc=route_emit_phys=ROUTE_NOARC;route_build_phase=RB_IDLE;route_build_nf=route_build_nb=route_build_arc_idx=route_build_sp=0;route_build_copy=-1;route_build_after_base=RB_UNPACK;route_base_meta_n=route_base_meta_idx=route_prefetch_n=route_prefetch_idx=0;route_sched_slices=route_sched_yields=route_build_steps=0;nav_maneuver_reset();}
static void ch_update_meet(struct ChState*s){if(!s||s->df==ROUTE_INF||s->db==ROUTE_INF)return;u32 v=sat_add32(s->df,s->db);if(v<route_best){route_best=v;route_meet=s->node;}}
static u32 route_exact_from=ROUTE_NOARC,route_exact_to=ROUTE_NOARC;
static void route_mark_query_start(void){route_query_lat=route_exact_from==ROUTE_NOARC?(atlas_probe?atlas_origin_lat:car_lat()):route_start_snap.snap_lat;route_query_lon=route_exact_from==ROUTE_NOARC?(atlas_probe?atlas_origin_lon:car_lon()):route_start_snap.snap_lon;}
static int ch_seed_destination(const struct RouteSnapCandidate*dest,u32 comp){if(dest&&dest->arc==ROUTE_NOARC){struct CprNode n;if(!route_read_node(dest->node,&n)||n.comp!=comp)return 0;struct ChState*s=ch_state(dest->node,1);if(!s)return 0;s->db=0;s->pb=ROUTE_NOARC;return ch_heap_insert(1,s);}struct CprBase b;if(!dest||!route_read_base(dest->arc,&b)||!b.src_count)return 0;int seeded=0;for(u32 i=0;i<b.src_count;i++){u32 node;if(!route_read_srcref(b.src_first+i,&node))return 0;struct CprNode rn;if(!route_read_node(node,&rn))return 0;if(rn.comp!=comp)continue;struct ChState*s=ch_state(node,1);if(!s)return 0;if(dest->extra<s->db){s->db=dest->extra;s->pb=ROUTE_NOARC;if(!ch_heap_insert(1,s))return 0;}seeded=1;}return seeded;}
static int route_dest_has_component(const struct RouteSnapCandidate*dest,u32 comp){if(dest&&dest->arc==ROUTE_NOARC){struct CprNode n;return route_read_node(dest->node,&n)&&n.comp==comp;}struct CprBase b;if(!dest||!route_read_base(dest->arc,&b))return 0;for(u32 i=0;i<b.src_count;i++){u32 node;struct CprNode n;if(!route_read_srcref(b.src_first+i,&node)||!route_read_node(node,&n))return 0;if(n.comp==comp)return 1;}return 0;}
static int ch_begin(const struct RouteSnapCandidate*start,const struct RouteSnapCandidate*dest){if(route_prof_on&&!route_prof_query_ms){route_prof_query_ms=s_now_ms();route_prof_pre_io=route_prof_io_bytes();}ch_reset();struct ChState*a=ch_state(start->node,1);if(!a)return 0;a->df=start->extra;if(!ch_heap_insert(0,a)||!ch_seed_destination(dest,route_dest_comp))return 0;route_mark_query_start();ch_update_meet(a);return 1;}
static int ch_retarget_start(const struct RouteSnapCandidate*start){if(!chst||!chbucket)return 0;chhf_n=0;route_best=route_meet=ROUTE_INF;route_n=0;route_head=0;route_result_cost=0;route_result_m=0;nav_maneuver_reset();route_expanded=0;route_tick=0;chheap_peak=chhb_n;/* dev5.10: a completed route leaves the cooperative finaliser in RB_DONE.
     * A start-only CH retarget must reset every start/path-build field just as
     * a fresh CH query would.  dev5.9 cleared route_n but left RB_DONE set,
     * so the very next routing slice treated an empty old path as a failed
     * finalisation and raised R_ERROR. */route_emit_arc=route_emit_phys=ROUTE_NOARC;route_build_phase=RB_IDLE;route_build_nf=route_build_nb=route_build_arc_idx=route_build_sp=0;route_build_copy=-1;route_build_after_base=RB_UNPACK;route_base_meta_n=route_base_meta_idx=route_prefetch_n=route_prefetch_idx=0;route_build_u=route_build_i=route_build_total=route_build_cum=route_build_prev_cost=0;route_build_steps=0;for(u32 i=0;i<chst_n;i++){chst[i].df=ROUTE_INF;chst[i].pf=ROUTE_NOARC;chst[i].posf=0;chst[i].sf=0;}struct ChState*a=ch_state(start->node,1);if(!a)return 0;a->df=start->extra;if(!ch_heap_insert(0,a))return 0;route_mark_query_start();ch_update_meet(a);return 1;}
static int ch_relax_entries(int dir,u32 du,struct QAdjEntry*qe,u32 cnt){
  for(u32 j=0;j<cnt;j++){
    u32 v=qe[j].other,aid=qe[j].arc;if(v>=rh.nodes||aid>=rh.ch_arcs)return-1;struct ChState*t=ch_state(v,1);if(!t)return-2;u32 nd=sat_add32(du,qe[j].cost);u32*dist=dir?&t->db:&t->df;
    if(nd<*dist){*dist=nd;if(dir)t->pb=aid;else t->pf=aid;if(!ch_heap_insert(dir,t))return-2;ch_update_meet(t);}
  }
  return 1;
}
static int ch_qad5_ensure_off(struct ChState*s){if(!s||(qadjmode!=5&&qadjmode!=6&&qadjmode!=7))return 1;if(s->qoff!=ROUTE_NOARC)return s->qoff<qh5.data_bytes;u32 off=0;if(!qadj5_index_get(s->node,&off))return 0;s->qoff=off;return 1;}
static int ch_qad5_stall(int dir,u32 du,struct QAdj5Entry*f,u16 fc,struct QAdj5Entry*r,u16 rc){struct QAdj5Entry*e=dir?f:r;u32 n=dir?fc:rc;for(u32 i=0;i<n;i++){if(e[i].other>=rh.nodes)continue;struct ChState*t=ch_state(e[i].other,0);if(!t)continue;u32 d=dir?t->db:t->df;if(d!=ROUTE_INF&&sat_add32(d,e[i].cost)<du)return 1;}return 0;}
static int ch_relax_entries5(int dir,u32 du,struct QAdj5Entry*qe,u32 cnt){for(u32 j=0;j<cnt;j++){u32 v=qe[j].other,aid=qe[j].arc,off=qe[j].next_off;if(v>=rh.nodes||aid>=rh.ch_arcs||off>=qh5.data_bytes)return-1;struct ChState*t=ch_state(v,1);if(!t)return-2;if(t->qoff==ROUTE_NOARC)t->qoff=off;else if(t->qoff!=off)return-1;u32 nd=sat_add32(du,qe[j].cost);u32*dist=dir?&t->db:&t->df;if(nd<*dist){*dist=nd;if(dir)t->pb=aid;else t->pf=aid;if(!ch_heap_insert(dir,t))return-2;ch_update_meet(t);}}return 1;}
static u32 route_diag_qad7_expanded=0,route_diag_cpr_expanded=0;
static int ch_expand_one(void){
  u32 mf=ch_heap_min(0),mb=ch_heap_min(1);
  if(route_best!=ROUTE_INF&&mf>=route_best&&mb>=route_best)return 2;
  if(mf==ROUTE_INF&&mb==ROUTE_INF)return route_best==ROUTE_INF?0:2;int dir=(mf>mb);struct ChState*s=ch_heap_pop(dir);if(!s)return 0;if((dir?s->sb:s->sf))return 1;if(dir)s->sb=1;else s->sf=1;ch_update_meet(s);u32 du=dir?s->db:s->df;
  if(qadjmode>=5){if(qadjmode==7)route_diag_qad7_expanded++;if(!ch_qad5_ensure_off(s))return-1;u16 fc=0,rc=0;struct QAdj5Entry*f=0,*r=0;if(!qadj5_read_block_off(s->qoff,&fc,&rc,&f,&r))return-1;if(ch_qad5_stall(dir,du,f,fc,r,rc)){RPROF_INC(route_prof_stall);return 1;}route_expanded++;u32 cnt=dir?rc:fc;RPROF_ADD(route_prof_relax,cnt);if(!cnt)return 1;return ch_relax_entries5(dir,du,dir?r:f,cnt);}
  route_expanded++;
  if(qadjmode>=3){u16 fc=0,rc=0;struct QAdjEntry*f=0,*r=0;if(!qadj3_read_block(s->node,&fc,&rc,&f,&r))return-1;u32 cnt=dir?rc:fc;RPROF_ADD(route_prof_relax,cnt);if(!cnt)return 1;return ch_relax_entries(dir,du,dir?r:f,cnt);}
  if(qadjmode==2){u16 fc=0,rc=0;struct QAdjEntry*f=0,*r=0;int qr=qadj2_read_block(s->node,&fc,&rc,&f,&r);if(!qr)return-1;u32 cnt=dir?rc:fc;RPROF_ADD(route_prof_relax,cnt);if(!cnt)return 1;if(qr==1)return ch_relax_entries(dir,du,dir?r:f,cnt);u32 pos=0;while(pos<cnt){u32 take=cnt-pos;if(take>QADJ_CHUNK)take=QADJ_CHUNK;struct QAdjEntry*qe=0;if(!qadj2_read_entries(s->node,dir,fc,rc,pos,take,&qe))return-1;int z=ch_relax_entries(dir,du,qe,take);if(z<0)return z;pos+=take;}return 1;}
  if(!qadjmode)route_diag_cpr_expanded++;struct CprNode n;if(!route_read_node(s->node,&n))return-1;u32 first=dir?n.upr_first:n.upf_first;u32 cnt=dir?n.upr_count:n.upf_count;RPROF_ADD(route_prof_relax,cnt);if(qadjmode==1){u32 pos=0;while(pos<cnt){u32 take=cnt-pos;if(take>QADJ_CHUNK)take=QADJ_CHUNK;struct QAdjEntry*qe=0;if(!qadj_read_chunk(dir,first+pos,take,&qe))return-1;int z=ch_relax_entries(dir,du,qe,take);if(z<0)return z;pos+=take;}return 1;}for(u32 i=0;i<cnt;i++){u32 aid;if(!route_read_upref(dir,first+i,&aid))return-1;struct CprArc a;if(!route_read_arc(aid,&a))return-1;u32 v=dir?a.from:a.to;struct ChState*t=ch_state(v,1);if(!t)return-2;u32 nd=sat_add32(du,a.cost);u32*dist=dir?&t->db:&t->df;if(nd<*dist){*dist=nd;if(dir)t->pb=aid;else t->pf=aid;if(!ch_heap_insert(dir,t))return-2;ch_update_meet(t);}}return 1;
}
static int route_append_coord_cost_m(s32 lat,s32 lon,u32 node,u32 cost,u32 segm){if(route_n&&route_lat[route_n-1]==lat&&route_lon[route_n-1]==lon){route_seg_cost[route_n-1]=sat_add32(route_seg_cost[route_n-1],cost);if(route_emit_arc!=ROUTE_NOARC){route_seg_arc[route_n-1]=route_emit_arc;route_seg_phys[route_n-1]=route_emit_phys;}if(node!=ROUTE_NOARC)route_node[route_n-1]=node;return 1;}if(route_n>=ROUTE_PATH_MAX)return 0;route_lat[route_n]=lat;route_lon[route_n]=lon;route_node[route_n]=node;route_seg_cost[route_n]=route_n?cost:0;route_seg_m[route_n]=route_n?segm:0;route_seg_arc[route_n]=route_n?route_emit_arc:ROUTE_NOARC;route_seg_phys[route_n]=route_n?route_emit_phys:ROUTE_NOARC;if(route_n)route_result_m=sat_add32(route_result_m,segm);route_n++;return 1;}
static int route_append_coord_cost(s32 lat,s32 lon,u32 node,u32 cost){u32 m=route_n?geo_seg_m(route_lat[route_n-1],route_lon[route_n-1],lat,lon):0;RPROF_INC(route_prof_div_fallback);return route_append_coord_cost_m(lat,lon,node,cost,m);}
static int route_append_node_cost(u32 node,u32 cost){struct CprNode n;if(!route_read_node(node,&n))return 0;return route_append_coord_cost(n.lat,n.lon,node,cost);}
static int ch_finish_begin(void){if(route_best==ROUTE_INF||route_meet==ROUTE_INF)return 0;if(route_prof_on&&!route_prof_build_ms){route_prof_build_ms=s_now_ms();route_prof_build_io=route_prof_io_bytes();}route_build_phase=RB_TRACE_F;route_build_nf=route_build_nb=route_build_arc_idx=route_build_sp=0;route_build_copy=-1;route_build_after_base=RB_UNPACK;route_base_meta_n=route_base_meta_idx=route_prefetch_n=route_prefetch_idx=0;route_build_u=route_meet;route_n=0;route_head=0;route_result_m=0;route_build_steps=0;return 1;}
static int route_build_start_init(const struct RouteSnapCandidate*c){if(c->arc==ROUTE_NOARC){route_n=route_head=0;route_result_m=0;route_emit_arc=route_emit_phys=ROUTE_NOARC;return route_append_node_cost(c->node,0);}if(!route_read_base(c->arc,&route_build_base)||route_build_base.geom_count<2||c->geom_seg+1u>=route_build_base.geom_count)return 0;route_n=0;route_head=0;route_result_m=0;route_emit_arc=route_emit_phys=ROUTE_NOARC;if(!route_append_coord_cost(route_query_lat,route_query_lon,ROUTE_NOARC,0))return 0;if(!route_append_coord_cost(c->snap_lat,c->snap_lon,ROUTE_NOARC,0))return 0;route_emit_arc=c->arc;route_emit_phys=route_build_base.geom_first;route_build_i=c->geom_seg+1u;route_build_total=c->total_u>c->along_u?c->total_u-c->along_u:0;route_build_cum=route_build_prev_cost=0;route_build_prev_point.lat=c->snap_lat;route_build_prev_point.lon=c->snap_lon;return 1;}
static int route_build_start_point(const struct RouteSnapCandidate*c){if(c->arc==ROUTE_NOARC)return 2;if(route_build_i>=route_build_base.geom_count)return 2;struct CprPoint p;if(!route_base_point(&route_build_base,route_build_i,&p))return 0;route_build_cum=sat_add32(route_build_cum,route_geom_seg_units(&route_build_prev_point,&p));u32 cc=(route_build_i+1u==route_build_base.geom_count)?c->extra:route_cost_for_units(c->extra,route_build_cum,route_build_total),cost=cc>=route_build_prev_cost?cc-route_build_prev_cost:0;u32 node=(route_build_i+1u==route_build_base.geom_count)?c->node:ROUTE_NOARC;if(!route_append_coord_cost(p.lat,p.lon,node,cost))return 0;route_build_prev_cost=cc;route_build_prev_point=p;route_build_i++;return route_build_i>=route_build_base.geom_count?2:1;}
static int route_build_base_begin(u32 aid,const struct CprArc*a){(void)aid;if(!a||a->flags&CPR_ARC_SHORTCUT||a->left>=rh.base_edges||!route_read_base(a->left,&route_build_base)||route_build_base.geom_count<2)return 0;route_emit_arc=a->left;route_emit_phys=route_build_base.geom_first;route_build_arc=*a;route_build_i=1;route_build_total=route_build_cum=route_build_prev_cost=0;route_build_geom_cached=route_build_base.geom_count<=ROUTE_GEOM_TMP_MAX;if(!route_base_point(&route_build_base,0,&route_build_prev_point))return 0;if(route_build_geom_cached){route_geom_tmp[0]=route_build_prev_point;route_geom_cum_tmp[0]=0;}route_build_phase=RB_BASE_TOTAL;return 1;}
static struct QRec7 route_build_q7;
static int route_build_base_begin_qrec7(const struct QRec7*q){if(!q||qrec7_flags(q)&CPR_ARC_SHORTCUT||q->left>=rh.base_edges)return 0;u16 gc=qrec7_geom_count(q);if(gc<2||q->packet_off==0xffffffffu)return 0;route_build_q7=*q;memzero(&route_build_base,sizeof(route_build_base));route_build_base.geom_first=q->geom_first;route_build_base.geom_count=gc;route_build_base.flags=qrec7_base_flags(q);route_build_base.cls=qrec7_base_cls(q);route_build_base.to_state=qrec7_to(q);route_emit_arc=q->left;route_emit_phys=q->geom_first;memzero(&route_build_arc,sizeof(route_build_arc));route_build_arc.cost=q->cost;route_build_arc.to=qrec7_to(q);route_build_arc.left=q->left;route_build_i=1;route_build_total=q->right_geom;route_build_cum=route_build_prev_cost=0;route_build_geom_cached=0;u32 raw=(route_build_base.flags&CPR_ARC_GEOM_REVERSED)?gc-1u:0u,qf=0,qr=0;u16 mf=0,mr=0;if(!qadj7_packet_point(q,raw,&route_build_prev_point,&qf,&qr,&mf,&mr))return 0;route_build_phase=RB_BASE_OUTPUT;return 1;}
static int route_build_base_begin_qrec4(const struct QRec4*q){if(!q||qrec4_flags(q)&CPR_ARC_SHORTCUT||q->left>=rh.base_edges)return 0;u16 gc=qrec4_geom_count(q);if(gc<2)return 0;memzero(&route_build_base,sizeof(route_build_base));route_build_base.geom_first=q->geom_first;route_build_base.geom_count=gc;route_build_base.flags=qrec4_base_flags(q);route_build_base.cls=qrec4_base_cls(q);route_build_base.to_state=qrec4_to(q);route_emit_arc=q->left;route_emit_phys=q->geom_first;memzero(&route_build_arc,sizeof(route_build_arc));route_build_arc.cost=q->cost;route_build_arc.to=qrec4_to(q);route_build_arc.left=q->left;route_build_i=1;route_build_total=q->right_geom;route_build_cum=route_build_prev_cost=0;route_build_geom_cached=0;if(!route_build_total||!route_base_point(&route_build_base,0,&route_build_prev_point))return 0;route_build_phase=RB_BASE_OUTPUT;return 1;}
static int route_build_base_begin_qrec(const struct QRec*q){if(!q||qrec_flags(q)&CPR_ARC_SHORTCUT||q->left>=rh.base_edges||!route_read_base(q->left,&route_build_base)||route_build_base.geom_count<2)return 0;route_emit_arc=q->left;route_emit_phys=route_build_base.geom_first;memzero(&route_build_arc,sizeof(route_build_arc));route_build_arc.cost=q->cost;route_build_arc.to=qrec_to(q);route_build_arc.left=q->left;route_build_i=1;route_build_total=q->right_geom;route_build_cum=route_build_prev_cost=0;route_build_geom_cached=0;if(!route_base_point(&route_build_base,0,&route_build_prev_point))return 0;/* QAD3 gives the exact builder-computed geometry total, so normal base arcs skip RB_BASE_TOTAL entirely. A zero total falls back to the legacy pass for degenerate geometry. */if(route_build_total){route_build_phase=RB_BASE_OUTPUT;return 1;}return route_build_base_begin(0,&route_build_arc);}
static int route_build_base_total_step(void){if(route_build_i>=route_build_base.geom_count){route_build_i=1;route_build_cum=route_build_prev_cost=0;if(route_build_geom_cached){route_build_prev_point=route_geom_tmp[0];RPROF_INC(route_prof_geom_reuse_arcs);RPROF_ADD(route_prof_geom_reuse_pts,route_build_base.geom_count);}else if(!route_base_point(&route_build_base,0,&route_build_prev_point))return 0;route_build_phase=RB_BASE_OUTPUT;return 1;}struct CprPoint p;if(!route_base_point(&route_build_base,route_build_i,&p))return 0;route_build_total=sat_add32(route_build_total,route_geom_seg_units(&route_build_prev_point,&p));if(route_build_geom_cached){route_geom_tmp[route_build_i]=p;route_geom_cum_tmp[route_build_i]=route_build_total;}route_build_prev_point=p;route_build_i++;return 1;}
static void route_prefetch_prepare(void){route_prefetch_n=route_prefetch_idx=0;if(qadjmode==7||qadjmode<5||route_base_meta_n<1)return;u32 file_end=rh.geom_off+rh.geom_points*(u32)sizeof(struct CprPoint);for(int i=0;i<route_base_meta_n;i++){struct QRec4*q=(struct QRec4*)&route_base_meta[i];u32 gc=qrec4_geom_count(q);if(gc<2)continue;u32 a=rh.geom_off+q->geom_first*(u32)sizeof(struct CprPoint),b=a+gc*(u32)sizeof(struct CprPoint)-1u;if(b<a||b>=file_end)continue;u32 pa=a>>12,pb=b>>12;for(u32 pg=pa;pg<=pb;pg++){if(route_prefetch_n>=ROUTE_PATH_MAX){route_prefetch_n=route_prefetch_idx=0;return;}route_prefetch_page[route_prefetch_n++]=pg;}}for(int gap=route_prefetch_n>>1;gap>0;gap>>=1)for(int i=gap;i<route_prefetch_n;i++){u32 v=route_prefetch_page[i];int j=i;while(j>=gap&&route_prefetch_page[j-gap]>v){route_prefetch_page[j]=route_prefetch_page[j-gap];j-=gap;}route_prefetch_page[j]=v;}int w=0;for(int i=0;i<route_prefetch_n;i++)if(!w||route_prefetch_page[i]!=route_prefetch_page[w-1])route_prefetch_page[w++]=route_prefetch_page[i];route_prefetch_n=w;RPROF_ADD(route_prof_prefetch_pages,route_prefetch_n);}
static int route_prefetch_step(void){if(route_prefetch_idx>=route_prefetch_n)return 2;if(route_geom_map_ok){int n=0;while(route_prefetch_idx<route_prefetch_n&&n<8){u32 pg=route_prefetch_page[route_prefetch_idx++],abs=pg<<12;if(abs>=route_geom_map_fileoff){u32 off=abs-route_geom_map_fileoff;if(off<route_geom_map_bytes){volatile u8 x=route_geom_map[off];(void)x;}}n++;}return route_prefetch_idx>=route_prefetch_n?2:1;}u32 pg=route_prefetch_page[route_prefetch_idx],run=1;while(route_prefetch_idx+(int)run<route_prefetch_n&&run<8u&&route_prefetch_page[route_prefetch_idx+run]==pg+run)run++;u32 abs=pg<<12,file_end=rh.geom_off+rh.geom_points*(u32)sizeof(struct CprPoint);if(abs>=file_end){route_prefetch_idx+=(int)run;return route_prefetch_idx>=route_prefetch_n?2:1;}u32 bytes=run*4096u;if(bytes>file_end-abs)bytes=file_end-abs;s_lseek(routefd,(long)abs,SEEK_SET_);long nr=s_read(routefd,route_prefetch_scratch,bytes);if(nr>0){RPROF_INC(route_prof_prefetch_reads);RPROF_ADD(route_prof_prefetch_bytes,nr);}route_prefetch_idx+=(int)run;return route_prefetch_idx>=route_prefetch_n?2:1;}
static int route_build_finish_bases(void){if(route_repair_active){if(!route_repair_validate_anchor())return 0;route_repair_suffix_copy=1;route_build_phase=RB_REPAIR_SUFFIX;return 1;}route_build_phase=RB_DEST_INIT;return 1;}
static int route_build_base_output_step(void){if(route_build_i>=route_build_base.geom_count){route_build_phase=route_build_after_base;return 1;}struct CprPoint p;u32 cc=0,segm=0;int fast=0;if(qadjmode==7){u32 raw=(route_build_base.flags&CPR_ARC_GEOM_REVERSED)?(route_build_base.geom_count-1u-route_build_i):route_build_i,qf=0,qr=0;u16 mf=0,mr=0;if(qadj7_packet_point(&route_build_q7,raw,&p,&qf,&qr,&mf,&mr)){u32 q=(route_build_base.flags&CPR_ARC_GEOM_REVERSED)?qr:qf;segm=(route_build_base.flags&CPR_ARC_GEOM_REVERSED)?mr:mf;cc=(route_build_i+1u==route_build_base.geom_count)?route_build_arc.cost:route_mul_q16(route_build_arc.cost,q);fast=1;}}else{if(!route_base_point(&route_build_base,route_build_i,&p))return 0;if(qadjmode==6&&qadj_map_ok){u32 raw=(route_build_base.flags&CPR_ARC_GEOM_REVERSED)?(route_build_base.geom_count-1u-route_build_i):route_build_i;u32 qf=0,qr=0;u16 mf=0,mr=0;if(qadj6_gmeta(raw,&qf,&qr,&mf,&mr)){u32 q=(route_build_base.flags&CPR_ARC_GEOM_REVERSED)?qr:qf;fast=1;if(route_build_base.flags&CPR_ARC_GEOM_REVERSED){u32 sr=raw+1u,tf=0,tr=0;u16 tmf=0,tmr=0;if(sr<qh6.geom_points&&qadj6_gmeta(sr,&tf,&tr,&tmf,&tmr))segm=tmr;else fast=0;}else segm=mf;if(fast){if(route_build_i+1u==route_build_base.geom_count)cc=route_build_arc.cost;else cc=route_mul_q16(route_build_arc.cost,q);}}}}
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
  if(route_build_nf>=ROUTE_PATH_MAX||!s||s->pf==ROUTE_NOARC)return 0;u32 fr=0;if(qadjmode==7){struct QRec7 q;if(!qadj7_read_qrec(s->pf,&q))return 0;fr=q.from;}else if(qadjmode>=4){struct QRec4 q;if(!qadj4_read_qrec(s->pf,&q))return 0;fr=q.from;}else if(qadjmode==3){struct QRec q;if(!qadj3_read_qrec(s->pf,&q))return 0;fr=q.from;}else{struct CprArc a;if(!route_read_arc(s->pf,&a))return 0;fr=a.from;}ch_arc_tmp[route_build_nf++]=s->pf;route_build_u=fr;return 1;}
case RB_COPY_F:if(route_build_copy<0){route_build_u=route_meet;route_build_phase=RB_TRACE_B;return 1;}if(route_build_nb>=ROUTE_PATH_MAX)return 0;ch_arc_path[route_build_nb++]=ch_arc_tmp[route_build_copy--];return 1;
case RB_TRACE_B:{struct ChState*s=ch_state(route_build_u,0);if(!s||s->db==ROUTE_INF)return 0;if(s->pb==ROUTE_NOARC){route_build_phase=RB_START_INIT;return 1;}if(route_build_nb>=ROUTE_PATH_MAX)return 0;u32 to=0;if(qadjmode==7){struct QRec7 q;if(!qadj7_read_qrec(s->pb,&q))return 0;to=qrec7_to(&q);}else if(qadjmode>=4){struct QRec4 q;if(!qadj4_read_qrec(s->pb,&q))return 0;to=qrec4_to(&q);}else if(qadjmode==3){struct QRec q;if(!qadj3_read_qrec(s->pb,&q))return 0;to=qrec_to(&q);}else{struct CprArc a;if(!route_read_arc(s->pb,&a))return 0;to=a.to;}ch_arc_path[route_build_nb++]=s->pb;route_build_u=to;}return 1;
case RB_START_INIT:{const struct RouteSnapCandidate*rs=route_repair_active?(route_repair_chosen_start>=0&&route_repair_chosen_start<route_repair_start_n?&route_repair_start[route_repair_chosen_start]:0):start;if(!rs||!route_build_start_init(rs))return 0;route_build_phase=RB_START_POINTS;return 1;}
case RB_START_POINTS:{const struct RouteSnapCandidate*rs=route_repair_active?&route_repair_start[route_repair_chosen_start]:start;int r=route_build_start_point(rs);if(!r)return 0;if(r==2)route_build_phase=RB_UNPACK;return 1;}
case RB_UNPACK:
  if(route_build_sp){u32 x=ch_unpack_stack[--route_build_sp];if(qadjmode==7){struct QRec7 q;if(!qadj7_read_qrec(x,&q))return 0;if(qrec7_flags(&q)&CPR_ARC_SHORTCUT){if(route_build_sp+2>ROUTE_PATH_MAX)return 0;ch_unpack_stack[route_build_sp++]=q.right_geom;ch_unpack_stack[route_build_sp++]=q.left;return 1;}if(route_base_meta_n>=ROUTE_PATH_MAX)return 0;route_base_meta[route_base_meta_n++]=q;return 1;}else if(qadjmode>=4){struct QRec4 q;if(!qadj4_read_qrec(x,&q))return 0;if(qrec4_flags(&q)&CPR_ARC_SHORTCUT){if(route_build_sp+2>ROUTE_PATH_MAX)return 0;ch_unpack_stack[route_build_sp++]=q.right_geom;ch_unpack_stack[route_build_sp++]=q.left;return 1;}if(qadjmode>=5){if(route_base_meta_n>=ROUTE_PATH_MAX)return 0;qrec7_from4(&route_base_meta[route_base_meta_n++],&q);return 1;}route_build_after_base=RB_UNPACK;return route_build_base_begin_qrec4(&q)?1:0;}else if(qadjmode==3){struct QRec q;if(!qadj3_read_qrec(x,&q))return 0;if(qrec_flags(&q)&CPR_ARC_SHORTCUT){if(route_build_sp+2>ROUTE_PATH_MAX)return 0;ch_unpack_stack[route_build_sp++]=q.right_geom;ch_unpack_stack[route_build_sp++]=q.left;return 1;}route_build_after_base=RB_UNPACK;return route_build_base_begin_qrec(&q)?1:0;}else{struct CprArc a;if(!route_read_arc(x,&a))return 0;if(a.flags&CPR_ARC_SHORTCUT){if(route_build_sp+2>ROUTE_PATH_MAX)return 0;ch_unpack_stack[route_build_sp++]=a.right;ch_unpack_stack[route_build_sp++]=a.left;return 1;}route_build_after_base=RB_UNPACK;return route_build_base_begin(x,&a)?1:0;}}
  if(route_build_arc_idx<route_build_nb){ch_unpack_stack[route_build_sp++]=ch_arc_path[route_build_arc_idx++];return 1;}
  if(qadjmode>=5){route_base_meta_idx=0;if(qadjmode==7){route_build_phase=RB_BASE_NEXT;return 1;}route_prefetch_prepare();route_build_phase=route_prefetch_n?RB_PREFETCH:RB_BASE_NEXT;return 1;}
  return route_build_finish_bases();
case RB_PREFETCH:{int r=route_prefetch_step();if(!r)return 0;if(r==2)route_build_phase=RB_BASE_NEXT;return 1;}
case RB_BASE_NEXT:if(route_base_meta_idx<route_base_meta_n){route_build_after_base=RB_BASE_NEXT;if(qadjmode==7)return route_build_base_begin_qrec7(&route_base_meta[route_base_meta_idx++])?1:0;return route_build_base_begin_qrec4((struct QRec4*)&route_base_meta[route_base_meta_idx++])?1:0;}return route_build_finish_bases();
case RB_BASE_TOTAL:return route_build_base_total_step()?1:0;
case RB_BASE_OUTPUT:{int n=qadjmode==7?16:(qadjmode==6?8:1);while(n--){if(!route_build_base_output_step())return 0;if(route_build_phase!=RB_BASE_OUTPUT)break;}return 1;}
case RB_DEST_INIT:if(!route_build_dest_init(dest))return 0;route_build_phase=RB_DEST_POINTS;return 1;
case RB_DEST_POINTS:{int r=route_build_dest_point(dest);if(!r)return 0;if(r==2){route_result_cost=route_best;route_build_phase=RB_DONE;return route_n>0?2:0;}return 1;}
case RB_REPAIR_SUFFIX:return route_repair_suffix_step();
case RB_DONE:return route_n>0?2:0;default:return 0;}}

static u32 route_used_bytes(void){return chbucket_n*4u+chst_n*(u32)sizeof(struct ChState)+(chhf_n+chhb_n+2u)*4u;}
static void route_debug_summary(const char*tag){if(!pref.debug)return;char b[256];b[0]=0;appendstr(b,sizeof(b),tag);appendstr(b,sizeof(b)," ALG=CH4 STATES=");append_int(b,sizeof(b),(int)chst_n);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)chst_cap);appendstr(b,sizeof(b)," EXP=");append_int(b,sizeof(b),(int)route_expanded);appendstr(b,sizeof(b)," HEAP=");append_int(b,sizeof(b),(int)chheap_peak);appendstr(b,sizeof(b)," USEDKB=");append_int(b,sizeof(b),(int)(route_used_bytes()>>10));appendstr(b,sizeof(b)," ARENAKB=");append_int(b,sizeof(b),(int)(route_arena_bytes>>10));appendstr(b,sizeof(b)," AVAILKB=");append_int(b,sizeof(b),(int)route_mem_avail_kb);appendstr(b,sizeof(b)," SLICES=");append_int(b,sizeof(b),(int)route_sched_slices);appendstr(b,sizeof(b)," YIELD=");append_int(b,sizeof(b),(int)route_sched_yields);appendstr(b,sizeof(b)," BUILD=");append_int(b,sizeof(b),(int)route_build_steps);debug_log(STATE_LOG,b);if(!route_prof_on)return;u32 now=s_now_ms(),wall=now-route_prof_start_ms,pre=route_prof_query_ms?route_prof_query_ms-route_prof_start_ms:wall,query=route_prof_query_ms?(route_prof_build_ms?route_prof_build_ms-route_prof_query_ms:now-route_prof_query_ms):0,build=route_prof_build_ms?now-route_prof_build_ms:0,cpu=route_prof_cpu_ms;if(route_prof_slice_ms)cpu+=now-route_prof_slice_ms;u32 io=route_prof_io_bytes(),preio=route_prof_pre_io,qio=route_prof_build_io>=preio?route_prof_build_io-preio:0,bio=io>=route_prof_build_io?io-route_prof_build_io:0;b[0]=0;appendstr(b,sizeof(b),"ROUTE PROF WALLMS=");append_int(b,sizeof(b),(int)wall);appendstr(b,sizeof(b)," PREMS=");append_int(b,sizeof(b),(int)pre);appendstr(b,sizeof(b)," QUERYMS=");append_int(b,sizeof(b),(int)query);appendstr(b,sizeof(b)," BUILDMS=");append_int(b,sizeof(b),(int)build);appendstr(b,sizeof(b)," CPUMS=");append_int(b,sizeof(b),(int)cpu);appendstr(b,sizeof(b)," RELAX=");append_int(b,sizeof(b),(int)route_prof_relax);appendstr(b,sizeof(b)," PATH=");append_int(b,sizeof(b),route_n);appendstr(b,sizeof(b)," PREKB=");append_int(b,sizeof(b),(int)(preio>>10));appendstr(b,sizeof(b)," QKB=");append_int(b,sizeof(b),(int)(qio>>10));appendstr(b,sizeof(b)," BKB=");append_int(b,sizeof(b),(int)(bio>>10));debug_log(STATE_LOG,b);b[0]=0;appendstr(b,sizeof(b),"CPR PROF N=");append_int(b,sizeof(b),(int)route_prof_n_hit);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)route_prof_n_miss);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)(route_prof_n_bytes>>10));appendstr(b,sizeof(b)," B=");append_int(b,sizeof(b),(int)route_prof_b_hit);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)route_prof_b_miss);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)(route_prof_b_bytes>>10));appendstr(b,sizeof(b)," A=");append_int(b,sizeof(b),(int)route_prof_a_hit);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)route_prof_a_miss);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)(route_prof_a_bytes>>10));appendstr(b,sizeof(b)," G=");append_int(b,sizeof(b),(int)route_prof_g_hit);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)route_prof_g_miss);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)(route_prof_g_bytes>>10));appendstr(b,sizeof(b)," T=");append_int(b,sizeof(b),(int)route_prof_tile_reads);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)(route_prof_tile_bytes>>10));debug_log(STATE_LOG,b);b[0]=0;appendstr(b,sizeof(b),"CPR REF F=");for(int i=0;i<4;i++){if(i)appendstr(b,sizeof(b),i==1?" R=":(i==2?" T=":" S="));append_int(b,sizeof(b),(int)route_prof_r_hit[i]);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)route_prof_r_miss[i]);appendstr(b,sizeof(b),"/");append_int(b,sizeof(b),(int)(route_prof_r_bytes[i]>>10));}debug_log(STATE_LOG,b);b[0]=0;appendstr(b,sizeof(b),"QADJ PROF MODE=");append_int(b,sizeof(b),qadjmode);appendstr(b,sizeof(b)," READS=");append_int(b,sizeof(b),(int)route_prof_qadj_reads);appendstr(b,sizeof(b)," KB=");append_int(b,sizeof(b),(int)(route_prof_qadj_bytes>>10));appendstr(b,sizeof(b)," IDXKB=");append_int(b,sizeof(b),(int)(qadj_index_bytes>>10));appendstr(b,sizeof(b)," VIDXKB=");append_int(b,sizeof(b),(int)((qadjmode>=5?qh5.index_bytes:0u)>>10));appendstr(b,sizeof(b)," MMAP=");append_int(b,sizeof(b),qadj_map_ok);appendstr(b,sizeof(b)," STALL=");append_int(b,sizeof(b),(int)route_prof_stall);appendstr(b,sizeof(b)," QMAP=");append_int(b,sizeof(b),(int)route_prof_qmap_access);appendstr(b,sizeof(b)," IDXIOKB=");append_int(b,sizeof(b),(int)(route_prof_qidx_bytes>>10));debug_log(STATE_LOG,b);b[0]=0;appendstr(b,sizeof(b),"QREC PROF H=");append_int(b,sizeof(b),(int)route_prof_qrec_hit);appendstr(b,sizeof(b)," M=");append_int(b,sizeof(b),(int)route_prof_qrec_miss);appendstr(b,sizeof(b)," KB=");append_int(b,sizeof(b),(int)(route_prof_qrec_bytes>>10));debug_log(STATE_LOG,b);b[0]=0;appendstr(b,sizeof(b),"BUILD PROF TF=");append_int(b,sizeof(b),(int)route_prof_build_phase_steps[RB_TRACE_F]);appendstr(b,sizeof(b)," TB=");append_int(b,sizeof(b),(int)route_prof_build_phase_steps[RB_TRACE_B]);appendstr(b,sizeof(b)," UNP=");append_int(b,sizeof(b),(int)route_prof_build_phase_steps[RB_UNPACK]);appendstr(b,sizeof(b)," PF=");append_int(b,sizeof(b),(int)route_prof_build_phase_steps[RB_PREFETCH]);appendstr(b,sizeof(b)," BASE=");append_int(b,sizeof(b),route_base_meta_n);appendstr(b,sizeof(b)," GT=");append_int(b,sizeof(b),(int)route_prof_build_phase_steps[RB_BASE_TOTAL]);appendstr(b,sizeof(b)," GO=");append_int(b,sizeof(b),(int)route_prof_build_phase_steps[RB_BASE_OUTPUT]);appendstr(b,sizeof(b)," SP=");append_int(b,sizeof(b),(int)route_prof_build_phase_steps[RB_START_POINTS]);appendstr(b,sizeof(b)," DP=");append_int(b,sizeof(b),(int)route_prof_build_phase_steps[RB_DEST_POINTS]);debug_log(STATE_LOG,b);b[0]=0;appendstr(b,sizeof(b),"BUILD IO MMAPG=");append_int(b,sizeof(b),route_geom_map_ok);appendstr(b,sizeof(b)," GMAP=");append_int(b,sizeof(b),(int)route_prof_gmap_access);appendstr(b,sizeof(b)," PFPAGES=");append_int(b,sizeof(b),(int)route_prof_prefetch_pages);appendstr(b,sizeof(b)," PFREADS=");append_int(b,sizeof(b),(int)route_prof_prefetch_reads);appendstr(b,sizeof(b)," PFKB=");append_int(b,sizeof(b),(int)(route_prof_prefetch_bytes>>10));debug_log(STATE_LOG,b);b[0]=0;appendstr(b,sizeof(b),"BUILD ARITH GMETA=");append_int(b,sizeof(b),(int)route_prof_gmeta);appendstr(b,sizeof(b)," FASTM=");append_int(b,sizeof(b),(int)route_prof_mfast);appendstr(b,sizeof(b)," FALLBACK=");append_int(b,sizeof(b),(int)route_prof_div_fallback);appendstr(b,sizeof(b)," ROUTEM=");append_int(b,sizeof(b),(int)route_result_m);appendstr(b,sizeof(b)," PKTPTS=");append_int(b,sizeof(b),(int)route_prof_packet_pts);debug_log(STATE_LOG,b);}
static u32 geo_seg_m(s32 la0,s32 lo0,s32 la1,s32 lo1);
static u32 route_point_d2(s32 lat,s32 lon,s32 qlat,s32 qlon){int cq=coslat_q15(lat);s32 dx=lon_scale(qlon-lon,cq),dy=qlat-lat;return square_distance(dx,dy);}
static int route_snap_car_component(u32 comp,struct RouteSnapCandidate*out){struct RouteSnapCandidate c[8];int n=route_snap_candidates(car_lat(),car_lon(),ROUTE_SNAP_D2,1,0,c,8);for(int i=0;i<n;i++){struct CprNode rn;if(route_read_node(c[i].node,&rn)&&rn.comp==comp){if(out)*out=c[i];return 1;}}return 0;}
static u32 route_compute_m(void){u32 m=0;int first=route_head;if(first<0)first=0;for(int i=first+1;i<route_n;i++){u32 d=geo_seg_m(route_lat[i-1],route_lon[i-1],route_lat[i],route_lon[i]);if(0xffffffffu-m<d)return 0xffffffffu;m+=d;}return m;}
static int route_path_index_for_car(u32 snapped){if(route_n-route_head<2)return-1;int cq=coslat_q15(car_lat());int end=route_n-1;if(end>route_head+ROUTE_PROGRESS_LOOKAHEAD)end=route_head+ROUTE_PROGRESS_LOOKAHEAD;for(int i=route_head;i<=end;i++)if(route_node[i]==snapped){s32 dx=lon_scale(route_lon[i]-car_lon(),cq),dy=route_lat[i]-car_lat();u32 d=square_distance(dx,dy);if(d<ROUTE_PROGRESS_D2)return i;}u32 best=ROUTE_PROGRESS_D2;int bi=-1;for(int i=route_head;i<end;i++){s32 ax=lon_scale(route_lon[i]-car_lon(),cq),ay=route_lat[i]-car_lat();s32 bx=lon_scale(route_lon[i+1]-car_lon(),cq),by=route_lat[i+1]-car_lat();u32 d=seg_dist2(ax,ay,bx,by);if(d<best){best=d;bi=i;}}return bi;}
static void route_trim_prefix(int idx){if(idx>route_head&&idx<route_n){u32 spent=0,spentm=0;for(int i=route_head+1;i<=idx;i++){spent=sat_add32(spent,route_seg_cost[i]);spentm=sat_add32(spentm,route_seg_m[i]);}route_result_cost=route_result_cost>spent?route_result_cost-spent:0;route_result_m=route_result_m>spentm?route_result_m-spentm:0;route_head=idx;if(route_node[route_head]!=ROUTE_NOARC)route_car_node=route_node[route_head];if(nav_man_idx>=0){if(route_head>=nav_man_idx){nav_maneuver_reset();if(route_status==R_READY)nav_maneuver_find();}else nav_man_m=nav_man_m>spentm?nav_man_m-spentm:0;}else if(route_status==R_READY&&route_head-nav_man_scan_head>=ROUTE_MAN_RESCAN_POINTS)nav_maneuver_find();}}
/* dev5.12 stores the physical-polyline key (geom_first) beside each route
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
static int regional_route_start(s32 dest_lat,s32 dest_lon){route_repair_reset();route_prof_reset();route_arena_release();route_dest_count=route_dest_idx=0;route_dest_comp=0;route_offroute_checks=0;route_arc_check_tick=0;route_rerouting=0;route_target_lat=dest_lat;route_target_lon=dest_lon;if(!routeok){route_status=R_NOFILE;return 0;}if(!(route_exact_from!=ROUTE_NOARC?route_exact_snap(route_exact_from,&route_start_snap):route_query_start_snap(&route_start_snap))){route_status=R_NOROAD;return 0;}route_car_node=route_start_snap.node;/* PERF5: at Z6 a CPR tile spans 256 coordinate units, larger than the 150-unit destination snap radius. A 3x3 neighbourhood therefore covers every geometrically legal candidate; the old 5x5 sweep only added I/O. */route_dest_count=route_exact_to!=ROUTE_NOARC?route_exact_snap(route_exact_to,&route_dest_cand[0]):route_query_dest_snaps(dest_lat,dest_lon);if(route_dest_count<1){route_status=R_NOROAD;return 0;}if(route_exact_to==ROUTE_NOARC&&route_same_endpoint(&route_start_snap,&route_dest_cand[0])){struct RouteSnapCandidate stop;memzero(&stop,sizeof(stop));stop.arc=ROUTE_NOARC;stop.node=route_start_snap.node;stop.snap_lat=route_start_snap.snap_lat;stop.snap_lon=route_start_snap.snap_lon;route_dest_cand[0]=stop;route_dest_count=1;}if(!route_arena_prepare()){route_status=R_MEMORY;return 0;}if(!route_begin_candidate(0)&&!route_retry_destination()){route_status=R_UNREACHABLE;return 0;}return 1;}
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
static const char* nav_man_text(void){static char exit_text[64];if(nav_man_type==MAN_ROUNDABOUT){if(!nav_man_exit)return ui_tr("ROUNDABOUT");copystr(exit_text,sizeof(exit_text),ui_tr("TAKE EXIT "));append_int(exit_text,sizeof(exit_text),nav_man_exit);return exit_text;}switch(nav_man_type){case MAN_SLIGHT_LEFT:return ui_tr("SLIGHT LEFT");case MAN_LEFT:return ui_tr("TURN LEFT");case MAN_SHARP_LEFT:return ui_tr("SHARP LEFT");case MAN_SLIGHT_RIGHT:return ui_tr("SLIGHT RIGHT");case MAN_RIGHT:return ui_tr("TURN RIGHT");case MAN_SHARP_RIGHT:return ui_tr("SHARP RIGHT");case MAN_UTURN:return ui_tr("U-TURN");default:return"";}}
static void append_nav_distance(char*b,unsigned cap,u32 m){if(!display_imperial()){append_metric_distance(b,cap,m,1);return;}if(m<70u){appendstr(b,cap,ui_tr("NOW"));return;}if(m<1000u){u32 yd=udiv32(m*1094u+500u,1000u);yd=udiv32(yd+5u,10u)*10u;append_int(b,cap,(int)yd);appendstr(b,cap," YD");return;}u32 mi10=udiv32(m*10u+804u,1609u);append_int(b,cap,(int)(mi10/10u));appendstr(b,cap,".");char d[2]={(char)('0'+mi10%10u),0};appendstr(b,cap,d);appendstr(b,cap," MI");}
static void nav_arrow_head(int x,int y,int dx,int dy,u16 c){/* Tiny HUD arrow head. */int px=-dy,py=dx;line1(x,y,x-dx*4+px*3,y-dy*4+py*3,c,1);line1(x,y,x-dx*4-px*3,y-dy*4-py*3,c,1);}
static void draw_manoeuvre_icon(int cx,int cy,int t){u16 c=C_CYAN;if(t==MAN_ROUNDABOUT){for(int k=0;k<36;k++){int n=(k+1)%36;line1(cx+(SIN36[k]*8>>15),cy-(COS36[k]*8>>15),cx+(SIN36[n]*8>>15),cy-(COS36[n]*8>>15),c,1);}line1(cx,cy+12,cx,cy+7,c,1);int clockwise=(region_flags&4)!=0;nav_arrow_head(cx+7,cy-4,clockwise?1:-1,clockwise?1:-1,c);if(nav_man_exit){char n[8];n[0]=0;append_int(n,sizeof(n),nav_man_exit);compact_center(cx,cy-5,13,0,n,c);}return;}if(t==MAN_UTURN){int side=(region_flags&4)?-1:1;line1(cx+side*4,cy+7,cx+side*4,cy-4,c,2);line1(cx+side*4,cy-4,cx+side,cy-7,c,2);line1(cx+side,cy-7,cx-side*5,cy-7,c,2);line1(cx-side*5,cy-7,cx-side*7,cy-4,c,2);line1(cx-side*7,cy-4,cx-side*7,cy+3,c,2);nav_arrow_head(cx-side*7,cy+3,0,1,c);return;}int left=t==MAN_SLIGHT_LEFT||t==MAN_LEFT||t==MAN_SHARP_LEFT;int slight=t==MAN_SLIGHT_LEFT||t==MAN_SLIGHT_RIGHT;int sharp=t==MAN_SHARP_LEFT||t==MAN_SHARP_RIGHT;line1(cx,cy+8,cx,cy,c,2);if(slight){int ex=cx+(left?-8:8),ey=cy-7;line1(cx,cy,ex,ey,c,2);nav_arrow_head(ex,ey,left?-1:1,-1,c);}else if(sharp){int mx=cx+(left?-4:4),my=cy-1,ex=cx+(left?-8:8),ey=cy+4;line1(cx,cy,mx,my,c,2);line1(mx,my,ex,ey,c,2);nav_arrow_head(ex,ey,left?-1:1,1,c);}else{int ex=cx+(left?-9:9),ey=cy;line1(cx,cy,ex,ey,c,2);nav_arrow_head(ex,ey,left?-1:1,0,c);}}
static void draw_manoeuvre_card(void){if(nav_man_type==MAN_NONE||nav_man_idx<0||nav_man_m>ROUTE_MAN_SHOW_M)return;char d[20];d[0]=0;append_nav_distance(d,sizeof(d),nav_man_m);/* Compact raised navigation card. */int mw=textw(d,1)+38;if(mw<62)mw=62;if(mw>92)mw=92;ui_surface(8,48,mw,24,C_PANEL2);fill(9,48,mw-2,2,C_CYAN);draw_manoeuvre_icon(22,60,nav_man_type);text(39,54,d,C_WHITE,1);}
static u32 route_path_m(void){return sat_add32(route_result_m,atlas_future_m);}
static void append_miles(char*b,unsigned cap,u32 m){if(!display_imperial()){append_metric_distance(b,cap,m,0);return;}u32 mi10=udiv32(m*10u+804u,1609u);append_int(b,cap,(int)(mi10/10u));appendstr(b,cap,".");char d[2]={(char)('0'+mi10%10u),0};appendstr(b,cap,d);appendstr(b,cap," MI");}
static void append_eta(char*b,unsigned cap,u32 ds){u32 min=udiv32(ds+300u,600u);if(min>=60u){append_int(b,cap,(int)(min/60u));appendstr(b,cap,"H ");append_int(b,cap,(int)(min%60u));appendstr(b,cap,"M");}else{append_int(b,cap,(int)min);appendstr(b,cap," MIN");}}
static void append_arrival_clock(char*b,unsigned cap,u32 ds){int mt=local_clock_minutes();if(mt<0){appendstr(b,cap,"--:--");return;}u32 add=udiv32(ds+300u,600u);mt=(mt+(int)(add%1440u))%1440;char t[6];t[0]=(char)('0'+mt/600);t[1]=(char)('0'+(mt/60)%10);t[2]=':';t[3]=(char)('0'+(mt%60)/10);t[4]=(char)('0'+mt%10);t[5]=0;appendstr(b,cap,t);}
static void route_draw_window(int*ps,int*pe){int first=route_head+1,last=route_n-1;if(first>last){*ps=1;*pe=0;return;}if(last-first+1<=ROUTE_DRAW_MAX_SEGMENTS){*ps=first;*pe=last;return;}if(!freecam){*ps=first;*pe=first+ROUTE_DRAW_MAX_SEGMENTS-1;if(*pe>last)*pe=last;return;}s32 vl=view_lat(),vo=view_lon();int cq=coslat_q15(vl),best=first;u32 bd=0xffffffffu;for(int i=first;i<=last;i+=64){s32 dx=lon_scale(route_lon[i]-vo,cq),dy=route_lat[i]-vl;u32 d=square_distance(dx,dy);if(d<bd){bd=d;best=i;}}{s32 dx=lon_scale(route_lon[last]-vo,cq),dy=route_lat[last]-vl;u32 d=square_distance(dx,dy);if(d<bd)best=last;}int half=ROUTE_DRAW_MAX_SEGMENTS/2,s=best-half;if(s<first)s=first;int e=s+ROUTE_DRAW_MAX_SEGMENTS-1;if(e>last){e=last;s=e-ROUTE_DRAW_MAX_SEGMENTS+1;if(s<first)s=first;}*ps=s;*pe=e;}
static void draw_route_overlay(int z){
 if(route_status!=R_READY||route_n-route_head<2)return;int s,e;route_draw_window(&s,&e);if(s>e)return;
 int x[255],y[255],n=1;if(s==route_head+1)map_project_q8(car_lat(),car_lon(),z,&x[0],&y[0]);else map_project_q8(route_lat[s-1],route_lon[s-1],z,&x[0],&y[0]);
 map_mask_begin(2);
 for(int i=s;i<=e;i++){
  map_project_q8(route_lat[i],route_lon[i],z,&x[n],&y[n]);
  int visible=!((x[n-1]<-20*256&&x[n]<-20*256)||(x[n-1]>(W+20)*256&&x[n]>(W+20)*256)||(y[n-1]<35*256&&y[n]<35*256)||(y[n-1]>(MAP_STRIP_Y+3)*256&&y[n]>(MAP_STRIP_Y+3)*256));
  if(visible&&!freecam&&ndrive_route_segs<DRIVE_ROUTE_SEGS&&x[n-1]>-2048*256&&x[n-1]<2048*256&&y[n-1]>-2048*256&&y[n-1]<2048*256&&x[n]>-2048*256&&x[n]<2048*256&&y[n]>-2048*256&&y[n]<2048*256){
   struct DriveRouteSeg*r=&drive_route_segs[ndrive_route_segs++];r->x0=(s16)(x[n-1]>>8);r->y0=(s16)(y[n-1]>>8);r->x1=(s16)(x[n]>>8);r->y1=(s16)(y[n]>>8);
  }
  n++;if(n==255){map_stroke_polyline(x,y,n,C_ROUTE,3);x[0]=x[n-1];y[0]=y[n-1];n=1;}
 }
 if(n>1)map_stroke_polyline(x,y,n,C_ROUTE,3);map_mask_end();
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
  /* REV12's uncluttered far-field criteria are now first-pass preferences,
   * not irreversible rejections.  Remaining road names can fill vacancies. */
  return d->prio<=159 && (d->y>=124||d->prio<=31) &&
         (d->y>=151||d->prio<=79);
}
static void drive_label_offer(int x,int y,int kind,int prio,const char*name){
  if(freecam||!name||!name[0])return;
  if(kind>=6){ /* only useful, nearby POIs; remove distant icon clutter */
    if(prio>47||y<119||y>208||x<18||x>W-18)return;
  }else{
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
  int road_count=0,poi_count=0,order[DRIVE_LABEL_CANDIDATES];
  for(int i=0;i<ndrive_labels;i++)order[i]=i;
  /* A small, bounded insertion sort; no allocations or tile re-reads. */
  for(int i=1;i<ndrive_labels;i++){
    int idx=order[i],j=i;
    while(j>0&&drive_labels[idx].score<drive_labels[order[j-1]].score){
      order[j]=order[j-1];j--;
    }
    order[j]=idx;
  }
  nlbox=0;nltxt=0;
  for(int pass=0;pass<2&&road_count<DRIVE_ROAD_LABELS;pass++){
    /* Pass 0: unobtrusive REV12 names, ordered by road priority/vicinity.
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
  /* POIs remain a separate two-icon budget and cannot steal road-label slots. */
  for(int k=0;k<ndrive_labels&&poi_count<DRIVE_POI_LABELS;k++){
    struct DriveLabel*d=&drive_labels[order[k]];
    if(d->kind<6)continue;
    int x=d->x,y=d->y;
    if(drive_label_hits_route(x-7,y-7,x+7,y+7))continue;
    if(label_free(x-8,y-8,x+8,y+8)){poi_icon(d->kind,x,y);poi_count++;}
  }
}
static void append_miles_hud(char*b,unsigned cap,u32 m){if(!display_imperial()){append_metric_distance(b,cap,m,0);return;}u32 mi10=udiv32(m*10u+804u,1609u);if(mi10>=100u){append_int(b,cap,(int)udiv32(mi10+5u,10u));appendstr(b,cap,"MI");}else{append_int(b,cap,(int)(mi10/10u));appendstr(b,cap,".");char d[2]={(char)('0'+mi10%10u),0};appendstr(b,cap,d);appendstr(b,cap,"MI");}}
static void append_eta_hud(char*b,unsigned cap,u32 ds){u32 min=udiv32(ds+300u,600u);if(min>=60u){append_int(b,cap,(int)(min/60u));appendstr(b,cap,"H");if(min%60u<10u)appendstr(b,cap,"0");append_int(b,cap,(int)(min%60u));}else{append_int(b,cap,(int)min);appendstr(b,cap,"M");}}
/* REV15: one centred, single-line navigation card replaces the lower-right
 * zoom badge. Build once before map labels so the collision filter knows the
 * real card bounds, including the width of the active route text. */
static char nav_journey[160],nav_zoom[48];
static u16 nav_journey_col,nav_zoom_col;
static int nav_calc=0;
static void nav_status_prepare(int z){
 nav_journey[0]=nav_zoom[0]=0;nav_calc=0;nav_journey_col=C_WHITE;nav_zoom_col=C_CYAN;
 if(freecam)appendstr(nav_zoom,sizeof(nav_zoom),ui_tr("Explore"));
 else if(pref.autozoom&&zoom_override>=0){appendstr(nav_zoom,sizeof(nav_zoom),ui_tr("Adjusted"));nav_zoom_col=C_WARN;}
 else appendstr(nav_zoom,sizeof(nav_zoom),pref.autozoom?ui_tr("Auto"):ui_tr("Manual"));
 if(pref.debug){appendstr(nav_zoom,sizeof(nav_zoom)," Z");append_int(nav_zoom,sizeof(nav_zoom),z);}
 if(route_status==R_READY&&route_n-route_head>1){
  appendstr(nav_journey,sizeof(nav_journey),"~");
  u32 min=udiv32(sat_add32(sat_add32(route_result_cost,atlas_future_cost),300u),600u);
  if(min>=60u){append_int(nav_journey,sizeof(nav_journey),min/60u);appendstr(nav_journey,sizeof(nav_journey),"h ");min%=60u;}
  append_int(nav_journey,sizeof(nav_journey),min);appendstr(nav_journey,sizeof(nav_journey)," min  ");
  append_miles_hud(nav_journey,sizeof(nav_journey),route_path_m());
  for(char*p=nav_journey;*p;p++){if(*p=='M')*p='m';else if(*p=='I')*p='i';else if(*p=='K')*p='k';}
 }else if(route_status==R_CALC){copystr(nav_journey,sizeof(nav_journey),route_dest_idx>0?ui_tr("Trying nearby road"):(route_rerouting?ui_tr("Rerouting"):ui_tr("Calculating route")));nav_calc=1;}
 else if(route_status==R_NOFILE){copystr(nav_journey,sizeof(nav_journey),ui_tr("Routing data missing"));nav_journey_col=C_WARN;}
 else if(route_status>=R_NOROAD){const char*m=route_status==R_NOROAD?ui_tr("No road near point"):(route_status==R_UNREACHABLE?ui_tr("No connected route"):(route_status==R_MEMORY?ui_tr("Routing memory full"):ui_tr("Routing data error")));copystr(nav_journey,sizeof(nav_journey),m);nav_journey_col=C_WARN;}
 int tw=width(nav_zoom,0);if(nav_journey[0])tw+=width(nav_journey,1)+16;
 nav_card_w=tw+16;if(nav_card_w<64)nav_card_w=64;if(nav_card_w>W-16)nav_card_w=W-16;
 nav_card_x=(W-nav_card_w)/2;nav_card_h=nav_calc?30:24;
}
static int map_notice_error_active(void);
static const char* map_notice_error_text(void);
static void draw_route_status(void){
 int y=48;
 if(map_notice_error_active()){const char*msg=map_notice_error_text();int w=width(msg,0)+24;if(w<150)w=150;if(w>W-16)w=W-16;ui_surface((W-w)/2,y,w,24,C_PANEL2);compact_center(240,y+5,w-16,0,msg,C_WARN);return;}
 ui_surface(nav_card_x,y,nav_card_w,nav_card_h,C_PANEL2);
 int lw=nav_journey[0]?width(nav_journey,1):0,zw=width(nav_zoom,0),total=lw+(lw?16:0)+zw;
 int x=nav_card_x+(nav_card_w-total)/2;
 if(lw){label(x,y+4,1,nav_journey,nav_journey_col,lw);x+=lw;fill(x+7,y+6,1,12,C_BORDER);x+=16;}
 label(x,y+5,0,nav_zoom,nav_zoom_col,zw);
 if(nav_calc){int barx=nav_card_x+10,barw=nav_card_w-20,travel=barw-36;if(travel<1)travel=1;u32 phase=(ui_anim_tick*8u)%(u32)(travel*2);int off=(int)(phase<=(u32)travel?phase:(u32)(travel*2)-phase);fill(barx,y+23,barw,2,C_BORDER);fill(barx+off,y+23,36,2,C_CYAN);}
}

static int freecam_transition_active(void){return freecam_anim_mode!=FC_ANIM_NONE;}
/* Hard recenter is reserved for programmatic state changes (arrival/trip/etc.).
 * User MAP toggling uses leave_freecam() below so it gets the reverse animation. */
static void recenter_map(void){freecam=0;map_tilt_q8=256;freecam_course_valid=0;freecam_anim_mode=FC_ANIM_NONE;freecam_anim_frames=0;freecam_anim_hold=0;zoom_override=-1;if(pref.autozoom)auto_zoom_level=-1;}
static void enter_freecam(void){if(freecam||freecam_transition_active()||(!gps.fix&&!track_valid))return;int z=active_zoom();int c=map_course();if(c<0||c>=360)c=0;freecam_course=c;freecam_course_valid=1;/* Always run five frames: even if already north-up, the tilt still animates away. */freecam_anim_mode=FC_ANIM_TO_NORTH;freecam_anim_frames=FREECAM_ORIENT_ANIM_FRAMES;freecam_anim_hold=0;map_tilt_q8=256;freecam=1;freecam_zoom=z;zoom_override=-1;/* MAP entry begins exactly at the car's current/last tracked position. */cam_lat=car_lat();cam_lon=car_lon();}
static void leave_freecam(void){if(!freecam||freecam_transition_active())return;/* Snap position back to the car immediately, but keep a north-up frame before rotating the map back onto the trusted vehicle heading. */freecam=0;freecam_course=0;freecam_course_valid=1;freecam_anim_mode=FC_ANIM_TO_CAR;freecam_anim_frames=FREECAM_ORIENT_ANIM_FRAMES;freecam_anim_hold=1;map_tilt_q8=0;zoom_override=-1;if(pref.autozoom)auto_zoom_level=-1;}
static void toggle_freecam(void){if(freecam_transition_active())return;if(freecam)leave_freecam();else enter_freecam();}
/* Entry: current heading -> north. Exit: after one recentered north-up frame,
 * north -> the current trusted car heading.  Both use the shortest angular path
 * and the project's software divider so the ARMv4T freestanding build gains no
 * compiler runtime helper. */
static void freecam_orientation_anim_step(void){
  if(freecam_anim_mode==FC_ANIM_NONE)return;
  if(freecam_anim_hold>0){freecam_anim_hold--;return;}
  int target=0,mode=freecam_anim_mode;
  if(mode==FC_ANIM_TO_CAR){target=map_course();if(target<0||target>=360){target=0;freecam_course_valid=0;}}
  if(freecam_course_valid){
    int d=target-freecam_course;while(d>180)d-=360;while(d<-180)d+=360;
    if(d&&freecam_anim_frames>0){
      u32 aq=udiv32((u32)(d<0?-d:d),(u32)freecam_anim_frames);
      int q=d<0?-(int)aq:(int)aq;if(!q)q=d>0?1:-1;
      freecam_course+=q;while(freecam_course<0)freecam_course+=360;while(freecam_course>=360)freecam_course-=360;
    }
  }
  /* Rotation may reach north/the car heading early; do not end the tilt early.
   * Both are tied to the same five rendered map frames and UI toggle lock. */
  int done=FREECAM_ORIENT_ANIM_FRAMES-freecam_anim_frames+1;
  if(done<1)done=1;if(done>FREECAM_ORIENT_ANIM_FRAMES)done=FREECAM_ORIENT_ANIM_FRAMES;
  map_tilt_q8=mode==FC_ANIM_TO_NORTH?256-(256*done)/FREECAM_ORIENT_ANIM_FRAMES:(256*done)/FREECAM_ORIENT_ANIM_FRAMES;
  freecam_anim_frames--;
  if(freecam_anim_frames<=0){freecam_course=target;map_tilt_q8=mode==FC_ANIM_TO_NORTH?0:256;if(mode==FC_ANIM_TO_CAR)freecam_course_valid=0;freecam_anim_frames=0;freecam_anim_mode=FC_ANIM_NONE;}
}
static int pan_step_for_zoom(int z){if(z<=6)return 80<<(6-z);z-=6;return 80>>z;}
/* During the short north-up transition, arrow presses must remain relative to
 * the screen rather than suddenly becoming world-north/world-east controls. */
static s32 freecam_east_to_lon(s32 east,int cq){if(!east)return 0;if(cq<=0)return east;u32 a=(u32)(east<0?-east:east);u32 q=udiv32((a<<15)+(u32)(cq>>1),(u32)cq);return east<0?-(s32)q:(s32)q;}
static void pan_freecam(int code){if(!gps.fix&&!freecam&&!track_valid)return;enter_freecam();if(!freecam)return;int z=active_zoom();int step=pan_step_for_zoom(z);if(step<1)step=1;int course=freecam_course_valid?freecam_course:0;int k=((course+5)/10)%36;int ss=SIN36[k],c=COS36[k];s32 de=0,dn=0;if(code==103){de=(s32)(((s32)step*ss)>>15);dn=(s32)(((s32)step*c)>>15);}else if(code==108){de=-(s32)(((s32)step*ss)>>15);dn=-(s32)(((s32)step*c)>>15);}else if(code==106){de=(s32)(((s32)step*c)>>15);dn=-(s32)(((s32)step*ss)>>15);}else if(code==105){de=-(s32)(((s32)step*c)>>15);dn=(s32)(((s32)step*ss)>>15);}int cq=coslat_q15(cam_lat);cam_lat+=dn;if(cam_lat>8500000)cam_lat=8500000;if(cam_lat< -8500000)cam_lat=-8500000;cam_lon=lon_delta(cam_lon+freecam_east_to_lon(de,cq));}
static void fill_car_triangle(int nx,int ny,int lx,int ly,int rx,int ry){for(int i=0;i<=12;i++){int x0=lx+((nx-lx)*i)/12,y0=ly+((ny-ly)*i)/12;int x1=rx+((nx-rx)*i)/12,y1=ry+((ny-ry)*i)/12;line1(x0,y0,x1,y1,C_CAR,1);}}
static void draw_car_marker(int z){if(!gps.fix)return;int cx,cy;world_to_screen(car_lat(),car_lon(),z,&cx,&cy);if(cx<-15||cx>W+15||cy<35||cy>250)return;int carc=map_course(),vc=view_course();int rel=view_rotates()?carc-vc:carc;while(rel<0)rel+=360;while(rel>=360)rel-=360;int k=((rel+5)/10)%36,ss=SIN36[k],c=COS36[k];int nx=cx+(ss*10>>15),ny=cy-(c*10>>15);int lx=cx+((-c*7)>>15),ly=cy+((-ss*7)>>15);int rx=cx+((c*7)>>15),ry=cy+((ss*7)>>15);fill_car_triangle(nx,ny,lx,ly,rx,ry);map_mask_begin(3);map_line(nx,ny,lx,ly,C_CAR,2);map_line(nx,ny,rx,ry,C_CAR,2);map_line(lx,ly,rx,ry,C_CAR,2);map_mask_end();}
static void draw_explore_crosshair(void){if(!freecam)return;int x=240,y=160;line1(x-12,y,x-4,y,C_DIM,1);line1(x+4,y,x+12,y,C_DIM,1);line1(x,y-12,x,y-4,C_DIM,1);line1(x,y+4,x,y+12,C_DIM,1);rect(x-2,y-2,5,5,C_DIM);}
static void draw_map_geometry(int z){fill(0,40,W,MAP_STRIP_Y-40,C_BG);if(!mapok){text_center(240,116,ui_tr("Map data missing"),C_MUTED,1);text_center(240,134,gps.fix?ui_tr("GPS active"):ui_tr("Acquiring GPS"),gps.fix?C_CYAN:C_WARN,1);return;}if(!gps.fix&&!freecam&&!track_valid){text_center(240,125,ui_tr("Acquiring GPS"),C_WARN,1);return;}s32 vl=view_lat(),vo=view_lon();if(!map_view_covered(vl,vo)){text_center(240,116,ui_tr("Outside map area"),C_MUTED,1);return;}int sourcez=z>6?6:clamp_map_zoom(z),tx0,tx1,ty0,ty1;
  map_visible_tile_bounds(z,sourcez,&tx0,&tx1,&ty0,&ty1);
  /* Ocean polygons retain close-zoom precision in shared Z0..Z3 tiles.
   * Reuse Z3 at closer zooms to avoid millions of redundant offshore tiles.
   * Finish the ocean before all local areas, strokes and roads. */
  if(MAP_AREA_FILLS&&mapfmt>=4&&(mh.flags&4)){
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
  int first=mapfmt==1?2:(MAP_AREA_FILLS?0:1);
  for(int phase=first;phase<3;phase++){
    if(phase)map_mask_begin(phase==1?0:1);
    for(int ty=ty0;ty<=ty1;ty++)for(int tx=tx0;tx<=tx1;tx++)
      if(map_tile_in_pack(sourcez,tx,ty))draw_tile_geometry(get_tile((u8)sourcez,tx,ty),z,
        phase==0?MAP_DRAW_AREAS:(phase==1?MAP_DRAW_CONTEXT:MAP_DRAW_ROADS));
    if(phase)map_mask_end();
  }
  if(mapfmt>=2){nlbox=0;nltxt=0;static const int cuts[5]={0,16,32,48,255};
    for(int b=0;b<4;b++)for(int ty=ty0;ty<=ty1;ty++)for(int tx=tx0;tx<=tx1;tx++)
      if(map_tile_in_pack(sourcez,tx,ty))draw_tile_labels(get_tile((u8)sourcez,tx,ty),z,cuts[b],cuts[b+1]-1);
  }}

/* DEV5.35 global information header.  The left side is a two-line source/
 * metadata region; the right side is three equal-width, centre-aligned cells.
 * Header metadata is always constrained to x<285 so it cannot collide with
 * volume, temperature or time. */
#define HDR_META_X 64
#define HDR_META_END 285
#define HDR_VOL_CX 317
#define HDR_TEMP_CX 382
#define HDR_TIME_CX 447
static void header(void){
  fill(0,0,W,40,C_HEADER);fill(0,39,W,1,C_SHADOW);
  char source[64],primary[193],secondary[96];source[0]=primary[0]=secondary[0]=0;
  if(streq(af.band,"CD")){
    copystr(source,sizeof(source),bt_source_label());
    if(btui.title[0])copystr(primary,sizeof(primary),btui.title);
    else if(btui.connected)copystr(primary,sizeof(primary),ui_tr("No track information"));
    else copystr(primary,sizeof(primary),bt_label());
    if(btui.artist[0])appendstr(secondary,sizeof(secondary),btui.artist);
    if(btui.album[0]){if(secondary[0])appendstr(secondary,sizeof(secondary)," - " );appendstr(secondary,sizeof(secondary),btui.album);}
  }else if(streq(af.band,"FM")||streq(af.band,"MW")||streq(af.band,"LW")){
    copystr(source,sizeof(source),af.band);
    if(af.station[0])copystr(primary,sizeof(primary),af.station);
    else if(af.freq[0])copystr(primary,sizeof(primary),af.freq);
    else copystr(primary,sizeof(primary),car.source[0]?car.source:ui_tr("Radio"));
    if(af.station[0]&&af.freq[0])copystr(secondary,sizeof(secondary),af.freq);
  }else if(streq(af.band,"AUX")){copystr(source,sizeof(source),"AUX");}
  else{copystr(source,sizeof(source),af.band[0]?af.band:"--");copystr(primary,sizeof(primary),car.source[0]?car.source:"--");}
  /* A rich Bluetooth context earns the two-line metadata layout. Otherwise
   * keep the audio content vertically centred, but preserve the exact same
   * horizontal columns: source/status at x=8 and metadata at HDR_META_X.
   * This prevents the left block shifting sideways as its content changes. */
  int rich_bt=streq(af.band,"CD")&&(bt_control_mode||btui.title[0]||btui.artist[0]||btui.album[0]);
  if(streq(af.band,"CD")){
    if(bt_control_mode){
      compact_fit(8,5,52,0,source,C_CYAN);
      label(8,22,0,ui_tr("CTRL"),C_CYAN,52);
    }else{
      char badge[16];copystr(badge,sizeof(badge),source);appendstr(badge,sizeof(badge)," ");
      appendstr(badge,sizeof(badge),!btui.connected?"--":(btui.playing?">":"II"));
      compact_center(34,13,52,0,badge,C_CYAN);
    }
  }
  if(rich_bt){
    compact_fit(HDR_META_X,3,HDR_META_END-HDR_META_X-4,1,primary,C_WHITE);
    compact_fit(HDR_META_X,22,HDR_META_END-HDR_META_X-4,0,secondary,C_MUTED);
  }else{
    char single[320];single[0]=0;
    if(primary[0])appendstr(single,sizeof(single),primary);
    if(secondary[0]){if(single[0])appendstr(single,sizeof(single),"  ");appendstr(single,sizeof(single),secondary);}
    if(!streq(af.band,"CD"))label(8,12,0,source,C_CYAN,52);
    if(single[0])compact_fit(HDR_META_X,11,HDR_META_END-HDR_META_X-4,1,single,C_WHITE);
  }
  compact_center(HDR_VOL_CX,12,58,0,car.volume[0]?car.volume:"--",C_MUTED);
  char t[12];t[0]=0;if(car.tempok){int fahrenheit=pref.units==2||(pref.units==0&&region_iso[0]=='U'&&region_iso[1]=='S');append_int(t,sizeof(t),fahrenheit?car.temp*9/5+32:car.temp);appendstr(t,sizeof(t),fahrenheit?"°F":"°C");}else copystr(t,sizeof(t),"--°");compact_center(HDR_TEMP_CX,11,58,1,t,C_MUTED);
  char tm[8]="--:--";if(gps.hour>=0){int mt=gps.hour*60+gps.min+pref.hoff*60+pref.moff;while(mt<0)mt+=1440;mt%=1440;int h=mt/60,m=mt%60;tm[0]=(char)('0'+h/10);tm[1]=(char)('0'+h%10);tm[2]=':';tm[3]=(char)('0'+m/10);tm[4]=(char)('0'+m%10);tm[5]=0;}compact_center(HDR_TIME_CX,10,62,3,tm,C_WHITE);
}
static const char* compass_name(int d){static const char*n[8]={"N","NE","E","SE","S","SW","W","NW"};return n[((d+22)/45)&7];}
static void draw_driver_strip(void){
 fill(0,MAP_STRIP_Y,W,30,C_HEADER);fill(0,MAP_STRIP_Y,W,1,C_SHADOW);fill(132,MAP_STRIP_Y+5,1,20,C_BORDER);fill(374,MAP_STRIP_Y+5,1,20,C_BORDER);
 if(!gps.fix){label(10,251,0,ui_tr("Acquiring GPS"),C_WARN,114);compact_center(253,250,226,1,lastfix_valid?ui_tr("Last position"):ui_tr("Map overview"),C_MUTED);return;}
 char sp[16];sp[0]=0;append_int(sp,sizeof(sp),display_speed());appendstr(sp,sizeof(sp)," / ");append_road_limit(sp,sizeof(sp),&roadctx);
 /* Keep the unit five pixels after the measured speed/limit group. The
  * shared face shrinks only when the whole group exceeds its compact cell. */
 const char*unit=display_imperial()?"mph":"km/h";int face=4,unitw=width(unit,0);if(width(sp,face)+5+unitw>116)face=3;
 int speedw=width(sp,face);label(8,face==4?246:248,face,sp,C_WHITE,116-unitw-5);label(8+speedw+5,253,0,unit,C_MUTED,unitw);
 const char*r=(roadctx.valid&&roadctx.street[0])?roadctx.street:ui_tr("Road unknown");compact_center(253,249,226,1,r,roadctx.valid?C_CYAN:C_MUTED);
 if(route_status==R_READY&&nav_man_type!=MAN_NONE&&nav_man_idx>=0){char d[20];d[0]=0;append_nav_distance(d,sizeof(d),nav_man_m);draw_manoeuvre_icon(391,257,nav_man_type);compact_fit(406,250,68,1,d,C_WHITE);}
 else{char hd[16];hd[0]=0;int mc=map_course();appendstr(hd,sizeof(hd),compass_name(mc));appendstr(hd,sizeof(hd)," ");append_int(hd,sizeof(hd),mc);appendstr(hd,sizeof(hd),"°");compact_center(427,249,96,1,hd,C_WHITE);}
}
/* Keep the synthetic-map warning that used to be piggybacked on the
 * footer, without resurrecting a permanent bottom bar. */
static void draw_dev_map_mark(void){if(!(mh.flags&1))return;ui_surface(8,186,132,22,C_PANEL2);text(16,192,ui_tr("Test map"),C_WARN,1);}
#include "global_cameras.h"

static void draw_map(void){int z=active_zoom();perspective_scale_update();nav_status_prepare(z);if(!freecam)drive_label_prepare(z);draw_map_geometry(z);draw_route_overlay(z);if(!freecam)drive_label_render();draw_car_marker(z);draw_explore_crosshair();draw_route_status();draw_dev_map_mark();draw_driver_strip();header();}

enum {SCR_MAP=0,SCR_MENU=1,SCR_TRIP=2,SCR_BT=3,SCR_SET=4,SCR_KEY=5};static int screen=SCR_MAP;
/* The device keyboard is intentionally naming-only. Destination discovery is
 * handled by ClioPlay Send; the on-device UI keeps only favourite naming and
 * renaming so no search sidecar/database is required. */
static char kbd_text[161];static int kbd_focus=0;
static void add_fave_place(const char*name,s32 lat,s32 lon);
static void rename_fave(int idx,const char*name);
enum {KBD_NAME_FAVE=0,KBD_RENAME_FAVE=1};static int kbd_action=KBD_NAME_FAVE;
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
#include "keyboard.h"

/* 0.5.0 navigation tree. Dashboard is replaced by Trip. The Trip page is a
 * split workspace: left = destination sources, right = ordered itinerary. CPR1
 * still calculates one active leg at a time. For multiple stops this means
 * CAR->STOP1 first; only after arrival/continue do we calculate STOP1->STOP2,
 * then the next leg, until the final destination is reached. */
#define RECENT_MAX 25
#define FAVE_MAX 25
#define TRIP_MAX_TARGETS 5
#define ARRIVAL_RADIUS_M 55u
#define ARRIVAL_TICKS 20
struct SavedPlace {s32 lat,lon;char name[161];};
struct LegacyPlace {s32 lat,lon;char name[32];};
struct TripTarget {s32 lat,lon;char name[161];};
struct LegacyTripState {u8 active,count,active_idx,paused;struct LegacyPlace target[TRIP_MAX_TARGETS];};
struct TripState {u8 active,count,active_idx,paused;struct TripTarget target[TRIP_MAX_TARGETS];};
struct OldTripState {int active;s32 dest_lat,dest_lon;char dest_name[32];};
static struct SavedPlace recents[RECENT_MAX],faves[FAVE_MAX];static int nrecents=0,nfaves=0;
static struct TripState trip;static int trip_need_route=0,trip_route_defer=0,arrival_ticks=0;

enum {MOD_NONE=0,MOD_ROUTE_POINT=1,MOD_RECENT=2,MOD_FAVE=3,MOD_CANCEL_ROUTE=4,MOD_TARGET=5,MOD_REMOVE_STOP=6,MOD_ARRIVED_STOP=7,MOD_ARRIVED_DEST=8,MOD_RESUME_TRIP=9,MOD_SOURCE_PICK=10,MOD_WAIT_PHONE=11};
static int modal=MOD_NONE,modal_choice=0,modal_index=-1;static s32 pending_lat=0,pending_lon=0;static int pending_point_stop=0;
enum {DEST_PICK_NONE=0,DEST_PICK_ADD_STOP=1,DEST_PICK_CHANGE_DEST=2};
static int dest_pick_action=DEST_PICK_NONE,trip_pick_mode=DEST_PICK_NONE,phone_wait_action=DEST_PICK_NONE;
struct PhoneDestination {int valid;u16 seq;char action;s32 lat,lon;char name[161];};
static struct PhoneDestination phone_dest;static int phone_seq_seen_valid=0;static u16 phone_seq_seen=0;
#define ARRIVAL_MODAL_MS 5000u
static u32 arrival_modal_deadline=0;
#define NOTICE_MS 1800u
static char notice_text[40];static u32 notice_deadline=0;static int notice_error=0;

static int read_places(const char*path,struct SavedPlace*a,int cap){long fd=s_open(path,O_RDONLY_,0);if(fd<0)return 0;u32 magic=0,count=0;if(s_read((int)fd,&magic,4)!=4||s_read((int)fd,&count,4)!=4||(magic!=0x31504c43u&&magic!=0x32504c43u)){s_close((int)fd);return 0;}if(count>(u32)cap)count=(u32)cap;int got=0;for(u32 i=0;i<count;i++){if(magic==0x31504c43u){struct LegacyPlace o;if(s_read((int)fd,&o,sizeof(o))!=(long)sizeof(o))break;o.name[31]=0;a[i].lat=o.lat;a[i].lon=o.lon;copystr(a[i].name,sizeof(a[i].name),o.name);}else{if(s_read((int)fd,&a[i],sizeof(a[i]))!=(long)sizeof(a[i]))break;a[i].name[160]=0;char n[161];copystr(n,sizeof(n),a[i].name);copystr(a[i].name,sizeof(a[i].name),n);}got++;}s_close((int)fd);return got;}
static void write_places(const char*path,const struct SavedPlace*a,int n){long fd=s_open(path,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd<0)return;u32 magic=0x32504c43u,count=(u32)n;s_write((int)fd,&magic,4);s_write((int)fd,&count,4);if(n>0)s_write((int)fd,a,(unsigned)n*sizeof(*a));s_close((int)fd);}
static int same_place(const struct SavedPlace*a,s32 lat,s32 lon){return iabs32(a->lat-lat)<=8&&iabs32(a->lon-lon)<=8;}
static void add_recent(const char*name,s32 lat,s32 lon){int found=-1;for(int i=0;i<nrecents;i++)if(same_place(&recents[i],lat,lon)){found=i;break;}struct SavedPlace p;p.lat=lat;p.lon=lon;copystr(p.name,sizeof(p.name),name&&name[0]?name:"MAP POINT");if(found>=0){for(int i=found;i>0;i--)recents[i]=recents[i-1];recents[0]=p;}else{if(nrecents<RECENT_MAX)nrecents++;for(int i=nrecents-1;i>0;i--)recents[i]=recents[i-1];recents[0]=p;}write_places(RECENT_PATH,recents,nrecents);}
static int fave_contains(s32 lat,s32 lon){for(int i=0;i<nfaves;i++)if(same_place(&faves[i],lat,lon))return i;return-1;}
static void add_fave_place(const char*name,s32 lat,s32 lon){struct SavedPlace p;p.lat=lat;p.lon=lon;copystr(p.name,sizeof(p.name),name&&name[0]?name:"MAP POINT");if(fave_contains(lat,lon)>=0)return;if(nfaves<FAVE_MAX)faves[nfaves++]=p;else{for(int i=1;i<FAVE_MAX;i++)faves[i-1]=faves[i];faves[FAVE_MAX-1]=p;}write_places(FAVES_PATH,faves,nfaves);}
static int fave_needs_name(const char*name){return !name||!name[0]||streq(name,"MAP POINT");}
static void add_fave_or_name(const char*name,s32 lat,s32 lon){if(fave_needs_name(name)){keyboard_open_fave_name(lat,lon);screen=SCR_KEY;}else add_fave_place(name,lat,lon);}
static void add_fave(const struct SavedPlace*p){if(p)add_fave_or_name(p->name,p->lat,p->lon);}
static void remove_fave(int idx){if(idx<0||idx>=nfaves)return;for(int i=idx+1;i<nfaves;i++)faves[i-1]=faves[i];nfaves--;write_places(FAVES_PATH,faves,nfaves);}
static void rename_fave(int idx,const char*name){if(idx<0||idx>=nfaves||!name||!name[0])return;copystr_limit(faves[idx].name,sizeof(faves[idx].name),name,40);write_places(FAVES_PATH,faves,nfaves);}
static void keyboard_open_fave_rename(int idx){if(idx<0||idx>=nfaves)return;kbd_action=KBD_RENAME_FAVE;kbd_fave_idx=idx;kbd_place_lat=faves[idx].lat;kbd_place_lon=faves[idx].lon;copystr_limit(kbd_text,sizeof(kbd_text),faves[idx].name,40);keyboard_begin();}

/* clioa2dp publishes validated CLP2 packets here. The packet carries a default
 * action (N = route as new, S = add as stop). If the user explicitly entered
 * ADD STOP / CHANGE DESTINATION -> SEND FROM PHONE on the Carminat, that local
 * action takes precedence for the next received destination. */
static void notice_set(const char*s,int is_error){copystr(notice_text,sizeof(notice_text),s&&s[0]?s:ui_tr("Done"));notice_deadline=s_now_ms()+NOTICE_MS;notice_error=is_error?1:0;}
static void notice_success(const char*s){if(screen==SCR_MAP){notice_text[0]=0;notice_deadline=0;notice_error=0;return;}notice_set(s,0);}
static void notice_fail(const char*s){notice_set(s,1);}
static int notice_active(void){return notice_text[0]&&time_before_u32(s_now_ms(),notice_deadline);}
static int map_notice_error_active(void){return screen==SCR_MAP&&modal==MOD_NONE&&notice_error&&notice_active();}
static const char* map_notice_error_text(void){return notice_text;}
static void poll_phone_destination(void){char b[384];long fd=s_open(BT_NAV_PATH,O_RDONLY_,0);if(fd<0)return;long n=s_read((int)fd,b,sizeof(b)-1);s_close((int)fd);if(n<=0)return;b[n]=0;int have_seq=0,have_action=0,have_lat=0,have_lon=0;int seqi=0;s32 lat=0,lon=0;char action=0,name[161];copystr(name,sizeof(name),"PHONE DESTINATION");char*ln=b;for(long i=0;i<=n;i++){if(b[i]=='\r'||b[i]=='\n'||b[i]==0){char save=b[i];b[i]=0;if(starts(ln,"SEQ=")){seqi=atoi0(ln+4);have_seq=seqi>=0&&seqi<=65535;}else if(starts(ln,"ACTION=")){action=ln[7];have_action=(action=='N'||action=='S')&&ln[8]==0;}else if(starts(ln,"LAT=")){lat=(s32)atoi0(ln+4);have_lat=lat>=-9000000&&lat<=9000000;}else if(starts(ln,"LON=")){lon=(s32)atoi0(ln+4);have_lon=lon>=-18000000&&lon<=18000000;}else if(starts(ln,"NAME=")){copystr_limit(name,sizeof(name),ln+5,40);}if(!save)break;ln=b+i+1;}}
  if(!have_seq||!have_action||!have_lat||!have_lon)return;u16 seq=(u16)seqi;if(phone_seq_seen_valid&&phone_seq_seen==seq)return;phone_seq_seen=seq;phone_seq_seen_valid=1;phone_dest.valid=1;phone_dest.seq=seq;phone_dest.action=action;phone_dest.lat=lat;phone_dest.lon=lon;copystr(phone_dest.name,sizeof(phone_dest.name),name[0]?name:"PHONE DESTINATION");}
static void phone_destination_clear(void){phone_dest.valid=0;phone_wait_action=DEST_PICK_NONE;}

static struct TripTarget* trip_current(void){if(!trip.active||trip.count<1||trip.active_idx>=trip.count)return 0;return &trip.target[trip.active_idx];}
static int trip_current_is_destination(void){return trip.active&&trip.count&&trip.active_idx==trip.count-1;}
static void save_trip(void){long fd=s_open(TRIP_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(fd<0)return;u32 magic=0x33525443u;s_write((int)fd,&magic,4);s_write((int)fd,&trip,sizeof(trip));s_close((int)fd);}
static void load_trip(void){memzero(&trip,sizeof(trip));long fd=s_open(TRIP_PATH,O_RDONLY_,0);if(fd<0)return;u32 magic=0;if(s_read((int)fd,&magic,4)==4){if(magic==0x33525443u){if(s_read((int)fd,&trip,sizeof(trip))!=(long)sizeof(trip))memzero(&trip,sizeof(trip));}else if(magic==0x32525443u){struct LegacyTripState o;memzero(&o,sizeof(o));if(s_read((int)fd,&o,sizeof(o))==(long)sizeof(o)){trip.active=o.active;trip.count=o.count;trip.active_idx=o.active_idx;trip.paused=o.paused;for(int i=0;i<o.count&&i<TRIP_MAX_TARGETS;i++){trip.target[i].lat=o.target[i].lat;trip.target[i].lon=o.target[i].lon;o.target[i].name[31]=0;copystr(trip.target[i].name,sizeof(trip.target[i].name),o.target[i].name);}}}else if(magic==0x31525443u){struct OldTripState o;memzero(&o,sizeof(o));if(s_read((int)fd,&o,sizeof(o))==(long)sizeof(o)&&o.active){trip.active=1;trip.count=1;trip.active_idx=0;trip.target[0].lat=o.dest_lat;trip.target[0].lon=o.dest_lon;copystr(trip.target[0].name,sizeof(trip.target[0].name),o.dest_name[0]?o.dest_name:"MAP POINT");}}}s_close((int)fd);if(trip.count>TRIP_MAX_TARGETS)trip.count=TRIP_MAX_TARGETS;if(!trip.count)trip.active=0;if(trip.active_idx>=trip.count)trip.active_idx=trip.count?trip.count-1:0;for(int i=0;i<trip.count;i++)trip.target[i].name[160]=0;trip.paused=0;}
static void trip_schedule_route(void){arrival_ticks=0;trip_need_route=1;trip_route_defer=1;route_cancel();}
static void trip_request_route(void){struct TripTarget*t=trip_current();arrival_ticks=0;if(!t||trip.paused){trip_need_route=0;trip_route_defer=0;return;}if(!gps.fix){trip_need_route=1;route_cancel();return;}trip_need_route=0;trip_route_defer=0;route_start(t->lat,t->lon);}
static void set_destination(const char*name,s32 lat,s32 lon){memzero(&trip,sizeof(trip));trip.active=1;trip.count=1;trip.active_idx=0;trip.target[0].lat=lat;trip.target[0].lon=lon;copystr(trip.target[0].name,sizeof(trip.target[0].name),name&&name[0]?name:"MAP POINT");save_trip();add_recent(trip.target[0].name,lat,lon);trip_schedule_route();}
static void clear_trip(void){memzero(&trip,sizeof(trip));save_trip();arrival_ticks=0;trip_need_route=trip_route_defer=0;route_cancel();}
static int trip_add_stop(const char*name,s32 lat,s32 lon){if(!trip.active||trip.count<1||trip.count>=TRIP_MAX_TARGETS)return 0;int ins=trip.count-1;for(int i=trip.count;i>ins;i--)trip.target[i]=trip.target[i-1];trip.target[ins].lat=lat;trip.target[ins].lon=lon;copystr(trip.target[ins].name,sizeof(trip.target[ins].name),name&&name[0]?name:"MAP POINT");trip.count++;if(ins==trip.active_idx)trip_schedule_route();save_trip();return 1;}
static void trip_change_destination(const char*name,s32 lat,s32 lon){if(!trip.active||!trip.count){set_destination(name,lat,lon);return;}int i=trip.count-1;trip.target[i].lat=lat;trip.target[i].lon=lon;copystr(trip.target[i].name,sizeof(trip.target[i].name),name&&name[0]?name:"MAP POINT");add_recent(trip.target[i].name,lat,lon);save_trip();if(i==trip.active_idx)trip_schedule_route();}
static void phone_destination_apply(void){if(!phone_dest.valid)return;int action=phone_wait_action;if(action==DEST_PICK_NONE)action=phone_dest.action=='S'?DEST_PICK_ADD_STOP:0;if(modal!=MOD_NONE&&modal!=MOD_WAIT_PHONE)return;if(modal==MOD_WAIT_PHONE)modal=MOD_NONE;if(action==DEST_PICK_CHANGE_DEST){if(trip.active&&trip.count){trip_change_destination(phone_dest.name,phone_dest.lat,phone_dest.lon);notice_success(ui_tr("DESTINATION CHANGED"));}else notice_fail(ui_tr("NO ACTIVE ROUTE"));}
  else if(action==DEST_PICK_ADD_STOP){if(!trip.active||!trip.count)notice_fail(ui_tr("NO ACTIVE ROUTE"));else if(trip.count>=TRIP_MAX_TARGETS)notice_fail(ui_tr("TRIP FULL - STOP NOT ADDED"));else if(trip_add_stop(phone_dest.name,phone_dest.lat,phone_dest.lon)){add_recent(phone_dest.name,phone_dest.lat,phone_dest.lon);notice_success(ui_tr("STOP ADDED"));}else notice_fail(ui_tr("STOP NOT ADDED"));}
  else{set_destination(phone_dest.name,phone_dest.lat,phone_dest.lon);screen=SCR_MAP;recenter_map();notice_success(ui_tr("ROUTE STARTED"));}
  phone_destination_clear();}
static void trip_remove_stop(int idx){if(!trip.active||idx<trip.active_idx||idx<0||idx>=trip.count-1)return;int was_current=idx==trip.active_idx;for(int i=idx+1;i<trip.count;i++)trip.target[i-1]=trip.target[i];trip.count--;save_trip();if(was_current)trip_schedule_route();}
static void trip_move_stop(int idx,int dir){if(!trip.active||idx<trip.active_idx||idx>=trip.count-1)return;int j=idx+dir;if(j<trip.active_idx||j>=trip.count-1)return;struct TripTarget t=trip.target[idx];trip.target[idx]=trip.target[j];trip.target[j]=t;save_trip();if(idx==trip.active_idx||j==trip.active_idx)trip_schedule_route();}
static void trip_continue_after_stop(void){if(!trip.active||trip.active_idx+1>=trip.count)return;trip.active_idx++;trip.paused=0;arrival_ticks=0;save_trip();trip_schedule_route();}
static void trip_service(void){if(trip_need_route&&gps.fix&&!trip.paused){if(trip_route_defer>0)trip_route_defer--;else trip_request_route();}if(atlas_active&&(atlas_phase!=AP_LEG||route_exact_to!=ROUTE_NOARC)){arrival_ticks=0;return;}if(!trip.active||trip.paused||!gps.fix)return;struct TripTarget*t=trip_current();if(!t)return;u32 d=geo_seg_m(car_lat(),car_lon(),t->lat,t->lon);/* dev5.6: the drivable route ends at the exact road-access snap, not at a fabricated straight connector to the POI centroid.  Count reaching that access point as arrival too. */if(route_status==R_READY&&!atlas_future_cost&&route_exact_to==ROUTE_NOARC&&route_dest_idx>=0&&route_dest_idx<route_dest_count){u32 sd=geo_seg_m(car_lat(),car_lon(),route_dest_cand[route_dest_idx].snap_lat,route_dest_cand[route_dest_idx].snap_lon);if(sd<d)d=sd;}if(d<=ARRIVAL_RADIUS_M&&gps.mph10<=100){if(arrival_ticks<ARRIVAL_TICKS)arrival_ticks++;}else arrival_ticks=0;if(arrival_ticks>=ARRIVAL_TICKS){arrival_ticks=0;trip.paused=1;route_cancel();save_trip();modal_choice=0;modal=trip_current_is_destination()?MOD_ARRIVED_DEST:MOD_ARRIVED_STOP;arrival_modal_deadline=s_now_ms()+ARRIVAL_MODAL_MS;}}
static void arrival_modal_service(void){if(modal!=MOD_ARRIVED_STOP&&modal!=MOD_ARRIVED_DEST){arrival_modal_deadline=0;return;}if(!arrival_modal_deadline)arrival_modal_deadline=s_now_ms()+ARRIVAL_MODAL_MS;if(time_before_u32(s_now_ms(),arrival_modal_deadline))return;arrival_modal_deadline=0;if(modal==MOD_ARRIVED_STOP){modal=MOD_NONE;trip_continue_after_stop();screen=SCR_MAP;recenter_map();}else{modal=MOD_NONE;clear_trip();screen=SCR_MAP;recenter_map();}}
static int menu_focus=0;
/* Three centred menu rows; branding belongs to the boot splash. */
static void menurow(int y,const char*l,const char*sub,int f){ui_choice(24,y-8,432,46,f);label(38,y-3,3,l,ui_choice_text(f),400);label(38,y+18,0,sub,f?C_ACTIVE_TEXT:C_MUTED,400);}
static void draw_menu(void){header();fill(0,40,W,H-40,C_BG);menurow(87,ui_tr("Trip"),ui_tr("Destinations and itinerary"),menu_focus==0);menurow(141,bt_label(),ui_tr("Connected and saved phones"),menu_focus==1);menurow(195,ui_tr("Settings"),ui_tr("Display and navigation"),menu_focus==2);centered(240,254,0,"(c) OpenStreetMap contributors",C_MUTED);}

static int trip_pane=0,trip_tab=0,trip_focus=0,trip_right_focus=0;
static void focus_rect(int x,int y,int w,int h,int active){ui_panel(x,y,w,h,active);}
static void trip_item(int x,int y,int w,const char*s,int f){char name[161];copystr_limit(name,sizeof(name),s,40);ui_choice(x,y-3,w,20,f);compact_fit(x+8,y-2,w-16,1,name,ui_choice_text(f));}
static const char* active_route_word(void){if(route_status==R_READY)return ui_tr("READY");if(trip_need_route||route_status==R_CALC)return ui_tr("CALC");if(route_status==R_NOFILE)return ui_tr("NO DATA");if(route_status==R_NOROAD)return ui_tr("NO ROAD");if(route_status==R_UNREACHABLE)return ui_tr("NO ROUTE");if(route_status==R_MEMORY)return ui_tr("MEM FULL");if(route_status==R_ERROR)return ui_tr("ERROR");return ui_tr("WAIT");}
static void trip_target_row(int y,int idx,int selected){
 ui_choice(249,y-3,220,20,selected);label(256,y-1,0,idx==trip.count-1?"D":"S",selected?C_ACTIVE_TEXT:C_MUTED,12);
 const char*st=idx<trip.active_idx?ui_tr("Done"):(idx==trip.active_idx?active_route_word():ui_tr("Wait"));int sw=width(st,0);if(sw<44)sw=44;if(sw>104)sw=104;int sx=462-sw;
 char name[161];copystr_limit(name,sizeof(name),trip.target[idx].name[0]?trip.target[idx].name:ui_tr("Map point"),40);compact_fit(272,y-2,sx-282,1,name,ui_choice_text(selected));
 u16 sc=selected?C_ACTIVE_TEXT:(idx==trip.active_idx?(route_status==R_READY?C_CYAN:C_WARN):C_MUTED);compact_fit(sx,y,sw,0,st,sc);
}
static void draw_trip(void){
 header();fill(0,40,W,232,C_BG);focus_rect(4,44,231,223,trip_pane==0);focus_rect(244,44,232,223,trip_pane==1);
 const char*title=trip_pick_mode==DEST_PICK_ADD_STOP?ui_tr("Add stop"):(trip_pick_mode==DEST_PICK_CHANGE_DEST?ui_tr("Change destination"):ui_tr("Destinations"));
 compact_fit(12,47,214,1,title,trip_pane==0?C_CYAN:C_WHITE);label(253,47,1,ui_tr("Current route"),trip_pane==1?C_CYAN:C_WHITE,210);
 ui_choice(10,66,104,22,trip_tab==0);centered(62,70,0,ui_tr("Recents"),trip_tab==0?C_ACTIVE_TEXT:C_MUTED);
 ui_choice(118,66,110,22,trip_tab==1);centered(173,70,0,ui_tr("Favourites"),trip_tab==1?C_ACTIVE_TEXT:C_MUTED);
 int count=trip_tab?nfaves:nrecents,first=trip_focus>7?trip_focus-7:0;if(first+8>count)first=count>8?count-8:0;
 if(!count)centered(119,122,1,trip_tab?ui_tr("No favourites yet"):ui_tr("No recent places"),C_MUTED);
 for(int row=0;row<8&&first+row<count;row++){int i=first+row;trip_item(10,96+row*21,218,trip_tab?faves[i].name:recents[i].name,trip_pane==0&&trip_focus==i);}
 if(trip.active){int rem=trip.count-trip.active_idx;
  for(int r=0;r<rem;r++)trip_target_row(76+r*22,trip.active_idx+r,trip_pane==1&&trip_right_focus==r);
  trip_item(249,76+rem*22,220,ui_tr("+ Add stop"),trip_pane==1&&trip_right_focus==rem);
  trip_item(249,98+rem*22,220,ui_tr("Cancel route"),trip_pane==1&&trip_right_focus==rem+1);
  if(route_status==R_READY){char m[64];m[0]=0;append_miles(m,sizeof(m),route_path_m());appendstr(m,sizeof(m),"  ~");append_eta(m,sizeof(m),sat_add32(route_result_cost,atlas_future_cost));compact_fit(253,228,214,0,m,C_MUTED);}
  else if(trip_need_route||route_status==R_CALC)label(253,228,0,ui_tr("Calculating route…"),C_CYAN,214);
 }else{
  centered(359,110,1,ui_tr("Choose a destination"),C_WHITE);centered(359,132,0,ui_tr("from your saved places"),C_MUTED);
  trip_item(253,178,214,ui_tr("Send from phone"),trip_pane==1);
 }
}

static int bt_page_focus=0,bt_page_connected_choice=0,bt_page_dialog=-1,bt_page_dialog_choice=0;
static const char* bt_display_name(int idx);
static void bt_page_button(int x,int y,int w,const char*l,int f){ui_choice(x,y,w,22,f);compact_center(x+w/2,y+4,w-12,0,l,ui_choice_text(f));}
static int bt_page_total(void){int ci=bt_connected_idx();return(ci>=0?1:0)+bt_saved_count()+1;}
static void bt_page_clamp(void){int t=bt_page_total();if(t<1)t=1;if(bt_page_focus<0)bt_page_focus=0;if(bt_page_focus>=t)bt_page_focus=t-1;}
static void draw_bt_page_dialog(void){if(bt_page_dialog<0||bt_page_dialog>=nbtdev)return;ui_modal(76,105,328,92);compact_center(240,117,300,1,bt_display_name(bt_page_dialog),C_WHITE);centered(240,138,0,ui_tr("Saved phone"),C_MUTED);bt_page_button(96,157,132,ui_tr("Reconnect"),bt_page_dialog_choice==0);bt_page_button(252,157,132,ui_tr("Forget"),bt_page_dialog_choice==1);}
static const char* bt_display_name(int idx){if(idx<0||idx>=nbtdev)return ui_tr("Unknown phone");if(bt_name_unknown(btdev[idx].name)){const char*cn=bt_name_cache_get(btdev[idx].addr);if(cn)return cn;if(btdev[idx].connected&&!bt_name_unknown(btui.device))return btui.device;return ui_tr("Unknown phone");}return btdev[idx].name;}
static const char*bt_status_text(void){if(pref.debug&&btui.status[0])return btui.status;if(starts(btui.status,"BT DAEMON")||starts(btui.status,"BT L2CAP")||starts(btui.status,"BT AUDIO ERR"))return bt_unavailable_label();if(starts(btui.status,"BT PAIR"))return ui_tr("Pairing phone");if(starts(btui.status,"BT WAIT"))return ui_tr("Waiting for phone");return ui_tr("Ready to connect");}
static void draw_bluetooth(void){
  header();fill(0,40,W,H-40,C_BG);label(18,46,3,bt_label(),C_WHITE,440);bt_page_clamp();
  int ci=bt_connected_idx();int conn_selected=(ci>=0&&bt_page_focus==0);
  /* Compact connected-device surface. Playback state already lives in the
   * persistent header, so this card only manages the Bluetooth device. */
  ui_surface(18,69,444,46,C_PANEL2);
  if(ci>=0){
    label(30,74,0,ui_tr("Connected"),C_MUTED,216);compact_fit(30,92,216,1,bt_display_name(ci),C_WHITE);
    bt_page_button(258,81,92,ui_tr("Disconnect"),conn_selected&&bt_page_connected_choice==0);bt_page_button(358,81,92,ui_tr("Forget"),conn_selected&&bt_page_connected_choice==1);
  }else{
    label(30,74,1,ui_tr("No phone connected"),C_MUTED,410);compact_fit(30,95,410,0,bt_status_text(),C_MUTED);
  }
  label(24,124,0,ui_tr("Saved phones"),C_MUTED,420);
  /* Six-row viewport over up to ten paired devices plus the pairing action.
   * The window follows focus, so no scrollbar or extra chrome is required. */
  int base=ci>=0?1:0,sc=bt_saved_count(),lf=bt_page_focus-base,listn=sc+1;if(lf<0)lf=0;int first=0;if(lf>5)first=lf-5;if(first+6>listn)first=listn>6?listn-6:0;
  for(int row=0;row<6&&first+row<listn;row++){
    int ord=first+row,y=142+row*20,f=bt_page_focus==base+ord;ui_choice(24,y-3,432,19,f);
    if(ord<sc){int di=bt_saved_idx(ord);compact_fit(36,y-2,326,1,di>=0?bt_display_name(di):ui_tr("Unknown phone"),ui_choice_text(f));label(392,y,0,ui_tr("Saved"),f?C_ACTIVE_TEXT:C_MUTED,52);}
    else{label(36,y-2,1,ui_tr("+ Pair new phone"),ui_choice_text(f),326);label(388,y,0,"0000",f?C_ACTIVE_TEXT:C_MUTED,56);}
  }
  if(bt_page_dialog>=0)draw_bt_page_dialog();
}

static int set_focus=0;
static void settingrow(int y,const char*l,const char*v,int f){int vw=width(v,1);if(vw<76)vw=76;if(vw>180)vw=180;int vx=446-vw;ui_choice(18,y-5,444,26,f);compact_fit(30,y-2,vx-42,1,l,ui_choice_text(f));compact_fit(vx,y-2,vw,1,v,f?C_ACTIVE_TEXT:C_MUTED);}
static const char* theme_value(void){if(pref.theme==THEME_DAY)return"DAY";if(pref.theme==THEME_NIGHT)return"NIGHT";return"AUTO";}
/* User-facing settings only: build/QAD diagnostics remain in logs rather than
 * occupying the page.  Section labels make the four controls easier to scan. */
static const char*units_value(void){return pref.units==1?ui_tr("Metric"):pref.units==2?ui_tr("Imperial"):ui_tr("Auto");}
static void draw_settings(void){header();fill(0,40,W,H-40,C_BG);label(18,46,3,ui_tr("Settings"),C_WHITE,440);char bright[16],tz[96];bright[0]=tz[0]=0;append_int(bright,sizeof(bright),pref.bright);appendstr(bright,sizeof(bright),"%");appendstr(tz,sizeof(tz),pref.clock_auto?ui_tr("Auto"):"UTC");appendstr(tz,sizeof(tz)," ");if(ui_language_rtl())appendstr(tz,sizeof(tz),"\xe2\x81\xa6");int mins=pref.hoff*60+pref.moff;appendstr(tz,sizeof(tz),mins<0?"-":"+");if(mins<0)mins=-mins;append_int(tz,sizeof(tz),mins/60);appendstr(tz,sizeof(tz),":");if(mins%60<10)appendstr(tz,sizeof(tz),"0");append_int(tz,sizeof(tz),mins%60);if(ui_language_rtl())appendstr(tz,sizeof(tz),"\xe2\x81\xa9");
 const char*names[]={ui_tr("Brightness"),ui_tr("Theme"),ui_tr("Units"),ui_tr("Auto zoom"),ui_tr("Camera alerts"),ui_tr("Clock"),ui_tr("Debug log"),ui_tr("Language")};const char*values[]={bright,pref.theme==THEME_DAY?ui_tr("Day"):pref.theme==THEME_NIGHT?ui_tr("Night"):ui_tr("Auto"),units_value(),pref.autozoom?ui_tr("On"):ui_tr("Off"),pref.cameras==2?ui_tr("Phone audio"):pref.cameras?ui_tr("Visual"):ui_tr("Off"),tz,pref.debug?ui_tr("On"):ui_tr("Off"),ui_language_name()};int first=set_focus>6?set_focus-6:0;for(int i=first;i<8&&i<first+7;i++)settingrow(72+(i-first)*28,names[i],values[i],set_focus==i);}



static void modal_option(int x,int y,int w,const char*s,int selected){ui_choice(x,y,w,22,selected);compact_center(x+w/2,y+3,w-16,1,s,ui_choice_text(selected));}
static int modal_action_count(void){if(modal==MOD_RECENT)return trip.active?3:2;if(modal==MOD_FAVE)return trip.active?4:3;if(modal==MOD_TARGET){if(modal_index>=0&&modal_index<trip.count-1)return 3;return 2;}if(modal==MOD_SOURCE_PICK)return 4;return 0;}
static void draw_modal(void){if(modal==MOD_NONE)return;if(modal==MOD_ARRIVED_STOP||modal==MOD_ARRIVED_DEST){ui_modal(74,100,332,74);compact_center(240,116,320,1,modal==MOD_ARRIVED_STOP?ui_tr("Arrived at stop"):ui_tr("Destination reached"),C_WHITE);char hint[64];copystr(hint,sizeof(hint),modal==MOD_ARRIVED_STOP?ui_tr("Continue in "):ui_tr("Finish in "));u32 now=s_now_ms(),left=arrival_modal_deadline&&time_before_u32(now,arrival_modal_deadline)?arrival_modal_deadline-now:0;append_int(hint,sizeof(hint),(int)((left+999u)/1000u));appendstr(hint,sizeof(hint),"s");compact_center(240,145,310,0,hint,C_CYAN);return;}
  if(modal==MOD_WAIT_PHONE){ui_modal(62,82,356,112);compact_center(240,96,320,1,phone_wait_action==DEST_PICK_CHANGE_DEST?ui_tr("Change destination"):(phone_wait_action==DEST_PICK_ADD_STOP?ui_tr("Add stop"):ui_tr("Receive destination")),C_WHITE);compact_center(240,126,320,1,ui_tr("Send a destination"),C_CYAN);compact_center(240,143,320,1,ui_tr("from ClioPlay Send"),C_CYAN);return;}
  if(modal==MOD_SOURCE_PICK){ui_modal(62,54,356,174);compact_center(240,68,320,1,dest_pick_action==DEST_PICK_CHANGE_DEST?ui_tr("Change destination"):ui_tr("Add stop"),C_WHITE);modal_option(100,92,280,ui_tr("Recents"),modal_choice==0);modal_option(100,119,280,ui_tr("Favourites"),modal_choice==1);modal_option(100,146,280,ui_tr("Send from phone"),modal_choice==2);modal_option(100,173,280,ui_tr("Cancel"),modal_choice==3);return;}

  int mody=78,modh=126;if(modal==MOD_FAVE&&trip.active){mody=54;modh=174;}ui_modal(62,mody,356,modh);const char*title=modal==MOD_ROUTE_POINT?(pending_point_stop?ui_tr("Add point as stop?"):ui_tr("Route to point?")):(modal==MOD_CANCEL_ROUTE?ui_tr("Cancel current route?"):(modal==MOD_REMOVE_STOP?ui_tr("Remove stop?"):(modal==MOD_RESUME_TRIP?ui_tr("Resume saved trip?"):(modal==MOD_RECENT?ui_tr("Recent trip"):(modal==MOD_FAVE?ui_tr("Favourite"):ui_tr("Manage target"))))));compact_center(240,mody+13,320,1,title,C_WHITE);
  if(modal==MOD_ROUTE_POINT||modal==MOD_CANCEL_ROUTE||modal==MOD_REMOVE_STOP||modal==MOD_RESUME_TRIP){modal_option(135,130,90,ui_tr("Yes"),modal_choice==0);modal_option(255,130,90,ui_tr("No"),modal_choice==1);return;}
  if(modal==MOD_RECENT){int row=0;if(trip.active)modal_option(100,112+row++*27,280,ui_tr("Add as stop"),modal_choice==0);modal_option(100,112+row*27,280,ui_tr("Route as new"),modal_choice==row);row++;modal_option(100,112+row*27,280,ui_tr("Add to favourites"),modal_choice==row);}
  else if(modal==MOD_FAVE){int row=0,y0=trip.active?84:116;if(trip.active){modal_option(100,y0+row*24,280,ui_tr("Add as stop"),modal_choice==row);row++;}modal_option(100,y0+row*24,280,ui_tr("Route as new"),modal_choice==row);row++;modal_option(100,y0+row*24,280,ui_tr("Rename favourite"),modal_choice==row);row++;modal_option(100,y0+row*24,280,ui_tr("Remove favourite"),modal_choice==row);}
  else if(modal==MOD_TARGET&&modal_index>=0&&modal_index<trip.count){if(modal_index<trip.count-1){modal_option(100,112,280,ui_tr("Remove stop"),modal_choice==0);modal_option(100,139,280,ui_tr("Move up"),modal_choice==1);modal_option(100,166,280,ui_tr("Move down"),modal_choice==2);}else{modal_option(100,125,280,ui_tr("Change destination"),modal_choice==0);int fi=fave_contains(trip.target[modal_index].lat,trip.target[modal_index].lon);modal_option(100,154,280,fi>=0?ui_tr("Remove favourite"):ui_tr("Add to favourites"),modal_choice==1);}}
  }

static void draw_notice(void){if(!notice_active()||modal!=MOD_NONE||screen==SCR_MAP)return;int w=width(notice_text,1)+32;if(w<150)w=150;if(w>360)w=360;int x=(W-w)/2;ui_surface(x,47,w,28,C_PANEL2);compact_center(240,53,w-16,1,notice_text,notice_error?C_WARN:C_WHITE);}
static void draw(void){clock_service();region_service();camera_service();refresh_theme();if(screen==SCR_MAP){draw_map();draw_camera_strip();}else if(screen==SCR_MENU)draw_menu();else if(screen==SCR_TRIP)draw_trip();else if(screen==SCR_BT)draw_bluetooth();else if(screen==SCR_KEY)draw_keyboard();else draw_settings();draw_modal();draw_notice();fb_flush();}

struct input_event16 {s32 sec,usec;u16 type,code;s32 value;};
#define EV_KEY 1
#define INPUT_MAX 8
#define EAGAIN_NEG (-11)
static int infd[INPUT_MAX]={-1,-1,-1,-1,-1,-1,-1,-1};
static int input_lock=-1;
static int known_remote_key(int code){return code==K_ESC||code==K_BACK||code==K_SR||code==K_ENTER||code==K_UP||code==K_LEFT||code==K_RIGHT||code==K_DOWN||code==K_MAP||code==K_MENU;}
static void input_log2(const char*tag,int ev,int code){if(!pref.debug)return;char b[72];b[0]=0;appendstr(b,sizeof(b),tag);appendstr(b,sizeof(b)," event");append_int(b,sizeof(b),ev);if(code>=0){appendstr(b,sizeof(b)," code=");append_int(b,sizeof(b),code);}log_append(KEY_LOG,b);}
static void scan_inputs(void){char p[]="/dev/input/event0";for(int i=0;i<INPUT_MAX;i++){if(infd[i]>=0)continue;p[16]=(char)('0'+i);long fd=s_open(p,O_RDONLY_|O_NONBLOCK_,0);if(fd>=0){infd[i]=(int)fd;input_log2("INPUT OPEN",i,-1);}}}
static void open_inputs(void){for(int i=0;i<INPUT_MAX;i++)infd[i]=-1;input_lock=-1;if(pref.debug)log_append(KEY_LOG,"--- CLIOPLAY GLOBAL 1.1.0-rc.1 COMPACT MEDIA INPUT ---");scan_inputs();}
static void keydiag(int code){if(!pref.debug)return;char b[48]="KEY code=";append_int(b,sizeof(b),code);log_append(KEY_LOG,b);}
static void key_handle(int code){keydiag(code);
/* Arrival/resume prompts cannot be silently escaped with MAP/BACK. */
if(code==K_MAP&&screen!=SCR_MAP&&modal!=MOD_RESUME_TRIP&&modal!=MOD_ARRIVED_STOP&&modal!=MOD_ARRIVED_DEST){modal=MOD_NONE;dest_pick_action=DEST_PICK_NONE;phone_wait_action=DEST_PICK_NONE;trip_pick_mode=DEST_PICK_NONE;screen=SCR_MAP;recenter_map();return;}
if(modal!=MOD_NONE){
  int yesno=modal==MOD_ROUTE_POINT||modal==MOD_CANCEL_ROUTE||modal==MOD_REMOVE_STOP||modal==MOD_RESUME_TRIP;
  if(code==K_BACK&&modal!=MOD_ARRIVED_STOP&&modal!=MOD_ARRIVED_DEST&&modal!=MOD_RESUME_TRIP){if(modal==MOD_WAIT_PHONE)phone_wait_action=DEST_PICK_NONE;if(modal==MOD_SOURCE_PICK)dest_pick_action=DEST_PICK_NONE;modal=MOD_NONE;return;}
  if(yesno&&(code==K_LEFT||code==K_RIGHT)){modal_choice=modal_choice?0:1;return;}
  if(!yesno&&modal!=MOD_WAIT_PHONE&&modal!=MOD_ARRIVED_STOP&&modal!=MOD_ARRIVED_DEST){int n=modal_action_count();if((code==K_UP||code==K_LEFT)&&modal_choice>0){modal_choice--;return;}if((code==K_DOWN||code==K_RIGHT)&&modal_choice+1<n){modal_choice++;return;}}
  if(code==K_ENTER){
    if(modal==MOD_ROUTE_POINT){if(modal_choice==0){if(pending_point_stop&&trip.active){if(trip_add_stop("MAP POINT",pending_lat,pending_lon))add_recent("MAP POINT",pending_lat,pending_lon);screen=SCR_TRIP;trip_pane=1;}else{set_destination("MAP POINT",pending_lat,pending_lon);screen=SCR_TRIP;trip_pane=1;trip_right_focus=0;}}modal=MOD_NONE;return;}
    if(modal==MOD_CANCEL_ROUTE){if(modal_choice==0)clear_trip();modal=MOD_NONE;return;}
    if(modal==MOD_REMOVE_STOP){if(modal_choice==0)trip_remove_stop(modal_index);modal=MOD_NONE;return;}
    if(modal==MOD_RESUME_TRIP){if(modal_choice==0){trip.paused=0;trip_need_route=1;trip_route_defer=1;}else clear_trip();modal=MOD_NONE;return;}
    if(modal==MOD_ARRIVED_STOP){arrival_modal_deadline=0;modal=MOD_NONE;trip_continue_after_stop();screen=SCR_MAP;recenter_map();return;}
    if(modal==MOD_ARRIVED_DEST){arrival_modal_deadline=0;modal=MOD_NONE;clear_trip();screen=SCR_MAP;recenter_map();return;}
    if(modal==MOD_SOURCE_PICK){if(modal_choice==0||modal_choice==1){trip_pick_mode=dest_pick_action;trip_tab=modal_choice;trip_pane=0;trip_focus=0;dest_pick_action=DEST_PICK_NONE;modal=MOD_NONE;screen=SCR_TRIP;}else if(modal_choice==2){phone_wait_action=dest_pick_action;dest_pick_action=DEST_PICK_NONE;modal=MOD_WAIT_PHONE;modal_choice=0;}else{dest_pick_action=DEST_PICK_NONE;modal=MOD_NONE;}return;}

    if(modal==MOD_RECENT&&modal_index>=0&&modal_index<nrecents){struct SavedPlace*p=&recents[modal_index];if(trip.active){if(modal_choice==0){if(trip_add_stop(p->name,p->lat,p->lon))add_recent(p->name,p->lat,p->lon);}else if(modal_choice==1){set_destination(p->name,p->lat,p->lon);screen=SCR_TRIP;trip_pane=1;trip_right_focus=0;}else add_fave(p);}else{if(modal_choice==0){set_destination(p->name,p->lat,p->lon);screen=SCR_TRIP;trip_pane=1;trip_right_focus=0;}else add_fave(p);}modal=MOD_NONE;return;}
    if(modal==MOD_FAVE&&modal_index>=0&&modal_index<nfaves){struct SavedPlace p=faves[modal_index];if(trip.active){if(modal_choice==0){if(trip_add_stop(p.name,p.lat,p.lon))add_recent(p.name,p.lat,p.lon);}else if(modal_choice==1){set_destination(p.name,p.lat,p.lon);screen=SCR_TRIP;trip_pane=1;trip_right_focus=0;}else if(modal_choice==2){keyboard_open_fave_rename(modal_index);screen=SCR_KEY;modal=MOD_NONE;return;}else remove_fave(modal_index);}else{if(modal_choice==0){set_destination(p.name,p.lat,p.lon);screen=SCR_TRIP;trip_pane=1;trip_right_focus=0;}else if(modal_choice==1){keyboard_open_fave_rename(modal_index);screen=SCR_KEY;modal=MOD_NONE;return;}else remove_fave(modal_index);}modal=MOD_NONE;return;}
    if(modal==MOD_TARGET&&modal_index>=trip.active_idx&&modal_index<trip.count){if(modal_index<trip.count-1){if(modal_choice==0){modal=MOD_REMOVE_STOP;modal_choice=1;return;}if(modal_choice==1)trip_move_stop(modal_index,-1);else trip_move_stop(modal_index,1);}else{if(modal_choice==0){dest_pick_action=DEST_PICK_CHANGE_DEST;modal=MOD_SOURCE_PICK;modal_choice=0;return;}else{int fi=fave_contains(trip.target[modal_index].lat,trip.target[modal_index].lon);if(fi>=0)remove_fave(fi);else add_fave_or_name(trip.target[modal_index].name,trip.target[modal_index].lat,trip.target[modal_index].lon);}}modal=MOD_NONE;return;}
  }
  return;
}
if(code==K_MENU){trip_pick_mode=DEST_PICK_NONE;screen=SCR_MENU;return;}
if(screen==SCR_BT&&bt_page_dialog>=0&&code==K_BACK){bt_page_dialog=-1;return;}
if(screen==SCR_KEY&&kbd_picker&&code==K_BACK){keyboard_handle(code);return;}
if(code==K_BACK){
  if(screen==SCR_TRIP&&trip_pick_mode!=DEST_PICK_NONE){trip_pick_mode=DEST_PICK_NONE;trip_pane=1;trip_right_focus=0;return;}
  if(screen==SCR_MAP&&!freecam&&bt_source_selected()){bt_control_mode=bt_control_mode?0:1;return;}
  if(screen==SCR_KEY){screen=SCR_TRIP;return;}if(screen==SCR_MENU)screen=SCR_MAP;else if(screen!=SCR_MAP)screen=SCR_MENU;return;
}
if(screen==SCR_KEY){keyboard_handle(code);return;}
if(screen==SCR_MAP){
  /* BACK enters this explicit TomTom-remote Bluetooth context.  It is kept
   * separate from Renault radio/stalk input: K_UP/K_DOWN remain navigation
   * directions and are deliberately ignored while BT control is active. */
  if(bt_control_mode){
    if(code==K_LEFT){bt_command("PREV");return;}
    if(code==K_RIGHT){bt_command("NEXT");return;}
    if(code==K_ENTER){bt_command(btui.playing?"PAUSE":"PLAY");return;}
    if(code==K_UP||code==K_DOWN)return;
  }
  if(code==K_MAP){toggle_freecam();return;}
  if(code==K_ENTER){if(freecam){pending_lat=cam_lat;pending_lon=cam_lon;pending_point_stop=trip.active?1:0;modal=MOD_ROUTE_POINT;modal_choice=0;}else if(pref.autozoom&&zoom_override>=0){zoom_override=-1;auto_zoom_level=-1;}return;}
  if(code==K_UP||code==K_DOWN||code==K_LEFT||code==K_RIGHT){pan_freecam(code);return;}
  if(code==K_ESC){int lo=mapok?(int)mh.minlvl:0;if(freecam){if(freecam_zoom>lo)freecam_zoom--;return;}if(pref.autozoom){int z=active_zoom();if(z>lo)zoom_override=z-1;return;}if(manual_zoom>lo)manual_zoom--;prefs_mark_dirty();return;}
  if(code==K_SR){if(freecam){if(freecam_zoom<MAX_EXPLORE_ZOOM)freecam_zoom++;return;}int mz=mapok?(int)mh.maxlvl:6;if(mz>6)mz=6;if(pref.autozoom){int z=active_zoom();if(z<mz)zoom_override=z+1;return;}if(manual_zoom<mz)manual_zoom++;prefs_mark_dirty();return;}
  return;}
if(screen==SCR_MENU){if(code==K_UP&&menu_focus>0)menu_focus--;else if(code==K_DOWN&&menu_focus<2)menu_focus++;else if(code==K_ENTER)screen=menu_focus==0?SCR_TRIP:(menu_focus==1?SCR_BT:SCR_SET);return;}
if(screen==SCR_TRIP){
  if(code==K_ESC){if(trip_pick_mode!=DEST_PICK_NONE){trip_pick_mode=DEST_PICK_NONE;trip_pane=1;trip_right_focus=0;}else trip_pane=0;return;}if(code==K_SR){if(trip_pick_mode!=DEST_PICK_NONE){trip_pick_mode=DEST_PICK_NONE;}trip_pane=1;trip_right_focus=0;return;}
  if(trip_pane==0){int maxf=trip_tab==0?(nrecents?nrecents-1:0):(nfaves?nfaves-1:0);if(code==K_LEFT||code==K_RIGHT){trip_tab=trip_tab?0:1;trip_focus=0;return;}if(code==K_UP&&trip_focus>0)trip_focus--;else if(code==K_DOWN&&trip_focus<maxf)trip_focus++;else if(code==K_ENTER){struct SavedPlace p;int ok=0;if(trip_tab==0&&trip_focus<nrecents){p=recents[trip_focus];ok=1;}else if(trip_tab==1&&trip_focus<nfaves){p=faves[trip_focus];ok=1;}if(ok&&trip_pick_mode!=DEST_PICK_NONE){if(trip_pick_mode==DEST_PICK_ADD_STOP){if(trip_add_stop(p.name,p.lat,p.lon)){add_recent(p.name,p.lat,p.lon);notice_success(ui_tr("STOP ADDED"));}else notice_fail(ui_tr("STOP NOT ADDED"));}else{trip_change_destination(p.name,p.lat,p.lon);notice_success(ui_tr("DESTINATION CHANGED"));}trip_pick_mode=DEST_PICK_NONE;trip_pane=1;trip_right_focus=0;return;}if(ok){modal=trip_tab==0?MOD_RECENT:MOD_FAVE;modal_index=trip_focus;modal_choice=0;}return;}}
  else if(trip.active){int rem=trip.count-trip.active_idx,total=rem+2;if(code==K_UP&&trip_right_focus>0)trip_right_focus--;else if(code==K_DOWN&&trip_right_focus+1<total)trip_right_focus++;else if(code==K_ENTER){if(trip_right_focus<rem){modal=MOD_TARGET;modal_index=trip.active_idx+trip_right_focus;modal_choice=0;}else if(trip_right_focus==rem){if(trip.count<TRIP_MAX_TARGETS){dest_pick_action=DEST_PICK_ADD_STOP;modal=MOD_SOURCE_PICK;modal_choice=0;}}else{modal=MOD_CANCEL_ROUTE;modal_choice=1;}return;}}
  else if(code==K_ENTER){phone_wait_action=DEST_PICK_NONE;modal=MOD_WAIT_PHONE;modal_choice=0;}
  return;}
if(screen==SCR_BT){if(code==K_ESC){bt_page_dialog=-1;return;}if(bt_page_dialog>=0){if(code==K_LEFT){bt_page_dialog_choice=0;return;}if(code==K_RIGHT){bt_page_dialog_choice=1;return;}if(code==K_ENTER){if(bt_page_dialog_choice==0)bt_command_arg("CONNECT",btdev[bt_page_dialog].addr);else bt_command_arg("UNPAIR",btdev[bt_page_dialog].addr);bt_page_dialog=-1;return;}return;}bt_page_clamp();int ci=bt_connected_idx(),base=ci>=0?1:0,sc=bt_saved_count(),total=base+sc+1;if(code==K_UP&&bt_page_focus>0){bt_page_focus--;return;}if(code==K_DOWN&&bt_page_focus+1<total){bt_page_focus++;return;}if(ci>=0&&bt_page_focus==0){if(code==K_LEFT){bt_page_connected_choice=0;return;}if(code==K_RIGHT){bt_page_connected_choice=1;return;}if(code==K_ENTER){if(bt_page_connected_choice==0)bt_command_arg("DISCONNECT",btdev[ci].addr);else bt_command_arg("UNPAIR",btdev[ci].addr);return;}return;}if(code==K_ENTER){int ord=bt_page_focus-base;if(ord>=0&&ord<sc){int di=bt_saved_idx(ord);if(di>=0){bt_page_dialog=di;bt_page_dialog_choice=0;}return;}if(ord==sc){bt_command("PAIR");return;}}return;}
if(screen==SCR_SET){int changed=0;if(code==K_ENTER&&set_focus==7){pref.language=(pref.language+1)%UI_LANGUAGE_COUNT;prefs_mark_dirty();return;}if(code==K_ENTER&&set_focus==5){pref.clock_auto=!pref.clock_auto;clock_next_ms=0;clock_service();prefs_mark_dirty();return;}if(code==K_UP&&set_focus>0)set_focus--;else if(code==K_DOWN&&set_focus<7)set_focus++;else if(code==K_LEFT||code==K_RIGHT){int d=code==K_RIGHT?1:-1;if(set_focus==0){pref.bright+=d*10;if(pref.bright<10)pref.bright=10;if(pref.bright>100)pref.bright=100;apply_brightness();changed=1;}else if(set_focus==1){pref.theme+=d;if(pref.theme<THEME_DAY)pref.theme=THEME_AUTO;if(pref.theme>THEME_AUTO)pref.theme=THEME_DAY;refresh_theme();changed=1;}else if(set_focus==2){pref.units=(pref.units+d+3)%3;changed=1;}else if(set_focus==3){if(pref.autozoom){manual_zoom=active_zoom();pref.autozoom=0;zoom_override=-1;}else{pref.autozoom=1;auto_zoom_level=-1;zoom_override=-1;}changed=1;}else if(set_focus==4){pref.cameras=(pref.cameras+d+3)%3;changed=1;}else if(set_focus==5){pref.clock_auto=0;int m=pref.hoff*60+pref.moff+d*15;if(m< -720)m=840;if(m>840)m=-720;pref.hoff=m/60;pref.moff=m%60;changed=1;}else if(set_focus==7){pref.language=(pref.language+d+UI_LANGUAGE_COUNT)%UI_LANGUAGE_COUNT;changed=1;}else if(set_focus==6){pref.debug=!pref.debug;changed=1;debug_state_write();if(!pref.debug)remove_stale_logs();}if(changed)prefs_mark_dirty();}return;}}

static void poll_keys(void){for(int i=0;i<INPUT_MAX;i++){if(infd[i]<0)continue;for(;;){struct input_event16 e;long n=s_read(infd[i],&e,sizeof(e));if(n==(long)sizeof(e)){if(e.type==EV_KEY&&e.value==1&&known_remote_key(e.code)){if(input_lock<0){input_lock=i;input_log2("INPUT LOCK",i,e.code);}if(input_lock==i)key_handle(e.code);}continue;}if(n==0||(n<0&&n!=EAGAIN_NEG)){s_close(infd[i]);infd[i]=-1;if(input_lock==i){input_log2("INPUT LOST",i,-1);input_lock=-1;}}break;}}}

static int power_requested(void){char b[24];long fd=s_open(POWER_PATH,O_RDWR_,0);if(fd<0)return 0;long n=s_read((int)fd,b,sizeof(b)-1);if(n<=0){s_close((int)fd);return 0;}b[n]=0;int v=atoi0(b),r=0;if(v&1){s_lseek((int)fd,0,SEEK_SET_);s_write((int)fd,"0",1);r=1;}s_close((int)fd);return r;}

#ifndef HOST_TEST
static void run(void){
  memzero(&gps,sizeof(gps));gps.hour=-1;gps.min=0;memzero(&car,sizeof(car));memzero(&af,sizeof(af));memzero(&roadctx,sizeof(roadctx));
  load_font();
#if CLIO_UNICODE
#ifdef HOST_TEST
 cliou_load(CLIO_HOST_ROOT "clio843/maps/fonts.cpf");
#else
 cliou_load("/mnt/sdcard/clio843/maps/fonts.cpf");
#endif
#endif
 load_prefs();debug_state_write();seed_boot_theme();if(!pref.debug)remove_stale_logs();load_last_audio();load_last_fix();nrecents=read_places(RECENT_PATH,recents,RECENT_MAX);nfaves=read_places(FAVES_PATH,faves,FAVE_MAX);load_trip();apply_brightness();load_map();load_timezones();load_regions();load_cameras();seed_boot_map_view();load_route_data();s_unlink(BT_CMD_PATH);bt_write_small(BT_ACTIVE_PATH,"0\n");bt_active_written=0;bt_name_cache_load();bt_poll_status();if(trip.active){modal=MOD_RESUME_TRIP;modal_choice=0;}open_inputs();
  int wdfd=(int)s_open(WD_PATH,O_WRONLY_,0);debug_log(STATE_LOG,"CLIOPLAY GLOBAL 1.1.0-rc.1 COMPACT REV16 LOWCAM224 ROADTRACE1 BTNAMECACHE1 LOWCAM224 AUTOZOOM654 PERF8 NAV QAD7+UI6+MEDIA3+FREECAM-BIDIR-ANIM+PACKETS+MMAP+STALL+REPAIRSAFE TRIPFLOW GPSRESUME BTMEDIA CPR2-v5 ATLAS1 CLOCK1 CAMERA2 ROUTE RUNTIME");debug_log(STATE_LOG,qadjmode==7?(qadj_map_ok?"QAD7 PACKET+MMAP QUERY ENABLED":"QAD7 PAGED QUERY ENABLED"):(qadjmode==6?(qadj_map_ok?"QAD6 GEOMMETA+MMAP QUERY ENABLED":"QAD6 PAGED QUERY ENABLED"):(qadjmode==5?(qadj_map_ok?"QAD5 MMAP+STALL QUERY ENABLED":"QAD5 PAGED+STALL QUERY ENABLED"):(qadjmode==4?"QAD4 LOCAL-RECON QUERY ENABLED":(qadjmode==3?"QAD3 GROUP QUERY+RECON ENABLED":(qadjmode==2?"QAD2 NODE-BLOCK QUERY ENABLED (PAGED INDEX)":(qadjmode==1?"QAD1 FAST QUERY ENABLED":"QADJ FAST QUERY FALLBACK CPR2")))))));if(qadjmode==6&&qadj6_legacy_header)debug_log(STATE_LOG,"QAD6 DEV5.31 HEADER COMPAT ENABLED");{char mb[96];mb[0]=0;appendstr(mb,sizeof(mb),"MMAP QAD=");append_int(mb,sizeof(mb),qadj_map_ok);appendstr(mb,sizeof(mb)," GEOM=");append_int(mb,sizeof(mb),route_geom_map_ok);debug_log(STATE_LOG,mb);}u32 tick=0,next_frame=s_now_ms();
  for(;;){
    next_frame+=ROUTE_FRAME_MS;
    if((tick%5)==0)scan_inputs();int had_fix=gps.fix;poll_gps();if(gps.fix){if(!had_fix){track_valid=0;track_course=-1;}remember_live_fix();}update_smooth_track();update_display_course();poll_keys();if((tick%2)==0){poll_audio();poll_car();bt_source_service();}if((tick%5)==0){bt_poll_status();poll_phone_destination();}if((tick%10)==0)update_road_context();
    phone_destination_apply();trip_service();arrival_modal_service();if(bt_control_mode&&(screen!=SCR_MAP||modal!=MOD_NONE||freecam||!bt_source_selected()))bt_control_mode=0;prefs_service();ui_anim_tick=tick;if((tick%10)==0&&wdfd>=0)s_write(wdfd,"W",1);

    /* DEV5.12 routed only in whatever time the completed frame happened to leave.
     * PERF2 gives an active query a guaranteed bounded foreground slice instead.
     * When parked this is intentionally aggressive; while moving or rerouting it
     * stays shorter so navigation/input cadence remains smooth. */
    int turbo_calc=(route_status==R_CALC&&!route_rerouting&&gps.mph10<20);
    if(route_status==R_CALC){u32 slice=turbo_calc?ROUTE_INITIAL_SLICE_MS:ROUTE_MOVING_SLICE_MS;if(route_prof_on){u32 pt=s_now_ms();route_prof_slice_ms=pt;route_service_until(pt+slice);u32 pe=s_now_ms();route_prof_cpu_ms+=pe-pt;route_prof_slice_ms=0;}else route_service_until(s_now_ms()+slice);}
    else if(route_status==R_READY)route_service_until(s_now_ms());

    /* Full map rendering is expensive on the 400 MHz CPU. During a stationary
     * initial calculation 5 Hz is still visually smooth enough, while every
     * skipped render leaves an entire frame for CH work. Moving reroutes remain
     * at the normal draw cadence. Always draw immediately when a route finishes. */
    int do_draw=(screen==SCR_MAP||modal!=MOD_NONE||((tick%2)==0));
    if(turbo_calc&&route_status==R_CALC&&screen==SCR_MAP&&modal==MOD_NONE&&(tick&1u))do_draw=0;
    /* Advance only on rendered map frames so the transition remains visibly
     * five frames even if route calculation temporarily skips a draw. */
    if(do_draw&&screen==SCR_MAP)freecam_orientation_anim_step();
    if(do_draw)draw();

    if(power_requested()){debug_log(STATE_LOG,"POWER REQUEST");bt_write_small(BT_ACTIVE_PATH,"0\n");if(prefs_dirty)save_prefs();save_last_audio();if(gps.fix)remember_live_fix();save_last_fix();save_trip();draw();s_sync();s_nanosleep(0,400000000);if(wdfd>=0)s_close(wdfd);for(int i=0;i<INPUT_MAX;i++)if(infd[i]>=0)s_close(infd[i]);if(gpsfd>=0)s_close(gpsfd);if(audiofd>=0)s_close(audiofd);if(mapfd>=0)s_close(mapfd);if(qadjfd>=0)s_close(qadjfd);if(routefd>=0)s_close(routefd);return;}
    u32 now=s_now_ms();if(time_before_u32(now,next_frame)){u32 rem=next_frame-now;s_nanosleep((long)(rem/1000u),(long)(rem%1000u)*1000000L);}else if((u32)(now-next_frame)>ROUTE_FRAME_MS*3u){next_frame=now;}tick++;
  }
}
__attribute__((noreturn))void _start(void){run();s_exit(0);}
#else
int main(void){memzero(&gps,sizeof(gps));gps.fix=1;gps.sats=8;gps.lat=5498200;gps.lon=-165638;gps.mph10=380;gps.course=70;gps.hour=13;gps.min=8;gps.day=15;gps.mon=9;gps.year=2026;copystr(car.source,sizeof(car.source),"FM BBC R1");copystr(car.volume,sizeof(car.volume),"VOL 12");car.temp=18;car.tempok=1;load_font();load_prefs();
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
screen=SCR_TRIP;notice_success(ui_tr("STOP ADDED"));
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
screen=SCR_MAP;modal=MOD_NONE;notice_fail(ui_tr("NO ACTIVE ROUTE"));
#endif
#ifdef PREVIEW_MAP_NOTICE_SUCCESS
screen=SCR_MAP;modal=MOD_NONE;notice_success(ui_tr("STOP ADDED"));
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
