/* Expandable live GPS HUD inside the top clock/temperature pill (HC_CLOCK).
 * Adapts smoothly across three tiers while keeping seconds always visible:
 *   GPS_HUD_MIN     (27 px)  — Menus and Explore (time with seconds + temp)
 *   GPS_HUD_COMPACT (52 px)  — Popups (header + live compact GPS strip, keeping popups centred)
 *   GPS_HUD_FULL    (148 px) — Driving map (compass + telemetry + recessed Altitude/Lat/Lon tray)
 * Every vector mark uses sampled subpixel coverage so it follows glass clip
 * and fade transitions cleanly in both Day and Night themes. */

/* Geometry is in 1/8 pixel units; pixel (x,y) has its centre at x*8+4. */
static void gps_cover(int x,int y,int cov,u16 c,int a){if(cov>0)blend_pixel(x,y,c,(cov*a+8)>>4);}
/* Annulus between ri and ro (ri=0 gives a disc). */
static void gps_ring(int cx8,int cy8,int ro,int ri,u16 c,int a){
 int x0=(cx8-ro)/8-1,x1=(cx8+ro)/8+1,y0=(cy8-ro)/8-1,y1=(cy8+ro)/8+1;
 int ro2=ro*ro,ri2=ri*ri,outer=(ro+6)*(ro+6),inner=ro>6?(ro-6)*(ro-6):0,hole=(ri+6)*(ri+6),edge=ri>6?(ri-6)*(ri-6):0;
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
  int dx=x*8+4-cx8,dy=y*8+4-cy8,d=dx*dx+dy*dy;
  if(d>outer||(ri&&d<edge))continue;
  if(d<=inner&&(!ri||d>=hole)){gps_cover(x,y,16,c,a);continue;}
  int cov=0;
  for(int sy=1;sy<8;sy+=2)for(int sx=1;sx<8;sx+=2){int px_=x*8+sx-cx8,py_=y*8+sy-cy8,e=px_*px_+py_*py_;cov+=e<=ro2&&e>=ri2;}
  gps_cover(x,y,cov,c,a);
 }
}
static int gps_edge(int ax,int ay,int bx,int by,int px_,int py_){return (bx-ax)*(py_-ay)-(by-ay)*(px_-ax);}
static void gps_triangle(int ax,int ay,int bx,int by,int cx,int cy,u16 c,int a){
 if(gps_edge(ax,ay,bx,by,cx,cy)<0){int tx=bx,ty=by;bx=cx;by=cy;cx=tx;cy=ty;}
 int lo_x=ax<bx?(ax<cx?ax:cx):(bx<cx?bx:cx),hi_x=ax>bx?(ax>cx?ax:cx):(bx>cx?bx:cx);
 int lo_y=ay<by?(ay<cy?ay:cy):(by<cy?by:cy),hi_y=ay>by?(ay>cy?ay:cy):(by>cy?by:cy);
 for(int y=lo_y/8-1;y<=hi_y/8+1;y++)for(int x=lo_x/8-1;x<=hi_x/8+1;x++){
  int cov=0;
  for(int sy=1;sy<8;sy+=2)for(int sx=1;sx<8;sx+=2){int px_=x*8+sx,py_=y*8+sy;
   cov+=gps_edge(ax,ay,bx,by,px_,py_)>=0&&gps_edge(bx,by,cx,cy,px_,py_)>=0&&gps_edge(cx,cy,ax,ay,px_,py_)>=0;}
  gps_cover(x,y,cov,c,a);
 }
}
/* Convex quadrilateral a-b-c-d in either winding order. */
static void gps_quad(int ax,int ay,int bx,int by,int cx,int cy,int dx,int dy,u16 c,int a){
 if(gps_edge(ax,ay,bx,by,cx,cy)<0){int tx=bx,ty=by;bx=dx;by=dy;dx=tx;dy=ty;}
 int xs[4]={ax,bx,cx,dx},ys[4]={ay,by,cy,dy},lo_x=ax,hi_x=ax,lo_y=ay,hi_y=ay;
 for(int i=1;i<4;i++){if(xs[i]<lo_x)lo_x=xs[i];if(xs[i]>hi_x)hi_x=xs[i];if(ys[i]<lo_y)lo_y=ys[i];if(ys[i]>hi_y)hi_y=ys[i];}
 for(int y=lo_y/8-1;y<=hi_y/8+1;y++)for(int x=lo_x/8-1;x<=hi_x/8+1;x++){
  int cov=0;
  for(int sy=1;sy<8;sy+=2)for(int sx=1;sx<8;sx+=2){int px_=x*8+sx,py_=y*8+sy,in=1;
   for(int i=0;i<4&&in;i++)in=gps_edge(xs[i],ys[i],xs[(i+1)&3],ys[(i+1)&3],px_,py_)>=0;cov+=in;}
  gps_cover(x,y,cov,c,a);
 }
}
/* Capsule of half-width hw around segment a-b. */
static void gps_segment(int ax,int ay,int bx,int by,int hw,u16 c,int a){
 int lo_x=(ax<bx?ax:bx)-hw,hi_x=(ax>bx?ax:bx)+hw,lo_y=(ay<by?ay:by)-hw,hi_y=(ay>by?ay:by)+hw;
 long long vx=bx-ax,vy=by-ay,len=vx*vx+vy*vy,hw2=(long long)hw*hw;
 for(int y=lo_y/8-1;y<=hi_y/8+1;y++)for(int x=lo_x/8-1;x<=hi_x/8+1;x++){
  int cov=0;
  for(int sy=1;sy<8;sy+=2)for(int sx=1;sx<8;sx+=2){
   long long wx=x*8+sx-ax,wy=y*8+sy-ay,t=wx*vx+wy*vy,d;
   if(t<=0||!len)d=wx*wx+wy*wy;
   else if(t>=len){long long ex=wx-vx,ey=wy-vy;d=ex*ex+ey*ey;}
   else{long long cr=wx*vy-wy*vx;cov+=cr*cr<=hw2*len;continue;}
   cov+=d<=hw2;
  }
  gps_cover(x,y,cov,c,a);
 }
}
static void gps_trig(int deg,int*s,int*c){
 deg%=360;if(deg<0)deg+=360;int k=deg/10,f=deg%10,n=(k+1)%36;
 *s=(SIN36[k]*(10-f)+SIN36[n]*f)/10;*c=(COS36[k]*(10-f)+COS36[n]*f)/10;
}
/* Point at radius r8 (1/8 px) and compass bearing deg, screen y downward. */
static void gps_polar(int cx8,int cy8,int r8,int deg,int*x,int*y){int s,c;gps_trig(deg,&s,&c);*x=cx8+(r8*s)/32767;*y=cy8-(r8*c)/32767;}

