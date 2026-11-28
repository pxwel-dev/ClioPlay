/* Native verification of ClioPlay v3.0 CLP3 bidirectional link, atomic trip
 * staging, 12-byte prefs.ini codec, favourites delta sync, pre-GPS phone clock,
 * non-disruptive incoming handoff, cold-GPS queueing, and navigation chimes. */
#define HOST_TEST 1
#define main clio040_host_main
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void write_nav(const char*text){
 FILE*f=fopen(BT_NAV_PATH,"wb");assert(f);
 fwrite(text,1,strlen(text),f);fclose(f);
}
static int read_ack(char*buf,size_t cap){
 FILE*f=fopen(BT_NAV_ACK_PATH,"rb");if(!f)return 0;
 size_t n=fread(buf,1,cap-1,f);fclose(f);buf[n]=0;return n>0;
}
static void reset_state(void){
 memzero(&gps,sizeof(gps));gps.hour=-1;gps.fix=1;gps.lat=5498000;gps.lon=-161000;gps.mph10=0;
 memzero(&trip,sizeof(trip));memzero(&phone_dest,sizeof(phone_dest));
 trip_stage_clear();phone_undo_clear();
 phone_seq_seen_valid=0;phone_clock_valid=0;
 nrecents=nfaves=0;modal=MOD_NONE;screen=SCR_MAP;freecam=0;bt_control_mode=0;
 hw_bluetooth_enabled=1;mapok=0;atlas_ready=0;
 hud_capture_clock=1;hud_capture_ms=10000;
 pref=(struct Prefs){50,0,0,1,0,THEME_AUTO,0,1,1,0,CLIO_DAY_START,CLIO_NIGHT_START,0,0,0,1,1,0,1};
 s_unlink(BT_NAV_PATH);s_unlink(BT_NAV_ACK_PATH);s_unlink(CAMERA_ALERT_PATH);
 menu_reset(hud_now_ms());hud_reset_animations();
}
static void test_prefs_codec(void){
 reset_state();
 pref.bright=80;pref.theme=THEME_NIGHT;pref.units=2;pref.cameras=3;pref.nav_chimes=2;
 pref.autozoom=0;pref.clock_auto=0;pref.top_swap=1;pref.bottom_swap=1;
 pref.learn_history=1;pref.destination_predictions=0;pref.diversions=0;pref.debug=1;
 pref.language=7;pref.gps_hud=1;pref.day_start=450;pref.night_start=1170;
 pref.hoff=-5;pref.moff=-30;
 u8 packed[12];clio_prefs_pack(packed);
 char hex[25];
 for(int i=0;i<12;i++)snprintf(hex+i*2,3,"%02X",packed[i]);
 /* Corrupt one field to prove rejection. */
 u8 bad[12];memcpy(bad,packed,12);bad[0]=2;assert(!clio_prefs_unpack(bad));
 memcpy(bad,packed,12);bad[1]=5;assert(!clio_prefs_unpack(bad));
 /* Reset prefs and restore via CLP3:P packet. */
 pref=(struct Prefs){50,0,0,1,0,THEME_DAY,0,1,1,0,CLIO_DAY_START,CLIO_NIGHT_START,0,0,0,1,1,0,0};
 char pkt[160];snprintf(pkt,sizeof(pkt),"VER=3\nSEQ=10\nACTION=P\nHEX=%s\nNAME=\n",hex);
 write_nav(pkt);poll_phone_destination();phone_destination_apply();
 assert(pref.bright==80&&pref.theme==THEME_NIGHT&&pref.units==2&&pref.cameras==3&&pref.nav_chimes==2);
 assert(!pref.autozoom&&!pref.clock_auto&&pref.top_swap&&pref.bottom_swap);
 assert(pref.learn_history&&!pref.destination_predictions&&!pref.diversions&&pref.debug);
 assert(pref.language==7&&pref.gps_hud&&pref.day_start==450&&pref.night_start==1170);
 assert(pref.hoff==-5&&pref.moff==-30);
 char ack[128];assert(read_ack(ack,sizeof(ack)));
 assert(strstr(ack,"SEQ=10\n")&&strstr(ack,"RES=OK\n")&&strstr(ack,"CHIME=3\n"));
 /* Ensure prefs.ini persistence includes navchimes. */
 save_prefs();pref.nav_chimes=0;load_prefs();assert(pref.nav_chimes==2);
}
static void test_atomic_trip_and_out_of_map(void){
 reset_state();
 /* Simulate installed single-pack bounds around Newcastle. */
 mapok=1;mh.minlat=5480000;mh.maxlat=5520000;mh.minlon=-190000;mh.maxlon=-140000;
 /* Out-of-map CLP3:N must NACK and preserve empty trip. */
 write_nav("VER=3\nSEQ=20\nACTION=N\nLAT=5150740\nLON=-12780\nNAME=London\n");
 poll_phone_destination();phone_destination_apply();
 char ack[128];assert(read_ack(ack,sizeof(ack)));
 assert(strstr(ack,"SEQ=20\n")&&strstr(ack,"RES=NACK\n")&&strstr(ack,"CHIME=0\n"));
 assert(!trip.active&&trip.count==0);
 /* Atomic 3-stop CLP3:T arrives in order 0, 1, 2. */
 write_nav("VER=3\nSEQ=21\nACTION=T\nLAT=5497000\nLON=-161000\nA=0\nB=3\nC=7\nNAME=Stop One\n");
 poll_phone_destination();phone_destination_apply();
 assert(trip_stage.active&&trip_stage.received==1&&trip_stage.total==3&&!trip.active);
 assert(read_ack(ack,sizeof(ack))&&strstr(ack,"SEQ=21\n")&&strstr(ack,"RES=OK\n")&&strstr(ack,"CHIME=0\n"));
 hud_prepare(6,hud_now_ms(),1);
 assert(hud_tween[HC_STATUS].target.flags&(HUD_PROGRESS|HUD_PROGRESS_FILL));
 write_nav("VER=3\nSEQ=22\nACTION=T\nLAT=5498000\nLON=-162000\nA=1\nB=3\nC=7\nNAME=Stop Two\n");
 poll_phone_destination();phone_destination_apply();
 assert(trip_stage.active&&trip_stage.received==2&&!trip.active);
 write_nav("VER=3\nSEQ=23\nACTION=T\nLAT=5499000\nLON=-163000\nA=2\nB=3\nC=7\nNAME=Final Dest\n");
 poll_phone_destination();phone_destination_apply();
 assert(!trip_stage.active&&trip.active&&trip.count==3&&trip.active_idx==0);
 assert(streq(trip.target[0].name,"Stop One")&&streq(trip.target[1].name,"Stop Two")&&streq(trip.target[2].name,"Final Dest"));
 assert(read_ack(ack,sizeof(ack))&&strstr(ack,"SEQ=23\n")&&strstr(ack,"RES=OK\n")&&strstr(ack,"CHIME=3\n"));
 /* Skip next stop via CLP3:C:K. */
 write_nav("VER=3\nSEQ=24\nACTION=C\nSUB=K\nLAT=0\nLON=0\nA=0\nB=0\nC=0\nNAME=\n");
 poll_phone_destination();phone_destination_apply();
 assert(trip.active_idx==1&&read_ack(ack,sizeof(ack))&&strstr(ack,"SEQ=24\n")&&strstr(ack,"RES=OK\n"));
 /* Cancel route via CLP3:C:X. */
 write_nav("VER=3\nSEQ=25\nACTION=C\nSUB=X\nLAT=0\nLON=0\nA=0\nB=0\nC=0\nNAME=\n");
 poll_phone_destination();phone_destination_apply();
 assert(!trip.active&&trip.count==0&&read_ack(ack,sizeof(ack))&&strstr(ack,"SEQ=25\n")&&strstr(ack,"RES=OK\n"));
}
static void test_handoff_undo_and_add_stop(void){
 reset_state();
 set_destination("Original Dest",5497000,-161000);
 /* Incoming CLP3:N while already navigating -> BACK restores original trip. */
 write_nav("VER=3\nSEQ=30\nACTION=N\nLAT=5499000\nLON=-165000\nNAME=Coffee Shop\n");
 poll_phone_destination();phone_destination_apply();
 assert(trip.active&&streq(trip.target[0].name,"Coffee Shop")&&phone_undo_active(hud_now_ms())&&phone_undo_can_add);
 key_handle(K_BACK);
 assert(trip.active&&trip.count==1&&streq(trip.target[0].name,"Original Dest")&&!phone_undo_valid);
 /* Incoming CLP3:N again -> OK converts it into a stop on Original Dest! */
 write_nav("VER=3\nSEQ=31\nACTION=N\nLAT=5499000\nLON=-165000\nNAME=Coffee Shop\n");
 poll_phone_destination();phone_destination_apply();
 assert(phone_undo_active(hud_now_ms())&&phone_undo_can_add);
 key_handle(K_ENTER);
 assert(trip.active&&trip.count==2&&streq(trip.target[0].name,"Coffee Shop")&&streq(trip.target[1].name,"Original Dest")&&!phone_undo_valid);
}
static void test_cold_gps_clock_faves_ping_and_chimes(void){
 reset_state();
 gps.fix=0;gps.hour=-1;
 /* Clock sync works before GPS lock (2026-11-28 14:30:00 UTC = 1795876200, +60 min BST -> 15:30 = 930 min). */
 assert(local_clock_minutes()==-1);
 write_nav("VER=3\nSEQ=40\nACTION=C\nSUB=T\nLAT=0\nLON=0\nA=1795876200\nB=60\nC=0\nNAME=\n");
 poll_phone_destination();phone_destination_apply();
 assert(phone_clock_valid&&local_clock_minutes()==930);
 /* Cold-GPS destination queues cleanly and ACKs OK. */
 write_nav("VER=3\nSEQ=41\nACTION=N\nLAT=5497000\nLON=-161000\nNAME=Home\n");
 poll_phone_destination();phone_destination_apply();
 assert(trip.active&&trip_need_route&&streq(notice_text,ui_tr("Queued - waiting for GPS")));
 /* Favourites delta sync: add, rename in-place, delete, clear. */
 write_nav("VER=3\nSEQ=42\nACTION=F\nLAT=5497000\nLON=-161000\nA=1\nB=0\nC=0\nNAME=Home\n");
 poll_phone_destination();phone_destination_apply();
 assert(nfaves==1&&streq(faves[0].name,"Home"));
 write_nav("VER=3\nSEQ=43\nACTION=F\nLAT=5497002\nLON=-161002\nA=1\nB=0\nC=0\nNAME=My House\n");
 poll_phone_destination();phone_destination_apply();
 assert(nfaves==1&&streq(faves[0].name,"My House"));
 write_nav("VER=3\nSEQ=44\nACTION=F\nLAT=5497001\nLON=-161001\nA=3\nB=0\nC=0\nNAME=\n");
 poll_phone_destination();phone_destination_apply();
 assert(nfaves==0);
 /* Silent heartbeat Ping updates phone_link_seen_ms and ACKs with CHIME=0. */
 phone_link_seen_ms=0;
 write_nav("VER=3\nSEQ=45\nACTION=H\nLAT=0\nLON=0\nA=3\nB=0\nC=0\nNAME=\n");
 poll_phone_destination();phone_destination_apply();
 char ack[128];assert(read_ack(ack,sizeof(ack))&&strstr(ack,"SEQ=45\n")&&strstr(ack,"RES=OK\n")&&strstr(ack,"CHIME=0\n"));
 assert(phone_link_seen_ms==hud_now_ms());
 /* When nav_chimes==0 (Off), CLP3 ACK suppresses chime cleanly. */
 pref.nav_chimes=0;
 write_nav("VER=3\nSEQ=46\nACTION=N\nLAT=5497000\nLON=-161000\nNAME=Silent Dest\n");
 poll_phone_destination();phone_destination_apply();
 assert(read_ack(ack,sizeof(ack))&&strstr(ack,"SEQ=46\n")&&strstr(ack,"RES=OK\n")&&strstr(ack,"CHIME=0\n"));
}
int main(void){
 test_prefs_codec();
 test_atomic_trip_and_out_of_map();
 test_handoff_undo_and_add_stop();
 test_cold_gps_clock_faves_ping_and_chimes();
 puts("CLP3 link: prefs 12B codec, atomic trip staging, out-of-map NACK, Undo/Add-stop handoff, cold-GPS queue, pre-GPS clock, fav delta sync, ping and chimes passed.");
 return 0;
}
