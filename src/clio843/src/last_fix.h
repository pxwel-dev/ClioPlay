/* Existing LGP1 saved-position format, shared by the UI and boot splash. */
#ifndef CLIO_LAST_FIX_H
#define CLIO_LAST_FIX_H
struct LastFixDisk {char magic[4];s32 lat,lon;u16 course,pad;u32 check;};
static u32 lastfix_check(s32 lat,s32 lon,u16 course){return 0x43504731u^(u32)lat^((u32)lon*2654435761u)^((u32)course<<16);}
static int lastfix_read(const char*path,struct LastFixDisk*d){
 long fd=s_open(path,0,0);if(fd<0)return 0;
 long n=s_read((int)fd,d,sizeof(*d));s_close((int)fd);
 if(n!=(long)sizeof(*d)||d->magic[0]!='L'||d->magic[1]!='G'||d->magic[2]!='P'||d->magic[3]!='1')return 0;
 if(d->lat<-9000000||d->lat>9000000||d->lon<-18000000||d->lon>18000000||d->course>359||(!d->lat&&!d->lon))return 0;
 return d->check==lastfix_check(d->lat,d->lon,d->course);
}
#endif