/* Accuracy colour: green up to 5 m, amber up to 15 m, red beyond. */
static u16 gps_error_colour(int dm){return hud_status_colour(dm<=50?HUD_SUCCESS:dm<=150?HUD_WARNING:HUD_ERROR,C_CYAN);}
static u16 gps_needle_colour(void){return theme_resolved?0xfb2c:0xd925;}

/* Crisp plus-minus sign (absent from Bitstream Vera font.bin): an exact
 * odd-width square '+' (equal horizontal and vertical arms centred on one
 * pixel) above a matching '-' bar separated by a 1 px gap.
 * Bounding box: w x (w + 2) px (5x7 for face 0, 7x9 for face 1). */
static void gps_plus_minus(int x,int y,int size,u16 c){
 int w=size|1,arm=w/2;
 hud_rect_ink(x,y+arm,w,1,c);
 hud_rect_ink(x+arm,y,1,w,c);
 hud_rect_ink(x,y+w+1,w,1,c);
}

/* Three rising signal-quality bars (8x9 px) tinted by fix quality. */
static void gps_signal_bars(int x,int y,int fix,int sats,int dm){
 int lit=!fix?0:sats>=8?3:sats>=5?2:sats>0?1:0;
 u16 active=dm>=0?gps_error_colour(dm):C_CYAN;
 const int heights[3]={3,6,9};
 for(int i=0;i<3;i++){
  int h=heights[i];
  round_rect(x+i*3,y+9-h,2,h,0,i<lit?active:C_BORDER,i<lit?15:11);
 }
}

