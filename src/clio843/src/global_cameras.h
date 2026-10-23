/* CPC1: spatially sorted, bounded I/O, visual camera guidance in the existing
 * road strip. Exact alerts fail closed without GPS and reviewed region policy.
 * Fixed=1, red light=2, average entry/exit pair=3. Heading is TRAFFIC direction. */
struct Camera {s32 lat,lon,endlat,endlon;u32 id;u16 heading,speed;u8 flags,kind;char iso[2];u32 waylo,wayhi;};
static int camerafd=-1,camera_visible=0,camera_average=0;
static u32 camera_recsize=32,camera_count=0,camera_next_ms=0,camera_distance=0,camera_prev_ms=0,camera_entry_ms=0,camera_average_m=0;
static int camera_average_speed=0,camera_candidate_valid=0,camera_prev_valid=0;
static s32 camera_prev_lat=0,camera_prev_lon=0;
static struct Camera camera_current,camera_candidate;
static u32 camera_candidate_distance=0;
static int load_cameras(void){u8 b[16];camerafd=(int)s_open(CAMERAS_PATH,O_RDONLY_,0);if(camerafd<0)return 0;if(s_read(camerafd,b,16)!=16||b[0]!='C'||b[1]!='P'||b[2]!='C'||b[3]!='1'||!((rd16(b+4)==1&&rd16(b+6)==32)||(rd16(b+4)==2&&rd16(b+6)==40))||rd32u(b+12)!=16)goto bad;camera_recsize=rd16(b+6);camera_count=rd32u(b+8);if(camera_count>1000000)goto bad;long size=s_lseek(camerafd,0,2);if(size<0||(u32)size!=16+camera_count*camera_recsize)goto bad;return 1;bad:s_close(camerafd);camerafd=-1;return 0;}
static int camera_read(u32 i,struct Camera*c){if(i>=camera_count)return 0;u8 b[40];s_lseek(camerafd,(long)(16+i*camera_recsize),SEEK_SET_);if(s_read(camerafd,b,camera_recsize)!=(long)camera_recsize)return 0;c->lat=(s32)rd32u(b);c->lon=(s32)rd32u(b+4);c->endlat=(s32)rd32u(b+8);c->endlon=(s32)rd32u(b+12);c->id=rd32u(b+16);c->heading=rd16(b+20);c->speed=rd16(b+22);c->flags=b[24];c->kind=b[25];c->iso[0]=(char)b[26];c->iso[1]=(char)b[27];c->waylo=camera_recsize==40?rd32u(b+32):0;c->wayhi=camera_recsize==40?rd32u(b+36):0;return c->kind>=1&&c->kind<=3&&c->speed<=255&&(c->heading<360||c->heading==65535)&&!(c->flags&~15);}
static u32 camera_lower(s32 lat){u32 lo=0,hi=camera_count;while(lo<hi){u32 m=lo+(hi-lo)/2;struct Camera c;if(!camera_read(m,&c))return camera_count;if(c.lat<lat)lo=m+1;else hi=m;}return lo;}
static int camera_heading_ok(const struct Camera*c){if(c->heading==65535)return 1;int d=shown_course()-(int)c->heading;if(d< -180)d+=360;if(d>180)d-=360;return d>=-45&&d<=45;}
static int camera_ahead(const struct Camera*c,u32*distance){
 if(!camera_heading_ok(c))return 0;
 if(route_status==R_READY&&route_n-route_head>1){u32 along=0,best=626,bm=0;int cq=coslat_q15(gps.lat);
 for(int i=route_head+1;i<route_n&&along<1300;i++){s32 ax=lon_scale(route_lon[i-1]-c->lon,cq),ay=route_lat[i-1]-c->lat,bx=lon_scale(route_lon[i]-c->lon,cq),by=route_lat[i]-c->lat;
 if(ax>20000||ax< -20000||ay>20000||ay< -20000||bx>20000||bx< -20000||by>20000||by< -20000)break;
 u32 d=seg_dist2(ax,ay,bx,by);if((c->waylo||c->wayhi)&&(rh.flags&2)){u32 lo,hi;if(!route_read_way(route_seg_arc[i],&lo,&hi)||lo!=c->waylo||hi!=c->wayhi){along+=nav_seg_m(route_lat[i-1],route_lon[i-1],route_lat[i],route_lon[i]);continue;}}if(d<best){best=d;bm=along+geo_seg_m(route_lat[i-1],route_lon[i-1],c->lat,c->lon);}along+=nav_seg_m(route_lat[i-1],route_lon[i-1],route_lat[i],route_lon[i]);}
 if(best>625||bm>1200)return 0;*distance=bm;return 1;
 }
 /* Without a route, show only points in a narrow forward corridor. Direction
  * metadata filters opposite travel; parallel/grade-separated roads can still
  * be ambiguous without matched road identities. */
 int cq=coslat_q15(gps.lat),k=((shown_course()+5)/10)%36;s32 x=lon_scale(c->lon-gps.lon,cq),y=c->lat-gps.lat;
 if(x>1500||x< -1500||y>1500||y< -1500)return 0;
 s32 forward=(x*SIN36[k]+y*COS36[k])>>15,cross=(x*COS36[k]-y*SIN36[k])>>15;
 if(forward<0||forward>700||cross>18||cross< -18)return 0;*distance=geo_seg_m(gps.lat,gps.lon,c->lat,c->lon);return 1;
}
static void camera_clear(void){camera_visible=camera_average=camera_candidate_valid=camera_prev_valid=0;camera_average_speed=0;camera_average_m=0;}
/* Volatile atomic request; the receiver consumes it only on the selected BT
 * source. No head-unit source switching or FM interruption is performed. */
