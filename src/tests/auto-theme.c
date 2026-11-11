/* Daily schedule boundaries, local clock selection and saved preferences. */
#define HOST_TEST 1
#define CLIO_THEME_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>

static void preferences(const char*text){
 FILE*f=fopen(PREF_PATH,"wb");assert(f);
 assert(fwrite(text,1,strlen(text),f)==strlen(text)&&fclose(f)==0);
}
int main(void){
 for(int minute=0;minute<1440;minute++){
  assert(clio_schedule_night(minute,420,1080)==(minute<420||minute>=1080));
  assert(clio_schedule_night(minute,1200,300)==(minute>=300&&minute<1200));
 }
 assert(clio_schedule_night(556,557,1062)==1);
 assert(clio_schedule_night(557,557,1062)==0);
 assert(clio_schedule_night(1061,557,1062)==0);
 assert(clio_schedule_night(1062,557,1062)==1);
 assert(clio_schedule_night(0,0,1)==0&&clio_schedule_night(1,0,1)==1);
 assert(clio_schedule_night(-1,420,1080)==-1);
 assert(clio_schedule_night(0,420,420)==-1);
 assert(clio_theme_time("1439",0)==1439&&clio_theme_time("0",99)==0);
 const char*bad[]={"","-1","1440","99999999999999","08:30","abc","420x"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++)assert(clio_theme_time(bad[i],420)==420);
 assert(clio_day_of_year(2028,2,29)==59&&clio_day_of_year(2026,2,29)==-1);

 pref.theme=THEME_AUTO;pref.day_start=557;pref.night_start=1062;pref.hoff=1;pref.moff=0;
 gps.fix=lastfix_valid=0;gps.year=gps.mon=gps.day=0;
 gps.hour=8;gps.min=16;assert(auto_is_night());
 gps.min=17;assert(!auto_is_night());
 gps.hour=16;gps.min=41;assert(!auto_is_night());
 gps.min=42;assert(auto_is_night());
 // No position or GPS date is needed, including a time-only RMC.
 char rmc[]="$GPRMC,081700,V,,,,,,,061126,,,N";parse_nmea(rmc);
 assert(!gps.fix&&!auto_is_night());
 gps.hour=-1;theme_test_utc=1793923200L+8*3600+17*60;
 write_theme_state_raw(1);seed_boot_theme();assert(!theme_resolved&&!auto_is_night());
 theme_test_utc=1793923200L+16*3600+42*60;refresh_theme();assert(theme_resolved);
 pref.hoff=-5;pref.moff=-30;gps.hour=0;gps.min=0;assert(auto_is_night());
 pref.theme=THEME_DAY;refresh_theme();assert(!theme_resolved);
 pref.theme=THEME_NIGHT;refresh_theme();assert(theme_resolved);
 pref.theme=THEME_AUTO;gps.hour=-1;theme_test_utc=-1;
 theme_resolved=0;assert(!auto_is_night());theme_resolved=1;assert(auto_is_night());
 assert(clio_boot_night(THEME_AUTO,-1,0,420,1080,0)==0);
 assert(clio_boot_night(THEME_AUTO,-1,0,420,1080,-1)==1);

 preferences("version=6\ntheme=2\nbright=60\n");
 load_prefs();assert(pref.day_start==420&&pref.night_start==1080&&pref.bright==60);
 pref.day_start=557;pref.night_start=1062;save_prefs();
 pref.day_start=pref.night_start=0;load_prefs();
 assert(pref.day_start==557&&pref.night_start==1062);
 preferences("daystart=420\nnightstart=420\n");load_prefs();
 assert(pref.day_start==420&&pref.night_start==1080);
 preferences("daystart=bad\nnightstart=99999999999999\n");load_prefs();
 assert(pref.day_start==420&&pref.night_start==1080);

 pref.theme=THEME_AUTO;pref.hoff=pref.moff=0;gps.hour=6;gps.min=30;
 screen=SCR_SET;set_focus=8;theme_time_part=0;
 key_handle(K_LEFT);assert(pref.day_start==360&&!auto_is_night());
 key_handle(K_ENTER);key_handle(K_RIGHT);assert(theme_time_part==1&&pref.day_start==361);
 key_handle(K_DOWN);assert(set_focus==9&&theme_time_part==0);
 key_handle(K_RIGHT);assert(pref.night_start==1140);
 key_handle(K_ENTER);key_handle(K_LEFT);assert(pref.night_start==1139);
 save_prefs();load_prefs();assert(pref.day_start==361&&pref.night_start==1139);
 // Draw both scrolled schedule rows through the normal page entry point.
 draw_settings();
 puts("AUTO schedule: every minute, midnight wrap, exact custom boundaries, local offsets, GPS-free boot/time-only input, legacy/corrupt preferences, persistence, schedule controls and manual overrides passed.");
 return 0;
}
