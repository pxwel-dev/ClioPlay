#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static void iso(const char*s){copystr(region_iso,sizeof(region_iso),s);}
int main(void){
 struct SpeedNeighbour v[8];memzero(v,sizeof(v));int flags=0;
 for(int i=0;i<5;i++){v[i].name=i+1;v[i].local=1;}
 /* Same decision code as the live matcher. Country baselines require a
  * demonstrably urban network; sparse/rural geometry remains unknown. */
 assert(speed_estimate_choice(v,4,"FR",&flags)==0);
 assert(speed_estimate_choice(v,5,"FR",&flags)==50&&flags==1);
 assert(!speed_estimate_choice(v,5,"ES",&flags));
 assert(!speed_estimate_choice(v,5,"GB",&flags));
 assert(!speed_estimate_choice(v,5,"US",&flags));
 assert(!speed_estimate_choice(v,5,"ZZ",&flags));
 for(int i=0;i<3;i++){v[i].value=30;v[i].flags=2;}
 assert(speed_estimate_choice(v,5,"GB",&flags)==30&&flags==3);
 v[3].value=20;v[3].flags=2;
 assert(speed_estimate_choice(v,5,"GB",&flags)==30);
 v[4].value=20;v[4].flags=2;
 assert(!speed_estimate_choice(v,5,"GB",&flags));
 for(int i=0;i<5;i++){v[i].value=30;v[i].flags=0;}
 assert(speed_estimate_choice(v,5,"ES",&flags)==30&&flags==1);
 v[0].value=20;v[1].value=20;
 assert(!speed_estimate_choice(v,5,"FR",&flags));
 for(int i=0;i<5;i++)v[i].value=100;
 assert(!speed_estimate_choice(v,5,"FR",&flags));
 struct RoadCtx r={0};r.valid=1;r.speed=r.reverse_speed=30;r.speed_flags=r.reverse_flags=2;
 assert(speed_local_vote(&r,1,&flags)==30&&flags==2);
 assert(!speed_local_vote(&r,0,&flags));
 r.speed_flags=3;assert(!speed_local_vote(&r,1,&flags)); // no chained estimates
 r.speed_flags=2;r.reverse_speed=20;assert(!speed_local_vote(&r,1,&flags));
 r.reverse_speed=0;r.reverse_flags=16;assert(speed_local_vote(&r,1,&flags)==30);
 r.reverse_flags=8;assert(!speed_local_vote(&r,1,&flags));
 struct RoadGeomRef ref={0};ref.valid=1;ref.cls=5;mapfmt=5;
 roadctx=r;roadctx.speed=30;roadctx.speed_flags=2;road_estimate_missing(&ref);
 assert(roadctx.speed==30&&roadctx.speed_flags==2);
 roadctx.speed=0;roadctx.speed_flags=8;road_estimate_missing(&ref);assert(roadctx.speed_flags==8);
 roadctx.speed_flags=4;road_estimate_missing(&ref);assert(roadctx.speed_flags==4);
 roadctx.speed_flags=roadctx.reverse_flags=16;road_estimate_missing(&ref);assert(!roadctx.speed);
 char b[32]="";iso("GB");region_flags=1;pref.units=0;r.speed=30;r.speed_flags=3;
 append_road_limit(b,sizeof(b),&r);assert(!strcmp(b,"30?"));
 pref.units=1;b[0]=0;append_road_limit(b,sizeof(b),&r);assert(!strcmp(b,"48?"));
 puts("Speed estimates passed: urban evidence threshold, metric/imperial, regional exclusions, local consensus/conflicts, explicit/variable/unrestricted priority and visible ?.");
 return 0;
}