static u32 camera_alert_ids[16],camera_alert_times[16],camera_alert_slot=0;
static void camera_sound_request(u32 id,u32 kind){u32 packet[4]={0x4c414c43u,id,s_now_ms()+1500u,kind};long f=s_open(CAMERA_ALERT_TMP,O_WRONLY_|O_CREAT_|O_TRUNC_,0666);if(f>=0){long n=s_write((int)f,packet,sizeof(packet));s_close((int)f);if(n==(long)sizeof(packet))s_rename(CAMERA_ALERT_TMP,CAMERA_ALERT_PATH);}}
static void camera_sound_service(u32 now){
 if(pref.cameras!=2||!bt_source_selected()||!camera_visible||camera_average||camera_distance>600||camera_distance<20)return;
 for(int i=0;i<16;i++)if(camera_alert_ids[i]==camera_current.id&&now-camera_alert_times[i]<300000u)return;
 u32 slot=camera_alert_slot++&15;camera_alert_ids[slot]=camera_current.id;camera_alert_times[slot]=now;camera_sound_request(camera_current.id,1);
}
static void camera_service(void){u32 now=s_now_ms();if(camera_next_ms&&(s32)(now-camera_next_ms)<0)return;camera_next_ms=now+500;
 if(!pref.cameras||!gps.fix||!gps_fix_ms||now-gps_fix_ms>5000||!(region_flags&2)||camerafd<0){camera_clear();camera_sound_request(0,0);return;}
 if(camera_prev_valid&&now-camera_prev_ms>3000)camera_clear();
 if(camera_average){u32 step=camera_prev_valid?geo_seg_m(camera_prev_lat,camera_prev_lon,gps.lat,gps.lon):0;
 if(step>150||camera_current.iso[0]!=region_iso[0]||camera_current.iso[1]!=region_iso[1]){camera_clear();return;}
 camera_average_m+=step;u32 seconds=(now-camera_entry_ms)/1000;
 camera_average_speed=seconds?(int)udiv32(camera_average_m*36u,seconds*10u):0;
 camera_distance=geo_seg_m(gps.lat,gps.lon,camera_current.endlat,camera_current.endlon);
 /* An exit is only accepted after entering, with a plausible path and time. */
 int k=((shown_course()+5)/10)%36;s32 ex=lon_scale(camera_current.endlon-gps.lon,coslat_q15(gps.lat)),ey=camera_current.endlat-gps.lat;int passed=ex> -150&&ex<150&&ey> -150&&ey<150&&((ex*SIN36[k]+ey*COS36[k])>>15)<=0;
 if((camera_distance<25||passed)&&seconds>3){camera_clear();return;}if(seconds>7200){camera_clear();return;}
 camera_visible=1;
 }else{
 struct Camera selected;u32 best=1201;int found=0;
 for(u32 i=camera_lower(gps.lat-1500),n=0;i<camera_count&&n<512;i++,n++){struct Camera c;if(!camera_read(i,&c)||c.lat>gps.lat+1500)break;if(c.iso[0]!=region_iso[0]||c.iso[1]!=region_iso[1])continue;u32 d;if(camera_ahead(&c,&d)&&d<best){best=d;selected=c;found=1;}}
 camera_visible=found;
 if(found){camera_current=selected;camera_distance=best;if(selected.kind==3&&camera_candidate_valid&&camera_candidate.id==selected.id&&camera_candidate_distance>25&&best<=25&&camera_prev_valid){camera_average=1;camera_entry_ms=now;camera_average_m=0;camera_average_speed=0;camera_distance=geo_seg_m(gps.lat,gps.lon,selected.endlat,selected.endlon);}camera_candidate=selected;camera_candidate_distance=best;camera_candidate_valid=1;}else camera_candidate_valid=0;
 }
 camera_sound_service(now);
 camera_prev_lat=gps.lat;camera_prev_lon=gps.lon;camera_prev_ms=now;camera_prev_valid=1;
}
/* Only fixed and average-speed cameras carry a speed restriction in the UI.
 * Red-light records can contain maxspeed metadata, but it is not their alert. */
static int camera_is_speed_alert(void){return camera_current.kind==1||camera_current.kind==3;}
static void camera_strip_text(char*b,int size){b[0]=0;
 if(camera_average){appendstr(b,size,ui_tr("AVG "));int v=display_imperial()?(camera_average_speed*100000+80467)/160934:camera_average_speed;append_int(b,size,v);appendstr(b,size,display_imperial()?" mph":" km/h");}
 else appendstr(b,size,camera_current.kind==2?ui_tr("RED LIGHT"):camera_current.kind==3?ui_tr("AVERAGE CHECK"):ui_tr("SPEED CAMERA"));
 if(camera_is_speed_alert()&&camera_current.speed&&!(camera_current.flags&12)){appendstr(b,size," / ");append_int(b,size,display_limit(camera_current.speed,camera_current.flags));}appendstr(b,size,"  ");if(display_imperial()){u32 yd=(camera_distance*1094+500)/1000;append_int(b,size,(int)((yd+5)/10*10));appendstr(b,size," yd");}else{append_int(b,size,(int)((camera_distance+5)/10*10));appendstr(b,size," m");}
}
static void draw_camera_strip(void){if(!camera_visible||!gps.fix)return;char b[64];camera_strip_text(b,sizeof(b));
 int actual=camera_average?camera_average_speed:(gps.mph10*160934+500000)/1000000;
 int limit=camera_current.flags&2?(camera_current.speed*160934+50000)/100000:camera_current.speed;
 u16 colour=camera_is_speed_alert()&&!(camera_current.flags&12)&&limit>0&&actual>limit?C_WARN:C_CYAN;
 fill(133,243,240,29,C_PANEL);fill(133,243,240,2,colour);compact_center(253,251,228,0,b,colour);
}
