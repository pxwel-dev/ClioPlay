/* Decimal-degree destination entry. All conversion is bounded integer arithmetic;
 * drafts never touch saved places or the itinerary until the review is confirmed. */
enum {COORD_LAT=0,COORD_LON=1,COORD_REVIEW=2};
/* Row-major order: 1–9, sign, decimal point, zero, Next. */
enum {COORD_SIGN=9,COORD_DOT,COORD_ZERO,COORD_NEXT,COORD_KEY_COUNT};
struct CoordinateDraft {
 int step,focus,review_focus,action,error;
 char value[2][16],name[161];
 s32 lat,lon;
};
static struct CoordinateDraft coordinates;

/* Up to seven fractional digits; round once to the router's 1e-5 degree units.
 * Check exact geographical bounds before rounding (90.000001 is invalid). */
static int coordinate_parse(const char*s,int limit,s32*out){
 if(!s||!*s)return 0;
 int negative=0,whole=0,digits=0,fraction=0,places=0,round=0,nonzero=0;
 if(*s=='-'||*s=='+'){negative=*s=='-';s++;}
 while(*s>='0'&&*s<='9'){
  if(++digits>3)return 0;
  whole=whole*10+*s++-'0';if(whole>limit)return 0;
 }
 if(!digits)return 0;
 if(*s=='.'){
  s++;int scale=10000;
  while(*s>='0'&&*s<='9'){
   int d=*s++-'0';if(++places>7)return 0;
   nonzero|=d;
   if(places<=5){fraction+=d*scale;scale/=10;}else if(places==6)round=d>=5;
  }
  if(!places)return 0;
 }
 if(*s||(whole==limit&&nonzero))return 0;
 s32 value=(s32)(whole*100000+fraction+round);*out=negative?-value:value;return 1;
}
static void coordinate_format(char*b,unsigned cap,s32 value){
 b[0]=0;if(value<0){appendstr(b,cap,"-");value=-value;}
 append_int(b,cap,(int)(value/100000));appendstr(b,cap,".");
 int fraction=(int)(value%100000);
 for(int scale=10000;scale;scale/=10){char c[2]={(char)('0'+fraction/scale),0};appendstr(b,cap,c);fraction%=scale;}
}
static const char*coordinates_title(void){
 return ui_tr(coordinates.step==COORD_LAT?"Latitude":coordinates.step==COORD_LON?"Longitude":"Review");
}
static void coordinates_save_name(void){
 const char*p=kbd_text;while(*p==' ')p++;
 copystr_limit(coordinates.name,sizeof(coordinates.name),p,KBD_TEXT_LIMIT);
 unsigned n=slen(coordinates.name);while(n&&coordinates.name[n-1]==' ')coordinates.name[--n]=0;
}
static void coordinates_name_done(void){
 coordinates_save_name();coordinates.step=COORD_REVIEW;coordinates.review_focus=0;screen=SCR_COORDINATES;
}
static void coordinates_open_name(void){
 kbd_action=KBD_NAME_COORDINATES;kbd_fave_idx=-1;
 copystr(kbd_text,sizeof(kbd_text),coordinates.name);keyboard_begin();screen=SCR_KEY;
}
static void coordinates_open(int action){
 memzero(&coordinates,sizeof(coordinates));coordinates.action=action;
 coordinates_active=1;modal=MOD_NONE;screen=SCR_COORDINATES;
}
static void coordinates_leave(int sources){
 coordinates_active=0;screen=SCR_TRIP;
 if(sources&&coordinates.action!=DEST_PICK_NONE){dest_pick_action=coordinates.action;modal=MOD_SOURCE_PICK;modal_choice=2;}
}
/* Soft-right always cycles between the two coordinate fields; it never
 * silently submits the destination. The on-screen Next key validates both. */
