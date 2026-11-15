/* One boot-only buffer, released after the elapsed-time transition or input.
 * No sleep, map suspension, extra render thread or permanent framebuffer. */
#include "boot_handoff.h"
enum {BOOT_FADE_MS=500};
static struct {u16*image;u32 at;int started;} boot_fade;
static void boot_fade_cancel(void){
 if(boot_fade.image)s_munmap(boot_fade.image,FB_PIX*2u);
 boot_fade.image=0;boot_fade.started=0;
}
static int boot_image_read(int fd,void*p,unsigned bytes){
 u8*b=p;while(bytes){long n=s_read(fd,b,bytes);if(n<=0)return 0;b+=n;bytes-=(unsigned)n;}return 1;
}
static void boot_fade_load(void){
 boot_fade_cancel();long fd=s_open(BOOT_IMAGE_PATH,0,0);s_unlink(BOOT_IMAGE_PATH);if(fd<0)return;
 struct BootImageHeader h;u8 extra;
 if(!boot_image_read((int)fd,&h,sizeof(h))||h.magic!=0x31425043u||h.pixels!=FB_PIX||s_now_ms()-h.stamp>120000u){s_close((int)fd);return;}
 boot_fade.image=s_mmap2(0,FB_PIX*2u,3,34,-1,0);
 int ok=boot_fade.image&&boot_image_read((int)fd,boot_fade.image,FB_PIX*2u)&&
  s_read((int)fd,&extra,1)==0&&boot_image_hash(boot_fade.image)==h.hash;
 s_close((int)fd);if(!ok)boot_fade_cancel();
}
static void boot_fade_apply(u32 now){
 if(!boot_fade.image)return;
 if(!boot_fade.started){boot_fade.started=1;boot_fade.at=now;}
 u32 elapsed=now-boot_fade.at;if(elapsed>=BOOT_FADE_MS){boot_fade_cancel();return;}
 unsigned q=elapsed*256u/BOOT_FADE_MS;
 q=(q*q*(768u-2u*q)+32768u)>>16;unsigned inv=256u-q;
 for(unsigned i=0;i<FB_PIX;i++){
  u16 a=boot_fade.image[i],b=fb[i];
  unsigned r=(((a>>11)*inv+(b>>11)*q+128u)>>8);
  unsigned g=((((a>>5)&63u)*inv+((b>>5)&63u)*q+128u)>>8);
  unsigned blue=(((a&31u)*inv+(b&31u)*q+128u)>>8);
  fb[i]=(u16)((r<<11)|(g<<5)|blue);
 }
}
