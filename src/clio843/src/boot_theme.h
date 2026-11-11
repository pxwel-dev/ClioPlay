/* Shared daily AUTO schedule for the splash and live interface. */
#ifndef CLIO_BOOT_THEME_H
#define CLIO_BOOT_THEME_H
enum {CLIO_DAY_START=7*60,CLIO_NIGHT_START=18*60};
static int clio_leap_year(int y){return(y%4)==0&&((y%100)!=0||(y%400)==0);}
static int clio_month_days(int y,int m){static const u8 days[12]={31,28,31,30,31,30,31,31,30,31,30,31};return m<1||m>12?0:days[m-1]+(m==2&&clio_leap_year(y));}
static int clio_day_of_year(int y,int m,int day){
 if(y<2020||y>2099||day<1||day>clio_month_days(y,m))return-1;
 int d=day-1;for(int i=1;i<m;i++)d+=clio_month_days(y,i);return d;
}
static int clio_theme_time(const char*s,int fallback){
 int value=0,n=0;
 for(;*s;s++,n++){
  if(*s<'0'||*s>'9'||n>=4)return fallback;
  value=value*10+*s-'0';
 }
 return n&&value<1440?value:fallback;
}
static int clio_theme_schedule_valid(int day_start,int night_start){
 return day_start>=0&&day_start<1440&&night_start>=0&&night_start<1440&&day_start!=night_start;
}
static int clio_schedule_night(int minute,int day_start,int night_start){
 if(minute<0||minute>=1440||!clio_theme_schedule_valid(day_start,night_start))return-1;
 if(day_start<night_start)return minute<day_start||minute>=night_start;
 return minute>=night_start&&minute<day_start;
}
static int clio_theme_local_minute(long utc,int offset){
 if(utc<1577836800L||offset< -720||offset>840)return-1;
 int minute=(int)((utc%86400L)/60L)+offset;
 return (minute+1440)%1440;
}
static int clio_boot_night(int mode,long utc,int offset,int day_start,int night_start,int saved){
 if(mode==0)return 0;if(mode==1)return 1;
 int night=clio_schedule_night(clio_theme_local_minute(utc,offset),day_start,night_start);
 return night>=0?night:(saved==0?0:1);
}
#endif
