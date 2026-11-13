/* HUD presentation. The route, camera and audio services own their data. */
#define HUD_CLOCK_X 354
enum {HUD_BOTTOM_MARGIN=8,HUD_SIDE_W=128,HUD_TURN_X=W-HUD_BOTTOM_MARGIN-HUD_SIDE_W,
 HUD_BOTTOM_GAP=4,HUD_ROAD_PAD=12,HUD_CAMERA_PAD=10,HUD_CAMERA_ICON_W=11,HUD_CAMERA_GAP=8};

#include "hud_glass.h"
#include "hud_turn_masks.h"
#include "hud_status.h"
#include "hud_playback.h"

static int hud_kind,hud_observed_route=R_IDLE;
static int hud_status_map=-1,hud_status_page=-1;
static u32 hud_status_frame_at;
static u32 hud_progress_at;
enum {HUD_MENU_TITLE_X=12,HUD_MENU_GAP=8};
static struct {
 const char*text;
 char first[193],second[193];
 int face,w;
} hud_menu_title;

static const char* hud_page_title(void){
 switch(screen){
 case SCR_MENU:return ui_tr("Menu");
 case SCR_TRIP:return ui_tr("Trip");
 case SCR_BT:return bt_label();
 case SCR_KEY:return ui_tr("Rename");
 case SCR_COORDINATES:return coordinates_title();
 default:return ui_tr("Settings");
 }
}
static int hud_split_title(const char*text,int face,int limit,char*first,char*second){
 int found=0,best_words=0,best_width=0x7fffffff;
 char a[193],b[193];
 for(const char*p=text;*p;){
  unsigned n=utf8_span(p);p+=n?n:1;
  if(!*p)break;
  int end=(int)(p-text);
  while(end>0&&text[end-1]==' ')end--;
  const char*tail=p;while(*tail==' ')tail++;
  if(!end||!*tail)continue;
  for(int i=0;i<end;i++)a[i]=text[i];a[end]=0;
  copystr(b,sizeof(b),tail);
  int aw=width(a,face),bw=width(b,face),widest=aw>bw?aw:bw;
  if(widest>limit)continue;
  int words=p[-1]==' '||*p==' ';
  if(words<best_words||(words==best_words&&widest>=best_width))continue;
  found=1;best_width=widest;best_words=words;
  copystr(first,193,a);copystr(second,193,b);
 }
 return found;
}
static void hud_menu_title_prepare(void){
 const char*text=hud_page_title();
 if(hud_menu_title.text==text)return;
 hud_menu_title.text=text;hud_menu_title.second[0]=0;hud_menu_title.face=3;
 copystr(hud_menu_title.first,sizeof(hud_menu_title.first),text);
 int w=width(text,3);
 // Cache wrapping by page title so notices and temperature never reflow it.
 if(w>140){
  hud_menu_title.face=1;
  if(!hud_split_title(text,1,140,hud_menu_title.first,hud_menu_title.second)&&
     !hud_split_title(text,1,166,hud_menu_title.first,hud_menu_title.second)){
   hud_menu_title.face=0;
   hud_split_title(text,0,166,hud_menu_title.first,hud_menu_title.second);
  }
  w=width(hud_menu_title.first,hud_menu_title.face);
  int second=width(hud_menu_title.second,hud_menu_title.face);if(second>w)w=second;
 }
 if(w<72)w=72;if(w>166)w=166;hud_menu_title.w=w;
}
static struct HudBox hud_status_pose(u32 now){
 struct HudBox b=hud_pose(&hud_tween[HC_STATUS],now);
 if(hud_status_map==0&&b.a){
  // Follow the clock's live edge while retaining the notice's size and fade tween.
  int left=HUD_MENU_TITLE_X+hud_menu_title.w+HUD_MENU_GAP;
  struct HudBox clock=hud_pose(&hud_tween[HC_CLOCK],now);
  int right=(clock.a?clock.x:HUD_CLOCK_X+50)-HUD_MENU_GAP;
  if(b.w>right-left)b.w=right-left;
  b.x=left+(right-left-b.w)/2;
 }
 return b;
}
static const char* hud_route_error(void){
 switch(route_status){
 case R_NOFILE:return ui_tr("Routing data missing");
 case R_NOROAD:return ui_tr("No road near point");
 case R_UNREACHABLE:return ui_tr("No connected route");
 case R_MEMORY:return ui_tr("Routing memory full");
 default:return ui_tr("Routing data error");
 }
}
static void hud_journey_text(char*b,unsigned cap){
 b[0]=0;
 appendstr(b,cap,"~");
 u32 minutes=udiv32(sat_add32(sat_add32(route_result_cost,atlas_future_cost),300u),600u);
 if(minutes>=60u){append_int(b,cap,minutes/60u);appendstr(b,cap,"h ");minutes%=60u;}
 append_int(b,cap,minutes);appendstr(b,cap," min  /  ");
 unsigned distance=slen(b);
 append_miles_hud(b,cap,route_path_m());
 for(char*p=b+distance;*p;p++)if(*p>='A'&&*p<='Z')*p+=32;
}
static const char* hud_audio_tag(void){
 return bt_source_selected()?bt_source_label():af.band;
}
static const char* hud_track(void){
 return reading_present(btui.title)?btui.title:"";
}
static void hud_heading_text(char*b,unsigned cap,int heading){
 static const char*n[8]={"N","NE","E","SE","S","SW","W","NW"};
 b[0]=0;appendstr(b,cap,n[((heading+22)/45)&7]);appendstr(b,cap," ");
 append_int(b,cap,heading);appendstr(b,cap,"°");
}
static void hud_source_text(char*b,unsigned cap){
 b[0]=0;
 if(bt_source_selected()){if(btui.connected)copystr(b,cap,hud_track());return;}
 if(reading_present(af.station)){
  copystr(b,cap,af.station);
  if(reading_present(af.freq)){appendstr(b,cap,"  ");appendstr(b,cap,af.freq);}
 }else if(reading_present(af.freq))copystr(b,cap,af.freq);
 else if(reading_present(car.source)&&!streq(car.source,af.band))copystr(b,cap,car.source);
}

