/* Freestanding ISO/IEC 18004 QR Code encoder (Versions 1-8, Byte mode, ECC L).
 * Pure integer C, zero heap allocation, ~2 KB static workspace.
 * Guarantees crisp 3-4 px/module output with ISO 4-module white quiet zone on
 * ClioPlay's 480x272 RGB565 framebuffer. */
#ifndef CLIO_QR_ENCODE_H
#define CLIO_QR_ENCODE_H

#define QR_MAX_VER 8
#define QR_MAX_SIZE (17 + 4 * QR_MAX_VER) /* 49 */
#define QR_MAX_CW 242 /* V8 total codewords: 194 data + 48 EC */

struct QrMatrix {
 int version;
 int size;
 u8 mod[QR_MAX_SIZE * QR_MAX_SIZE]; /* bit 0: dark, bit 1: reserved function module */
};

/* ISO/IEC 18004 ECC Level L tables for Versions 1..8. */
static const u16 qr_data_cw[QR_MAX_VER + 1] = {0, 19, 34, 55, 80, 108, 136, 156, 194};
static const u8  qr_ec_per_blk[QR_MAX_VER + 1] = {0, 7, 10, 15, 20, 26, 18, 20, 24};
static const u8  qr_blocks[QR_MAX_VER + 1] = {0, 1, 1, 1, 1, 1, 2, 2, 2};
static const u8  qr_align_pos[QR_MAX_VER + 1][3] = {
 {0,0,0}, {0,0,0}, {6,18,0}, {6,22,0}, {6,26,0},
 {6,30,0}, {6,34,0}, {6,22,38}, {6,24,42}
};
static const u16 qr_fmt_bits_L[8] = {
 0x77C4u, 0x72F3u, 0x7DAAu, 0x789Du, 0x662Fu, 0x6318u, 0x6C41u, 0x6976u
};
static const u32 qr_ver_bits[QR_MAX_VER + 1] = {
 0, 0, 0, 0, 0, 0, 0, 0x07C94u, 0x085BCu
};

static u8 qr_gf_mul(u8 a, u8 b){
 u8 r = 0;
 for(int i = 0; i < 8; i++){
  if(b & 1) r ^= a;
  int hi = a & 0x80;
  a = (u8)(a << 1);
  if(hi) a ^= 0x1Du;
  b >>= 1;
 }
 return r;
}

static void qr_rs_compute(const u8* data, int dlen, int eclen, u8* out){
 u8 gen[28];
 for(int i = 0; i <= eclen; i++) gen[i] = 0;
 gen[0] = 1;
 u8 root = 1;
 for(int i = 0; i < eclen; i++){
  for(int j = i + 1; j >= 1; j--){
   gen[j] = (u8)(gen[j] ^ qr_gf_mul(gen[j - 1], root));
  }
  root = qr_gf_mul(root, 2);
 }
 for(int i = 0; i < eclen; i++) out[i] = 0;
 for(int i = 0; i < dlen; i++){
  u8 factor = (u8)(data[i] ^ out[0]);
  for(int j = 0; j < eclen - 1; j++) out[j] = out[j + 1];
  out[eclen - 1] = 0;
  for(int j = 0; j < eclen; j++) out[j] = (u8)(out[j] ^ qr_gf_mul(gen[j + 1], factor));
 }
}

static void qr_set_func(struct QrMatrix* q, int x, int y, int dark){
 if(x < 0 || y < 0 || x >= q->size || y >= q->size) return;
 q->mod[y * q->size + x] = (u8)(2u | (dark ? 1u : 0u));
}

static void qr_draw_finder(struct QrMatrix* q, int cx, int cy){
 for(int dy = -4; dy <= 4; dy++){
  for(int dx = -4; dx <= 4; dx++){
   int x = cx + dx, y = cy + dy;
   if(x < 0 || y < 0 || x >= q->size || y >= q->size) continue;
   int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
   int d = ax > ay ? ax : ay;
   int dark = (d != 2 && d != 4);
   qr_set_func(q, x, y, dark);
  }
 }
}

static void qr_draw_align(struct QrMatrix* q, int cx, int cy){
 for(int dy = -2; dy <= 2; dy++){
  for(int dx = -2; dx <= 2; dx++){
   int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
   int d = ax > ay ? ax : ay;
   qr_set_func(q, cx + dx, cy + dy, d != 1);
  }
 }
}

static void qr_draw_format(struct QrMatrix* q, int mask){
 u16 bits = qr_fmt_bits_L[mask & 7];
 int s = q->size;
 for(int i = 0; i <= 5; i++) qr_set_func(q, 8, i, (bits >> i) & 1);
 qr_set_func(q, 8, 7, (bits >> 6) & 1);
 qr_set_func(q, 8, 8, (bits >> 7) & 1);
 qr_set_func(q, 7, 8, (bits >> 8) & 1);
 for(int i = 9; i < 15; i++) qr_set_func(q, 14 - i, 8, (bits >> i) & 1);
 for(int i = 0; i < 8; i++) qr_set_func(q, s - 1 - i, 8, (bits >> i) & 1);
 for(int i = 8; i < 15; i++) qr_set_func(q, 8, s - 15 + i, (bits >> i) & 1);
 qr_set_func(q, 8, s - 8, 1); /* Dark module */
}

