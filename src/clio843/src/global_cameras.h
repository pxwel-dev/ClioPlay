/* CPC1: spatially sorted, bounded I/O, visual camera guidance in the existing
 * road strip and single upcoming-camera map marker. Exact alerts fail closed
 * without GPS and reviewed region policy.
 * Fixed=1, red light=2, average entry/exit pair=3. Heading is TRAFFIC direction. */
struct Camera {s32 lat,lon,endlat,endlon;u32 id;u16 heading,speed;u8 flags,kind;char iso[2];u32 waylo,wayhi;};
static int camerafd=-1,camera_visible=0,camera_average=0;
static u32 camera_recsize=32,camera_count=0,camera_next_ms=0,camera_distance=0,camera_prev_ms=0,camera_entry_ms=0,camera_average_m=0;
static u32 camera_warn_span_m=0,camera_avg_min_dist=0,camera_avg_diverge_ms=0,camera_last_gps_ms=0;
static int camera_average_speed=0,camera_candidate_valid=0,camera_prev_valid=0;
static s32 camera_prev_lat=0,camera_prev_lon=0;
static struct Camera camera_current,camera_candidate;
static u32 camera_candidate_distance=0;
static int load_cameras(void){u8 b[16];camerafd=(int)s_open(CAMERAS_PATH,O_RDONLY_,0);if(camerafd<0)return 0;if(s_read(camerafd,b,16)!=16||b[0]!='C'||b[1]!='P'||b[2]!='C'||b[3]!='1'||!((rd16(b+4)==1&&rd16(b+6)==32)||(rd16(b+4)==2&&rd16(b+6)==40))||rd32u(b+12)!=16)goto bad;camera_recsize=rd16(b+6);camera_count=rd32u(b+8);if(camera_count>1000000)goto bad;long size=s_lseek(camerafd,0,2);if(size<0||(u32)size!=16+camera_count*camera_recsize)goto bad;return 1;bad:s_close(camerafd);camerafd=-1;return 0;}
static int camera_read(u32 i,struct Camera*c){if(i>=camera_count)return 0;u8 b[40];s_lseek(camerafd,(long)(16+i*camera_recsize),SEEK_SET_);if(s_read(camerafd,b,camera_recsize)!=(long)camera_recsize)return 0;c->lat=(s32)rd32u(b);c->lon=(s32)rd32u(b+4);c->endlat=(s32)rd32u(b+8);c->endlon=(s32)rd32u(b+12);c->id=rd32u(b+16);c->heading=rd16(b+20);c->speed=rd16(b+22);c->flags=b[24];c->kind=b[25];c->iso[0]=(char)b[26];c->iso[1]=(char)b[27];c->waylo=camera_recsize==40?rd32u(b+32):0;c->wayhi=camera_recsize==40?rd32u(b+36):0;return c->kind>=1&&c->kind<=3&&c->speed<=255&&(c->heading<360||c->heading==65535)&&!(c->flags&~15);}
static u32 camera_lower(s32 lat){u32 lo=0,hi=camera_count;while(lo<hi){u32 m=lo+(hi-lo)/2;struct Camera c;if(!camera_read(m,&c))return camera_count;if(c.lat<lat)lo=m+1;else hi=m;}return lo;}
static int camera_heading_ok(const struct Camera*c){if(c->heading==65535)return 1;int d=shown_course()-(int)c->heading;if(d< -180)d+=360;if(d>180)d-=360;return d>=-45&&d<=45;}
static u32 camera_speed_mps(void){return (u32)((gps.mph10*447+5000)/10000);}
static u32 camera_warn_horizon_m(int routed){
 u32 h=camera_speed_mps()*(routed?22u:18u),hi=routed?1000u:800u;
 if(h<250u)h=250u;if(h>hi)h=hi;
 return h;
}
static u32 camera_chime_horizon_m(void){
 u32 h=camera_speed_mps()*14u;
 if(h<180u)h=180u;if(h>600u)h=600u;
 return h;
}
static int camera_is_speed_alert(void){return camera_current.kind==1||camera_current.kind==3;}
static int camera_has_limit(void){return camera_is_speed_alert()&&camera_current.speed>0&&!(camera_current.flags&12);}
static int camera_is_overspeed(void){
 if(!camera_visible||!camera_has_limit())return 0;
 if(camera_average){
  if(camera_current.flags&2)return (long long)camera_average_speed*100000>(long long)camera_current.speed*160934;
  return camera_average_speed>(int)camera_current.speed;
 }
 if(camera_current.flags&2)return gps.mph10>(int)camera_current.speed*10;
 return (long long)gps.mph10*160934>(long long)camera_current.speed*1000000;
}
static int camera_route_remaining(s32 elat,s32 elon,u32*out_m){
 if(route_status!=R_READY||route_n-route_head<=1)return 0;
 u32 along=0,best=901,bm=0;int cq=coslat_q15(gps.lat);
 for(int i=route_head+1;i<route_n&&along<60000u;i++){
  s32 ax=lon_scale(route_lon[i-1]-elon,cq),ay=route_lat[i-1]-elat;
  s32 bx=lon_scale(route_lon[i]-elon,cq),by=route_lat[i]-elat;
  if(ax<=20000&&ax>=-20000&&ay<=20000&&ay>=-20000&&bx<=20000&&bx>=-20000&&by<=20000&&by>=-20000){
   u32 d=seg_dist2(ax,ay,bx,by);
   if(d<best){best=d;bm=along+geo_seg_m(route_lat[i-1],route_lon[i-1],elat,elon);}
  }
  along+=nav_seg_m(route_lat[i-1],route_lon[i-1],route_lat[i],route_lon[i]);
 }
 if(best>900u)return 0;
 *out_m=bm;return 1;
}
static int camera_crossed_point(s32 plat,s32 plon,s32 lat,s32 lon,s32 tlat,s32 tlon){
 if(geo_seg_m(plat,plon,lat,lon)>150u)return 0;
 int cq=coslat_q15(lat),k=((shown_course()+5)/10)%36;
 s32 ax=lon_scale(plon-tlon,cq),ay=plat-tlat;
 s32 bx=lon_scale(lon-tlon,cq),by=lat-tlat;
 if(ax>2000||ax<-2000||ay>2000||ay<-2000||bx>2000||bx<-2000||by>2000||by<-2000)return 0;
 if(seg_dist2(ax,ay,bx,by)>625u)return 0;
 if(geo_seg_m(lat,lon,tlat,tlon)<=25u)return 1;
 return ((bx*SIN36[k]+by*COS36[k])>>15)>=0;
}
static int camera_ahead(const struct Camera*c,u32*distance){
 if(!camera_heading_ok(c))return 0;
 int routed=route_status==R_READY&&route_n-route_head>1;
 u32 horizon=camera_warn_horizon_m(routed);
 if(camera_visible&&!camera_average&&c->id==camera_current.id)horizon+=60u;
 if(routed){u32 along=0,best=626,bm=0;int cq=coslat_q15(gps.lat);
  for(int i=route_head+1;i<route_n&&along<horizon+120u;i++){s32 ax=lon_scale(route_lon[i-1]-c->lon,cq),ay=route_lat[i-1]-c->lat,bx=lon_scale(route_lon[i]-c->lon,cq),by=route_lat[i]-c->lat;
   if(ax>20000||ax<-20000||ay>20000||ay<-20000||bx>20000||bx<-20000||by>20000||by<-20000)break;
   u32 d=seg_dist2(ax,ay,bx,by);if((c->waylo||c->wayhi)&&(rh.flags&2)){u32 lo,hi;if(!route_read_way(route_seg_arc[i],&lo,&hi)||lo!=c->waylo||hi!=c->wayhi){along+=nav_seg_m(route_lat[i-1],route_lon[i-1],route_lat[i],route_lon[i]);continue;}}if(d<best){best=d;bm=along+geo_seg_m(route_lat[i-1],route_lon[i-1],c->lat,c->lon);}along+=nav_seg_m(route_lat[i-1],route_lon[i-1],route_lat[i],route_lon[i]);}
  if(best>625||bm>horizon)return 0;*distance=bm;return 1;
 }
 /* Without a route, keep near cross-track tight (~20 m) to reject parallel
  * streets, flaring gently at distance to catch sweeping motorway/A-road bends. */
 int cq=coslat_q15(gps.lat),k=((shown_course()+5)/10)%36;s32 x=lon_scale(c->lon-gps.lon,cq),y=c->lat-gps.lat;
 if(x>1500||x<-1500||y>1500||y<-1500)return 0;
 s32 forward=(x*SIN36[k]+y*COS36[k])>>15,cross=(x*COS36[k]-y*SIN36[k])>>15;
 s32 max_fwd=(s32)((horizon*9u+9u)/10u)+10;
 s32 max_cross=forward<=160?18:18+(forward-160)/12;
 if(max_cross>48)max_cross=48;
 if(forward<0||forward>max_fwd||cross>max_cross||cross<-max_cross)return 0;
 u32 dist=geo_seg_m(gps.lat,gps.lon,c->lat,c->lon);
 if(dist>horizon)return 0;
 *distance=dist;return 1;
}
static void camera_clear(void){camera_visible=camera_average=camera_candidate_valid=camera_prev_valid=0;camera_average_speed=0;camera_average_m=0;camera_warn_span_m=0;camera_avg_min_dist=0;camera_avg_diverge_ms=0;}
/* Volatile atomic request; the receiver consumes it only on the selected BT
 * source. No head-unit source switching or FM interruption is performed. */