enum {HUD_KEYS=1, HUD_PROGRESS=2, HUD_PLAYBACK=8};
enum {HUD_STATUS_TITLE,HUD_STATUS_BODY,HUD_STATUS_DETAIL,HUD_STATUS_CONNECTION,
 HUD_STATUS_FEEDBACK,HUD_STATUS_TRANSPORT,HUD_STATUS_LAYOUT_COUNT};
static struct {
 int from[HUD_STATUS_LAYOUT_COUNT],to[HUD_STATUS_LAYOUT_COUNT];
 u32 at;
} hud_status_layout;

static int hud_status_offset(int row,int q){
 int to=hud_status_layout.to[row];
 return q==256?to:hud_lerp(hud_status_layout.from[row],to,q);
}
static void hud_prepare_status_layout(const struct HudCard*c,u32 now,int was_visible){
 if(!c->box.a)return;
 const int to[]={5,25,c->detail[0]?16:0,
  c->connection[0]?14:0,c->feedback[0]?14:0,c->flags&HUD_KEYS?17:0};
 int changed=0;
 for(int i=0;i<HUD_STATUS_LAYOUT_COUNT;i++)changed|=hud_status_layout.to[i]!=to[i];
 if(was_visible&&!changed)return;
 int q=hud_ease(now-hud_status_layout.at,320);
 for(int i=0;i<HUD_STATUS_LAYOUT_COUNT;i++){
  hud_status_layout.from[i]=was_visible?hud_status_offset(i,q):to[i];
  hud_status_layout.to[i]=to[i];
 }
 hud_status_layout.at=now;
}
static struct HudBox hud_box(int x,int y,int w,int h){return (struct HudBox){x,y,w,h,256};}
static void keyboard_surface(int x,int y,int w,int h,int selected){
 u16 tint=selected?C_ACTIVE:(theme_resolved?rgb(26,39,54):rgb(226,235,244));
 u16 edge=selected?C_CYAN:(theme_resolved?rgb(81,104,128):rgb(141,160,178));
 hud_glass_surface(hud_box(x,y,w,h),tint,edge,selected?12u:5u,0);
 if(selected)round_rect(x+3,y+5,2,h-10,1,C_CYAN,15);
}
static void hud_clock_prepare(u32 now){
 struct HudCard c={0};
 int minute=local_clock_minutes();
 if(minute>=0){
  int h=minute/60,m=minute%60;
  c.title[0]='0'+h/10;c.title[1]='0'+h%10;c.title[2]=':';c.title[3]='0'+m/10;c.title[4]='0'+m%10;
 }
 if(car.tempok&&car_temp_valid(car.temp)){
  int f=pref.units==2||(pref.units==0&&region_iso[0]=='U'&&region_iso[1]=='S');
  append_int(c.detail,sizeof(c.detail),f?car.temp*9/5+32:car.temp);
  appendstr(c.detail,sizeof(c.detail),f?"°F":"°C");
 }
 if(c.title[0]||c.detail[0]){
  int x=c.title[0]&&c.detail[0]?HUD_CLOCK_X:HUD_CLOCK_X+50;
  c.box=hud_box(x,8,W-x-8,27);
 }
 hud_set_card(HC_CLOCK,c,now);
}
static void hud_nav_text(struct HudCard*c,int z,int kind,int tag){
 switch(kind){
 case HUD_CALC:
  copystr(c->title,sizeof(c->title),route_dest_idx>0?ui_tr("Trying nearby road"):(route_rerouting?ui_tr("Rerouting"):ui_tr("Calculating route")));
  c->flags|=HUD_PROGRESS;break;
 case HUD_FAILURE:copystr(c->title,sizeof(c->title),hud_route_error());break;
 case HUD_JOURNEY:
  if(route_status==R_READY){if(tag)copystr(c->tag,sizeof(c->tag),"ETA");hud_journey_text(c->title,sizeof(c->title));}break;
 case HUD_AUTO_ZOOM:copystr(c->title,sizeof(c->title),ui_tr("Auto zoom"));break;
 case HUD_EXPLORE:copystr(c->title,sizeof(c->title),ui_tr("Explore"));break;
 case HUD_MANUAL_ZOOM:copystr(c->title,sizeof(c->title),ui_tr("Manual zoom"));break;
 case HUD_HEADING:
  if(gps.fix)hud_heading_text(c->title,sizeof(c->title),map_course());break;
 }
}
static void hud_prepare(int z,u32 now,int map){
 struct HudCard c={0};
 // Navigation and the entire menu tree are two presentation contexts.
 // Moving between menu pages keeps the current entrance and notification term.
 if(hud_status_map!=map){
  memzero(&hud_tween[HC_STATUS],sizeof(hud_tween[HC_STATUS]));
  hud_status_map=map;
 }
 hud_status_page=screen;
 hud_clock_prepare(now);hud_playback_update(now);
 if(!map)hud_menu_title_prepare();
 /* No radio feed still has a valid BT source; volume requires a live feed. */
 if(!audio_source_detected()||!hud_pulse_live(&hud_audio,now)||!reading_present(car.volume))hud_audio.kind=HUD_NONE;
 if(!hud_pulse_live(&hud_source,now)||
    (hud_source.kind==HUD_TRACK&&(!btui.connected||!bt_source_selected()||!reading_present(btui.title))))hud_source.kind=HUD_NONE;
 if(!hud_pulse_live(&hud_navigation,now))hud_navigation.kind=HUD_NONE;
 if(!btui.connected||!hud_pulse_live(&hud_connection,now))hud_connection.kind=HUD_NONE;
 if(!hud_pulse_live(&hud_bt_feedback,now))hud_bt_feedback.kind=HUD_NONE;
 if(route_status!=hud_observed_route){
  hud_observed_route=route_status;
  if(route_status==R_READY)hud_pulse_at(&hud_navigation,HUD_JOURNEY,now,4200);
  else if(route_status>=R_NOFILE)hud_pulse_at(&hud_navigation,HUD_FAILURE,now,4200);
  else if(route_status==R_IDLE&&(hud_navigation.kind==HUD_JOURNEY||hud_navigation.kind==HUD_FAILURE))hud_navigation.kind=HUD_NONE;
 }
 int media=map&&!freecam&&bt_control_mode&&bt_controls_available();
 int nav=hud_navigation.kind;
 int urgent=route_status==R_CALC?HUD_CALC:(nav==HUD_FAILURE?HUD_FAILURE:HUD_NONE);
 if(media){
  copystr(c.tag,sizeof(c.tag),"BT ");appendstr(c.tag,sizeof(c.tag),ui_tr("CTRL"));
  c.flags|=HUD_KEYS;if(hud_playback.valid)c.flags|=HUD_PLAYBACK;c.value=bt_transport_playing(now);
 }
 if(urgent){
  c.kind=urgent;hud_nav_text(&c,z,urgent,!media);
  if(urgent==HUD_FAILURE)c.ink|=HUD_ERROR;
  if(hud_source.kind){copystr(c.detail,sizeof(c.detail),hud_audio_tag());appendstr(c.detail,sizeof(c.detail),"  ");char source[96];hud_source_text(source,sizeof(source));appendstr(c.detail,sizeof(c.detail),source);}
  else if(media)copystr(c.detail,sizeof(c.detail),hud_track());
 }else if(media){
  c.kind=HUD_MEDIA;copystr(c.title,sizeof(c.title),hud_track());
  struct HudCard n={0};hud_nav_text(&n,z,nav,1);
  if(n.title[0]){
   if(n.tag[0]){copystr(c.detail,sizeof(c.detail),n.tag);appendstr(c.detail,sizeof(c.detail),"  ");}
   appendstr(c.detail,sizeof(c.detail),n.title);
  }else{
   if(reading_present(btui.artist))copystr(c.detail,sizeof(c.detail),btui.artist);
   if(reading_present(btui.album)){if(c.detail[0])appendstr(c.detail,sizeof(c.detail)," / ");appendstr(c.detail,sizeof(c.detail),btui.album);}
  }
 }else if(nav&&(!hud_source.kind||!time_before_u32(hud_navigation.changed,hud_source.changed))){
  c.kind=nav;hud_nav_text(&c,z,nav,1);
  if(nav==HUD_FAILURE)c.ink|=HUD_ERROR;
  if(hud_source.kind){copystr(c.detail,sizeof(c.detail),hud_audio_tag());appendstr(c.detail,sizeof(c.detail),"  ");char source[96];hud_source_text(source,sizeof(source));appendstr(c.detail,sizeof(c.detail),source);}
 }else if(hud_source.kind){
  c.kind=HUD_SOURCE;copystr(c.tag,sizeof(c.tag),hud_audio_tag());hud_source_text(c.title,sizeof(c.title));
  if(bt_source_selected()&&reading_present(btui.artist))copystr(c.detail,sizeof(c.detail),btui.artist);
 }
 int show_connection=hud_connection.kind;
 int has_device_name=reading_present(hud_connected_name);
 if(show_connection){
  char*text=c.kind?c.connection:c.title;
  unsigned cap=c.kind?sizeof(c.connection):sizeof(c.title);
  copystr(text,cap,ui_tr(has_device_name?"Connected to":"Connected"));
  if(has_device_name){appendstr(text,cap," ");appendstr(text,cap,hud_connected_name);}
  if(!c.kind){c.kind=HUD_CONNECTION;copystr(c.tag,sizeof(c.tag),bt_source_label());c.ink|=HUD_SUCCESS;}else c.ink|=HUD_SUCCESS<<4;
 }
 if(hud_bt_feedback.kind){
  if(c.kind){copystr(c.feedback,sizeof(c.feedback),hud_bt_text);c.ink|=hud_bt_level<<6;}
  else{c.kind=HUD_BT_FEEDBACK;copystr(c.title,sizeof(c.title),hud_bt_text);c.ink|=hud_bt_level;}
 }
 /* Volume is independent of source/route changes, and never replaces track
  * metadata or transport keys in BT CTRL. */
 if(hud_audio.kind){
  if(!c.kind){c.kind=HUD_VOLUME;copystr(c.tag,sizeof(c.tag),hud_audio_tag());copystr(c.title,sizeof(c.title),car.volume);}
  else copystr(c.badge,sizeof(c.badge),car.volume);
 }
 hud_status_observe(map,now);
 hud_status_compose(&c,now);
 if(!c.tag[0]&&!c.title[0]&&!c.badge[0]&&!c.flags)c.kind=HUD_NONE;
 hud_kind=c.kind;
 if(c.kind){
  int y=8,h=27+(c.detail[0]?16:0)+(c.connection[0]?14:0)+(c.feedback[0]?14:0)+
   (c.flags&HUD_KEYS?17:0)+(c.flags&HUD_PROGRESS?7:0);
  int w=20+width(c.title,1)+(c.tag[0]?width(c.tag,0)+12:0)+(c.badge[0]?width(c.badge,0)+18:0);
  int detail_w=width(c.detail,0)+20;
  int connection_w=width(c.connection,0)+20,feedback_w=width(c.feedback,0)+20;
  int minw=c.flags&HUD_KEYS?282:c.flags&HUD_PROGRESS?174:c.kind==HUD_VOLUME?128:62;
  if(w<detail_w)w=detail_w;if(w<minw)w=minw;
  if(w<connection_w)w=connection_w;if(w<feedback_w)w=feedback_w;
  int x=8;
  if(map){if(w>HUD_CLOCK_X-16)w=HUD_CLOCK_X-16;}
  else{
   int left=HUD_MENU_TITLE_X+hud_menu_title.w+HUD_MENU_GAP;
   struct HudBox clock=hud_tween[HC_CLOCK].target.box;
   int right=(clock.a?clock.x:HUD_CLOCK_X+50)-HUD_MENU_GAP;
   if(w>right-left)w=right-left;
   // hud_status_pose centres against the live header. A different page title
   // must not retarget the entrance merely because its horizontal anchor moves.
  }
  c.box=hud_box(x,y,w,h);
 }
 const struct HudCard*previous=&hud_tween[HC_STATUS].target;
 if((c.flags&HUD_PROGRESS)&&(!previous->box.a||!(previous->flags&HUD_PROGRESS)))hud_progress_at=now;
 hud_prepare_status_layout(&c,now,hud_status_pose(now).a>0);
 hud_set_card(HC_STATUS,c,now);
 struct HudBox b=hud_status_pose(now);
 nav_card_x=b.x;nav_card_y=b.y;nav_card_w=b.a?b.w:0;nav_card_h=b.h;
 hud_status_frame_at=now;
}

