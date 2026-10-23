/* Native text shaping, Unicode Bidi and bounded, pre-rasterized Noto glyph I/O.
 * Full script shaping runs here, including arbitrary new names from the phone.
 * No font outlines or glyph rasterization execute on the head unit. */
#include "global_text.h"
#include "vendor/harfbuzz/src/hb.h"
#include "vendor/harfbuzz/src/hb-ot.h"
#include <SheenBidi/SheenBidi.h>
#include <stdint.h>
#include <string.h>
struct NativeFont {uint32_t mo,ml,glyphs,io;hb_face_t*face;hb_font_t*font[3];};
static NativeFont fonts[32];static uint32_t font_n=0,font_data=0,font_bytes=0;static int font_fd=-1;
static uint32_t ticks=1,missing=0,shapes=0;
static uint16_t r16(const uint8_t*p){return uint16_t(p[0]|uint16_t(p[1])<<8);}
static uint32_t r32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
static int raster_face(int face){return face<=0?0:face==1?1:2;}
static int sizes[3]={11,13,15};
extern "C" int cliou_load(const char*path){if(font_n)return 1;int fd=cliou_open(path);if(fd<0)return 0;uint8_t h[32];int bytes=cliou_size(fd);if(bytes<32||!cliou_read(fd,0,h,32)||memcmp(h,"CPF1",4)||r16(h+4)!=1||r16(h+6)!=48)return 0;uint32_t n=r32(h+8),meta=r32(h+12),ix=r32(h+16),data=r32(h+20);if(!n||n>32||meta!=32+n*48||ix<meta||data<ix||data>uint32_t(bytes))return 0;
 for(uint32_t i=0;i<n;i++){uint8_t b[48];if(!cliou_read(fd,32+i*48,b,48))return 0;NativeFont&f=fonts[i];f.mo=r32(b+32);f.ml=r32(b+36);f.glyphs=r32(b+40);f.io=r32(b+44);if(!f.ml||f.mo<meta||f.mo>ix||f.ml>ix-f.mo||!f.glyphs||f.glyphs>65535||f.io<ix||f.io>data||f.glyphs*36>data-f.io)return 0;}
 font_fd=fd;font_n=n;font_data=data;font_bytes=uint32_t(bytes);return 1;}
extern "C" int cliou_ready(void){return font_n!=0;}
static hb_font_t*get_font(unsigned id,int face){if(id>=font_n)return nullptr;NativeFont&f=fonts[id];if(f.font[face])return f.font[face];if(!f.face){const char*p=static_cast<const char*>(cliou_map(font_fd,f.mo,f.ml));if(!p)return nullptr;hb_blob_t*b=hb_blob_create(p,f.ml,HB_MEMORY_MODE_READONLY,nullptr,nullptr);f.face=hb_face_create(b,0);hb_blob_destroy(b);if(!f.face)return nullptr;}hb_font_t*font=hb_font_create(f.face);if(!font)return nullptr;hb_ot_font_set_funcs(font);hb_font_set_scale(font,sizes[face]*64,sizes[face]*64);f.font[face]=font;return font;}
struct Positioned {uint16_t font,glyph;int16_t x,y;};
struct Shape {char text[193];uint32_t stamp;int face,width,count;Positioned glyph[192];};
static Shape cache[48];
static uint32_t next_cp(const char*s,unsigned end,unsigned&at){if(at>=end)return 0;unsigned c=(uint8_t)s[at++];if(c<128)return c;unsigned n=c>=0xf0?3:c>=0xe0?2:c>=0xc2?1:0;if(!n||at+n>end)return 0xfffd;uint32_t cp=c&((1u<<(6-n))-1);while(n--){unsigned t=(uint8_t)s[at++];if((t&192)!=128)return 0xfffd;cp=(cp<<6)|(t&63);}return cp;}
static unsigned choose_font(uint32_t cp,int rf,unsigned previous){hb_codepoint_t gid;if(previous<font_n){hb_font_t*f=get_font(previous,rf);if(f&&hb_font_get_nominal_glyph(f,cp,&gid))return previous;}for(unsigned i=0;i<font_n;i++){hb_font_t*f=get_font(i,rf);if(f&&hb_font_get_nominal_glyph(f,cp,&gid))return i;
}return 0;}
struct Chunk {unsigned start,end,font;hb_script_t script;};
static void append_chunk(Shape&s,const Chunk&c,int rtl,int&pen){hb_font_t*f=get_font(c.font,s.face);if(!f)return;hb_buffer_t*b=hb_buffer_create();if(!b)return;hb_buffer_add_utf8(b,s.text,-1,c.start,int(c.end-c.start));hb_buffer_set_direction(b,rtl?HB_DIRECTION_RTL:HB_DIRECTION_LTR);if(c.script!=HB_SCRIPT_COMMON&&c.script!=HB_SCRIPT_INHERITED&&c.script!=HB_SCRIPT_UNKNOWN)hb_buffer_set_script(b,c.script);hb_buffer_guess_segment_properties(b);hb_shape(f,b,nullptr,0);unsigned count=0;hb_glyph_info_t*g=hb_buffer_get_glyph_infos(b,&count);hb_glyph_position_t*p=hb_buffer_get_glyph_positions(b,nullptr);
 for(unsigned i=0;i<count&&s.count<192;i++){if(!g[i].codepoint){missing++;
 }int x=(pen+p[i].x_offset+32)/64,y=(p[i].y_offset+32)/64;s.glyph[s.count++]={uint16_t(c.font),uint16_t(g[i].codepoint),int16_t(x),int16_t(y)};pen+=p[i].x_advance;if(pen>65536)break;}hb_buffer_destroy(b);}
