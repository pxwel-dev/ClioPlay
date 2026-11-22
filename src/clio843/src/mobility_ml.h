/* Extended bounded, integer-only on-device mobility learning (MML1).
 * Complements mobility.dat (MOB1) without altering existing map or MOB1 formats.
 * Zero heap allocation, zero floating point, FNV-1a checksummed atomic saves,
 * strictly opt-in under pref.learn_history and wiped by Clear driving history.
 */
#ifndef CLIO_MOBILITY_ML_H
#define CLIO_MOBILITY_ML_H

#ifdef HOST_TEST
#define MOB_ML_FILE CLIO_HOST_ROOT "mobility-ml.dat"
#define MOB_ML_TEMP CLIO_HOST_ROOT "mobility-ml.tmp"
#else
#define MOB_ML_FILE "./clio843/mobility-ml.dat"
#define MOB_ML_TEMP "./clio843/mobility-ml.tmp"
#endif

#define MOB_ML_MAGIC 0x314c4d4du /* MML1 */
#define MOB_ML_VER 1u
#define MOB_ML_CLASSES 8
#define MOB_ML_CHAINS 128
#define MOB_ML_STOPS 32
#define MOB_ML_PREF_ARCS 512
#define MOB_ML_ROAD_VAR 1024
#define MOB_ML_POI_AFF 32
#define MOB_ML_PARKING 32
#define MOB_ML_HAZARDS 128
#define MOB_ML_STAGE_ARCS 256
#define MOB_ML_STAGE_BRAKES 64

struct MobClassProfile {
 u32 ratio_q8;
 u16 count, cold_ratio_q8, cold_count, pad;
};
struct MobChain {
 struct MobPoint prev_origin, origin, dest;
 u16 dwell_bucket, count;
 u32 stamp;
};
struct MobStopHabit {
 struct MobPoint origin, dest, stop;
 u16 count, day_mask;
 u32 depart_min, stamp;
 char name[48];
};
struct MobPrefArc {
 u32 key;
 s16 score;
 u16 stamp;
};
struct MobVar {
 u32 key;
 u16 bucket, mad_ds, count, stamp;
};
struct MobPoiAff {
 u32 name_hash;
 s32 lat, lon;
 u8 kind, count;
 u16 stamp;
};
struct MobParking {
 struct MobPoint target, park;
 u16 count, stamp;
};
struct MobHazard {
 u32 key;
 s32 lat, lon;
 u8 count, cold_count;
 u16 stamp;
};
struct MobML {
 u32 magic, ver, serial, last_arrival;
 struct MobPoint last_origin, last_dest;
 struct MobClassProfile road_cls[MOB_ML_CLASSES];
 struct MobChain chains[MOB_ML_CHAINS];
 struct MobStopHabit stops[MOB_ML_STOPS];
 struct MobPrefArc pref_arcs[MOB_ML_PREF_ARCS];
 struct MobVar road_var[MOB_ML_ROAD_VAR];
 struct MobPoiAff poi_aff[MOB_ML_POI_AFF];
 struct MobParking parking[MOB_ML_PARKING];
 struct MobHazard hazards[MOB_ML_HAZARDS];
 signed char zoom_bias_q4[3];
 u8 zoom_samples[3];
 u8 bright_day, bright_night, bright_day_n, bright_night_n, last_valid, pad;
};
static struct MobML mob_ml;
_Static_assert(sizeof(struct MobML) == 26508u, "MobML layout");
static int ml_dirty = 0;
static u32 ml_dirty_at = 0;

/* Session-scoped staging buffers committed only on clean mob_finish(). */
struct MobMlStageCls {u32 sum_q8, cold_sum_q8; u16 count, cold_count;};
static struct MobMlStageCls ml_stage_cls[MOB_ML_CLASSES];
struct MobMlStagePref {u32 key; signed char delta;};
static struct MobMlStagePref ml_stage_pref[MOB_ML_STAGE_ARCS];
static unsigned ml_stage_pref_n;
struct MobMlStageBrake {u32 key; s32 lat, lon; u8 cold;};
static struct MobMlStageBrake ml_stage_brake[MOB_ML_STAGE_BRAKES];
static unsigned ml_stage_brake_n;
#define ML_ZOOM_SETTLE_MS 20000u /* Sustained manual zoom before learning (avoids misclicks/peeks) */
static u8 ml_zoom_nudged[3];
static int ml_zoom_pend_band = -1;
static int ml_zoom_pend_base = -1;
static int ml_zoom_pend_target = -1;
static u32 ml_zoom_pend_since = 0;
static int ml_prev_speed = -1;
static u32 ml_prev_speed_ms = 0;

/* Live guided-trip context for parking/stop/deviation learning. */
static int ml_guided_target_valid = 0;
static struct MobPoint ml_guided_target;

/* Active usual-parking invitation (explicit Yes/No confirmation, never silent). */
static int ml_park_valid = 0;
static struct MobPoint ml_park_point, ml_park_for_target;
static u32 ml_park_serial = 0;
static struct MobPoint ml_park_declined[16];
static unsigned ml_park_declined_n = 0;

/* Active recurring intermediate stop invitation (explicit Yes/No confirmation). */
static int ml_stop_valid = 0;
static struct MobStopHabit ml_stop_habit;
static u32 ml_stop_serial = 0;
static struct MobPoint ml_stop_declined[16];
static unsigned ml_stop_declined_n = 0;

/* Active repeat-slowdown & unguided congestion notice state (once per arc per trip). */
static u32 ml_slowdown_seen[32];
static unsigned ml_slowdown_seen_n = 0;
static u32 ml_slowdown_arc = 0;
static int ml_slowdown_cold = 0;
static u32 ml_congestion_seen[32];
static unsigned ml_congestion_seen_n = 0;

/* Active route ETA variance (deciseconds) & diversion kind (0=traffic, 1=usual). */
static u32 ml_route_var_ds = 0;
static int ml_offer_kind = 0;