static char hud_last_road[113];
static int hud_road_live;
static u32 hud_road_at;
static int hud_speed_over,hud_speed_from;
static u32 hud_speed_at;
static int hud_confirmed_limit(void){
 return roadctx.valid&&roadctx.speed>0&&!(roadctx.speed_flags&(1|4|8));
}
static int hud_is_overspeed(void){
 if(!gps.fix||!hud_confirmed_limit())return 0;
 if(roadctx.speed_flags&2)return gps.mph10>roadctx.speed*10;
 return (long long)gps.mph10*160934>=(long long)roadctx.speed*1000000+1;
}
static int hud_speed_red(u32 now){
 u32 age=now-hud_speed_at;
 if(age<360u)return hud_lerp(hud_speed_from,hud_speed_over?256:0,hud_ease(age,360));
 if(!hud_speed_over)return 0;
 /* Slow eased pulse, rim only, with no hard flashes. */
 u32 t=(age-360u)%1800u;
 return 256-hud_ease(t<=900u?t:1800u-t,900)*210/256;
}
static void hud_driver_prepare(u32 now){
 struct HudCard speed={0},road={0},road_name={0},turn={0};
 int camera=gps.fix&&camera_visible;
 int show_speed=gps.fix&&(!freecam||camera);
 const char*name=gps.fix&&roadctx.valid?roadctx.street:"";
 if(gps.fix&&roadctx.valid&&!streq(name,hud_last_road)){
  copystr(hud_last_road,sizeof(hud_last_road),name);hud_road_at=now;hud_road_live=name[0]!=0;
 }
 if(now-hud_road_at>=4000u)hud_road_live=0;
 int show_name=!freecam&&name[0]&&hud_road_live;
 if(show_name){
  // The name has its own tween. Its vertical offset is relative to the plate's bottom.
  road_name.box=hud_box(0,camera?-19:-23,1,1);
  copystr(road_name.title,sizeof(road_name.title),name);
 }
 int over=show_speed&&hud_is_overspeed();
 if(over!=hud_speed_over){hud_speed_from=hud_speed_red(now);hud_speed_over=over;hud_speed_at=now;}
 if(show_speed){
  speed.box=hud_box(HUD_BOTTOM_MARGIN,HUD_BOTTOM_Y,HUD_SIDE_W,43);
  append_int(speed.title,sizeof(speed.title),display_speed());
  append_road_limit(speed.detail,sizeof(speed.detail),&roadctx);
  speed.flags=hud_confirmed_limit();
 }
 if(trip.active&&!trip.paused&&gps.fix&&!freecam&&route_status==R_READY&&nav_man_type!=MAN_NONE&&nav_man_idx>=0){
  turn.box=hud_box(HUD_TURN_X,HUD_BOTTOM_Y,HUD_SIDE_W,43);turn.kind=nav_man_type;
  turn.flags=region_flags&4;turn.value=nav_man_exit;
  append_nav_distance(turn.title,sizeof(turn.title),nav_man_m);
  copystr(turn.detail,sizeof(turn.detail),nav_man_text());
 }
 if(camera||show_name){
  int max_w=W-2*(HUD_BOTTOM_MARGIN+HUD_SIDE_W+HUD_BOTTOM_GAP);
  int w=show_name?width(name,1)+2*HUD_ROAD_PAD:0;
  if(camera){
   road.kind=1;road.ink=camera_alert_colour();
   camera_alert_text(road.title,sizeof(road.title));
   camera_distance_text(road.badge,sizeof(road.badge));
   int camera_w=2*HUD_CAMERA_PAD+HUD_CAMERA_ICON_W+2*HUD_CAMERA_GAP+width(road.title,0)+width(road.badge,0);
   if(camera_w>w)w=camera_w;
  }
  if(w>max_w)w=max_w;
  int h=camera&&show_name?43:29;
  road.box=hud_box((W-w)/2,HUD_BOTTOM_Y+43-h,w,h);
 }
 hud_set_card(HC_SPEED,speed,now);hud_set_card(HC_ROAD,road,now);hud_set_card(HC_TURN,turn,now);
 hud_set_card(HC_ROAD_NAME,road_name,now);
}

