/* Native verification of ClioPlay v3.0 all-legs-at-once multi-stop CH planner,
 * smart partial invalidation / pool compaction, Option B HUD vs Trip totals,
 * secondary future-leg route rendering, and Z7-Z9 on-carriageway turn chevrons. */
#define HOST_TEST 1
#define main clio040_host_main
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void set_pos(s32 la,s32 lo){
 gps.fix=1;gps.mph10=300;gps.course=0;gps.lat=la;gps.lon=lo;
 track_lat=la;track_lon=lo;track_valid=1;
 track_lat4=la*16;track_lon4=lo*16;track_q4_valid=1;
 display_course=0;display_course_valid=1;
}

static void test_chevron_and_future_overlay(void){
 apply_palette(1); /* Night theme */
 fill(0,0,W,H,C_MAP_BG);
 freecam=0;mapok=1;
 set_pos(5500000,-164000);
 /* Build a clean L-shaped active Leg 0 turning left at index 2 (North -> West), all inside Z8 viewport. */
 route_status=R_READY;route_head=0;route_n=5;
 route_lat[0]=5499960;route_lon[0]=-164000;
 route_lat[1]=5500000;route_lon[1]=-164000;
 route_lat[2]=5500050;route_lon[2]=-164000; /* Turn vertex (~55m ahead of car) */
 route_lat[3]=5500050;route_lon[3]=-164060;
 route_lat[4]=5500050;route_lon[4]=-164120;
 nav_man_type=MAN_LEFT;nav_man_idx=2;nav_man_m=55;
 /* Build a cached future Leg 1 continuing North from the stop inside the Z8 viewport. */
 memzero(&trip,sizeof(trip));trip.active=1;trip.count=2;trip.active_idx=0;
 trip.target[0]=(struct TripTarget){5500050,-164120,"Stop 1"};
 trip.target[1]=(struct TripTarget){5500120,-164120,"Final"};
 trip_plan_reset();
 trip_future_lat[0]=5500050;trip_future_lon[0]=-164120;
 trip_future_lat[1]=5500085;trip_future_lon[1]=-164120;
 trip_future_lat[2]=5500120;trip_future_lon[2]=-164120;
 trip_future_used=3;
 trip_legs[1]=(struct TripLeg){TLEG_READY,5500050,-164120,5500120,-164120,1800,1200,0,3};
 /* Render Z8 overlay. */
 draw_route_overlay(8);
 int px_route=0,px_future=0,px_white=0;
 for(int y=MAP_TOP;y<MAP_BOTTOM;y++){
  for(int x=0;x<W;x++){
   u16 c=fb[y*W+x];
   if(c==C_ROUTE)px_route++;
   if(c==C_ROUTE_FUTURE)px_future++;
   if(c==0xffff)px_white++;
  }
 }
 assert(px_route>100);
 assert(px_future>50);
 assert(px_white>20);
 /* Verify Option B HUD still shows Leg 0 (Next stop), while Trip total sums Leg 0 + Leg 1. */
 route_result_m=800;route_result_cost=1200;atlas_future_cost=0;atlas_future_m=0;
 route_seg_cost[0]=0;for(int i=1;i<route_n;i++)route_seg_cost[i]=300;
 route_seg_m[0]=0;for(int i=1;i<route_n;i++)route_seg_m[i]=200;
 assert(trip_plan_all_ready());
 assert(trip_plan_future_cost_ds()==1800);
 assert(trip_plan_future_dist_m()==1200);
 u32 c0=trip_plan_stop_cum_cost_ds(0);
 u32 c1=trip_plan_stop_cum_cost_ds(1);
 assert(c0<c1&&c1-c0==1800);
 char hud_line[96];hud_journey_text(hud_line,sizeof(hud_line));
 /* Option B: HUD pill reports Next stop (~2 min), NOT combined 5 min! */
 assert(strstr(hud_line,"2 min"));
}

