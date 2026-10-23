#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
int main(int argc,char**argv){assert(argc==2||argc==5);s32 lat=argc==5?atoi(argv[2]):4885660,lon=argc==5?atoi(argv[3]):235220;const char*zone=argc==5?argv[4]:"Europe/Paris";u8 h[40];clockfd=open(argv[1],O_RDONLY);assert(clockfd>=0);clock_size=lseek(clockfd,0,2);assert(clock_read(0,h,40));clock_polys=clock_u32(h+8);clock_zones=clock_u32(h+12);clock_po=clock_u32(h+16);clock_zo=clock_u32(h+20);clock_go=clock_u32(h+24);clock_to=clock_u32(h+28);clock_start=clock_u32(h+32);clock_end=clock_u32(h+36);
 int z=clock_resolve(lat,lon),off=0;assert(z>=0);gps.year=2026;gps.mon=1;gps.day=15;gps.hour=12;gps.min=gps.sec=0;assert(clock_gps_epoch()==1768478400u);assert(clock_transition(z,clock_gps_epoch(),&off)&&off==3600);assert(!strcmp(clock_name,zone));gps.mon=7;assert(clock_transition(z,clock_gps_epoch(),&off)&&off==7200);
 gps.mon=3;gps.day=29;gps.hour=0;gps.min=59;gps.sec=59;assert(clock_transition(z,clock_gps_epoch(),&off)&&off==3600);gps.hour=1;gps.min=gps.sec=0;assert(clock_transition(z,clock_gps_epoch(),&off)&&off==7200);gps.mon=10;gps.day=25;gps.hour=0;gps.min=59;gps.sec=59;assert(clock_transition(z,clock_gps_epoch(),&off)&&off==7200);gps.hour=1;gps.min=gps.sec=0;assert(clock_transition(z,clock_gps_epoch(),&off)&&off==3600);
 gps.year=2040;gps.mon=7;gps.day=1;assert(clock_gps_epoch()>0x7fffffffu);assert(clock_transition(z,clock_gps_epoch(),&off)&&off==7200);assert(clock_resolve(0,0)<0);assert(!clock_transition(z,0,&off));
 gps.year=2026;gps.mon=7;gps.day=15;gps.hour=12;gps.min=gps.sec=0;gps.fix=1;gps.lat=lat;gps.lon=lon;pref.clock_auto=1;pref.hoff=pref.moff=0;clock_next_ms=0;clock_service();assert(clock_valid&&pref.hoff==2&&pref.moff==0);pref.clock_auto=0;pref.hoff=-5;pref.moff=0;clock_next_ms=0;clock_service();assert(pref.hoff==-5);screen=SCR_SET;set_focus=5;key_handle(K_ENTER);assert(pref.clock_auto&&pref.hoff==2);key_handle(K_LEFT);assert(!pref.clock_auto&&pref.hoff==1&&pref.moff==45);
 printf("Native offline clock: %s winter/summer, exact DST transition seconds, post-2038 UTC, auto/manual controls and unknown location/range checks passed.\n",zone);return 0;}
