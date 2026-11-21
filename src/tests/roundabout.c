#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
int main(void){FILE*f=tmpfile();assert(f);routefd=fileno(f);routeok=1;rh.base_off=0;rh.base_edges=5;struct CprBase arcs[5];memzero(arcs,sizeof(arcs));for(int i=1;i<=3;i++){arcs[i].flags=4;arcs[i].pad=1;}assert(fwrite(arcs,sizeof(arcs),1,f)==1);fflush(f);cpr_bcache_count=0;
 route_status=R_READY;route_head=0;route_n=6;for(int i=0;i<6;i++){route_node[i]=ROUTE_NOARC;route_lat[i]=4880000+i*50;route_lon[i]=200000;route_seg_arc[i]=i?i-1:ROUTE_NOARC;route_seg_phys[i]=i?i:ROUTE_NOARC;}
 nav_maneuver_find();assert(nav_man_type==MAN_ROUNDABOUT&&nav_man_exit==3&&nav_man_idx==4&&nav_man_m>0);assert(!strcmp(nav_man_text(),"Take exit 3"));route_head=2;nav_maneuver_find();assert(nav_man_type==MAN_ROUNDABOUT&&nav_man_exit==0); // no invented exit number when joining midway
 region_flags=0;fill(0,0,W,H,0);draw_manoeuvre_icon(30,30,MAN_ROUNDABOUT);u16 right[FB_PIX];memcpy(right,fb,sizeof(fb));region_flags=4;fill(0,0,W,H,0);draw_manoeuvre_icon(30,30,MAN_ROUNDABOUT);assert(memcmp(right,fb,sizeof(fb)));
 // The circulation glyph is a true mirror; the unknown-exit centre stays
 // clear, edge coverage is antialiased, and it fits both 24/30-pixel HUD cells.
 unsigned solid=0,partial=0;
 for(int y=0;y<H;y++)for(int x=0;x<W;x++){
  int dx=x-30,dy=y-30;
  if(dx< -11||dx>11||dy< -11||dy>11){assert(!fb[y*W+x]);continue;}
  assert(fb[y*W+x]==right[y*W+60-x]);
  if(fb[y*W+x]==C_CYAN)solid++;else if(fb[y*W+x])partial++;
  if(dx>=-3&&dx<=3&&dy>=-4&&dy<=2)assert(!fb[y*W+x]);
 }
 assert(solid>20&&partial>10);
 nav_man_exit=1;fill(0,0,W,H,0);draw_manoeuvre_icon(30,30,MAN_ROUNDABOUT);memcpy(right,fb,sizeof(fb));
 nav_man_exit=3;fill(0,0,W,H,0);draw_manoeuvre_icon(30,30,MAN_ROUNDABOUT);assert(memcmp(right,fb,sizeof(fb)));
 // Two-digit exit counts retain both digits instead of fitting to an ellipsis.
 nav_man_exit=12;fill(0,0,W,H,0);draw_manoeuvre_icon(30,30,MAN_ROUNDABOUT);memcpy(right,fb,sizeof(fb));
 nav_man_exit=13;fill(0,0,W,H,0);draw_manoeuvre_icon(30,30,MAN_ROUNDABOUT);assert(memcmp(right,fb,sizeof(fb)));
 nav_man_exit=22;fill(0,0,W,H,0);draw_manoeuvre_icon(30,30,MAN_ROUNDABOUT);assert(memcmp(right,fb,sizeof(fb)));
 nav_man_exit=0;fill(0,0,W,H,0);draw_manoeuvre_icon(0,0,MAN_ROUNDABOUT);
 fclose(f);
 // A physical exit exists, but only state 1 is legally allowed to use it.
 // State 2 may continue on the roundabout; state 0 has no public exit.
 f=tmpfile();assert(f);routefd=fileno(f);rh.ver=5;rh.nodes=3;rh.node_off=0;rh.tiles=1;rh.tile_off=120;rh.tile_refs=2;rh.tref_off=136;rh.base_edges=2;rh.base_off=144;rh.src_refs=2;rh.src_off=192;
 struct CprNode nodes[3];memzero(nodes,sizeof(nodes));for(int i=0;i<3;i++){nodes[i].lat=4880000;nodes[i].lon=200000;}assert(fwrite(nodes,sizeof(nodes),1,f)==1);
 struct CprTile tile={.tx=tile_div(200000,6),.ty=tile_div(4880000,6),.first=0,.count=2};assert(fwrite(&tile,sizeof(tile),1,f)==1);u32 refs[2]={0,1};assert(fwrite(refs,sizeof(refs),1,f)==1);
 struct CprBase exits[2];memzero(exits,sizeof(exits));exits[0].src_count=1;exits[1].src_first=1;exits[1].src_count=1;exits[1].flags=4;assert(fwrite(exits,sizeof(exits),1,f)==1);u32 sources[2]={1,2};assert(fwrite(sources,sizeof(sources),1,f)==1);fflush(f);cpr_bcache_count=0;memzero(cpr_tcache_count,sizeof(cpr_tcache_count));memzero(cpr_rcache_count,sizeof(cpr_rcache_count));
 route_node[1]=0;assert(!nav_round_exit_legal(1,&arcs[1]));route_node[1]=1;assert(nav_round_exit_legal(1,&arcs[1]));route_node[1]=2;assert(!nav_round_exit_legal(1,&arcs[1]));fclose(f);
 // Secondary ("then") manoeuvre: turn right followed 90 m later by turn left.
 routefd=-1;routeok=0;route_status=R_READY;route_head=0;route_n=7;
 s32 tlat[]={5497000,5497050,5497100,5497100,5497100,5497150,5497200};
 s32 tlon[]={-161000,-161000,-161000,-160930,-160860,-160860,-160860};
 for(int i=0;i<7;i++){route_node[i]=ROUTE_NOARC;route_lat[i]=tlat[i];route_lon[i]=tlon[i];route_seg_arc[i]=ROUTE_NOARC;route_seg_phys[i]=i<=2?10u:(i<=4?20u:30u);}
 nav_maneuver_find();
 assert(nav_man_type==MAN_RIGHT&&nav_man_idx==2);
 assert(nav_next_type==MAN_LEFT&&nav_next_m>=70u&&nav_next_m<=130u);
 // A follow-up turn beyond 250 m is suppressed so the secondary cue stays uncluttered.
 route_lon[3]=-160600;route_lon[4]=-160400;route_lon[5]=-160400;route_lon[6]=-160400;
 nav_maneuver_find();
 assert(nav_man_type==MAN_RIGHT&&nav_next_type==MAN_NONE);
 puts("Roundabout and secondary manoeuvre: third-exit count, unknown midway entry, mirrored antialiased circulation, clear numeral space, clipped HUD bounds and chained turns passed.");return 0;}
