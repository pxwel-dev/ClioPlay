/* Adaptive native keyboard, glyph bounds and persisted names, using real keys. */
#define HOST_TEST 1
#define CLIO_UNICODE 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static void layout(int lang){pref_keyboard_language=lang;keyboard_open_fave_name(123,456);menu_open(P_NAME,0,hud_now_ms());keyboard_origin=KBD_NAME_FAVE;pk_reset();pk_prepare();assert(kbd_language==lang);}
static void type(int cp){
 for(int page=0;page<pk_pages();page++){pk_page=page;for(int i=0;i<pk_count();i++){char b[16];const char*p=pk_label(i,b);int c=utf8_code(&p);if(c==0x25cc)c=utf8_code(&p);if(c==cp){pk_focus=i;key_handle(K_ENTER);return;}}}
 assert(!"character missing from native layout");
}
static void word(const char*s){while(*s)type(utf8_code(&s));}
static void graph(void){
 int seen[105]={0},queue[105],a=0,b=0;queue[b++]=0;seen[0]=1;
 while(a<b){int at=queue[a++];const int codes[]={K_LEFT,K_RIGHT,K_UP,K_DOWN};for(int d=0;d<4;d++){pk_focus=at;pk_nav_x=-1;key_handle(codes[d]);assert(pk_valid(pk_focus)&&pk_focus<105);if(!seen[pk_focus]){seen[pk_focus]=1;queue[b++]=pk_focus;}}}
 for(int i=0;i<105;i++)assert(!pk_valid(i)||seen[i]);
 for(int row=0;row<pk_grid_rows();row++){
  pk_focus=pk_row_start(row);int initial=pk_focus;for(int i=0;i<pk_row_count(row);i++)key_handle(K_RIGHT);assert(pk_focus==initial);
  for(int i=0;i<pk_row_count(row);i++)key_handle(K_LEFT);assert(pk_focus==initial);
 }
 pk_focus=0;pk_nav_x=-1;int x=pk_key_centre(0);for(int i=0;i<pk_grid_rows()+1;i++)key_handle(K_DOWN);assert(pk_focus==pk_nearest(0,x));
}
static void exact_bounds(const char*text,int face){
 fill(0,0,W,H,0);hud_ink_alpha=16;cliou_draw(40,50,face,text,65535,0);int bounds[4];assert(cliou_bounds(text,face,bounds));
 int l=W,t=H,r=-1,b=-1;for(int y=0;y<H;y++)for(int x=0;x<W;x++)if(fb[y*W+x]){if(x<l)l=x;if(x>r)r=x;if(y<t)t=y;if(y>b)b=y;}
 assert(r>=l&&l==40+bounds[0]&&t==50+bounds[1]&&r-l+1==bounds[2]&&b-t+1==bounds[3]);
}
int main(int argc,char**argv){
 assert(argc==3&&load_font()&&cliou_load(argv[1]));hud_capture_clock=1;hud_capture_ms=10000;pref.language=0;pref.units=2;pref.cameras=2;gps.hour=-1;mapok=0;modal=MOD_NONE;nfaves=0;hud_reset_animations();
 layout(0);word("HOME");key_handle(K_SR);assert(streq(kbd_text,"HOME "));key_handle(K_ESC);assert(streq(kbd_text,"HOME"));
 pk_focus=PK_LANGUAGE;key_handle(K_ENTER);assert(sheet_page==P_LANG&&kbd_picker);key_handle(K_DOWN);key_handle(K_BACK);assert(sheet_page==P_NAME&&streq(kbd_text,"HOME"));
 unsigned pages=0;
 for(int lang=0;lang<UI_LANGUAGE_COUNT;lang++){
  layout(0);pk_focus=PK_LANGUAGE;key_handle(K_ENTER);for(int i=0;i<lang;i++)key_handle(K_DOWN);key_handle(K_ENTER);
  assert(!kbd_picker&&kbd_language==lang&&pref_keyboard_language==lang&&pref.language==0&&pref.units==2&&pref.cameras==2);
  save_prefs();pref_keyboard_language=-1;load_prefs();assert(pref_keyboard_language==lang);
  for(int mode=0;mode<(lang==14?3:2);mode++){
   int count=pk_pages();for(int page=0;page<count;page++){
    pk_page=page;graph();pages++;int seen=0;
    for(int row=0;row<pk_grid_rows();row++){assert(pk_row_count(row)>0);seen+=pk_row_count(row);}assert(seen==pk_count());
    for(int i=0;i<pk_count();i++){char text[16];pk_label(i,text);struct Ink*m=ink_bounds(text,18);assert(m->w<=pk_ink_w[lang]&&m->h<=pk_ink_h[lang]);}
    for(int night=0;night<2;night++){apply_palette(night);pk_focus=0;menu_contents(P_NAME,0,menu_box(P_NAME),16,hud_now_ms());}
   }
   pk_focus=PK_MODE;key_handle(K_ENTER);
  }
 }
 layout(1);word("ÉTÉ");key_handle(K_SR);word("À");assert(streq(kbd_text,"ÉTÉ À"));
 layout(7);word("ŁÓDŹ");assert(streq(kbd_text,"ŁÓDŹ"));layout(9);word("ΑΘΉΝΑ");assert(streq(kbd_text,"ΑΘΉΝΑ"));
 layout(10);word("КИЇВ");assert(streq(kbd_text,"КИЇВ"));layout(12);word("دبي");assert(streq(kbd_text,"دبي"));
 layout(16);word("दिल्ली");assert(streq(kbd_text,"दिल्ली"));layout(17);word("ירושלים");assert(streq(kbd_text,"ירושלים"));
 layout(14);word("とうきょう");pk_focus=PK_MODE;key_handle(K_ENTER);word("カナ");assert(streq(kbd_text,"とうきょうカナ"));
 layout(13);word("你好");assert(streq(kbd_text,"你好"));pk_focus=PK_MODE;key_handle(K_ENTER);assert(pk_stage==1);type(0x4e00);assert(pk_stage==2);pk_focus=0;key_handle(K_ENTER);assert(utf8_count(kbd_text)==3);
 /* Existing composition backend retains dictionary and all Hangul syllables. */
 layout(13);keyboard_type('N');keyboard_type('I');assert(streq(kbd_pinyin,"ni")&&kbd_candidates_n);kbd_focus=41;keyboard_press();assert(streq(kbd_text,"你"));
 layout(15);word("ㅎㅏㄴㄱㅡㄹ");assert(streq(kbd_text,"한글"));layout(15);word("ㄱㅗㅏ");assert(streq(kbd_text,"과"));key_handle(K_ESC);assert(streq(kbd_text,"고"));
 for(int l=0;l<19;l++)for(int v=0;v<21;v++)for(int t=0;t<28;t++){kbd_text[0]=0;keyboard_type(kbd_hangul_l[l]);keyboard_type(0x314f+v);if(t)keyboard_type(kbd_hangul_t[t]);assert(utf8_count(kbd_text)==1&&kbd_last()==0xac00+(l*21+v)*28+t);}
 layout(0);for(int i=0;i<KBD_TEXT_LIMIT+5;i++)type('A');assert(utf8_count(kbd_text)==KBD_TEXT_LIMIT);key_handle(K_SR);assert(utf8_count(kbd_text)==KBD_TEXT_LIMIT);pk_focus=PK_SAVE;key_handle(K_ENTER);assert(nfaves==1&&sheet_page==P_PLACES);
 copystr(faves[0].name,sizeof(faves[0].name),"東京駅");keyboard_open_fave_rename(0);menu_open(P_NAME,0,hud_now_ms());keyboard_origin=KBD_RENAME_FAVE;pk_reset();pk_prepare();key_handle(K_ESC);key_handle(K_SR);type('A');pk_focus=PK_SAVE;key_handle(K_ENTER);
 nfaves=read_places(FAVES_PATH,faves,FAVE_MAX);assert(nfaves==1&&streq(faves[0].name,"東京 A"));
 const char*texts[]={"ÉTÉ Hgj", "ΑΘΉΝΑ", "КИЇВ", "دبي", "東京駅", "한글", "दिल्ली", "ירושלים"};for(int i=0;i<8;i++)for(int face=0;face<4;face++)exact_bounds(texts[i],face);
 assert(!cliou_missing());printf("Keyboard: 18 adaptive layouts, %u pages/modes, wrap graph, pinned glyph limits, multilingual entry, both kana scripts, Hanzi browser, dictionary/Hangul, persistence and exact cached glyph bounds passed.\n",pages);
}