static void coordinates_next(void){
 coordinates.step=coordinates.step==COORD_LAT?COORD_LON:COORD_LAT;
 coordinates.error=0;
}
static int coordinates_finish(void){
 s32 lat,lon;
 if(!coordinate_parse(coordinates.value[COORD_LAT],90,&lat)){
  coordinates.step=COORD_LAT;coordinates.error=1;return 0;
 }
 if(!coordinate_parse(coordinates.value[COORD_LON],180,&lon)){
  coordinates.step=COORD_LON;coordinates.error=1;return 0;
 }
 coordinates.lat=lat;coordinates.lon=lon;coordinates.error=0;
 coordinates_open_name();return 1;
}
static void coordinates_delete(void){
 char*b=coordinates.value[coordinates.step];unsigned n=slen(b);if(n)b[n-1]=0;coordinates.error=0;
}
static void coordinates_type(int key){
 char*b=coordinates.value[coordinates.step];unsigned n=slen(b);coordinates.error=0;
 if(key==COORD_SIGN){
  if(b[0]=='-'){for(unsigned i=0;i<n;i++)b[i]=b[i+1];}
  else if(n+1<sizeof(coordinates.value[0])){for(unsigned i=n+1;i>0;i--)b[i]=b[i-1];b[0]='-';}
  return;
 }
 if(key==COORD_DOT){
  for(unsigned i=0;i<n;i++)if(b[i]=='.')return;
  if(!n||streq(b,"-")){appendstr(b,sizeof(coordinates.value[0]),"0");n++;}
  if(n+1<sizeof(coordinates.value[0]))appendstr(b,sizeof(coordinates.value[0]),".");return;
 }
 /* Limit the useful precision and whole-part length before appending. */
 const char*dot=0;unsigned whole=0;
 for(const char*p=b;*p;p++){if(*p=='.')dot=p;else if(!dot&&*p!='-')whole++;}
 if((dot&&slen(dot+1)>=7)||(!dot&&whole>=3)||n+1>=sizeof(coordinates.value[0]))return;
 char c[2]={(char)(key==COORD_ZERO?'0':'1'+key),0};appendstr(b,sizeof(coordinates.value[0]),c);
}
/* Share the layout and traversal between the sheet and legacy renderer.
 * The bottom-left column is split in two; vertical arrows align by column,
 * with the sign as the entry point to that split and horizontal access to dot. */
