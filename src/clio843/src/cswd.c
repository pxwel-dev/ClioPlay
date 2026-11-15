/* cswd — Scenic III / Mégane central-switch (armrest console) bridge.
 *
 * TomTom's vehicledaemon writes 28-byte UiInput::MCswInput::TEventStruct
 * records to the FIFO /dev/input/CswEvent0, which the stock navigation app
 * normally creates and reads.  This helper takes that reader role, decodes
 * each record and forwards the matching ClioPlay key as a Linux input_event
 * through a FIFO at /dev/input/event7, which clio040 already scans.
 *
 * TEventStruct layout (from vehicledaemon CCentralSwitch::Set* routines):
 *   +0  source   0 = joystick/knob, 1..9 = hard buttons
 *   +4  dir      1..4 = direction, 5 = centre
 *   +8  rot      1/2 = rotation direction, 3 = none
 *   +12 aux      1/2
 *   +16 pressed  1 = joystick pushed
 *   +20 duration 1 = short, 2 = long
 *   +24 steps    rotation steps (byte)
 *
 * Mapping is read from /media/sdcard/clio843/cswmap.ini (name=KEY lines) so
 * it can be corrected without rebuilding.  Every record is logged to
 * cswd.log (capped) for diagnosis.
 *
 * Written and tested on the car in the Renatka fork
 * (https://github.com/bobczan/Renatka), MIT, offered upstream as-is.
 * Surrounding hardware facts: src/docs/scenic-iii-hardware.md.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;

#ifndef HOST_TEST
static inline long sc1(long nr,long a){ register long r0 __asm__("r0")=a; __asm__ volatile("svc %1":"+r"(r0):"i"(0x900000+nr):"memory","cc"); return r0; }
static inline long sc2(long nr,long a,long b){ register long r0 __asm__("r0")=a; register long r1 __asm__("r1")=b; __asm__ volatile("svc %2":"+r"(r0):"r"(r1),"i"(0x900000+nr):"memory","cc"); return r0; }
static inline long sc3(long nr,long a,long b,long c){ register long r0 __asm__("r0")=a; register long r1 __asm__("r1")=b; register long r2 __asm__("r2")=c; __asm__ volatile("svc %3":"+r"(r0):"r"(r1),"r"(r2),"i"(0x900000+nr):"memory","cc"); return r0; }
static long s_read(int fd,void*p,unsigned n){return sc3(3,fd,(long)p,(long)n);}
static long s_write(int fd,const void*p,unsigned n){return sc3(4,fd,(long)p,(long)n);}
static long s_open(const char*p,int f,int m){return sc3(5,(long)p,f,m);}
static long s_close(int fd){return sc1(6,fd);}
static long s_unlink(const char*p){return sc1(10,(long)p);}
static long s_mknod(const char*p,int mode,int dev){return sc3(14,(long)p,mode,dev);}
struct kts { long sec,nsec; };
struct ktv { long sec,usec; };
static long s_nanosleep(long sec,long ns){ struct kts t={sec,ns}; return sc2(162,(long)&t,0); }
static u32 s_now_ms(void){struct kts t;if(sc2(263,1,(long)&t)>=0)return (u32)t.sec*1000u+(u32)(t.nsec/1000000L);struct ktv v;if(sc2(78,(long)&v,0)>=0)return (u32)v.sec*1000u+(u32)(v.usec/1000L);return 0;}
__attribute__((noreturn)) static void s_exit(int c){sc1(1,c);for(;;){}}
#define CSW_PATH "/dev/input/CswEvent0"
#define OUT_PATH "/dev/input/event7"
#define LOG_PATH "/media/sdcard/clio843/cswd.log"
#define MAP_PATH "/media/sdcard/clio843/cswmap.ini"
#else
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
static long s_read(int fd,void*p,unsigned n){return read(fd,p,n);}
static long s_write(int fd,const void*p,unsigned n){return write(fd,p,n);}
static long s_open(const char*p,int f,int m){return open(p,f,m);}
static long s_close(int fd){return close(fd);}
static long s_unlink(const char*p){return unlink(p);}
static long s_mknod(const char*p,int mode,int dev){return mknod(p,(mode_t)mode,(dev_t)dev);}
static long s_nanosleep(long sec,long ns){struct timespec t={sec,ns};return nanosleep(&t,0);}
static u32 s_now_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (u32)t.tv_sec*1000u+(u32)(t.tv_nsec/1000000L);}
__attribute__((noreturn,unused)) static void s_exit(int c){exit(c);}
#ifndef HOST_ROOT
#define HOST_ROOT "/tmp/cswd-host/"
#endif
#define CSW_PATH HOST_ROOT "CswEvent0"
#define OUT_PATH HOST_ROOT "event7"
#define LOG_PATH HOST_ROOT "cswd.log"
#define MAP_PATH HOST_ROOT "cswmap.ini"
#endif

#define O_RDONLY_ 0
#define O_WRONLY_ 1
#define O_RDWR_ 2
#define O_CREAT_ 0x40
#define O_TRUNC_ 0x200
#define O_APPEND_ 0x400
#define O_NONBLOCK_ 0x800
#define S_IFIFO_ 0x1000

/* ClioPlay key codes (clio040.c). */
#define K_ESC 1      /* left soft key: zoom out */
#define K_BACK 13
#define K_SR 14      /* right soft key: zoom in */
#define K_ENTER 28
#define K_UP 103
#define K_LEFT 105
#define K_RIGHT 106
#define K_DOWN 108
#define K_MAP 67
#define K_MENU 68
#define K_ROTN 191   /* knob clockwise: zoom on map, next item elsewhere */
#define K_ROTP 192   /* knob anticlockwise */

