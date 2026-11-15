/* vstatd — provides the navigation-side sockets vehicledaemon connects to
 * (/var/run/PAC<port>) and turns VehicleStatus (interface 37) updates into
 * /tmp/cltemp for the UI.
 *
 * VehicleStatus Update packet: u16 iid=37, u8 method=1, u32 id, u32 value
 * (little endian).  id 4 = outside temperature in degC; -1024 = unknown.
 * Nothing is ever sent back to vehicledaemon.
 *
 * Written for the Renatka fork (https://github.com/bobczan/Renatka), MIT,
 * offered upstream as-is. Hardware facts: src/docs/scenic-iii-hardware.md.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;

static inline long sc1(long nr,long a){ register long r0 __asm__("r0")=a; __asm__ volatile("svc %1":"+r"(r0):"i"(0x900000+nr):"memory","cc"); return r0; }
static inline long sc2(long nr,long a,long b){ register long r0 __asm__("r0")=a; register long r1 __asm__("r1")=b; __asm__ volatile("svc %2":"+r"(r0):"r"(r1),"i"(0x900000+nr):"memory","cc"); return r0; }
static inline long sc3(long nr,long a,long b,long c){ register long r0 __asm__("r0")=a; register long r1 __asm__("r1")=b; register long r2 __asm__("r2")=c; __asm__ volatile("svc %3":"+r"(r0):"r"(r1),"r"(r2),"i"(0x900000+nr):"memory","cc"); return r0; }
static long s_read(int fd,void*p,unsigned n){return sc3(3,fd,(long)p,(long)n);}
static long s_write(int fd,const void*p,unsigned n){return sc3(4,fd,(long)p,(long)n);}
static long s_open(const char*p,int f,int m){return sc3(5,(long)p,f,m);}
static long s_close(int fd){return sc1(6,fd);}
static long s_unlink(const char*p){return sc1(10,(long)p);}
static long s_rename(const char*a,const char*b){return sc2(38,(long)a,(long)b);}
static long s_poll(void*p,u32 n,int timeout){return sc3(168,(long)p,(long)n,timeout);}
static long s_socketcall(int call,unsigned long*a){return sc2(102,call,(long)a);}
struct kts { long sec,nsec; };
static u32 s_now_ms(void){struct kts t;if(sc2(263,1,(long)&t)>=0)return (u32)t.sec*1000u+(u32)(t.nsec/1000000L);return 0;}
__attribute__((noreturn)) static void s_exit(int c){sc1(1,c);for(;;){}}

#define LOG_PATH "/media/sdcard/clio843/vstatd.log"
#define TEMP_PATH "/tmp/cltemp"
#define TEMP_TMP "/tmp/cltemp.tmp"
#define SC_SOCKET 1
#define SC_BIND 2
#define SC_LISTEN 4
#define SC_ACCEPT 5
#define AF_UNIX_ 1
#define SOCK_SEQPACKET_ 5
#define POLLIN_ 1

struct pollfd0{int fd;short events,revents;};
struct sun0{u16 family;char path[108];};

static int logfd=-1;static u32 logged=0;
#define LOG_CAP 100000u
static int slen(const char*s){int n=0;while(s[n])n++;return n;}
static void log_s(const char*s){if(logfd<0||logged>LOG_CAP)return;int n=slen(s);s_write(logfd,s,(unsigned)n);logged+=(u32)n;}
static void fmt_dec(char*b,s32 v){char t[12];int i=11;t[i]=0;int neg=v<0;u32 u=neg?(u32)(-v):(u32)v;do{t[--i]=(char)('0'+u%10u);u/=10u;}while(u&&i>1);if(neg)t[--i]='-';int k=0;while(t[i])b[k++]=t[i++];b[k]=0;}
static void log_dec(s32 v){char b[13];fmt_dec(b,v);log_s(b);}

static const int ports[]={3400,3401,3700,3800,4000,4001,5600,5700,5800};
#define NPORT 9
#define MAXCONN 16
static void make_path(int port,char*out){const char*p="/var/run/PAC";int i=0;while(p[i]){out[i]=p[i];i++;}
 int d=10000;for(int k=0;k<5;k++){out[i++]=(char)('0'+(port/d)%10);d/=10;}out[i]=0;}
static int lfd[NPORT],cfd[MAXCONN],cport[MAXCONN];

static int listen_on(int port){
 struct sun0 a;a.family=AF_UNIX_;make_path(port,a.path);s_unlink(a.path);
 unsigned long sv[3]={AF_UNIX_,SOCK_SEQPACKET_,0};long fd=s_socketcall(SC_SOCKET,sv);if(fd<0)return -1;
 unsigned long bv[3]={(unsigned long)fd,(unsigned long)&a,(unsigned long)(2+slen(a.path)+1)};
 if(s_socketcall(SC_BIND,bv)<0){s_close((int)fd);return -1;}
 unsigned long lv[2]={(unsigned long)fd,4};s_socketcall(SC_LISTEN,lv);return (int)fd;}

static s32 rd32(const u8*b){return (s32)((u32)b[0]|((u32)b[1]<<8)|((u32)b[2]<<16)|((u32)b[3]<<24));}
static void write_temp(s32 v){
 char b[16];if(v<-60||v>70)fmt_dec(b,999);else fmt_dec(b,v);int n=slen(b);b[n++]='\n';b[n]=0;
 long fd=s_open(TEMP_TMP,1|0x40|0x200,0644);if(fd<0)return;s_write((int)fd,b,(unsigned)n);s_close((int)fd);s_rename(TEMP_TMP,TEMP_PATH);}

static void vehicle_status(const u8*b,long n){
 if(n<11||b[0]!=37||b[1]!=0||b[2]!=1)return;
 s32 id=rd32(b+3),val=rd32(b+7);
 log_s("t=");log_dec((s32)s_now_ms());log_s(" STATUS id=");log_dec(id);log_s(" value=");log_dec(val);log_s("\n");
 if(id==4)write_temp(val);}

static int run(void){
 logfd=(int)s_open(LOG_PATH,1|0x40|0x200,0644);
 log_s("--- vstatd 1 ---\n");
 for(int i=0;i<NPORT;i++){lfd[i]=listen_on(ports[i]);log_s("listen ");log_dec(ports[i]);log_s(lfd[i]>=0?" ok\n":" FAILED\n");}
 for(int i=0;i<MAXCONN;i++)cfd[i]=-1;
 static u8 buf[4096];
 for(;;){
  struct pollfd0 pf[NPORT+MAXCONN];int map[NPORT+MAXCONN];int n=0;
  for(int i=0;i<NPORT;i++)if(lfd[i]>=0){pf[n].fd=lfd[i];pf[n].events=POLLIN_;pf[n].revents=0;map[n]=i;n++;}
  for(int i=0;i<MAXCONN;i++)if(cfd[i]>=0){pf[n].fd=cfd[i];pf[n].events=POLLIN_;pf[n].revents=0;map[n]=100+i;n++;}
  if(s_poll(pf,(u32)n,5000)<=0)continue;
  for(int k=0;k<n;k++){
   if(!pf[k].revents)continue;
   if(map[k]<100){int i=map[k];unsigned long av[3]={(unsigned long)lfd[i],0,0};long c=s_socketcall(SC_ACCEPT,av);
    if(c>=0){int slot=-1;for(int j=0;j<MAXCONN;j++)if(cfd[j]<0){slot=j;break;}if(slot<0)s_close((int)c);else{cfd[slot]=(int)c;cport[slot]=ports[i];log_s("connected ");log_dec(ports[i]);log_s("\n");}}
   }else{int j=map[k]-100;long m=s_read(cfd[j],buf,sizeof(buf));
    if(m<=0){log_s("closed ");log_dec(cport[j]);log_s("\n");s_close(cfd[j]);cfd[j]=-1;continue;}
    if(cport[j]==3700)vehicle_status(buf,m);}
  }
 }
}
__attribute__((noreturn))void _start(void){s_exit(run());}
#include "arm_runtime.h"
