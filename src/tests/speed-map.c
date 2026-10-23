#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
int main(int argc,char**argv){
 assert(argc==2&&load_map()&&mapfmt==5&&load_regions());gps.fix=1;gps.mph10=200;
 FILE*f=fopen(argv[1],"r");assert(f);int lat,lon,heading;char expected[32];int count=0;
 while(fscanf(f,"%d %d %d %31s",&lat,&lon,&heading,expected)==4){
  gps.lat=track_lat=lat;gps.lon=track_lon=lon;gps.course=track_course=heading;track_valid=1;
  pref.units=0;region_resolve(lat,lon);update_road_context();char actual[32]="";append_road_limit(actual,sizeof(actual),&roadctx);
  printf("%s %d,%d: %s, %s\n",region_iso,lat,lon,roadctx.street,actual);
  if(strcmp(actual,expected)){fprintf(stderr,"Expected %s, got %s\n",expected,actual);return 1;}count++;
 }
 fclose(f);assert(count>=10);
 puts("Production map matching passed: country/context estimates, local metric/mph votes, wrong-way/unsupported/special road exclusions and explicit limits.");return 0;
}