/* Precision recessed compass dial with 15/30 degree ticks, cardinal markers
 * aligned to the exact subpixel pivot axes (cx+0.5, cy.0), theme-tuned red
 * heading arrow and tiny machined centre pivot cap. */
static void gps_dial(int cx,int cy,int r,int heading,int dim){
 /* Odd-width N/S (7 px) centre cleanly on x=cx+0.5; even-height E/W (8 px)
  * centre cleanly on y=cy.0, matching the needle pivot with zero pixel bias. */
 int cx8=cx*8+4,cy8=cy*8,r8=r*8;
 u16 well=theme_resolved?rgb(10,17,28):rgb(212,223,236);
 u16 bezel=dim?C_BORDER:(theme_resolved?rgb(96,122,152):rgb(122,144,168));
 gps_ring(cx8,cy8,r8-6,0,well,10);
 gps_ring(cx8,cy8,r8,r8-12,bezel,15);
 for(int d=0;d<360;d+=15){
  if(d%90==0)continue;
  int ax,ay,bx,by,major=d%30==0;
  gps_polar(cx8,cy8,r8-18,d,&ax,&ay);
  gps_polar(cx8,cy8,r8-(major?42:28),d,&bx,&by);
  gps_segment(ax,ay,bx,by,major?5:4,major?C_MUTED:C_BORDER,15);
 }
 struct Ink mn=*ink_bounds("N",0),me=*ink_bounds("E",0),ms=*ink_bounds("S",0),mw=*ink_bounds("W",0);
 int pad=r-4;
 u16 c_n=dim?C_BORDER:hud_status_colour(HUD_ERROR,C_WHITE),c_o=dim?C_BORDER:C_MUTED;
 label(cx-mn.w/2-mn.l,cy-pad-mn.t,0,"N",c_n,0);
 label(cx-ms.w/2-ms.l,cy+pad-ms.h-ms.t,0,"S",c_o,0);
 label(cx+pad-me.w+1-me.l,cy-me.h/2-me.t,0,"E",c_o,0);
 label(cx-pad-mw.l,cy-mw.h/2-mw.t,0,"W",c_o,0);
 if(heading<0)return;
 u16 red=gps_needle_colour();
 int tip=r8-30,base=r8*26/100,half=r8*24/100,thick=r8*10/100,tail=r8*34/100;
 int tx,ty,mx,my,lx,ly,rx,ry,ex,ey,fx,fy,q[8];
 gps_polar(cx8,cy8,tip,heading,&tx,&ty);gps_polar(cx8,cy8,base,heading,&mx,&my);
 gps_polar(mx,my,half,heading+90,&lx,&ly);gps_polar(mx,my,half,heading-90,&rx,&ry);
 gps_polar(cx8,cy8,tail,heading+180,&ex,&ey);gps_polar(cx8,cy8,base+thick*2,heading,&fx,&fy);
 gps_polar(ex,ey,thick,heading+90,&q[0],&q[1]);gps_polar(fx,fy,thick,heading+90,&q[2],&q[3]);
 gps_polar(fx,fy,thick,heading-90,&q[4],&q[5]);gps_polar(ex,ey,thick,heading-90,&q[6],&q[7]);
 gps_quad(q[0],q[1],q[2],q[3],q[4],q[5],q[6],q[7],red,15);
 gps_triangle(tx,ty,lx,ly,rx,ry,red,15);
 /* Subtle centre pivot cap. */
 gps_ring(cx8,cy8,20,0,theme_resolved?C_WHITE:rgb(42,58,76),14);
 gps_ring(cx8,cy8,9,0,well,15);
}

