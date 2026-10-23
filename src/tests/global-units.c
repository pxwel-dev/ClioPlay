#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static void country(int lat,int lon,const char*iso,int imperial){gps.fix=1;gps.lat=lat;gps.lon=lon;region_resolve(lat,lon);assert(!strcmp(region_iso,iso));pref.units=0;roadctx.valid=0;assert(display_imperial()==imperial);}
int main(void){assert(load_regions());mapfmt=5;
 country(4885660,235220,"FR",0);assert(!(region_flags&2));roadctx.valid=1;roadctx.speed=30;roadctx.speed_flags=2;assert(!display_imperial());assert(display_limit(30,2)==48);roadctx.valid=0;country(4041680,-370380,"ES",0);assert(region_flags&2);
 country(5497000,-161000,"GB",1);country(3777000,-12242000,"US",1);country(-3386000,15120000,"AU",0);country(631000,-1080000,"LR",1);
 country(0,0,"",0);country(4885660,235220,"FR",0);pref.units=2;assert(display_imperial());assert(display_limit(130,0)==81);pref.units=1;assert(display_limit(70,2)==113);assert(display_limit(130,0)==130);
 gps.mph10=621;assert(display_speed()==100);pref.units=2;assert(display_speed()==62);
 char b[80]="";pref.units=1;append_nav_distance(b,sizeof(b),180);assert(!strcmp(b,"180 M"));b[0]=0;append_miles(b,sizeof(b),1300);assert(!strcmp(b,"1.3 KM"));
 struct RoadCtx r={0};r.valid=1;r.speed=130;r.speed_flags=0;b[0]=0;append_road_limit(b,sizeof(b),&r);assert(!strcmp(b,"130"));r.speed_flags=8;b[0]=0;append_road_limit(b,sizeof(b),&r);assert(!strcmp(b,"VAR"));r.speed_flags=4;b[0]=0;append_road_limit(b,sizeof(b),&r);assert(!strcmp(b,"INF"));
 assert(coslat_q15(0)==32767&&coslat_q15(6000000)==16384&&coslat_q15(-6000000)==16384&&coslat_q15(8500000)==2856);assert(lon_delta(35999900)==-100);assert(square_distance(10000000,10000000)==0xffffffffu);
 assert(seg_dist2(-10000,0,10000,0)<100);assert(seg_dist2(10000000,10000000,10000050,10000050)>100000000);
 gps.year=2026;gps.mon=12;gps.day=21;gps.lat=-3386000;gps.lon=15120000;gps.hour=0;gps.min=0;assert(!auto_is_night());gps.hour=12;assert(auto_is_night());gps.lat=8000000;gps.lon=1500000;gps.mon=6;assert(!auto_is_night());gps.mon=12;assert(auto_is_night());
 pref.units=2;pref.cameras=0;pref.hoff=5;pref.moff=45;save_prefs();pref.units=0;pref.cameras=1;pref.hoff=pref.moff=0;load_prefs();assert(pref.units==2&&!pref.cameras&&pref.hoff==5&&pref.moff==45);
 screen=SCR_SET;set_focus=2;key_handle(K_RIGHT);assert(pref.units==0);key_handle(K_RIGHT);assert(pref.units==1);key_handle(K_LEFT);assert(pref.units==0);set_focus=5;key_handle(K_RIGHT);assert(pref.hoff*60+pref.moff==360);set_focus=0;for(int i=0;i<10;i++)key_handle(K_DOWN);assert(set_focus==7);
 puts("Global units: France/Spain/UK/US/Australia/Liberia/offshore, overrides, exact limits, distances, both-hemisphere/polar themes, settings/persistence and remote control passed.");return 0;}
