#include <stdio.h>
#include <assert.h>
#include <stdint.h>
static unsigned udiv32(unsigned n,unsigned d){return d?n/d:0;}
#include "../clio843/src/audio_chime.h"
int main(int argc,char**argv){
 for(unsigned rate=11025;rate<=44100;rate*=2){struct AudioChime c={0};chime_start(&c,rate);unsigned count=0;int previous=0,maxstep=0,peak=0;
 FILE*f=argc>1&&rate==44100?fopen(argv[1],"wb"):0;
 while(chime_active(&c)){int gain,v=chime_sample(&c,&gain);assert(gain>=80&&gain<=256);int d=v-previous;if(d<0)d=-d;if(d>maxstep)maxstep=d;previous=v;if(v>peak)peak=v;assert(chime_mix(32767,gain,v)<=32767);assert(chime_mix(-32768,gain,v)>=-32768);if(f){int16_t s=v;fwrite(&s,2,1,f);}count++;}
 if(f)fclose(f);assert(count==rate/2);assert(previous==0);assert(peak>3000&&peak<4097);assert(maxstep<3000);int gain;assert(chime_sample(&c,&gain)==0&&gain==256);
 }puts("PCM camera cue: duration, envelopes, bounded ducking, clipping and 11/22/44kHz passed.");return 0;}