/* Miniature live compass badge (14x14 px) for the compact popup strip. */
static void gps_dial_mini(int cx,int cy,int r,int heading,int dim){
 int cx8=cx*8+4,cy8=cy*8+4,r8=r*8;
 u16 well=theme_resolved?rgb(10,17,28):rgb(212,223,236);
 u16 bezel=dim?C_BORDER:(theme_resolved?rgb(96,122,152):rgb(122,144,168));
 u16 red=gps_needle_colour();
 gps_ring(cx8,cy8,r8-4,0,well,9);
 gps_ring(cx8,cy8,r8,r8-10,bezel,15);
 gps_ring(cx8,cy8-r8+4,7,0,dim?C_BORDER:hud_status_colour(HUD_ERROR,C_WHITE),15);
 if(heading<0)return;
 int tip=r8-12,tail=r8*46/100,half=r8*36/100;
 int tx,ty,bx,by,lx,ly,rx,ry;
 gps_polar(cx8,cy8,tip,heading,&tx,&ty);
 gps_polar(cx8,cy8,tail,heading+180,&bx,&by);
 gps_polar(bx,by,half,heading+90,&lx,&ly);
 gps_polar(bx,by,half,heading-90,&rx,&ry);
 gps_triangle(tx,ty,lx,ly,rx,ry,red,15);
}

static void gps_accuracy_text(char*b,unsigned cap,int dm){
 b[0]=0;int imperial=display_imperial(),v=imperial?(dm*328+500)/1000:(dm+5)/10;if(v<1)v=1;
 if(v>999){appendstr(b,cap,">");v=999;}
 append_int(b,cap,v);appendstr(b,cap,imperial?" ft":" m");
}
static void gps_time_text(char*b,unsigned cap){
 int minute=local_clock_minutes();
 if(minute<0){copystr(b,cap,"--:--:--");return;}
 int s=gps.sec<0?0:gps.sec>59?59:gps.sec,parts[3]={minute/60,minute%60,s};b[0]=0;
 for(int i=0;i<3;i++){if(i)appendstr(b,cap,":");char d[3]={(char)('0'+parts[i]/10),(char)('0'+parts[i]%10),0};appendstr(b,cap,d);}
}
/* Marine degrees and decimal minutes, e.g. "47° 29.887' N" or "001° 39.383' W".
 * Latitude pads degrees to two digits and longitude to three. The fonts have no
 * prime glyph, so minutes use an apostrophe. 1e-5 degree is 0.6 thousandths of
 * a minute, so the rounded thousandths are (fraction*3+2)/5, at most 59999. */
static void gps_coordinate_text(char*b,unsigned cap,s32 e5,char positive,char negative){
 u32 a=(u32)(e5<0?-(long long)e5:(long long)e5);int deg=(int)(a/100000u),milli=(int)(((a%100000u)*3u+2u)/5u);
 b[0]=0;for(int p=positive=='N'?10:100;p>1&&deg<p;p/=10)appendstr(b,cap,"0");
 append_int(b,cap,deg);appendstr(b,cap,"° ");
 int minutes=milli/1000,thousandths=milli%1000;
 if(minutes<10)appendstr(b,cap,"0");append_int(b,cap,minutes);appendstr(b,cap,".");
 if(thousandths<100)appendstr(b,cap,"0");if(thousandths<10)appendstr(b,cap,"0");append_int(b,cap,thousandths);
 char h[4]={'\'',' ',e5<0?negative:positive,0};appendstr(b,cap,h);
}
/* Altitude above mean sea level, rounded to whole metres or feet. */
static void gps_altitude_text(char*b,unsigned cap,int dm){
 int imperial=display_imperial();long long v=imperial?((long long)dm*328):((long long)dm*100);
 v=v<0?-((-v+500)/1000):(v+500)/1000;b[0]=0;append_int(b,cap,(int)v);appendstr(b,cap,imperial?" ft":" m");
}
/* The last course while a fix is held. Below the course-update speed the map
 * freezes its course, so the arrow holds still instead of following GPS noise. */
static int gps_panel_heading(void){
 if(!gps.fix)return -1;
 int c=map_course();return c>=0&&c<360?c:-1;
}
static const char*gps_cardinal_text(int heading){
 static const char*n[8]={"N","NE","E","SE","S","SW","W","NW"};
 return n[((heading+22)/45)&7];
}
static void gps_heading_degrees(char*b,unsigned cap,int heading){
 b[0]=0;
 if(heading<100)appendstr(b,cap,"0");
 if(heading<10)appendstr(b,cap,"0");
 append_int(b,cap,heading);appendstr(b,cap,"°");
}

