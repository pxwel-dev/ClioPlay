/* Adaptive native keyboard presentation. Composition and persistence stay in
 * keyboard.h; the menu controller handles navigation and submission. */
enum {PK_LANGUAGE=100,PK_MODE,PK_PREV,PK_NEXT,PK_SAVE};
static int pk_focus,pk_page,pk_symbols,pk_lang=-1,pk_stage,pk_syllable,pk_kana,pk_nav_x=-1;
/* Upper bounds for the pinned 15 px CPF pack; verified by menu-unicode.c. */
static const u8 pk_ink_w[18]={15,15,15,15,15,15,15,15,15,13,15,15,19,15,15,14,22,13};
static const u8 pk_ink_h[18]={14,15,15,14,15,15,15,15,14,14,14,14,18,16,16,15,16,13};
static char pk_chars[1024],pk_kana_chars[2][512];
static const char pk_common[]="的一是不了在人有我他这中大来上国个到说们为子和你地出道也时年得就那要下以生会自着去之过家学对可她里后小么心多天而能好都然没日于起还发成事只作当想看文无开手十用主行方又如前所本见经头面公同三已老从动两长知民样现分将外但身些与高意进把法此实回理美点月明其种声全工己话儿者向情部正名定女";
static const char*const pk_native_names[]={"ABC","ABC","ABC","ABC","ABC","ABC","ABC","ABC","ABC","ΑΒΓ","АБВ","АБВ","أب","常用","かな","한글","अआ","אב"};
static int pk_has(const char*s,int cp){while(*s)if(utf8_code(&s)==cp)return 1;return 0;}
static void pk_add(int cp){if(pk_has(pk_chars,cp))return;char t[8];kbd_encode(cp,t);appendstr(pk_chars,sizeof(pk_chars),t);}
static void pk_reset(void){pk_focus=pk_page=pk_symbols=pk_stage=pk_kana=0;pk_lang=pk_nav_x=-1;}
static void pk_prepare(void){
 if(pk_lang==kbd_language)return;pk_lang=kbd_language;pk_focus=pk_page=pk_symbols=pk_stage=pk_kana=0;pk_nav_x=-1;pk_chars[0]=0;
 const char*first=kbd_language==1?"AZERTYUIOPQSDFGHJKLMWXCVBN":kbd_language==3?"QWERTZUIOPASDFGHJKLYXCVBNM":kbd_language<9?"QWERTYUIOPASDFGHJKLZXCVBNM":"";
 while(*first)pk_add(*first++);
 const char*p=kbd_layouts[kbd_language].characters;while(*p){int cp=utf8_code(&p);if(!((cp>='0'&&cp<='9')||(cp>=0x660&&cp<=0x669)||(cp>=0x966&&cp<=0x96f)))pk_add(cp);}
 if(kbd_language==14){pk_kana_chars[0][0]=pk_kana_chars[1][0]=0;p=pk_chars;while(*p){int cp=utf8_code(&p);char t[8];kbd_encode(cp,t);
  if(cp>=0x3040&&cp<=0x309f)appendstr(pk_kana_chars[0],sizeof(pk_kana_chars[0]),t);
  else appendstr(pk_kana_chars[1],sizeof(pk_kana_chars[1]),t);
 }appendstr(pk_kana_chars[0],sizeof(pk_kana_chars[0]),"ー・");}
}
static void pk_digits(char*out,unsigned cap){out[0]=0;if(kbd_language==12)appendstr(out,cap,"٠١٢٣٤٥٦٧٨٩");if(kbd_language==16)appendstr(out,cap,"०१२३४५६७८९");appendstr(out,cap,"0123456789-.,'()/+&:!?@");}
static int pk_total(void){pk_prepare();if(pk_symbols){char t[128];pk_digits(t,sizeof(t));return utf8_count(t);}if(kbd_language==13){if(pk_stage==0)return utf8_count(pk_common);if(pk_stage==1)return sizeof(kbd_pinyin_rows)/sizeof(kbd_pinyin_rows[0]);return utf8_count(kbd_pinyin_rows[pk_syllable].characters);}return utf8_count(kbd_language==14?pk_kana_chars[pk_kana]:pk_chars);}
/* Measured 15 px glyphs set the density ceiling: >=31 px key width, four
 * pixels of ink padding, four-pixel gutters, and no more than five rows. */
