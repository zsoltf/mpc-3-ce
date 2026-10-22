/* Presentation only: all committed geometry and topology are copied from Pd. */
#include "pd-canvas.h"
#include "pd-touch.h"
#include "bitmap-font.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
struct PdCanvas {
    PdTouch* pd;PdTouchSnapshot state;unsigned char* pixels;
    uint64_t parameterBinding,parameterIdentity;int parameterSelected;float parameterMinimum,parameterMaximum;
    int width,height,selected,contact,port,x,y,startX,startY,previewX,previewY,clipX,clipY,clipRight,clipBottom;
    uint32_t revision;int dirty;char status[512];PdFont* font;int real,keyboard,keypage,interact,borrowed;char input[512];float zoom,panX,panY,startPanX,startPanY,startValue,previewValue;
    float arrayPreview[PD_TOUCH_SAMPLES];int arrayFirst,arrayLast,arrayPrevious,createKind;
    int panMode,overview,continuous,externalText,wireArmed,wireNode,wirePort;PdCanvasViewState views;
};
namespace {
const int cardW=230,cardH=150;
int clamp(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
char keyboardKey(int page,int row,int column){const char* letters[]={"1234567890","qwertyuiop","asdfghjkl~","zxcvbnm.*-","_/+<>$%!?="};const char* symbols[]={"1234567890","[]{}(),;:\"","\\|@#^&`~!?","_/+-*%=<>.","$012345678"};char key=(page==2?symbols:letters)[row][column];return page==1&&key>='a'&&key<='z'?key-'a'+'A':key;}
void pixel(PdCanvas* c,int x,int y,uint32_t rgb){if(x<0||y<0||x>=c->width||y>=c->height||x<c->clipX||y<c->clipY||x>=c->clipRight||y>=c->clipBottom)return;auto* p=c->pixels+4*(y*c->width+x);p[0]=rgb>>16;p[1]=rgb>>8;p[2]=rgb;p[3]=255;}
void rect(PdCanvas* c,int x,int y,int w,int h,uint32_t color){int bottom=clamp(y+h,0,c->height),right=clamp(x+w,0,c->width);for(int j=clamp(y,0,c->height);j<bottom;++j)for(int i=clamp(x,0,c->width);i<right;++i)pixel(c,i,j,color);}
void circle(PdCanvas* c,int x,int y,int radius,uint32_t color){for(int j=-radius;j<=radius;++j)for(int i=-radius;i<=radius;++i)if(i*i+j*j<=radius*radius)pixel(c,x+i,y+j,color);}
void ring(PdCanvas* c,int x,int y,int radius,uint32_t color){for(int j=-radius;j<=radius;++j)for(int i=-radius;i<=radius;++i){int d=i*i+j*j;if(d<=radius*radius&&d>=(radius-2)*(radius-2))pixel(c,x+i,y+j,color);}}
void line(PdCanvas* c,int x,int y,int xx,int yy,uint32_t color){float a=0,b=1,dx=xx-x,dy=yy-y;float p[]={-dx,dx,-dy,dy},q[]={(float)x,c->width-1.f-x,(float)y,c->height-1.f-y};
    for(int k=0;k<4;++k){if(p[k]==0){if(q[k]<0)return;}else{float r=q[k]/p[k];if(p[k]<0){if(r>b)return;if(r>a)a=r;}else{if(r<a)return;if(r<b)b=r;}}}
    xx=x+(int)(b*dx);yy=y+(int)(b*dy);x+=(int)(a*dx);y+=(int)(a*dy);int n=abs(xx-x)>abs(yy-y)?abs(xx-x):abs(yy-y);if(!n)n=1;for(int i=0;i<=n;++i)circle(c,x+(xx-x)*i/n,y+(yy-y)*i/n,c->real?1:3,color);}
const char* glyph(char x){switch(x){
case 'o':return "01110100011000110001100011000101110";
case 's':return "01111100001000001110000010000111110";
case 'c':return "01111100001000010000100001000001111";
case '~':return "00000000000100110110000000000000000";
case '*':return "00000101010111011111011101010100000";
case '.':return "00000000000000000000000000110001100";
case '-':return "00000000000000011111000000000000000";
case '0':return "01110100011001110101110011000101110";
case '1':return "00100011000010000100001000010001110";
case '2':return "01110100010000100010001000100011111";
case '3':return "11110000010000101110000010000111110";
case '4':return "00010001100101010010111110001000010";
case '5':return "11111100001000011110000010000111110";
case '6':return "01110100001000011110100011000101110";
case '7':return "11111000010001000100010000100001000";
case '8':return "01110100011000101110100011000101110";
case '9':return "01110100011000101111000010000101110";
default:return "00000000000000000000000000000000000";}}
void text(PdCanvas* c,int x,int y,const char* value){for(;*value;++value,x+=18){const char* g=glyph(*value);for(int j=0;j<7;++j)for(int i=0;i<5;++i)if(g[j*5+i]=='1')rect(c,x+i*3,y+j*3,3,3,0xf1f4f8);}}
int worldWidth(PdCanvas* c,int n){auto& node=c->state.node[n];if(node.pixel_width>0)return node.pixel_width;int chars=node.width>0?node.width:node.kind==0?60:(int)strlen(node.text);return clamp(chars*7+10,node.kind==0?24:30,node.kind==0?900:400);}
int worldHeight(PdCanvas* c,int n){auto& node=c->state.node[n];if(node.pixel_height>0)return node.pixel_height;
    if(node.kind!=0)return 24;int cols=(worldWidth(c,n)-10)/7,rows=1,col=0;const char* p=node.text;
    while(*p){if(*p=='\n'){++rows;col=0;++p;continue;}if(*p==' '){if(col)++col;++p;continue;}int word=0;while(p[word]&&p[word]!=' '&&p[word]!='\n')++word;if(col&&col+word>cols){++rows;col=0;}col+=word;while(col>cols){++rows;col-=cols;}p+=word;}return rows*16+8;}
int nodeWidth(PdCanvas* c,int n){return c->real?(int)(worldWidth(c,n)*c->zoom):cardW;}
int nodeHeight(PdCanvas* c,int n){return c->real?(int)(worldHeight(c,n)*c->zoom):cardH;}
void previewOffset(PdCanvas* c,int n,int& dx,int& dy){dx=dy=0;if(c->contact!=1)return;for(int depth=0;n>=0&&depth<17;++depth,n=c->state.node[n].owner_parent)if(n==c->selected){dx=c->previewX-c->state.node[n].x;dy=c->previewY-c->state.node[n].y;return;}}
void position(PdCanvas* c,int n,int& x,int& y){int dx,dy;previewOffset(c,n,dx,dy);x=c->state.node[n].x+dx;y=c->state.node[n].y+dy;if(c->real){x=(int)((x-c->panX)*c->zoom);y=(int)((y-c->panY)*c->zoom);}}
void portPosition(PdCanvas* c,int n,int p,bool output,int& x,int& y){position(c,n,x,y);if(c->real){int count=output?c->state.node[n].outputs:c->state.node[n].inputs;x+=count>1?8+p*(nodeWidth(c,n)-16)/(count-1):10;y+=output?nodeHeight(c,n):0;}else{x+=32+p*56;y+=output?cardH-12:12;}}
void working(PdCanvas* c){c->zoom=2;c->panX=c->panY=-10;c->overview=0;
    if(c->state.count){float x=c->state.node[0].x,y=c->state.node[0].y;
        for(int i=1;i<c->state.count;++i){x=fminf(x,c->state.node[i].x);y=fminf(y,c->state.node[i].y);}
        c->panX=x-12;c->panY=y-12;}}
void fit(PdCanvas* c){if(!c->real)return;float left=0,top=0,right=300,bottom=200;
    if(c->state.count){left=right=c->state.node[0].x;top=bottom=c->state.node[0].y;}
    for(int i=0;i<c->state.count;++i){auto& n=c->state.node[i];left=fminf(left,n.x);top=fminf(top,n.y);right=fmaxf(right,n.x+worldWidth(c,i));bottom=fmaxf(bottom,n.y+worldHeight(c,i));}
    c->zoom=fmaxf(.05f,fminf(2,fminf((c->width-48.f)/fmaxf(1,right-left),(c->height-48.f)/fmaxf(1,bottom-top))));
    c->panX=left-24/c->zoom;c->panY=top-24/c->zoom;c->overview=1;}
MpcEditorViewport viewport(PdCanvas* c){return {c->zoom,c->panX,c->panY,(uint32_t)(c->panMode?MPC_EDITOR_PAN:c->interact?MPC_EDITOR_CONTROLS:MPC_EDITOR_EDIT),(uint32_t)c->overview};}
void remember(PdCanvas* c){if(c->state.depth>=0&&c->state.depth<17){c->views.level[c->state.depth]=viewport(c);c->views.valid|=1u<<c->state.depth;}}
void recallView(PdCanvas* c){if(c->state.depth>=0&&c->state.depth<17&&(c->views.valid&(1u<<c->state.depth))){auto v=c->views.level[c->state.depth];c->zoom=v.zoom;c->panX=v.x;c->panY=v.y;c->overview=v.overview;c->interact=v.mode!=MPC_EDITOR_EDIT;c->panMode=v.mode==MPC_EDITOR_PAN;}else working(c);}
bool arrayPoint(PdCanvas* c,int x,int y){auto& n=c->state.node[c->selected];int nx,ny;position(c,c->selected,nx,ny);int w=nodeWidth(c,c->selected),h=nodeHeight(c,c->selected);
    if(w<1||h<1||x<nx||y<ny||x>=nx+w||y>=ny+h)return false;
    float index=n.array_x1+(x-nx)*(n.array_x2-n.array_x1)/w,value=n.array_y1+(y-ny)*(n.array_y2-n.array_y1)/h;
    if(!std::isfinite(index)||!std::isfinite(value)||index<0||index>=n.array_count)return false;int at=(int)index;
    if(c->arrayPrevious<0)c->arrayPreview[at]=value;
    else{int previous=c->arrayPrevious,lo=at<previous?at:previous,hi=at>previous?at:previous;float old=c->arrayPreview[previous];for(int i=lo;i<=hi;++i)c->arrayPreview[i]=at==previous?value:old+(value-old)*(i-previous)/(at-previous);}
    c->arrayPrevious=at;if(at<c->arrayFirst)c->arrayFirst=at;if(at>c->arrayLast)c->arrayLast=at;return true;
}
void arrayDraw(PdCanvas* c,int i,int x,int y,int w,int h){auto& n=c->state.node[i];
    if(n.array_offset<0||n.array_count<1||!std::isfinite(n.array_x1)||!std::isfinite(n.array_x2)||!std::isfinite(n.array_y1)||!std::isfinite(n.array_y2)||n.array_x1==n.array_x2||n.array_y1==n.array_y2){pd_font_draw(c->font,c->pixels,c->width,c->height,x+4,y+4,"Array retained: unsupported or display sample limit",12*c->zoom,0xffbb66,w-8,h-8);return;}
    int oldX=c->clipX,oldY=c->clipY,oldR=c->clipRight,oldB=c->clipBottom;c->clipX=clamp(x,oldX,oldR);c->clipY=clamp(y,oldY,oldB);c->clipRight=clamp(x+w,oldX,oldR);c->clipBottom=clamp(y+h,oldY,oldB);
    const float* values=c->contact==8&&c->selected==i?c->arrayPreview:c->state.sample+n.array_offset;
    int stride=(n.array_count+clamp(w,1,1280)-1)/clamp(w,1,1280),lastX=0,lastY=0;bool last=false;
    for(int k=0;k<n.array_count;k+=stride){float lo=values[k],hi=lo;for(int j=k+1;j<k+stride&&j<n.array_count;++j){if(values[j]<lo)lo=values[j];if(values[j]>hi)hi=values[j];}
        float px=x+(k-n.array_x1)*w/(n.array_x2-n.array_x1),py=y+(lo-n.array_y1)*h/(n.array_y2-n.array_y1),py2=y+(hi-n.array_y1)*h/(n.array_y2-n.array_y1);
        if(!std::isfinite(px)||!std::isfinite(py)||!std::isfinite(py2)){last=false;continue;}
        int xx=(int)fmaxf(-32768,fminf(32768,px)),yy=(int)fmaxf(-32768,fminf(32768,py)),yy2=(int)fmaxf(-32768,fminf(32768,py2));if(last)line(c,lastX,lastY,xx,yy,0x53d7c7);line(c,xx,yy,xx,yy2,0x53d7c7);lastX=xx;lastY=yy;last=true;}
    c->clipX=oldX;c->clipY=oldY;c->clipRight=oldR;c->clipBottom=oldB;
    char label[160];snprintf(label,sizeof(label),"%s [%d]",n.text,n.array_count);pd_font_draw(c->font,c->pixels,c->width,c->height,x+5,y+3,label,11*c->zoom,0xffffff,w-10,20*c->zoom);
    snprintf(label,sizeof(label),"%.7g",n.array_y1);pd_font_draw(c->font,c->pixels,c->width,c->height,x+w-90,y+3,label,10*c->zoom,0xb5c6df,88,20*c->zoom);
    snprintf(label,sizeof(label),"%.7g",n.array_y2);pd_font_draw(c->font,c->pixels,c->width,c->height,x+w-90,y+h-18*c->zoom,label,10*c->zoom,0xb5c6df,88,20*c->zoom);
}
void render(PdCanvas* c){
    c->dirty=0;
    c->clipX=c->clipY=0;c->clipRight=c->width;c->clipBottom=c->height;
    rect(c,0,0,c->width,c->height,0x111723);
    if(c->keyboard){int inputX=c->keyboard==1?200:20;pd_font_draw(c->font,c->pixels,c->width,c->height,inputX,5,c->input,26,0xffffff,c->width-inputX-150,48);
        if(c->keyboard==1){const char* kinds[]={"OBJECT","MESSAGE","COMMENT"};rect(c,3,3,185,50,0x334f65);pd_font_draw(c->font,c->pixels,c->width,c->height,14,12,kinds[c->createKind],22,0xffffff,172,36);}
        rect(c,c->width-135,3,130,50,0x334f65);pd_font_draw(c->font,c->pixels,c->width,c->height,c->width-120,12,c->keypage==0?"ABC":c->keypage==1?";,#":"abc",26,0xffffff,110,36);
        int h=(c->height-60)/6,w=c->width/10;
        for(int r=0;r<5;++r)for(int k=0;k<10;++k){char key[2]={keyboardKey(c->keypage,r,k),0};rect(c,k*w+3,60+r*h+3,w-6,h-6,0x2a3546);pd_font_draw(c->font,c->pixels,c->width,c->height,k*w+18,60+r*h+8,key,28,0xffffff,w-24,h-10);}
        const char* actions[]={"SPACE","DELETE",c->keyboard==1?"CREATE":"APPLY","CANCEL"};for(int k=0;k<4;++k){rect(c,k*c->width/4+3,60+5*h+3,c->width/4-6,h-6,0x334f65);pd_font_draw(c->font,c->pixels,c->width,c->height,k*c->width/4+20,60+5*h+8,actions[k],24,0xffffff,c->width/4-30,h-10);}
        if(!++c->revision)++c->revision;return;}
    for(int i=0;i<c->state.edges;++i){auto& e=c->state.edge[i];int x,y,xx,yy;portPosition(c,e.source,e.outlet,true,x,y);portPosition(c,e.target,e.inlet,false,xx,yy);line(c,x,y,xx,yy,0xf0bd50);}
    for(int i=0;i<c->state.visible;++i){auto& n=c->state.node[i];int x,y;position(c,i,x,y);int w=nodeWidth(c,i),h=nodeHeight(c,i);uint32_t border=n.unresolved?0xff5555:n.child?0x55b5cf:0x66778c;
        if(n.clip_w){int dx,dy;previewOffset(c,i,dx,dy);c->clipX=(n.clip_x+dx-c->panX)*c->zoom;c->clipY=(n.clip_y+dy-c->panY)*c->zoom;c->clipRight=c->clipX+n.clip_w*c->zoom;c->clipBottom=c->clipY+n.clip_h*c->zoom;}
        else {c->clipX=c->clipY=0;c->clipRight=c->width;c->clipBottom=c->height;}
        if(n.kind!=0||i==c->selected){rect(c,x,y,w,h,i==c->selected?0xf5cd61:border);rect(c,x+2,y+2,w-4,h-4,n.kind==0?0x111723:0x2a3546);}
        if(c->real){char value[64];const char* display=n.text;
            if(n.control==PD_CONTROL_NUMBER){snprintf(value,sizeof(value),"%.7g",c->contact==5&&i==c->selected?c->previewValue:n.value);display=value;}
            if(n.array)arrayDraw(c,i,x,y,w,h);
            else if(n.control==PD_CONTROL_SLIDER){float p=c->contact==7&&c->selected==i?c->previewValue:n.position;p=fmaxf(0,fminf(1,p));if(n.horizontal){line(c,x+3,y+h/2,x+w-3,y+h/2,0x9cabbc);rect(c,x+3+p*(w-9),y+3,4,h-6,0xf5cd61);}else{line(c,x+w/2,y+3,x+w/2,y+h-3,0x9cabbc);rect(c,x+3,y+3+(1-p)*(h-9),w-6,4,0xf5cd61);}}
            else if(n.control==PD_CONTROL_RADIO){int selected=c->contact==7&&c->selected==i?clamp(c->previewValue*n.steps,0,n.steps-1):(int)n.position;for(int k=0;k<n.steps;++k){int rx=x+(n.horizontal?k*w/n.steps:0),ry=y+(n.horizontal?0:k*h/n.steps),rw=n.horizontal?w/n.steps:w,rh=n.horizontal?h:h/n.steps;rect(c,rx+1,ry+1,rw-2,rh-2,0x66778c);rect(c,rx+3,ry+3,rw-6,rh-6,k==selected?0xf5cd61:0x2a3546);}}
            else if(n.control==PD_CONTROL_BANG){int radius=(w<h?w:h)/2-4;if(radius>0){circle(c,x+w/2,y+h/2,radius,0xdfe9f2);circle(c,x+w/2,y+h/2,radius-2,c->contact==4&&c->selected==i?0xf5cd61:0x2a3546);}}
            else if(n.control==PD_CONTROL_TOGGLE){if(n.value!=0){line(c,x+4,y+4,x+w-4,y+h-4,0xf5cd61);line(c,x+4,y+h-4,x+w-4,y+4,0xf5cd61);}}
            else if(!n.hide_text){
                // Short Pd number boxes keep their canonical geometry. Fit
                // the font's 1.25-line spacing inside that existing height.
                float textSize=12*c->zoom;
                if(n.control==PD_CONTROL_NUMBER)textSize=fminf(textSize,floorf((h-4)*.8f));
                pd_font_draw(c->font,c->pixels,c->width,c->height,x+4,y+2,display,textSize,0xf1f4f8,w-8,h-4,c->clipX,c->clipY,c->clipRight,c->clipBottom);
                if(n.control==PD_CONTROL_MESSAGE){line(c,x+w-9,y+2,x+w-2,y+h/2,0x55b5cf);line(c,x+w-2,y+h/2,x+w-9,y+h-2,0x55b5cf);}}
            if(n.label[0])pd_font_draw(c->font,c->pixels,c->width,c->height,n.label_positioned?x+n.label_x*c->zoom:x+w+6,n.label_positioned?y+(n.label_y-n.label_size/2)*c->zoom:y,n.label,(n.label_positioned?n.label_size:11)*c->zoom,0xbbc8d6,240,80,c->clipX,c->clipY,c->clipRight,c->clipBottom);
        }else text(c,x+16,y+60,n.text);
        if(n.owner_parent>=0)continue;
        for(int j=0;j<n.inputs;++j){int xx,yy;portPosition(c,i,j,false,xx,yy);circle(c,xx,yy,c->real?6:19,n.signal_inputs&(1u<<j)?0x36cfcc:0x9cabbc);if(!c->real)circle(c,xx,yy,10,0x111723);}
        for(int j=0;j<n.outputs;++j){int xx,yy;portPosition(c,i,j,true,xx,yy);circle(c,xx,yy,c->real?6:19,n.signal_outputs&(1u<<j)?0xf3b747:0x9cabbc);if(!c->real)circle(c,xx,yy,10,0x111723);}}
    c->clipX=c->clipY=0;c->clipRight=c->width;c->clipBottom=c->height;
    if(c->contact==2||c->wireArmed){
        int source=c->wireArmed?c->wireNode:c->selected,outlet=c->wireArmed?c->wirePort:c->port;
        if(source>=0&&source<c->state.count&&outlet<c->state.node[source].outputs){
            int x,y;portPosition(c,source,outlet,true,x,y);ring(c,x,y,12,0xffd35a);
            int endX=c->x,endY=c->y;
            if(c->wireArmed&&!c->contact&&abs(endX-x)<=22&&abs(endY-y)<=22){endX=x+36;endY=y-48;}
            line(c,x,y,endX,endY,0xffffff);
            const bool signal=(c->state.node[source].signal_outputs&(1u<<outlet))!=0;
            for(int n=0;n<c->state.count;++n)for(int p=0;p<c->state.node[n].inputs;++p)
                if(!signal||(c->state.node[n].signal_inputs&(1u<<p))){int px,py;portPosition(c,n,p,false,px,py);ring(c,px,py,12,0x52d5bf);}
        }
    }
    if(!++c->revision)++c->revision;
}
void draw(PdCanvas* c,const char* verb){
    c->dirty=1;
    if(c->wireArmed){snprintf(c->status,sizeof(c->status),"%s, output %d: tap an input; empty space cancels",c->state.node[c->wireNode].text,c->wirePort+1);return;}
    if(c->keyboard){snprintf(c->status,sizeof(c->status),"%s",c->keyboard==1?"NEW: tap OBJECT / MESSAGE / COMMENT to choose; CREATE uses Pd's owner":c->keyboard==4?"EDIT TEXT: Pd recreates objects; unavailable ports lose their cables":"CONTROL VALUE: Apply dispatches to the actual Pd object");return;}
    const char* selected=c->selected>=0?c->state.node[c->selected].text:c->state.warning;char confirmed[192];
    if(c->selected>=0&&c->state.node[c->selected].array){auto& n=c->state.node[c->selected];snprintf(confirmed,sizeof(confirmed),"%.64s: %d samples / %s / %s%s",n.text,n.array_count,n.array_edit?"draw in CONTROLS":"read only",n.array_save?"samples saved in patch":"samples NOT saved in patch",c->contact==8?" / PREVIEW":" / Pd confirmed");selected=confirmed;}
    if(c->selected>=0){auto& node=c->state.node[c->selected];
        if(node.control==PD_CONTROL_SLIDER||node.control==PD_CONTROL_RADIO){snprintf(confirmed,sizeof(confirmed),"%s %s = %.7g (Pd confirmed)",node.horizontal?"Horizontal":"Vertical",node.control==PD_CONTROL_SLIDER?"slider":"radio",node.value);selected=confirmed;}
        else if(node.control==PD_CONTROL_TOGGLE||node.control==PD_CONTROL_NUMBER){snprintf(confirmed,sizeof(confirmed),"%.96s = %.7g (Pd confirmed)",node.label[0]?node.label:node.control==PD_CONTROL_TOGGLE?"Toggle":"Number",node.value);selected=confirmed;}}
    snprintf(c->status,sizeof(c->status),"%s / %s / %s%s / %d objects %d cables / %s",verb,c->panMode?"PAN":c->interact?"CONTROLS":"EDIT",c->state.name,c->state.readonly?" [abstraction: structure read only]":"",c->state.count,c->state.edges,selected);
}
void read(PdCanvas* c,const char* verb){if(!pd_touch_snapshot(c->pd,&c->state)){memset(&c->state,0,sizeof(c->state));c->contact=0;verb=pd_touch_error(c->pd);}if(c->selected>=c->state.visible)c->selected=-1;if(c->wireArmed&&(c->wireNode>=c->state.count||c->wirePort>=c->state.node[c->wireNode].outputs))c->wireArmed=0;draw(c,verb);}
int applyText(PdCanvas* c,int mode){
    int ok=0;
    if(mode==1)ok=pd_touch_create_text(c->pd,c->createKind,c->input,clamp((int)c->panX+40,0,30000),clamp((int)c->panY+60,0,30000));
    else if(mode==2){char* end=nullptr;float value=strtof(c->input,&end);if(end&&end!=c->input&&!*end&&std::isfinite(value))ok=pd_touch_number_from_snapshot(c->pd,&c->state,c->selected,value);}
    else if(mode==4){int before=c->state.edges;ok=pd_touch_edit_text(c->pd,c->selected,c->input,&c->selected);read(c,ok?"TEXT APPLIED":pd_touch_error(c->pd));if(ok){char result[96];snprintf(result,sizeof(result),"TEXT APPLIED: Pd cables %d -> %d%s",before,c->state.edges,c->state.edges<before?" (ports/connections removed)":"");draw(c,result);}return ok;}
    else ok=pd_touch_atom_text(c->pd,c->selected,c->input);
    read(c,ok?(mode==1?"OBJECT ADDED":"CONTROL APPLIED"):pd_touch_error(c->pd));
    if(ok&&mode==1){c->selected=c->state.count-1;draw(c,"OBJECT ADDED");}
    return ok;
}
bool hitPort(PdCanvas* c,int x,int y,bool output,int& node,int& port,bool* ambiguous=nullptr){
    const int radius=c->real?22:24,exactRadius=c->real?6:19;
    int near=0,exact=0,exactNode=0,exactPort=0;
    if(ambiguous)*ambiguous=false;
    for(int n=c->state.count-1;n>=0;--n){int count=output?c->state.node[n].outputs:c->state.node[n].inputs;
        for(int p=0;p<count;++p){int xx,yy;portPosition(c,n,p,output,xx,yy);
            if(xx<0||yy<0||xx>=c->width||yy>=c->height)continue;
            int distance=(x-xx)*(x-xx)+(y-yy)*(y-yy);
            if(distance<=exactRadius*exactRadius){++exact;exactNode=n;exactPort=p;}
            if(distance<=radius*radius){++near;node=n;port=p;}
        }
    }
    if(exact==1){node=exactNode;port=exactPort;return true;}
    if(exact>1||near>1){if(ambiguous)*ambiguous=true;return false;}
    return near==1;
}
}
extern "C" PdCanvas* pd_canvas_open(uint32_t width,uint32_t height){
    if(width<640||width>1280||height<400||height>800)return nullptr;
    auto* c=(PdCanvas*)calloc(1,sizeof(PdCanvas));if(!c)return nullptr;c->width=width;c->height=height;c->selected=-1;
    c->pixels=(unsigned char*)malloc(width*height*4);c->font=pd_font_open("/tmp/mpclearn-pd-touch/assets/DejaVuSans.ttf");c->zoom=1;c->pd=pd_touch_open();if(!c->pixels||!c->pd){pd_canvas_close(&c);return nullptr;}read(c,"READY / DSP STOPPED");return c;
}
extern "C" PdCanvas* pd_canvas_open_document(uint32_t width,uint32_t height,const char* source,int saved_copy){
    if(width<640||width>1280||height<400||height>800)return nullptr;
    auto* c=(PdCanvas*)calloc(1,sizeof(PdCanvas));if(!c)return nullptr;c->width=width;c->height=height;c->selected=-1;c->zoom=1;
    c->pixels=(unsigned char*)malloc(width*height*4);c->font=pd_font_open("/tmp/mpclearn-pd-touch/assets/DejaVuSans.ttf");c->pd=pd_touch_open_document(source,saved_copy);
    if(!c->pixels||!c->font||!c->pd){pd_canvas_close(&c);return nullptr;}c->real=1;c->interact=1;read(c,"READY");working(c);draw(c,"READY");return c;
}
extern "C" PdCanvas* pd_canvas_prepare_attachment(uint32_t width,uint32_t height){
    if(width<640||width>1280||height<400||height>800)return nullptr;
    auto* c=(PdCanvas*)calloc(1,sizeof(PdCanvas));if(!c)return nullptr;
    c->width=width;c->height=height;c->selected=-1;c->zoom=1;c->real=1;c->borrowed=1;c->interact=1;
    c->pixels=(unsigned char*)malloc(width*height*4);c->font=pd_font_open("/tmp/mpclearn-pd-touch/assets/DejaVuSans.ttf");
    if(!c->pixels||!c->font){pd_canvas_close(&c);return nullptr;}return c;
}
extern "C" int pd_canvas_bind_attachment(PdCanvas* c,PdTouch* owner){
    if(!c||!owner||!c->borrowed||c->pd)return 0;
    c->pd=owner;read(c,"MPC OWNER");return 1;
}
extern "C" PdCanvas* pd_canvas_attach(uint32_t width,uint32_t height,PdTouch* owner){
    if(!owner)return nullptr;auto* c=pd_canvas_prepare_attachment(width,height);if(!c)return nullptr;
    if(!pd_canvas_bind_attachment(c,owner)){pd_canvas_close(&c);return nullptr;}
    working(c);draw(c,"MPC OWNER");return c;
}
extern "C" void pd_canvas_sync(PdCanvas* c){if(c){c->contact=c->keyboard=c->externalText=c->wireArmed=0;c->selected=-1;c->views.valid=0;read(c,"OWNER RECALLED");working(c);draw(c,"OWNER RECALLED");}}
extern "C" void pd_canvas_close(PdCanvas** p){if(!p||!*p)return;auto* c=*p;if(!c->borrowed)pd_touch_close(&c->pd);pd_font_close(c->font);free(c->pixels);free(c);*p=nullptr;}
extern "C" void pd_canvas_pointer(PdCanvas* c,uint32_t event,float fx,float fy){
    if(!c||!std::isfinite(fx)||!std::isfinite(fy))return;
    if(event==1)++c->parameterBinding;
    if(c->keyboard){if(event==4){c->keyboard=0;draw(c,"KEYBOARD CANCEL");return;}if(event==1&&c->keyboard==1&&fx>=0&&fx<190&&fy>=0&&fy<60){c->createKind=(c->createKind+1)%3;draw(c,"KEYBOARD");return;}if(event==1&&fx>=c->width-135&&fx<c->width&&fy>=0&&fy<60){c->keypage=(c->keypage+1)%3;draw(c,"KEYBOARD");return;}if(event!=1||fx<0||fy<60||fx>=c->width||fy>=c->height)return;
        int row=((int)fy-60)/((c->height-60)/6),column=(int)fx/(c->width/10);size_t len=strlen(c->input);
        if(row<5&&column<10){if(len<(c->keyboard==4?511u:255u))c->input[len]=keyboardKey(c->keypage,row,column),c->input[len+1]=0;}
        else if(row>=5){int key=(int)fx/(c->width/4);if(key==0&&len<(c->keyboard==4?511u:255u))c->input[len]=' ',c->input[len+1]=0;else if(key==1&&len)c->input[len-1]=0;
            else if(key==2){int mode=c->keyboard;c->keyboard=0;applyText(c,mode);return;}else if(key==3)c->keyboard=0;}
        draw(c,"KEYBOARD");return;}
    // Native capture continues delivering MOVE/UP outside the image. Leaving
    // the canvas cancels presentation intent; it is not a clamped Pd move.
    const bool edge=fx<2||fy<2||fx>=c->width-2||fy>=c->height-2;
    if(event==4||edge){if(c->contact||c->wireArmed){c->contact=c->wireArmed=0;draw(c,edge?"EDGE CANCEL":"CANCEL");}return;}
    int x=(int)fx,y=(int)fy;
    if(event==1&&c->wireArmed){c->contact=9;c->startX=c->x=x;c->startY=c->y=y;draw(c,"CONNECT");return;}
    if(event==1&&c->overview){float wx=fx/c->zoom+c->panX,wy=fy/c->zoom+c->panY;c->zoom=2;c->panX=wx-c->width/(2*c->zoom);c->panY=wy-c->height/(2*c->zoom);c->overview=0;c->contact=0;draw(c,"WORKING VIEW");return;}
    if(event==1&&c->panMode){c->contact=3;c->startPanX=c->panX;c->startPanY=c->panY;c->startX=c->x=x;c->startY=c->y=y;return;}
    if(event==1&&c->interact){c->contact=0;c->selected=-1;
        for(int i=c->state.visible-1;i>=0;--i){auto& n=c->state.node[i];if(n.kind==0)continue;int nx,ny;position(c,i,nx,ny);
            if(n.clip_w){float wx=x/c->zoom+c->panX,wy=y/c->zoom+c->panY;if(wx<n.clip_x||wy<n.clip_y||wx>=n.clip_x+n.clip_w||wy>=n.clip_y+n.clip_h)continue;}
            if(x>=nx&&x<nx+nodeWidth(c,i)&&y>=ny&&y<ny+nodeHeight(c,i)){c->selected=i;c->contact=(n.control==PD_CONTROL_SLIDER||n.control==PD_CONTROL_RADIO)?7:n.control==PD_CONTROL_NUMBER?5:(n.control==PD_CONTROL_SYMBOL||n.control==PD_CONTROL_LIST)?6:n.control?4:0;c->startValue=c->previewValue=n.value;if(c->contact==7){c->previewValue=n.horizontal?(x-nx)/(float)nodeWidth(c,i):(y-ny)/(float)nodeHeight(c,i);if(n.control==PD_CONTROL_SLIDER&&!n.horizontal)c->previewValue=1-c->previewValue;}
                if(n.array&&n.array_edit&&n.array_offset>=0){memcpy(c->arrayPreview,c->state.sample+n.array_offset,n.array_count*sizeof(float));c->arrayFirst=n.array_count;c->arrayLast=c->arrayPrevious=-1;c->contact=8;if(!arrayPoint(c,x,y))c->contact=0;}break;}}
        if(c->selected<0){c->contact=3;c->startPanX=c->panX;c->startPanY=c->panY;}
        c->continuous=0;c->startX=c->x=x;c->startY=c->y=y;draw(c,"CONTROL TOUCH");return;}
    if(event==1){c->contact=0;int n,p;bool ambiguous=false;bool output=!c->state.readonly&&hitPort(c,x,y,true,n,p,&ambiguous);
        if(ambiguous){draw(c,"Ports overlap here; zoom in to choose one");return;}
        if(output){c->selected=n;c->port=p;c->contact=2;}else{
        c->selected=-1;for(int i=c->state.count-1;i>=0;--i){auto& n=c->state.node[i];int nx,ny;position(c,i,nx,ny);if(x>=nx&&x<nx+nodeWidth(c,i)&&y>=ny&&y<ny+nodeHeight(c,i)){c->selected=i;c->contact=n.editable?1:0;c->previewX=n.x;c->previewY=n.y;break;}}if(c->selected<0&&c->real){c->contact=3;c->startPanX=c->panX;c->startPanY=c->panY;}}
        c->startX=x;c->startY=y;c->x=x;c->y=y;draw(c,"SELECT");return;}
    if(!c->contact)return;if(event==2&&x==c->x&&y==c->y)return;c->x=x;c->y=y;
    if(c->contact==8&&!arrayPoint(c,x,y)){c->contact=0;draw(c,"ARRAY EDGE CANCEL");return;}
    if((c->contact==4||c->contact==6)&&(abs(x-c->startX)>12||abs(y-c->startY)>12)){c->contact=0;draw(c,"CONTROL CANCEL");return;}
    if(c->contact==5){c->previewValue=c->startValue+(c->startY-y)/c->zoom;auto& n=c->state.node[c->selected];if(n.maximum>n.minimum)c->previewValue=fmaxf(n.minimum,fminf(n.maximum,c->previewValue));}
    if(c->contact==7){auto& n=c->state.node[c->selected];int nx,ny;position(c,c->selected,nx,ny);c->previewValue=n.horizontal?(x-nx)/(float)nodeWidth(c,c->selected):(y-ny)/(float)nodeHeight(c,c->selected);if(n.control==PD_CONTROL_SLIDER&&!n.horizontal)c->previewValue=1-c->previewValue;c->previewValue=fmaxf(0,fminf(1,c->previewValue));}
    if(c->contact==1){auto& n=c->state.node[c->selected];c->previewX=c->real?clamp(n.x+(x-c->startX)/c->zoom,0,30000):clamp(n.x+x-c->startX,4,c->width-cardW-4);c->previewY=c->real?clamp(n.y+(y-c->startY)/c->zoom,0,30000):clamp(n.y+y-c->startY,24,c->height-cardH-24);}
    if(c->contact==3){c->panX=c->startPanX-(x-c->startX)/c->zoom;c->panY=c->startPanY-(y-c->startY)/c->zoom;}
    if(event==2&&(c->contact==5||c->contact==7)&&(c->continuous||abs(x-c->startX)>3||abs(y-c->startY)>3)){
        const int ok=c->contact==5?pd_touch_number_from_snapshot(c->pd,&c->state,c->selected,c->previewValue):pd_touch_control_position(c->pd,c->selected,c->previewValue);
        c->continuous=1;read(c,ok?"CONTROL":pd_touch_error(c->pd));return;}
    if(event==3){int mode=c->contact,n,p;c->contact=0;int ok=1;
        if(mode==1)ok=pd_touch_move(c->pd,c->selected,c->previewX,c->previewY);
        else if(mode==2||mode==9){
            const int source=mode==9?c->wireNode:c->selected,outlet=mode==9?c->wirePort:c->port;
            if(mode==2&&abs(x-c->startX)<=8&&abs(y-c->startY)<=8){
                c->wireArmed=1;c->wireNode=source;c->wirePort=outlet;draw(c,"CONNECT");return;
            }
            bool ambiguous=false;
            if(hitPort(c,x,y,false,n,p,&ambiguous)){
                ok=pd_touch_connect(c->pd,source,outlet,n,p);c->wireArmed=0;
                read(c,ok?"CONNECTED":pd_touch_error(c->pd));return;
            }
            c->wireArmed=0;
            draw(c,ambiguous?"Ports overlap here; zoom in to choose one":"CABLE CANCELLED");return;
        }
        else if(mode==4)ok=pd_touch_activate(c->pd,c->selected);
        else if(mode==7)ok=pd_touch_control_position(c->pd,c->selected,c->previewValue);
        else if(mode==8)ok=pd_touch_array(c->pd,c->selected,c->state.revision,c->arrayFirst,c->arrayLast-c->arrayFirst+1,c->arrayPreview+c->arrayFirst);
        else if(mode==5){if(!c->continuous&&abs(y-c->startY)<=3){c->keyboard=2;c->keypage=0;snprintf(c->input,sizeof(c->input),"%.7g",c->state.node[c->selected].value);draw(c,"VALUE");return;}ok=pd_touch_number_from_snapshot(c->pd,&c->state,c->selected,c->previewValue);}
        else if(mode==6){c->keyboard=3;c->keypage=0;snprintf(c->input,sizeof(c->input),"%.255s",c->state.node[c->selected].text);draw(c,"VALUE");return;}
        read(c,ok?"EDIT":pd_touch_error(c->pd));
    }else draw(c,"PREVIEW");
}
extern "C" void pd_canvas_action(PdCanvas* c,uint32_t action){if(!c)return;++c->parameterBinding;c->contact=c->wireArmed=0;int ok=0;const char* verb="EDIT";
    c->keyboard=c->externalText=0;c->keypage=0;
    switch(action){case 1:case 2:{int n=c->state.count;ok=pd_touch_add(c->pd,action-1,20+(n%4)*290,24+(n/4)*210);if(ok)c->selected=n;break;}
    case 3:ok=pd_touch_unwire(c->pd,c->selected);verb="UNWIRED";break;
    case 4:ok=pd_touch_delete(c->pd,c->selected);c->selected=-1;verb="DELETED";break;
    case 5:ok=pd_touch_save(c->pd);verb="SAVED";break;
    case 6:ok=pd_touch_reload(c->pd);c->selected=-1;verb="RELOADED";break;
    case 14:ok=pd_touch_recall_copy(c->pd);c->selected=-1;verb="COPY LOADED";break;
    case 22:c->interact=!c->interact;ok=1;verb=c->interact?"CONTROLS: touch messages/bangs; drag or tap numbers":"EDIT: move/wire/select objects";break;
    case 7:remember(c);ok=pd_touch_enter(c->pd,c->selected);if(ok&&c->state.depth<16)c->views.valid&=(1u<<(c->state.depth+1))-1;c->selected=-1;verb="SUBPATCH";break;
    case 8:remember(c);ok=pd_touch_parent(c->pd);c->selected=-1;verb="PARENT";break;
    case 9:fit(c);ok=1;verb="FIT";break;
    case 11:case 12:pd_canvas_view(c,MPC_EDITOR_VIEW_ZOOM,c->width/2.f,c->height/2.f,action==11?1.25f:.8f);return;default:break;}
    if(action==21||action==24||action==25){c->input[0]=0;c->keyboard=1;c->createKind=action==24?PD_CREATE_MESSAGE:action==25?PD_CREATE_COMMENT:PD_CREATE_OBJECT;draw(c,"NEW");return;}
    if(action==23){if(c->selected>=0&&c->selected<c->state.count&&c->state.node[c->selected].text_editable){snprintf(c->input,sizeof(c->input),"%s",c->state.node[c->selected].text);c->keyboard=4;draw(c,"EDIT TEXT");}else draw(c,"SELECT EDITABLE OBJECT, MESSAGE OR COMMENT (full text required)");return;}
    read(c,ok?verb:pd_touch_error(c->pd));
    if(ok&&(action==7||action==8)){recallView(c);draw(c,verb);}
}
extern "C" int pd_canvas_load(PdCanvas* c,const char* path){if(!c||!c->font)return 0;c->contact=c->keyboard=c->externalText=c->wireArmed=0;int ok=pd_touch_load(c->pd,path);if(ok){c->real=1;c->selected=-1;c->views.valid=0;read(c,"LOADED");working(c);draw(c,"LOADED");}else read(c,pd_touch_error(c->pd));return ok;}
extern "C" void pd_canvas_view_state(PdCanvas* c,PdCanvasViewState* state,int restore){if(!c||!state)return;if(restore){c->views=*state;recallView(c);draw(c,"VIEW RESTORED");}else{remember(c);*state=c->views;}}
extern "C" int pd_canvas_viewport(PdCanvas* c,MpcEditorViewport* state){if(!c||!state)return 0;*state=viewport(c);return 1;}
extern "C" int pd_canvas_view(PdCanvas* c,uint32_t intent,float x,float y,float amount){
    if(!c||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(amount))return 0;
    c->contact=c->keyboard=c->externalText=c->wireArmed=0;
    switch(intent){
    case MPC_EDITOR_VIEW_MODE:if(amount<0||amount>2)return 0;c->interact=amount!=MPC_EDITOR_EDIT;c->panMode=amount==MPC_EDITOR_PAN;break;
    case MPC_EDITOR_VIEW_PAN:c->panX+=x/c->zoom;c->panY+=y/c->zoom;break;
    case MPC_EDITOR_VIEW_ZOOM_SELECTION:
        if(c->selected>=0&&c->selected<c->state.visible){int nx,ny;position(c,c->selected,nx,ny);x=nx+nodeWidth(c,c->selected)/2.f;y=ny+nodeHeight(c,c->selected)/2.f;}
        else{x=c->width/2.f;y=c->height/2.f;}
        __attribute__((fallthrough));
    case MPC_EDITOR_VIEW_ZOOM:{if(amount<=0)return 0;float wx=c->panX+x/c->zoom,wy=c->panY+y/c->zoom;c->zoom=fmaxf(.05f,fminf(6,c->zoom*amount));c->panX=wx-x/c->zoom;c->panY=wy-y/c->zoom;c->overview=0;break;}
    case MPC_EDITOR_VIEW_FIT:fit(c);break;
    case MPC_EDITOR_VIEW_FOCUS:if(c->selected>=0){auto& n=c->state.node[c->selected];c->zoom=2;c->panX=n.x+worldWidth(c,c->selected)/2.f-c->width/4.f;c->panY=n.y+worldHeight(c,c->selected)/2.f-c->height/4.f;c->overview=0;}else working(c);break;
    default:return 0;}
    draw(c,c->overview?"OVERVIEW: tap a region to work there":c->panMode?"PAN":"VIEW");return 1;
}
extern "C" int pd_canvas_create(PdCanvas* c,int kind,const char* text,float x,float y){
    if(!c||!text||!std::isfinite(x)||!std::isfinite(y)||x<0||y<0||x>=c->width||y>=c->height)return 0;
    c->contact=c->keyboard=c->externalText=c->wireArmed=0;const int ok=pd_touch_create_text(c->pd,kind,text,clamp(x/c->zoom+c->panX,0,30000),clamp(y/c->zoom+c->panY,0,30000));
    read(c,ok?"ADDED":pd_touch_error(c->pd));if(ok){c->selected=c->state.count-1;c->interact=c->panMode=0;draw(c,"ADDED / EDIT");}return ok;
}
extern "C" const unsigned char* pd_canvas_pixels(PdCanvas* c,uint32_t* revision){if(!c)return nullptr;if(c->dirty)render(c);*revision=c->revision;return c->pixels;}
extern "C" int pd_canvas_text_begin(PdCanvas* c,char* title,uint32_t titleSize,char* initial,uint32_t initialSize){
    if(!c||!c->keyboard||c->externalText||!title||titleSize<64||!initial||initialSize<sizeof(c->input))return 0;
    const char* label=c->keyboard==1?(c->createKind==PD_CREATE_MESSAGE?"New message":c->createKind==PD_CREATE_COMMENT?"New comment":"New object and arguments"):c->keyboard==4?"Edit text and arguments":c->keyboard==2?"Number value":"Symbol value";
    snprintf(title,titleSize,"%s",label);snprintf(initial,initialSize,"%s",c->input);
    c->externalText=c->keyboard;c->keyboard=c->contact=0;draw(c,"TEXT ENTRY");return 1;
}
extern "C" int pd_canvas_text_finish(PdCanvas* c,int accepted,const char* text){
    if(!c||!c->externalText)return 0;int mode=c->externalText;c->externalText=0;
    if(!accepted){draw(c,"TEXT CANCELLED");return 1;}
    if(!text||strlen(text)>=sizeof(c->input)){draw(c,"TEXT TOO LONG");return 0;}
    snprintf(c->input,sizeof(c->input),"%s",text);int ok=applyText(c,mode);
    if(ok&&mode==1){c->interact=c->panMode=0;draw(c,"OBJECT ADDED / EDIT");}return ok;
}
extern "C" const char* pd_canvas_status(PdCanvas* c){return c?c->status:"Pd editor unavailable";}

extern "C" int pd_canvas_parameters(PdCanvas* c,MpcEditorParameters* output){
    if(!c||!output)return 0;*output={};
    PdTouchNode node{};
    bool valid=!c->keyboard&&!c->externalText&&c->selected>=0&&
        pd_touch_control_read(c->pd,&c->state,c->selected,&node);
    if(valid){
        // Pd's default unbounded number box range is +/-1e37. A normalized
        // knob has no meaningful position there; leave that box unmapped.
        if(node.control==PD_CONTROL_NUMBER)
            valid=std::isfinite(node.minimum)&&std::isfinite(node.maximum)&&
                node.maximum>node.minimum&&node.minimum>-1e37f&&node.maximum<1e37f;
        else if(node.control==PD_CONTROL_SLIDER)
            valid=std::isfinite(node.position)&&node.maximum!=node.minimum;
        else if(node.control==PD_CONTROL_RADIO)valid=node.steps>1;
        else valid=node.control==PD_CONTROL_TOGGLE;
    }
    const uint64_t identity=valid?node.identity:0;
    if(!c->parameterBinding||c->parameterIdentity!=identity||c->parameterSelected!=c->selected||
       (valid&&(c->parameterMinimum!=node.minimum||c->parameterMaximum!=node.maximum))){
        ++c->parameterBinding;c->parameterIdentity=identity;c->parameterSelected=c->selected;
        c->parameterMinimum=node.minimum;c->parameterMaximum=node.maximum;
    }
    output->binding=c->parameterBinding;
    snprintf(output->owner,sizeof(output->owner),"%s",valid?(node.label[0]?node.label:node.class_name):"Select a control");
    if(!valid)return 1;
    auto& parameter=output->parameters[0];parameter.id=c->selected;output->count=1;
    if(node.control==PD_CONTROL_SLIDER)parameter.value=node.position;
    else if(node.control==PD_CONTROL_RADIO){parameter.value=node.position/(node.steps-1);parameter.steps=node.steps;}
    else if(node.control==PD_CONTROL_TOGGLE){parameter.value=node.value!=0;parameter.steps=2;}
    else parameter.value=(node.value-node.minimum)/(node.maximum-node.minimum);
    parameter.value=fmaxf(0,fminf(1,parameter.value));
    snprintf(parameter.name,sizeof(parameter.name),"%.63s",node.label[0]?node.label:
        node.control==PD_CONTROL_TOGGLE?"Toggle":node.control==PD_CONTROL_RADIO?"Choice":
        node.control==PD_CONTROL_SLIDER?"Level":"Value");
    snprintf(parameter.display,sizeof(parameter.display),"%.7g",node.value);
    if(c->state.node[c->selected].value!=node.value||c->state.node[c->selected].position!=node.position){
        c->state.node[c->selected]=node;draw(c,"CONTROL");
    }
    return 1;
}
extern "C" int pd_canvas_parameter(PdCanvas* c,uint64_t binding,uint32_t index,uint32_t event,float value){
    if(!c||index||!std::isfinite(value)||event<MPC_EDITOR_PARAMETER_BEGIN||event>MPC_EDITOR_PARAMETER_END)return 0;
    MpcEditorParameters current{};
    if(!pd_canvas_parameters(c,&current)||current.binding!=binding||current.count!=1)return 0;
    if(event!=MPC_EDITOR_PARAMETER_SET)return 1;
    value=fmaxf(0,fminf(1,value));auto& node=c->state.node[c->selected];int ok=0;
    if(node.control==PD_CONTROL_TOGGLE)ok=((node.value!=0)==(value>=.5f))||pd_touch_activate(c->pd,c->selected);
    else if(node.control==PD_CONTROL_NUMBER){
        ok=pd_touch_number_from_snapshot(c->pd,&c->state,c->selected,c->parameterMinimum+value*(c->parameterMaximum-c->parameterMinimum));
        read(c,ok?"Q-LINK":pd_touch_error(c->pd));return ok;
    }
    else ok=pd_touch_control_position(c->pd,c->selected,value);
    if(ok)read(c,"Q-LINK");return ok;
}
