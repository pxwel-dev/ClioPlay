/* Optical Connect QR & Floating Map QR Sync Engine (ClioPlay v3.0).
 * Pure integer freestanding C, zero heap allocation, bounded static buffers.
 * Encodes:
 *  - Static Connect QR URL for P_PAIR & P_PHONE_ACTIONS (#q=C1...)
 *  - Closed-loop stacked CS1 binary sync bundle triggered via CLP3:Q:S/A/D:
 *      Header + 12-byte prefs.ini + Favourites + Active Trip (with multi-leg ETAs)
 *      + Recents + mobility.h / mobility_ml.h Driving Insights + CRC-16-CCITT. */
#ifndef CLIO_QR_SYNC_H
#define CLIO_QR_SYNC_H

#define QR_SYNC_MAX_BYTES 1400
#define QR_SYNC_CHUNK_BYTES 96
#define QR_SYNC_MAX_FRAMES 15

static struct QrMatrix qr_connect_mat;
static char qr_connect_url[112];

static u8 qr_sync_buf[QR_SYNC_MAX_BYTES];
static int qr_sync_len;
static struct QrMatrix qr_pill_mat;
static char qr_pill_frame_text[160];

static int qr_pill_active; /* 0=closed, 1=showing QR, 2=synced flash */
static int qr_pill_idx;
static int qr_pill_total;
static int qr_pill_bt_ack;
static u8  qr_pill_sid;
static u32 qr_pill_serial;
static u32 qr_pill_started_ms;
static u32 qr_pill_frame_ms;
static u32 qr_pill_activity_ms;
static u32 qr_pill_done_ms;

static u16 qr_crc16(const u8* data, int len){
 u16 crc = 0xFFFFu;
 for(int i = 0; i < len; i++){
  crc ^= (u16)((u16)data[i] << 8);
  for(int b = 0; b < 8; b++){
   if(crc & 0x8000u) crc = (u16)((crc << 1) ^ 0x1021u);
   else crc = (u16)(crc << 1);
  }
 }
 return crc;
}

static void qr_parse_version(u8* maj, u8* min, u8* pat){
 u8 v[3] = {0, 0, 0};
 int part = 0;
 const char* s = CLIOPLAY_VERSION;
 while(*s && part < 3){
  if(*s >= '0' && *s <= '9'){
   unsigned next = (unsigned)v[part] * 10u + (unsigned)(*s - '0');
   v[part] = next > 255u ? 255u : (u8)next;
  }else if(*s == '.') part++;
  s++;
 }
 *maj = v[0]; *min = v[1]; *pat = v[2];
}

static void qr_map_bbox(s16* minlat, s16* maxlat, s16* minlon, s16* maxlon){
 *minlat = *maxlat = *minlon = *maxlon = 0;
 if(!mapok) return;
 s32 s0 = 9000000, n0 = -9000000, w0 = 18000000, e0 = -18000000;
 if(atlas_ready && atlas_nr > 0){
  for(u32 i = 0; i < atlas_nr; i++){
   const struct AtlasRegion* r = &atlas_regions[i];
   if(r->south < s0) s0 = r->south;
   if(r->north > n0) n0 = r->north;
   if(r->west < w0) w0 = r->west;
   if(r->east > e0) e0 = r->east;
  }
 }else{
  s0 = mh.minlat; n0 = mh.maxlat; w0 = mh.minlon; e0 = mh.maxlon;
 }
 if(s0 > n0 || w0 > e0) return;
 s32 a = s0 / 1000, b = n0 / 1000, c = w0 / 1000, d = e0 / 1000;
 if(a < -9000) a = -9000; if(a > 9000) a = 9000;
 if(b < -9000) b = -9000; if(b > 9000) b = 9000;
 if(c < -18000) c = -18000; if(c > 18000) c = 18000;
 if(d < -18000) d = -18000; if(d > 18000) d = 18000;
 *minlat = (s16)a; *maxlat = (s16)b; *minlon = (s16)c; *maxlon = (s16)d;
}

