#define HOST_TEST 1
#define CLIO_UNICODE 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
static const char*names[]={"Église Saint-Étienne","Łódź — Śródmieście","Οδός Αθηνάς","улица Ленина","東京駅 八重洲口","北京市 朝阳路","서울역 세종대로","شارع الملك فيصل","רחוב יפו","महात्मा गांधी मार्ग","ถนนสุขุมวิท","ঢাকা বিমানবন্দর","அண்ணா சாலை","ရန်ကုန်လမ်း","කොළඹ","თბილისი","Երևան"};
static void shot(const char*out,const char*name){char path[1024];snprintf(path,sizeof(path),"%s/%s.rgb565",out,name);FILE*f=fopen(path,"wb");assert(f);assert(fwrite(fb,1,sizeof(fb),f)==sizeof(fb));fclose(f);}
static void native_labels(int start,int night,const char*out){apply_palette(night);fill(0,0,W,H,C_BG);label(12,8,3,night?"Native names · night":"Native names · day",C_WHITE,456);for(int i=0;i<9&&start+i<17;i++){int at=start+i;char number[8];snprintf(number,sizeof(number),"%02d",at+1);label(12,39+i*25,0,number,C_MUTED,24);assert(width(names[at],1)>20);compact_fit(46,36+i*25,420,1,names[at],C_WHITE);}char n[32];snprintf(n,sizeof(n),"scripts-%d-%s",start,night?"night":"day");shot(out,n);}
int main(int argc,char**argv){assert(argc==3);assert(load_font());assert(cliou_load(argv[1]));for(int n=0;n<17;n++){char b[161];copystr(b,sizeof(b),names[n]);assert(!strcmp(b,names[n]));for(const char*p=b;*p;){int cp=utf8_code(&p);assert(cp!=0xfffd);}assert(cliou_width(b,1)>0);}assert(!cliou_missing());unsigned shapes=cliou_shapes();for(int n=0;n<17;n++)assert(cliou_width(names[n],1)>0);assert(cliou_shapes()==shapes);
 for(int night=0;night<2;night++){native_labels(0,night,argv[2]);native_labels(9,night,argv[2]);}
 // Actual phone handoff file, then saved state: no transliteration or byte cuts.
 char packet[384];snprintf(packet,sizeof(packet),"SEQ=17\nACTION=N\nLAT=4041680\nLON=-370380\nNAME=%s\n",names[7]);FILE*f=fopen(BT_NAV_PATH,"wb");assert(f);fwrite(packet,1,strlen(packet),f);fclose(f);poll_phone_destination();assert(phone_dest.valid&&!strcmp(phone_dest.name,names[7]));trip.active=0;set_destination(phone_dest.name,phone_dest.lat,phone_dest.lon);assert(!strcmp(trip.target[0].name,names[7]));memzero(&trip,sizeof(trip));load_trip();assert(!strcmp(trip.target[0].name,names[7]));nrecents=read_places(RECENT_PATH,recents,RECENT_MAX);assert(nrecents==1&&!strcmp(recents[0].name,names[7]));
 // Old card records migrate with their coordinates/names intact.
 f=fopen(FAVES_PATH,"wb");assert(f);u32 magic=0x31504c43u,count=1;struct LegacyPlace old={12345,-54321,"Old favourite"};fwrite(&magic,4,1,f);fwrite(&count,4,1,f);fwrite(&old,sizeof(old),1,f);fclose(f);nfaves=read_places(FAVES_PATH,faves,FAVE_MAX);assert(nfaves==1&&faves[0].lat==12345&&!strcmp(faves[0].name,"Old favourite"));add_fave_place(names[9],4041680,-370380);nfaves=read_places(FAVES_PATH,faves,FAVE_MAX);assert(nfaves==2&&!strcmp(faves[1].name,names[9]));
 // UTF-8 clipping preserves complete code points even at 1..7-byte capacities.
 for(unsigned cap=1;cap<8;cap++){char b[8];copystr(b,cap,"東京駅");const char*p=b;while(*p)assert(utf8_code(&p)!=0xfffd);}
 // Uppercase Latin keys can rename a received native name without byte corruption or truncation.
 keyboard_open_fave_rename(1);assert(!strcmp(kbd_text,names[9]));kbd_focus=38;keyboard_press();assert(!strcmp(faves[1].name,names[9]));
 copystr(kbd_text,sizeof(kbd_text),"東京駅");screen=SCR_KEY;key_handle(K_ESC);assert(!strcmp(kbd_text,"東京"));key_handle(K_ESC);assert(!strcmp(kbd_text,"東"));kbd_focus=0;keyboard_press();assert(!strcmp(kbd_text,"東A"));
 // Mixed bidi, combining accents and native digits use the real shaping path.
 assert(cliou_width("شارع فيصل 123 / A4",1)>0);assert(cliou_width("רחוב יפו 42",1)>0);assert(cliou_width("E\xcc\x81glise",1)>0);
 // The same trip page, with multilingual remote keyboard and native names.
 for(int i=0;i<17;i++){recents[i].lat=4041680;recents[i].lon=-370380;copystr(recents[i].name,sizeof(recents[i].name),names[i]);}nrecents=17;screen=SCR_TRIP;trip.active=1;trip.count=2;trip.active_idx=0;copystr(trip.target[0].name,sizeof(trip.target[0].name),names[7]);copystr(trip.target[1].name,sizeof(trip.target[1].name),names[4]);trip_tab=trip_focus=trip_pane=0;pref.theme=THEME_NIGHT;gps.hour=12;gps.min=35;set_band("FM");copystr(af.station,sizeof(af.station),"Radio");draw();shot(argv[2],"native-trip");
 assert(!cliou_missing());extern unsigned cliou_heap_used(void);assert(cliou_heap_used()<2*1024*1024);printf("Native shaping heap in use: %u bytes of 2 MiB.\n",cliou_heap_used());printf("Native Unicode passed: %u shaped runs; no missing glyphs across 17 scripts/names; UTF-8 clipping, phone handoff, recents/trip persistence and old-card migration.\n",cliou_shapes());return 0;}
