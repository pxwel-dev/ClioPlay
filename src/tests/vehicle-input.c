/* Actual inline console/FIFO, UI actions and Focus/PAC temperature integration. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static void put32(u8*b,unsigned value){for(int i=0;i<4;i++)b[i]=(u8)(value>>(i*8));}
static void record(u8*b,unsigned src,unsigned dir,unsigned rot,unsigned press,unsigned count){
 memset(b,0,28);put32(b,src);put32(b+4,dir);put32(b+8,rot);put32(b+16,press);put32(b+20,1);b[24]=(u8)count;
}
static void reset_console(void){
 if(csw.fd>=0)s_close(csw.fd);memset(&csw,0,sizeof(csw));csw.fd=-1;csw_defaults(csw.map);pref.debug=0;
}
static void write_text(const char*p,const char*s){FILE*f=fopen(p,"wb");assert(f);fputs(s,f);fclose(f);}
static void console_protocol(void){
 u8 b[28];reset_console();
 const int buttons[]={K_ESC,K_BACK,K_SR,K_CONSOLE_INFO,K_MENU,K_CONSOLE_THEME,K_CONSOLE_GPS,K_CONSOLE_TRIP,K_MAP};
 for(unsigned src=1;src<=9;src++){
  memset(b,255,sizeof(b));put32(b,src);csw_record(b,1000);assert(csw_next()==buttons[src-1]);assert(!csw_next());
  csw_record(b,1100);assert(!csw_next());csw_record(b,1250);assert(!csw_next());csw_record(b,1500);assert(csw_next()==buttons[src-1]);
 }
 reset_console();record(b,0,0,0,1,0);csw_record(b,0);assert(!csw_next());record(b,0,5,3,0,0);csw_record(b,0);assert(!csw_next());
 record(b,0,5,3,1,0);csw_record(b,1);assert(csw_next()==K_ENTER);
 /* Duplicate/held push records (or a burst read after a render stall) give one OK. */
 csw_record(b,1);assert(!csw_next());csw_record(b,200);assert(!csw_next());csw_record(b,400);assert(!csw_next());
 /* The car sends one pressed==1 record per push and NO release record
  * (csw.log): consecutive pushes must each be an OK. */
 csw_record(b,1000);assert(csw_next()==K_ENTER&&!csw_next());
 csw_record(b,1600);assert(csw_next()==K_ENTER&&!csw_next());
 csw_record(b,2200);assert(csw_next()==K_ENTER&&!csw_next());
 /* A real release still re-arms immediately. */
 record(b,0,5,3,0,0);csw_record(b,2201);record(b,0,5,3,1,0);csw_record(b,2202);assert(csw_next()==K_ENTER);
 /* Car sequence: push on the map, MENU button, push selects the first option. */
 reset_console();record(b,0,5,3,1,0);put32(b+12,1);csw_record(b,5000);assert(csw_next()==K_ENTER);
 record(b,5,0,0,0,0);csw_record(b,6000);assert(csw_next()==K_MENU);
 record(b,0,5,3,1,0);put32(b+12,1);csw_record(b,7000);assert(csw_next()==K_ENTER&&!csw_next());
 reset_console();record(b,0,5,1,0,14);csw_record(b,100);for(int i=0;i<14;i++)assert(csw_next()==K_ROTCCW);assert(!csw_next());
 record(b,0,5,2,0,3);csw_record(b,101);for(int i=0;i<3;i++)assert(csw_next()==K_ROTCW);
 record(b,0,5,1,0,0);csw_record(b,102);assert(!csw_next());b[24]=255;csw_record(b,103);assert(!csw_next());
 reset_console();unsigned invalid=csw.invalid;record(b,10,1,1,1,1);csw_record(b,104);assert(csw.invalid==invalid+1&&!csw_next()&&!csw.active);
 record(b,0,0,0,1,0);csw_record(b,105);assert(!csw.active);
 reset_console();record(b,0,1,3,0,0);csw_record(b,0);assert(csw_next()==K_UP);csw_record(b,40);assert(!csw_next());csw_record(b,150);assert(csw_next()==K_UP);
 record(b,0,5,3,0,0);csw_record(b,151);record(b,0,1,3,0,0);csw_record(b,152);assert(csw_next()==K_UP);
 /* All possible byte split points retain exactly one complete action. */
 for(int split=1;split<28;split++){
  reset_console();record(b,5,0,0,0,0);csw_feed(b,split,100);assert(!csw_next());csw_feed(b+split,28-split,100);assert(csw_next()==K_MENU&&!csw_next());
 }
 reset_console();for(int i=0;i<32;i++)csw_enqueue(K_DOWN,14);csw_enqueue(K_ENTER,1);assert(csw.dropped==1);
 for(int i=0;i<448;i++)assert(csw_next()==K_DOWN);assert(!csw_next());
 /* Bounded config parser, aliases, unsupported edits and same-size reload. */
 u16 map[17];int verbose;char good[]="btn4=BACK\nrot2=knob+\npush=ok\nverbose=1\n";
 assert(csw_parse_config(good,map,&verbose)&&map[3]==K_BACK&&map[14]==K_ROTCW&&verbose);
 char bad[]="btn5=MAP\nrot2=NOT_AN_ACTION\n";assert(!csw_parse_config(bad,map,&verbose));
 write_text(CSW_MAP_PATH,"btn5=UP\n");csw_config_load();assert(csw.map[4]==K_UP);
 write_text(CSW_MAP_PATH,"btn5=OK\n");csw_config_load();assert(csw.map[4]==K_ENTER);
 write_text(CSW_MAP_PATH,"btn5=NOPE\n");csw_config_load();assert(csw.map[4]==K_ENTER);
 s_unlink(CSW_MAP_PATH);csw_config_load();assert(csw.map[4]==K_MENU);
 /* Repeat (BTN7) defaults to GPS and remains remappable via cswmap.ini. */
 assert(csw.map[6]==K_CONSOLE_GPS);
 char gps_map[]="btn4=gps\nbtn7=info\n";assert(csw_parse_config(gps_map,map,&verbose)&&map[3]==K_CONSOLE_GPS&&map[6]==K_CONSOLE_INFO);
 write_text(CSW_MAP_PATH,"btn7=INFO\n");csw_config_load();memset(b,255,sizeof(b));put32(b,7);csw_record(b,9000);assert(csw_next()==K_CONSOLE_INFO&&!csw_next());
 s_unlink(CSW_MAP_PATH);csw_config_load();assert(csw.map[6]==K_CONSOLE_GPS);
 puts("Console protocol: buttons with garbage tails, release/hold edges, neutral suppression, detents, framing, queue bounds and atomic same-size config reload passed.");
}
static void console_ui(void){
 modal=MOD_NONE;menu_open(P_HOME,0,hud_now_ms());key_handle(K_ROTCW);assert(menu_focus==1);key_handle(K_ROTCCW);assert(menu_focus==0);
 modal=MOD_ROUTE_POINT;modal_choice=0;key_handle(K_ROTCW);assert(modal_choice==1);key_handle(K_ROTCCW);assert(modal_choice==0);
 modal=MOD_NONE;pref.language=0;keyboard_open_fave_name(1,2);menu_open(P_NAME,0,hud_now_ms());pk_reset();pk_prepare();pk_focus=5;key_handle(K_ROTCW);assert(pk_focus==6);pk_focus=PK_LANGUAGE;key_handle(K_ROTCW);assert(pk_focus==PK_MODE);
 pk_focus=PK_LANGUAGE;key_handle(K_ENTER);key_handle(K_ROTCW);assert(kbd_pick==1);key_handle(K_BACK);
 screen=SCR_COORDINATES;coordinates.step=COORD_LAT;coordinates.focus=2;key_handle(K_ROTCW);assert(coordinates.focus==3);
 key_handle(K_CONSOLE_TRIP);assert(sheet_page==P_TRIP);
 int old=pref.theme;key_handle(K_CONSOLE_THEME);assert(pref.theme==(old+1)%3);
 screen=SCR_MAP;freecam=0;route_status=R_READY;
 gps.fix=roadctx.valid=1;copystr(roadctx.street,sizeof(roadctx.street),"Quayside");
 u32 info_at=hud_now_ms();hud_driver_prepare(info_at);hud_driver_prepare(info_at+HUD_ROAD_NAME_MS);
 assert(!hud_tween[HC_ROAD_NAME].target.box.a);
 key_handle(K_CONSOLE_INFO);assert(hud_navigation.kind==HUD_JOURNEY);
 hud_driver_prepare(hud_now_ms());
 assert(hud_tween[HC_ROAD_NAME].target.box.a&&streq(hud_tween[HC_ROAD_NAME].target.title,"Quayside"));
 gps.fix=roadctx.valid=0;
 /* GPS action toggles the live GPS HUD, stays non-blocking on menus, and defers to modals. */
 pref.gps_hud=0;
 key_handle(K_CONSOLE_GPS);assert(pref.gps_hud&&screen==SCR_MAP&&gps_hud_mode()==GPS_HUD_FULL);
 key_handle(K_CONSOLE_GPS);assert(!pref.gps_hud&&screen==SCR_MAP&&gps_hud_mode()==GPS_HUD_OFF);
 key_handle(K_CONSOLE_TRIP);assert(sheet_page==P_TRIP);
 key_handle(K_CONSOLE_GPS);assert(sheet_page==P_TRIP&&pref.gps_hud&&gps_hud_mode()==GPS_HUD_MIN);
 key_handle(K_MAP);assert(screen==SCR_MAP&&pref.gps_hud&&gps_hud_mode()==GPS_HUD_FULL);
 key_handle(K_CONSOLE_GPS);assert(!pref.gps_hud);
 modal=MOD_RESUME_TRIP;key_handle(K_CONSOLE_GPS);assert(modal==MOD_RESUME_TRIP&&!pref.gps_hud);modal=MOD_NONE;
 modal=MOD_RESUME_TRIP;key_handle(K_CONSOLE_TRIP);assert(modal==MOD_RESUME_TRIP&&screen==SCR_MAP);modal=MOD_NONE;
 /* An evdev remote stays usable while the independent console queue runs. */
 int fds[2];assert(pipe(fds)==0);fcntl(fds[0],F_SETFL,O_NONBLOCK);
 for(int i=0;i<INPUT_MAX;i++)infd[i]=-1;infd[0]=fds[0];input_lock=-1;
 int wheel[2];assert(pipe(wheel)==0);fcntl(wheel[0],F_SETFL,O_NONBLOCK);infd[1]=wheel[0];
 struct input_event16 rotary={0,0,EV_KEY,K_ROTCW,1};menu_open(P_HOME,0,hud_now_ms());
 assert(write(wheel[1],&rotary,sizeof(rotary))==sizeof(rotary));poll_keys();assert(menu_focus==1&&input_lock==-1);
 sheet_focus=menu_focus=0;
 struct input_event16 e={0,0,EV_KEY,K_MENU,1};assert(write(fds[1],&e,sizeof(e))==sizeof(e));poll_keys();assert(screen==SCR_MENU&&input_lock==0);
 reset_console();csw_init();assert(csw.fd>=0&&!csw_live());
 /* An idle automatically opened FIFO must not steal evdev knob input. */
 assert(write(wheel[1],&rotary,sizeof(rotary))==sizeof(rotary));poll_csw();poll_keys();assert(menu_focus==1&&!csw_live());
 /* The first real FIFO record and matching evdev event in the same slice
  * produce one action, while the Bluetooth remote keeps its input lock. */
 sheet_focus=menu_focus=0;int writer=open(CSW_FIFO_PATH,O_WRONLY|O_NONBLOCK);assert(writer>=0);
 u8 b[28];record(b,0,5,2,0,1);assert(write(writer,b,sizeof(b))==sizeof(b));s_close(writer);
 assert(write(wheel[1],&rotary,sizeof(rotary))==sizeof(rotary));poll_csw();poll_keys();assert(menu_focus==1&&input_lock==0&&csw_live());
 e.code=K_UP;assert(write(fds[1],&e,sizeof(e))==sizeof(e));poll_keys();assert(menu_focus==0&&input_lock==0);
 s_close(fds[0]);s_close(fds[1]);s_close(wheel[0]);s_close(wheel[1]);infd[0]=infd[1]=-1;input_lock=-1;reset_console();s_unlink(CSW_FIFO_PATH);
 puts("Console UI: menus, popup choices, complete keyboard/coordinate traversal, shortcuts and simultaneous Bluetooth-remote input passed.");
}
static void fifo_transport(void){
 reset_console();s_unlink(CSW_FIFO_PATH);s_unlink(CSW_LOG_PATH);
 csw_init();assert(csw.fd>=0&&!csw_live());poll_csw();assert(!csw_live());
 struct stat st;assert(stat(CSW_FIFO_PATH,&st)==0&&S_ISFIFO(st.st_mode));
 int writer=open(CSW_FIFO_PATH,O_WRONLY|O_NONBLOCK);assert(writer>=0);
 u8 b[28];record(b,5,0,0,0,0);assert(write(writer,b,9)==9);poll_csw();assert(csw.used==9);s_close(writer);
 poll_csw();assert(csw.fd>=0&&!csw_live());writer=open(CSW_FIFO_PATH,O_WRONLY|O_NONBLOCK);assert(writer>=0);
 assert(write(writer,b+9,19)==19);screen=SCR_MAP;modal=MOD_NONE;poll_csw();assert(screen==SCR_MENU&&csw_live());s_close(writer);
 assert(access(CSW_LOG_PATH,F_OK)<0); /* Debug off really means no log creation. */
 pref.debug=1;control_log("RAW",(s32)0x80000000u,(s32)0x80000000u);assert(access(CSW_LOG_PATH,F_OK)==0);pref.debug=0;remove_stale_logs();assert(access(CSW_LOG_PATH,F_OK)<0);
 reset_console();s_unlink(CSW_FIFO_PATH);write_text(CSW_FIFO_PATH,"not a pipe");csw_init();assert(csw.fd<0&&!csw_live());
 s_unlink(CSW_FIFO_PATH);csw.retry_at=s_now_ms()-1000u;poll_csw();assert(csw.fd>=0&&!csw_live());
 reset_console();s_unlink(CSW_FIFO_PATH);
 puts("Console FIFO: automatic creation, idle/invalid input isolation, late endpoint retry, partial records, writer reconnect, non-FIFO rejection and Debug-only logs passed.");
}
static void temp_packet(u8*b,int value){memset(b,0,11);b[0]=37;b[2]=1;put32(b+3,4);put32(b+7,(u32)value);}
static void temperatures(void){
 memset(&temperature,0,sizeof(temperature));u8 b[12];
 temp_packet(b,18);vehicle_packet(b,11);assert(car.tempok&&car.temp==18);
 car_read_temperature("temp=20\n");assert(car.tempok&&car.temp==20);
 temp_packet(b,21);vehicle_packet(b,11);assert(car.temp==20);
 car_read_temperature("temp=999\n");assert(car.tempok&&car.temp==21);
 temp_packet(b,-1024);vehicle_packet(b,11);assert(!car.tempok);
 temp_packet(b,-60);vehicle_packet(b,11);assert(car.tempok&&car.temp==-60);
 temp_packet(b,85);vehicle_packet(b,11);assert(car.tempok&&car.temp==85);
 temp_packet(b,86);vehicle_packet(b,11);assert(!car.tempok);
 temp_packet(b,10);vehicle_packet(b,10);assert(!car.tempok);b[11]=0;vehicle_packet(b,12);assert(!car.tempok);
 b[0]=40;vehicle_packet(b,11);assert(!car.tempok);temp_packet(b,19);put32(b+3,99);vehicle_packet(b,11);assert(!car.tempok);
 temp_packet(b,17);vehicle_packet(b,11);vehicle_disconnect();assert(!car.tempok);
 car_read_temperature("temp=-4\n");temp_packet(b,10);vehicle_packet(b,11);vehicle_disconnect();assert(car.tempok&&car.temp==-4);
 memset(&temperature,0,sizeof(temperature));temperature_select();
 puts("Temperature providers: Focus priority, VehicleStatus fallback, invalid sentinels/ranges, exact packet validation and disconnect invalidation passed.");
}
static void socket_transport(void){
 vehicle_init();assert(vehicle.enabled&&!temperature.vehicle_connected&&!car.tempok);
 int probe=control_socket();if(probe<0){vehicle_close();puts("Host AF_UNIX SEQPACKET unavailable: transport test skipped; automatic initialization and protocol tests above passed.");return;}s_close(probe);
 int idx=2;assert(vehicle.listen[idx]>=0);char path[108];vehicle_path(3700,path);
 int client=control_socket();assert(client>=0&&control_address(client,path,1)==0);poll_vehicle();
 u8 b[11];temp_packet(b,23);assert(write(client,b,11)==11);poll_vehicle();assert(car.tempok&&car.temp==23);
 s_close(client);poll_vehicle();assert(!car.tempok);
 /* Another listener must not steal or unlink our live endpoint. */
 assert(vehicle_listener(3700)<0);assert(access(path,F_OK)==0);
 vehicle_close();assert(access(path,F_OK)<0);
 puts("VehicleStatus sockets: real seqpacket accept/read/disconnect and live-endpoint ownership passed.");
}
int main(void){console_protocol();console_ui();fifo_transport();temperatures();socket_transport();return 0;}
