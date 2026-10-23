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
static void run_case(int mode,int minute,const char*saved,int night){
 char pref[128];snprintf(pref,sizeof(pref),"theme=%d\nbright=50\nhoff=1\nmoff=0\n",mode);put(SPLASH_PREF,pref);put(SPLASH_THEME,saved);put(SPLASH_BL_MAX,"255");put(SPLASH_BL_VAL,"");put(SPLASH_FB,"");
 spl_theme=2;spl_bright=50;spl_hoff=1;spl_moff=0;spl_test_minutes=minute;
 assert(spl_run()==0);unsigned actual=check();spl_render(night);assert(actual==check());
 FILE*f=fopen(SPLASH_THEME,"rb");assert(f);char b[32];size_t n=fread(b,1,sizeof(b),f);fclose(f);assert(n==strlen(saved)&&!memcmp(b,saved,n));
 f=fopen(SPLASH_BL_VAL,"rb");assert(f);n=fread(b,1,sizeof(b),f);fclose(f);assert(n==3&&!memcmp(b,"128",3));
 f=fopen(SPLASH_FB,"rb");assert(f);assert(fseek(f,0,SEEK_END)==0&&ftell(f)==sizeof(fb));fclose(f);
}
int main(int argc,char**argv){
 assert(argc==2);
 assert(clio_boot_minutes(0,1,0)==-1);
 assert(clio_boot_minutes(1577836800L,1,0)==60);
 assert(clio_boot_minutes(1577836800L,0,-30)==1410);
 assert(clio_boot_minutes(1577836800L+23*3600L,1,30)==30);
 icon_checks(0);icon_checks(1);
 run_case(0,0,"1",0);snapshot(argv[1],"splash-day");
 run_case(1,720,"0",1);snapshot(argv[1],"splash-night");
 run_case(2,419,"0",1);run_case(2,420,"1",0);run_case(2,1139,"1",0);run_case(2,1140,"0",1);
 run_case(2,-1,"0",0);run_case(2,-1,"1",1);run_case(2,-1,"invalid",1);
 run_case(8,720,"1",0);
 puts("Native splash passed: antialiased symmetric icon contours, no out-of-icon writes, Day/Night, AUTO boundaries, UTC offsets, saved fallback, brightness, exact framebuffer write, and no theme-state mutation.");
 return 0;
}