static u8 ml_poi_recorded_session = 0;

static void ml_zoom_cancel_override(void){
 ml_zoom_pend_band = -1;
 ml_zoom_pend_base = -1;
 ml_zoom_pend_target = -1;
 ml_zoom_pend_since = 0;
}
static void ml_session_reset(void){
 memzero(ml_stage_cls, sizeof(ml_stage_cls));
 ml_stage_pref_n = 0;
 ml_stage_brake_n = 0;
 memzero(ml_zoom_nudged, sizeof(ml_zoom_nudged));
 ml_zoom_cancel_override();
 ml_poi_recorded_session = 0;
 ml_prev_speed = -1;
 ml_prev_speed_ms = 0;
 ml_park_valid = 0;
 ml_park_declined_n = 0;
 ml_stop_valid = 0;
 ml_stop_declined_n = 0;
 ml_slowdown_seen_n = 0;
 ml_slowdown_arc = 0;
 ml_slowdown_cold = 0;
 ml_congestion_seen_n = 0;
}
static void ml_reset(void){
 memzero(&mob_ml, sizeof(mob_ml));
 mob_ml.magic = MOB_ML_MAGIC;
 mob_ml.ver = MOB_ML_VER;
 for(int i = 0; i < MOB_ML_CLASSES; i++){
  mob_ml.road_cls[i].ratio_q8 = 256u;
  mob_ml.road_cls[i].cold_ratio_q8 = 256u;
 }
 ml_session_reset();
 ml_dirty = 0;
 ml_dirty_at = 0;
 ml_guided_target_valid = 0;
 ml_route_var_ds = 0;
 ml_offer_kind = 0;
}
static int ml_validate(void){
 if(mob_ml.magic != MOB_ML_MAGIC || mob_ml.ver != MOB_ML_VER) return 0;
 if(mob_ml.last_valid > 1u) return 0;
 if(mob_ml.last_valid && (!mob_point_ok(mob_ml.last_origin) || !mob_point_ok(mob_ml.last_dest))) return 0;
 for(int i = 0; i < MOB_ML_CLASSES; i++){
  if(mob_ml.road_cls[i].ratio_q8 < 180u || mob_ml.road_cls[i].ratio_q8 > 512u) return 0;
  if(mob_ml.road_cls[i].cold_ratio_q8 < 180u || mob_ml.road_cls[i].cold_ratio_q8 > 512u) return 0;
 }
 for(int i = 0; i < MOB_ML_CHAINS; i++){
  if(mob_ml.chains[i].count){
   if(mob_ml.chains[i].dwell_bucket >= 4u) return 0;
   if(!mob_point_ok(mob_ml.chains[i].prev_origin) || !mob_point_ok(mob_ml.chains[i].origin) || !mob_point_ok(mob_ml.chains[i].dest)) return 0;
  }
 }
 for(int i = 0; i < MOB_ML_STOPS; i++){
  if(mob_ml.stops[i].count){
   if(!mob_point_ok(mob_ml.stops[i].origin) || !mob_point_ok(mob_ml.stops[i].dest) || !mob_point_ok(mob_ml.stops[i].stop)) return 0;
   mob_ml.stops[i].name[47] = 0;
  }
 }
 for(int i = 0; i < MOB_ML_ROAD_VAR; i++){
  if(mob_ml.road_var[i].key && (mob_ml.road_var[i].bucket >= 84u || mob_ml.road_var[i].mad_ds > 36000u)) return 0;
 }
 for(int i = 0; i < MOB_ML_PARKING; i++){
  if(mob_ml.parking[i].count && (!mob_point_ok(mob_ml.parking[i].target) || !mob_point_ok(mob_ml.parking[i].park))) return 0;
 }
 for(int i = 0; i < MOB_ML_HAZARDS; i++){
  if(mob_ml.hazards[i].key && !mob_point_ok((struct MobPoint){mob_ml.hazards[i].lat, mob_ml.hazards[i].lon})) return 0;
 }
 for(int i = 0; i < 3; i++){
  if(mob_ml.zoom_bias_q4[i] < -16 || mob_ml.zoom_bias_q4[i] > 16) return 0;
 }
 if(mob_ml.bright_day > 100u || mob_ml.bright_night > 100u) return 0;
 return 1;
}
static void ml_load(void){
 ml_reset();
 long fd = s_open(MOB_ML_FILE, O_RDONLY_, 0);
 if(fd < 0) return;
 u32 check = 0; u8 extra = 0;
 int ok = mob_read_all((int)fd, &mob_ml, sizeof(mob_ml)) &&
          mob_read_all((int)fd, &check, 4) &&
          s_read((int)fd, &extra, 1) == 0;
 s_close((int)fd);
 if(!ok || check != mob_hash(2166136261u, &mob_ml, sizeof(mob_ml)) || !ml_validate()){
  ml_reset();
 }
}
static int ml_save(void){
 mob_ml.magic = MOB_ML_MAGIC;
 mob_ml.ver = MOB_ML_VER;
 mob_ml.serial = mob.serial;
 for(int i = 0; i < MOB_ML_CLASSES; i++){
  if(!mob_ml.road_cls[i].ratio_q8) mob_ml.road_cls[i].ratio_q8 = 256u;
  if(!mob_ml.road_cls[i].cold_ratio_q8) mob_ml.road_cls[i].cold_ratio_q8 = 256u;
 }
 u32 check = mob_hash(2166136261u, &mob_ml, sizeof(mob_ml));
 long fd = s_open(MOB_ML_TEMP, O_WRONLY_|O_CREAT_|O_TRUNC_, 0666);
 if(fd < 0) return 0;
 int ok = mob_write_all((int)fd, &mob_ml, sizeof(mob_ml)) &&
          mob_write_all((int)fd, &check, 4);
 s_close((int)fd);
 if(ok){ s_sync(); ok = s_rename(MOB_ML_TEMP, MOB_ML_FILE) >= 0; }
 if(!ok) s_unlink(MOB_ML_TEMP);
 else ml_dirty = 0;
 return ok;
}
static void ml_mark_dirty(u32 now){
 ml_dirty = 1;
 ml_dirty_at = now;
}
static void ml_poll_zoom_override(u32 now);
static void ml_dirty_service(u32 now){
 ml_poll_zoom_override(now);
 if(ml_dirty && pref.learn_history && now - ml_dirty_at >= 3000u){
  (void)ml_save();
 }
}
static void ml_clear(void){
 ml_reset();
 s_unlink(MOB_ML_FILE);
 s_unlink(MOB_ML_TEMP);
}

