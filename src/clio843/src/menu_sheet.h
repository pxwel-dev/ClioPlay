/* Animated map-based menu sheet. All mutations use the canonical persisted
 * trip, saved-place, coordinate and Bluetooth models below. */
enum {P_CLOSED,P_HOME,P_SETTINGS,P_TRIP,P_PLACES,P_NEARBY,P_NEARBY_LIST,P_STOP,P_DEST,P_PLACE,
 P_NAME,P_COORD,P_REVIEW,P_LANG,P_PHONE,P_PHONE_ACTIONS,P_PAIR,P_CONFIRM,P_SOURCE,P_SET_DISPLAY,P_SET_HUD,P_SET_NAV,P_SET_HISTORY,P_SET_SYSTEM};
/* Settings IDs remain stable; categories only control navigation and presentation. */
static const struct {const char*title;int count,rows[4];} settings_groups[]={
 {"Display",4,{0,1,8,9}}, {"HUD layout",2,{10,11}}, {"Navigation",4,{2,3,4,16}},
 {"Driving history",4,{12,13,14,15}}, {"System",3,{7,5,6}}
};
enum {SETTINGS_GROUP_COUNT=sizeof(settings_groups)/sizeof(settings_groups[0])};
static int settings_group(int p){return p>=P_SET_DISPLAY&&p<=P_SET_SYSTEM?p-P_SET_DISPLAY:-1;}
static int settings_row(int p,int focus){int g=settings_group(p);return g>=0&&focus>=0&&focus<settings_groups[g].count?settings_groups[g].rows[focus]:-1;}
static int sheet_page,sheet_focus,sheet_old_page,sheet_old_focus,sheet_stack_page[12],sheet_stack_focus[12],sheet_depth;
static int places_tab,place_index,place_from_nearby,stop_index,flow_action,language_keyboard,trip_tab_focus[4];
static int keyboard_origin,phone_index,confirm_kind,confirm_top_focus;
static u32 confirm_diversion_revision,confirm_diversion_serial;
static int confirm_diversion_map,confirm_diversion_kind;
static struct SavedPlace confirm_suggestion;
static struct MobPoint confirm_frequent;
static u32 confirm_parking_serial,confirm_stop_serial;
static struct MobPoint confirm_parking_point;
static char confirm_parking_name[113];
static struct MobStopHabit confirm_stop;
/* Menu-only origin: the keyboard still uses the normal Favourite save action. */
enum {KBD_NAME_FREQUENT=3};
static u32 confirm_suggestion_serial;
static int menu_suggestion_current(void){
 struct SavedPlace p;return confirm_suggestion_serial==mob_suggest_serial&&suggested_place(&p)&&suggestion_same(&p,&confirm_suggestion);
}
static int menu_parking_current(void){
 return trip.active&&route_status==R_READY&&ml_park_valid&&confirm_parking_serial&&confirm_parking_serial==ml_park_serial;
}
static int menu_stop_current(void){
 return trip.active&&trip.count==1&&route_status==R_READY&&ml_stop_valid&&confirm_stop_serial&&confirm_stop_serial==ml_stop_serial;
}
static int confirm_diversion_begin,confirm_diversion_join;
/* A delayed response must never act on a replacement offer. */
static int menu_diversion_current(void){
 return traffic_offer_valid()&&confirm_diversion_revision==traffic_offer_revision&&confirm_diversion_serial==traffic_offer_serial&&
  confirm_diversion_begin==traffic_begin_idx&&confirm_diversion_join==traffic_join_idx;
}
static int confirm_has_view(void){return confirm_kind==3&&menu_diversion_current();}
static char phone_addr[18];
static int places_picker,review_from_place,source_action;
static struct CoordinateDraft coord_before_edit;
static char context_name[161];
static int menu_pending;
static u32 menu_at,focus_at,bottom_at,menu_presented_at,menu_interaction_at;
static u32 menu_paint_at;
static int sheet_old_tab,menu_pair_waited,menu_pair_before;
static char menu_pair_name[33];
static int menu_screen(int p){return p==P_CLOSED?SCR_MAP:p==P_HOME?SCR_MENU:(p==P_SETTINGS||settings_group(p)>=0)||(p==P_LANG&&!language_keyboard)?SCR_SET:p==P_PHONE||p==P_PHONE_ACTIONS||p==P_PAIR?SCR_BT:p==P_NAME||p==P_LANG?SCR_KEY:p==P_COORD||p==P_REVIEW?SCR_COORDINATES:SCR_TRIP;}
static int bottom_from=256,bottom_to=256;
/* Only the actual typing grid can borrow the lower HUD area.  The language
 * picker, including the picker opened from the keyboard, is an ordinary
 * compact sheet and must leave both persistent HUD pill rows unobstructed. */
static int compact_page(int p){return p!=P_NAME;}
static int bottom_pose(u32 now){int q=frame_ease(now-bottom_at,320);return q==256?bottom_to:q?hud_lerp(bottom_from,bottom_to,q):bottom_from;}
static struct HudBox sheet_from_box,sheet_to_box;

enum {MENU_TOP=44,MENU_PILL_GAP=8,MENU_MIN_BODY=76};

