/* Importance colours are theme-aware so Day confirmations stay legible. */
static u16 hud_status_colour(int level,u16 normal){
 switch(level){
 case HUD_SUCCESS:return theme_resolved?rgb(113,221,171):rgb(20,108,72);
 case HUD_WARNING:return C_WARN;
 case HUD_ERROR:return theme_resolved?rgb(255,113,129):rgb(190,42,50);
 default:return normal;
 }
}
/* Small, allocation-free queue for state changes and action confirmations.
 * A message's lifetime is spent on screen, not while waiting behind controls.
 * GPS/map/idle-route slots coalesce to their latest state; actions retain order. */
enum {HUD_EVENT_GPS,HUD_EVENT_MAP,HUD_EVENT_ROUTE,HUD_EVENT_ACTION,HUD_EVENT_COUNT=8};
struct HudEvent {char text[112];u32 order,left;int level;};
static struct HudEvent hud_events[HUD_EVENT_COUNT];
static u32 hud_event_order,hud_event_at,hud_event_displayed_order;
static int hud_event_visible=-1,hud_seen_gps=-1,hud_seen_map=-1,hud_seen_idle=-1;

static void hud_status_reset(void){
 memzero(hud_events,sizeof(hud_events));hud_event_order=hud_event_at=hud_event_displayed_order=0;
 hud_event_visible=-1;hud_seen_gps=hud_seen_map=hud_seen_idle=-1;
}
static void hud_event_set(int slot,const char*text,int level){
 struct HudEvent*e=&hud_events[slot];
 if(!text||!text[0]){e->left=0;return;}
 if(e->left&&streq(e->text,text))return;
 copystr(e->text,sizeof(e->text),text);e->level=level;
 e->left=4200;e->order=++hud_event_order;
}
static void hud_status_notice(const char*text,int level){
 int slot=-1,oldest=HUD_EVENT_ACTION;
 for(int i=HUD_EVENT_ACTION;i<HUD_EVENT_COUNT;i++){
  if(hud_events[i].left&&streq(hud_events[i].text,text))return;
  if(!hud_events[i].left&&slot<0)slot=i;
  if((s32)(hud_events[i].order-hud_events[oldest].order)<0)oldest=i;
 }
 /* A bounded burst drops the oldest action, never the GPS/map state slots. */
 hud_event_set(slot<0?oldest:slot,text,level);
}
static void hud_status_observe(int map,u32 now){
 int fix=gps.fix!=0;
 if(fix!=hud_seen_gps){
  int first=hud_seen_gps<0;hud_seen_gps=fix;
  hud_event_set(HUD_EVENT_GPS,!fix?ui_tr("Acquiring GPS"):first?0:ui_tr("GPS active"),fix?HUD_SUCCESS:HUD_WARNING);
 }
 if(trip.active||route_status!=R_IDLE){hud_events[HUD_EVENT_ROUTE].left=0;hud_seen_idle=0;}
 if(!map)return;
 int state=!mapok?1:((gps.fix||freecam||track_valid)&&!map_view_covered(view_lat(),view_lon())?2:(mh.flags&1?3:0));
 if(state!=hud_seen_map){
  hud_seen_map=state;
  hud_event_set(HUD_EVENT_MAP,state==1?ui_tr("Map data missing"):state==2?ui_tr("Outside map area"):state==3?ui_tr("Test map"):0,state==1?HUD_ERROR:state==2?HUD_WARNING:HUD_INFO);
 }
 int idle=!trip.active&&route_status==R_IDLE;
 if(idle!=hud_seen_idle){
  hud_seen_idle=idle;
  hud_event_set(HUD_EVENT_ROUTE,idle?ui_tr("NO ACTIVE ROUTE"):0,HUD_INFO);
 }
 (void)now;
}
static void hud_status_compose(struct HudCard*c,int map,u32 now){
 if(hud_event_visible>=0){
  struct HudEvent*e=&hud_events[hud_event_visible];
  if(e->order==hud_event_displayed_order){
   u32 elapsed=now-hud_event_at;e->left=elapsed>=e->left?0:e->left-elapsed;
  }
 }
 hud_event_at=now;hud_event_visible=-1;
 /* Action dialogs own the user's attention. Pending notifications wait. */
 if(modal!=MOD_NONE)return;
 char*line=0;unsigned cap=0;int shift=0;
 if(!c->kind){line=c->title;cap=sizeof(c->title);shift=0;}
 else if(!map){
  if(!c->detail[0]&&!c->connection[0]&&!c->feedback[0]){line=c->detail;cap=sizeof(c->detail);shift=2;}
 }else if(!c->feedback[0]){line=c->feedback;cap=sizeof(c->feedback);shift=6;}
 else if(!c->connection[0]){line=c->connection;cap=sizeof(c->connection);shift=4;}
 else if(!c->detail[0]){line=c->detail;cap=sizeof(c->detail);shift=2;}
 if(!line)return;
 int selected=-1;
 for(int i=0;i<HUD_EVENT_COUNT;i++)if(hud_events[i].left){
  if(selected<0||((hud_events[i].level>=HUD_WARNING)>(hud_events[selected].level>=HUD_WARNING))||
     ((hud_events[i].level>=HUD_WARNING)==(hud_events[selected].level>=HUD_WARNING)&&
      (s32)(hud_events[i].order-hud_events[selected].order)<0))selected=i;
 }
 if(selected<0)return;
 struct HudEvent*e=&hud_events[selected];
 copystr(line,cap,e->text);c->ink|=e->level<<shift;
 if(!c->kind)c->kind=HUD_SYSTEM;
 hud_event_visible=selected;hud_event_displayed_order=e->order;
}