static int gps_hud_mode(void){
 if(!pref.gps_hud)return GPS_HUD_OFF;
 if(freecam)return GPS_HUD_MIN;
 if(screen!=SCR_MAP&&sheet_page!=P_CONFIRM)return GPS_HUD_MIN;
 if(modal!=MOD_NONE||(screen!=SCR_MAP&&sheet_page==P_CONFIRM))return GPS_HUD_COMPACT;
 return screen==SCR_MAP?GPS_HUD_FULL:GPS_HUD_MIN;
}

static void gps_hud_toggle(void){
 pref.gps_hud=!pref.gps_hud;
 prefs_mark_dirty();
 render_background_dirty=1;
}

/* Key-value row on one baseline inside the lower tray, RTL-mirrored for ar/he. */
static void gps_tray_row(int x,int y,int w,const char*key,const char*val,u16 val_col){
 int origin=box_text_origin(y,14,0,"0"),rtl=ui_language_rtl();
 int vw=width(val,0),kw=width(key,0),avail=w-vw-4;
 if(avail<0)avail=0;
 if(rtl){
  label(x,origin,0,val,val_col,0);
  if(kw<=avail)label(x+w-kw,origin,0,key,C_MUTED,0);
  else compact_fit(x+vw+4,y+2,avail,0,key,C_MUTED);
 }else{
  if(kw<=avail)label(x,origin,0,key,C_MUTED,0);
  else compact_fit(x,y+2,avail,0,key,C_MUTED);
  label(x+w-vw,origin,0,val,val_col,0);
 }
}

static void gps_hud_draw_compact(struct HudBox b,int base_alpha,int fix,int heading,int dm){
 int mid_y=b.y+39;
 gps_dial_mini(b.x+15,mid_y,7,heading,!fix);
 if(!fix){
  box_text(b.x+27,b.y+29,b.w-37,21,0,ui_tr("No fix"),C_MUTED);
  return;
 }
 char hdg[20]="";
 if(heading<0)copystr(hdg,sizeof(hdg),"--");
 else{gps_heading_degrees(hdg,sizeof(hdg),heading);appendstr(hdg,sizeof(hdg)," ");appendstr(hdg,sizeof(hdg),gps_cardinal_text(heading));}
 box_text(b.x+24,b.y+29,52,21,0,hdg,heading<0?C_MUTED:C_WHITE);
 hud_rect_ink(b.x+78,b.y+34,1,11,C_BORDER);
 if(dm<0){
  box_text(b.x+80,b.y+29,48,21,0,"--",C_MUTED);
 }else{
  char acc[16];gps_accuracy_text(acc,sizeof(acc),dm);
  u16 col=gps_error_colour(dm);
  int pm=5,gap=2,tw=width(acc,0),total=pm+gap+tw,sx=b.x+80+(48-total)/2;
  int origin=box_text_origin(b.y+29,21,0,"0");
  gps_plus_minus(sx,origin+4,pm,col);
  label(sx+pm+gap,origin,0,acc,col,0);
 }
 hud_rect_ink(b.x+130,b.y+34,1,11,C_BORDER);
 char sc[8]="";append_int(sc,sizeof(sc),gps.sats>0?gps.sats:0);
 int bw=8,gap=2,sw=width(sc,0),sx=b.x+132+(26-(bw+gap+sw))/2;
 int origin=box_text_origin(b.y+29,21,0,"0");
 gps_signal_bars(sx,origin+2,fix,gps.sats,dm);
 label(sx+bw+gap,origin,0,sc,C_MUTED,0);
 (void)base_alpha;
}

