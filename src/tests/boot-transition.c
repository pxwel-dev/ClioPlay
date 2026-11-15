/* Production splash handoff, live compositor cadence and one-shot lifetime. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include "../clio843/src/boot_art.h"
#include <assert.h>
#include <string.h>
static u16 original[FB_PIX];
static void save_splash(int night){spl_render(night);memcpy(original,fb,sizeof(original));boot_image_save(fb);}
static void target(void){fill(0,0,W,H,rgb(90,160,100));}
int main(void){
 s_unlink(BOOT_IMAGE_PATH); // Other CI fixtures share the isolated host root.
 boot_fade_load();assert(!boot_fade.image);
 for(int night=0;night<2;night++){
  save_splash(night);boot_fade_load();assert(boot_fade.image&&!boot_fade.started);
  assert(s_open(BOOT_IMAGE_PATH,0,0)<0); // Consumed once, before slow UI loaders.
  target();boot_fade_apply(0xfffffff0u);assert(!memcmp(fb,original,sizeof(original)));
  unsigned previous=0;
  for(u32 elapsed=67;elapsed<500;elapsed+=67){
   target();boot_fade_apply(0xfffffff0u+elapsed);unsigned changed=0;
   for(unsigned i=0;i<FB_PIX;i++)changed+=fb[i]!=original[i];
   assert(changed>=previous);previous=changed;
  }
  target();boot_fade_apply(0xfffffff0u+500);assert(!boot_fade.image);
  for(unsigned i=0;i<FB_PIX;i++)assert(fb[i]==rgb(90,160,100));
 }
 save_splash(0);FILE*f=fopen(BOOT_IMAGE_PATH,"ab");assert(f);fputc(0,f);fclose(f);
 boot_fade_load();assert(!boot_fade.image);
 save_splash(1);f=fopen(BOOT_IMAGE_PATH,"r+b");assert(f);struct BootImageHeader h;
 assert(fread(&h,sizeof(h),1,f)==1);h.stamp=s_now_ms()-120001u;rewind(f);assert(fwrite(&h,sizeof(h),1,f)==1);fclose(f);
 boot_fade_load();assert(!boot_fade.image);
 save_splash(1);f=fopen(BOOT_IMAGE_PATH,"r+b");assert(f);assert(fseek(f,sizeof(h)+100,SEEK_SET)==0);int byte=fgetc(f);assert(byte!=EOF);assert(fseek(f,-1,SEEK_CUR)==0);fputc(byte^1,f);fclose(f);
 boot_fade_load();assert(!boot_fade.image);
 f=fopen(BOOT_IMAGE_PATH,"wb");assert(f);fputs("truncated",f);fclose(f);boot_fade_load();assert(!boot_fade.image);
 save_splash(0);boot_fade_load();key_handle(K_MENU);assert(!boot_fade.image&&screen==SCR_MENU);
 /* Nothing waits for the fade: normal map/model updates keep their cadence. */
 for(int calc=0;calc<2;calc++){
  save_splash(calc);boot_fade_load();hud_capture_clock=1;hud_capture_ms=10000;
  screen=SCR_MAP;modal=MOD_NONE;mapok=gps.fix=track_valid=trip.active=0;
  route_status=calc?R_CALC:R_IDLE;route_n=0;freecam=bt_control_mode=0;pref.debug=0;
  hud_reset_animations();apply_palette(calc);render_runtime_reset(10000);
  for(u32 t=10000;t<11000;t++){hud_capture_ms=t;render_runtime_step(t,t%100u==0);}
  assert(!boot_fade.image&&render_stats.maps==(calc?5u:10u));
  assert(render_stats.presents<=(calc?5u:15u)&&render_stats.presents>=(calc?5u:10u));
 }
 puts("Boot transition: exact splash/UI endpoints, monotonic blend, clock wrap, corrupt/stale/truncated handoffs, one-shot cleanup, immediate input, live map updates and 15/5 Hz cadence passed.");
}