static int menu_top(struct HudBox status,struct HudBox clock){
 int top=MENU_TOP;
 const struct HudBox pills[]={status,clock};
 for(unsigned i=0;i<sizeof(pills)/sizeof(*pills);i++){
  const struct HudBox b=pills[i];
  int edge=b.y+b.h+MENU_PILL_GAP;
  if(b.a&&edge>MENU_TOP){
   /* Give back the space gradually as an outgoing pill fades. */
   edge=hud_lerp(MENU_TOP,edge,b.a);
   if(edge>top)top=edge;
  }
 }
 return top;
}
static int menu_bottom(int p){return (compact_page(p)?HUD_BOTTOM_Y:H)-MENU_PILL_GAP;}
static int menu_fit_top(int p,int top){
 int limit=menu_bottom(p)-MENU_MIN_BODY;
 return top<limit?top:limit;
}
static struct HudBox menu_box_at_top(int p,int top){
 int bottom=menu_bottom(p),w=280,h=154;
 top=menu_fit_top(p,top);
 if(p==P_TRIP||p==P_PLACES||p==P_NEARBY||p==P_NEARBY_LIST){w=456;h=220;}
 if(p==P_PHONE){w=372;h=210;}
 if(p==P_SETTINGS){w=372;h=31+SETTINGS_GROUP_COUNT*22+15;}
 int group=settings_group(p);if(group>=0){w=372;h=48+settings_groups[group].count*29;}
 if(p==P_NAME){w=456;h=220;}
 if(p==P_COORD){w=432;h=220;}
 if(p==P_REVIEW){w=416;h=220;}
 if(p==P_LANG){w=364;h=220;}
 if(p==P_STOP||p==P_DEST||p==P_PLACE){w=356;h=p==P_PLACE?174:154;}
 if(p==P_PHONE_ACTIONS){w=324;h=136;}
 if(p==P_PAIR){w=316;h=128;}
 if(p==P_CONFIRM){w=280;h=confirm_has_view()?122:92;
  if(confirm_kind==4&&width(place_display_name(confirm_suggestion.name),0)>252){w=356;h=106;}
  if(confirm_kind==6||confirm_kind==7){w=312;h=96;}
 }
 if(p==P_SOURCE){w=356;h=174;}
 if(h>bottom-top)h=bottom-top;
 int y=p==P_CONFIRM?MENU_TOP+(bottom-MENU_TOP-h)/2:top+(bottom-top-h)/2;
 if(y<top)y=top;
 return (struct HudBox){(W-w)/2,y,w,h,p?256:0};
}
static struct HudBox menu_box(int p){
 return menu_box_at_top(p,menu_top(hud_tween[HC_STATUS].target.box,hud_tween[HC_CLOCK].target.box));
}
static struct HudBox menu_pose(u32 now){
 /* The sheet follows the pill's actual frame, including prepared frames.
  * Geometry never resets the independent page/content transition. */
 int top=menu_top(hud_status_pose(now),hud_pose(&hud_tween[HC_CLOCK],now));
 struct HudBox to=sheet_page?menu_box_at_top(sheet_page,top):sheet_to_box;
 struct HudBox b=popup_lerp_box(sheet_from_box,to,frame_ease(now-menu_at,320));
 top=menu_fit_top(sheet_page,top);
 if(b.a&&b.y<top){
  b.h-=top-b.y;b.y=top;
  if(b.h<=0)b.a=0;
 }
 return b;
}
static void menu_change(int next,int selected,u32 now){
 sheet_from_box=menu_pose(now);sheet_old_page=sheet_page;sheet_old_focus=sheet_focus;sheet_old_tab=places_tab;
 bottom_from=bottom_pose(now);bottom_to=compact_page(next)?256:0;bottom_at=now;
 if(sheet_page==P_TRIP)trip_tab_focus[0]=sheet_focus;if(sheet_page==P_PLACES)trip_tab_focus[places_tab+1]=sheet_focus;if(sheet_page==P_NEARBY&&!places_picker)trip_tab_focus[3]=sheet_focus;
 sheet_page=next;sheet_focus=selected;sheet_to_box=menu_box(next);menu_at=focus_at=menu_interaction_at=now;screen=menu_screen(next);
 if(next==P_PLACES){place_from_nearby=0;int count=places_tab?nfaves:nrecents;if(sheet_focus>=count)sheet_focus=count?count-1:0;}
 if(next==P_NEARBY_LIST){if(sheet_focus>=nearby_count)sheet_focus=nearby_count?nearby_count-1:0;}
 if(next==P_TRIP||next==P_HOME||next==P_CLOSED){places_picker=review_from_place=place_from_nearby=0;flow_action=DEST_PICK_NONE;}
 if(!sheet_old_page){sheet_from_box=sheet_to_box;sheet_from_box.x+=6;sheet_from_box.y+=6;sheet_from_box.w-=12;sheet_from_box.h-=12;sheet_from_box.a=0;}
 if(!next){sheet_to_box=sheet_from_box;sheet_to_box.x+=6;sheet_to_box.y+=6;sheet_to_box.w-=12;sheet_to_box.h-=12;sheet_to_box.a=0;}
}
static void menu_push(int next,int selected,u32 now){if(sheet_depth>=12)return;sheet_stack_page[sheet_depth]=sheet_page;sheet_stack_focus[sheet_depth++]=sheet_focus;menu_change(next,selected,now);}
static void menu_back(u32 now){if(sheet_depth){sheet_depth--;menu_change(sheet_stack_page[sheet_depth],sheet_stack_focus[sheet_depth],now);}else menu_change(P_CLOSED,0,now);}
static void menu_open_diversion(int from_map,u32 now){
 if(!traffic_offer_valid())return;
 confirm_kind=3;confirm_top_focus=0;confirm_diversion_revision=traffic_offer_revision;confirm_diversion_serial=traffic_offer_serial;
 confirm_diversion_begin=traffic_begin_idx;confirm_diversion_join=traffic_join_idx;confirm_diversion_map=from_map;confirm_diversion_kind=ml_offer_kind;
 if(from_map){sheet_depth=0;menu_change(P_CONFIRM,0,now);}
 else if(sheet_page==P_CLOSED){sheet_stack_page[0]=P_HOME;sheet_stack_focus[0]=0;sheet_stack_page[1]=P_TRIP;sheet_stack_focus[1]=trip.active_idx;sheet_depth=2;menu_change(P_CONFIRM,0,now);}
 else menu_push(P_CONFIRM,0,now);
 menu_pending=1;
}
static void menu_open_suggestion(u32 now){
 if(!suggested_place(&confirm_suggestion))return;
 confirm_suggestion_serial=mob_suggest_serial;confirm_kind=4;sheet_depth=0;menu_change(P_CONFIRM,0,now);menu_pending=1;
}
static void ml_resolve_point_name(struct MobPoint p,const char*hint,char*out,unsigned cap){
 out[0]=0;
 if(hint&&hint[0]){copystr(out,cap,hint);return;}
 u32 best=151u;const struct SavedPlace*chosen=0;
 for(int i=0;i<nfaves;i++){u32 d=mob_dist(p,(struct MobPoint){faves[i].lat,faves[i].lon});if(d<best&&faves[i].name[0]&&!fave_needs_name(faves[i].name)){best=d;chosen=&faves[i];}}
 if(!chosen)for(int i=0;i<nrecents;i++){u32 d=mob_dist(p,(struct MobPoint){recents[i].lat,recents[i].lon});if(d<best&&recents[i].name[0]&&!fave_needs_name(recents[i].name)&&!streq(recents[i].name,"PHONE DESTINATION")){best=d;chosen=&recents[i];}}
 if(chosen){copystr(out,cap,chosen->name);return;}
 (void)suggested_road_name(p,out,cap);
}
static void menu_open_parking(u32 now){
 if(!trip.active||route_status!=R_READY||!ml_park_valid)return;
 confirm_parking_serial=ml_park_serial;confirm_parking_point=ml_park_point;
 ml_resolve_point_name(ml_park_point,0,confirm_parking_name,sizeof(confirm_parking_name));
 confirm_kind=6;sheet_depth=0;menu_change(P_CONFIRM,0,now);menu_pending=1;
}
static void menu_open_stop(u32 now){
 if(!trip.active||trip.count!=1||route_status!=R_READY||!ml_stop_valid)return;
 confirm_stop_serial=ml_stop_serial;confirm_stop=ml_stop_habit;
 if(!confirm_stop.name[0]){
  char nm[48];ml_resolve_point_name(confirm_stop.stop,0,nm,sizeof(nm));
  copystr(confirm_stop.name,sizeof(confirm_stop.name),nm);
 }
 confirm_kind=7;sheet_depth=0;menu_change(P_CONFIRM,0,now);menu_pending=1;
}
static void frequent_startup_service(u32 now,int fresh){
 if(!frequent_boot_pending)return;
 if(!pref.learn_history||!pref.destination_predictions||mob_session_generation!=frequent_boot_session||mob_started||now-mob_begin_ms>180000u||trip.active||trip.count){frequent_boot_pending=0;return;}
 if(!fresh){if(!mob_rmc_fix||now-mob_rmc_ms>2500u)frequent_still=0;return;}
 if(mob_rmc_speed>10){frequent_still=0;return;}
 if(!frequent_still||now-frequent_last_at>2500u||mob_dist(frequent_still_point,mob_rmc_point)>20u){frequent_still=1;frequent_still_at=now;frequent_still_point=mob_rmc_point;frequent_last_at=now;return;}
 frequent_last_at=now;
 if(now-frequent_still_at<3000u||screen!=SCR_MAP||sheet_page!=P_CLOSED||modal!=MOD_NONE||freecam||bt_control_mode||boot_fade.image||!gps.fix||mob_time(now)==MOB_UNKNOWN)return;
 struct MobPoint p;frequent_boot_pending=0;
 if(!mob_frequent_destination(mob_time(now),mob_rmc_point,&p)||!frequent_current(p,now)||!mob_fave_mark(p))return;
 confirm_frequent=p;confirm_kind=5;sheet_depth=0;menu_change(P_CONFIRM,1,now);menu_pending=1;
}
static int place_count(void){return places_tab?nfaves:nrecents;}
static int menu_count(int p){
 return p==P_HOME?(hw_bluetooth_enabled?3:2):p==P_SOURCE?4:p==P_SETTINGS?SETTINGS_GROUP_COUNT:settings_group(p)>=0?settings_groups[settings_group(p)].count:p==P_TRIP?(trip.count?trip.count+2+traffic_offer_valid():1):p==P_PLACES?place_count():p==P_NEARBY?NEARBY_CAT_COUNT:p==P_NEARBY_LIST?nearby_count:
 p==P_STOP?3:p==P_DEST?3:p==P_PLACE?((!place_from_nearby&&places_tab)?4:3):p==P_PHONE?nbtdev+1:
 p==P_PHONE_ACTIONS?2:p==P_REVIEW?4:p==P_CONFIRM?(confirm_has_view()?3:2):0;
}
static void selected_focus(int next,u32 now){if(next!=sheet_focus){sheet_old_focus=sheet_focus;sheet_focus=next;focus_at=menu_interaction_at=now;}}
static void open_sources(int action,u32 now){source_action=flow_action=action;places_picker=review_from_place=0;menu_push(P_SOURCE,0,now);}
static void trip_tab_move(int delta,u32 now){
 int tab=sheet_page==P_TRIP?0:sheet_page==P_NEARBY?3:places_tab+1,next=wrap_focus(tab,delta,4);
 int wanted=trip_tab_focus[next];
 /* Save the departing tab before changing the source used by Places. */
 int page=next==0?P_TRIP:next==3?P_NEARBY:P_PLACES;
 menu_change(page,wanted,now);
 if(page==P_PLACES){places_tab=next-1;int count=place_count();sheet_focus=count?(wanted<count?wanted:count-1):0;}
}
static void menu_return_trip(int selected,u32 now){sheet_depth=1;sheet_stack_page[0]=P_HOME;sheet_stack_focus[0]=0;menu_change(P_TRIP,selected,now);}
static void menu_close(u32 now){coordinates_active=0;kbd_picker=0;sheet_depth=0;menu_change(P_CLOSED,0,now);}
static void menu_view_diversion(u32 now){
 if(!menu_diversion_current())return;
 s32 base_lon=traffic_path[0].lon;
 s32 min_lat=traffic_path[0].lat,max_lat=min_lat,min_dlon=0,max_dlon=0;
 for(int i=traffic_begin_idx;i<=traffic_join_idx;i++){
  s32 la=route_lat[i],dl=lon_delta(route_lon[i]-base_lon);
  if(la<min_lat)min_lat=la;if(la>max_lat)max_lat=la;
  if(dl<min_dlon)min_dlon=dl;if(dl>max_dlon)max_dlon=dl;
 }
 for(int k=0;k<traffic_offer_n;k++){
  s32 la=traffic_path[k].lat,dl=lon_delta(traffic_path[k].lon-base_lon);
  if(la<min_lat)min_lat=la;if(la>max_lat)max_lat=la;
  if(dl<min_dlon)min_dlon=dl;if(dl>max_dlon)max_dlon=dl;
 }
 s32 mid_lat=min_lat+(max_lat-min_lat)/2;
 s32 mid_lon=lon_delta(base_lon+min_dlon+(max_dlon-min_dlon)/2);
 s32 lat_span=max_lat-min_lat,lon_span=lon_scale(max_dlon-min_dlon,coslat_q15(mid_lat));
 int hi=MAX_EXPLORE_ZOOM,lo=mapok?(int)mh.minlvl:0,best_z=lo;
 for(int z=hi;z>=lo;z--){
  int sh=14-z;
  unsigned long long pw=((unsigned long long)(u32)lon_span*256u)>>sh;
  unsigned long long ph=((unsigned long long)(u32)lat_span*256u)>>sh;
  if(pw<=448u&&ph<=240u){best_z=z;break;}
 }
 /* Centre the route box vertically at H/2 (16 px margins all round) rather
  * than at FREECAM_ANCHOR_Y=160 so the full 480x272 Explore view is used. */
 s32 y_shift=(s32)(((((long long)(FREECAM_ANCHOR_Y-H/2))<<(14-best_z))+128)>>8);
 s32 view_lat_target=mid_lat-y_shift;
 if(view_lat_target< -8500000)view_lat_target=-8500000;
 if(view_lat_target>8500000)view_lat_target=8500000;
 diversion_preview_active=1;
 diversion_preview_from_map=confirm_diversion_map;
 diversion_preview_kind=confirm_diversion_kind;
 diversion_preview_revision=confirm_diversion_revision;
 diversion_preview_serial=confirm_diversion_serial;
 diversion_preview_begin=confirm_diversion_begin;
 diversion_preview_join=confirm_diversion_join;
 menu_close(now);
 enter_freecam_at(view_lat_target,mid_lon,best_z);
 hud_pulse(&hud_navigation,HUD_EXPLORE,1800);
}
static void start_name(int origin,u32 now){
 keyboard_origin=origin;
 if(origin==KBD_RENAME_FAVE)keyboard_open_fave_rename(place_index);
 else if(origin==KBD_NAME_FAVE){
  s32 lat=sheet_page==P_DEST?trip.target[trip.count-1].lat:(place_from_nearby?nearby_pois[place_index].lat:(places_tab?faves[place_index].lat:recents[place_index].lat));
  s32 lon=sheet_page==P_DEST?trip.target[trip.count-1].lon:(place_from_nearby?nearby_pois[place_index].lon:(places_tab?faves[place_index].lon:recents[place_index].lon));
  const char*name=sheet_page==P_DEST?trip.target[trip.count-1].name:(place_from_nearby?nearby_pois[place_index].name:(places_tab?faves[place_index].name:recents[place_index].name));
  char draft[161];copystr(draft,sizeof(draft),fave_needs_name(name)?"":name);keyboard_open_fave_name(lat,lon);copystr(kbd_text,sizeof(kbd_text),draft);
 }else coordinates_open_name();
 pk_reset();pk_prepare();menu_push(P_NAME,0,now);
}
static int menu_phone_index(void){for(int i=0;i<nbtdev;i++)if(streq(btdev[i].addr,phone_addr))return i;return -1;}
static void menu_pair(u32 now){
 if(!bt_command("PAIR")){notice_fail(ui_tr("Bluetooth unavailable"));return;}
 menu_pair_before=btui.connected;menu_pair_waited=0;copystr(menu_pair_name,sizeof(menu_pair_name),btui.device);menu_push(P_PAIR,0,now);
}
static void setting_adjust(int code,u32 now){
 (void)now;int row=settings_row(sheet_page,sheet_focus);if(row>=0){set_focus=row;settings_key(code);}
}
static void menu_key(int code,u32 now){
 if(code==K_ROTCW||code==K_ROTCCW){
  int d=code==K_ROTCW?1:-1;
  if(sheet_page==P_NAME){pk_prepare();pk_rotate(d);return;}
  if(sheet_page==P_COORD){coordinate_move(code);return;}
  if(sheet_page==P_CONFIRM&&confirm_has_view()){
   selected_focus(wrap_focus(sheet_focus,d,3),now);
   if(sheet_focus<2)confirm_top_focus=sheet_focus;
   return;
  }
  code=d>0?K_DOWN:K_UP;
 }
 if(code==K_MAP){int keep=sheet_page==P_CONFIRM&&((confirm_kind==3&&confirm_diversion_map)||confirm_kind==4||confirm_kind==6||confirm_kind==7);menu_close(now);if(!keep)recenter_map();return;}
 if(code==K_MENU){coordinates_active=0;kbd_picker=0;sheet_depth=0;bt_control_mode=0;menu_change(P_HOME,0,now);return;}
 if(sheet_page==P_NAME){
  pk_prepare();
  if(code==K_BACK){
   if(kbd_language==13&&!pk_symbols&&pk_stage){if(pk_stage==2){pk_stage=1;pk_page=pk_syllable/pk_page_size();pk_focus=pk_syllable%pk_page_size();}else{pk_stage=pk_page=pk_focus=0;}return;}
   if(keyboard_origin==KBD_NAME_COORDINATES){coordinates_save_name();coordinates.step=COORD_LON;menu_change(P_COORD,0,now);}
   else menu_back(now);return;
  }
  if(code==K_UP||code==K_DOWN||code==K_LEFT||code==K_RIGHT){pk_move(code);return;}
  int save=code==K_ENTER&&pk_focus==PK_SAVE;
  if(code==K_ENTER)pk_press();else keyboard_handle(code);
  if(kbd_picker){language_keyboard=1;menu_push(P_LANG,kbd_pick,now);}
  else if(save&&keyboard_origin==KBD_NAME_COORDINATES&&coordinates.step==COORD_REVIEW){
   if(sheet_depth&&sheet_stack_page[sheet_depth-1]==P_REVIEW)sheet_depth--;
   review_from_place=0;menu_change(P_REVIEW,0,now);
  }
  else if(keyboard_origin==KBD_NAME_FREQUENT){if(save&&screen==SCR_TRIP)menu_close(now);}
  else if(save&&keyboard_origin!=KBD_NAME_COORDINATES&&kbd_text[0]){sheet_depth=0;places_tab=1;place_index=fave_contains(kbd_place_lat,kbd_place_lon);menu_change(P_PLACES,place_index>=0?place_index:0,now);}
  screen=menu_screen(sheet_page);return;
 }
 if(sheet_page==P_LANG){
  if(code==K_BACK){kbd_picker=0;menu_back(now);return;}
  if(code==K_ESC||code==K_SR)return;
  int delta=code==K_UP?-1:code==K_DOWN?1:0;
  if(language_keyboard){
   if(code==K_LEFT||code==K_RIGHT)delta=code==K_LEFT?-6:6;
   if(delta){kbd_pick=wrap_focus(kbd_pick,delta,UI_LANGUAGE_COUNT);selected_focus(kbd_pick,now);return;}
   keyboard_handle(code);sheet_focus=kbd_pick;
   if(!kbd_picker)menu_back(now);
  }else{
   if(delta)selected_focus(wrap_focus(sheet_focus,delta,UI_LANGUAGE_COUNT),now);
   if(code==K_ENTER){pref.language=sheet_focus;prefs_mark_dirty();menu_back(now);}
  }
  return;
 }
 if(sheet_page==P_COORD){
  if(code==K_BACK){
   if(sheet_depth&&sheet_stack_page[sheet_depth-1]==P_REVIEW){coordinates=coord_before_edit;menu_back(now);}
   else{coordinates_active=0;menu_back(now);}
   return;
  }
  screen=SCR_COORDINATES;coordinates_handle(code);
  if(screen==SCR_KEY){keyboard_origin=KBD_NAME_COORDINATES;pk_reset();pk_prepare();menu_change(P_NAME,0,now);}
  screen=menu_screen(sheet_page);return;
 }
 if(sheet_page==P_REVIEW&&code==K_BACK){
  if(review_from_place){coordinates_active=0;menu_back(now);}
  else{coordinates_open_name();keyboard_origin=KBD_NAME_COORDINATES;pk_reset();pk_prepare();menu_change(P_NAME,0,now);screen=menu_screen(sheet_page);}return;
 }
 int horizontal=code==K_LEFT||code==K_RIGHT,vertical=code==K_UP||code==K_DOWN;
 int delta=code==K_LEFT||code==K_UP?-1:1;
 if(sheet_page==P_TRIP&&trip.count){
  int first_button=trip.count+(trip.count>=TRIP_MAX_TARGETS);
  if(horizontal&&sheet_focus>=trip.count){
   selected_focus(first_button+wrap_focus(sheet_focus-first_button,delta,menu_count(P_TRIP)-first_button),now);
   return;
  }
  if(vertical){
   int row=sheet_focus<trip.count?sheet_focus:trip.count;
   int next=wrap_focus(row,delta,trip.count+1);
   selected_focus(next==trip.count?first_button:next,now);
   return;
  }
 }
 if((sheet_page==P_TRIP||sheet_page==P_PLACES||(sheet_page==P_NEARBY&&!places_picker))&&horizontal){trip_tab_move(delta,now);return;}
 if(sheet_page==P_CONFIRM&&horizontal){
  if(confirm_has_view()){
   selected_focus(sheet_focus<2?(sheet_focus^1):(code==K_LEFT?0:1),now);
   confirm_top_focus=sheet_focus;return;
  }
  selected_focus(wrap_focus(sheet_focus,delta,2),now);return;
 }
 if(sheet_page==P_CONFIRM&&vertical&&confirm_has_view()){
  if(sheet_focus<2){confirm_top_focus=sheet_focus;selected_focus(2,now);}
  else selected_focus(confirm_top_focus,now);
  return;
 }
 if(code==K_BACK){menu_back(now);return;}
 if(settings_group(sheet_page)>=0&&horizontal){setting_adjust(code,now);return;}
 if(vertical){
  if(settings_group(sheet_page)>=0)theme_time_part=0;
  selected_focus(wrap_focus(sheet_focus,delta,menu_count(sheet_page)),now);
  return;
 }
 if(code!=K_ENTER)return;
 if(sheet_page==P_HOME){menu_push(sheet_focus==0?P_TRIP:hw_bluetooth_enabled&&sheet_focus==1?P_PHONE:P_SETTINGS,0,now);return;}
 if(sheet_page==P_SETTINGS){theme_time_part=0;menu_push(P_SET_DISPLAY+sheet_focus,0,now);return;}
 if(settings_group(sheet_page)>=0){
  int row=settings_row(sheet_page,sheet_focus);
  if(row==7){language_keyboard=0;menu_push(P_LANG,pref.language,now);}
  else if(row==15){confirm_kind=2;menu_push(P_CONFIRM,1,now);}
  else if(row>=12){setting_adjust(K_RIGHT,now);}
  else setting_adjust(K_ENTER,now);return;
 }
 if(sheet_page==P_SOURCE){
  flow_action=source_action;review_from_place=0;
  if(sheet_focus==3){coordinates_open(flow_action);menu_push(P_COORD,0,now);}
  else if(sheet_focus==2){places_picker=1;menu_push(P_NEARBY,0,now);}
  else{places_tab=sheet_focus;places_picker=1;menu_push(P_PLACES,0,now);}return;
 }
 if(sheet_page==P_TRIP){
  if(!trip.count){places_picker=review_from_place=0;flow_action=DEST_PICK_NONE;coordinates_open(flow_action);menu_push(P_COORD,0,now);return;}
  if(sheet_focus<trip.count){stop_index=sheet_focus;copystr(context_name,sizeof(context_name),trip.target[sheet_focus].name);menu_push(sheet_focus==trip.count-1?P_DEST:P_STOP,0,now);}
  else if(sheet_focus==trip.count){if(trip.count>=TRIP_MAX_TARGETS)return;open_sources(DEST_PICK_ADD_STOP,now);}
  else if(sheet_focus==trip.count+2&&traffic_offer_valid()){
   menu_open_diversion(0,now);
  }
  else if(sheet_focus==trip.count+1){confirm_kind=0;menu_push(P_CONFIRM,1,now);}return;
 }
 if(sheet_page==P_PLACES){
  if(!place_count()||sheet_focus>=place_count())return;
  place_index=sheet_focus;place_from_nearby=0;const struct SavedPlace*point=places_tab?&faves[sheet_focus]:&recents[sheet_focus];
  copystr(context_name,sizeof(context_name),point->name);
  if(places_picker){
   coordinates_open(flow_action);coordinates.lat=point->lat;coordinates.lon=point->lon;coordinates.step=COORD_REVIEW;
   coordinate_format(coordinates.value[0],sizeof(coordinates.value[0]),point->lat);coordinate_format(coordinates.value[1],sizeof(coordinates.value[1]),point->lon);
   copystr(coordinates.name,sizeof(coordinates.name),point->name);review_from_place=1;menu_push(P_REVIEW,0,now);
  }else menu_push(P_PLACE,0,now);return;
 }
 if(sheet_page==P_NEARBY){
  nearby_scan(sheet_focus);menu_push(P_NEARBY_LIST,0,now);return;
 }
 if(sheet_page==P_NEARBY_LIST){
  if(!nearby_count||sheet_focus>=nearby_count)return;
  place_index=sheet_focus;place_from_nearby=1;const struct NearbyPoi*poi=&nearby_pois[sheet_focus];
  copystr(context_name,sizeof(context_name),poi->name);
  if(places_picker){
   coordinates_open(flow_action);coordinates.lat=poi->lat;coordinates.lon=poi->lon;coordinates.step=COORD_REVIEW;
   coordinate_format(coordinates.value[0],sizeof(coordinates.value[0]),poi->lat);coordinate_format(coordinates.value[1],sizeof(coordinates.value[1]),poi->lon);
   copystr(coordinates.name,sizeof(coordinates.name),poi->name);review_from_place=1;menu_push(P_REVIEW,0,now);
  }else menu_push(P_PLACE,0,now);return;
 }
 if(sheet_page==P_STOP){
  if(stop_index<trip.active_idx||stop_index>=trip.count-1)return;
  if(sheet_focus==2){confirm_kind=1;menu_push(P_CONFIRM,1,now);return;}
  int delta=sheet_focus==0?-1:1,next=stop_index+delta;if(next<trip.active_idx||next>=trip.count-1)return;
  trip_move_stop(stop_index,delta);menu_return_trip(next,now);return;
 }
 if(sheet_page==P_DEST){
  if(sheet_focus==0){open_sources(DEST_PICK_CHANGE_DEST,now);}
  else if(sheet_focus==1){int i=fave_contains(trip.target[trip.count-1].lat,trip.target[trip.count-1].lon);if(i>=0)remove_fave(i);else start_name(KBD_NAME_FAVE,now);}
  else{confirm_kind=0;menu_push(P_CONFIRM,1,now);}return;
 }
 if(sheet_page==P_PLACE){
  int total=place_from_nearby?nearby_count:place_count();
  if(place_index<0||place_index>=total){menu_back(now);return;}
  if(sheet_focus==2){start_name((!place_from_nearby&&places_tab)?KBD_RENAME_FAVE:KBD_NAME_FAVE,now);return;}
  if(sheet_focus==3&&!place_from_nearby&&places_tab){remove_fave(place_index);menu_back(now);return;}
  struct SavedPlace point;
  if(place_from_nearby){point.lat=nearby_pois[place_index].lat;point.lon=nearby_pois[place_index].lon;copystr(point.name,sizeof(point.name),nearby_pois[place_index].name);}
  else point=places_tab?faves[place_index]:recents[place_index];
  if(sheet_focus==0){
   if(place_from_nearby)ml_record_poi_use(nearby_pois[place_index].kind,nearby_pois[place_index].lat,nearby_pois[place_index].lon,nearby_pois[place_index].name);
   set_destination(point.name,point.lat,point.lon);menu_close(now);recenter_map();
  }else if(sheet_focus==1){
   int index=trip.count-1;
   if(trip_add_stop(point.name,point.lat,point.lon)){
    if(place_from_nearby)ml_record_poi_use(nearby_pois[place_index].kind,nearby_pois[place_index].lat,nearby_pois[place_index].lon,nearby_pois[place_index].name);
    add_recent(point.name,point.lat,point.lon);menu_return_trip(index,now);
   }else notice_fail(ui_tr(trip.active?"Trip full - stop not added":"No active route"));
  }return;
 }
 if(sheet_page==P_REVIEW){
  if(sheet_focus==1){if(sheet_depth>=12)return;coord_before_edit=coordinates;coordinates.step=COORD_LAT;coordinates.focus=0;coordinates.error=0;menu_push(P_COORD,0,now);}
  else if(sheet_focus==2){review_from_place=0;coordinates_open_name();keyboard_origin=KBD_NAME_COORDINATES;pk_reset();pk_prepare();menu_change(P_NAME,0,now);}
  else if(sheet_focus==3){coordinates_active=0;menu_return_trip(0,now);}
  else{int action=coordinates.action,index=trip.count-1;
   if(place_from_nearby&&place_index>=0&&place_index<nearby_count){
    ml_record_poi_use(nearby_pois[place_index].kind,nearby_pois[place_index].lat,nearby_pois[place_index].lon,nearby_pois[place_index].name);
   }
   coordinates_submit();
   if(!coordinates_active){if(screen==SCR_MAP)menu_close(now);else menu_return_trip(action==DEST_PICK_ADD_STOP?index:trip.count-1,now);}
  }return;
 }
 if(sheet_page==P_PHONE){if(sheet_focus==0)menu_pair(now);else if(sheet_focus<=nbtdev){phone_index=sheet_focus-1;copystr(phone_addr,sizeof(phone_addr),btdev[phone_index].addr);menu_push(P_PHONE_ACTIONS,0,now);}return;}
 if(sheet_page==P_PHONE_ACTIONS){
  int index=menu_phone_index();if(index<0){menu_back(now);return;}
  if(sheet_focus==0)bt_command_arg(btdev[index].connected?"DISCONNECT":"CONNECT",phone_addr);
  else bt_forget_device(phone_addr);
  menu_back(now);return;
 }
 if(sheet_page==P_CONFIRM){
  if(confirm_kind==2){if(sheet_focus==0){mob_clear();traffic_invalidate();traffic_learning_reset();notice_success(ui_tr("History cleared"));}menu_back(now);return;}
  if(confirm_kind==7){
   if(menu_stop_current()){
    if(sheet_focus==0){
     const char*nm=confirm_stop.name[0]?confirm_stop.name:"MAP POINT";
     ml_stop_valid=0;
     if(trip_add_stop(nm,confirm_stop.stop.lat,confirm_stop.stop.lon))add_recent(nm,confirm_stop.stop.lat,confirm_stop.stop.lon);
    }else ml_decline_stop_offer();
   }
   menu_close(now);return;
  }
  if(confirm_kind==6){
   if(menu_parking_current()){
    if(sheet_focus==0){
     const char*nm=confirm_parking_name[0]?confirm_parking_name:"PARKING";
     ml_guided_target_valid=0;ml_park_valid=0;
     trip_change_destination_notice(nm,confirm_parking_point.lat,confirm_parking_point.lon,ui_tr("Usual parking selected"));
    }else ml_decline_parking_offer();
   }
   menu_close(now);return;
  }
  if(confirm_kind==5){
   struct MobPoint p=confirm_frequent;
   if(sheet_focus==0&&frequent_current(p,now)){
    struct SavedPlace draft;
    /* Defer naming until the driver agrees to edit; never save an inferred name. */
    frequent_named_place(p,&draft);
    keyboard_open_fave_name(p.lat,p.lon);copystr_limit(kbd_text,sizeof(kbd_text),draft.name,KBD_TEXT_LIMIT);
    keyboard_origin=KBD_NAME_FREQUENT;sheet_depth=0;pk_reset();pk_prepare();menu_change(P_NAME,0,now);return;
   }
   menu_close(now);return;
  }
  if(confirm_kind==4){
   if(menu_suggestion_current()){
    if(sheet_focus==0){set_destination(confirm_suggestion.name,confirm_suggestion.lat,confirm_suggestion.lon);mob_suggest_valid=0;}
    else{mob_reject_destination((struct MobPoint){confirm_suggestion.lat,confirm_suggestion.lon});mob_reject_suggestion();}
   }
   menu_close(now);return;
  }
  if(confirm_kind==3){
   if(sheet_focus==2){
    if(menu_diversion_current()){menu_view_diversion(now);return;}
   }else if(menu_diversion_current()){
    int is_usual=confirm_diversion_kind==1;
    if(sheet_focus==0){if(traffic_accept()){notice_success(ui_tr(is_usual?"Usual route selected":"Diversion selected"));menu_close(now);if(freecam)recenter_map();return;}}
    else traffic_reject();
   }
   if(confirm_diversion_map)menu_close(now);else menu_return_trip(trip.active_idx,now);return;
  }
  if(sheet_focus==0){if(confirm_kind==0){clear_trip();notice_success(ui_tr("Route cancelled"));}else trip_remove_stop(stop_index);}
  menu_return_trip(stop_index<trip.count?stop_index:0,now);return;
 }
}
static void menu_tile(int x,int y,int w,int h,int chosen,int opacity){
 int old=hud_ink_alpha;hud_ink_alpha=opacity;
 round_rect(x,y,w,h,5,chosen?C_ACTIVE:C_PANEL2,chosen?15:5);hud_ink_alpha=old;
}
static void text_value(int x,int y,int w,const char*s,int opacity){hud_ink_alpha=opacity;box_text(x,y,w,15,0,s,C_MUTED);}
static const char*menu_title(int p){return p==P_HOME?"Menu":p==P_SETTINGS?"Settings":p==P_TRIP||p==P_PLACES?"Trip":p==P_NEARBY?"Nearby":p==P_NEARBY_LIST?(nearby_category==0?"Fuel":nearby_category==1?"Parking":"Hospital"):p==P_STOP?"Stop":p==P_DEST?"Destination":p==P_PLACE?"Saved place":p==P_NAME?"Rename":p==P_COORD?"Coordinates":p==P_REVIEW?"Review":p==P_LANG?(language_keyboard?"Keyboard language":"Language"):p==P_CONFIRM?(confirm_kind==7?"Add usual stop?":confirm_kind==6?"Route to usual parking?":confirm_kind==5?"Add to favourites?":confirm_kind==4?"Start suggested trip?":confirm_kind==3?(confirm_diversion_kind==1?"Use usual route?":"Take traffic diversion?"):confirm_kind==2?"Clear driving history?":confirm_kind?"Remove stop?":"Cancel route?"):p==P_PHONE_ACTIONS?"Device":p==P_PAIR?"Pair new phone":p==P_SOURCE?(source_action==DEST_PICK_ADD_STOP?"Add stop":source_action==DEST_PICK_CHANGE_DEST?"Change destination":"Choose source"):"Bluetooth";}
/* Scroll by screen pixels so a one-pixel resize cannot move the list
 * by a whole row. The viewport clips rows as they enter or leave it. */
