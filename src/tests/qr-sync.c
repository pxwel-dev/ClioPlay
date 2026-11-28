#define HOST_TEST
#ifndef CLIO_HOST_ROOT
#define CLIO_HOST_ROOT "/tmp/clio_qrs_test_"
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define main clio_host_main
#include "../clio843/src/clio040.c"
#undef main

static int b64url_val(char c){
 if(c>='A'&&c<='Z')return c-'A';
 if(c>='a'&&c<='z')return c-'a'+26;
 if(c>='0'&&c<='9')return c-'0'+52;
 if(c=='-')return 62;
 if(c=='_')return 63;
 return -1;
}

static int b64url_decode(const char* in, u8* out, int cap){
 int n = 0;
 u32 acc = 0; int bits = 0;
 for(; *in; in++){
  int v = b64url_val(*in);
  assert(v >= 0);
  acc = (acc << 6) | (u32)v;
  bits += 6;
  if(bits >= 8){
   bits -= 8;
   assert(n < cap);
   out[n++] = (u8)((acc >> bits) & 0xffu);
  }
 }
 return n;
}

static u16 rd_u16(const u8* p){ return (u16)p[0] | (u16)((u16)p[1] << 8); }
static s16 rd_s16(const u8* p){ return (s16)rd_u16(p); }
static u32 rd_u32(const u8* p){ return (u32)p[0] | ((u32)p[1]<<8) | ((u32)p[2]<<16) | ((u32)p[3]<<24); }
static s32 rd_s32(const u8* p){ return (s32)rd_u32(p); }

static void dump_fb_ppm(const char* path){
 FILE* f = fopen(path, "wb");
 assert(f != 0);
 fprintf(f, "P6\n%d %d\n255\n", W, H);
 for(int i = 0; i < W * H; i++){
  u16 p = fb[i];
  u8 rgb[3] = {
   (u8)(((p >> 11) & 31) * 255 / 31),
   (u8)(((p >> 5) & 63) * 255 / 63),
   (u8)((p & 31) * 255 / 31)
  };
  fwrite(rgb, 1, 3, f);
 }
 fclose(f);
}