static u32 camera_alert_ids[16],camera_alert_times[16],camera_alert_slot=0;
static u32 camera_over_ids[16],camera_over_times[16],camera_over_slot=0;
static void camera_sound_request(u32 id,u32 kind){u32 packet[4]={0x4c414c43u,id,s_now_ms()+1500u,kind};long f=s_open(CAMERA_ALERT_TMP,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(f>=0){long n=s_write((int)f,packet,sizeof(packet));s_close((int)f);if(n==(long)sizeof(packet))s_rename(CAMERA_ALERT_TMP,CAMERA_ALERT_PATH);}}
static int camera_ring_seen(const u32*ids,const u32*times,u32 id,u32 now,u32 cooldown){
 for(int i=0;i<16;i++)if(ids[i]==id&&now-times[i]<cooldown)return 1;
 return 0;
}
static void camera_ring_mark(u32*ids,u32*times,u32*slot,u32 id,u32 now){
 u32 s=(*slot)++&15u;ids[s]=id;times[s]=now;
}
static void camera_sound_service(u32 now){
 if(pref.cameras<2||!bt_source_selected()||!camera_visible)return;
 int over=camera_is_overspeed();
 if(camera_average){
  if(!over)return;
  if(camera_ring_seen(camera_over_ids,camera_over_times,camera_current.id,now,30000u))return;
  camera_ring_mark(camera_over_ids,camera_over_times,&camera_over_slot,camera_current.id,now);
  camera_sound_request(camera_current.id,2u);
  return;
 }
 u32 chime_max=camera_chime_horizon_m();
 if(camera_distance>chime_max||camera_distance<20u)return;
 if(camera_current.kind==2){
  /* Red-light cameras never measure speed: soft approach cue only (modes 2 & 3). */
  if(camera_ring_seen(camera_alert_ids,camera_alert_times,camera_current.id,now,300000u))return;
  camera_ring_mark(camera_alert_ids,camera_alert_times,&camera_alert_slot,camera_current.id,now);
  camera_sound_request(camera_current.id,1u);
  return;
 }
 if(over){
  if(camera_ring_seen(camera_over_ids,camera_over_times,camera_current.id,now,300000u))return;
  camera_ring_mark(camera_over_ids,camera_over_times,&camera_over_slot,camera_current.id,now);
  camera_ring_mark(camera_alert_ids,camera_alert_times,&camera_alert_slot,camera_current.id,now);
  camera_sound_request(camera_current.id,2u);
  return;
 }
 if(pref.cameras==2){
  if(camera_ring_seen(camera_alert_ids,camera_alert_times,camera_current.id,now,300000u))return;
  camera_ring_mark(camera_alert_ids,camera_alert_times,&camera_alert_slot,camera_current.id,now);
  camera_sound_request(camera_current.id,1u);
 }
}
static void camera_enter_average(struct Camera c,u32 now){
 camera_average=1;camera_visible=1;camera_current=c;camera_entry_ms=now;
 camera_average_m=0;camera_average_speed=0;
 u32 rem=0;
 if(!camera_route_remaining(c.endlat,c.endlon,&rem))rem=geo_seg_m(gps.lat,gps.lon,c.endlat,c.endlon);
 camera_distance=rem;camera_avg_min_dist=rem;camera_avg_diverge_ms=0;
}
static void camera_service(void){
 u32 now=s_now_ms();
 int new_fix=gps_fix_ms&&gps_fix_ms!=camera_last_gps_ms;
 if(camera_next_ms&&(s32)(now-camera_next_ms)<0){
  if(!new_fix||(camera_prev_valid&&now-camera_prev_ms<200u))return;
 }
 camera_next_ms=now+500u;camera_last_gps_ms=gps_fix_ms;
 if(!pref.cameras||!gps.fix||!gps_fix_ms||now-gps_fix_ms>5000||!(region_flags&2)||camerafd<0){camera_clear();camera_sound_request(0,0);return;}
 if(camera_prev_valid&&now-camera_prev_ms>3000)camera_clear();
 if(camera_average){
  u32 step=camera_prev_valid?geo_seg_m(camera_prev_lat,camera_prev_lon,gps.lat,gps.lon):0;
  if(step>150||camera_current.iso[0]!=region_iso[0]||camera_current.iso[1]!=region_iso[1]){camera_clear();return;}
  camera_average_m+=step;u32 seconds=(now-camera_entry_ms)/1000;
  camera_average_speed=seconds?(int)udiv32(camera_average_m*36u,seconds*10u):0;
  u32 exit_geo=geo_seg_m(gps.lat,gps.lon,camera_current.endlat,camera_current.endlon);
  u32 rem=0;if(!camera_route_remaining(camera_current.endlat,camera_current.endlon,&rem))rem=exit_geo;
  camera_distance=rem;
  /* An exit is only accepted after entering, with a plausible path and time. */
  int k=((shown_course()+5)/10)%36;s32 ex=lon_scale(camera_current.endlon-gps.lon,coslat_q15(gps.lat)),ey=camera_current.endlat-gps.lat;int passed=ex>-150&&ex<150&&ey>-150&&ey<150&&((ex*SIN36[k]+ey*COS36[k])>>15)<=0;
  if((exit_geo<25||passed)&&seconds>3){camera_clear();return;}if(seconds>7200){camera_clear();return;}
  /* Auto-clear if the driver exited the zone early (e.g. motorway slip road). */
  if(rem<camera_avg_min_dist){camera_avg_min_dist=rem;camera_avg_diverge_ms=0;}
  else if(rem>camera_avg_min_dist+120u){
   if(!camera_avg_diverge_ms)camera_avg_diverge_ms=now;
   else if(now-camera_avg_diverge_ms>=15000u){camera_clear();return;}
  }else camera_avg_diverge_ms=0;
  camera_visible=1;
 }else{
  int prev_visible=camera_visible;u32 prev_id=camera_current.id;
  struct Camera selected;u32 best=1201;int found=0;
  for(u32 i=camera_lower(gps.lat-1500),n=0;i<camera_count&&n<512;i++,n++){struct Camera c;if(!camera_read(i,&c)||c.lat>gps.lat+1500)break;if(c.iso[0]!=region_iso[0]||c.iso[1]!=region_iso[1])continue;u32 d;if(camera_ahead(&c,&d)&&d<best){best=d;selected=c;found=1;}}
  /* High-speed segment crossing: if a 1 Hz fix jumped straight past a tracked
   * average-speed entry gantry, still enter the zone instead of skipping it. */
  if(camera_candidate_valid&&camera_candidate.kind==3&&camera_candidate_distance>20u&&camera_prev_valid&&
     camera_candidate.iso[0]==region_iso[0]&&camera_candidate.iso[1]==region_iso[1]&&
     camera_heading_ok(&camera_candidate)&&
     ((found&&selected.id==camera_candidate.id&&best<=25u)||
      camera_crossed_point(camera_prev_lat,camera_prev_lon,gps.lat,gps.lon,camera_candidate.lat,camera_candidate.lon))){
   camera_enter_average(camera_candidate,now);
  }else{
   camera_visible=found;
   if(found){
    camera_current=selected;camera_distance=best;
    int routed=route_status==R_READY&&route_n-route_head>1;
    if(!prev_visible||prev_id!=selected.id){
     u32 span=camera_warn_horizon_m(routed);if(best>span)span=best;camera_warn_span_m=span;
    }else if(best>camera_warn_span_m)camera_warn_span_m=best;
    camera_candidate=selected;camera_candidate_distance=best;camera_candidate_valid=1;
   }else{camera_candidate_valid=0;camera_warn_span_m=0;}
  }
 }
 camera_sound_service(now);
 camera_prev_lat=gps.lat;camera_prev_lon=gps.lon;camera_prev_ms=now;camera_prev_valid=1;
}
static void camera_title_text(char*b,int size){
 b[0]=0;
 if(camera_average){
  appendstr(b,size,ui_tr("Avg "));
  int v=display_imperial()?(camera_average_speed*100000+80467)/160934:camera_average_speed;
  append_int(b,size,v);appendstr(b,size,display_imperial()?" mph":" km/h");
 }else appendstr(b,size,camera_current.kind==2?ui_tr("Red light"):camera_current.kind==3?ui_tr("Average check"):ui_tr("Speed camera"));
}
static void camera_limit_text(char*b,int size){
 b[0]=0;
 if(!camera_has_limit())return;
 appendstr(b,size,ui_tr("Limit "));
 append_int(b,size,display_limit(camera_current.speed,camera_current.flags));
 appendstr(b,size,display_imperial()?" mph":" km/h");
}
static void camera_alert_text(char*b,int size){
 camera_title_text(b,size);
 if(camera_has_limit()){appendstr(b,size," / ");append_int(b,size,display_limit(camera_current.speed,camera_current.flags));}
}
static void camera_distance_text(char*b,int size){
 b[0]=0;
 if(display_imperial()){
  if(camera_distance>=800u){
   u32 mi10=udiv32(camera_distance*10u+804u,1609u);
   append_int(b,size,(int)(mi10/10u));appendstr(b,size,".");
   char d[2]={(char)('0'+mi10%10u),0};appendstr(b,size,d);appendstr(b,size," mi");
  }else{
   u32 yd=udiv32(camera_distance*1094u+500u,1000u);
   yd=udiv32(yd+5u,10u)*10u;if(camera_distance>0&&yd<10u)yd=10u;
   append_int(b,size,(int)yd);appendstr(b,size," yd");
  }
 }else{
  if(camera_distance>=1000u){
   u32 km10=udiv32(camera_distance+50u,100u);
   append_int(b,size,(int)(km10/10u));appendstr(b,size,".");
   char d[2]={(char)('0'+km10%10u),0};appendstr(b,size,d);appendstr(b,size," km");
  }else if(camera_distance>=100u){
   append_int(b,size,(int)((camera_distance+5u)/10u*10u));appendstr(b,size," m");
  }else{
   u32 m5=(camera_distance+2u)/5u*5u;if(camera_distance>0&&m5<5u)m5=5u;
   append_int(b,size,(int)m5);appendstr(b,size," m");
  }
 }
}
static void camera_strip_text(char*b,int size){
 char d[24];camera_alert_text(b,size);camera_distance_text(d,sizeof(d));
 appendstr(b,size,"  ");appendstr(b,size,d);
}
static u16 camera_alert_colour(void){return camera_is_overspeed()?C_WARN:C_CYAN;}