static int pk_max_cols(void){int min=pk_ink_w[kbd_language]+12;if(min<35)min=35;int cols=430/min;return cols>12?12:cols<1?1:cols;}
static int pk_max_rows(void){int min=pk_ink_h[kbd_language]+8;if(min<24)min=24;int rows=120/min;return rows>5?5:rows<1?1:rows;}
static int pk_page_size(void){
 int total=pk_total(),capacity=pk_max_cols()*pk_max_rows();
 if(kbd_language==14&&!pk_symbols)return capacity>=46?46:capacity; /* Base kana, then variants. */
 if(kbd_language<9&&!pk_symbols){if(total<=36)return 36;int base=kbd_language==1?20:19,cols=(total-base+1)/2;if(cols<10)cols=10;if(cols>pk_max_cols())cols=pk_max_cols();return base+2*cols;}
 if(total<=capacity)return total;
 int pages=(total+capacity-1)/capacity;return (total+pages-1)/pages;
}
static int pk_count(void){int n=pk_total()-pk_page*pk_page_size();return n>pk_page_size()?pk_page_size():n;}
static int pk_grid_rows(void){
 int n=pk_count(),max=pk_max_rows();
 if(kbd_language<9&&!pk_symbols){if(pk_page==0&&n<=26)return 3;return 4;}
 if(kbd_language==14&&!pk_symbols)return pk_page==0&&max>=5?5:4;
 if(pk_symbols){int rows=(n+pk_max_cols()-1)/pk_max_cols();return rows<1?1:rows;}
 if((kbd_language==9||kbd_language==10||kbd_language==11||kbd_language==17)&&n<=3*pk_max_cols())return 3;
 if(kbd_language==12||kbd_language==15||kbd_language==16)return n<=4*pk_max_cols()?(max<4?max:4):max;
 int rows=n>40?max:(n+9)/10;if(rows>max)rows=max;return rows<1?1:rows;
}
static int pk_columns(void){
 if(kbd_language<9&&!pk_symbols){if(pk_total()<=36)return 10;int cols=(pk_total()-(kbd_language==1?20:19)+1)/2;return cols<10?10:cols>12?12:cols;}
 int cols=(pk_count()+pk_grid_rows()-1)/pk_grid_rows();return cols<1?1:cols;
}
static int pk_pages(void){return (pk_total()+pk_page_size()-1)/pk_page_size();}
static const char*pk_label(int i,char out[16]){
 int n=pk_page*pk_page_size()+i;if(pk_symbols){char t[128];pk_digits(t,sizeof(t));kbd_character(t,n,out);}
 else if(kbd_language==13){if(pk_stage==1)kbd_character(kbd_pinyin_rows[n].characters,0,out);else kbd_character(pk_stage==0?pk_common:kbd_pinyin_rows[pk_syllable].characters,n,out);}
 else kbd_character(kbd_language==14?pk_kana_chars[pk_kana]:pk_chars,n,out);
 const char*p=out;int cp=*p?utf8_code(&p):0;
 if((cp>=0x64b&&cp<=0x670)||(cp>=0x93a&&cp<=0x94f)||cp==0x901||cp==0x902||cp==0x903){char mark[16];copystr(mark,sizeof(mark),out);copystr(out,16,"◌");appendstr(out,16,mark);}return out;
}
static int pk_valid(int f){return (f>=0&&f<pk_count())||f==PK_LANGUAGE||f==PK_MODE||f==PK_SAVE||((f==PK_PREV||f==PK_NEXT)&&pk_pages()>1);}
static int pk_row_start(int row){
 if(kbd_language<9&&!pk_symbols&&pk_page==0){
  int second=kbd_language==1?20:19;
  if(pk_total()>36){const int starts[]={0,10,second,second+pk_columns()};return starts[row];}
  const int en[]={0,10,19,26},fr[]={0,10,20,26};return kbd_language==1?fr[row]:en[row];
 }return row*pk_columns();
}
static int pk_row_count(int row){if(row<0||row>=pk_grid_rows())return 0;int n=pk_count()-pk_row_start(row),max=row==pk_grid_rows()-1?pk_columns():pk_row_start(row+1)-pk_row_start(row);return n<0?0:n<max?n:max;}
static int pk_key_row(int f){for(int row=0;row<pk_grid_rows();row++)if(f>=pk_row_start(row)&&f<pk_row_start(row)+pk_row_count(row))return row;return 0;}
static int pk_key_centre(int f){int row=pk_key_row(f),n=pk_row_count(row),col=f-pk_row_start(row),cw=430/pk_columns();if(kbd_language==12||kbd_language==17)col=n-1-col;return (456-n*cw+4)/2+col*cw+(cw-4)/2;}
static int pk_control_centre(int f){const int x[]={106,196,244,326,376};return x[f-PK_LANGUAGE];}
static int pk_nearest(int row,int x){
 int best=-1,dist=9999,start=row<0?PK_LANGUAGE:pk_row_start(row),end=row<0?PK_SAVE+1:start+pk_row_count(row);
 for(int i=start;i<end;i++)if(pk_valid(i)){int at=row<0?pk_control_centre(i):pk_key_centre(i),d=at-x;if(d<0)d=-d;if(d<dist){dist=d;best=i;}}
 return best;
}
static void pk_move(int code){
 int dx=code==K_RIGHT?1:code==K_LEFT?-1:0,dy=code==K_DOWN?1:code==K_UP?-1:0;
 if(!pk_valid(pk_focus)){pk_focus=pk_count()?0:PK_LANGUAGE;pk_nav_x=-1;}
 int control=pk_focus>=PK_LANGUAGE,current=control?-1:pk_key_row(pk_focus);
 if(dx){
  pk_nav_x=-1;
  if(control){
   int focus=pk_focus;
   do{focus=PK_LANGUAGE+wrap_focus(focus-PK_LANGUAGE,dx,PK_SAVE-PK_LANGUAGE+1);}while(!pk_valid(focus));
   pk_focus=focus;
  }else{
   if(kbd_language==12||kbd_language==17)dx=-dx;
   int start=pk_row_start(current);
   pk_focus=start+wrap_focus(pk_focus-start,dx,pk_row_count(current));
  }
  return;
 }
 if(dy){
  if(pk_nav_x<0)pk_nav_x=control?pk_control_centre(pk_focus):pk_key_centre(pk_focus);
  int rows=pk_grid_rows()+1,row=current+1;
  for(int i=1;i<rows;i++){
   row=wrap_focus(row,dy,rows);
   int best=pk_nearest(row-1,pk_nav_x);
   if(best>=0){pk_focus=best;return;}
  }
 }
}
static void pk_rotate(int direction){
 int items[65],n=0;for(int i=0;i<pk_count();i++)items[n++]=i;for(int i=100;i<=PK_SAVE;i++)if(pk_valid(i))items[n++]=i;
 pk_nav_x=-1;for(int i=0;i<n;i++)if(items[i]==pk_focus){pk_focus=items[(i+direction+n)%n];return;}
}
static void pk_press(void){
 pk_prepare();pk_nav_x=-1;
 if(pk_focus==PK_LANGUAGE||pk_focus==PK_SAVE){kbd_focus=pk_focus==PK_LANGUAGE?36:38;keyboard_press();return;}
 if(pk_focus==PK_PREV||pk_focus==PK_NEXT){int pages=pk_pages();pk_page=(pk_page+(pk_focus==PK_PREV?pages-1:1))%pages;return;}
 if(pk_focus==PK_MODE){pk_page=0;if(kbd_language==13){if(pk_symbols){pk_symbols=0;pk_stage=0;}else if(pk_stage==0)pk_stage=1;else if(pk_stage==2){pk_stage=1;pk_page=pk_syllable/pk_page_size();}else{pk_symbols=1;pk_stage=0;}}
  else if(kbd_language==14){if(pk_symbols){pk_symbols=pk_kana=0;}else if(!pk_kana)pk_kana=1;else pk_symbols=1;}
  else pk_symbols=!pk_symbols;return;}
 if(!pk_valid(pk_focus))return;
 if(kbd_language==13&&!pk_symbols&&pk_stage==1){pk_syllable=pk_page*pk_page_size()+pk_focus;pk_stage=2;pk_page=pk_focus=0;return;}
 char t[16];pk_label(pk_focus,t);const char*p=t;int cp=utf8_code(&p);if(cp==0x25cc)cp=utf8_code(&p);keyboard_type(cp);
}