static int slen(const char*s){int n=0;while(s[n])n++;return n;}
static int streq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}

/* ---- log (capped so it can never fill the card) ---- */
static int logfd=-1;static u32 logged=0;
#define LOG_CAP 200000u
static void log_s(const char*s){if(logfd<0||logged>LOG_CAP)return;int n=slen(s);s_write(logfd,s,(unsigned)n);logged+=(u32)n;}
static void log_hex(u32 v,int digits){char b[9];for(int i=digits-1;i>=0;i--){int d=(int)(v&15u);b[i]=(char)(d<10?'0'+d:'a'+d-10);v>>=4;}b[digits]=0;log_s(b);}
static void log_dec(s32 v){char b[12];int i=11;b[i]=0;int neg=v<0;u32 u=neg?(u32)(-v):(u32)v;do{b[--i]=(char)('0'+u%10u);u/=10u;}while(u&&i>1);if(neg)b[--i]='-';log_s(b+i);}

/* ---- mapping ---- */
enum {M_BTN1=0,M_BTN9=8,M_DIR1=9,M_DIR4=12,M_ROT1=13,M_ROT2=14,M_PUSH=15,M_LONG=16,M_COUNT=17};
static int keymap[M_COUNT];
static const char*slot_name(int i){static const char*n[M_COUNT]={"btn1","btn2","btn3","btn4","btn5","btn6","btn7","btn8","btn9","dir1","dir2","dir3","dir4","rot1","rot2","push","longpush"};return n[i];}
static int key_from_name(const char*v){
 if(streq(v,"MAP"))return K_MAP;
 if(streq(v,"MENU"))return K_MENU;
 if(streq(v,"BACK"))return K_BACK;
 if(streq(v,"OK"))return K_ENTER;
 if(streq(v,"UP"))return K_UP;
 if(streq(v,"DOWN"))return K_DOWN;
 if(streq(v,"LEFT"))return K_LEFT;
 if(streq(v,"RIGHT"))return K_RIGHT;
 if(streq(v,"ZOOMOUT"))return K_ESC;
 if(streq(v,"ZOOMIN"))return K_SR;
 if(streq(v,"ROTNEXT"))return K_ROTN;
 if(streq(v,"ROTPREV"))return K_ROTP;
 return 0;}
static void default_map(void){
 static const int d[M_COUNT]={K_ESC,K_BACK,K_SR,0,K_MENU,0,0,K_MENU,K_MAP, K_UP,K_DOWN,K_RIGHT,K_LEFT, K_ROTP,K_ROTN, K_ENTER,K_BACK};
 for(int i=0;i<M_COUNT;i++)keymap[i]=d[i];}
static void load_map(void){
 default_map();
 long fd=s_open(MAP_PATH,O_RDONLY_,0);if(fd<0){log_s("MAP default (no cswmap.ini)\n");return;}
 static char buf[2048];long n=s_read((int)fd,buf,sizeof(buf)-1);s_close((int)fd);if(n<=0)return;buf[n]=0;
 char*p=buf;
 while(*p){char*line=p;while(*p&&*p!='\n')p++;if(*p)*p++=0;
  int L=slen(line);while(L>0&&(line[L-1]=='\r'||line[L-1]==' '))line[--L]=0;
  if(line[0]=='#'||!line[0])continue;
  char*eq=line;while(*eq&&*eq!='=')eq++;if(!*eq)continue;*eq=0;char*val=eq+1;
  for(int i=0;i<M_COUNT;i++)if(streq(line,slot_name(i))){keymap[i]=key_from_name(val);log_s("MAP ");log_s(line);log_s("=");log_s(val);log_s("\n");}
 }}