/* ---------- 1. Cold-weather helper & Per-Class ETA Calibration ---------- */
static int ml_is_cold(void){
 return car.tempok && car_temp_valid(car.temp) && car.temp <= 3;
}
static void ml_stage_class(unsigned cls, int roundabout, u32 base_ds, u32 actual_ds, int cold){
 if(!mob_recording || !mob_started || base_ds < 20u || base_ds > 36000u || actual_ds < 10u || actual_ds > 36000u) return;
 unsigned idx = roundabout ? 0u : (cls < MOB_ML_CLASSES ? cls : 0u);
 if(!roundabout && (idx == 0u || idx >= MOB_ML_CLASSES)) return;
 u32 q8 = (actual_ds * 256u + base_ds / 2u) / base_ds;
 if(q8 < 180u) q8 = 180u;
 if(q8 > 512u) q8 = 512u;
 struct MobMlStageCls *s = &ml_stage_cls[idx];
 if(s->count < 200u){ s->sum_q8 += q8; s->count++; }
 if(cold && s->cold_count < 200u){ s->cold_sum_q8 += q8; s->cold_count++; }
}
static void ml_commit_classes(void){
 for(int i = 0; i < MOB_ML_CLASSES; i++){
  struct MobMlStageCls *s = &ml_stage_cls[i];
  struct MobClassProfile *p = &mob_ml.road_cls[i];
  if(s->count){
   u32 obs = (s->sum_q8 + s->count / 2u) / s->count;
   u32 n = p->count; if(n > 32u) n = 32u;
   p->ratio_q8 = (p->ratio_q8 * n + obs) / (n + 1u);
   if(p->ratio_q8 < 218u) p->ratio_q8 = 218u;
   if(p->ratio_q8 > 384u) p->ratio_q8 = 384u;
   if(p->count < 60000u) p->count++;
  }
  if(s->cold_count){
   u32 obs = (s->cold_sum_q8 + s->cold_count / 2u) / s->cold_count;
   u32 n = p->cold_count; if(n > 24u) n = 24u;
   u32 prev = p->cold_count ? (u32)p->cold_ratio_q8 : p->ratio_q8;
   u32 next = (prev * n + obs) / (n + 1u);
   if(next < 218u) next = 218u;
   if(next > 420u) next = 420u;
   p->cold_ratio_q8 = (u16)next;
   if(p->cold_count < 60000u) p->cold_count++;
  }
 }
}
/* Returns shrunk Q8 multiplier (256 = 1.00x) for an unfamiliar road class. */
static u32 ml_class_factor_q8(unsigned cls, int roundabout, int cold){
 if(!pref.learn_history) return 256u;
 unsigned idx = roundabout ? 0u : cls;
 if(idx >= MOB_ML_CLASSES) return 256u;
 const struct MobClassProfile *p = &mob_ml.road_cls[idx];
 u32 base_q8 = 256u;
 if(p->count >= 4u){
  u32 n = p->count > 32u ? 32u : (u32)p->count;
  base_q8 = (256u * 8u + p->ratio_q8 * n) / (8u + n);
 }
 if(cold && p->cold_count >= 3u){
  u32 cn = p->cold_count > 16u ? 16u : (u32)p->cold_count;
  u32 c_q8 = (base_q8 * 6u + (u32)p->cold_ratio_q8 * cn) / (6u + cn);
  /* Cold weather never predicts faster than mild baseline. */
  if(c_q8 > base_q8) base_q8 = c_q8;
 }
 if(base_q8 < 230u) base_q8 = 230u;
 if(base_q8 > 384u) base_q8 = 384u;
 return base_q8;
}

