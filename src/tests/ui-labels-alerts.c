/* Regression: translated page names, shared BT badge and camera strips. */
#define HOST_TEST 1
#define main unused_device_entry
#include "../clio843/src/clio040.c"
#undef main
#include <assert.h>
#include <string.h>
int main(void){
 assert(load_font());pref.language=0;pref.units=2;apply_palette(0);
 assert(streq(bt_label(),"Bluetooth")&&streq(bt_source_label(),"BT"));
 for(int lang=1;lang<UI_LANGUAGE_COUNT;lang++){
  pref.language=lang;assert(streq(bt_label(),ui_tr("Phone connection")));
  assert(streq(bt_source_label(),"BT"));
 }
 // Playback and source form one centred row in every interface language.
 copystr(af.band,sizeof(af.band),"CD");car.tempok=0;gps.hour=-1;
 bt_control_mode=0;btui.title[0]=btui.artist[0]=btui.album[0]=0;
 for(int lang=0;lang<UI_LANGUAGE_COUNT;lang++)for(int state=0;state<3;state++){
  pref.language=lang;btui.connected=state>0;btui.playing=state==2;header();
  int top=40,bottom=-1,left=60,right=-1;
  for(int y=0;y<39;y++)for(int x=8;x<60;x++)if(fb[y*W+x]!=C_HEADER){
   if(y<top)top=y;if(y>bottom)bottom=y;if(x<left)left=x;if(x>right)right=x;
  }
  assert(bottom>=top&&bottom-top<12);assert(top+bottom>=37&&top+bottom<=41);
  assert(left>=8&&right<60);
 }
 // Preserve the established two-line BT/CTRL placement.
 pref.language=0;bt_control_mode=1;header();u16 control[52*39];
 for(int y=0;y<39;y++)memcpy(control+y*52,fb+y*W+8,52*2);
 fill(8,0,52,39,C_HEADER);compact_fit(8,5,52,0,"BT",C_CYAN);label(8,22,0,ui_tr("CTRL"),C_CYAN,52);
 for(int y=0;y<39;y++)assert(!memcmp(control+y*52,fb+y*W+8,52*2));
 bt_control_mode=0;
 pref.language=0;camera_visible=gps.fix=1;camera_distance=180;
 camera_current.kind=2;camera_current.speed=30;camera_current.flags=2;
 char text[64];camera_strip_text(text,sizeof(text));assert(streq(text,"RED LIGHT  200 yd"));
 gps.mph10=900;draw_camera_strip();u16 strip[240*29];
 for(int y=0;y<29;y++)memcpy(strip+y*240,fb+(243+y)*W+133,240*2);
 camera_current.speed=0;gps.mph10=0;draw_camera_strip();
 for(int y=0;y<29;y++)assert(!memcmp(strip+y*240,fb+(243+y)*W+133,240*2));
 camera_current.kind=1;camera_current.speed=30;camera_strip_text(text,sizeof(text));assert(streq(text,"SPEED CAMERA / 30  200 yd"));
 camera_current.kind=3;camera_strip_text(text,sizeof(text));assert(streq(text,"AVERAGE CHECK / 30  200 yd"));
 camera_average=1;camera_average_speed=48;camera_strip_text(text,sizeof(text));assert(starts(text,"AVG 30 mph / 30"));
 camera_average=0;camera_current.kind=2;pref.units=1;camera_strip_text(text,sizeof(text));assert(streq(text,"RED LIGHT  180 m"));
 puts("Shared centred BT/playback row in all languages; BT CTRL layout retained; translated page names preserved; red-light label and colour ignore speed metadata; fixed/average limits retained.");return 0;
}
