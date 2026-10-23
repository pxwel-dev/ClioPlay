#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <stdio.h>
#include "camera_policy_cases.h"
int main(void){
 assert(load_regions()&&load_cameras());gps.fix=track_valid=display_course_valid=1;gps.course=track_course=display_course=0;route_status=R_IDLE;pref.cameras=1;
 int checks=0;
 for(int language=0;language<UI_LANGUAGE_COUNT;language++)for(unsigned i=0;i<sizeof(camera_policy_cases)/sizeof(camera_policy_cases[0]);i++){
  const struct PolicyCase*c=&camera_policy_cases[i];pref.language=language;gps.lat=c->lat;gps.lon=c->lon;region_resolve(gps.lat,gps.lon);
  assert(region_iso[0]==c->iso[0]&&region_iso[1]==c->iso[1]);assert(!!(region_flags&2)==c->enabled);
  camera_clear();gps_fix_ms=s_now_ms();camera_next_ms=0;camera_service();assert(!!camera_visible==c->enabled);checks++;
  if(c->enabled){pref.cameras=0;camera_next_ms=0;camera_service();assert(!camera_visible);pref.cameras=1;}
 }
 printf("%d native country/language combinations: exported policy flags, GPS camera alerts, blocked/pending suppression and user-off passed.\n",checks);return 0;
}