/* ---------- 2. Dwell-Time & Trip-Chain Destination Prediction ---------- */
static unsigned ml_dwell_bucket(u32 dwell_sec){
 if(dwell_sec < 900u) return 0u;   /* < 15 min */
 if(dwell_sec < 5400u) return 1u;  /* 15 - 90 min */
 if(dwell_sec < 18000u) return 2u; /* 90 min - 5 h */
 return 3u;                        /* > 5 h */
}
static int ml_current_chain_context(u32 epoch, struct MobPoint *prev_origin, unsigned *dwell_b){
 if(!mob_ml.last_valid || epoch == MOB_UNKNOWN || mob_ml.last_arrival == MOB_UNKNOWN) return 0;
 if(epoch < mob_ml.last_arrival) return 0;
 u32 dwell = epoch - mob_ml.last_arrival;
 if(dwell > 7u * 86400u) return 0;
 if(mob_dist(mob_live.origin, mob_ml.last_dest) > 300u) return 0;
 *prev_origin = mob_ml.last_origin;
 *dwell_b = ml_dwell_bucket(dwell);
 return 1;
}
/* Returns percentage weight multiplier (75..165, 100 = neutral) for mob_predict(). */
static u32 ml_chain_factor(struct MobPoint origin, struct MobPoint dest, u32 epoch){
 struct MobPoint prev_origin; unsigned dwell_b = 0;
 if(!ml_current_chain_context(epoch, &prev_origin, &dwell_b)) return 100u;
 unsigned exact_hits = 0, dwell_hits = 0, other_dwell_hits = 0;
 for(int i = 0; i < MOB_ML_CHAINS; i++){
  const struct MobChain *c = &mob_ml.chains[i];
  if(!c->count) continue;
  if(mob_dist(c->origin, origin) > 250u || mob_dist(c->dest, dest) > MOB_DEST_RADIUS) continue;
  int same_prev = mob_dist(c->prev_origin, prev_origin) <= 300u;
  if(c->dwell_bucket == dwell_b){
   dwell_hits += c->count;
   if(same_prev) exact_hits += c->count;
  }else{
   other_dwell_hits += c->count;
  }
 }
 if(exact_hits >= 2u){
  u32 bonus = exact_hits >= 5u ? 65u : (25u + exact_hits * 8u);
  return 100u + bonus;
 }
 if(dwell_hits >= 2u){
  u32 bonus = dwell_hits >= 5u ? 35u : (10u + dwell_hits * 5u);
  return 100u + bonus;
 }
 if(!dwell_hits && other_dwell_hits >= 4u) return 75u;
 return 100u;
}
static void ml_record_chain(struct MobPoint prev_origin, struct MobPoint origin, struct MobPoint dest, unsigned dwell_b){
 if(mob_dist(origin, dest) < 100u) return;
 int empty = -1, oldest = 0;
 for(int i = 0; i < MOB_ML_CHAINS; i++){
  struct MobChain *c = &mob_ml.chains[i];
  if(!c->count){ if(empty < 0) empty = i; continue; }
  if(c->dwell_bucket == dwell_b &&
     mob_dist(c->prev_origin, prev_origin) <= 300u &&
     mob_dist(c->origin, origin) <= 250u &&
     mob_dist(c->dest, dest) <= MOB_DEST_RADIUS){
   if(c->count < 60000u) c->count++;
   c->stamp = mob.serial;
   return;
  }
  if(c->stamp < mob_ml.chains[oldest].stamp) oldest = i;
 }
 int slot = empty >= 0 ? empty : oldest;
 mob_ml.chains[slot] = (struct MobChain){prev_origin, origin, dest, (u16)dwell_b, 1u, mob.serial};
}
static void ml_record_stop_habit(struct MobPoint origin, struct MobPoint stop, struct MobPoint dest, u32 depart_epoch, const char *name){
 if(mob_dist(origin, dest) < 250u || mob_dist(origin, stop) < 150u || mob_dist(stop, dest) < 150u) return;
 u32 mins = depart_epoch != MOB_UNKNOWN ? (depart_epoch % 86400u) / 60u : 720u;
 u16 day_bit = depart_epoch != MOB_UNKNOWN ? (u16)(1u << mob_day(depart_epoch)) : 0x7fu;
 int empty = -1, oldest = 0;
 for(int i = 0; i < MOB_ML_STOPS; i++){
  struct MobStopHabit *s = &mob_ml.stops[i];
  if(!s->count){ if(empty < 0) empty = i; continue; }
  if(mob_dist(s->origin, origin) <= 300u && mob_dist(s->dest, dest) <= 300u && mob_dist(s->stop, stop) <= 150u){
   if(s->count < 60000u) s->count++;
   s->day_mask |= day_bit;
   s->depart_min = (s->depart_min * 3u + mins) / 4u;
   s->stamp = mob.serial;
   if(name && name[0]) copystr(s->name, sizeof(s->name), name);
   return;
  }
  if(s->stamp < mob_ml.stops[oldest].stamp) oldest = i;
 }
 int slot = empty >= 0 ? empty : oldest;
 struct MobStopHabit *s = &mob_ml.stops[slot];
 memzero(s, sizeof(*s));
 s->origin = origin; s->dest = dest; s->stop = stop;
 s->count = 1u; s->day_mask = day_bit; s->depart_min = mins; s->stamp = mob.serial;
 if(name && name[0]) copystr(s->name, sizeof(s->name), name);
}
static int ml_usual_stop_for(struct MobPoint origin, struct MobPoint dest, u32 epoch, struct MobStopHabit *out){
 if(!pref.learn_history || !pref.destination_predictions) return 0;
 int best = -1; u16 best_c = 0;
 for(int i = 0; i < MOB_ML_STOPS; i++){
  const struct MobStopHabit *s = &mob_ml.stops[i];
  if(s->count < 3u) continue;
  if(mob_dist(s->origin, origin) > 300u || mob_dist(s->dest, dest) > 300u) continue;
  if(epoch != MOB_UNKNOWN){
   unsigned now_min = (epoch % 86400u) / 60u;
   unsigned diff = now_min > s->depart_min ? now_min - s->depart_min : s->depart_min - now_min;
   if(diff > 720u) diff = 1440u - diff;
   if(diff > 180u) continue;
  }
  if(s->count > best_c){ best_c = s->count; best = i; }
 }
 if(best < 0) return 0;
 if(out) *out = mob_ml.stops[best];
 return 1;
}

static int ml_stop_is_declined(struct MobPoint stop){
 for(unsigned i = 0; i < ml_stop_declined_n; i++){
  if(mob_dist(ml_stop_declined[i], stop) <= 150u) return 1;
 }
 return 0;
}
static void ml_check_stop_offer(struct MobPoint origin, struct MobPoint dest, u32 epoch){
 ml_stop_valid = 0;
 struct MobStopHabit h;
 if(ml_usual_stop_for(origin, dest, epoch, &h) && !ml_stop_is_declined(h.stop)){
  ml_stop_habit = h;
  ml_stop_valid = 1;
  if(!++ml_stop_serial) ml_stop_serial++;
 }
}
static void ml_decline_stop_offer(void){
 if(ml_stop_valid && !ml_stop_is_declined(ml_stop_habit.stop) && ml_stop_declined_n < 16u){
  ml_stop_declined[ml_stop_declined_n++] = ml_stop_habit.stop;
 }
 ml_stop_valid = 0;
}

