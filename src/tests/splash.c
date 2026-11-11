/* Exercise the maintained splash, shared boot resolver and real file I/O. */
#define HOST_TEST 1
#define CLIO_SPLASH_TEST 1
#define main unused_splash_entry
#include "../clio843/src/cliospl.c"
#undef main
#include <assert.h>
#include <string.h>

static void put(const char*path,const char*text){FILE*f=fopen(path,"wb");assert(f);assert(fwrite(text,1,strlen(text),f)==strlen(text));fclose(f);}
static void snapshot(const char*out,const char*name){char p[1024];snprintf(p,sizeof(p),"%s/%s.rgb565",out,name);FILE*f=fopen(p,"wb");assert(f);assert(fwrite(fb,1,sizeof(fb),f)==sizeof(fb));fclose(f);}
static unsigned check(void){unsigned n=0;for(int i=0;i<FB_PIX;i++)n=n*33u+fb[i];return n;}
static void icon_checks(int night){
 u16 background=ui_canvas(night);fill(0,0,W,H,background);spl_icon(204,70);int soft_outer=0,soft_arrow=0;
 for(int y=0;y<H;y++)for(int x=0;x<W;x++){
  u16 c=fb[y*W+x];
  if(x<204||x>=276||y<70||y>=142){assert(c==background);continue;}
  int xx=x-204,yy=y-70,i=yy*SPL_ICON_SIZE+xx;
  assert(c==fb[y*W+204+SPL_ICON_SIZE-1-xx]); // Symmetric tile and mark.
  if(spl_icon_outer[i]&&spl_icon_outer[i]<64&&c!=background)soft_outer++;
  if(spl_icon_arrow[i]&&spl_icon_arrow[i]<64){
   assert(spl_icon_outer[i]==64);u16 blue=0;int found=0;
   for(int bx=0;bx<SPL_ICON_SIZE;bx++){int bi=yy*SPL_ICON_SIZE+bx;if(spl_icon_outer[bi]==64&&!spl_icon_arrow[bi]){blue=fb[y*W+204+bx];found=1;break;}}
   assert(found);assert(((c>>11)&31)>=((blue>>11)&31));assert(((c>>5)&63)>=((blue>>5)&63));assert((c&31)>=(blue&31));soft_arrow++;
  }
 }
 assert(soft_outer>40&&soft_arrow>40); // Both independent contours have coverage.
}
static int test_day=CLIO_DAY_START,test_night=CLIO_NIGHT_START;
static void run_case(int mode,long utc,const char*saved,int night,int valid_fix){
 char pref[160];snprintf(pref,sizeof(pref),"theme=%d\nbright=50\nhoff=1\nmoff=0\ndaystart=%d\nnightstart=%d\n",mode,test_day,test_night);put(SPLASH_PREF,pref);put(SPLASH_THEME,saved);put(SPLASH_BL_MAX,"255");put(SPLASH_BL_VAL,"");put(SPLASH_FB,"");
 struct LastFixDisk fix={{'L','G','P','1'},5497548,-164050,0,0,0};fix.check=lastfix_check(fix.lat,fix.lon,fix.course);
 if(valid_fix){if(valid_fix<0)fix.check++;FILE*f=fopen(SPLASH_LASTGPS,"wb");assert(f&&fwrite(&fix,sizeof(fix),1,f)==1);fclose(f);}else unlink(SPLASH_LASTGPS);
 spl_theme=2;spl_bright=50;spl_test_utc=utc;
 assert(spl_run()==0);unsigned actual=check();spl_render(night);assert(actual==check());
 FILE*f=fopen(SPLASH_THEME,"rb");assert(f);char b[32];size_t n=fread(b,1,sizeof(b),f);fclose(f);assert(n==strlen(saved)&&!memcmp(b,saved,n));
 f=fopen(SPLASH_BL_VAL,"rb");assert(f);n=fread(b,1,sizeof(b),f);fclose(f);assert(n==3&&!memcmp(b,"128",3));
 f=fopen(SPLASH_FB,"rb");assert(f);assert(fseek(f,0,SEEK_END)==0&&ftell(f)==sizeof(fb));fclose(f);
}
int main(int argc,char**argv){
 assert(argc==2);
 icon_checks(0);icon_checks(1);
 run_case(0,1793984400L,"1",0,1);snapshot(argv[1],"splash-day");
 run_case(1,1793966400L,"0",1,1);snapshot(argv[1],"splash-night");
 // Daily times are local, using the saved UTC +01:00 offset at boot.
 run_case(2,1793923200L+5*3600+59*60,"0",1,1);
 run_case(2,1793923200L+6*3600,"1",0,1);
 run_case(2,1793923200L+16*3600+59*60,"1",0,1);
 run_case(2,1793923200L+17*3600,"0",1,1);
 test_day=557;test_night=1062;
 run_case(2,1793923200L+8*3600+16*60,"0",1,0);
 run_case(2,1793923200L+8*3600+17*60,"1",0,0);
 run_case(2,1793923200L+16*3600+41*60,"1",0,-1);
 run_case(2,1793923200L+16*3600+42*60,"0",1,-1);
 test_day=CLIO_DAY_START;test_night=CLIO_NIGHT_START;
 run_case(2,-1,"0",0,1);run_case(2,-1,"1",1,1);run_case(2,-1,"invalid",1,1);
 run_case(2,1793966400L,"1",0,0);run_case(2,1793966400L,"1",0,-1);
 run_case(8,1793966400L,"1",0,1);
 put(SPLASH_PREF,"version=6\ntheme=2\nbright=50\nhoff=1\nmoff=0\n");spl_load_prefs();
 assert(spl_day_start==CLIO_DAY_START&&spl_night_start==CLIO_NIGHT_START&&spl_offset==60);
 puts("Native splash passed: antialiased icon, custom AUTO schedule and saved UTC offset, GPS-independent startup, Day/Night overrides, brightness, exact framebuffer write and unchanged saved theme.");
 return 0;
}