static void hud_rect_ink(int x,int y,int w,int h,u16 ink){
 for(int j=0;j<h;j++)for(int i=0;i<w;i++)blend_pixel(x+i,y+j,ink,15);
}
static void hud_disc(int x,int y,int r,u16 ink){
 int limit=r*8;
 for(int j=-r;j<=r;j++)for(int i=-r;i<=r;i++){
  int dx=i*8,dy=j*8,d=dx*dx+dy*dy;
  if(d<=(limit-6)*(limit-6)){blend_pixel(x+i,y+j,ink,15);continue;}
  if(d>=(limit+6)*(limit+6))continue;
  int a=0;for(int yy=-3;yy<=3;yy+=2)for(int xx=-3;xx<=3;xx+=2)
   a+=(dx+xx)*(dx+xx)+(dy+yy)*(dy+yy)<=limit*limit;
  if(a)blend_pixel(x+i,y+j,ink,(a*15+8)>>4);
 }
}
static void hud_play_triangle(int x,int y,int direction,u16 ink){
 for(int j=-5;j<=5;j++)for(int i=0;i<8;i++){
  int a=0;
  for(int yy=-3;yy<=3;yy+=2)for(int xx=-3;xx<=3;xx+=2){
   int v=j*8+yy;if(v<0)v=-v;
   a+=v*7<=5*(56-i*8-xx);
  }
  if(a)blend_pixel(x+i*direction,y+j,ink,(a*15+8)>>4);
 }
}
static void hud_draw_turn(int x,int y,const struct HudCard*c){
 int t=c->kind,idx=0,mirror=0;
 if(t==MAN_ROUNDABOUT){idx=4;mirror=!c->flags;}
 else if(t==MAN_UTURN){idx=3;mirror=!c->flags;}
 else{
  mirror=t==MAN_LEFT||t==MAN_SLIGHT_LEFT||t==MAN_SHARP_LEFT;
  idx=t==MAN_SLIGHT_LEFT||t==MAN_SLIGHT_RIGHT?1:t==MAN_SHARP_LEFT||t==MAN_SHARP_RIGHT?2:0;
 }
 for(int j=0;j<HUD_TURN_ICON;j++)for(int i=0;i<HUD_TURN_ICON;i++){
  int a=hud_turn_masks[idx][j*HUD_TURN_ICON+(mirror?HUD_TURN_ICON-1-i:i)];
  if(a)blend_pixel(x-HUD_TURN_ICON/2+i,y-HUD_TURN_ICON/2+j,C_CYAN,a);
 }
 if(t==MAN_ROUNDABOUT&&c->value>0&&c->value<100){
  char b[4]="";append_int(b,sizeof(b),c->value);compact_center(x,y-10,17,c->value>=10?0:1,b,C_WHITE);
 }
}
static void hud_draw_transport(int x,int w,int y,u32 now,int playing,int play_fade){
 int previous_x=x+w/4,play_x=x+w/2,next_x=x+3*w/4;
 u16 previous_ink=C_MUTED,next_ink=C_MUTED;
 if(bt_control_mode&&hud_pulse_live(&bt_transport_press,now)){
  enum {PRESS_RISE_MS=45,PRESS_HOLD_MS=140};
  u32 age=now-bt_transport_press.changed;
  int glow=hud_ease(age,PRESS_RISE_MS)>>4;
  if(age>PRESS_HOLD_MS)glow-=hud_ease(age-PRESS_HOLD_MS,BT_PRESS_MS-PRESS_HOLD_MS)>>4;
  int button=bt_transport_press.kind;
  int selected_x=button==BT_TRANSPORT_PREVIOUS?previous_x:button==BT_TRANSPORT_NEXT?next_x:play_x;
  round_rect(selected_x-22,y-7,44,15,5,C_CYAN,(glow+1)/2);
  if(button==BT_TRANSPORT_PREVIOUS)previous_ink=hud_mix16(C_MUTED,C_WHITE,glow);
  if(button==BT_TRANSPORT_NEXT)next_ink=hud_mix16(C_MUTED,C_WHITE,glow);
 }
 hud_rect_ink(previous_x-6,y-4,2,9,previous_ink);
 hud_play_triangle(previous_x+3,y,-1,previous_ink);
 int row_alpha=hud_ink_alpha;
 hud_ink_alpha=(row_alpha*play_fade+128)>>8;
 if(playing){
  hud_rect_ink(play_x-4,y-4,2,9,C_WHITE);
  hud_rect_ink(play_x+2,y-4,2,9,C_WHITE);
 }else hud_play_triangle(play_x-4,y,1,C_WHITE);
 hud_ink_alpha=row_alpha;
 hud_play_triangle(next_x-4,y,1,next_ink);
 hud_rect_ink(next_x+5,y-4,2,9,next_ink);
}
static int hud_status_ink(int alpha,int fade){return (alpha*fade+128)>>8;}
static int hud_status_row_ink(int alpha,int room){
 return hud_status_ink(alpha,hud_ease(room>0?(u32)room:0,3));
}
static void hud_draw_status(struct HudBox b,const struct HudCard*c,u32 now){
 const struct HudTween*t=&hud_tween[HC_STATUS];
 const struct HudCard*old=&t->previous,*next=&t->target;
 u32 age=now-t->content_at;
 int phase=age<95u?256-hud_ease(age,95):hud_ease(age-95u,125);
 int q=hud_ease(now-hud_status_layout.at,320),layout[HUD_STATUS_LAYOUT_COUNT];
 for(int i=0;i<HUD_STATUS_LAYOUT_COUNT;i++)layout[i]=hud_status_offset(i,q);
 int full=(16*b.a+128)>>8,fade=hud_status_ink(full,phase);
 int x=b.x,y=b.y,w=b.w,bottom=y+b.h,badge=c->badge[0]?width(c->badge,0)+17:0,tx=x+10;
 int title_y=y+layout[HUD_STATUS_TITLE],small_y=title_y+2;
 int end=x+w-9-badge;
 hud_ink_alpha=streq(old->tag,next->tag)&&streq(old->title,next->title)&&((old->ink^next->ink)&3)==0?full:fade;
 if(c->tag[0]){
  int tw=width(c->tag,0);
  compact_fit(tx,small_y,tw,0,c->tag,C_CYAN);tx+=tw;
  if(c->title[0]){hud_rect_ink(tx+5,small_y+1,1,11,C_BORDER);tx+=12;}
 }
 compact_fit(tx,title_y,end-tx,1,c->title,hud_status_colour(c->ink&3,c->flags&HUD_PROGRESS?C_CYAN:C_WHITE));
 if(badge){
  hud_ink_alpha=streq(old->badge,next->badge)?full:fade;
  int right=x+w-8;
  hud_rect_ink(right-badge+4,small_y+1,1,11,C_BORDER);
  compact_fit(right-badge+12,small_y,badge-10,0,c->badge,C_CYAN);
 }
 int row=y+layout[HUD_STATUS_BODY];
 const struct {const char*from,*to,*text;u16 ink;int same_colour;} lines[]={
  {old->detail,next->detail,c->detail,hud_status_colour((c->ink>>2)&3,C_MUTED),((old->ink^next->ink)&12)==0},
  {old->connection,next->connection,c->connection,hud_status_colour((c->ink>>4)&3,C_CYAN),((old->ink^next->ink)&48)==0},
  {old->feedback,next->feedback,c->feedback,hud_status_colour((c->ink>>6)&3,C_WHITE),((old->ink^next->ink)&192)==0},
 };
 // Reserve space gradually so existing rows move as neighbours arrive or leave.
 for(unsigned i=0;i<sizeof(lines)/sizeof(lines[0]);i++){
  int height=layout[HUD_STATUS_DETAIL+i];
  if(lines[i].text[0]){
   int room=bottom-row-13;
   if(room>height-11)room=height-11;
   hud_ink_alpha=hud_status_row_ink(streq(lines[i].from,lines[i].to)&&lines[i].same_colour?full:fade,room);
   if(hud_ink_alpha)compact_fit(x+10,row-2,w-20,0,lines[i].text,lines[i].ink);
  }
  row+=height;
 }
 int changed_flags=old->flags^next->flags;
 if(c->flags&HUD_KEYS){
  hud_ink_alpha=hud_status_row_ink(changed_flags&HUD_KEYS?fade:full,bottom-row-14);
  if(hud_ink_alpha){
   int controls_alpha=hud_ink_alpha;
   if(c->flags&HUD_PLAYBACK){
    hud_ink_alpha=hud_status_ink(controls_alpha,changed_flags&HUD_PLAYBACK?phase:256);
    hud_draw_playback(x+10,row-3,w-20,now);hud_ink_alpha=controls_alpha;
   }else hud_rect_ink(x+9,row-3,w-18,1,C_BORDER);
   int play_fade=!(changed_flags&HUD_KEYS)&&old->value!=next->value?phase:256;
   hud_draw_transport(x,w,row+8,now,c->value,play_fade);
  }
 }
 row+=layout[HUD_STATUS_TRANSPORT];
 if(c->flags&HUD_PROGRESS){
  int track=w-20,travel=track-28;if(travel<1)return;
  hud_ink_alpha=hud_status_row_ink(changed_flags&HUD_PROGRESS?fade:full,bottom-row-5);
  u32 p=((now-hud_progress_at)/22u)%(u32)(travel*2);
  int off=p<=(u32)travel?(int)p:2*travel-(int)p;
  int py=b.y+b.h-5;
  hud_rect_ink(x+10,py,track,2,C_BORDER);hud_rect_ink(x+10+off,py,28,2,C_CYAN);
 }
}
static void hud_draw_camera(int x,int y,int w,const struct HudCard*c){
 int distance_w=width(c->badge,0),title_w=width(c->title,0);
 int fixed_w=HUD_CAMERA_ICON_W+2*HUD_CAMERA_GAP+distance_w;
 int title_limit=w-2*HUD_CAMERA_PAD-fixed_w;
 if(title_limit<=0)return;
 if(title_w>title_limit)title_w=title_limit;
 int start=x+(w-fixed_w-title_w)/2;
 int icon_x=start+HUD_CAMERA_ICON_W/2;
 hud_disc(icon_x,y+14,5,c->ink);hud_disc(icon_x,y+14,2,C_PANEL);
 int title_x=start+HUD_CAMERA_ICON_W+HUD_CAMERA_GAP;
 compact_fit(title_x,y+6,title_w,0,c->title,c->ink);
 compact_fit(title_x+title_w+HUD_CAMERA_GAP,y+6,distance_w,0,c->badge,c->ink);
}
static void hud_draw_speed(struct HudBox b,const struct HudCard*c){
 struct HudBox limit={b.x+b.w-54,b.y+5,48,33,b.a};
 int speed_x=(b.x+limit.x)/2;
 int speed_face=width(c->title,6)<=68?6:5;
 compact_center(speed_x,b.y+(speed_face==6?3:7),68,speed_face,c->title,C_WHITE);

 u16 tint=theme_resolved?rgb(39,52,71):rgb(223,234,246);
 u16 edge=theme_resolved?rgb(119,143,171):rgb(126,148,172);
 if(c->flags)edge=theme_resolved?rgb(225,100,110):rgb(187,48,62);
 /* Reuse the frost sampler on the glass already behind the inset. */
 hud_glass_surface(limit,tint,edge,8u,0);

 int text_w=limit.w-8,face=5;
 while(face>3&&width(c->detail,face)>text_w)face--;
 int text_y=face==5?2:face==4?5:7;
 compact_center(limit.x+limit.w/2,limit.y+text_y,text_w,face,c->detail,C_WHITE);
}
static void hud_draw_card(int id,u32 now){
 struct HudTween*t=&hud_tween[id];if(!t->seen)return;
 struct HudBox b=id==HC_STATUS?hud_status_pose(now):hud_pose(t,now);if(b.a<=0)return;
 int alpha,shift;
 const struct HudCard*c=hud_ink_card(t,now,&alpha,&shift);
 int red=id==HC_SPEED?hud_speed_red(now):0;
 hud_glass(b,red);
 int ox=clip_x,oy=clip_y,or=clip_right,ob=clip_bottom;
 if(b.x+3>clip_x)clip_x=b.x+3;if(b.y+2>clip_y)clip_y=b.y+2;
 if(b.x+b.w-3<clip_right)clip_right=b.x+b.w-3;
 if(b.y+b.h-2<clip_bottom)clip_bottom=b.y+b.h-2;
 hud_ink_alpha=(alpha*b.a+128)>>8;
 int x=b.x,y=b.y+shift,w=b.w;
 switch(id){
 case HC_STATUS:hud_draw_status(b,c,now);break;
 case HC_CLOCK:
  if(!c->title[0]){
   compact_center(x+w/2,y+5,w-12,1,c->detail,C_WHITE);
   break;
  }
  if(c->detail[0]&&w>=99){
   hud_ink_alpha=hud_ink_alpha*hud_ease((u32)(w-99),19)/256;
   compact_center(x+w-93,y+6,44,0,c->detail,C_MUTED);hud_rect_ink(x+w-69,y+7,1,12,C_BORDER);
  }
  hud_ink_alpha=(16*b.a+128)>>8;
  compact_center(x+w-33,b.y+4,58,3,t->target.title[0]?t->target.title:c->title,C_WHITE);break;
 case HC_SPEED:hud_draw_speed(b,c);break;
 case HC_ROAD:{
  struct HudTween*nt=&hud_tween[HC_ROAD_NAME];
  struct HudBox np=hud_pose(nt,now);
  int name_alpha,name_shift;
  const struct HudCard*nc=hud_ink_card(nt,now,&name_alpha,&name_shift);
  int name_y=b.y+b.h+np.y+name_shift;
  name_alpha=(name_alpha*(np.a<b.a?np.a:b.a)+128)>>8;
  if(c->kind){
   // Fade the incoming row as the glass makes room for it, keeping the other row readable.
   int gap=name_y-y-18;
   int room=hud_ease(gap>0?(u32)gap:0,5);
   if(name_alpha&&!t->previous.kind&&now-t->content_at<320u)hud_ink_alpha=hud_ink_alpha*room/256;
   else name_alpha=name_alpha*room/256;
   hud_draw_camera(x,y,w,c);
  }
  hud_ink_alpha=name_alpha;
  if(name_alpha)compact_center(x+w/2,name_y,w-2*HUD_ROAD_PAD,1,nc->title,C_WHITE);
  break;
 }
 case HC_TURN:
  hud_draw_turn(x+23,y+21,c);
  compact_fit(x+46,y+4,w-53,1,c->title,C_WHITE);
  compact_fit(x+46,y+23,w-53,0,c->detail,C_MUTED);break;
 }
 hud_ink_alpha=16;clip_x=ox;clip_y=oy;clip_right=or;clip_bottom=ob;
}
static void hud_draw_all(u32 now,int map){
 if(map){hud_draw_card(HC_SPEED,now);hud_draw_card(HC_ROAD,now);hud_draw_card(HC_TURN,now);}
 hud_draw_card(HC_CLOCK,now);hud_draw_card(HC_STATUS,now);
}
/* New content and geometry use about 15 Hz; routine values use the map cadence.
 * Include the final frame after a transition ends. */