/* ---------- 3. Preferred-Route / "Usual Way" Learning ---------- */
static struct MobPrefArc *ml_pref_find(u32 key, int create){
 if(!key) return 0;
 unsigned at = (key * 2654435761u) & (MOB_ML_PREF_ARCS - 1u);
 struct MobPrefArc *old = 0;
 for(unsigned n = 0; n < 24u; n++){
  struct MobPrefArc *e = &mob_ml.pref_arcs[(at + n) & (MOB_ML_PREF_ARCS - 1u)];
  if(e->key == key) return e;
  if(!e->key){ if(!create) return 0; old = e; break; }
  if(!old || e->stamp < old->stamp) old = e;
 }
 if(!create || !old) return 0;
 memzero(old, sizeof(*old));
 old->key = key;
 return old;
}
static void ml_stage_pref_arc(u32 key, int delta){
 if(!mob_recording || !mob_started || !key || !delta) return;
 for(unsigned i = 0; i < ml_stage_pref_n; i++){
  if(ml_stage_pref[i].key == key){
   /* An explicit avoidance takes precedence over traversal in the same session. */
   if(delta < 0) ml_stage_pref[i].delta = (signed char)delta;
   return;
  }
 }
 if(ml_stage_pref_n < MOB_ML_STAGE_ARCS){
  ml_stage_pref[ml_stage_pref_n++] = (struct MobMlStagePref){key, (signed char)delta};
 }
}
static void ml_commit_pref_arcs(void){
 u16 st = (u16)(mob.serial & 0xffffu);
 for(unsigned i = 0; i < ml_stage_pref_n; i++){
  struct MobPrefArc *e = ml_pref_find(ml_stage_pref[i].key, 1);
  if(!e) continue;
  int sc = e->score;
  /* Gentle decay if unseen for many journeys. */
  u16 age = (u16)(st - e->stamp);
  if(e->stamp && age > 128u) sc /= 2;
  sc += ml_stage_pref[i].delta;
  if(sc > 120) sc = 120;
  if(sc < -120) sc = -120;
  e->score = (s16)sc;
  e->stamp = st ? st : 1u;
 }
}
static int ml_pref_score(u32 key){
 if(!pref.learn_history || !key) return 0;
 const struct MobPrefArc *e = ml_pref_find(key, 0);
 return e ? (int)e->score : 0;
}
static int ml_has_strong_pref(int min_score){
 if(!pref.learn_history) return 0;
 for(int i = 0; i < MOB_ML_PREF_ARCS; i++){
  if(mob_ml.pref_arcs[i].key && mob_ml.pref_arcs[i].score >= min_score) return 1;
 }
 return 0;
}

/* ---------- 4. ETA Reliability / Online MAD Variance ---------- */
static struct MobVar *ml_var_find(u32 key, unsigned bucket, int create){
 if(!key || bucket >= 84u) return 0;
 unsigned at = (key ^ (bucket * 2654435761u)) & (MOB_ML_ROAD_VAR - 1u);
 struct MobVar *old = 0;
 for(unsigned n = 0; n < 24u; n++){
  struct MobVar *v = &mob_ml.road_var[(at + n) & (MOB_ML_ROAD_VAR - 1u)];
  if(v->key == key && v->bucket == bucket) return v;
  if(!v->key){ if(!create) return 0; old = v; break; }
  if(!old || v->stamp < old->stamp) old = v;
 }
 if(!create || !old) return 0;
 memzero(old, sizeof(*old));
 old->key = key;
 old->bucket = (u16)bucket;
 return old;
}
static void ml_commit_road_variance(u32 key, unsigned bucket, u32 prev_mean, u32 obs_ds, int had_prev){
 if(!had_prev || !key || bucket >= 84u) return;
 u32 diff = obs_ds > prev_mean ? obs_ds - prev_mean : prev_mean - obs_ds;
 if(diff > 36000u) diff = 36000u;
 struct MobVar *v = ml_var_find(key, bucket, 1);
 if(!v) return;
 unsigned n = v->count; if(n > 16u) n = 16u;
 u32 next = ((u32)v->mad_ds * n + diff) / (n + 1u);
 if(next > 36000u) next = 36000u;
 v->mad_ds = (u16)next;
 if(v->count < 60000u) v->count++;
 u16 st = (u16)(mob.serial & 0xffffu);
 v->stamp = st ? st : 1u;
}
static u32 ml_road_mad(u32 key, u32 epoch){
 if(!pref.learn_history || !key || epoch == MOB_UNKNOWN) return 0;
 unsigned b = mob_bucket(epoch);
 const struct MobVar *v = ml_var_find(key, b, 0);
 if(v && v->count >= 3u) return (u32)v->mad_ds;
 /* Pool across same 2-hour slot across days if exact day is sparse. */
 u32 sum = 0, cnt = 0;
 for(unsigned day = 0; day < 7u; day++){
  v = ml_var_find(key, day * 12u + (b % 12u), 0);
  if(v && v->count){ sum += (u32)v->mad_ds * v->count; cnt += v->count; }
 }
 return cnt >= 3u ? sum / cnt : 0u;
}

/* ---------- 5. Adaptive Auto-Zoom & Day/Night Brightness ---------- */
/* Three speed regimes aligned with Auto zoom's hysteresis tiers:
 *   0: < 25 mph (Z9-Z7)   1: 25-45 mph (Z6)   2: >= 45 mph (Z5-Z4). */