/* ---- output: Linux input_event into the FIFO clio040 reads ---- */
struct input_event16 {s32 sec,usec;u16 type,code;s32 value;};
static int outfd=-1;
static void emit(int code,const char*why){
 if(!code)return;
 struct input_event16 e;e.sec=0;e.usec=0;e.type=1;e.code=(u16)code;e.value=1;
 long a=s_write(outfd,&e,sizeof(e));e.value=0;s_write(outfd,&e,sizeof(e));
 log_s("  -> key ");log_dec(code);log_s(" (");log_s(why);log_s(") rc=");log_dec((s32)a);log_s("\n");}

/* ---- decoding ---- */
struct csw_event {s32 source,dir,rot,aux,pressed,duration;u8 steps,pad[3];};
typedef char csw_size_check[sizeof(struct csw_event)==28?1:-1];
static s32 prev_dir=5,prev_pressed=0;static int last_btn=-1;static u32 last_btn_ms=0;

static void handle(const struct csw_event*e,const u8*raw){
 u32 now=s_now_ms();
 log_s("EV t=");log_dec((s32)now);log_s(" raw=");for(int i=0;i<28;i++){log_hex(raw[i],2);}
 log_s(" src=");log_dec(e->source);log_s(" dir=");log_dec(e->dir);log_s(" rot=");log_dec(e->rot);
 log_s(" aux=");log_dec(e->aux);log_s(" pr=");log_dec(e->pressed);log_s(" dur=");log_dec(e->duration);log_s(" st=");log_dec(e->steps);log_s("\n");
 if(e->source>=1&&e->source<=9){
  int slot=M_BTN1+e->source-1;
  if(last_btn==e->source&&(u32)(now-last_btn_ms)<400u){log_s("  (debounced)\n");return;}
  last_btn=e->source;last_btn_ms=now;emit(keymap[slot],slot_name(slot));return;}
 if(e->source!=0)return;
 if(e->pressed==1&&prev_pressed!=1)emit(e->duration==2?keymap[M_LONG]:keymap[M_PUSH],e->duration==2?"longpush":"push");
 prev_pressed=e->pressed;
 if(e->dir>=1&&e->dir<=4&&e->dir!=prev_dir)emit(keymap[M_DIR1+e->dir-1],slot_name(M_DIR1+e->dir-1));
 prev_dir=e->dir;
 if(e->rot==1||e->rot==2){int n=e->steps;if(n<1)n=1;if(n>5)n=5;int slot=e->rot==1?M_ROT1:M_ROT2;while(n--)emit(keymap[slot],slot_name(slot));}
}

static int make_fifo(const char*path){s_unlink(path);long r=s_mknod(path,S_IFIFO_|0666,0);log_s("FIFO ");log_s(path);log_s(" rc=");log_dec((s32)r);log_s("\n");return r>=0;}

static int run(void){
 logfd=(int)s_open(LOG_PATH,O_WRONLY_|O_CREAT_|O_TRUNC_,0644);
 log_s("--- cswd 2 ---\n");
 load_map();
 make_fifo(OUT_PATH);
 outfd=(int)s_open(OUT_PATH,O_RDWR_|O_NONBLOCK_,0);
 log_s("OUT fd=");log_dec(outfd);log_s("\n");
 make_fifo(CSW_PATH);
 /* O_RDWR keeps the FIFO alive (no EOF) when the writer closes. */
 int fd=(int)s_open(CSW_PATH,O_RDWR_,0);
 log_s("CSW fd=");log_dec(fd);log_s("\n");
 if(fd<0||outfd<0){log_s("FATAL open failed\n");return 1;}
 u8 raw[28];int have=0;
 for(;;){
  long n=s_read(fd,raw+have,(unsigned)(28-have));
  if(n<=0){s_nanosleep(0,100000000);continue;}
  have+=(int)n;if(have<28)continue;have=0;
  struct csw_event e;u8*d=(u8*)&e;for(int i=0;i<28;i++)d[i]=raw[i];
  handle(&e,raw);
 }
}

#ifndef HOST_TEST
__attribute__((noreturn))void _start(void){s_exit(run());}
#include "arm_runtime.h"
#else
int main(void){return run();}
#endif
