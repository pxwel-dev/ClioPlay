/* Actual remote naming editor, Unicode shaping, layout/focus and persistence. */
#define HOST_TEST 1
#define CLIO_UNICODE 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static void layout(int lang){pref_keyboard_language=lang;keyboard_open_fave_name(123,456);screen=SCR_KEY;assert(kbd_language==lang);}
static void type(int cp){const char*p=kbd_layouts[kbd_language].characters;int i=0;while(*p){int c=utf8_code(&p);if(c==cp){kbd_page=keyboard_page_count()>1?i/34:0;kbd_focus=keyboard_page_count()>1?i%34:i;key_handle(K_ENTER);return;}i++;}assert(!"character missing from layout");}
static void word(const char*s){while(*s)type(utf8_code(&s));}
static void focus_graph(void){int seen[40]={0},queue[40],a=0,b=0;queue[b++]=0;seen[0]=1;while(a<b){int at=queue[a++];const int codes[]={K_LEFT,K_RIGHT,K_UP,K_DOWN};for(int d=0;d<4;d++){kbd_focus=at;kbd_column=at%8;key_handle(codes[d]);assert(keyboard_valid_focus(kbd_focus)&&kbd_focus<40);if(!seen[kbd_focus]){seen[kbd_focus]=1;queue[b++]=kbd_focus;}}}for(int i=0;i<40;i++)assert(!keyboard_valid_focus(i)||seen[i]);}
static void shot(const char*out,const char*name){char p[1024];snprintf(p,sizeof(p),"%s/%s.rgb565",out,name);FILE*f=fopen(p,"wb");assert(f&&fwrite(fb,1,sizeof(fb),f)==sizeof(fb));fclose(f);}
int main(int argc,char**argv){assert(argc==3&&load_font()&&cliou_load(argv[1]));pref.language=0;pref.units=2;pref.cameras=2;gps.hour=-1;mapok=0;copystr(af.band,sizeof(af.band),"AUX");nfaves=0;
 layout(0);word("HOME");kbd_focus=38;key_handle(K_SR);assert(!strcmp(kbd_text,"HOME ")&&screen==SCR_KEY);key_handle(K_ESC);assert(!strcmp(kbd_text,"HOME"));
 kbd_focus=35;key_handle(K_RIGHT);assert(kbd_focus==36);key_handle(K_RIGHT);assert(kbd_focus==38);key_handle(K_LEFT);assert(kbd_focus==36);kbd_focus=29;key_handle(K_DOWN);assert(kbd_focus==36);key_handle(K_UP);assert(kbd_focus==29);
 kbd_focus=31;key_handle(K_DOWN);assert(kbd_focus==38);key_handle(K_UP);assert(kbd_focus==31);kbd_focus=38;key_handle(K_RIGHT);assert(kbd_focus==38);key_handle(K_LEFT);assert(kbd_focus==36);
 kbd_focus=36;key_handle(K_ENTER);assert(kbd_picker);key_handle(K_DOWN);key_handle(K_BACK);assert(!kbd_picker&&screen==SCR_KEY&&!strcmp(kbd_text,"HOME"));
 key_handle(K_BACK);assert(screen==SCR_TRIP&&nfaves==0);
 unsigned pages=0,frames=0;
 for(int lang=0;lang<UI_LANGUAGE_COUNT;lang++){
  layout(0);copystr(kbd_text,sizeof(kbd_text),"HOME");kbd_focus=36;key_handle(K_ENTER);for(int i=0;i<lang;i++)key_handle(K_DOWN);key_handle(K_ENTER);
  assert(!kbd_picker&&kbd_language==lang&&pref_keyboard_language==lang&&!strcmp(kbd_text,"HOME"));assert(pref.language==0&&pref.units==2&&pref.cameras==2);
  save_prefs();pref_keyboard_language=-1;load_prefs();assert(pref_keyboard_language==lang);
  for(int page=0;page<keyboard_page_count();page++){
   kbd_page=page;kbd_clear_composition();kbd_focus=0;focus_graph();assert(!keyboard_valid_focus(37)&&!keyboard_valid_focus(39));pages++;
   for(int night=0;night<2;night++){apply_palette(night);kbd_focus=36;draw_keyboard();char name[64];snprintf(name,sizeof(name),"keyboard-%s-%d-%d",ui_language_codes[lang],page,night);shot(argv[2],name);frames++;}
  }
 }
 layout(1);word("ÉTÉ");assert(!strcmp(kbd_text,"ÉTÉ"));key_handle(K_SR);word("À");assert(!strcmp(kbd_text,"ÉTÉ À"));
 layout(7);word("ŁÓDŹ");assert(!strcmp(kbd_text,"ŁÓDŹ"));layout(9);word("ΑΘΉΝΑ");assert(!strcmp(kbd_text,"ΑΘΉΝΑ"));layout(10);word("КИЇВ");assert(!strcmp(kbd_text,"КИЇВ"));
 layout(12);word("دبي");assert(!strcmp(kbd_text,"دبي"));layout(16);word("दिल्ली");assert(!strcmp(kbd_text,"दिल्ली"));layout(17);word("ירושלים");assert(!strcmp(kbd_text,"ירושלים"));layout(14);word("とうきょう");assert(!strcmp(kbd_text,"とうきょう"));
 layout(13);word("NI");assert(!strcmp(kbd_pinyin,"ni")&&kbd_candidates_n>0);kbd_focus=0;key_handle(K_UP);assert(kbd_focus==41);key_handle(K_ENTER);assert(!strcmp(kbd_text,"你")&&!kbd_pinyin[0]);word("HAO");kbd_focus=41;key_handle(K_ENTER);assert(!strcmp(kbd_text,"你好"));key_handle(K_SR);assert(!strcmp(kbd_text,"你好 "));key_handle(K_ESC);key_handle(K_ESC);assert(!strcmp(kbd_text,"你"));
 word("SHI");assert(kbd_candidates_n>6);kbd_focus=47;key_handle(K_ENTER);assert(kbd_candidate_page==1&&kbd_focus==41);key_handle(K_ENTER);assert(utf8_count(kbd_text)==2&&!kbd_pinyin[0]);
 word("ZHONG");kbd_focus=38;key_handle(K_ENTER);assert(screen==SCR_KEY&&kbd_focus==41&&kbd_pinyin[0]);key_handle(K_ENTER);assert(!kbd_pinyin[0]);
 layout(13);word("XYZ");kbd_focus=36;key_handle(K_ENTER);assert(kbd_picker&&!strcmp(kbd_text,"XYZ"));key_handle(K_BACK);
 layout(13);word("ZHONG");apply_palette(1);draw_keyboard();shot(argv[2],"keyboard-pinyin");
 layout(15);word("ㅎㅏㄴㄱㅡㄹ");assert(!strcmp(kbd_text,"한글"));layout(15);word("ㄱㅏㄴㅏ");assert(!strcmp(kbd_text,"가나"));layout(15);word("ㄷㅏㄹㄱㅏ");assert(!strcmp(kbd_text,"달가"));layout(15);word("ㄱㅏㅂㅅㅏ");assert(!strcmp(kbd_text,"갑사"));layout(15);word("ㄱㅗㅏ");assert(!strcmp(kbd_text,"과"));key_handle(K_ESC);assert(!strcmp(kbd_text,"고"));key_handle(K_ESC);assert(!strcmp(kbd_text,"ㄱ"));key_handle(K_ESC);assert(!kbd_text[0]);
 for(int l=0;l<19;l++)for(int v=0;v<21;v++)for(int t=0;t<28;t++){kbd_text[0]=0;keyboard_type(kbd_hangul_l[l]);keyboard_type(0x314f+v);if(t)keyboard_type(kbd_hangul_t[t]);assert(utf8_count(kbd_text)==1&&kbd_last()==0xac00+(l*21+v)*28+t);}
 layout(15);word("ㅎㅏㄴㄱㅡㄹ");draw_keyboard();shot(argv[2],"keyboard-hangul");
 layout(0);for(int i=0;i<KBD_TEXT_LIMIT+5;i++)type('A');assert(utf8_count(kbd_text)==KBD_TEXT_LIMIT);key_handle(K_SR);assert(utf8_count(kbd_text)==KBD_TEXT_LIMIT);kbd_focus=38;key_handle(K_ENTER);assert(nfaves==1&&screen==SCR_TRIP);
 copystr(faves[0].name,sizeof(faves[0].name),"東京駅");pref_keyboard_language=0;keyboard_open_fave_rename(0);screen=SCR_KEY;key_handle(K_ESC);assert(!strcmp(kbd_text,"東京"));key_handle(K_SR);type('A');kbd_focus=38;key_handle(K_ENTER);assert(!strcmp(faves[0].name,"東京 A"));nfaves=read_places(FAVES_PATH,faves,FAVE_MAX);assert(nfaves==1&&!strcmp(faves[0].name,"東京 A"));
 layout(0);kbd_focus=36;key_handle(K_ENTER);draw_keyboard();shot(argv[2],"keyboard-picker");assert(!cliou_missing());
 printf("Keyboard: 18 selectable layouts, %u pages/%u native frames, uppercase/accented/native text, connected focus graph, soft-key editing, picker cancel, saved layout/names, 40-character bounds, Pinyin candidates and all 11,172 Hangul syllables passed.\n",pages,frames);return 0;
}