static int ml_zoom_band(int mph10){
 return mph10 >= 450 ? 2 : (mph10 >= 250 ? 1 : 0);
}
static int ml_zoom_band_for_level(int base_z){
 return base_z <= 5 ? 2 : (base_z == 6 ? 1 : 0);
}
static void ml_note_zoom_override_band(int band, int delta){
 if(!pref.learn_history || !pref.autozoom || !delta || band < 0 || band > 2) return;
 if(ml_zoom_nudged[band]) return;
 ml_zoom_nudged[band] = 1u;
 int cur = mob_ml.zoom_bias_q4[band];
 int target = delta > 0 ? 16 : -16;
 /* Slow integer EWMA (~1/4 step); takes 4 consistent sessions from 0 to cross +-10. */
 cur = cur + (target - cur) / 4;
 if(cur > 16) cur = 16;
 if(cur < -16) cur = -16;
 mob_ml.zoom_bias_q4[band] = (signed char)cur;
 if(mob_ml.zoom_samples[band] < 250u) mob_ml.zoom_samples[band]++;
 ml_mark_dirty(hud_now_ms());
}
static void ml_note_zoom_override(int mph10, int delta){
 ml_note_zoom_override_band(ml_zoom_band(mph10), delta);
}
static void ml_zoom_stage_override(int band, int base_zoom, int target_zoom, u32 now){
 if(!pref.learn_history || !pref.autozoom || band < 0 || band > 2 || ml_zoom_nudged[band]) return;
 if(target_zoom == base_zoom){ ml_zoom_cancel_override(); return; }
 ml_zoom_pend_band = band;
 ml_zoom_pend_base = base_zoom;
 ml_zoom_pend_target = target_zoom;
 ml_zoom_pend_since = now;
}
static void ml_zoom_adjust_override(int target_zoom, u32 now){
 if(ml_zoom_pend_band < 0) return;
 ml_zoom_pend_target = target_zoom;
 ml_zoom_pend_since = now;
}
static void ml_poll_zoom_override(u32 now){
 if(ml_zoom_pend_band < 0) return;
 if(!pref.learn_history || !pref.autozoom || freecam || zoom_override < 0){
  ml_zoom_cancel_override();
  return;
 }
 if(ml_zoom_pend_target == ml_zoom_pend_base) return;
 if((u32)(now - ml_zoom_pend_since) < ML_ZOOM_SETTLE_MS) return;
 int band = ml_zoom_pend_band, delta = ml_zoom_pend_target - ml_zoom_pend_base;
 ml_zoom_cancel_override();
 if(delta) ml_note_zoom_override_band(band, delta > 0 ? 1 : -1);
}
static int ml_autozoom_offset_band(int band){
 if(!pref.learn_history || band < 0 || band > 2) return 0;
 if(mob_ml.zoom_samples[band] < 3u) return 0;
 int bias = mob_ml.zoom_bias_q4[band];
 if(bias >= 10) return 1;
 if(bias <= -10) return -1;
 return 0;
}
static int ml_autozoom_offset(int mph10){
 return ml_autozoom_offset_band(ml_zoom_band(mph10));
}
static void ml_note_brightness(int night, int bright){
 if(!pref.learn_history || bright < 10 || bright > 100) return;
 if(night){
  if(!mob_ml.bright_night_n) mob_ml.bright_night = (u8)bright;
  else mob_ml.bright_night = (u8)((mob_ml.bright_night + bright + 1) / 2);
  if(mob_ml.bright_night_n < 250u) mob_ml.bright_night_n++;
 }else{
  if(!mob_ml.bright_day_n) mob_ml.bright_day = (u8)bright;
  else mob_ml.bright_day = (u8)((mob_ml.bright_day + bright + 1) / 2);
  if(mob_ml.bright_day_n < 250u) mob_ml.bright_day_n++;
 }
 ml_mark_dirty(hud_now_ms());
}
/* Returns learned brightness rounded to 10% step, or 0 if not yet learned. */
static int ml_learned_brightness(int night){
 if(!pref.learn_history) return 0;
 u8 n = night ? mob_ml.bright_night_n : mob_ml.bright_day_n;
 u8 b = night ? mob_ml.bright_night : mob_ml.bright_day;
 if(n < 2u || b < 10u || b > 100u) return 0;
 int r = ((int)b + 5) / 10 * 10;
 if(r < 10) r = 10;
 if(r > 100) r = 100;
 return r;
}

