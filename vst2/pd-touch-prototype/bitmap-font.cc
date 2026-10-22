/* CPU glyph rasterization into the existing public image surface. The pinned
 * DPF stb_truetype source and DejaVu font are copied into each build closure. */
#include "bitmap-font.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
struct PdGlyph {int codepoint,width,height,x,y,advance;unsigned char alpha[32*32];};
struct PdFont {unsigned char* data;stbtt_fontinfo info;PdGlyph glyph[256];unsigned count;};
PdFont* pd_font_open(const char* filename){FILE* f=fopen(filename,"rb");if(!f)return nullptr;if(fseek(f,0,SEEK_END)){fclose(f);return nullptr;}long size=ftell(f);
    if(size<1||size>2097152){fclose(f);return nullptr;}rewind(f);auto* font=(PdFont*)calloc(1,sizeof(PdFont));if(!font){fclose(f);return nullptr;}
    font->data=(unsigned char*)malloc(size);bool ok=font->data&&fread(font->data,1,size,f)==(size_t)size;fclose(f);
    if(!ok||!stbtt_InitFont(&font->info,font->data,0)){pd_font_close(font);return nullptr;}return font;}
void pd_font_close(PdFont* font){if(font){free(font->data);free(font);}}
static PdGlyph* glyph(PdFont* f,int codepoint){for(unsigned i=0;i<f->count;++i)if(f->glyph[i].codepoint==codepoint)return f->glyph+i;
    unsigned slot=f->count<256?f->count++:255;auto* g=f->glyph+slot;memset(g,0,sizeof(*g));g->codepoint=codepoint;
    float scale=stbtt_ScaleForPixelHeight(&f->info,24);int x1,y1,advance,bearing;stbtt_GetCodepointBitmapBox(&f->info,codepoint,scale,scale,&g->x,&g->y,&x1,&y1);
    g->width=x1-g->x;g->height=y1-g->y;if(g->width>32)g->width=32;if(g->height>32)g->height=32;
    stbtt_GetCodepointHMetrics(&f->info,codepoint,&advance,&bearing);g->advance=(int)ceilf(advance*scale);
    if(g->width>0&&g->height>0)stbtt_MakeCodepointBitmap(&f->info,g->alpha,g->width,g->height,32,scale,scale,codepoint);return g;}
static int decode(const unsigned char*& s){unsigned a=*s++;if(a<128)return a;
    unsigned count=a>=240?3:a>=224?2:a>=192?1:0;int code=a&((1u<<(6-count))-1);if(!count)return '?';
    for(unsigned i=0;i<count;++i){if((*s&0xc0)!=0x80)return '?';code=(code<<6)|(*s++&63);}return code;}
void pd_font_draw(PdFont* font,unsigned char* rgba,int width,int height,int x,int y,const char* text,float size,uint32_t color,int maxWidth,int maxHeight,int clipX,int clipY,int clipRight,int clipBottom){
    if(!font||!text||size<5||maxWidth<1||maxHeight<1)return;float scale=size/24;int cx=x,cy=y;int line=(int)ceilf(size*1.25f);
    const unsigned char* s=(const unsigned char*)text;
    while(*s&&cy+line<=y+maxHeight){int cp=decode(s);if(cp=='\n'){cx=x;cy+=line;continue;}PdGlyph* g=glyph(font,cp);int advance=(int)ceilf(g->advance*scale);
        if(cp==' '&&cx==x)continue;
        if(cp==' '){const unsigned char* look=s;int word=0;while(*look&&*look!=' '&&*look!='\n'){PdGlyph* next=glyph(font,decode(look));word+=(int)ceilf(next->advance*scale);}
            if(cx+advance+word>x+maxWidth&&word<=maxWidth){cx=x;cy+=line;continue;}}
        if(cx+advance>x+maxWidth){cx=x;cy+=line;if(cy+line>y+maxHeight)break;}
        int gw=(int)ceilf(g->width*scale),gh=(int)ceilf(g->height*scale),gx=cx+(int)(g->x*scale),gy=cy+(int)(size+g->y*scale);
        for(int j=0;j<gh;++j){int yy=gy+j;if(yy<0||yy>=height||yy>=y+maxHeight||yy<clipY||yy>=clipBottom)continue;for(int i=0;i<gw;++i){int xx=gx+i;if(xx<0||xx>=width||xx>=x+maxWidth||xx<clipX||xx>=clipRight)continue;
            int sy=(int)(j/scale),sx=(int)(i/scale);if(sy>=g->height||sx>=g->width)continue;unsigned a=g->alpha[sy*32+sx];auto* p=rgba+4*(yy*width+xx);
            p[0]=(p[0]*(255-a)+((color>>16)&255)*a)/255;p[1]=(p[1]*(255-a)+((color>>8)&255)*a)/255;p[2]=(p[2]*(255-a)+(color&255)*a)/255;p[3]=255;}}
        cx+=advance;}
}