int main(void){
 load_font();
 hw_bluetooth_enabled = 1;
 load_prefs();
 pref.theme = THEME_NIGHT;
 apply_palette(1);
 pref.units = 2; /* Imperial */
 pref.language = 7; /* Polish */
 pref.nav_chimes = 2;
 pref.learn_history = 1;
 pref.bright = 75;

 /* Set map coverage & region ISO. */
 mapok = 1;
 atlas_ready = 0;
 mh.minlat = 5000000; mh.maxlat = 5880000;
 mh.minlon = -620000; mh.maxlon = 180000;
 region_iso[0] = 'G'; region_iso[1] = 'B'; region_iso[2] = 0;
 region_flags = 1;

 /* 1. Connect QR URL & Version check (must fit <= V5). */
 assert(qr_connect_ensure());
 assert(qr_connect_mat.version >= 4 && qr_connect_mat.version <= 5);
 assert(strstr(qr_connect_url, "https://pxwel-dev.github.io/ClioPlay-Link/#q=C1.") == qr_connect_url);
 assert(strstr(qr_connect_url, ".GB.21.5000.5880.-620.180") != 0);

 /* 2. Populate rich Favourites, Active Multi-Stop Trip, Recents, and Mobility/ML data. */
 nfaves = 4;
 faves[0].lat = 5499000; faves[0].lon = -160000; copystr(faves[0].name, sizeof(faves[0].name), "Home");
 faves[1].lat = 5496810; faves[1].lon = -161770; copystr(faves[1].name, sizeof(faves[1].name), "Café Łódź");
 faves[2].lat = 5501450; faves[2].lon = -169420; copystr(faves[2].name, sizeof(faves[2].name), "Kingston Park");
 faves[3].lat = 5497700; faves[3].lon = -161100; copystr(faves[3].name, sizeof(faves[3].name), "Quayside");

 nrecents = 3;
 recents[0] = faves[1];
 recents[1] = faves[2];
 recents[2].lat = 5497400; recents[2].lon = -162900; copystr(recents[2].name, sizeof(recents[2].name), "Eldon Square");

 memzero(&trip, sizeof(trip));
 trip.active = 1; trip.count = 3; trip.active_idx = 0;
 trip.target[0].lat = 5501450; trip.target[0].lon = -169420; copystr(trip.target[0].name, sizeof(trip.target[0].name), "Kingston Park");
 trip.target[1].lat = 5497700; trip.target[1].lon = -161100; copystr(trip.target[1].name, sizeof(trip.target[1].name), "Quayside");
 trip.target[2].lat = 5496810; trip.target[2].lon = -161770; copystr(trip.target[2].name, sizeof(trip.target[2].name), "Café Łódź");
 route_status = R_READY;
 route_n = 2; route_head = 0;
 route_lat[0] = 5498200; route_lon[0] = -165638;
 route_lat[1] = 5501450; route_lon[1] = -169420;
 route_result_m = 4200;
 route_result_cost = 3600; /* 360 s */
 trip_legs[1].status = TLEG_READY; trip_legs[1].dist_m = 6800; trip_legs[1].cost_ds = 5400;
 trip_legs[2].status = TLEG_READY; trip_legs[2].dist_m = 2100; trip_legs[2].cost_ds = 2400;

 /* Mobility + ML state. */
 mob_model_reset();
 ml_reset();
 mob.serial = 42;
 mob.n = 12;
 mob.head = 12;
 for(unsigned i = 0; i < 12; i++){
  struct MobJourney* j = &mob.journey[i];
  j->origin = (struct MobPoint){5499000, -160000};
  j->dest = (i % 3 == 0) ? (struct MobPoint){5496810, -161770} : (struct MobPoint){5501450, -169420};
  j->depart = mob_epoch(2026, 9, 14 + (int)(i % 7), 8 + (int)(i % 10), 15, 0);
  j->serial = i + 1;
  j->distance = 5000u + i * 1000u;
  j->duration = 600u + i * 60u;
 }
 for(unsigned i = 0; i < 35; i++){
  mob.road[i].key = 100u + i;
  mob.road[i].count = 3;
  mob.road[i].mean_ds = 120;
 }
 mob_ml.road_cls[1].count = 12; mob_ml.road_cls[1].ratio_q8 = 240; /* motorway faster */
 mob_ml.road_cls[2].count = 10; mob_ml.road_cls[2].ratio_q8 = 256; /* A-road baseline */
 mob_ml.road_cls[4].count = 8;  mob_ml.road_cls[4].ratio_q8 = 280; /* B-road calmer */
 mob_ml.road_cls[6].count = 15; mob_ml.road_cls[6].ratio_q8 = 270; /* Local */
 mob_ml.road_cls[2].cold_count = 6; mob_ml.road_cls[2].cold_ratio_q8 = 290;
 mob_ml.parking[0].count = 4; mob_ml.parking[0].target = faves[1].lat ? (struct MobPoint){5496810,-161770} : (struct MobPoint){0,0};
 mob_ml.stops[0].count = 3;
 mob_ml.hazards[0].key = 555; mob_ml.hazards[0].lat = 5498000; mob_ml.hazards[0].lon = -163000;
 mob_ml.hazards[0].count = 5; mob_ml.hazards[0].cold_count = 2;
 mob_ml.bright_day = 85; mob_ml.bright_day_n = 5;
 mob_ml.bright_night = 30; mob_ml.bright_night_n = 4;

 /* 3. Start floating QR pill via CLP3:Q:S and verify frame stream + exact binary reassembly! */
 assert(phone_qr_command('S', 0) == 1);
 assert(qr_pill_active == 1);
 assert(qr_pill_total >= 4 && qr_pill_total <= QR_SYNC_MAX_FRAMES);
 assert(qr_pill_mat.version <= 5);

 u8 reassembled[QR_SYNC_MAX_BYTES];
 int rlen = 0;
 for(int f = 0; f < qr_pill_total; f++){
  if(f > 0){
   assert(phone_qr_command('A', (u32)f) == 1);
   assert(qr_pill_bt_ack == 1);
   assert(qr_pill_idx == f);
  }
  assert(qr_pill_mat.version <= 5);
  /* Verify header format CS1:<sid>:<idx>/<tot>:<b64> */
  const char* p = qr_pill_frame_text;
  assert(strncmp(p, "CS1:", 4) == 0);
  const char* c1 = strchr(p + 4, ':'); assert(c1 != 0);
  const char* sl = strchr(c1 + 1, '/'); assert(sl != 0);
  const char* c2 = strchr(sl + 1, ':'); assert(c2 != 0);
  assert(atoi(c1 + 1) == f);
  assert(atoi(sl + 1) == qr_pill_total);
  int dec = b64url_decode(c2 + 1, reassembled + rlen, QR_SYNC_MAX_BYTES - rlen);
  rlen += dec;
 }
 assert(rlen == qr_sync_len);
 assert(memcmp(reassembled, qr_sync_buf, (size_t)rlen) == 0);

 /* Verify CRC-16-CCITT trailer. */
 u16 got_crc = rd_u16(reassembled + rlen - 2);
 u16 exp_crc = qr_crc16(reassembled, rlen - 2);
 assert(got_crc == exp_crc);

 /* Parse header & TLV sections. */
 assert(reassembled[0] == 'C' && reassembled[1] == 'S' && reassembled[2] == 1);
 assert((reassembled[3] & 7u) == 7u); /* mapok | imperial | learn_history */
 assert(reassembled[7] == 'G' && reassembled[8] == 'B');
 assert(rd_s16(reassembled + 9) == 5000 && rd_s16(reassembled + 11) == 5880);
 assert(rd_s16(reassembled + 13) == -620 && rd_s16(reassembled + 15) == 180);
 u8 exp_prefs[12]; clio_prefs_pack(exp_prefs);
 assert(memcmp(reassembled + 17, exp_prefs, 12) == 0);

 int pos = 29, payload_end = rlen - 2;
 int seen_faves = 0, seen_trip = 0, seen_recents = 0, seen_insights = 0;
 while(pos + 3 <= payload_end){
  u8 tag = reassembled[pos];
  u16 blen = rd_u16(reassembled + pos + 1);
  const u8* body = reassembled + pos + 3;
  assert(pos + 3 + blen <= payload_end);
  if(tag == 0x01){
   seen_faves = 1;
   assert(body[0] == 4);
  }else if(tag == 0x02){
   seen_trip = 1;
   assert(body[0] == 3);
   u32 tot_m = rd_u32(body + 1);
   u32 tot_s = rd_u32(body + 5);
   assert(tot_m == 4200u + 6800u + 2100u);
   assert(tot_s == (3600u + 5400u + 2400u) / 10u);
  }else if(tag == 0x03){
   seen_recents = 1;
   assert(body[0] == 3);
  }else if(tag == 0x04){
   seen_insights = 1;
   assert(rd_u16(body) == 12);
   assert(rd_u32(body + 10) == 16000u); /* longest = 5000 + 11*1000 */
   assert(body[40] == qr_pace_pct(240)); /* motorway pace */
   assert(body[41] == 100); /* A-road pace */
   assert(body[44] == qr_pace_pct(290)); /* cold pace */
   assert(rd_u16(body + 45) == 35); /* learned_roads */
   assert(body[47] == 1 && body[48] == 1 && body[49] == 1); /* park, stop, haz */
   assert(body[50] == 85 && body[51] == 30); /* bright day/night */
   assert(body[52] == 2); /* 2 clustered destinations */
  }
  pos += 3 + blen;
 }
 assert(pos == payload_end);
 assert(seen_faves && seen_trip && seen_recents && seen_insights);

 /* Optional visual + CoreImage PPM dump of floating pill! */
 const char* dump_dir = getenv("QR_DUMP_DIR");
 if(dump_dir && dump_dir[0]){
  char path[256];
  screen = SCR_MAP; modal = MOD_NONE; sheet_page = P_CLOSED;
  assert(phone_qr_command('S', 0) == 1);
  qr_pill_started_ms = hud_now_ms() - 500u;
  memset(fb, 0x10, sizeof(fb));
  qr_pill_draw(hud_now_ms());
  snprintf(path, sizeof(path), "%s/qr_pill_frame0.ppm", dump_dir);
  dump_fb_ppm(path);
  printf("PILL_FRAME0=%s\n", qr_pill_frame_text);
 }

 /* 4. Verify CLP3:Q:D flashes Synced and auto-closes after 900 ms. */
 assert(phone_qr_command('D', 0) == 1);
 assert(qr_pill_active == 2);
 u32 t0 = qr_pill_done_ms;
 qr_pill_service(t0 + 400u);
 assert(qr_pill_active == 2);
 qr_pill_service(t0 + 950u);
 assert(qr_pill_active == 0);

 /* 5. Verify Frame 0 holds STILL before first BT ACK, steps after BT ACK, and clears active Turn HUD. */
 assert(phone_qr_command('S', 0) == 1);
 assert(qr_pill_idx == 0 && qr_pill_bt_ack == 0);
 u32 s0 = qr_pill_started_ms;
 /* Must NEVER auto-advance before ClioPlay Link confirms Frame 0 via CLP3:Q:A! */
 qr_pill_service(s0 + 1600u);
 assert(qr_pill_active == 1 && qr_pill_idx == 0);
 qr_pill_service(s0 + 6000u);
 assert(qr_pill_active == 1 && qr_pill_idx == 0);
 /* Once ClioPlay Link confirms Frame 0 -> requests Frame 1, advance immediately and keep 2.2 s BT-drop fallback. */
 assert(phone_qr_command('A', 1) == 1);
 assert(qr_pill_bt_ack == 1 && qr_pill_idx == 1);
 u32 f1 = qr_pill_frame_ms;
 qr_pill_service(f1 + 1500u);
 assert(qr_pill_idx == 1);
 qr_pill_service(f1 + 2250u);
 assert(qr_pill_idx == (2 % qr_pill_total));
 /* Verify floating QR pill never overlaps HC_TURN, HC_TURN_NEXT (even 43 px roundabout), or HC_SPEED! */
 for(int swap = 0; swap <= 1; swap++){
  pref.bottom_swap = swap;
  struct HudBox qb = qr_pill_box(256);
  int turn_x = swap ? HUD_BOTTOM_MARGIN : HUD_TURN_X;
  int speed_x = swap ? HUD_TURN_X : HUD_BOTTOM_MARGIN;
  /* Vertically clear above HUD_BOTTOM_Y (220) -> zero overlap with HC_TURN and HC_SPEED. */
  assert(qb.y + qb.h <= HUD_BOTTOM_Y - 4);
  /* Horizontally clear of HC_TURN and HC_TURN_NEXT stacked above it! */
  assert(qb.x + qb.w <= turn_x || qb.x >= turn_x + HUD_SIDE_W);
  /* Also clear of HC_SPEED vertically. */
  assert(qb.y + qb.h < HUD_BOTTOM_Y && speed_x >= 0);
 }
 pref.bottom_swap = 0;
 key_handle(K_BACK);
 assert(qr_pill_active == 0);

 /* 6. Verify privacy guard: learn_history=0 omits Section 0x04. */
 pref.learn_history = 0;
 assert(phone_qr_command('S', 0) == 1);
 pos = 29; payload_end = qr_sync_len - 2;
 int saw_04 = 0;
 while(pos + 3 <= payload_end){
  if(qr_sync_buf[pos] == 0x04) saw_04 = 1;
  pos += 3 + rd_u16(qr_sync_buf + pos + 1);
 }
 assert(!saw_04);
 qr_pill_dismiss();

 /* 7. Verify P_PAIR stay-open on BT connect + auto-close on ClioPlay Link Ping! */
 u32 now = hud_now_ms();
 btui.connected = 0;
 copystr(btui.status, sizeof(btui.status), "BT PAIR 0000");
 nbtdev = 1;
 copystr(btdev[0].name, sizeof(btdev[0].name), "PAWEL'S IPHONE");
 copystr(btdev[0].addr, sizeof(btdev[0].addr), "AA:BB:CC:DD:EE:FF");
 btdev[0].connected = 0;
 menu_open(P_PHONE, 0, now);
 menu_pair_before = 0; menu_pair_waited = 1; menu_pair_connected = 0;
 menu_pair_link_seen = phone_link_seen_ms;
 menu_push(P_PAIR, 0, now);
 assert(sheet_page == P_PAIR);
 /* Phone connects over Bluetooth -> P_PAIR stays open showing Connected + Done + QR! */
 btui.connected = 1; btdev[0].connected = 1;
 copystr(btui.device, sizeof(btui.device), "PAWEL'S IPHONE");
 copystr(btui.status, sizeof(btui.status), "BT CONNECTED");
 menu_sync(now + 100u);
 assert(sheet_page == P_PAIR);
 assert(menu_pair_connected == 1);
 assert(menu_count(P_PAIR) == 1);
 if(dump_dir && dump_dir[0]){
  char path[256];
  menu_settle(now + 400u);
  memset(fb, 0x10, sizeof(fb));
  menu_paint(now + 400u);
  snprintf(path, sizeof(path), "%s/qr_pair_connected.ppm", dump_dir);
  dump_fb_ppm(path);
  printf("CONNECT_URL=%s\n", qr_connect_url);
 }
 /* User scans Connect QR -> ClioPlay Link fires CLP3:H Ping -> P_PAIR auto-closes! */
 phone_link_seen_ms = now + 500u;
 menu_sync(now + 500u);
 assert(sheet_page == P_PHONE);

 /* 8. Verify P_PHONE_ACTIONS connected shows expanded QR card. */
 copystr(phone_addr, sizeof(phone_addr), btdev[0].addr);
 menu_push(P_PHONE_ACTIONS, 0, now + 600u);
 struct HudBox ab = menu_box(P_PHONE_ACTIONS);
 assert(ab.w == 384 && ab.h == 166);
 if(dump_dir && dump_dir[0]){
  char path[256];
  menu_settle(now + 1000u);
  memset(fb, 0x10, sizeof(fb));
  menu_paint(now + 1000u);
  snprintf(path, sizeof(path), "%s/qr_phone_actions.ppm", dump_dir);
  dump_fb_ppm(path);
 }

 printf("qr-sync: ALL PASS (sync=%dB, frames=%d, connect_ver=V%d)\n",
        qr_sync_len, qr_pill_total, qr_connect_mat.version);
 return 0;
}