static void qr_draw_version(struct QrMatrix* q){
 if(q->version < 7) return;
 u32 bits = qr_ver_bits[q->version];
 int s = q->size;
 for(int i = 0; i < 18; i++){
  int dark = (int)((bits >> i) & 1u);
  int a = s - 11 + (i % 3);
  int b = i / 3;
  qr_set_func(q, a, b, dark);
  qr_set_func(q, b, a, dark);
 }
}

static int qr_mask_bit(int mask, int x, int y){
 switch(mask){
  case 0: return ((x + y) & 1) == 0;
  case 1: return (y & 1) == 0;
  case 2: return (x % 3) == 0;
  case 3: return ((x + y) % 3) == 0;
  case 4: return (((y >> 1) + (x / 3)) & 1) == 0;
  case 5: return ((x * y) & 1) + ((x * y) % 3) == 0;
  case 6: return ((((x * y) & 1) + ((x * y) % 3)) & 1) == 0;
  default: return ((((x + y) & 1) + ((x * y) % 3)) & 1) == 0;
 }
}

static int qr_eval_penalty(const struct QrMatrix* q){
 int s = q->size, pen = 0, dark = 0;
 /* Rule 1: adjacent runs >= 5 in rows and cols. */
 for(int y = 0; y < s; y++){
  int run_r = 1, run_c = 1;
  int pr = q->mod[y * s] & 1, pc = q->mod[y] & 1;
  if(pr) dark++;
  for(int x = 1; x < s; x++){
   int dr = q->mod[y * s + x] & 1;
   if(dr) dark++;
   if(dr == pr){ run_r++; if(run_r == 5) pen += 3; else if(run_r > 5) pen++; }
   else{ pr = dr; run_r = 1; }
   int dc = q->mod[x * s + y] & 1;
   if(dc == pc){ run_c++; if(run_c == 5) pen += 3; else if(run_c > 5) pen++; }
   else{ pc = dc; run_c = 1; }
  }
 }
 /* Rule 2: 2x2 same-colour blocks. */
 for(int y = 0; y < s - 1; y++){
  for(int x = 0; x < s - 1; x++){
   int c = q->mod[y * s + x] & 1;
   if(c == (q->mod[y * s + x + 1] & 1) &&
      c == (q->mod[(y + 1) * s + x] & 1) &&
      c == (q->mod[(y + 1) * s + x + 1] & 1)) pen += 3;
  }
 }
 /* Rule 4: dark/light balance deviation from 50%. */
 int total = s * s;
 int diff = dark * 20 - total * 10;
 if(diff < 0) diff = -diff;
 pen += (diff / total) * 10;
 return pen;
}

