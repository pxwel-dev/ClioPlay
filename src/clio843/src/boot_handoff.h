/* A one-shot RAM-filesystem image, never an SD-card log or preference. */
#ifndef CLIO_BOOT_HANDOFF_H
#define CLIO_BOOT_HANDOFF_H
#ifdef HOST_TEST
#define BOOT_IMAGE_PATH CLIO_HOST_ROOT "boot-image"
#define BOOT_IMAGE_TEMP CLIO_HOST_ROOT "boot-image.new"
#else
#define BOOT_IMAGE_PATH "/tmp/clioplay-boot-image"
#define BOOT_IMAGE_TEMP "/tmp/clioplay-boot-image.new"
#endif
struct BootImageHeader {u32 magic,stamp,pixels,hash;};
static u32 boot_image_hash(const u16*p){u32 h=2166136261u;for(unsigned i=0;i<FB_PIX;i++)h=(h^p[i])*16777619u;return h;}
static int boot_image_write(int fd,const void*p,unsigned bytes){
 const u8*b=p;while(bytes){long n=s_write(fd,b,bytes);if(n<=0)return 0;b+=n;bytes-=(unsigned)n;}return 1;
}
static void boot_image_save(const u16*p){
 struct BootImageHeader h={0x31425043u,s_now_ms(),FB_PIX,boot_image_hash(p)};
 long fd=s_open(BOOT_IMAGE_TEMP,1|64|512,0600);if(fd<0)return;
 int ok=boot_image_write((int)fd,&h,sizeof(h))&&boot_image_write((int)fd,p,FB_PIX*2u);s_close((int)fd);
 if(!ok||s_rename(BOOT_IMAGE_TEMP,BOOT_IMAGE_PATH)<0)s_unlink(BOOT_IMAGE_TEMP);
}
#endif