static void gps_hud_draw_medium(struct HudBox b,int fix,int heading,int dm,int show_nofix_here){
 gps_dial(b.x+37,b.y+62,26,heading,!fix);
 int rx=b.x+70,rw=b.w-78;
 /* Row 1: Bold degree heading + accented cardinal direction on one baseline. */
 if(heading<0){
  box_text(rx,b.y+36,rw,18,3,"--",C_MUTED);
 }else{
  char deg[12];gps_heading_degrees(deg,sizeof(deg),heading);
  const char*card=gps_cardinal_text(heading);
  int dw=width(deg,3),cw=width(card,0),gap=4,total=dw+gap+cw;
  int sx=rx+(rw-total)/2;
  int d_org=box_text_origin(b.y+36,18,3,"0");
  int c_org=d_org+3;
  label(sx,d_org,3,deg,C_WHITE,0);
  label(sx+dw+gap,c_org,0,card,C_CYAN,0);
 }
 /* Row 2: Colour-coded accuracy with crisp drawn +/- sign on digit bounds. */
 if(dm<0){
  box_text(rx,b.y+56,rw,16,1,"--",C_MUTED);
 }else{
  char acc[16];gps_accuracy_text(acc,sizeof(acc),dm);
  u16 col=gps_error_colour(dm);
  int pm=7,gap=3,aw=width(acc,1),total=pm+gap+aw,sx=rx+(rw-total)/2;
  int origin=box_text_origin(b.y+56,16,1,"0");
  gps_plus_minus(sx,origin+4,pm,col);
  label(sx+pm+gap,origin,1,acc,col,0);
 }
 /* Row 3: Rising signal bars + satellite count (RTL-mirrored, graceful fit for wide locales). */
 int rtl=ui_language_rtl();
 if(fix&&gps.sats>0){
  char sats[48]="";appendstr(sats,sizeof(sats),ui_tr("Satellites: "));append_int(sats,sizeof(sats),gps.sats);
  int sw=width(sats,0),bw=8,gap=4;
  int origin=box_text_origin(b.y+75,14,0,"0");
  if(bw+gap+sw<=rw){
   int sx=rx+(rw-(bw+gap+sw))/2;
   if(rtl){
    label(sx,origin,0,sats,C_MUTED,0);
    gps_signal_bars(sx+sw+gap,origin+2,fix,gps.sats,dm);
   }else{
    gps_signal_bars(sx,origin+2,fix,gps.sats,dm);
    label(sx+bw+gap,origin,0,sats,C_MUTED,0);
   }
  }else if(sw<=rw){
   label(rx+(rw-sw)/2,origin,0,sats,C_MUTED,0);
  }else{
   compact_fit(rx,b.y+77,rw,0,sats,C_MUTED);
  }
 }else if(show_nofix_here){
  box_text(rx,b.y+75,rw,14,0,ui_tr("No fix"),C_MUTED);
 }else{
  int bw=8,gap=4,dw=width("--",0),sx=rx+(rw-(bw+gap+dw))/2;
  int origin=box_text_origin(b.y+75,14,0,"0");
  gps_signal_bars(sx,origin+2,0,0,-1);
  label(sx+bw+gap,origin,0,"--",C_MUTED,0);
 }
}

static void gps_hud_draw_tray(struct HudBox b,int fix){
 int tx=b.x+6,ty=b.y+96,tw=b.w-12,th=46;
 u16 rim=theme_resolved?rgb(56,78,102):rgb(154,174,196);
 u16 fill_c=theme_resolved?rgb(10,18,29):rgb(220,231,243);
 round_rect(tx,ty,tw,th,5,rim,5);
 round_rect(tx+1,ty+1,tw-2,th-2,4,fill_c,9);
 int ix=tx+6,iw=tw-12;
 char alt[16];
 if(fix&&gps.alt_valid)gps_altitude_text(alt,sizeof(alt),gps.alt_dm);
 else copystr(alt,sizeof(alt),"--");
 gps_tray_row(ix,ty+2,iw,ui_tr("Altitude"),alt,fix&&gps.alt_valid?C_WHITE:C_MUTED);
 if(fix){
  char lat[24],lon[24];
  gps_coordinate_text(lat,sizeof(lat),gps.lat,'N','S');
  gps_coordinate_text(lon,sizeof(lon),gps.lon,'E','W');
  gps_tray_row(ix,ty+16,iw,ui_tr("Lat"),lat,C_WHITE);
  gps_tray_row(ix,ty+30,iw,ui_tr("Lon"),lon,C_WHITE);
 }else{
  box_text(ix,ty+17,iw,26,0,ui_tr("No fix"),C_MUTED);
 }
}

