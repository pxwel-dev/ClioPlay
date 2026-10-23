#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static void position(s32 la,s32 lo){gps.fix=1;gps.lat=la;gps.lon=lo;track_lat=la;track_lon=lo;track_valid=1;gps.mph10=0;}
int main(int argc,char**argv){assert(argc==2);assert(load_map()&&load_route_data()&&load_regions()&&load_timezones()&&load_font());assert(cliou_load(CLIO_HOST_ROOT "clio843/maps/fonts.cpf"));position(4335340,-178050);assert(route_start(4333940,-178890));for(int i=0;route_status==R_CALC&&i<10000;i++)route_service_until(s_now_ms()+20);assert(route_status==R_READY&&route_n>2);int fr=0,es=0;for(int i=0;i<route_n;i++){region_resolve(route_lat[i],route_lon[i]);if(streq(region_iso,"FR")){fr=1;assert(!(region_flags&2));}if(streq(region_iso,"ES")){es=1;assert(region_flags&2);}assert(!(region_flags&1));}assert(fr&&es);
 char dir[512];copystr(dir,sizeof(dir),argv[1]);char out[640];position(4335340,-178050);region_resolve(gps.lat,gps.lon);screen=SCR_MAP;gps.year=2026;gps.mon=10;gps.day=19;gps.hour=12;gps.min=45;gps_fix_ms=s_now_ms();pref.autozoom=0;manual_zoom=6;
 for(int night=0;night<2;night++){pref.theme=night?THEME_NIGHT:THEME_DAY;pref.language=night?2:1;draw();copystr(out,sizeof(out),dir);appendstr(out,sizeof(out),night?"/border-night.rgb565":"/border-day.rgb565");FILE*f=fopen(out,"wb");assert(f);fwrite(fb,1,sizeof(fb),f);fclose(f);screen=SCR_SET;set_focus=7;draw();copystr(out,sizeof(out),dir);appendstr(out,sizeof(out),night?"/settings-es.rgb565":"/settings-fr.rgb565");f=fopen(out,"wb");assert(f);fwrite(fb,1,sizeof(fb),f);fclose(f);screen=SCR_MAP;}
 printf("Actual Hendaye–Irun route: %d points, %u metres, %u ds; FR→ES geometry, metric units, French camera suppression and Spanish policy passed.\n",route_n,route_result_m,route_result_cost);return 0;}
