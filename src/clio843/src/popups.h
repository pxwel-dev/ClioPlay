/* Shared live popup model and presentation. Action handlers retain ownership of
 * modal state; copied view data survives closing, deletion and page changes. */
enum {POPUP_PHONE=100,POPUP_KEYBOARD=101,POPUP_ITEMS=9};
struct PopupItem {int x,y,w,face,button;u16 ink;char text[193];};
struct PopupView {int key,count,selected,scroll_first,scroll_visible,scroll_total;struct HudBox box;struct PopupItem item[POPUP_ITEMS];};
static struct {
 struct PopupView card,previous;
 struct HudBox from,to,focus_from,focus_to;
 u32 at,content_at,focus_at,presented_at;
 int key;
} popup;
static int popup_requested_key(void){
 if(modal!=MOD_NONE)return modal;
 if(screen==SCR_KEY&&kbd_picker)return POPUP_KEYBOARD;
 if(screen==SCR_BT&&bt_page_dialog>=0&&bt_page_dialog<nbtdev)return POPUP_PHONE;
 return 0;
}
static void popup_text(struct PopupView*v,int x,int y,int w,int face,const char*s,u16 ink,int button){
 if(v->count>=POPUP_ITEMS)return;
 struct PopupItem*i=&v->item[v->count++];i->x=x;i->y=y;i->w=w;i->face=face;i->ink=ink;i->button=button;
 copystr(i->text,sizeof(i->text),s);
}
static void popup_option(struct PopupView*v,int x,int y,int w,const char*s,int choice){
 if(choice==modal_choice)v->selected=v->count;
 popup_text(v,x,y,w,1,s,C_WHITE,1);
}
static void popup_build(struct PopupView*v,int key){
 memzero(v,sizeof(*v));v->key=key;v->selected=-1;
 if(key==POPUP_KEYBOARD){
  v->box=hud_box(76,MENU_BODY_Y,328,222);
  popup_text(v,88,52,304,1,kbd_language_labels[pref.language],C_WHITE,0);
  int first=kbd_pick>=7?kbd_pick-6:0;if(first+7>UI_LANGUAGE_COUNT)first=UI_LANGUAGE_COUNT-7;
  v->scroll_first=first;v->scroll_visible=7;v->scroll_total=UI_LANGUAGE_COUNT;
  for(int row=0;row<7;row++){
   int i=first+row;if(i==kbd_pick)v->selected=v->count;
   popup_text(v,88,77+row*26,296,1,kbd_layouts[i].name,C_WHITE,1);
  }return;
 }
 if(key==POPUP_PHONE){
  v->key+=bt_page_dialog*256;v->box=hud_box(76,105,328,92);
  popup_text(v,90,117,300,1,bt_display_name(bt_page_dialog),C_WHITE,0);
  popup_text(v,90,138,300,0,ui_tr("Saved phone"),C_MUTED,0);
  v->selected=2+bt_page_dialog_choice;
  popup_text(v,96,157,132,0,ui_tr("Reconnect"),C_WHITE,1);
  popup_text(v,252,157,132,0,ui_tr("Forget"),C_WHITE,1);return;
 }
 if(key==MOD_ARRIVED_STOP||key==MOD_ARRIVED_DEST){
  v->box=hud_box(74,100,332,74);
  popup_text(v,80,116,320,1,ui_tr(key==MOD_ARRIVED_STOP?"Arrived at stop":"Destination reached"),C_WHITE,0);
  char hint[64];copystr(hint,sizeof(hint),ui_tr(key==MOD_ARRIVED_STOP?"Continue in ":"Finish in "));
  u32 now=s_now_ms(),left=arrival_modal_deadline&&time_before_u32(now,arrival_modal_deadline)?arrival_modal_deadline-now:0;
  append_int(hint,sizeof(hint),(int)((left+999u)/1000u));appendstr(hint,sizeof(hint),"s");
  popup_text(v,85,145,310,0,hint,C_CYAN,0);return;
 }
 if(key==MOD_WAIT_PHONE){
  v->key+=phone_wait_action*256;v->box=hud_box(62,82,356,112);
  popup_text(v,80,96,320,1,ui_tr(phone_wait_action==DEST_PICK_CHANGE_DEST?"Change destination":phone_wait_action==DEST_PICK_ADD_STOP?"Add stop":"Receive destination"),C_WHITE,0);
  popup_text(v,80,126,320,1,ui_tr("Send a destination"),C_CYAN,0);
  popup_text(v,80,143,320,1,ui_tr("from ClioPlay Send"),C_CYAN,0);return;
 }
 if(key==MOD_SOURCE_PICK){
  v->key+=dest_pick_action*256;v->box=hud_box(62,44,356,192);
  popup_text(v,80,56,320,1,ui_tr(dest_pick_action==DEST_PICK_CHANGE_DEST?"Change destination":"Add stop"),C_WHITE,0);
  const char*options[]={"Recents","Favourites","Send from phone","Coordinates","Cancel"};
  for(int i=0;i<5;i++)popup_option(v,100,84+i*29,280,ui_tr(options[i]),i);return;
 }
 int y=key==MOD_FAVE&&trip.active?54:78,h=key==MOD_FAVE&&trip.active?174:126;
 v->box=hud_box(62,y,356,h);
 const char*title=key==MOD_ROUTE_POINT?(pending_point_stop?"Add point as stop?":"Route to point?"):
  key==MOD_CANCEL_ROUTE?"Cancel current route?":key==MOD_REMOVE_STOP?"Remove stop?":key==MOD_RESUME_TRIP?"Resume saved trip?":
  key==MOD_RECENT?"Recent trip":key==MOD_FAVE?"Favourite":"Manage target";
 popup_text(v,80,y+13,320,1,ui_tr(title),C_WHITE,0);
 if(key==MOD_ROUTE_POINT||key==MOD_CANCEL_ROUTE||key==MOD_REMOVE_STOP||key==MOD_RESUME_TRIP){
  popup_option(v,135,130,90,ui_tr("Yes"),0);popup_option(v,255,130,90,ui_tr("No"),1);return;
 }
 if(key==MOD_RECENT||key==MOD_FAVE){
  int row=0,top=key==MOD_RECENT?112:trip.active?84:116,step=key==MOD_RECENT?27:24;
  if(trip.active){popup_option(v,100,top,280,ui_tr("Add as stop"),row++);}
  popup_option(v,100,top+row*step,280,ui_tr("Route as new"),row);row++;
  if(key==MOD_RECENT)popup_option(v,100,top+row*step,280,ui_tr("Add to favourites"),row);
  else{
   popup_option(v,100,top+row*step,280,ui_tr("Rename favourite"),row);row++;
   popup_option(v,100,top+row*step,280,ui_tr("Remove favourite"),row);
  }return;
 }
 if(key==MOD_TARGET&&modal_index>=0&&modal_index<trip.count){
  if(modal_index<trip.count-1){
   popup_option(v,100,112,280,ui_tr("Remove stop"),0);popup_option(v,100,139,280,ui_tr("Move up"),1);popup_option(v,100,166,280,ui_tr("Move down"),2);
  }else{
   popup_option(v,100,125,280,ui_tr("Change destination"),0);
   popup_option(v,100,154,280,ui_tr(fave_contains(trip.target[modal_index].lat,trip.target[modal_index].lon)>=0?"Remove favourite":"Add to favourites"),1);
  }
 }
}
static struct HudBox popup_lerp_box(struct HudBox a,struct HudBox b,int q){
 if(q==256)return b;if(!q)return a;
 return (struct HudBox){hud_lerp(a.x,b.x,q),hud_lerp(a.y,b.y,q),hud_lerp(a.w,b.w,q),hud_lerp(a.h,b.h,q),hud_lerp(a.a,b.a,q)};
}
static struct HudBox popup_pose(u32 now){return popup_lerp_box(popup.from,popup.to,hud_ease(now-popup.at,320));}
static struct HudBox popup_focus(u32 now){return popup_lerp_box(popup.focus_from,popup.focus_to,hud_ease(now-popup.focus_at,160));}
static struct HudBox popup_item_box(const struct PopupView*v){
 if(v->selected<0||v->selected>=v->count)return (struct HudBox){0};
 const struct PopupItem*i=&v->item[v->selected];return (struct HudBox){i->x-v->box.x,i->y-v->box.y,i->w,22,256};
}
static int popup_text_changed(const struct PopupView*a,const struct PopupView*b){
 if(a->count!=b->count)return 1;
 for(int i=0;i<a->count;i++)if(!streq(a->item[i].text,b->item[i].text)||a->item[i].ink!=b->item[i].ink)return 1;
 return 0;
}
static void popup_reset(void){memzero(&popup,sizeof(popup));}
static void popup_prepare(u32 now){
 int key=popup_requested_key();
 if(!key){
  if(popup.key){
   struct HudBox b=popup_pose(now);popup.from=b;popup.to=b;
   /* Retain the live content/focus phase while fading out: interrupting a
    * text or selection transition must not jump to its final appearance. */
   popup.to.x+=12;popup.to.y+=8;popup.to.w-=24;popup.to.h-=16;popup.to.a=0;popup.at=now;popup.key=0;
  }return;
 }
 struct PopupView next;popup_build(&next,key);
 struct HudBox here=popup_pose(now);
 int changed=popup.key!=next.key||!hud_box_eq(popup.to,next.box);
 if(changed){
  popup.previous=popup.card;popup.card=next;
  popup.from=here;popup.to=next.box;popup.at=now;popup.key=next.key;
  if(here.a<=0){popup.from=next.box;popup.from.x+=12;popup.from.y+=8;popup.from.w-=24;popup.from.h-=16;popup.from.a=0;popup.content_at=now-320u;}
  else popup.content_at=now;
  popup.focus_from=popup.focus_to=popup_item_box(&next);popup.focus_at=now-160u;
 }else{
  if(popup_text_changed(&popup.card,&next)){
   if(now-popup.content_at>=110u)popup.previous=popup.card;
   popup.content_at=now;
  }
  struct HudBox focus=popup_item_box(&next);
  if(!hud_box_eq(popup.focus_to,focus)){popup.focus_from=popup_focus(now);popup.focus_to=focus;popup.focus_at=now;}
  popup.card=next;
 }
}
static int popup_animating(u32 now){
 return (popup.from.a||popup.to.a)&&((now-popup.at<320u||(s32)(popup.presented_at-popup.at)<320)||
  (popup.to.a&&(now-popup.content_at<320u||(s32)(popup.presented_at-popup.content_at)<320||
   now-popup.focus_at<160u||(s32)(popup.presented_at-popup.focus_at)<160)));
}
static void popup_settle_for_capture(u32 now){
 popup.at=now-320u;popup.content_at=now-320u;popup.focus_at=now-160u;popup.presented_at=now;
}
static void draw_modal(void){
 u32 now=hud_now_ms();popup_prepare(now);popup.presented_at=now;
 struct HudBox b=popup_pose(now);if(b.a<=0)return;
 hud_glass(b,0);
 int ox=clip_x,oy=clip_y,orr=clip_right,ob=clip_bottom,oa=hud_ink_alpha;
 if(b.x+8>clip_x)clip_x=b.x+8;if(b.y+5>clip_y)clip_y=b.y+5;
 if(b.x+b.w-8<clip_right)clip_right=b.x+b.w-8;if(b.y+b.h-5<clip_bottom)clip_bottom=b.y+b.h-5;
 const struct PopupView*v=&popup.card;unsigned age=now-popup.content_at;int opacity=256;
 if(age<110u){v=&popup.previous;opacity=256-hud_ease(age,110u);}
 else if(age<320u)opacity=hud_ease(age-110u,210u);
 int alpha=(b.a*opacity+128)>>8;
 int dx=b.x+b.w/2-(v->box.x+v->box.w/2),dy=b.y-v->box.y;
 for(int i=0;i<v->count;i++)if(v->item[i].button){
  const struct PopupItem*r=&v->item[i];
  hud_glass_surface((struct HudBox){r->x+dx,r->y+dy,r->w,22,alpha},theme_resolved?rgb(26,39,54):rgb(226,235,244),C_BORDER,5,0);
 }
 struct HudBox focus=age<110u?popup_item_box(v):popup_focus(now);
 if(focus.a){
  focus.x+=v->box.x+dx;focus.y+=v->box.y+dy;focus.a=alpha;
  /* Day selections retain the menu's exact blue rather than a pale blend. */
  hud_glass_surface(focus,C_ACTIVE,C_CYAN,theme_resolved?12u:16u,0);
 }
 hud_ink_alpha=(16*alpha+128)>>8;
 ui_scrollbar(v->box.x+316+dx,v->box.y+35+dy,178,v->scroll_first,v->scroll_visible,v->scroll_total);
 for(int i=0;i<v->count;i++){
  const struct PopupItem*r=&v->item[i];
  u16 ink=r->ink;
  if(r->button&&focus.a){
   int x0=r->x+dx>focus.x?r->x+dx:focus.x,y0=r->y+dy>focus.y?r->y+dy:focus.y;
   int x1=r->x+dx+r->w<focus.x+focus.w?r->x+dx+r->w:focus.x+focus.w;
   int y1=r->y+dy+22<focus.y+focus.h?r->y+dy+22:focus.y+focus.h;
   if(x1>x0&&y1>y0)ink=hud_mix16(ink,C_ACTIVE_TEXT,(unsigned)((x1-x0)*(y1-y0)*16/(r->w*22)));
  }
  compact_center(r->x+dx+r->w/2,r->y+dy+(r->button?3:0),r->w- (r->button?16:0),r->face,r->text,ink);
 }
 clip_x=ox;clip_y=oy;clip_right=orr;clip_bottom=ob;hud_ink_alpha=oa;
}