struct MenuRows {int first,end,offset,top,height,previous_top,previous_bottom;};
static struct MenuRows menu_rows(int top,int height,int pitch,int count,int selected){
 if(height<0)height=0;
 int total=count*pitch,offset=(selected+1)*pitch-height;
 if(offset>total-height)offset=total-height;
 if(offset<0)offset=0;
 int end=(offset+height+pitch-1)/pitch;
 struct MenuRows rows={offset/pitch,end<count?end:count,offset,top,height,clip_y,clip_bottom};
 if(top>clip_y)clip_y=top;
 if(top+height<clip_bottom)clip_bottom=top+height;
 return rows;
}
static int menu_row_opacity(struct MenuRows rows,int y,int pitch,int opacity){
 int top=y>rows.top?y:rows.top;
 int bottom=rows.top+rows.height;
 if(y+pitch<bottom)bottom=y+pitch;
 int visible=bottom-top;
 return visible>=pitch?opacity:visible>0?(opacity*visible+pitch/2)/pitch:0;
}
static void menu_rows_end(struct MenuRows rows,int scrollbar_x,int pitch,int count,int opacity){
 hud_ink_alpha=opacity;ui_scrollbar(scrollbar_x,rows.top,rows.height,rows.offset,rows.height,count*pitch);
 clip_y=rows.previous_top;clip_bottom=rows.previous_bottom;
}
static void row_selection(struct HudBox b,int y,int index,int selected,int opacity,int height,int pitch){
 if(index!=selected)return;int offset=sheet_old_page==sheet_page?(sheet_old_focus-selected)*pitch:0;
 if(offset>height*3||offset< -height*3)offset=0;
 int q=frame_ease(menu_paint_at-focus_at,160),dy=q==256?0:hud_lerp(offset,0,q);
 menu_tile(b.x+9,y+dy,b.w-18,height,1,opacity);
}
static void row_text(struct HudBox b,int y,int index,int selected,const char*name,const char*value,int opacity,int face,int height){
 int pitch=height+((sheet_page==P_PLACES||sheet_page==P_NEARBY_LIST||sheet_page==P_LANG||sheet_page==P_REVIEW)?1:2);
 row_selection(b,y-2,index,selected,opacity,height,pitch);
 hud_ink_alpha=opacity;int cy=pair_text(b,y-2,height,face,name,value,index==selected?C_ACTIVE_TEXT:C_WHITE,index==selected?C_ACTIVE_TEXT:C_MUTED);
 if(value&&streq(value,">"))disclosure(b,cy,index==selected?C_ACTIVE_TEXT:C_MUTED);
}
static void trip_header(struct HudBox b,int p,int opacity){
 hud_ink_alpha=opacity;title_draw(b,"Trip");
 int w=(b.w-42)/4,total=w*4+18,x=b.x+(b.w-total)/2,tab=p==P_TRIP?0:p==P_NEARBY?3:places_tab+1;
 const char*names[]={"Itinerary","Recents","Favourites","Nearby"};
 for(int i=0;i<4;i++){int active=tab==i;menu_tile(x+i*(w+6),b.y+27,w,20,active,opacity);box_text(x+i*(w+6),b.y+27,w,20,0,ui_tr(names[i]),active?C_ACTIVE_TEXT:C_MUTED);}
}