/* Render the unified expandable Clock + Live GPS HUD pill. */
static void gps_hud_draw(struct HudBox b,const struct HudCard*c,const struct HudTween*t,u32 now){
 (void)now;
 int full_a=(16*b.a+128)>>8;
 if(full_a<=0)return;
 int target_mode=t->target.value,prev_mode=t->previous.value;
 int has_temp=c->detail[0]!=0,has_clock=local_clock_minutes()>=0;
 int with_sec=target_mode!=GPS_HUD_OFF;
 const char*time_text=t->target.title[0]?t->target.title:c->title;
 struct HudClockLayout layout=hud_clock_header_layout(b,with_sec);
 /* Top header bar (27 px): keep exact clock/temp alignment and outer-edge anchor. */
 if(!with_sec){
  if(!time_text[0]){
   hud_ink_alpha=full_a;
   compact_center(b.x+b.w/2,b.y+5,b.w-12,1,c->detail,C_WHITE);
  }else{
   if(has_temp&&b.w>=99){
    int q=hud_ease((u32)(b.w-99),19);
    hud_ink_alpha=(full_a*q+128)>>8;
    compact_center(layout.temp_x,b.y+6,44,0,c->detail,C_MUTED);
    hud_rect_ink(layout.divider_x,b.y+7,1,12,C_BORDER);
   }
   hud_ink_alpha=full_a;
   compact_center(layout.time_x,b.y+4,58,3,time_text,C_WHITE);
  }
 }else if(!has_temp&&b.w<120){
  hud_ink_alpha=full_a;
  compact_center(b.x+b.w/2,b.y+4,b.w-10,3,time_text,has_clock?C_WHITE:C_MUTED);
 }else{
  if(b.w>=108){
   int q=hud_ease((u32)(b.w-108),22);
   hud_ink_alpha=(full_a*q+128)>>8;
   if(hud_ink_alpha){
    hud_rect_ink(layout.divider_x,b.y+7,1,12,C_BORDER);
    if(has_temp){
     compact_center(layout.temp_x,b.y+6,b.w-96,0,c->detail,C_MUTED);
    }else if(b.w>=126){
     int gq=hud_ease((u32)(b.w-126),24);
     hud_ink_alpha=(hud_ink_alpha*gq+128)>>8;
     if(hud_ink_alpha)compact_center(layout.temp_x,b.y+6,b.w-96,0,ui_tr("GPS"),C_CYAN);
    }
   }
  }
  hud_ink_alpha=full_a;
  compact_center(layout.time_x,b.y+4,78,3,time_text,has_clock?C_WHITE:C_MUTED);
 }
 /* Horizontal hairline separator below header when expanded past 32 px. */
 if(b.h<34)return;
 int sep_q=hud_ease((u32)(b.h-32),14);
 hud_ink_alpha=(full_a*sep_q+128)>>8;
 if(hud_ink_alpha)hud_rect_ink(b.x+8,b.y+27,b.w-16,1,C_BORDER);

 int fix=gps.fix,heading=gps_panel_heading(),dm=gps_accuracy_dm();
 /* Compact popup strip (52 px) — only shown when entering, in, or leaving COMPACT. */
 if((target_mode==GPS_HUD_COMPACT||prev_mode==GPS_HUD_COMPACT)&&b.h>=34&&b.h<76){
  int strip_q=b.h<50?hud_ease((u32)(b.h-34),16):(b.h<=62?256:256-hud_ease((u32)(b.h-62),14));
  hud_ink_alpha=(full_a*strip_q+128)>>8;
  if(hud_ink_alpha)gps_hud_draw_compact(b,full_a,fix,heading,dm);
 }
 /* Compass + primary telemetry band (148 px driving mode). */
 if((target_mode==GPS_HUD_FULL||prev_mode==GPS_HUD_FULL)&&b.h>=66){
  int mid_q=hud_ease((u32)(b.h-66),36);
  hud_ink_alpha=(full_a*mid_q+128)>>8;
  if(hud_ink_alpha)gps_hud_draw_medium(b,fix,heading,dm,0);
 }
 /* Recessed Altitude / Lat / Lon tray (148 px driving mode). */
 if((target_mode==GPS_HUD_FULL||prev_mode==GPS_HUD_FULL)&&b.h>=106){
  int tray_q=hud_ease((u32)(b.h-106),38);
  hud_ink_alpha=(full_a*tray_q+128)>>8;
  if(hud_ink_alpha)gps_hud_draw_tray(b,fix);
 }
 hud_ink_alpha=full_a;
}
