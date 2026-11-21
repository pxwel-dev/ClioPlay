/* GPS information sheet: large local time, course dial and estimated
 * horizontal accuracy. Every mark is blended with sampled coverage so it
 * follows the sheet clip and fade exactly like the text around it. */

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

static void gps_dial(int cx,int cy,int r,int heading,int dim){
 int cx8=cx*8+4,cy8=cy*8+4,r8=r*8;
 gps_ring(cx8,cy8,r8,r8-14,dim?C_BORDER:C_MUTED,15);
 for(int d=0;d<360;d+=30){int ax,ay,bx,by,major=d%90==0;if(major)continue;
  gps_polar(cx8,cy8,r8-22,d,&ax,&ay);gps_polar(cx8,cy8,r8-50,d,&bx,&by);gps_segment(ax,ay,bx,by,5,C_BORDER,15);}
 static const char*cardinal[4]={"N","E","S","W"};
 for(int i=0;i<4;i++){int x,y;gps_polar(cx8,cy8,r8-84,i*90,&x,&y);
  u16 c=dim?C_BORDER:i==0?hud_status_colour(HUD_ERROR,C_WHITE):C_MUTED;box_text(x/8-8,y/8-8,16,16,i==0&&r>=34?3:0,cardinal[i],c);}
 if(heading<0)return;
 int tx,ty,lx,ly,rx,ry,bx,by;
 gps_polar(cx8,cy8,r8-26,heading,&tx,&ty);gps_polar(cx8,cy8,(r8-26)*6/10,heading+180,&bx,&by);
 gps_polar(cx8,cy8,44,heading+90,&lx,&ly);gps_polar(cx8,cy8,44,heading-90,&rx,&ry);
 gps_triangle(bx,by,lx,ly,rx,ry,C_MUTED,15);
 gps_triangle(tx,ty,lx,ly,rx,ry,C_CYAN,15);
 gps_ring(cx8,cy8,24,0,C_WHITE,15);
}
/* Accuracy colour: green up to 5 m, amber up to 15 m, red beyond. */
static u16 gps_error_colour(int dm){return hud_status_colour(dm<=50?HUD_SUCCESS:dm<=150?HUD_WARNING:HUD_ERROR,C_CYAN);}
/* A plus-minus sign built from bars: the glyph is absent from the large faces. */
static void gps_plus_minus(int x,int y,int size,u16 c){
 int t=size/6;if(t<2)t=2;int mid=x+size/2;
 round_rect(x,y+size/2-t,size,t,0,c,15);round_rect(mid-t/2,y,t,size-t*2,0,c,15);round_rect(x,y+size-t,size,t,0,c,15);
}
/* Big value (24 px, or 18 px when it would not fit) with an optional detail line. */
static void gps_value(int x,int y,int w,const char*value,u16 value_colour,const char*detail,int detail_face,int plus_minus){
 int face=5,sign=plus_minus?14:0,gap=plus_minus?5:0;
 if(sign+gap+width(value,face)>w){face=4;if(plus_minus){sign=11;gap=4;}}
 int text_w=width(value,face)+2,left=x+(w-(sign+gap+text_w))/2;
 if(plus_minus)gps_plus_minus(left,y+(28-sign)/2,sign,value_colour);
 box_text(left+sign+gap,y,text_w,28,face,value,value_colour);
 if(detail&&*detail)box_text(x,y+30,w,15,detail_face,detail,C_MUTED);
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
/* Unsigned decimal degrees with a hemisphere letter, e.g. "1.65638° W". */
static void gps_coordinate_text(char*b,unsigned cap,s32 e5,char positive,char negative){
 coordinate_format(b,cap,e5<0?-e5:e5);appendstr(b,cap,"° ");
 char h[2]={e5<0?negative:positive,0};appendstr(b,cap,h);
}
/* Altitude above mean sea level, rounded to whole metres or feet. */
static void gps_altitude_text(char*b,unsigned cap,int dm){
 int imperial=display_imperial();long long v=imperial?((long long)dm*328):((long long)dm*100);
 v=v<0?-((-v+500)/1000):(v+500)/1000;b[0]=0;append_int(b,cap,(int)v);appendstr(b,cap,imperial?" ft":" m");
}
/* "Lat 47.49811° N    Lon 19.04033° E": muted labels, white values, one baseline. */
static void gps_position(struct HudBox b){
 const char*name[2]={ui_tr("Lat"),ui_tr("Lon")};char value[2][24];
 gps_coordinate_text(value[0],sizeof(value[0]),gps.lat,'N','S');
 gps_coordinate_text(value[1],sizeof(value[1]),gps.lon,'E','W');
 int count=2,gap=6,separator=30,total=-separator;
 for(int i=0;i<count;i++)total+=width(name[i],1)+gap+width(value[i],1)+separator;
 int x=b.x+(b.w-total)/2,origin=box_text_origin(b.y+b.h-24,18,1,"0");
 for(int i=0;i<count;i++){
  label(x,origin,1,name[i],C_MUTED,0);x+=width(name[i],1)+gap;
  label(x,origin,1,value[i],C_WHITE,0);x+=width(value[i],1)+separator;
 }
}
static int gps_panel_heading(void){
 if(!gps.fix||gps.mph10<DISPLAY_COURSE_UPDATE_MPH10)return -1;
 int c=shown_course();return c>=0&&c<360?c:-1;
}
/* Time on top; a compass on the left, then Heading | Accuracy | Altitude
 * columns (centred label, large value, detail); position or "No fix" below. */
static void gps_content(struct HudBox b,int opacity,u32 now){
 hud_ink_alpha=opacity;(void)now;
 box_text(b.x+14,b.y+4,40,22,title_face,ui_tr("GPS"),C_WHITE);
 char time[16];gps_time_text(time,sizeof(time));
 box_text(b.x+60,b.y+6,b.w-120,34,6,time,local_clock_minutes()<0?C_MUTED:C_WHITE);
 int fix=gps.fix,heading=gps_panel_heading(),dm=gps_accuracy_dm();
 int top=b.y+46,bottom=b.y+b.h-26,r=(bottom-top-6)/2;if(r>38)r=38;if(r<14)r=14;int cy=(top+bottom)/2;
 int dial_x=b.x+18+r;gps_dial(dial_x,cy,r,heading,!fix);
 int left=dial_x+r+12,col=(b.x+b.w-12-left)/3,ly=cy-31,vy=ly+16;
 static const char*titles[3]={"Heading","Accuracy","Altitude"};
 for(int i=0;i<3;i++){
  int x=left+i*col;box_text(x,ly,col,13,0,ui_tr(titles[i]),C_MUTED);
  if(i==0){
   if(heading<0)gps_value(x,vy,col,"--",C_MUTED,"",0,0);
   else{
    char deg[8]="";if(heading<100)appendstr(deg,sizeof(deg),"0");if(heading<10)appendstr(deg,sizeof(deg),"0");append_int(deg,sizeof(deg),heading);appendstr(deg,sizeof(deg),"°");
    static const char*n[8]={"N","NE","E","SE","S","SW","W","NW"};
    gps_value(x,vy,col,deg,C_WHITE,n[((heading+22)/45)&7],3,0);
   }
  }else if(i==1){
   char sats[48]="";if(fix&&gps.sats>0){appendstr(sats,sizeof(sats),ui_tr("Satellites: "));append_int(sats,sizeof(sats),gps.sats);}
   if(dm<0)gps_value(x,vy,col,"--",C_MUTED,sats,0,0);
   else{char acc[16];gps_accuracy_text(acc,sizeof(acc),dm);gps_value(x,vy,col,acc,gps_error_colour(dm),sats,0,1);}
  }else{
   if(!fix||!gps.alt_valid)gps_value(x,vy,col,"--",C_MUTED,"",0,0);
   else{char alt[16];gps_altitude_text(alt,sizeof(alt),gps.alt_dm);gps_value(x,vy,col,alt,C_WHITE,"",0,0);}
  }
 }
 if(fix)gps_position(b);
 else box_text(b.x+14,b.y+b.h-24,b.w-28,18,1,ui_tr("No fix"),C_MUTED);
}