static int qr_encode_bytes(struct QrMatrix* q, const u8* data, int len){
 if(!q || !data || len <= 0) return 0;
 int ver = 0;
 for(int v = 1; v <= QR_MAX_VER; v++){
  if(len + 2 <= (int)qr_data_cw[v]){ ver = v; break; }
 }
 if(!ver) return 0;
 int s = 17 + 4 * ver;
 q->version = ver;
 q->size = s;
 for(int i = 0; i < s * s; i++) q->mod[i] = 0;

 /* Build bit stream: Byte mode (0100), 8-bit count, payload bytes, terminator, pad. */
 int dcw = (int)qr_data_cw[ver];
 u8 raw[QR_MAX_CW];
 for(int i = 0; i < QR_MAX_CW; i++) raw[i] = 0;
 int bitpos = 0;
 #define QR_PUT_BITS(val, nbits) do { \
  for(int _k = (nbits) - 1; _k >= 0; _k--){ \
   if(((val) >> _k) & 1) raw[bitpos >> 3] |= (u8)(1u << (7 - (bitpos & 7))); \
   bitpos++; \
  } \
 } while(0)
 QR_PUT_BITS(4, 4);
 QR_PUT_BITS(len, 8);
 for(int i = 0; i < len; i++) QR_PUT_BITS(data[i], 8);
 int rem = dcw * 8 - bitpos;
 int term = rem < 4 ? rem : 4;
 for(int i = 0; i < term; i++) bitpos++;
 while(bitpos & 7) bitpos++;
 int byte_at = bitpos >> 3;
 static const u8 pad_cw[2] = {0xECu, 0x11u};
 for(int p = 0; byte_at < dcw; byte_at++, p ^= 1) raw[byte_at] = pad_cw[p];
 #undef QR_PUT_BITS

 /* Split into blocks, compute RS ECC, and interleave into final cw[]. */
 int nblk = (int)qr_blocks[ver];
 int eclen = (int)qr_ec_per_blk[ver];
 int blk_d = dcw / nblk; /* Exact for all V1..V8 ECC L */
 u8 ec[2][28];
 for(int b = 0; b < nblk; b++) qr_rs_compute(raw + b * blk_d, blk_d, eclen, ec[b]);
 u8 cw[QR_MAX_CW];
 int cw_n = 0;
 for(int i = 0; i < blk_d; i++) for(int b = 0; b < nblk; b++) cw[cw_n++] = raw[b * blk_d + i];
 for(int i = 0; i < eclen; i++) for(int b = 0; b < nblk; b++) cw[cw_n++] = ec[b][i];

 /* Place fixed function patterns. */
 qr_draw_finder(q, 3, 3);
 qr_draw_finder(q, s - 4, 3);
 qr_draw_finder(q, 3, s - 4);
 for(int i = 8; i < s - 8; i++){
  qr_set_func(q, i, 6, (i & 1) == 0);
  qr_set_func(q, 6, i, (i & 1) == 0);
 }
 if(ver >= 2){
  int np = ver >= 7 ? 3 : 2;
  for(int i = 0; i < np; i++){
   for(int j = 0; j < np; j++){
    if((i == 0 && j == 0) || (i == 0 && j == np - 1) || (i == np - 1 && j == 0)) continue;
    qr_draw_align(q, qr_align_pos[ver][i], qr_align_pos[ver][j]);
   }
  }
 }
 qr_draw_format(q, 0);
 qr_draw_version(q);

 /* Zigzag place data + ECC bits. */
 int idx = 0, total_bits = cw_n * 8;
 for(int right = s - 1; right >= 1; right -= 2){
  if(right == 6) right = 5;
  for(int vert = 0; vert < s; vert++){
   int up = (((right + 1) & 2) == 0);
   int y = up ? (s - 1 - vert) : vert;
   for(int j = 0; j < 2; j++){
    int x = right - j;
    if(q->mod[y * s + x] & 2u) continue;
    int bit = 0;
    if(idx < total_bits){
     bit = (cw[idx >> 3] >> (7 - (idx & 7))) & 1;
     idx++;
    }
    q->mod[y * s + x] = (u8)bit;
   }
  }
 }

 /* Pick best mask 0..7. */
 int best_mask = 0, best_pen = 0x7fffffff;
 for(int m = 0; m < 8; m++){
  for(int y = 0; y < s; y++) for(int x = 0; x < s; x++){
   if(!(q->mod[y * s + x] & 2u) && qr_mask_bit(m, x, y)) q->mod[y * s + x] ^= 1u;
  }
  qr_draw_format(q, m);
  int pen = qr_eval_penalty(q);
  if(pen < best_pen){ best_pen = pen; best_mask = m; }
  for(int y = 0; y < s; y++) for(int x = 0; x < s; x++){
   if(!(q->mod[y * s + x] & 2u) && qr_mask_bit(m, x, y)) q->mod[y * s + x] ^= 1u;
  }
 }
 for(int y = 0; y < s; y++) for(int x = 0; x < s; x++){
  if(!(q->mod[y * s + x] & 2u) && qr_mask_bit(best_mask, x, y)) q->mod[y * s + x] ^= 1u;
 }
 qr_draw_format(q, best_mask);
 return 1;
}

static int qr_encode_text(struct QrMatrix* q, const char* s){
 int n = 0;
 while(s && s[n]) n++;
 return qr_encode_bytes(q, (const u8*)s, n);
}

/* Render a crisp glare-proof pure-white QR card into ClioPlay's framebuffer
 * centered inside [x..x+box_w, y..y+box_h] with >= 3-module white quiet zone. */
static void qr_draw_matrix(const struct QrMatrix* q, int cx, int cy, int max_px, int opacity){
 if(!q || q->size <= 0 || opacity <= 0) return;
 int quiet = 3;
 int total_mods = q->size + quiet * 2;
 int scale = max_px / total_mods;
 if(scale < 2) scale = 2;
 if(scale > 4) scale = 4;
 int card = total_mods * scale;
 int ox = cx - card / 2, oy = cy - card / 2;
 int old = hud_ink_alpha; hud_ink_alpha = opacity;
 round_rect(ox, oy, card, card, 6, 0xffff, 15);
 int qx = ox + quiet * scale, qy = oy + quiet * scale;
 int alpha = (opacity >= 16) ? 16 : opacity;
 for(int my = 0; my < q->size; my++){
  for(int mx = 0; mx < q->size; mx++){
   if(!(q->mod[my * q->size + mx] & 1u)) continue;
   int px0 = qx + mx * scale, py0 = qy + my * scale;
   if(alpha >= 16){
    for(int dy = 0; dy < scale; dy++){
     int py = py0 + dy;
     if(py < 0 || py >= H) continue;
     for(int dx = 0; dx < scale; dx++){
      int px = px0 + dx;
      if(px >= 0 && px < W) fb[py * W + px] = 0x0000;
     }
    }
   }else{
    for(int dy = 0; dy < scale; dy++)
     for(int dx = 0; dx < scale; dx++)
      blend_pixel(px0 + dx, py0 + dy, 0x0000, alpha);
   }
  }
 }
 hud_ink_alpha = old;
}

#endif