static void qr_connect_build_url(char* out, unsigned cap){
 u8 maj = 0, min = 0, pat = 0;
 qr_parse_version(&maj, &min, &pat);
 s16 la0 = 0, la1 = 0, lo0 = 0, lo1 = 0;
 qr_map_bbox(&la0, &la1, &lo0, &lo1);
 char iso[3];
 iso[0] = (region_iso[0] >= 'A' && region_iso[0] <= 'Z') ? region_iso[0] : '-';
 iso[1] = (region_iso[1] >= 'A' && region_iso[1] <= 'Z') ? region_iso[1] : '-';
 iso[2] = 0;
 char unitpair[3];
 unitpair[0] = (char)('0' + (pref.units % 3));
 unitpair[1] = display_imperial() ? '1' : '0';
 unitpair[2] = 0;
 out[0] = 0;
 appendstr(out, cap, "https://pxwel-dev.github.io/ClioPlay-Send/#q=C1.");
 append_int(out, cap, maj); appendstr(out, cap, ".");
 append_int(out, cap, min); appendstr(out, cap, ".");
 append_int(out, cap, pat); appendstr(out, cap, ".");
 appendstr(out, cap, iso); appendstr(out, cap, ".");
 appendstr(out, cap, unitpair); appendstr(out, cap, ".");
 append_int(out, cap, la0); appendstr(out, cap, ".");
 append_int(out, cap, la1); appendstr(out, cap, ".");
 append_int(out, cap, lo0); appendstr(out, cap, ".");
 append_int(out, cap, lo1);
}

static int qr_connect_ensure(void){
 char next[112];
 qr_connect_build_url(next, sizeof(next));
 if(qr_connect_mat.size > 0 && streq(next, qr_connect_url)) return 1;
 copystr(qr_connect_url, sizeof(qr_connect_url), next);
 return qr_encode_text(&qr_connect_mat, qr_connect_url);
}

/* Safe UTF-8 truncation to <= max_bytes without splitting a multi-byte char. */
static int qr_utf8_trunc_len(const char* s, int max_bytes){
 if(!s || max_bytes <= 0) return 0;
 int len = 0;
 while(s[len] && len < max_bytes) len++;
 if(s[len] == 0) return len;
 /* Step back over any trailing UTF-8 continuation bytes (10xxxxxx). */
 while(len > 0 && ((u8)s[len] & 0xC0u) == 0x80u) len--;
 return len;
}

static void qr_put_u8(u8* dst, int* pos, int cap, u8 v){
 if(*pos + 1 <= cap) dst[(*pos)++] = v;
}
static void qr_put_u16(u8* dst, int* pos, int cap, u16 v){
 if(*pos + 2 <= cap){ dst[(*pos)++] = (u8)(v & 0xffu); dst[(*pos)++] = (u8)(v >> 8); }
}
static void qr_put_s16(u8* dst, int* pos, int cap, s16 v){
 qr_put_u16(dst, pos, cap, (u16)v);
}
static void qr_put_u32(u8* dst, int* pos, int cap, u32 v){
 if(*pos + 4 <= cap){
  dst[(*pos)++] = (u8)(v & 0xffu);
  dst[(*pos)++] = (u8)((v >> 8) & 0xffu);
  dst[(*pos)++] = (u8)((v >> 16) & 0xffu);
  dst[(*pos)++] = (u8)((v >> 24) & 0xffu);
 }
}
static void qr_put_s32(u8* dst, int* pos, int cap, s32 v){
 qr_put_u32(dst, pos, cap, (u32)v);
}
static void qr_put_bytes(u8* dst, int* pos, int cap, const u8* src, int n){
 if(n <= 0 || *pos + n > cap) return;
 for(int i = 0; i < n; i++) dst[(*pos)++] = src[i];
}
static int qr_append_tlv(u8 tag, const u8* body, int blen){
 if(blen < 0 || qr_sync_len + 3 + blen + 2 > QR_SYNC_MAX_BYTES) return 0;
 qr_sync_buf[qr_sync_len++] = tag;
 qr_sync_buf[qr_sync_len++] = (u8)(blen & 0xff);
 qr_sync_buf[qr_sync_len++] = (u8)((blen >> 8) & 0xff);
 for(int i = 0; i < blen; i++) qr_sync_buf[qr_sync_len++] = body[i];
 return 1;
}

/* Resolve a friendly saved/recent name within 150 m for a mobility point. */
static const char* qr_mob_place_name(s32 lat, s32 lon){
 struct MobPoint p = {lat, lon};
 u32 best = 151u;
 const char* found = "";
 for(int i = 0; i < nfaves; i++){
  u32 d = mob_dist(p, (struct MobPoint){faves[i].lat, faves[i].lon});
  if(d < best && faves[i].name[0]){ best = d; found = faves[i].name; }
 }
 if(found[0]) return found;
 for(int i = 0; i < nrecents; i++){
  u32 d = mob_dist(p, (struct MobPoint){recents[i].lat, recents[i].lon});
  if(d < best && recents[i].name[0] && !streq(recents[i].name, "PHONE DESTINATION")){
   best = d; found = recents[i].name;
  }
 }
 return found;
}

