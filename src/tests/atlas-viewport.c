/* Match device boot ordering: load the atlas before any live GPS position. */
#define HOST_TEST 1
#define CLIO_UNICODE 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static u16 outside_pixels[W*H];
static const char*out;
static void shot(const char*name){char p[1024];snprintf(p,sizeof(p),"%s/%s.rgb565",out,name);FILE*f=fopen(p,"wb");assert(f&&fwrite(fb,1,sizeof(fb),f)==sizeof(fb));fclose(f);}
static int outside_frame(int z){
 fill(0,40,W,MAP_STRIP_Y-40,C_BG);text_center(240,116,ui_tr("Outside map area"),C_MUTED,1);memcpy(outside_pixels,fb,sizeof(fb));
 perspective_scale_update();draw_map_geometry(z);
 return !memcmp(fb+40*W,outside_pixels+40*W,(MAP_STRIP_Y-40)*W*sizeof(u16));
}
static void position(s32 lat,s32 lon){gps.lat=track_lat=cam_lat=lat;gps.lon=track_lon=cam_lon=lon;track_valid=1;}
int main(int argc,char**argv){
 assert(argc==2||argc==3);out=argv[1];assert(load_font());assert(cliou_load(CLIO_HOST_ROOT "clio843/maps/fonts.cpf"));
 memzero(&gps,sizeof(gps));gps.hour=-1;pref.language=0;pref.autozoom=0;pref.theme=THEME_NIGHT;manual_zoom=6;apply_palette(1);
 assert(load_map()&&atlas_ready&&atlas_nr>1&&atlas_route_region==0);struct CpmHeader boot_header=mh;
 /* A saved Newcastle fix is restored only AFTER load_map(), as on the device. */
 lastfix_valid=1;lastfix_lat=5497548;lastfix_lon=-164050;lastfix_course=0;seed_boot_map_view();
 int owner=atlas_owner(view_lat(),view_lon());assert(owner>0);assert(view_lat()>mh.maxlat||view_lat()<mh.minlat||view_lon()>mh.maxlon||view_lon()<mh.minlon);
 printf("Cold boot: opened %s before GPS; saved Newcastle view belongs to %s.\n",atlas_regions[0].path,atlas_regions[owner].path);
 int rejected=outside_frame(6);shot("newcastle-saved-fix");
 if(argc==3){assert(!strcmp(argv[2],"--expect-bug")&&rejected);puts("Reproduced: valid Newcastle atlas coverage incorrectly renders Outside map area.");return 0;}
 assert(!rejected);
 /* Live GPS must also work without changing the initially opened region. */
 gps.fix=1;position(lastfix_lat,lastfix_lon);update_road_context();assert(roadctx.valid);
 for(int night=0;night<2;night++){pref.theme=night?THEME_NIGHT:THEME_DAY;apply_palette(night);for(int z=4;z<=9;z++){freecam=z>6;map_tilt_q8=freecam?0:256;assert(!outside_frame(z));}freecam=0;map_tilt_q8=256;manual_zoom=6;draw();shot(night?"newcastle-night":"newcastle-day");}
 unsigned frames=0;
 for(u32 i=0;i<atlas_nr;i++){
  struct AtlasRegion*r=&atlas_regions[i];position(r->south+(r->north-r->south)/2,r->west+(r->east-r->west)/2);assert(atlas_owner(gps.lat,gps.lon)==(int)i);assert(!outside_frame(6));frames++;
  /* Explore can cross either side of every shared core edge. */
  const s32 lat[4]={r->south+(r->north-r->south)/2,r->south+(r->north-r->south)/2,r->south,r->north};
  const s32 lon[4]={r->west,r->east,r->west+(r->east-r->west)/2,r->west+(r->east-r->west)/2};
  freecam=1;map_tilt_q8=0;
  for(int e=0;e<4;e++)for(int delta=-1;delta<=1;delta++){position(lat[e]+(e>=2?delta:0),lon[e]+(e<2?delta:0));assert(outside_frame(6)==(atlas_owner(view_lat(),view_lon())<0));frames++;}
  freecam=0;map_tilt_q8=256;
 }
 position(4000000,0);assert(atlas_owner(gps.lat,gps.lon)<0&&outside_frame(6));shot("outside-installed-atlas");
 /* A physical child header remains physical; never overwrite its index bounds. */
 assert(!memcmp(&mh,&boot_header,sizeof(mh)));
 atlas_ready=0;position(mh.minlat-3,mh.minlon+(mh.maxlon-mh.minlon)/2);assert(outside_frame(6));
 position(mh.minlat-2,mh.minlon+(mh.maxlon-mh.minlon)/2);assert(!outside_frame(6));
 printf("Atlas viewport passed: saved/live fixes after GPS-free startup, Day/Night zooms 4..9, %u regions and %u core/seam frames, genuine outside coverage and unchanged single-pack edge tolerance.\n",atlas_nr,frames);
 return 0;
}
