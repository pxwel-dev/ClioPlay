/* ClioPlay UI — maintained, theme-aware RGB565 boot screen.
 * Draw once and return immediately: no fake progress or added boot delay. */
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
static void*s_mmap2(void*addr,u32 len,int prot,int flags,int fd,u32 pgoff){void*p=mmap(addr,len,prot,flags,fd,(off_t)pgoff*4096);return p==MAP_FAILED?0:p;}
static long s_munmap(void*p,u32 len){return munmap(p,len);}
static long s_sync(void){return 0;} 
static long s_rename(const char*a,const char*b){return rename(a,b);} 
static long s_nanosleep(long sec,long ns){(void)sec;(void)ns;return 0;}
static u32 s_now_ms(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t)==0)return (u32)t.tv_sec*1000u+(u32)(t.tv_nsec/1000000L);return 0;} 
__attribute__((noreturn)) static void s_exit(int c){exit(c);} 
#endif

#define W 480
#define H 272
#define FB_PIX (W*H)
#ifndef HOST_TEST
#define SPLASH_FB "/dev/fb"
#define SPLASH_PREF "/media/sdcard/clio843/prefs.ini"
#define SPLASH_THEME "/media/sdcard/clio843/theme.dat"
#define SPLASH_BL_MAX "/sys/class/backlight/s3c/max_brightness"
#define SPLASH_BL_VAL "/sys/class/backlight/s3c/brightness"
#else
#ifndef CLIO_HOST_ROOT
#define CLIO_HOST_ROOT "/tmp/clioplay-ui-host/"
#endif
#define SPLASH_FB CLIO_HOST_ROOT "splash.fb"
#define SPLASH_PREF CLIO_HOST_ROOT "clio843/prefs.ini"
#define SPLASH_THEME CLIO_HOST_ROOT "clio040_theme.dat"
#define SPLASH_BL_MAX CLIO_HOST_ROOT "maxbrightness"
#define SPLASH_BL_VAL CLIO_HOST_ROOT "brightness"
#endif
static u16 fb[FB_PIX];
static int clip_x=0,clip_y=0,clip_right=W,clip_bottom=H;
static void px(int x,int y,u16 c){if((unsigned)x<W&&(unsigned)y<H)fb[y*W+x]=c;}
static void fill(int x,int y,int w,int h,u16 c){if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}if(x+w>W)w=W-x;if(y+h>H)h=H-y;for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++)fb[yy*W+xx]=c;}
#include "ui_raster.h"
#include "boot_theme.h"
#include "splash_icon.h"
static int spl_theme=2,spl_bright=50,spl_hoff=0,spl_moff=0;
static int spl_int(const char*s){int sign=1,v=0;if(*s=='-'){sign=-1;s++;}for(int n=0;n<6&&*s>='0'&&*s<='9';n++,s++)v=v*10+*s-'0';return v*sign;}
static int spl_eq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static void spl_load_prefs(void){
 char b[512];long fd=s_open(SPLASH_PREF,0,0);if(fd<0)return;long n=s_read(fd,b,sizeof(b)-1);s_close(fd);if(n<=0)return;b[n]=0;
 for(char*p=b;*p;){char*name=p;while(*p&&*p!='\r'&&*p!='\n')p++;char next=*p;*p=0;char*eq=name;while(*eq&&*eq!='=')eq++;
  if(*eq){*eq++=0;int v=spl_int(eq);if(spl_eq(name,"theme"))spl_theme=v;else if(spl_eq(name,"bright"))spl_bright=v;else if(spl_eq(name,"hoff"))spl_hoff=v;else if(spl_eq(name,"moff"))spl_moff=v;}
  if(!next)break;p++;
 }
 if(spl_theme<0||spl_theme>2)spl_theme=2;if(spl_bright<10)spl_bright=10;if(spl_bright>100)spl_bright=100;
}
static int spl_saved_theme(void){char c=0;long fd=s_open(SPLASH_THEME,0,0);if(fd<0)return-1;long n=s_read(fd,&c,1);s_close(fd);return n==1&&c=='0'?0:(n==1&&c=='1'?1:-1);}
#ifdef CLIO_SPLASH_TEST
static int spl_test_minutes=-2;
#endif
static int spl_clock_minutes(void){long sec=-1;
#ifdef CLIO_SPLASH_TEST
 if(spl_test_minutes>=-1)return spl_test_minutes;
#endif
#ifndef HOST_TEST
 struct ktv v;if(sc2(78,(long)&v,0)>=0)sec=v.sec;
#else
 time_t t=time(0);if(t!=(time_t)-1)sec=(long)t;
#endif
 return clio_boot_minutes(sec,spl_hoff,spl_moff);
}
static void spl_brightness(void){
 char b[24];int max=100;long fd=s_open(SPLASH_BL_MAX,0,0);if(fd>=0){long n=s_read(fd,b,sizeof(b)-1);s_close(fd);if(n>0){b[n]=0;int v=spl_int(b);if(v>0&&v<=65535)max=v;}}
 int v=(spl_bright*max+50)/100,n=0;char rev[16];do{rev[n++]=(char)('0'+v%10);v/=10;}while(v);for(int i=0;i<n;i++)b[i]=rev[n-i-1];
 fd=s_open(SPLASH_BL_VAL,1,0);if(fd>=0){s_write(fd,b,n);s_close(fd);}
}
static void spl_icon(int ix,int iy){
 /* Composite canvas, blue tile and white mark once per pixel. The masks are
  * sampled offline, so smooth edges add no large buffer or boot-time sampling.
  * A single composite avoids a dark seam around the white arrow. */
 for(int y=0;y<SPL_ICON_SIZE;y++){
  int d=SPL_ICON_SIZE-1;
  u16 blue=rgb((74*(d-y)+d/2)/d,(177*(d-y)+96*y+d/2)/d,(255*(d-y)+235*y+d/2)/d);
  for(int x=0;x<SPL_ICON_SIZE;x++){
   int i=y*SPL_ICON_SIZE+x,a=spl_icon_outer[i],white=spl_icon_arrow[i];if(!a)continue;
   u16 bg=fb[(iy+y)*W+ix+x];int tile=a-white;
   int r=(((bg>>11)&31)*(64-a)+((blue>>11)&31)*tile+31*white+32)>>6;
   int g=(((bg>>5)&63)*(64-a)+((blue>>5)&63)*tile+63*white+32)>>6;
   int b=((bg&31)*(64-a)+(blue&31)*tile+31*white+32)>>6;
   px(ix+x,iy+y,(u16)((r<<11)|(g<<5)|b));
  }
 }
}
static void spl_render(int night){
 fill(0,0,W,H,ui_canvas(night));
 /* Centre the icon and visible title bounds as one group. Glyph top/side
  * bearings are excluded from the group's padding, so font metrics cannot
  * leave the mark floating above the screen centre. */
 const char*title="ClioPlay",*p=title;int pen=0,left=W,right=0,top=H,bottom=0;
 while(*p){int code=cp_code(&p);const struct CpGlyph*g=&cp_glyphs[6*98+cp_slot(code)];
  if(g->w&&g->h){if(pen+g->left<left)left=pen+g->left;if(pen+g->left+g->w>right)right=pen+g->left+g->w;if(g->top<top)top=g->top;if(g->top+g->h>bottom)bottom=g->top+g->h;}
  pen+=g->advance;
 }
 const int icon=SPL_ICON_SIZE,gap=20;int textw=right-left,texth=bottom-top;
 int groupw=textw>icon?textw:icon,grouph=icon+gap+texth;
 int groupx=(W-groupw)/2,groupy=(H-grouph)/2;
 int ix=groupx+(groupw-icon)/2,iy=groupy;
 /* All parts derive from the same group origin. */
 spl_icon(ix,iy);
 label(groupx+(groupw-textw)/2-left,groupy+icon+gap-top,6,title,night?rgb(249,250,255):rgb(23,29,38),0);
}
static int spl_present(void){long fd=s_open(SPLASH_FB,1,0);if(fd<0)return 0;unsigned done=0;while(done<sizeof(fb)){long n=s_write(fd,(const u8*)fb+done,sizeof(fb)-done);if(n<=0)break;done+=(unsigned)n;}s_close(fd);return done==sizeof(fb);}
static int spl_run(void){spl_load_prefs();int night=clio_boot_night(spl_theme,spl_clock_minutes(),spl_saved_theme());spl_brightness();spl_render(night);return spl_present()?0:1;}
#ifndef HOST_TEST
__attribute__((noreturn))void _start(void){s_exit(spl_run());}
#else
int main(void){return spl_run();}
#endif
#include "arm_runtime.h"