static u8 qr_pace_pct(u32 ratio_q8){
 if(ratio_q8 < 128u || ratio_q8 > 640u) return 0;
 u32 pct = (256u * 100u + ratio_q8 / 2u) / ratio_q8;
 if(pct < 50u) pct = 50u;
 if(pct > 200u) pct = 200u;
 return (u8)pct;
}

static u8 qr_class_bucket_pace(int c0, int c1){
 u32 wsum = 0, wtot = 0;
 for(int c = c0; c <= c1 && c < MOB_ML_CLASSES; c++){
  if(mob_ml.road_cls[c].count >= 3u){
   u32 w = mob_ml.road_cls[c].count;
   if(w > 64u) w = 64u;
   wsum += (u32)mob_ml.road_cls[c].ratio_q8 * w;
   wtot += w;
  }
 }
 if(!wtot) return 0;
 return qr_pace_pct((wsum + wtot / 2u) / wtot);
}

static void qr_sync_build_insights(u8* sec, int* slen, int cap){
 int p = 0;
 u16 jcnt = (u16)(mob.n > 65535u ? 65535u : mob.n);
 u32 tot_dist = 0, tot_dur = 0, max_dist = 0;
 u16 day_counts[7] = {0,0,0,0,0,0,0};
 u32 hour2_raw[12] = {0};
 struct { s32 lat, lon; u16 visits; } clust[16];
 int nclust = 0;

 for(unsigned i = 0; i < mob.n && i < MOB_JOURNEYS; i++){
  const struct MobJourney* j = &mob.journey[i];
  if(tot_dist <= 0xffffffffu - j->distance) tot_dist += j->distance;
  else tot_dist = 0xffffffffu;
  if(tot_dur <= 0xffffffffu - j->duration) tot_dur += j->duration;
  else tot_dur = 0xffffffffu;
  if(j->distance > max_dist) max_dist = j->distance;
  if(j->depart != MOB_UNKNOWN){
   /* mob_day: 0=Sat, 1=Sun, 2..6=Mon..Fri -> export 0=Mon..6=Sun. */
   unsigned d = (mob_day(j->depart) + 5u) % 7u;
   if(day_counts[d] < 65535u) day_counts[d]++;
   unsigned h2 = (j->depart % 86400u) / 7200u;
   if(h2 < 12u) hour2_raw[h2]++;
  }
  /* Cluster destination into up to 16 candidate hotspots (150 m radius). */
  int found = -1;
  for(int c = 0; c < nclust; c++){
   if(mob_dist(j->dest, (struct MobPoint){clust[c].lat, clust[c].lon}) <= 150u){
    found = c; break;
   }
  }
  if(found >= 0){
   if(clust[found].visits < 65535u) clust[found].visits++;
  }else if(nclust < 16){
   clust[nclust].lat = j->dest.lat;
   clust[nclust].lon = j->dest.lon;
   clust[nclust].visits = 1;
   nclust++;
  }else{
   int weakest = 0;
   for(int c = 1; c < 16; c++) if(clust[c].visits < clust[weakest].visits) weakest = c;
   if(clust[weakest].visits <= 1u){
    clust[weakest].lat = j->dest.lat;
    clust[weakest].lon = j->dest.lon;
    clust[weakest].visits = 1;
   }
  }
 }

 /* Scale 2-hour counts to u8 (preserving relative shape if any > 255). */
 u32 hmax = 0;
 for(int i = 0; i < 12; i++) if(hour2_raw[i] > hmax) hmax = hour2_raw[i];
 u8 hour2[12];
 for(int i = 0; i < 12; i++){
  if(hmax > 255u) hour2[i] = (u8)((hour2_raw[i] * 255u + hmax / 2u) / hmax);
  else hour2[i] = (u8)hour2_raw[i];
 }

 /* Road-class pace profile & cold-weather pace. */
 u8 cls_pace[4];
 cls_pace[0] = qr_class_bucket_pace(1, 1); /* Motorway */
 cls_pace[1] = qr_class_bucket_pace(2, 3); /* Trunk / Primary (A-roads) */
 cls_pace[2] = qr_class_bucket_pace(4, 5); /* Secondary / Tertiary (B-roads) */
 cls_pace[3] = qr_class_bucket_pace(6, 7); /* Local / Residential */
 u32 cold_sum = 0, cold_tot = 0;
 for(int c = 0; c < MOB_ML_CLASSES; c++){
  if(mob_ml.road_cls[c].cold_count >= 3u){
   u32 w = mob_ml.road_cls[c].cold_count > 32u ? 32u : (u32)mob_ml.road_cls[c].cold_count;
   cold_sum += (u32)mob_ml.road_cls[c].cold_ratio_q8 * w;
   cold_tot += w;
  }
 }
 u8 cold_pace = cold_tot ? qr_pace_pct((cold_sum + cold_tot / 2u) / cold_tot) : 0;

 /* Learned intelligence counts. */
 u16 learned_roads = 0;
 for(unsigned i = 0; i < MOB_TRAFFIC; i++) if(mob.road[i].key && mob.road[i].count) learned_roads++;
 u8 learned_park = 0, learned_stops = 0, learned_haz = 0;
 for(int i = 0; i < MOB_ML_PARKING; i++) if(mob_ml.parking[i].count) learned_park++;
 for(int i = 0; i < MOB_ML_STOPS; i++) if(mob_ml.stops[i].count) learned_stops++;
 for(int i = 0; i < MOB_ML_HAZARDS; i++) if(mob_ml.hazards[i].key && mob_ml.hazards[i].count) learned_haz++;
 u8 bday = mob_ml.bright_day_n >= 3u ? mob_ml.bright_day : 0;
 u8 bnight = mob_ml.bright_night_n >= 3u ? mob_ml.bright_night : 0;

 /* Write 51-byte fixed summary header. */
 qr_put_u16(sec, &p, cap, jcnt);
 qr_put_u32(sec, &p, cap, tot_dist);
 qr_put_u32(sec, &p, cap, tot_dur);
 qr_put_u32(sec, &p, cap, max_dist);
 for(int i = 0; i < 7; i++) qr_put_u16(sec, &p, cap, day_counts[i]);
 qr_put_bytes(sec, &p, cap, hour2, 12);
 qr_put_bytes(sec, &p, cap, cls_pace, 4);
 qr_put_u8(sec, &p, cap, cold_pace);
 qr_put_u16(sec, &p, cap, learned_roads);
 qr_put_u8(sec, &p, cap, learned_park);
 qr_put_u8(sec, &p, cap, learned_stops);
 qr_put_u8(sec, &p, cap, learned_haz);
 qr_put_u8(sec, &p, cap, bday);
 qr_put_u8(sec, &p, cap, bnight);

 /* Sort clusters descending and export Top 5 visited places. */
 for(int i = 0; i < nclust; i++){
  for(int j = i + 1; j < nclust; j++){
   if(clust[j].visits > clust[i].visits){
    s32 la = clust[i].lat, lo = clust[i].lon; u16 v = clust[i].visits;
    clust[i] = clust[j]; clust[j].lat = la; clust[j].lon = lo; clust[j].visits = v;
   }
  }
 }
 int top_n = nclust < 5 ? nclust : 5;
 int top_cnt_pos = p;
 qr_put_u8(sec, &p, cap, 0);
 u8 top_written = 0;
 for(int i = 0; i < top_n; i++){
  const char* nm = qr_mob_place_name(clust[i].lat, clust[i].lon);
  int nlen = qr_utf8_trunc_len(nm, 24);
  if(p + 11 + nlen > cap - 16) break;
  qr_put_s32(sec, &p, cap, clust[i].lat);
  qr_put_s32(sec, &p, cap, clust[i].lon);
  qr_put_u16(sec, &p, cap, clust[i].visits);
  qr_put_u8(sec, &p, cap, (u8)nlen);
  qr_put_bytes(sec, &p, cap, (const u8*)nm, nlen);
  top_written++;
 }
 sec[top_cnt_pos] = top_written;

 /* Top 5 slowdown hotspots from mob_ml.hazards[]. */
 int haz_idx[5], haz_n = 0;
 for(int i = 0; i < MOB_ML_HAZARDS; i++){
  if(!mob_ml.hazards[i].key || mob_ml.hazards[i].count < 2u) continue;
  int pos = haz_n;
  if(haz_n < 5) haz_n++;
  else if(mob_ml.hazards[i].count <= mob_ml.hazards[haz_idx[4]].count) continue;
  else pos = 4;
  haz_idx[pos] = i;
  while(pos > 0 && mob_ml.hazards[haz_idx[pos]].count > mob_ml.hazards[haz_idx[pos - 1]].count){
   int tmp = haz_idx[pos - 1]; haz_idx[pos - 1] = haz_idx[pos]; haz_idx[pos] = tmp;
   pos--;
  }
 }
 int haz_cnt_pos = p;
 qr_put_u8(sec, &p, cap, 0);
 u8 haz_written = 0;
 for(int i = 0; i < haz_n; i++){
  if(p + 10 > cap - 8) break;
  const struct MobHazard* h = &mob_ml.hazards[haz_idx[i]];
  qr_put_s32(sec, &p, cap, h->lat);
  qr_put_s32(sec, &p, cap, h->lon);
  qr_put_u8(sec, &p, cap, h->count);
  qr_put_u8(sec, &p, cap, h->cold_count);
  haz_written++;
 }
 sec[haz_cnt_pos] = haz_written;

 /* Last 6 recent drives walking backwards from mob.head. */
 int drv_cnt_pos = p;
 qr_put_u8(sec, &p, cap, 0);
 u8 drv_written = 0;
 unsigned lim = mob.n < 6u ? mob.n : 6u;
 for(unsigned k = 0; k < lim; k++){
  unsigned idx = (mob.head + MOB_JOURNEYS - 1u - k) % MOB_JOURNEYS;
  const struct MobJourney* j = &mob.journey[idx];
  const char* nm = qr_mob_place_name(j->dest.lat, j->dest.lon);
  int nlen = qr_utf8_trunc_len(nm, 24);
  if(p + 19 + nlen > cap) break;
  qr_put_s32(sec, &p, cap, j->dest.lat);
  qr_put_s32(sec, &p, cap, j->dest.lon);
  qr_put_u32(sec, &p, cap, j->depart);
  qr_put_u32(sec, &p, cap, j->distance);
  qr_put_u16(sec, &p, cap, j->duration > 65535u ? 65535u : (u16)j->duration);
  qr_put_u8(sec, &p, cap, (u8)nlen);
  qr_put_bytes(sec, &p, cap, (const u8*)nm, nlen);
  drv_written++;
 }
 sec[drv_cnt_pos] = drv_written;
 *slen = p;
}