static Shape*shape(const char*text,int face){if(!font_n||!text)return nullptr;int rf=raster_face(face);unsigned len=unsigned(strlen(text));if(len>=193)return nullptr;Shape*out=nullptr;uint32_t age=0xffffffff;
 for(auto&s:cache){if(s.stamp&&s.face==rf&&!strcmp(s.text,text)){s.stamp=++ticks;return&s;}if(!s.stamp){out=&s;age=0;}else if(age&&s.stamp<age){out=&s;age=s.stamp;}}
 if(!out)return nullptr;Shape&s=*out;memcpy(s.text,text,len+1);s.face=rf;s.width=s.count=0;s.stamp=++ticks;shapes++;int pen=0;
 SBCodepointSequence seq={SBStringEncodingUTF8,s.text,len};SBAlgorithmRef algo=SBAlgorithmCreate(&seq);SBParagraphRef para=algo?SBAlgorithmCreateParagraph(algo,0,len,SBLevelDefaultLTR):nullptr;SBLineRef line=para?SBParagraphCreateLine(para,0,len):nullptr;
 if(line){const SBRun*runs=SBLineGetRunsPtr(line);unsigned nr=unsigned(SBLineGetRunCount(line));for(unsigned r=0;r<nr;r++){unsigned at=unsigned(runs[r].offset),end=at+unsigned(runs[r].length),nc=0,lastfont=0;hb_script_t last=HB_SCRIPT_UNKNOWN;Chunk chunks[96];
 while(at<end&&nc<96){unsigned start=at;uint32_t cp=next_cp(s.text,end,at);hb_script_t script=hb_unicode_script(hb_unicode_funcs_get_default(),cp);if(script==HB_SCRIPT_COMMON||script==HB_SCRIPT_INHERITED)script=last;unsigned fid=choose_font(cp,rf,lastfont);if(nc&&chunks[nc-1].font==fid&&(script==last||script==HB_SCRIPT_UNKNOWN))chunks[nc-1].end=at;else chunks[nc++]={start,at,fid,script};last=script;lastfont=fid;}
 int rtl=runs[r].level&1;for(unsigned i=0;i<nc;i++)append_chunk(s,chunks[rtl?nc-i-1:i],rtl,pen);
 }SBLineRelease(line);}if(para)SBParagraphRelease(para);if(algo)SBAlgorithmRelease(algo);s.width=(pen+32)/64;return out;}
struct Bitmap {uint32_t key,stamp;int left,top,w,h;uint8_t data[1024];};
static Bitmap bitmaps[256];
static Bitmap*bitmap(unsigned fid,int face,unsigned gid){if(fid>=font_n||gid>=fonts[fid].glyphs)return nullptr;uint32_t key=(fid<<18)|(unsigned(face)<<16)|gid;Bitmap*out=nullptr;uint32_t age=0xffffffff;
 for(auto&b:bitmaps){if(b.stamp&&b.key==key){b.stamp=++ticks;return&b;}if(!b.stamp){out=&b;age=0;}else if(age&&b.stamp<age){out=&b;age=b.stamp;}}
 uint8_t rec[12];if(!out||!cliou_read(font_fd,fonts[fid].io+(unsigned(face)*fonts[fid].glyphs+gid)*12,rec,12))return nullptr;unsigned w=rec[6],h=rec[7],n=(w*h+1)/2,off=r32(rec);if(n>1024||off>font_bytes-font_data||n>font_bytes-font_data-off)return nullptr;
 out->left=int8_t(rec[4]);out->top=int8_t(rec[5]);out->w=w;out->h=h;out->key=key;if(n&&!cliou_read(font_fd,font_data+off,out->data,n))return nullptr;out->stamp=++ticks;return out;}
extern "C" int cliou_width(const char*text,int face){Shape*s=shape(text,face);return s?s->width:0;}
extern "C" void cliou_draw(int x,int y,int face,const char*text,unsigned short colour,int limit){Shape*s=shape(text,face);if(!s)return;int base=y+sizes[s->face];for(int i=0;i<s->count;i++){const Positioned&g=s->glyph[i];Bitmap*b=bitmap(g.font,s->face,g.glyph);if(!b)continue;int xx=x+g.x+b->left,yy=base-g.y-b->top;for(int row=0;row<b->h;row++)for(int col=0;col<b->w;col++){if(limit>0&&xx+col>=x+limit)continue;int n=row*b->w+col,a=(b->data[n/2]>>((n&1)*4))&15;if(a)cliou_pixel(xx+col,yy+row,colour,a);}}}
extern "C" unsigned cliou_missing(void){return missing;}
extern "C" unsigned cliou_shapes(void){return shapes;}
