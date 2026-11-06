/* Profile real startup loaders with isolated host I/O; no vehicle emulation. */
#define HOST_TEST 1
#define CLIO_UNICODE 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <sys/resource.h>

static double profile_now(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return t.tv_sec*1000.+t.tv_nsec/1000000.;}
#define STAGE(name,call) do{ \
 struct rusage a,b;getrusage(RUSAGE_SELF,&a);double t=profile_now(); \
 call;double elapsed=profile_now()-t;getrusage(RUSAGE_SELF,&b); \
 printf("%-25s %9.3f ms   %6ld / %6ld\n",name,elapsed,b.ru_minflt-a.ru_minflt,b.ru_majflt-a.ru_majflt); \
}while(0)

int main(int argc,char**argv){
 assert(argc==3);s32 lat=atoi0(argv[1]),lon=atoi0(argv[2]);
 memzero(&gps,sizeof(gps));gps.hour=-1;
 puts("HOST timings only; not a cold SD-card or 400 MHz head-unit benchmark.");
 puts("Stage                              Time      minor / major page faults");
 STAGE("Base font",assert(load_font()));
 STAGE("Unicode catalog",assert(cliou_load(CLIO_HOST_ROOT "clio843/maps/fonts.cpf")));
 STAGE("Preferences/fix/theme",load_prefs();load_last_fix();seed_boot_theme());
 STAGE("Places/trip/audio",load_last_audio();nrecents=read_places(RECENT_PATH,recents,RECENT_MAX);nfaves=read_places(FAVES_PATH,faves,FAVE_MAX);load_trip());
 STAGE("Atlas/map catalog",assert(load_map()));
 STAGE("Timezones",load_timezones());
 STAGE("Regions",load_regions());
 STAGE("Cameras",load_cameras());
 /* Match the device's GPS-free boot and restore a representative saved view. */
 lastfix_valid=1;lastfix_lat=lat;lastfix_lon=lon;lastfix_course=0;seed_boot_map_view();
 STAGE("Route/fast-query headers",assert(load_route_data()));
 STAGE("First framebuffer",draw());
 STAGE("Second framebuffer",draw());
 printf("Atlas: %u mapped bytes, %u regions, %u nodes, %u edges, %u members; QAD%d.\n",atlas_bytes,atlas_nr,atlas_nn,atlas_ne,atlas_nm,qadjmode);
 puts("Atlas loading validates every graph record; QAD7 loading maps the file and reads its header, not the whole packet/index data.");
 return 0;
}
