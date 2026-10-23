#ifndef CLIO_GLOBAL_TEXT_H
#define CLIO_GLOBAL_TEXT_H
#ifdef __cplusplus
extern "C" {
#endif
int cliou_load(const char*path);
int cliou_ready(void);
int cliou_width(const char*text,int face);
void cliou_draw(int x,int y,int face,const char*text,unsigned short colour,int limit);
unsigned cliou_missing(void);
unsigned cliou_shapes(void);
int cliou_open(const char*path);
int cliou_size(int fd);
int cliou_read(int fd,unsigned off,void*data,unsigned len);
void*cliou_map(int fd,unsigned off,unsigned len);
void cliou_pixel(int x,int y,unsigned short colour,int alpha);
#ifdef __cplusplus
}
#endif
#endif