static int qr_sync_build(void){
 qr_sync_len = 0;
 u8 maj = 0, min = 0, pat = 0;
 qr_parse_version(&maj, &min, &pat);
 s16 la0 = 0, la1 = 0, lo0 = 0, lo1 = 0;
 qr_map_bbox(&la0, &la1, &lo0, &lo1);
 u8 flags = (u8)((mapok ? 1u : 0u) | (display_imperial() ? 2u : 0u) |
                 (pref.learn_history ? 4u : 0u) | (clock_valid ? 8u : 0u));
 u8 packed_prefs[12];
 clio_prefs_pack(packed_prefs);

 /* 29-byte header. */
 qr_put_u8(qr_sync_buf, &qr_sync_len, QR_SYNC_MAX_BYTES, 'C');
 qr_put_u8(qr_sync_buf, &qr_sync_len, QR_SYNC_MAX_BYTES, 'S');
 qr_put_u8(qr_sync_buf, &qr_sync_len, QR_SYNC_MAX_BYTES, 1);
 qr_put_u8(qr_sync_buf, &qr_sync_len, QR_SYNC_MAX_BYTES, flags);
 qr_put_u8(qr_sync_buf, &qr_sync_len, QR_SYNC_MAX_BYTES, maj);
 qr_put_u8(qr_sync_buf, &qr_sync_len, QR_SYNC_MAX_BYTES, min);
 qr_put_u8(qr_sync_buf, &qr_sync_len, QR_SYNC_MAX_BYTES, pat);
 qr_put_u8(qr_sync_buf, &qr_sync_len, QR_SYNC_MAX_BYTES, (u8)(region_iso[0] ? region_iso[0] : '-'));
 qr_put_u8(qr_sync_buf, &qr_sync_len, QR_SYNC_MAX_BYTES, (u8)(region_iso[1] ? region_iso[1] : '-'));
 qr_put_s16(qr_sync_buf, &qr_sync_len, QR_SYNC_MAX_BYTES, la0);
 qr_put_s16(qr_sync_buf, &qr_sync_len, QR_SYNC_MAX_BYTES, la1);
 qr_put_s16(qr_sync_buf, &qr_sync_len, QR_SYNC_MAX_BYTES, lo0);
 qr_put_s16(qr_sync_buf, &qr_sync_len, QR_SYNC_MAX_BYTES, lo1);
 qr_put_bytes(qr_sync_buf, &qr_sync_len, QR_SYNC_MAX_BYTES, packed_prefs, 12);

 static u8 sec[640];

 /* Section 0x01: Favourites (up to 25). */
 if(nfaves > 0){
  int p = 1, cap = 560;
  u8 cnt = 0;
  int lim = nfaves < FAVE_MAX ? nfaves : FAVE_MAX;
  for(int i = 0; i < lim; i++){
   int nlen = qr_utf8_trunc_len(faves[i].name, 32);
   if(p + 9 + nlen > cap) break;
   qr_put_s32(sec, &p, cap, faves[i].lat);
   qr_put_s32(sec, &p, cap, faves[i].lon);
   qr_put_u8(sec, &p, cap, (u8)nlen);
   qr_put_bytes(sec, &p, cap, (const u8*)faves[i].name, nlen);
   cnt++;
  }
  sec[0] = cnt;
  if(cnt) (void)qr_append_tlv(0x01, sec, p);
 }

 /* Section 0x02: Active Trip (remaining stops + Leg 0 & future leg ETAs). */
 if(trip.active && trip.count > 0 && trip.active_idx < trip.count){
  int p = 0, cap = 420;
  int rem = trip.count - trip.active_idx;
  if(rem > TRIP_MAX_TARGETS) rem = TRIP_MAX_TARGETS;
  u32 leg0_m = route_status == R_READY ? sat_add32(route_path_m(), atlas_future_m) : 0;
  u32 leg0_ds = route_status == R_READY ? sat_add32(traffic_remaining_cost(), atlas_future_cost) : 0;
  u32 fut_m = trip_plan_future_dist_m();
  u32 fut_ds = trip_plan_future_cost_ds();
  if(fut_ds == ROUTE_INF) fut_ds = 0;
  u32 tot_m = sat_add32(leg0_m, fut_m);
  u32 tot_s = sat_add32(leg0_ds, fut_ds) / 10u;
  qr_put_u8(sec, &p, cap, 0);
  qr_put_u32(sec, &p, cap, tot_m);
  qr_put_u32(sec, &p, cap, tot_s);
  u8 trip_written = 0;
  for(int k = 0; k < rem; k++){
   int ti = trip.active_idx + k;
   u32 lm = 0, ls = 0;
   if(k == 0){ lm = leg0_m; ls = leg0_ds / 10u; }
   else if(trip_legs[ti].status == TLEG_READY){ lm = trip_legs[ti].dist_m; ls = trip_legs[ti].cost_ds / 10u; }
   int nlen = qr_utf8_trunc_len(trip.target[ti].name, 32);
   if(p + 17 + nlen > cap) break;
   qr_put_s32(sec, &p, cap, trip.target[ti].lat);
   qr_put_s32(sec, &p, cap, trip.target[ti].lon);
   qr_put_u32(sec, &p, cap, lm);
   qr_put_u32(sec, &p, cap, ls);
   qr_put_u8(sec, &p, cap, (u8)nlen);
   qr_put_bytes(sec, &p, cap, (const u8*)trip.target[ti].name, nlen);
   trip_written++;
  }
  sec[0] = trip_written;
  if(trip_written) (void)qr_append_tlv(0x02, sec, p);
 }

 /* Section 0x04: Driving Insights (mobility.h + mobility_ml.h) before Recents
  * so insights are guaranteed a budget even with 25 favourites. */
 if(pref.learn_history && mob.n > 0){
  int avail = QR_SYNC_MAX_BYTES - qr_sync_len - 5;
  if(avail > 560) avail = 560;
  if(avail >= 64){
   int slen = 0;
   qr_sync_build_insights(sec, &slen, avail);
   if(slen > 0) (void)qr_append_tlv(0x04, sec, slen);
  }
 }

 /* Section 0x03: Recents (up to 10, trimmed to remaining byte budget). */
 if(nrecents > 0){
  int avail = QR_SYNC_MAX_BYTES - qr_sync_len - 5;
  if(avail > 320) avail = 320;
  if(avail >= 16){
   int p = 1;
   u8 cnt = 0;
   int lim = nrecents < 10 ? nrecents : 10;
   for(int i = 0; i < lim; i++){
    int nlen = qr_utf8_trunc_len(recents[i].name, 28);
    if(p + 9 + nlen > avail) break;
    qr_put_s32(sec, &p, avail, recents[i].lat);
    qr_put_s32(sec, &p, avail, recents[i].lon);
    qr_put_u8(sec, &p, avail, (u8)nlen);
    qr_put_bytes(sec, &p, avail, (const u8*)recents[i].name, nlen);
    cnt++;
   }
   sec[0] = cnt;
   if(cnt) (void)qr_append_tlv(0x03, sec, p);
  }
 }

 /* 2-byte trailing CRC-16-CCITT (LE). */
 u16 crc = qr_crc16(qr_sync_buf, qr_sync_len);
 qr_put_u16(qr_sync_buf, &qr_sync_len, QR_SYNC_MAX_BYTES, crc);
 int total = (qr_sync_len + QR_SYNC_CHUNK_BYTES - 1) / QR_SYNC_CHUNK_BYTES;
 if(total < 1) total = 1;
 if(total > QR_SYNC_MAX_FRAMES) total = QR_SYNC_MAX_FRAMES;
 return total;
}