static void trip_content(struct HudBox b,int selected,int opacity){
 trip_header(b,P_TRIP,opacity);char total[96]="";
 if(!trip.count){
  /* Close up the prompt and footer gradually as the available height falls. */
  int q=frame_ease(b.h>114?(u32)(b.h-114):0,36);
  int title_y=hud_lerp(47,60,q),help_y=hud_lerp(59,81,q),last_y=hud_lerp(71,98,q);
  int row_h=hud_lerp(17,25,q),actions_y=b.h-7-row_h;
  int bottom=clip_bottom;
  if(b.y+actions_y-3<clip_bottom)clip_bottom=b.y+actions_y-3;
  hud_ink_alpha=opacity;
  box_text(b.x+18,b.y+title_y,b.w-36,16,1,ui_tr("Where would you like to go?"),C_WHITE);
  text_value(b.x+18,b.y+help_y,b.w-36,hw_bluetooth_enabled?ui_tr("Browse Recents/Favourites, use ClioPlay Send"):ui_tr("Browse Recents/Favourites"),opacity);
  text_value(b.x+18,b.y+last_y,b.w-36,ui_tr("or enter coordinates"),opacity);
  clip_bottom=bottom;
  row_text(b,b.y+actions_y,0,selected,ui_tr("Coordinates"),"",opacity,1,row_h);
  return;
 }
 appendstr(total,sizeof(total),ui_tr(trip.active_idx==trip.count-1?"Destination: ":"Next: "));append_miles(total,sizeof(total),route_path_m());appendstr(total,sizeof(total),"  ~");append_eta_range(total,sizeof(total),sat_add32(traffic_remaining_cost(),atlas_future_cost),ml_route_var_ds);
 int remaining=(int)trip.count-1-(int)trip.active_idx;if(remaining>0){appendstr(total,sizeof(total),"  /  ");append_int(total,sizeof(total),remaining);appendstr(total,sizeof(total),ui_tr(" stops left"));}
 /* Reserve a full target row before making room for the route summary. */
 int summary=b.h-100;
 if(summary<0)summary=0;if(summary>20)summary=20;
 int list_top=b.y+49+summary,bottom=clip_bottom;
 if(summary){
  if(list_top<clip_bottom)clip_bottom=list_top;
  text_value(b.x+18,b.y+51,b.w-36,trip.active?total:ui_tr("Where would you like to go?"),opacity*summary/20);
  clip_bottom=bottom;
 }
 int list_space=b.h-79-summary,step=list_space/trip.count;
 if(step<21)step=21;if(step>26)step=26;
 int visible=list_space/step,show=trip.count<visible?trip.count:visible;
 int view_h=show>0?show*step:list_space;
 int view_top=list_top+(list_space-view_h+(view_h<list_space))/2;
 struct MenuRows rows=menu_rows(view_top,view_h,step,trip.count,selected);
 for(int i=rows.first;i<rows.end;i++){
  int y=rows.top+i*step-rows.offset,active=i==selected;
  int alpha=menu_row_opacity(rows,y,step,opacity);
  if(!alpha)continue;
  if(active)menu_tile(b.x+9,y,b.w-18,step-1,1,alpha);
  /* Centre each route marker in the selection highlight itself. */
  int cy=y+(step-1)/2;
  hud_ink_alpha=alpha;
  /* Connect only consecutive visible stops on the same page. */
  if(i+1<rows.end&&cy-4>=rows.top&&cy+step+4<rows.top+rows.height)
   round_rect(b.x+23,cy+5,1,step-9,0,C_BORDER,10);
  round_rect(b.x+19,cy-4,9,9,i==trip.count-1?2:4,active?C_ACTIVE_TEXT:i==trip.active_idx?C_CYAN:C_MUTED,15);
  if(active&&i==trip.active_idx)round_rect(b.x+22,cy-1,3,3,i==trip.count-1?0:1,C_ACTIVE,15);
  const char*state=ui_tr(i<trip.active_idx?"Done":i==trip.active_idx?"Next":i==trip.count-1?"Destination":"Stop");
  int text_cy=pair_text(b,y,step-1,1,place_display_name(trip.target[i].name),state,active?C_ACTIVE_TEXT:i<trip.active_idx?C_MUTED:C_WHITE,active?C_ACTIVE_TEXT:i==trip.active_idx?C_CYAN:C_MUTED);
  disclosure(b,text_cy,active?C_ACTIVE_TEXT:C_MUTED);
 }
 menu_rows_end(rows,b.x+b.w-6,step,trip.count,opacity);
 const char*actions[]={"+ Add stop","Cancel route",ml_offer_kind==1?"Usual route":"Diversion"};int action_n=2+traffic_offer_valid(),w=(b.w-20-(action_n-1)*8)/action_n;
 for(int i=0;i<action_n;i++){int x=b.x+10+i*(w+8),s=selected==trip.count+i;menu_tile(x,b.y+b.h-25,w,21,s,opacity);
  hud_ink_alpha=opacity;box_text(x,b.y+b.h-25,w,21,0,ui_tr(actions[i]),i==0&&trip.count>=TRIP_MAX_TARGETS?C_BORDER:s?C_ACTIVE_TEXT:C_MUTED);}
}
static void places_content(struct HudBox b,int selected,int opacity){
 trip_header(b,P_PLACES,opacity);int count=place_count();
 struct MenuRows rows=menu_rows(b.y+52,b.h-67,22,count,selected);
 for(int i=rows.first;i<rows.end;i++){
  int y=rows.top+i*22-rows.offset,alpha=menu_row_opacity(rows,y,22,opacity);
  if(alpha)row_text(b,y+2,i,selected,place_display_name(places_tab?faves[i].name:recents[i].name),">",alpha,1,21);
 }
 menu_rows_end(rows,b.x+b.w-6,22,count,opacity);
 if(!count)text_value(b.x+18,b.y+75,b.w-36,ui_tr(places_tab?"No favourites yet":"No recent places yet"),opacity);
}
static void nearby_content(struct HudBox b,int selected,int opacity){
 hud_ink_alpha=opacity;
 int top;
 if(places_picker){title_draw(b,"Nearby");top=b.y+34;}
 else{trip_header(b,P_NEARBY,opacity);top=b.y+54;}
 int space=b.y+b.h-10-top,step=space/NEARBY_CAT_COUNT;
 if(step<24)step=24;if(step>36)step=36;
 int start=top+(space-step*NEARBY_CAT_COUNT)/2;
 if(start<top)start=top;
 for(int i=0;i<NEARBY_CAT_COUNT;i++){
  int y=start+i*step,row_h=step-3,active=i==selected;
  row_selection(b,y,i,selected,opacity,row_h,step);
  hud_ink_alpha=opacity;
  poi_icon(nearby_kind_for_cat(i),b.x+25,y+row_h/2);
  int cy=pair_text(b,y,row_h,1,ui_tr(nearby_cat_title(i)),"",active?C_ACTIVE_TEXT:C_WHITE,active?C_ACTIVE_TEXT:C_MUTED);
  disclosure(b,cy,active?C_ACTIVE_TEXT:C_MUTED);
 }
}
static void nearby_star_icon(int cx,int cy,u16 col){
 static const u8 m[7][7]={
  { 0, 0, 0,14, 0, 0, 0},
  { 0, 0, 7,15, 7, 0, 0},
  { 0, 7,15,15,15, 7, 0},
  {14,15,15,15,15,15,14},
  { 0, 7,15,15,15, 7, 0},
  { 0, 0, 7,15, 7, 0, 0},
  { 0, 0, 0,14, 0, 0, 0},
 };
 for(int j=0;j<7;j++)for(int i=0;i<7;i++)if(m[j][i])blend_pixel(cx+i-3,cy+j-3,col,m[j][i]);
}
static void nearby_list_content(struct HudBox b,int selected,int opacity){
 hud_ink_alpha=opacity;title_draw(b,nearby_cat_title(nearby_category));
 poi_icon(nearby_kind_for_cat(nearby_category),b.x+25,b.y+15);
 struct MenuRows rows=menu_rows(b.y+32,b.h-40,22,nearby_count,selected);
 for(int i=rows.first;i<rows.end;i++){
  int y=rows.top+i*22-rows.offset,alpha=menu_row_opacity(rows,y,22,opacity);
  if(!alpha)continue;
  int active=i==selected,preferred=nearby_pois[i].preferred;
  char dist[24]="";append_nearby_distance(dist,sizeof(dist),nearby_pois[i].dist_m);
  row_selection(b,y,i,selected,alpha,21,22);
  hud_ink_alpha=alpha;
  int cy=pair_text(b,y,21,1,place_display_name(nearby_pois[i].name),dist,active?C_ACTIVE_TEXT:C_WHITE,active?C_ACTIVE_TEXT:(preferred?C_CYAN:C_MUTED));
  if(preferred)nearby_star_icon(b.x+23,cy,active?C_ACTIVE_TEXT:C_CYAN);
  disclosure(b,cy,active?C_ACTIVE_TEXT:C_MUTED);
 }
 menu_rows_end(rows,b.x+b.w-6,22,nearby_count,opacity);
 if(!nearby_count){
  const char*msg=nearby_status==1?"Map data missing":nearby_status==2?"Acquiring GPS":"No nearby places found";
  text_value(b.x+18,b.y+62,b.w-36,ui_tr(msg),opacity);
 }
}
static const char*entry_tail(char*out,unsigned cap,const char*text,int available){
 const char*p=text;while(*p&&width(p,1)>available)p+=utf8_span(p);copystr(out,cap,p);return out;
}
static void name_content(struct HudBox b,int opacity,u32 now){
 (void)now;pk_prepare();hud_ink_alpha=opacity;title_draw(b,menu_title(P_NAME));
 menu_tile(b.x+12,b.y+57,b.w-24,28,0,opacity);char text[193];entry_tail(text,sizeof(text),kbd_text,b.w-58);
 box_text(b.x+22,b.y+57,b.w-44,28,1,text[0]?text:ui_tr("Enter a name"),text[0]?C_WHITE:C_MUTED);
 struct Ink m=*ink_bounds(text,1);int caret=b.x+b.w/2+m.w/2+3;
 if(caret>b.x+b.w-25)caret=b.x+b.w-25;if(text[0])round_rect(caret,b.y+64,1,14,0,C_CYAN,14);
 int widths[]={112,56,28,42,28,60};int total=112+56+28+42+28+60+5*6,x=b.x+(b.w-total)/2;
 int ids[]={PK_LANGUAGE,PK_MODE,PK_PREV,-1,PK_NEXT,PK_SAVE};char pages[20]="";append_int(pages,sizeof(pages),pk_page+1);appendstr(pages,sizeof(pages)," / ");append_int(pages,sizeof(pages),pk_pages());
 const char*mode=pk_symbols?pk_native_names[kbd_language]:kbd_language==13?(pk_stage==1?"123":"音节"):kbd_language==14?(!pk_kana?"カナ":"123"):"123";
 const char*names[]={kbd_layouts[kbd_language].name,mode,"‹",pages,"›",keyboard_origin==KBD_NAME_COORDINATES?"Next":"Save"};
 int previous_x=x+112+6+56+6,counter_x=previous_x+28+6;
 struct Ink page_ink=*ink_bounds(pages,0);int counter_left=counter_x+(42-page_ink.w)/2;
 int left_arrow=previous_x+13,right_arrow=2*counter_left+page_ink.w-1-left_arrow;
 int arrow_y=body_centre(box_text_origin(b.y+28,23,0,pages),0);
 for(int i=0;i<6;i++){
  int valid=ids[i]>=0&&pk_valid(ids[i]);if(valid)menu_tile(x,b.y+28,widths[i],23,pk_focus==ids[i],opacity);hud_ink_alpha=opacity;
  u16 colour=valid?(pk_focus==ids[i]?C_ACTIVE_TEXT:C_WHITE):C_MUTED;
  if(i==2||i==4)menu_chevron(i==2?left_arrow:right_arrow,arrow_y,i==2?-1:1,colour);
  else box_text(x,b.y+28,widths[i],23,0,i==5?ui_tr(names[i]):names[i],colour);
  x+=widths[i]+6;
 }
 int count=pk_grid_rows(),height=b.h-100,step=height/count;
 int min_step=pk_ink_h[kbd_language]+8;
 if(step<min_step)step=min_step;if(step>40)step=40;
 int spare=height-count*step,start=b.y+92+(spare>0?spare/2:0),cw=(b.w-26)/pk_columns();
 struct MenuRows rows=menu_rows(start,height,step,count,pk_focus<100?pk_key_row(pk_focus):-1);
 for(int row=rows.first;row<rows.end;row++){
  int y=start+row*step-rows.offset,alpha=menu_row_opacity(rows,y,step,opacity);
  if(!alpha)continue;
  int n=pk_row_count(row),left=b.x+(b.w-n*cw+4)/2;
  for(int col=0;col<n;col++){int i=pk_row_start(row)+col,x=left+((kbd_language==12||kbd_language==17)?n-1-col:col)*cw;menu_tile(x,y,cw-4,step-4,pk_focus==i,alpha);
   char label_text[16];hud_ink_alpha=alpha;key_text(x,y,cw-4,step-4,2,pk_label(i,label_text),pk_focus==i?C_ACTIVE_TEXT:C_WHITE);
  }
 }
 menu_rows_end(rows,b.x+b.w-6,step,count,opacity);
}
static void coord_content(struct HudBox b,int opacity){
 hud_ink_alpha=opacity;title_draw(b,"Coordinates");
 int fw=(b.w-38)/2,field_y=b.y+27,field_h=32;
 for(int i=0;i<2;i++){
  int x=b.x+14+i*(fw+10),selected=i==coordinates.step;
  menu_tile(x,field_y,fw,field_h,selected,opacity);
  char title[193],value[193];
  fitted_text(title,ui_tr(i?"Longitude":"Latitude"),0,fw-16);
  fitted_text(value,coordinates.value[i][0]?coordinates.value[i]:"0.00000",1,fw-16);
  struct Ink title_ink=*ink_bounds(title,0),value_ink=*ink_bounds(value,1);
  const int gap=3;
  int top=field_y+(field_h-title_ink.h-gap-value_ink.h)/2;
  hud_ink_alpha=opacity;
  label(x+(fw-title_ink.w)/2-title_ink.l,top-title_ink.t,0,title,selected?C_ACTIVE_TEXT:C_MUTED,0);
  label(x+(fw-value_ink.w)/2-value_ink.l,top+title_ink.h+gap-value_ink.t,1,value,selected?C_ACTIVE_TEXT:C_WHITE,0);
 }
 /* Reserve a dedicated warning row ABOVE the keypad; in a compact sheet
  * putting the message at b.y+b.h used to clip it below the glass. */
 if(coordinates.error){
  hud_ink_alpha=opacity;
  box_text(b.x+14,b.y+61,b.w-28,13,0,
   ui_tr(coordinates.step==COORD_LAT?"Enter a valid latitude":"Enter a valid longitude"),C_WARN);
 }
 int start=b.y+76,step=(b.h-83)/4,cw=(b.w-28)/3;
 if(step<14)step=14;
 for(int i=0;i<COORD_KEY_COUNT;i++){
  int x,w,y=start+(i<COORD_SIGN?i/3:3)*step;
  coordinate_key_bounds(i,cw,5,&x,&w);x+=b.x+14;
  menu_tile(x,y,w,step-3,coordinates.focus==i,opacity);
  char t[2];hud_ink_alpha=opacity;
  box_text(x,y,w,step-3,1,coordinate_key_label(i,t),coordinates.focus==i?C_ACTIVE_TEXT:C_WHITE);
 }
}
static void review_content(struct HudBox b,int selected,int opacity){
 hud_ink_alpha=opacity;title_draw(b,"Review");
 const char*name=coordinates.name[0]?coordinates.name:ui_tr("Coordinates");box_text(b.x+15,b.y+29,b.w-30,18,1,name,C_CYAN);
 char pos[64]="";coordinate_format(pos,sizeof(pos),coordinates.lat);appendstr(pos,sizeof(pos),"   ");char lon[20];coordinate_format(lon,sizeof(lon),coordinates.lon);appendstr(pos,sizeof(pos),lon);text_value(b.x+15,b.y+49,b.w-30,pos,opacity);
 const char*actions[]={coordinates.action==DEST_PICK_ADD_STOP?"Add stop":coordinates.action==DEST_PICK_CHANGE_DEST?"Change destination":"Route as new","Edit coordinates","Rename","Cancel"};
 int step=(b.h-74)/4;
 for(int i=0;i<4;i++)row_text(b,b.y+70+i*step,i,selected,ui_tr(actions[i]),"",opacity,0,step-1);
}
static void settings_content(int page,struct HudBox b,int selected,int opacity,u32 now){
 (void)now;hud_ink_alpha=opacity;int group=settings_group(page),count=menu_count(page);
 title_draw(b,group<0?"Settings":settings_groups[group].title);
 int pitch=group<0?22:29;
 struct MenuRows rows=menu_rows(b.y+29,b.h-(group<0?43:48),pitch,count,selected);
 for(int focus=rows.first;focus<rows.end;focus++){
  int y=rows.top+focus*pitch-rows.offset,active=focus==selected;
  int alpha=menu_row_opacity(rows,y,pitch,opacity);
  if(!alpha)continue;
  if(group<0){row_text(b,y+2,focus,selected,ui_tr(settings_groups[focus].title),">",alpha,1,pitch-2);continue;}
  y+=2;
  int i=settings_row(page,focus);char value[96];const char*v=setting_value(i,value);
  row_selection(b,y,focus,selected,alpha,27,29);hud_ink_alpha=alpha;
  struct RowPair p=pair_layout(b,1,ui_tr(setting_names[i]),v,i!=15);
  pair_draw(&p,y,27,1,active?C_ACTIVE_TEXT:C_WHITE,active?C_ACTIVE_TEXT:C_MUTED,i!=15&&(i!=0||pref.bright>10),i!=15&&(i!=0||pref.bright<100));
  if(i==15)disclosure(b,pair_centre_y(&p,y,27,1),active?C_ACTIVE_TEXT:C_MUTED);
  /* Hours/minutes retain a focused-part underline; brightness has no slider. */
  if(active&&(i==8||i==9)){char part[3]={v[theme_time_part?3:0],v[theme_time_part?4:1],0};int x=p.vx+(theme_time_part?width("00:",1):0);
   round_rect(x,pair_origin(&p,y,27)+16,width(part,1),1,0,C_ACTIVE_TEXT,15);}
 }
 menu_rows_end(rows,b.x+b.w-6,pitch,count,opacity);
 box_text(b.x+12,b.y+b.h-14,b.w-24,10,0,"ClioPlay " CLIOPLAY_VERSION,C_MUTED);
}
static void phone_content(struct HudBox b,int selected,int opacity){
 hud_ink_alpha=opacity;title_draw(b,"Bluetooth");
 menu_tile(b.x+10,b.y+31,b.w-20,24,selected==0,opacity);hud_ink_alpha=opacity;
 box_text(b.x+10,b.y+31,b.w-20,24,1,ui_tr("Pair new phone"),selected==0?C_ACTIVE_TEXT:C_CYAN);
 struct MenuRows rows=menu_rows(b.y+63,b.h-72,31,nbtdev,selected-1);
 for(int i=rows.first;i<rows.end;i++){
  int y=rows.top+i*31-rows.offset,active=i+1==selected;
  int alpha=menu_row_opacity(rows,y,31,opacity);
  if(!alpha)continue;
  row_selection(b,y,i+1,selected,alpha,29,31);hud_ink_alpha=alpha;
  box_text(b.x+34,y+1,b.w-68,15,1,bt_display_name(i),active?C_ACTIVE_TEXT:C_WHITE);
  const char*state=ui_tr(btdev[i].connected?"Connected":"Saved");
  box_text(b.x+34,y+17,b.w-68,10,0,state,active?C_ACTIVE_TEXT:C_MUTED);
  int top=body_centre(box_text_origin(y+1,15,1,bt_display_name(i)),1),bottom=body_centre(box_text_origin(y+17,10,0,state),0);
  /* A two-line device card keeps the cue centred across both text bodies. */
  disclosure(b,(top+bottom)/2,active?C_ACTIVE_TEXT:C_MUTED);
 }
 menu_rows_end(rows,b.x+b.w-6,31,nbtdev,opacity);
 if(!nbtdev)text_value(b.x+16,b.y+84,b.w-32,ui_tr("No saved phones"),opacity);
}
static void menu_contents(int p,int selected,struct HudBox b,int opacity,u32 now){
 if(!p||opacity<=0)return;hud_ink_alpha=opacity;
 if(p==P_TRIP){trip_content(b,selected,opacity);return;}
 if(p==P_PLACES){places_content(b,selected,opacity);return;}
 if(p==P_NEARBY){nearby_content(b,selected,opacity);return;}
 if(p==P_NEARBY_LIST){nearby_list_content(b,selected,opacity);return;}
 if(p==P_NAME){name_content(b,opacity,now);return;}
 if(p==P_COORD){coord_content(b,opacity);return;}
 if(p==P_REVIEW){review_content(b,selected,opacity);return;}
 if(p==P_SETTINGS||settings_group(p)>=0){settings_content(p,b,selected,opacity,now);return;}
 if(p==P_PHONE){phone_content(b,selected,opacity);return;}
 if(p==P_CONFIRM)box_text(b.x+14,b.y+4,b.w-28,22,CONFIRMATION_FACE,ui_tr(menu_title(p)),C_WHITE);
 else title_draw(b,menu_title(p));
 const char*options[6]={0};int count=0;
 if(p==P_SOURCE){options[0]="Recents";options[1]="Favourites";options[2]="Nearby";options[3]="Coordinates";count=4;}
 if(p==P_HOME){options[0]="Trip";options[1]=hw_bluetooth_enabled?"Bluetooth":"Settings";
  if(hw_bluetooth_enabled)options[2]="Settings";
  count=hw_bluetooth_enabled?3:2;}
 if(p==P_STOP){options[0]="Move earlier";options[1]="Move later";options[2]="Remove stop";count=3;}
 if(p==P_DEST){options[0]="Change destination";options[1]=trip.count&&fave_contains(trip.target[trip.count-1].lat,trip.target[trip.count-1].lon)>=0?"Remove favourite":"Save favourite";options[2]="Cancel route";count=3;}
 if(p==P_PLACE){options[0]="Route as new";options[1]="Add as stop";options[2]=(!place_from_nearby&&places_tab)?"Rename":"Save favourite";options[3]="Remove favourite";count=(!place_from_nearby&&places_tab)?4:3;}
 if(p==P_PHONE_ACTIONS){int index=menu_phone_index();if(index<0)return;text_value(b.x+18,b.y+31,b.w-36,bt_display_name(index),opacity);options[0]=btdev[index].connected?"Disconnect":"Connect";options[1]="Forget";count=2;}
 if(p==P_CONFIRM){
  char detail[193]="";
  if(confirm_kind==7)copystr(detail,sizeof(detail),menu_stop_current()?(confirm_stop.name[0]?place_display_name(confirm_stop.name):ui_tr("Map point")):ui_tr("Suggestion unavailable"));
  else if(confirm_kind==6)copystr(detail,sizeof(detail),menu_parking_current()?(confirm_parking_name[0]?place_display_name(confirm_parking_name):ui_tr("Usual parking available")):ui_tr("Suggestion unavailable"));
  else if(confirm_kind==5)copystr(detail,sizeof(detail),ui_tr("You come here often"));
  else if(confirm_kind==4)copystr(detail,sizeof(detail),menu_suggestion_current()?place_display_name(confirm_suggestion.name):ui_tr("Suggestion unavailable"));
  else if(confirm_kind==3){
   if(menu_diversion_current()){
    if(confirm_diversion_kind==1&&traffic_saving<600u)copystr(detail,sizeof(detail),ui_tr("Based on your usual route"));
    else{copystr(detail,sizeof(detail),ui_tr("Save approximately "));append_eta(detail,sizeof(detail),traffic_saving);}
   }else copystr(detail,sizeof(detail),ui_tr(confirm_diversion_kind==1?"Route unavailable":"Diversion unavailable"));
  }
  else if(confirm_kind==2)copystr(detail,sizeof(detail),ui_tr("Delete learned journeys and traffic"));
  else copystr(detail,sizeof(detail),place_display_name(confirm_kind?context_name:trip.count?trip.target[trip.count-1].name:""));
  int detail_y=b.y+29;
  if(confirm_kind==4&&b.h>=106&&width(detail,0)>b.w-28){
   char first[193],second[193];
   if(hud_split_title(detail,0,b.w-28,first,second)){
    box_text(b.x+14,detail_y,b.w-28,14,0,first,C_MUTED);detail_y+=14;
    copystr(detail,sizeof(detail),second);
   }
  }
  box_text(b.x+14,detail_y,b.w-28,14,0,detail,C_MUTED);
  if(b.h>=92&&((confirm_kind==3&&menu_diversion_current())||(confirm_kind==4&&menu_suggestion_current())||
     (confirm_kind==6&&menu_parking_current())||(confirm_kind==7&&menu_stop_current()))){
   const char*sub=confirm_kind==3?(confirm_diversion_kind==0?ui_tr("Likely traffic ahead"):traffic_saving>=600u?ui_tr("Based on your usual route"):ui_tr("Based on driving history")):ui_tr("Based on driving history");
   box_text(b.x+14,detail_y+14,b.w-28,14,0,sub,C_MUTED);
  }
  int has_view=confirm_has_view()&&b.h>=114;
  int w=(b.w-40)/2,row1_y=b.y+b.h-(has_view?62:34);const char*choices[]={"Yes","No"};
  for(int i=0;i<2;i++){int x=b.x+14+i*(w+12);menu_tile(x,row1_y,w,24,selected==i,opacity);hud_ink_alpha=opacity;box_text(x,row1_y,w,24,CONFIRMATION_FACE,ui_tr(choices[i]),selected==i?C_ACTIVE_TEXT:C_WHITE);}
  if(has_view){
   int vy=b.y+b.h-32,vw=b.w-28;
   menu_tile(b.x+14,vy,vw,24,selected==2,opacity);hud_ink_alpha=opacity;
   box_text(b.x+14,vy,vw,24,CONFIRMATION_FACE,ui_tr(confirm_diversion_kind==1?"View route":"View diversion"),selected==2?C_ACTIVE_TEXT:C_WHITE);
  }
  return;
 }
 if(p==P_PAIR){text_value(b.x+18,b.y+42,b.w-36,bt_status_text(),opacity);text_value(b.x+18,b.y+64,b.w-36,ui_tr("Waiting for phone"),opacity);return;}
 if(p==P_LANG){
  int selected_lang=language_keyboard?kbd_pick:selected;
  struct MenuRows rows=menu_rows(b.y+32,b.h-36,24,UI_LANGUAGE_COUNT,selected_lang);
  for(int i=rows.first;i<rows.end;i++){
   int y=rows.top+i*24-rows.offset,alpha=menu_row_opacity(rows,y,24,opacity);
   if(alpha)row_text(b,y+2,i,selected_lang,kbd_layouts[i].name,"",alpha,1,23);
  }
  menu_rows_end(rows,b.x+b.w-6,24,UI_LANGUAGE_COUNT,opacity);return;
 }
 int start=p==P_PHONE_ACTIONS?60:34;
 if(p==P_STOP||p==P_DEST||p==P_PLACE){text_value(b.x+18,b.y+30,b.w-36,place_display_name(context_name),opacity);start=56;}
 int step=(b.h-start-9)/(count?count:1);
 for(int i=0;i<count;i++){
  row_text(b,b.y+start+i*step,i,selected,ui_tr(options[i]),"",opacity,1,step-2);
 }
}

