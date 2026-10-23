/* Shared by the boot splash and first UI frame. GPS refines AUTO afterwards. */
#ifndef CLIO_BOOT_THEME_H
#define CLIO_BOOT_THEME_H
static int clio_boot_minutes(long utc_seconds,int hour_offset,int minute_offset){
 if(utc_seconds<1577836800L)return -1;
 int minutes=(int)((utc_seconds%86400L)/60L)+hour_offset*60+minute_offset;
 minutes%=1440;if(minutes<0)minutes+=1440;return minutes;
}
static int clio_boot_night(int mode,int local_minutes,int saved){
 if(mode==0)return 0;if(mode==1)return 1;
 if(local_minutes>=0&&local_minutes<1440)return local_minutes<420||local_minutes>=1140;
 return saved==0?0:1;
}
#endif
