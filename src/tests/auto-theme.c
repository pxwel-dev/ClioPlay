/* Actual UI resolver, saved-position I/O and independent solar equations. */
#define HOST_TEST 1
#define CLIO_THEME_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <math.h>
#include <string.h>

static void gps_date(int year,int month,int day,int hour,int minute){gps.year=year;gps.mon=month;gps.day=day;gps.hour=hour;gps.min=minute;}
static void solar_reference(void){
 // Independently evaluate NOAA's published floating-point equations on the
 // host. Ignore ten minutes around the true boundary: device tables use
 // coarse latitude/date interpolation and are not an ephemeris service.
 const double pi=3.14159265358979323846;
 unsigned checked=0;const int lats[]={-60,-35,0,35,55,60};
 for(int year=2026;year<=2028;year+=2)for(int mon=1;mon<=12;mon++)for(unsigned k=0;k<sizeof(lats)/sizeof(lats[0]);k++){
  int d=clio_day_of_year(year,mon,6);double g=2*pi*d/(clio_leap_year(year)?366:365);
  double dec=.006918-.399912*cos(g)+.070257*sin(g)-.006758*cos(2*g)+.000907*sin(2*g)-.002697*cos(3*g)+.00148*sin(3*g);
  double eot=229.18*(.000075+.001868*cos(g)-.032077*sin(g)-.014615*cos(2*g)-.040849*sin(2*g));
  double lat=lats[k]*pi/180,half=acos((cos(90.833*pi/180)-sin(lat)*sin(dec))/(cos(lat)*cos(dec)))*720/pi;
  for(int longitude=-150;longitude<=150;longitude+=50)for(int minute=0;minute<1440;minute+=11){
   double solar=fmod(minute+longitude*4+eot+2880,1440);
   if(fabs(solar-(720-half))<10||fabs(solar-(720+half-15))<10)continue;
   assert(clio_solar_night(year,mon,6,minute,lats[k]*100000,longitude*100000)==(solar<720-half||solar>=720+half-15));checked++;
  }
 }
 printf("Solar reference: %u independent date/latitude/longitude/time cases passed.\n",checked);
}
int main(void){
 solar_reference();
 assert(clio_day_of_year(2028,2,29)==59&&clio_day_of_year(2026,2,29)==-1);
 assert(clio_day_of_year(2026,4,31)==-1&&clio_day_of_year(2026,0,1)==-1);
 assert(clio_solar_night(2026,11,6,-1,5497548,-164050)==-1);
 assert(clio_solar_night(2026,6,21,0,8000000,1500000)==0);
 assert(clio_solar_night(2026,12,21,720,8000000,1500000)==1);
 lastfix_valid=1;lastfix_lat=5497548;lastfix_lon=-164050;lastfix_course=70;assert(save_last_fix()==1);
 lastfix_valid=0;assert(load_last_fix()&&lastfix_lat==5497548&&lastfix_lon==-164050);
 pref.theme=THEME_AUTO;pref.hoff=14;pref.moff=0;theme_resolved=0;write_theme_state_raw(0);
 theme_test_utc=1793923200L+17*3600;memzero(&gps,sizeof(gps));gps.hour=-1;
 seed_boot_theme();assert(theme_resolved==1);refresh_theme();assert(theme_resolved==1);
 // Time/date-only RMC has no position: use the saved coordinates, not 0,0.
 char rmc[]="$GPRMC,170000,V,,,,,,,061126,,,N";parse_nmea(rmc);
 assert(!gps.fix&&gps.lat==0&&gps.lon==0&&gps.year==2026);assert(auto_is_night());
 gps.hour=15;assert(!auto_is_night());gps.hour=7;assert(auto_is_night());gps.hour=8;assert(!auto_is_night());
 gps.hour=16;gps.min=0;assert(!auto_is_night());gps.min=15;assert(auto_is_night());
 // Live position wins over the saved location; live UTC wins over the RTC.
 gps.fix=1;gps.lat=-3386000;gps.lon=15120000;gps_date(2026,12,21,0,0);assert(!auto_is_night());
 gps.lat=5497548;gps.lon=-164050;gps_date(2026,6,21,20,0);assert(!auto_is_night());gps.hour=22;assert(auto_is_night());
 // No GPS date yet: the same seasonal decision still runs from the RTC.
 gps.fix=0;gps_date(0,0,0,-1,0);theme_test_utc=1782000000L+20*3600;assert(!auto_is_night());
 theme_test_utc=1793923200L+17*3600;assert(auto_is_night());
 pref.theme=THEME_DAY;refresh_theme();assert(!theme_resolved);pref.theme=THEME_NIGHT;refresh_theme();assert(theme_resolved);
 pref.theme=THEME_AUTO;theme_test_utc=-1;theme_resolved=0;assert(!auto_is_night());theme_resolved=1;assert(auto_is_night());
 lastfix_valid=0;theme_test_utc=1793923200L+12*3600;seed_boot_theme();assert(theme_resolved==1);
 assert(clio_boot_night(THEME_AUTO,-1,1,5497548,-164050,-1)==1);
 puts("AUTO theme: November/summer, both hemispheres/polar dates, leap validation, GPS-free boot, time-only RMC, saved/live location, UTC/DST independence, missing inputs and manual overrides passed.");
 return 0;
}