static int qr_b64url_encode(const u8* in, int len, char* out, int cap){
 static const char tbl[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
 int o = 0;
 for(int i = 0; i < len; i += 3){
  u32 a = in[i];
  u32 b = (i + 1 < len) ? in[i + 1] : 0;
  u32 c = (i + 2 < len) ? in[i + 2] : 0;
  u32 v = (a << 16) | (b << 8) | c;
  if(o + 2 >= cap) break;
  out[o++] = tbl[(v >> 18) & 63u];
  out[o++] = tbl[(v >> 12) & 63u];
  if(i + 1 < len && o + 1 < cap) out[o++] = tbl[(v >> 6) & 63u];
  if(i + 2 < len && o + 1 < cap) out[o++] = tbl[v & 63u];
 }
 out[o] = 0;
 return o;
}

static int qr_pill_encode_frame(int idx){
 if(idx < 0 || idx >= qr_pill_total) return 0;
 int off = idx * QR_SYNC_CHUNK_BYTES;
 int len = qr_sync_len - off;
 if(len <= 0) return 0;
 if(len > QR_SYNC_CHUNK_BYTES) len = QR_SYNC_CHUNK_BYTES;
 static const char hx[17] = "0123456789ABCDEF";
 qr_pill_frame_text[0] = 0;
 char prefix[16];
 prefix[0] = 'C'; prefix[1] = 'S'; prefix[2] = '1'; prefix[3] = ':';
 prefix[4] = hx[(qr_pill_sid >> 4) & 15u];
 prefix[5] = hx[qr_pill_sid & 15u];
 prefix[6] = ':'; prefix[7] = 0;
 appendstr(qr_pill_frame_text, sizeof(qr_pill_frame_text), prefix);
 append_int(qr_pill_frame_text, sizeof(qr_pill_frame_text), idx);
 appendstr(qr_pill_frame_text, sizeof(qr_pill_frame_text), "/");
 append_int(qr_pill_frame_text, sizeof(qr_pill_frame_text), qr_pill_total);
 appendstr(qr_pill_frame_text, sizeof(qr_pill_frame_text), ":");
 int base = slen(qr_pill_frame_text);
 qr_b64url_encode(qr_sync_buf + off, len, qr_pill_frame_text + base, (int)sizeof(qr_pill_frame_text) - base);
 qr_pill_idx = idx;
 return qr_encode_text(&qr_pill_mat, qr_pill_frame_text);
}

static void qr_pill_dismiss(void){
 if(!qr_pill_active) return;
 qr_pill_active = 0;
 qr_pill_bt_ack = 0;
 qr_pill_serial++;
}

static int phone_qr_command(char sub, u32 arg){
 u32 now = hud_now_ms();
 if(sub == 'S'){
  qr_pill_sid++;
  if(!qr_pill_sid) qr_pill_sid = 1;
  qr_pill_total = qr_sync_build();
  if(!qr_pill_encode_frame(0)) return 0;
  qr_pill_active = 1;
  qr_pill_bt_ack = 0;
  qr_pill_started_ms = qr_pill_frame_ms = qr_pill_activity_ms = now;
  qr_pill_serial++;
  return 1;
 }
 if(sub == 'A'){
  if(qr_pill_active != 1 || qr_pill_total <= 0) return 0;
  qr_pill_bt_ack = 1;
  qr_pill_activity_ms = now;
  int next = (int)(arg % (u32)qr_pill_total);
  if(!qr_pill_encode_frame(next)) return 0;
  qr_pill_frame_ms = now;
  qr_pill_serial++;
  return 1;
 }
 if(sub == 'D'){
  if(!qr_pill_active) return 0;
  qr_pill_active = 2;
  qr_pill_done_ms = qr_pill_activity_ms = now;
  qr_pill_serial++;
  return 1;
 }
 return 0;
}

static void qr_pill_service(u32 now){
 if(!qr_pill_active) return;
 if(qr_pill_active == 2){
  if(now - qr_pill_done_ms >= 900u) qr_pill_dismiss();
  return;
 }
 if(now - qr_pill_activity_ms >= 20000u){
  qr_pill_dismiss();
  return;
 }
 /* Auto-cycle fallback when BT ACK is unavailable (e.g. legacy link or BT drop). */
 if(!qr_pill_bt_ack && qr_pill_total > 1 &&
    now - qr_pill_started_ms >= 1500u && now - qr_pill_frame_ms >= 1100u){
  int next = (qr_pill_idx + 1) % qr_pill_total;
  if(qr_pill_encode_frame(next)){
   qr_pill_frame_ms = now;
   qr_pill_serial++;
  }
 }
}

static int qr_pill_animating(u32 now){
 if(!qr_pill_active) return 0;
 if(qr_pill_active == 2) return 1;
 if(now - qr_pill_started_ms < 240u) return 1;
 return 0;
}

static void qr_pill_draw(u32 now){
 if(!qr_pill_active || screen != SCR_MAP || modal != MOD_NONE) return;
 int q = frame_ease(now - qr_pill_started_ms, 180);
 if(q <= 0) return;
 int alpha = (q * 16 + 128) / 256;
 if(alpha < 1) alpha = 1;
 if(alpha > 16) alpha = 16;
 struct HudBox b = {W - 180 - 10, 44, 180, 188, q};
 hud_glass(b, 0);
 int ox = clip_x, oy = clip_y, orr = clip_right, ob = clip_bottom, oa = hud_ink_alpha;
 clip_x = b.x + 2; clip_y = b.y + 2; clip_right = b.x + b.w - 2; clip_bottom = b.y + b.h - 2;
 hud_ink_alpha = alpha;
 if(qr_pill_active == 1){
  box_text(b.x + 8, b.y + 4, b.w - 16, 16, 0, ui_tr("Sync to phone"), C_WHITE);
  qr_draw_matrix(&qr_pill_mat, b.x + b.w / 2, b.y + 98, 153, alpha);
  /* Bottom progress dots (or compact counter when > 9 frames). */
  int dy = b.y + b.h - 11;
  if(qr_pill_total <= 9){
   int span = qr_pill_total * 10 - 4;
   int sx = b.x + (b.w - span) / 2;
   for(int i = 0; i < qr_pill_total; i++){
    u16 col = (i == qr_pill_idx) ? C_CYAN : (i < qr_pill_idx ? C_WHITE : C_BORDER);
    round_rect(sx + i * 10, dy - 3, 6, 6, 3, col, alpha >= 16 ? 15 : alpha);
   }
  }else{
   char prog[16]; prog[0] = 0;
   append_int(prog, sizeof(prog), qr_pill_idx + 1);
   appendstr(prog, sizeof(prog), " / ");
   append_int(prog, sizeof(prog), qr_pill_total);
   box_text(b.x + 8, b.y + b.h - 17, b.w - 16, 13, 0, prog, C_CYAN);
  }
 }else{
  /* Synced completion confirmation flash. */
  int cx = b.x + b.w / 2, cy = b.y + b.h / 2 - 14;
  round_rect(cx - 24, cy - 24, 48, 48, 24, C_CYAN, alpha >= 16 ? 15 : alpha);
  /* Crisp white checkmark inside badge. */
  for(int t = 0; t <= 8; t++){
   for(int d = -1; d <= 1; d++){
    blend_pixel(cx - 10 + t, cy + t + d, 0xffff, alpha >= 16 ? 15 : alpha);
   }
  }
  for(int t = 0; t <= 16; t++){
   for(int d = -1; d <= 1; d++){
    blend_pixel(cx - 2 + t, cy + 8 - t + d, 0xffff, alpha >= 16 ? 15 : alpha);
   }
  }
  box_text(b.x + 8, cy + 36, b.w - 16, 18, 1, ui_tr("Synced"), C_WHITE);
 }
 clip_x = ox; clip_y = oy; clip_right = orr; clip_bottom = ob; hud_ink_alpha = oa;
}

#endif