static int hud_menu_animating(u32 now){
 if(hud_status_page!=screen)return 1;
 const int ids[]={HC_STATUS,HC_CLOCK};
 for(unsigned i=0;i<sizeof(ids)/sizeof(ids[0]);i++){
  const struct HudTween*t=&hud_tween[ids[i]];
  if(!t->seen||(!t->from.a&&!t->target.box.a))continue;
  if(now-t->position_at<320u||hud_status_frame_at-t->position_at<320u||
     now-t->fast_content_at<220u||hud_status_frame_at-t->fast_content_at<220u)return 1;
 }
 const struct HudTween*t=&hud_tween[HC_STATUS];
 if(t->seen&&(t->from.a||t->target.box.a)){
  if(now-hud_status_layout.at<320u||hud_status_frame_at-hud_status_layout.at<320u)return 1;
  if(t->target.box.a&&(t->target.flags&HUD_PROGRESS))return 1;
 }
 return 0;
}
static void hud_draw_page_title(void){
 int face=hud_menu_title.face,w=hud_menu_title.w;
 if(hud_menu_title.second[0]){
  compact_fit(HUD_MENU_TITLE_X,4,w,face,hud_menu_title.first,C_WHITE);
  compact_fit(HUD_MENU_TITLE_X,20,w,face,hud_menu_title.second,C_WHITE);
 }else compact_fit(HUD_MENU_TITLE_X,11,w,face,hud_menu_title.first,C_WHITE);
}
static void header(void){
 u32 now=hud_now_ms();hud_prepare(active_zoom(),now,0);hud_draw_all(now,0);hud_draw_page_title();
}

#ifdef HOST_TEST
static void hud_reset_animations(void){
 popup_reset();
 memzero(hud_tween,sizeof(hud_tween));hud_last_road[0]=0;hud_road_at=0;hud_road_live=0;
 hud_status_reset();hud_playback_reset();
 hud_speed_over=hud_speed_from=0;hud_speed_at=0;hud_status_map=hud_status_page=-1;hud_status_frame_at=0;
 bt_transport_press.kind=0;bt_playback_request=-1;
}
static void hud_settle_for_capture(u32 now){
 popup_settle_for_capture(now);
 for(int i=0;i<HC_COUNT;i++)if(hud_tween[i].seen){hud_tween[i].position_at=now-320u;hud_tween[i].content_at=hud_tween[i].fast_content_at=now-220u;}
 hud_status_layout.at=now-320u;
}
#endif