static void test_real_ch_multileg(const char*cpr_path){
 trip_plan_reset();route_cancel();
 mkdir(CLIO_HOST_ROOT "clio843",0777);mkdir(CLIO_HOST_ROOT "clio843/maps",0777);
 unlink(ROUTE_PATH);unlink(QADJ_PATH);
 assert(symlink(cpr_path,ROUTE_PATH)==0);
 atlas_ready=0;
 assert(load_route_data());
 /* 3-stop trip across the atlas-chain restricted graph (West -> Mid1 -> Mid2 -> East). */
 set_pos(5500000,-168840);
 memzero(&trip,sizeof(trip));
 trip.active=1;trip.count=3;trip.active_idx=0;
 trip.target[0]=(struct TripTarget){5500500,-163540,"Stop One"};
 trip.target[1]=(struct TripTarget){5500500,-155840,"Stop Two"};
 trip.target[2]=(struct TripTarget){5500000,-142456,"East Final"};
 assert(route_start(trip.target[0].lat,trip.target[0].lon));
 for(int i=0;i<10000&&route_status==R_CALC;i++)route_service_until(s_now_ms()+20u);
 assert(route_status==R_READY&&route_n>=2);
 int leg0_n=route_n;s32 leg0_end_lat=route_lat[route_n-1],leg0_end_lon=route_lon[route_n-1];
 u32 leg0_comp=route_dest_comp;
 /* Cooperatively calculate future Legs 1 and 2 using idle CH arena. */
 trip_plan_sync();
 assert(trip_plan_pending());
 for(int i=0;i<200&&trip_plan_pending();i++)trip_plan_service_until(s_now_ms()+20u);
 assert(!trip_plan_pending()&&trip_plan_all_ready());
 /* Leg 0 path & snap metadata must remain 100% untouched! */
 assert(route_n==leg0_n&&route_lat[route_n-1]==leg0_end_lat&&route_lon[route_n-1]==leg0_end_lon&&route_dest_comp==leg0_comp);
 assert(trip_legs[1].status==TLEG_READY&&trip_legs[1].dist_m>0&&trip_legs[1].cost_ds>0&&trip_legs[1].pt_n>=2);
 assert(trip_legs[2].status==TLEG_READY&&trip_legs[2].dist_m>0&&trip_legs[2].cost_ds>0&&trip_legs[2].pt_n>=2);
 u32 leg1_cost=trip_legs[1].cost_ds,leg2_cost=trip_legs[2].cost_ds;
 /* Off-route Leg 0 recalculation MUST preserve already-cached Legs 1 & 2! */
 assert(route_start(trip.target[0].lat,trip.target[0].lon));
 for(int i=0;i<10000&&route_status==R_CALC;i++)route_service_until(s_now_ms()+20u);
 assert(route_status==R_READY);
 trip_plan_sync();
 assert(!trip_plan_pending()&&trip_legs[1].status==TLEG_READY&&trip_legs[2].status==TLEG_READY);
 assert(trip_legs[1].cost_ds==leg1_cost&&trip_legs[2].cost_ds==leg2_cost);
 /* Advancing past Stop 0 keeps Leg 2 cached with zero recomputation and compacts pool. */
 u16 used_before=trip_future_used;
 trip.active_idx=1;trip_plan_sync();
 assert(trip_legs[1].status==TLEG_NONE&&trip_legs[2].status==TLEG_READY&&trip_legs[2].cost_ds==leg2_cost);
 assert(trip_future_used<used_before&&trip_legs[2].pt_off==0);
 trip_plan_reset();route_cancel();
}

int main(int argc,char**argv){
 test_chevron_and_future_overlay();
 if(argc>1)test_real_ch_multileg(argv[1]);
 puts("Multi-leg CH planner, partial invalidation/compaction, Option B HUD, future overlay and Z8 turn chevron passed.");
 return 0;
}