/* ---------- 6. Nearby-POI Affinity & Usual Parking Spot ---------- */
static u32 ml_name_hash(const char *s){
 if(!s || !s[0]) return 0;
 u32 h = 2166136261u;
 while(*s){
  unsigned char c = (unsigned char)*s++;
  if(c >= 'a' && c <= 'z') c = (unsigned char)(c - 32);
  if(c == ' ' && (h == 2166136261u)) continue;
  h = (h ^ c) * 16777619u;
 }
 return h ? h : 1u;
}
static int ml_record_poi_use_nosave(u8 kind, s32 lat, s32 lon, const char *name){
 if(!pref.learn_history || kind < 6u || kind > 8u) return 0;
 u32 nh = (name && name[0] && !streq(name, "FUEL") && !streq(name, "PARKING") && !streq(name, "HOSPITAL")) ? ml_name_hash(name) : 0u;
 int empty = -1, oldest = 0;
 u16 st = (u16)((mob.serial + 1u) & 0xffffu); if(!st) st = 1u;
 for(int i = 0; i < MOB_ML_POI_AFF; i++){
  struct MobPoiAff *a = &mob_ml.poi_aff[i];
  if(!a->count){ if(empty < 0) empty = i; continue; }
  if(a->kind == kind && ((nh && a->name_hash == nh) || mob_dist((struct MobPoint){a->lat, a->lon}, (struct MobPoint){lat, lon}) <= 120u)){
   if(a->count < 250u) a->count++;
   a->lat = lat; a->lon = lon;
   if(nh) a->name_hash = nh;
   a->stamp = st;
   return 1;
  }
  if(a->stamp < mob_ml.poi_aff[oldest].stamp) oldest = i;
 }
 int slot = empty >= 0 ? empty : oldest;
 mob_ml.poi_aff[slot] = (struct MobPoiAff){nh, lat, lon, kind, 1u, st};
 return 1;
}
static void ml_record_poi_use(u8 kind, s32 lat, s32 lon, const char *name){
 if(ml_record_poi_use_nosave(kind, lat, lon, name)){
  ml_poi_recorded_session = 1u;
  (void)ml_save();
 }
}
static unsigned ml_poi_affinity(u8 kind, s32 lat, s32 lon, const char *name){
 if(!pref.learn_history) return 0u;
 u32 nh = (name && name[0]) ? ml_name_hash(name) : 0u;
 unsigned best = 0u;
 for(int i = 0; i < MOB_ML_POI_AFF; i++){
  const struct MobPoiAff *a = &mob_ml.poi_aff[i];
  if(a->count < 2u || a->kind != kind) continue;
  if(mob_dist((struct MobPoint){a->lat, a->lon}, (struct MobPoint){lat, lon}) <= 120u){
   if((unsigned)a->count + 2u > best) best = (unsigned)a->count + 2u;
  }else if(nh && a->name_hash == nh){
   if((unsigned)a->count > best) best = (unsigned)a->count;
  }
 }
 return best;
}
static void ml_record_parking(struct MobPoint target, struct MobPoint park){
 u32 d = mob_dist(target, park);
 if(d < 100u || d > 600u) return;
 int empty = -1, oldest = 0;
 u16 st = (u16)(mob.serial & 0xffffu); if(!st) st = 1u;
 for(int i = 0; i < MOB_ML_PARKING; i++){
  struct MobParking *p = &mob_ml.parking[i];
  if(!p->count){ if(empty < 0) empty = i; continue; }
  if(mob_dist(p->target, target) <= 150u && mob_dist(p->park, park) <= 120u){
   if(p->count < 60000u) p->count++;
   p->park = park;
   p->stamp = st;
   return;
  }
  if(p->stamp < mob_ml.parking[oldest].stamp) oldest = i;
 }
 int slot = empty >= 0 ? empty : oldest;
 mob_ml.parking[slot] = (struct MobParking){target, park, 1u, st};
}
static int ml_park_is_declined(struct MobPoint target){
 for(unsigned i = 0; i < ml_park_declined_n; i++){
  if(mob_dist(ml_park_declined[i], target) <= 150u) return 1;
 }
 return 0;
}
static int ml_find_usual_parking(struct MobPoint target, struct MobPoint *out){
 if(!pref.learn_history || !pref.destination_predictions) return 0;
 int best = -1; u16 best_c = 0;
 for(int i = 0; i < MOB_ML_PARKING; i++){
  const struct MobParking *p = &mob_ml.parking[i];
  if(p->count < 3u) continue;
  if(mob_dist(p->target, target) <= 150u && mob_dist(p->park, target) >= 100u){
   if(p->count > best_c){ best_c = p->count; best = i; }
  }
 }
 if(best >= 0){
  if(out) *out = mob_ml.parking[best].park;
  return 1;
 }
 /* Priority 2: fall back to a habitual Parking POI (visited >= 3 times,
  * guided or unguided, even with no saved Favourite/Recent nearby) sitting
  * 100-600 m from target — unless target is itself a car park or a place the
  * driver already regularly parks directly at (< 100 m), or the car is
  * currently departing from that car park. */
 if(ml_find_poi_at(target.lat, target.lon, 7, 0, 0, 0, 0, 0)) return 0;
 unsigned direct_arrivals = 0;
 for(unsigned i = 0; i < mob.n && i < MOB_JOURNEYS; i++){
  if(mob_dist(mob.journey[i].dest, target) < 100u && ++direct_arrivals >= 2u) return 0;
 }
 int best_poi = -1; u8 best_pc = 0; u32 best_pd = 0xffffffffu;
 int have_car = gps.fix || track_valid;
 struct MobPoint here = have_car ? (struct MobPoint){car_lat(), car_lon()} : (struct MobPoint){0, 0};
 for(int i = 0; i < MOB_ML_POI_AFF; i++){
  const struct MobPoiAff *a = &mob_ml.poi_aff[i];
  if(a->kind != 7u || a->count < 3u) continue;
  struct MobPoint pt = {a->lat, a->lon};
  u32 d = mob_dist(pt, target);
  if(d < 100u || d > 600u) continue;
  if(have_car && mob_dist(pt, here) < 100u) continue;
  if(a->count > best_pc || (a->count == best_pc && d < best_pd)){
   best_pc = a->count; best_pd = d; best_poi = i;
  }
 }
 if(best_poi < 0) return 0;
 if(out) *out = (struct MobPoint){mob_ml.poi_aff[best_poi].lat, mob_ml.poi_aff[best_poi].lon};
 return 1;
}
static void ml_check_parking_offer(s32 target_lat, s32 target_lon){
 ml_park_valid = 0;
 struct MobPoint target = {target_lat, target_lon};
 ml_guided_target = target;
 ml_guided_target_valid = 1;
 struct MobPoint park;
 if(!ml_park_is_declined(target) && ml_find_usual_parking(target, &park)){
  ml_park_for_target = target;
  ml_park_point = park;
  ml_park_valid = 1;
  if(!++ml_park_serial) ml_park_serial++;
 }
}
static void ml_decline_parking_offer(void){
 if(ml_park_valid && !ml_park_is_declined(ml_park_for_target) && ml_park_declined_n < 16u){
  ml_park_declined[ml_park_declined_n++] = ml_park_for_target;
 }
 ml_park_valid = 0;
}

