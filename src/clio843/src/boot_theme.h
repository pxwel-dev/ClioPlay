/* Shared offline seasonal AUTO resolver for splash, boot and live guidance.
 * Existing NOAA-derived tables; integer interpolation, no runtime trig. */
#ifndef CLIO_BOOT_THEME_H
#define CLIO_BOOT_THEME_H
#include "global_solar.h"
static int clio_leap_year(int y){return(y%4)==0&&((y%100)!=0||(y%400)==0);}
static int clio_month_days(int y,int m){static const u8 days[12]={31,28,31,30,31,30,31,31,30,31,30,31};return m<1||m>12?0:days[m-1]+(m==2&&clio_leap_year(y));}
static int clio_day_of_year(int y,int m,int day){
 if(y<2020||y>2099||day<1||day>clio_month_days(y,m))return-1;
 int d=day-1;for(int i=1;i<m;i++)d+=clio_month_days(y,i);return d;
}
static int clio_solar_night(int year,int month,int day,int utc_minutes,s32 lat,s32 lon){
 int d=clio_day_of_year(year,month,day);
 if(d<0||utc_minutes<0||utc_minutes>=1440||lat<-9000000||lat>9000000||lon<-18000000||lon>18000000)return-1;
 /* Normalize leap years to the existing 365-day table, keeping fractional
  * days until interpolation. Latitude rows run -85..85 at five degrees. */
 int days=clio_leap_year(year)?366:365,phase=d*365;
 int i=phase/(days*15),frac=phase-i*days*15,span=(i==24?5:15)*days;
 int latitude=lat+8500000;if(latitude<0)latitude=0;if(latitude>=17000000)latitude=16999999;
 int row=latitude/500000,f=latitude%500000;
 int a=GLOBAL_SUN_HALF[row][i],b=GLOBAL_SUN_HALF[row+1][i];
 a+=((int)GLOBAL_SUN_HALF[row][i+1]-a)*frac/span;
 b+=((int)GLOBAL_SUN_HALF[row+1][i+1]-b)*frac/span;
 int half=a+(b-a)*f/500000;
 if(half<=0)return 1;if(half>=720)return 0;
 int eot=GLOBAL_EOT[i]+(GLOBAL_EOT[i+1]-GLOBAL_EOT[i])*frac/span;
 int now=(utc_minutes+lon/25000+eot+2880)%1440;
 /* Reduce dusk glare before the calculated sunset, rather than leaving
  * a bright map on until the last sliver of sun disappears. */
 return now<720-half||now>=720+half-15;
}
static int clio_solar_from_utc(long utc,s32 lat,s32 lon){
 if(utc<1577836800L)return-1;
 long days=(utc-1577836800L)/86400L;int year=2020,month=1;
 while(year<=2099&&days>=365+clio_leap_year(year)){days-=365+clio_leap_year(year);year++;}
 if(year>2099)return-1;
 while(days>=clio_month_days(year,month)){days-=clio_month_days(year,month);month++;}
 return clio_solar_night(year,month,(int)days+1,(int)((utc%86400L)/60L),lat,lon);
}
static int clio_boot_night(int mode,long utc,int position_valid,s32 lat,s32 lon,int saved){
 if(mode==0)return 0;if(mode==1)return 1;
 if(position_valid){int night=clio_solar_from_utc(utc,lat,lon);if(night>=0)return night;}
 /* Never invent daylight from a fixed clock window without date/location. */
 return saved==0?0:1;
}
#endif
