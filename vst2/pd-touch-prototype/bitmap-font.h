#ifndef MPCLEARN_PD_BITMAP_FONT_H
#define MPCLEARN_PD_BITMAP_FONT_H
#include <stdint.h>
struct PdFont;
PdFont* pd_font_open(const char* filename);
void pd_font_close(PdFont*);
void pd_font_draw(PdFont*,unsigned char* rgba,int width,int height,int x,int y,
                  const char* text,float size,uint32_t rgb,int maxWidth,int maxHeight,
                  int clipX=0,int clipY=0,int clipRight=2147483647,int clipBottom=2147483647);
#endif
