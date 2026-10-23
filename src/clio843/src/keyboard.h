/* Naming-only remote keyboard. Text, layout and interface language are separate.
 * Layout data is UTF-8; input never indexes bytes as characters. */
#include "keyboard-data/layouts.h"
#include "keyboard-data/pinyin.h"
static const char KBD_CHARS[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static int kbd_language=0,kbd_page=0,kbd_picker=0,kbd_pick=0,kbd_column=0;
static char kbd_pinyin[9];static const char*kbd_candidates="";static int kbd_candidates_n=0,kbd_candidate_page=0;
static int kbd_cmp(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return(unsigned char)*a-(unsigned char)*b;}
static void kbd_character(const char*s,int index,char out[16]){while(*s&&index-->0){unsigned n=utf8_span(s);s+=n?n:1;}unsigned n=utf8_span(s);if(!n){out[0]=0;return;}for(unsigned i=0;i<n;i++)out[i]=s[i];out[n]=0;}
static void kbd_encode(int cp,char out[8]){int n=0;if(cp<128)out[n++]=(char)cp;else if(cp<2048){out[n++]=(char)(0xc0|(cp>>6));out[n++]=(char)(0x80|(cp&63));}else if(cp<65536){out[n++]=(char)(0xe0|(cp>>12));out[n++]=(char)(0x80|((cp>>6)&63));out[n++]=(char)(0x80|(cp&63));}else{out[n++]=(char)(0xf0|(cp>>18));out[n++]=(char)(0x80|((cp>>12)&63));out[n++]=(char)(0x80|((cp>>6)&63));out[n++]=(char)(0x80|(cp&63));}out[n]=0;}
static int kbd_append(int cp){char b[8];kbd_encode(cp,b);unsigned n=slen(kbd_text),bytes=slen(b);if(utf8_count(kbd_text)>=KBD_TEXT_LIMIT||n+bytes>=sizeof(kbd_text))return 0;copystr(kbd_text+n,sizeof(kbd_text)-n,b);return 1;}
static int kbd_last(void){const char*p=kbd_text;int cp=0;while(*p)cp=utf8_code(&p);return cp;}
static void kbd_replace_last(int cp){char b[8];kbd_encode(cp,b);utf8_backspace(kbd_text);appendstr(kbd_text,sizeof(kbd_text),b);}
static void kbd_lookup(void){kbd_candidates="";kbd_candidates_n=0;kbd_candidate_page=0;if(!kbd_pinyin[0])return;int lo=0,hi=(int)(sizeof(kbd_pinyin_rows)/sizeof(kbd_pinyin_rows[0]));while(lo<hi){int mid=(lo+hi)/2,c=kbd_cmp(kbd_pinyin,kbd_pinyin_rows[mid].syllable);if(c>0)lo=mid+1;else if(c<0)hi=mid;else{kbd_candidates=kbd_pinyin_rows[mid].characters;kbd_candidates_n=(int)utf8_count(kbd_candidates);return;}}}
static void kbd_clear_composition(void){kbd_pinyin[0]=0;kbd_lookup();}
static int kbd_commit_pinyin(int candidate){if(!kbd_pinyin[0])return 1;if(candidate>=0&&candidate<kbd_candidates_n){char b[16];kbd_character(kbd_candidates,candidate,b);const char*p=b;if(!kbd_append(utf8_code(&p)))return 0;}else{unsigned count=utf8_count(kbd_text),n=slen(kbd_pinyin);if(count+n>KBD_TEXT_LIMIT||slen(kbd_text)+n>=sizeof(kbd_text))return 0;for(unsigned i=0;i<n;i++)kbd_append(kbd_pinyin[i]-'a'+'A');}kbd_clear_composition();return 1;}
/* Modern Hangul: compatibility-jamo keys, Unicode algorithmic syllables.
 * Compound vowels/finals and syllable-boundary resplitting remain reversible. */
static const int kbd_hangul_l[]={0x3131,0x3132,0x3134,0x3137,0x3138,0x3139,0x3141,0x3142,0x3143,0x3145,0x3146,0x3147,0x3148,0x3149,0x314a,0x314b,0x314c,0x314d,0x314e};
static const int kbd_hangul_t[]={0,0x3131,0x3132,0x3133,0x3134,0x3135,0x3136,0x3137,0x3139,0x313a,0x313b,0x313c,0x313d,0x313e,0x313f,0x3140,0x3141,0x3142,0x3144,0x3145,0x3146,0x3147,0x3148,0x314a,0x314b,0x314c,0x314d,0x314e};
static const unsigned char kbd_v_pairs[][3]={{8,0,9},{8,1,10},{8,20,11},{9,20,10},{13,4,14},{13,5,15},{13,20,16},{14,20,15},{18,20,19}};
static const unsigned char kbd_t_pairs[][3]={{1,1,2},{1,19,3},{4,22,5},{4,27,6},{8,1,9},{8,16,10},{8,17,11},{8,19,12},{8,25,13},{8,26,14},{8,27,15},{17,19,18},{19,19,20}};
static int kbd_index(const int*a,int n,int cp){for(int i=0;i<n;i++)if(a[i]==cp)return i;return-1;}
static int kbd_pair(const unsigned char a[][3],int n,int x,int y){for(int i=0;i<n;i++)if(a[i][0]==x&&a[i][1]==y)return a[i][2];return-1;}
static int kbd_first(const unsigned char a[][3],int n,int value){for(int i=0;i<n;i++)if(a[i][2]==value)return a[i][0];return-1;}
static int kbd_hangul(int cp){int last=kbd_last(),li=kbd_index(kbd_hangul_l,19,last),v=cp>=0x314f&&cp<=0x3163?cp-0x314f:-1,newl=kbd_index(kbd_hangul_l,19,cp),t=kbd_index(kbd_hangul_t,28,cp);
 if(li>=0&&v>=0){kbd_replace_last(0xac00+(li*21+v)*28);return 1;}
 if(li>=0&&newl==li){int doubled=li==0?1:li==3?4:li==7?8:li==9?10:li==12?13:-1;if(doubled>=0){kbd_replace_last(kbd_hangul_l[doubled]);return 1;}}
 if(last>=0xac00&&last<0xac00+11172){int s=last-0xac00,l=s/588,oldv=(s/28)%21,oldt=s%28;
  if(!oldt&&v>=0){int c=kbd_pair(kbd_v_pairs,9,oldv,v);if(c>=0){kbd_replace_last(0xac00+(l*21+c)*28);return 1;}}
  if(!oldt&&t>0){kbd_replace_last(last+t);return 1;}
  if(oldt&&t>0){int c=kbd_pair(kbd_t_pairs,13,oldt,t);if(c>=0){kbd_replace_last(last-oldt+c);return 1;}}
  if(oldt&&v>=0){int first=kbd_first(kbd_t_pairs,13,oldt),carry=oldt;
   /* Double finals are a single initial when followed by a vowel. */
   if(oldt==2||oldt==20)first=-1;
   if(first>=0){for(int i=0;i<13;i++)if(kbd_t_pairs[i][2]==oldt){carry=kbd_t_pairs[i][1];break;}}
   int nextl=kbd_index(kbd_hangul_l,19,kbd_hangul_t[carry]);if(nextl>=0){if(utf8_count(kbd_text)>=KBD_TEXT_LIMIT||slen(kbd_text)+3>=sizeof(kbd_text))return 0;kbd_replace_last(last-oldt+(first>=0?first:0));return kbd_append(0xac00+(nextl*21+v)*28);}
  }
 }
 if(last>=0x314f&&last<=0x3163&&v>=0){int c=kbd_pair(kbd_v_pairs,9,last-0x314f,v);if(c>=0){kbd_replace_last(0x314f+c);return 1;}}
 return kbd_append(cp);
}
static void keyboard_backspace(void){if(kbd_pinyin[0]){kbd_pinyin[slen(kbd_pinyin)-1]=0;kbd_lookup();if(kbd_focus>=40)kbd_focus=0;return;}if(kbd_language==15){int last=kbd_last();if(last>=0xac00&&last<0xac00+11172){int s=last-0xac00,t=s%28,v=(s/28)%21,l=s/588;if(t){int first=kbd_first(kbd_t_pairs,13,t);kbd_replace_last(last-t+(first>=0?first:0));}else{int first=kbd_first(kbd_v_pairs,9,v);kbd_replace_last(first>=0?0xac00+(l*21+first)*28:kbd_hangul_l[l]);}return;}}utf8_backspace(kbd_text);}
static void keyboard_space(void){if(!kbd_commit_pinyin(kbd_candidates_n?0:-1))return;if(kbd_text[0])kbd_append(' ');if(kbd_focus>=40)kbd_focus=0;}
static int keyboard_page_count(void){int n=kbd_layouts[kbd_language].count;return n<=36?1:(n+33)/34;}
static int keyboard_char_count(void){int n=kbd_layouts[kbd_language].count;if(n<=36)return n;n-=kbd_page*34;return n<34?n:34;}
static int keyboard_valid_focus(int i){if(i<0)return 0;if(i<34)return i<keyboard_char_count();if(i<36)return keyboard_page_count()>1||i<keyboard_char_count();if(i==36||i==38)return 1;if(i>=40&&i<48&&kbd_candidates_n){if(i==40||i==47)return kbd_candidates_n>6;return kbd_candidate_page*6+i-41<kbd_candidates_n;}return 0;}
static void keyboard_begin(void){kbd_language=pref_keyboard_language>=0&&pref_keyboard_language<UI_LANGUAGE_COUNT?pref_keyboard_language:pref.language;if(kbd_language<0||kbd_language>=UI_LANGUAGE_COUNT)kbd_language=0;kbd_page=kbd_picker=kbd_column=0;kbd_focus=0;kbd_clear_composition();}
static void keyboard_open_fave_name(s32 lat,s32 lon){kbd_action=KBD_NAME_FAVE;kbd_fave_idx=-1;kbd_place_lat=lat;kbd_place_lon=lon;kbd_text[0]=0;keyboard_begin();}
static void keyboard_type(int cp){if(kbd_language==13&&cp>='A'&&cp<='Z'){unsigned n=slen(kbd_pinyin);if(n<sizeof(kbd_pinyin)-1){kbd_pinyin[n]=(char)(cp-'A'+'a');kbd_pinyin[n+1]=0;kbd_lookup();}return;}if(kbd_pinyin[0]&&!kbd_commit_pinyin(kbd_candidates_n?0:-1))return;if(kbd_language==15)kbd_hangul(cp);else kbd_append(cp);}
static void keyboard_press(void){if(!keyboard_valid_focus(kbd_focus))return;if(kbd_focus>=40){if(kbd_focus==40||kbd_focus==47){int pages=(kbd_candidates_n+5)/6;kbd_candidate_page=(kbd_candidate_page+(kbd_focus==40?pages-1:1))%pages;kbd_focus=41;}else if(kbd_commit_pinyin(kbd_candidate_page*6+kbd_focus-41))kbd_focus=0;return;}
 if(kbd_focus<36){if(keyboard_page_count()>1&&kbd_focus>=34){int pages=keyboard_page_count();kbd_page=(kbd_page+(kbd_focus==34?pages-1:1))%pages;return;}char b[16];kbd_character(kbd_layouts[kbd_language].characters,kbd_page*(keyboard_page_count()>1?34:36)+kbd_focus,b);const char*p=b;if(*p)keyboard_type(utf8_code(&p));return;}
 if(kbd_focus==36){if(!kbd_commit_pinyin(-1))return;kbd_picker=1;kbd_pick=kbd_language;return;}
 if(kbd_focus==38){if(kbd_pinyin[0]){if(kbd_candidates_n)kbd_focus=41;return;}if(!kbd_text[0])return;if(kbd_action==KBD_NAME_FAVE){add_fave_place(kbd_text,kbd_place_lat,kbd_place_lon);screen=SCR_TRIP;}else if(kbd_fave_idx>=0){rename_fave(kbd_fave_idx,kbd_text);screen=SCR_TRIP;}return;}
}
static const char*key_label(int i,char b[16]){b[0]=0;if(i>=40){if(i==40)return"<";if(i==47)return">";kbd_character(kbd_candidates,kbd_candidate_page*6+i-41,b);return b;}if(i<36){if(keyboard_page_count()>1&&i>=34)return i==34?"<":">";kbd_character(kbd_layouts[kbd_language].characters,kbd_page*(keyboard_page_count()>1?34:36)+i,b);const char*p=b;int cp=*p?utf8_code(&p):0;if((cp>=0x64b&&cp<=0x670)||(cp>=0x93a&&cp<=0x94f)||cp==0x901||cp==0x902||cp==0x903){char mark[8];copystr(mark,sizeof(mark),b);copystr(b,16,"◌");appendstr(b,16,mark);}return b;}if(i==36)return kbd_layouts[kbd_language].name;if(i==38)return kbd_save_labels[pref.language];return"";}
static int keyboard_col(int i){return i>=40?i-40:i==36?(kbd_column>=4&&kbd_column<=5?kbd_column:4):i==38?(kbd_column>=6&&kbd_column<=7?kbd_column:6):i%8;}
static void keyboard_move(int dx,int dy){if(!keyboard_valid_focus(kbd_focus))kbd_focus=0;int row=kbd_focus>=40?-1:kbd_focus/8,col=keyboard_col(kbd_focus);if(dx){for(int c=col+dx;c>=0&&c<8;c+=dx){int i=row<0?40+c:row*8+c;if(row==4&&c==5)i=36;if(row==4&&c==7)i=38;if(i!=kbd_focus&&keyboard_valid_focus(i)){kbd_focus=i;kbd_column=c;return;}}return;}for(int r=row+dy;r>=-1&&r<=4;r+=dy){int best=-1,dist=99;for(int c=0;c<8;c++){int i=r<0?40+c:r*8+c;if(r<0&&(i==40||i==47))continue;if(r==4&&c==5)i=36;if(r==4&&c==7)i=38;if(!keyboard_valid_focus(i))continue;int d=c>col?c-col:col-c;if(d<dist){dist=d;best=i;}}if(best>=0){kbd_focus=best;kbd_column=col;return;}}}
static void keyboard_handle(int code){if(kbd_picker){if(code==K_BACK||code==K_ESC){kbd_picker=0;return;}if(code==K_UP&&kbd_pick>0)kbd_pick--;else if(code==K_DOWN&&kbd_pick+1<UI_LANGUAGE_COUNT)kbd_pick++;else if(code==K_LEFT){kbd_pick-=6;if(kbd_pick<0)kbd_pick=0;}else if(code==K_RIGHT){kbd_pick+=6;if(kbd_pick>=UI_LANGUAGE_COUNT)kbd_pick=UI_LANGUAGE_COUNT-1;}else if(code==K_ENTER){kbd_language=kbd_pick;pref_keyboard_language=kbd_pick;kbd_page=0;kbd_picker=0;kbd_clear_composition();prefs_mark_dirty();}return;}
 if(code==K_ESC){keyboard_backspace();return;}if(code==K_SR){keyboard_space();return;}if(code==K_LEFT)keyboard_move(-1,0);else if(code==K_RIGHT)keyboard_move(1,0);else if(code==K_UP)keyboard_move(0,-1);else if(code==K_DOWN)keyboard_move(0,1);else if(code==K_ENTER)keyboard_press();}
static void draw_keyboard(void){header();fill(0,40,W,232,C_BG);label(16,47,3,kbd_action==KBD_RENAME_FAVE?ui_tr("Rename favourite"):ui_tr("Name favourite"),C_WHITE,448);ui_surface(12,72,456,30,C_PANEL2);char display[193];copystr(display,sizeof(display),kbd_text);if(kbd_pinyin[0]){appendstr(display,sizeof(display)," [");char roman[9];copystr(roman,sizeof(roman),kbd_pinyin);for(int i=0;roman[i];i++)roman[i]=(char)(roman[i]-'a'+'A');appendstr(display,sizeof(display),roman);appendstr(display,sizeof(display),"]");}compact_fit(22,79,436,1,display[0]?display:ui_tr("Enter a name"),display[0]?C_WHITE:C_MUTED);
 for(int i=0;i<40;i++){if(!keyboard_valid_focus(i))continue;int x=12+(i%8)*57,y=150+(i/8)*21,w=i==36||i==38?109:52,f=kbd_focus==i;ui_choice(x,y,w,18,f);char b[16],name[96];const char*s=key_label(i,b);if(i==36&&keyboard_page_count()>1){copystr(name,sizeof(name),s);appendstr(name,sizeof(name)," ");append_int(name,sizeof(name),kbd_page+1);appendstr(name,sizeof(name),"/");append_int(name,sizeof(name),keyboard_page_count());s=name;}compact_center(x+w/2,y+1,w-8,0,s,ui_choice_text(f));}
 if(kbd_candidates_n)for(int i=40;i<48;i++){if(!keyboard_valid_focus(i))continue;int x=12+(i-40)*57,f=kbd_focus==i;ui_choice(x,119,52,22,f);char b[16];compact_center(x+26,122,44,1,key_label(i,b),ui_choice_text(f));}
 if(kbd_picker){ui_modal(76,61,328,199);compact_center(240,71,304,1,kbd_language_labels[pref.language],C_WHITE);int first=kbd_pick>5?kbd_pick-5:0;if(first+6>UI_LANGUAGE_COUNT)first=UI_LANGUAGE_COUNT-6;for(int row=0;row<6;row++){int i=first+row,y=96+row*25,f=i==kbd_pick;ui_choice(88,y,304,22,f);compact_center(240,y+3,288,1,kbd_layouts[i].name,ui_choice_text(f));}}
}