/* ---------- 8. Repeat Hard-Braking / Unmapped Slowdown Detection ---------- */
static void ml_stage_hard_brake(u32 key, s32 lat, s32 lon, int cold){
 if(!mob_recording || !mob_started || !key) return;
 for(unsigned i = 0; i < ml_stage_brake_n; i++){
  if(ml_stage_brake[i].key == key) return;
 }
 if(ml_stage_brake_n < MOB_ML_STAGE_BRAKES){
  ml_stage_brake[ml_stage_brake_n++] = (struct MobMlStageBrake){key, lat, lon, (u8)(cold ? 1u : 0u)};
 }
}
static void ml_observe_decel(u32 now_ms, int speed_mph10, u32 arc_key, s32 lat, s32 lon){
 if(ml_prev_speed >= 0 && now_ms > ml_prev_speed_ms){
  u32 dt = now_ms - ml_prev_speed_ms;
  if(dt >= 800u && dt <= 2200u && ml_prev_speed >= 200 && ml_prev_speed > speed_mph10){
   /* Sharp braking: >= 9.0 mph/s deceleration (~0.41 g). */
   u32 drop = (u32)(ml_prev_speed - speed_mph10);
   if(drop * 1000u >= 90u * dt && arc_key){
    ml_stage_hard_brake(arc_key, lat, lon, ml_is_cold());
   }
  }
 }
 ml_prev_speed = speed_mph10;
 ml_prev_speed_ms = now_ms;
}
static struct MobHazard *ml_hazard_find(u32 key, int create){
 if(!key) return 0;
 int empty = -1, oldest = 0;
 for(int i = 0; i < MOB_ML_HAZARDS; i++){
  struct MobHazard *h = &mob_ml.hazards[i];
  if(!h->key){ if(empty < 0) empty = i; continue; }
  if(h->key == key) return h;
  if(h->stamp < mob_ml.hazards[oldest].stamp) oldest = i;
 }
 if(!create) return 0;
 int slot = empty >= 0 ? empty : oldest;
 memzero(&mob_ml.hazards[slot], sizeof(struct MobHazard));
 mob_ml.hazards[slot].key = key;
 return &mob_ml.hazards[slot];
}
static void ml_commit_hazards(void){
 u16 st = (u16)(mob.serial & 0xffffu); if(!st) st = 1u;
 for(unsigned i = 0; i < ml_stage_brake_n; i++){
  struct MobHazard *h = ml_hazard_find(ml_stage_brake[i].key, 1);
  if(!h) continue;
  h->lat = ml_stage_brake[i].lat;
  h->lon = ml_stage_brake[i].lon;
  if(h->count < 250u) h->count++;
  if(ml_stage_brake[i].cold && h->cold_count < 250u) h->cold_count++;
  h->stamp = st;
 }
}
/* Returns 2 for cold-weather repeat hazard, 1 for general repeat slowdown, 0 otherwise. */
static int ml_hazard_level(u32 key, int cold){
 if(!pref.learn_history || !key) return 0;
 const struct MobHazard *h = ml_hazard_find(key, 0);
 if(!h) return 0;
 if(cold && h->cold_count >= 2u) return 2;
 if(h->count >= 3u) return 1;
 return 0;
}
static int ml_slowdown_already_seen(u32 key){
 for(unsigned i = 0; i < ml_slowdown_seen_n; i++) if(ml_slowdown_seen[i] == key) return 1;
 return 0;
}
static void ml_slowdown_mark_seen(u32 key){
 if(!ml_slowdown_already_seen(key) && ml_slowdown_seen_n < 32u){
  ml_slowdown_seen[ml_slowdown_seen_n++] = key;
 }
}
static int ml_congestion_already_seen(u32 key){
 for(unsigned i = 0; i < ml_congestion_seen_n; i++) if(ml_congestion_seen[i] == key) return 1;
 return 0;
}
static void ml_congestion_mark_seen(u32 key){
 if(!ml_congestion_already_seen(key) && ml_congestion_seen_n < 32u){
  ml_congestion_seen[ml_congestion_seen_n++] = key;
 }
}

/* ---------- Master Session Commit (called from mob_finish) ---------- */
static void ml_commit_session(void){
 ml_commit_classes();
 ml_commit_pref_arcs();
 ml_commit_hazards();
 /* Record dwell-time & trip-chain transition and recurring intermediate stop
  * (resolving a Favourite, mapped POI or Recent name when learned unguided). */
 if(mob_ml.last_valid && mob_live.depart != MOB_UNKNOWN && mob_ml.last_arrival != MOB_UNKNOWN &&
    mob_live.depart >= mob_ml.last_arrival && mob_dist(mob_live.origin, mob_ml.last_dest) <= 300u){
  u32 dwell = mob_live.depart - mob_ml.last_arrival;
  if(dwell <= 7u * 86400u){
   ml_record_chain(mob_ml.last_origin, mob_live.origin, mob_live.dest, ml_dwell_bucket(dwell));
   if(dwell <= 2700u && mob_dist(mob_ml.last_origin, mob_live.dest) >= 250u){
    char stop_name[32];stop_name[0]=0;
    (void)ml_resolve_stop_name(mob_live.origin, stop_name, sizeof(stop_name));
    ml_record_stop_habit(mob_ml.last_origin, mob_live.origin, mob_live.dest, mob_live.depart, stop_name[0] ? stop_name : 0);
   }
  }
 }
 /* Unguided Nearby-POI affinity: record arrivals at a mapped Fuel/Parking/Hospital
  * POI automatically, deduplicated against explicit Trip -> Nearby picks. */
 if(!ml_poi_recorded_session){
  u8 pk = 0; s32 plat = 0, plon = 0; char pname[113];
  if(ml_find_poi_at(mob_live.dest.lat, mob_live.dest.lon, 0, &pk, &plat, &plon, pname, sizeof(pname))){
   (void)ml_record_poi_use_nosave(pk, plat, plon, pname);
  }
 }
 /* Record usual parking spot: guided target directly, or unguided arrival at a
  * mapped Parking POI 100-600 m from a unique saved Favourite/Recent. */
 if(ml_guided_target_valid){
  ml_record_parking(ml_guided_target, mob_live.dest);
  ml_guided_target_valid = 0;
 }else{
  u8 pk = 0; s32 plat = 0, plon = 0; struct MobPoint target;
  if(ml_find_poi_at(mob_live.dest.lat, mob_live.dest.lon, 7, &pk, &plat, &plon, 0, 0) &&
     ml_unguided_parking_target(mob_live.dest, &target)){
   ml_record_parking(target, mob_live.dest);
  }
 }
 mob_ml.last_origin = mob_live.origin;
 mob_ml.last_dest = mob_live.dest;
 mob_ml.last_arrival = mob_live.depart != MOB_UNKNOWN ? sat_add32(mob_live.depart, mob_live.duration) : MOB_UNKNOWN;
 mob_ml.last_valid = 1u;
 (void)ml_save();
 ml_session_reset();
}

#endif