static void coordinate_key_bounds(int key,int cw,int gap,int*x,int*w){
 int width=cw-gap;
 *x=(key%3)*cw;*w=width;
 if(key==COORD_SIGN||key==COORD_DOT){
  *w=(width-gap)/2;*x=key==COORD_SIGN?0:*w+gap;
  if(key==COORD_DOT)*w=width-*x;
 }else if(key==COORD_ZERO)*x=cw;
 else if(key==COORD_NEXT)*x=2*cw;
}
static const char*coordinate_key_label(int key,char b[2]){
 if(key==COORD_SIGN)return "±";
 if(key==COORD_DOT)return ".";
 if(key==COORD_NEXT)return ui_tr("Next");
 b[0]=key==COORD_ZERO?'0':(char)('1'+key);b[1]=0;return b;
}
static void coordinate_move(int code){
 int f=coordinates.focus,row=f<COORD_SIGN?f/3:3;
 int col=row<3?f%3:f<=COORD_DOT?0:f==COORD_ZERO?1:2;
 if(code==K_ROTCW||code==K_ROTCCW){
  coordinates.focus=(f+(code==K_ROTCW?1:-1)+COORD_KEY_COUNT)%COORD_KEY_COUNT;return;
 }
 if(code==K_LEFT||code==K_RIGHT){
  int start=row*3,n=row==3?4:3;
  coordinates.focus=start+(f-start+(code==K_RIGHT?1:-1)+n)%n;return;
 }
 row=(row+(code==K_DOWN?1:3))%4;
 coordinates.focus=row<3?row*3+col:col==0?COORD_SIGN:col==1?COORD_ZERO:COORD_NEXT;
}
static void coordinates_submit(void){
 char name[161];copystr(name,sizeof(name),coordinates.name);
 if(!name[0]){coordinate_format(name,sizeof(name),coordinates.lat);appendstr(name,sizeof(name),", ");char lon[20];coordinate_format(lon,sizeof(lon),coordinates.lon);appendstr(name,sizeof(name),lon);}
 int action=coordinates.action;
 if(action==DEST_PICK_ADD_STOP){
  if(!trip.active||!trip.count){notice_fail(ui_tr("No active route"));return;}
  if(trip.count>=TRIP_MAX_TARGETS){notice_fail(ui_tr("Trip full - stop not added"));return;}
  if(!trip_add_stop(name,coordinates.lat,coordinates.lon)){notice_fail(ui_tr("Stop not added"));return;}
  add_recent(name,coordinates.lat,coordinates.lon);
 }else if(action==DEST_PICK_CHANGE_DEST){
  if(!trip.active||!trip.count){notice_fail(ui_tr("No active route"));return;}
  trip_change_destination(name,coordinates.lat,coordinates.lon);
 }else set_destination(name,coordinates.lat,coordinates.lon);
 coordinates_active=0;trip_pick_mode=dest_pick_action=DEST_PICK_NONE;
 /* The explicit on-screen confirmation takes precedence over a phone packet
  * received during entry. Cancellation leaves that packet available instead. */
 phone_destination_clear();modal=MOD_NONE;
 if(action==DEST_PICK_NONE){screen=SCR_MAP;recenter_map();}
 else{screen=SCR_TRIP;trip_pane=1;trip_right_focus=0;}
}
static void coordinates_handle(int code){
 if(code==K_BACK){
  if(coordinates.step==COORD_REVIEW)coordinates_open_name();
  else coordinates_leave(1);
  return;
 }
 if(coordinates.step==COORD_REVIEW){
  if((code==K_UP||code==K_LEFT)&&coordinates.review_focus>0)coordinates.review_focus--;
  else if((code==K_DOWN||code==K_RIGHT)&&coordinates.review_focus<3)coordinates.review_focus++;
  else if(code==K_ENTER){
   if(coordinates.review_focus==0)coordinates_submit();
   else if(coordinates.review_focus==1){coordinates.step=COORD_LAT;coordinates.focus=0;coordinates.error=0;}
   else if(coordinates.review_focus==2)coordinates_open_name();else coordinates_leave(0);
  }
  return;
 }
 if(code==K_ESC){coordinates_delete();return;}
 if(code==K_SR){coordinates_next();return;}
 if(code==K_LEFT||code==K_RIGHT||code==K_UP||code==K_DOWN||code==K_ROTCW||code==K_ROTCCW)coordinate_move(code);
 else if(code==K_ENTER){if(coordinates.focus==COORD_NEXT)coordinates_finish();else coordinates_type(coordinates.focus);}
}
static void draw_coordinates(void){
 fill(0,0,W,H,C_BG);
 if(coordinates.step==COORD_REVIEW){
  keyboard_surface(12,MENU_BODY_Y,456,80,0);
  char lat[20],lon[20];coordinate_format(lat,sizeof(lat),coordinates.lat);coordinate_format(lon,sizeof(lon),coordinates.lon);
  compact_fit(24,51,432,1,coordinates.name[0]?coordinates.name:ui_tr("Coordinates"),C_WHITE);
  compact_fit(24,78,208,0,ui_tr("Latitude"),C_MUTED);compact_fit(248,78,208,0,ui_tr("Longitude"),C_MUTED);
  compact_fit(24,94,208,1,lat,C_WHITE);compact_fit(248,94,208,1,lon,C_WHITE);
  const char*options[]={coordinates.action==DEST_PICK_ADD_STOP?ui_tr("Add stop"):coordinates.action==DEST_PICK_CHANGE_DEST?ui_tr("Change destination"):ui_tr("Route as new"),ui_tr("Edit coordinates"),ui_tr("Edit name"),ui_tr("Cancel")};
  for(int i=0;i<4;i++){int y=132+i*32,f=coordinates.review_focus==i;keyboard_surface(12,y,456,28,f);compact_center(240,y+6,432,1,options[i],ui_choice_text(f));}
  return;
 }
 int axis=coordinates.step;const char*value=coordinates.value[axis];
 keyboard_surface(12,MENU_BODY_Y,456,44,0);
 compact_fit(24,50,430,3,value[0]?value:"0.00000",value[0]?C_WHITE:C_MUTED);
 if(coordinates.error)compact_fit(24,112,432,0,ui_tr(axis==COORD_LAT?"Enter a valid latitude":"Enter a valid longitude"),C_WARN);
 for(int i=0;i<COORD_KEY_COUNT;i++){
  int x,w,y=131+(i<COORD_SIGN?i/3:3)*29,f=coordinates.focus==i;char b[2];
  coordinate_key_bounds(i,132,8,&x,&w);x+=42;
  keyboard_surface(x,y,w,25,f);compact_center(x+w/2,y+3,w-8,1,coordinate_key_label(i,b),ui_choice_text(f));
 }
}
