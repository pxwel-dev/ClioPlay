/* Offline nearby POI discovery from existing CPM5 Z3/Z5 map tiles.
 * Scans Z5 3x3 around the origin first so dense city-centre POIs dropped by
 * the 32 KiB Z3 tile cap are still found, then expands ring-by-ring at Z3
 * across atlas pack boundaries up to NEARBY_MAX_DIST_M. No heap or map rebuild. */
#ifndef CLIO_NEARBY_POIS_H
#define CLIO_NEARBY_POIS_H

#define NEARBY_MAX 15
#define NEARBY_MAX_DIST_M 12000u
enum {NEARBY_CAT_FUEL=0,NEARBY_CAT_PARKING=1,NEARBY_CAT_HOSPITAL=2,NEARBY_CAT_COUNT=3};
struct NearbyPoi {s32 lat,lon;u32 dist_m;u8 kind;char name[113];};
static struct NearbyPoi nearby_pois[NEARBY_MAX];
static int nearby_count=0,nearby_category=0,nearby_status=0;

static int nearby_kind_for_cat(int cat){
 return cat==NEARBY_CAT_FUEL?6:(cat==NEARBY_CAT_PARKING?7:8);
}
static const char*nearby_cat_title(int cat){
 return cat==NEARBY_CAT_FUEL?"Fuel":(cat==NEARBY_CAT_PARKING?"Parking":"Hospital");
}
static int nearby_origin(s32*lat,s32*lon){
 if(gps.fix||track_valid){*lat=car_lat();*lon=car_lon();return 1;}
 if(lastfix_valid){*lat=lastfix_lat;*lon=lastfix_lon;return 1;}
 return 0;
}
static int nearby_is_generic(const char*s){
 return !s||!s[0]||streq(s,"FUEL")||streq(s,"PARKING")||streq(s,"HOSPITAL");
}
static int nearby_before(const struct NearbyPoi*a,const struct NearbyPoi*b){
 if(a->dist_m!=b->dist_m)return a->dist_m<b->dist_m;
 if(a->lat!=b->lat)return a->lat<b->lat;
 return a->lon<b->lon;
}
static void nearby_offer(s32 lat,s32 lon,u32 dist_m,u8 kind,const char*name){
 if(dist_m>NEARBY_MAX_DIST_M)return;
 for(int i=0;i<nearby_count;i++){
  struct NearbyPoi*e=&nearby_pois[i];
  if(e->kind==kind&&iabs32(lat-e->lat)<=65&&iabs32(lon-e->lon)<=110&&geo_seg_m(lat,lon,e->lat,e->lon)<=65u){
   if(nearby_is_generic(e->name)&&!nearby_is_generic(name))copystr(e->name,sizeof(e->name),name);
   if(dist_m<e->dist_m){
    e->lat=lat;e->lon=lon;e->dist_m=dist_m;
    while(i>0&&nearby_before(&nearby_pois[i],&nearby_pois[i-1])){
     struct NearbyPoi tmp=nearby_pois[i-1];nearby_pois[i-1]=nearby_pois[i];nearby_pois[i]=tmp;i--;
    }
   }
   return;
  }
 }
 struct NearbyPoi p;p.lat=lat;p.lon=lon;p.dist_m=dist_m;p.kind=kind;
 copystr(p.name,sizeof(p.name),name&&name[0]?name:(kind==6?"FUEL":kind==7?"PARKING":"HOSPITAL"));
 int at=0;
 while(at<nearby_count&&!nearby_before(&p,&nearby_pois[at]))at++;
 if(at>=NEARBY_MAX)return;
 if(nearby_count<NEARBY_MAX)nearby_count++;
 for(int i=nearby_count-1;i>at;i--)nearby_pois[i]=nearby_pois[i-1];
 nearby_pois[at]=p;
}
static void nearby_scan_tile(struct Tile*t,int want_kind,s32 olat,s32 olon){
 if(!t||t->len<8u)return;
 const u8*p=t->data,*end=t->data+t->len;
 u16 ng=rd16(p),nl=rd16(p+2);p+=8;
 for(u16 f=0;f<ng&&p+4<=end;f++){int np=p[2];p+=4+(unsigned)np*4u;if(p>end)return;}
 int span=span_for_level((int)t->lvl);
 for(u16 i=0;i<nl&&p+8<=end;i++){
  int kind=p[0];u16 lx=rd16(p+2),ly=rd16(p+4);int tl=p[6];p+=8;
  if(p+tl>end)return;
  if(kind==want_kind&&tl>0&&tl<113){
   char b[113];for(int j=0;j<tl;j++)b[j]=(char)p[j];b[tl]=0;
   s32 lon=(s32)t->tx*span+map_tile_offset(lx,(u32)span);
   s32 lat=(s32)t->ty*span+map_tile_offset(ly,(u32)span);
   nearby_offer(lat,lon,geo_seg_m(olat,olon,lat,lon),(u8)kind,b);
  }
  p+=tl;
 }
}
static void nearby_scan(int cat){
 if(cat<0||cat>=NEARBY_CAT_COUNT)cat=0;
 nearby_category=cat;nearby_count=0;nearby_status=0;
 if(!mapok){nearby_status=1;return;}
 s32 olat=0,olon=0;
 if(!nearby_origin(&olat,&olon)){nearby_status=2;return;}
 int want=nearby_kind_for_cat(cat);
 /* Pass 1: Z5 3x3 captures dense city-centre POIs that may overflow Z3 tiles. */
 if(mh.minlvl<=5&&mh.maxlvl>=5){
  int cx5=tile_div(olon,5),cy5=tile_div(olat,5);
  for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
   if(map_tile_in_pack(5,cx5+dx,cy5+dy))nearby_scan_tile(get_tile(5,cx5+dx,cy5+dy),want,olat,olon);
  }
 }
 /* Pass 2: Z3 ring-by-ring reaches ~10-12 km with early stop once full. */
 int blvl=(mh.minlvl<=3&&mh.maxlvl>=3)?3:(int)mh.maxlvl;
 int cx=tile_div(olon,blvl),cy=tile_div(olat,blvl),bspan=span_for_level(blvl);
 int cq=coslat_q15(olat);
 for(int r=0;r<=4;r++){
  if(r>=2&&nearby_count>=NEARBY_MAX){
   u32 gap=(u32)(r-1)*(u32)bspan;
   u32 ns_m=udiv32(gap*111u,100u);
   u32 ew_u=(u32)lon_scale((s32)gap,cq);
   u32 ew_m=udiv32(ew_u*111u,100u);
   u32 min_ring_m=ns_m<ew_m?ns_m:ew_m;
   if(min_ring_m>nearby_pois[NEARBY_MAX-1].dist_m)break;
  }
  for(int dy=-r;dy<=r;dy++)for(int dx=-r;dx<=r;dx++){
   if(r>0&&dy>-r&&dy<r&&dx>-r&&dx<r)continue;
   if(map_tile_in_pack(blvl,cx+dx,cy+dy))nearby_scan_tile(get_tile((u8)blvl,cx+dx,cy+dy),want,olat,olon);
  }
 }
}
static void append_nearby_distance(char*b,unsigned cap,u32 m){
 if(!display_imperial()){
  if(m<1000u){
   u32 r=udiv32(m+5u,10u)*10u;if(m>0u&&r<10u)r=10u;
   append_int(b,cap,(int)r);appendstr(b,cap," m");return;
  }
  append_metric_distance(b,cap,m,0);return;
 }
 if(m<161u){
  u32 yd=udiv32(m*1094u+500u,1000u);yd=udiv32(yd+5u,10u)*10u;if(m>0u&&yd<10u)yd=10u;
  append_int(b,cap,(int)yd);appendstr(b,cap," yd");return;
 }
 append_miles(b,cap,m);
}

#endif