static int menu_blocks_phone(void){return screen!=SCR_MAP&&(sheet_page==P_SOURCE||sheet_page==P_PLACE||sheet_page==P_STOP||sheet_page==P_DEST||sheet_page==P_CONFIRM||sheet_page==P_NAME||sheet_page==P_COORD||sheet_page==P_REVIEW||(sheet_page==P_LANG&&language_keyboard));}
static void menu_reset(u32 now){
 driving_info_ok_armed=0;
 sheet_page=sheet_old_page=P_CLOSED;sheet_focus=sheet_old_focus=sheet_depth=0;places_picker=review_from_place=place_from_nearby=0;confirm_top_focus=0;diversion_preview_clear();
 sheet_from_box=sheet_to_box=(struct HudBox){0};bottom_from=bottom_to=256;bottom_at=menu_at=focus_at=now-320u;
 menu_interaction_at=menu_presented_at=now-320u;menu_pending=0;memzero(trip_tab_focus,sizeof(trip_tab_focus));pk_reset();
}
static void menu_open(int p,int selected,u32 now){
 driving_info_ok_armed=0;
 if(!hw_bluetooth_enabled&&(p==P_PHONE||p==P_PHONE_ACTIONS||p==P_PAIR))p=P_HOME;
 coordinates_active=0;bt_control_mode=0;sheet_depth=0;
 if(p==P_NAME){keyboard_origin=kbd_action;pk_reset();pk_prepare();}
 if(p!=P_HOME){sheet_stack_page[0]=P_HOME;sheet_stack_focus[0]=0;sheet_depth=1;}
 menu_change(p,selected,now);menu_pending=1;
}
#ifdef HOST_TEST
/* Native fixtures enter the same category/row used by physical input. */
static void menu_open_setting(int row,u32 now){
 menu_open(P_SETTINGS,0,now);
 for(int g=0;g<SETTINGS_GROUP_COUNT;g++)for(int i=0;i<settings_groups[g].count;i++)if(settings_groups[g].rows[i]==row){
  sheet_focus=g;menu_push(P_SET_DISPLAY+g,i,now);set_focus=row;theme_time_part=0;return;
 }
}
#endif
static void menu_sync(u32 now){
 if(((sheet_page==P_CONFIRM&&confirm_kind==5)||
     (keyboard_origin==KBD_NAME_FREQUENT&&(sheet_page==P_NAME||(sheet_page==P_LANG&&language_keyboard))))
    &&!frequent_current(confirm_frequent,now)){menu_close(now);return;}
 if(!hw_bluetooth_enabled&&(sheet_page==P_PHONE||sheet_page==P_PHONE_ACTIONS||sheet_page==P_PAIR||screen==SCR_BT)){
  sheet_depth=0;menu_change(P_HOME,0,now);return;
 }
 if(screen==SCR_MAP){if(sheet_page)menu_close(now);return;}
 if(sheet_page&&menu_screen(sheet_page)==screen){
  if(sheet_page==P_PHONE_ACTIONS){int i=menu_phone_index();if(i<0){menu_back(now);return;}phone_index=i;}
  if(sheet_page==P_PAIR){
   if(!btui.connected||starts(btui.status,"BT PAIR")||starts(btui.status,"BT WAIT"))menu_pair_waited=1;
   if(btui.connected&&(menu_pair_waited||!menu_pair_before||!streq(menu_pair_name,btui.device))){menu_back(now);sheet_focus=bt_connected_idx()+1;}
  }
  if(sheet_page==P_STOP&&(!trip.active||stop_index>=trip.count-1))menu_return_trip(trip.active_idx,now);
  if(sheet_page==P_DEST&&(!trip.active||!trip.count))menu_return_trip(0,now);
  int count=menu_count(sheet_page);
  if(count&&sheet_focus>=count)sheet_focus=count-1;
  if(sheet_page==P_TRIP&&trip.count>=TRIP_MAX_TARGETS&&sheet_focus==trip.count)sheet_focus++;
  return;
 }
 /* Imports are used by startup/phone/coordinate services and native captures;
  * physical input normally enters through menu_open/menu_change. */
 int p=screen==SCR_MENU?P_HOME:screen==SCR_TRIP?P_TRIP:screen==SCR_BT?P_PHONE:screen==SCR_SET?P_SETTINGS:screen==SCR_KEY?(kbd_picker?P_LANG:P_NAME):coordinates.step==COORD_REVIEW?P_REVIEW:P_COORD;
 int selected=p==P_HOME?menu_focus:p==P_SETTINGS?0:p==P_PHONE?bt_page_focus:p==P_TRIP?trip.active_idx:0;
 if(p==P_NAME||p==P_LANG){keyboard_origin=kbd_action;pk_reset();pk_prepare();language_keyboard=p==P_LANG;}
 sheet_depth=p==P_HOME?0:1;sheet_stack_page[0]=P_HOME;sheet_stack_focus[0]=0;
 menu_change(p,selected,now);
}
static void menu_prepare(u32 now){
 menu_sync(now);
 if(sheet_page==P_NAME||sheet_old_page==P_NAME)pk_prepare();
 if(sheet_page)sheet_to_box=menu_box(sheet_page);
 int visible=compact_page(sheet_page)?256:0;
 if(visible!=bottom_to){bottom_from=bottom_pose(now);bottom_to=visible;bottom_at=now;}
}
static int menu_animating(u32 now){
 return (sheet_page||sheet_from_box.a)&&((now-menu_at<320u||(s32)(menu_presented_at-menu_at)<320)||
  now-bottom_at<320u||(s32)(menu_presented_at-bottom_at)<320||
  now-menu_interaction_at<160u||(s32)(menu_presented_at-menu_interaction_at)<160);
}
static void menu_settle(u32 now){menu_at=focus_at=bottom_at=now-320u;menu_interaction_at=menu_presented_at=now-320u;sheet_from_box=sheet_to_box;bottom_from=bottom_to;}
static void menu_dispatch(int code,u32 now){
 menu_sync(now);int previous_key=pk_focus,previous_page=pk_page,previous_symbols=pk_symbols,previous_stage=pk_stage,previous_language=kbd_language;
 menu_key(code,now);screen=menu_screen(sheet_page);menu_pending=1;
 if(pk_focus!=previous_key||pk_page!=previous_page||pk_symbols!=previous_symbols||pk_stage!=previous_stage||kbd_language!=previous_language)menu_interaction_at=now;
 if(sheet_page==P_HOME)menu_focus=sheet_focus;
 int row=settings_row(sheet_page,sheet_focus);if(row>=0)set_focus=row;
}
static void menu_paint(u32 now){
 menu_paint_at=now;
 const int instruments[]={HC_SPEED,HC_ROAD,HC_TURN,HC_TURN_NEXT};int visibility=bottom_pose(now);
 for(unsigned i=0;i<sizeof(instruments)/sizeof(instruments[0]);i++){
  struct HudTween*t=&hud_tween[instruments[i]];int from=t->from.a,to=t->target.box.a;
  t->from.a=from*visibility/256;t->target.box.a=to*visibility/256;hud_draw_card(instruments[i],now);t->from.a=from;t->target.box.a=to;
 }
 struct HudBox b=menu_pose(now);
 if(b.a){
  hud_glass(b,0);int x=clip_x,y=clip_y,r=clip_right,bottom=clip_bottom;
  clip_x=b.x+4;clip_y=b.y+4;clip_right=b.x+b.w-4;clip_bottom=b.y+b.h-4;
  int q=frame_ease(now-menu_at,220),tab=places_tab;
  if((sheet_old_page!=sheet_page||sheet_old_tab!=tab)&&q<256){places_tab=sheet_old_tab;menu_contents(sheet_old_page,sheet_old_focus,b,(256-q)*b.a/4096,now);places_tab=tab;}
  menu_contents(sheet_page,sheet_focus,b,q*b.a/4096,now);
  clip_x=x;clip_y=y;clip_right=r;clip_bottom=bottom;hud_ink_alpha=16;
 }
 hud_draw_card(HC_CLOCK,now);hud_draw_card(HC_STATUS,now);
}
static void menu_note_presented(u32 now){
 menu_presented_at=now;
 if(!sheet_page&&!menu_pose(now).a)sheet_old_page=P_CLOSED;
}
static void menu_draw(u32 now){
 menu_prepare(now);menu_paint(now);menu_note_presented(now);
}
