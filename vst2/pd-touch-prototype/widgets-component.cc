#define main controls_regression_main
#include "controls-component.cc"
#undef main
static int where(PdCanvas* c,int x,int y){for(int i=0;i<c->state.count;++i)if(c->state.node[i].x==x&&c->state.node[i].y==y)return i;return -1;}
static unsigned numberInk(PdCanvas* c,int node){
    uint32_t revision;const auto* pixels=pd_canvas_pixels(c,&revision);int x,y;position(c,node,x,y);unsigned ink=0;
    // Count light text inside the box, excluding its border and darker ports.
    for(int yy=y+3;yy<y+nodeHeight(c,node)-3;++yy)for(int xx=x+3;xx<x+nodeWidth(c,node)-3;++xx){
        const auto* p=pixels+4*(yy*c->width+xx);if(p[0]>200&&p[1]>200&&p[2]>200)++ink;
    }return ink;
}
static void control(PdCanvas* c,int node,float position){frame(c,node);int x,y;center(c,node,x,y);auto& n=c->state.node[node];int nx,ny;::position(c,node,nx,ny);
    if(n.horizontal)x=nx+(int)(position*nodeWidth(c,node));else y=ny+(int)((n.control==PD_CONTROL_SLIDER?1-position:position)*nodeHeight(c,node));
    pd_canvas_pointer(c,1,x,y);pd_canvas_pointer(c,2,x,y);pd_canvas_pointer(c,3,x,y);}
int main(){auto* c=pd_canvas_open_document(1216,425,"",0);CHECK(c);
    CHECK(pd_canvas_load(c,"/tmp/mpclearn-pd-touch/library/doc/5.reference/sliders-help.pd"));CHECK(c->interact);
    int horizontal=where(c,205,363),vertical=where(c,32,282);CHECK(horizontal>=0&&vertical>=0&&c->state.node[horizontal].control==PD_CONTROL_SLIDER&&c->state.node[vertical].control==PD_CONTROL_SLIDER);
    control(c,horizontal,.75);CHECK(fabs(c->state.node[horizontal].value-95.25)<.1&&fabs(c->state.node[where(c,202,412)].value-95.25)<.1);
    printf("HSL actual range0..127 touch75%% -> owner/downstream %g/%g\n",c->state.node[horizontal].value,c->state.node[where(c,202,412)].value);
    control(c,vertical,.25);CHECK(fabs(c->state.node[vertical].value-31.75)<.1&&fabs(c->state.node[where(c,32,434)].value-31.75)<.1);printf("VSL touch25%% -> owner/downstream %g/%g\n",c->state.node[vertical].value,c->state.node[where(c,32,434)].value);CHECK(raster(c,"/work/adapter/widgets-slider.ppm"));
    int x,y;center(c,vertical,x,y);pd_canvas_pointer(c,1,x,y);pd_canvas_pointer(c,2,x,y-25);
    float held=c->state.node[vertical].value;CHECK(held>31.75&&c->state.node[where(c,32,434)].value==held);
    pd_canvas_pointer(c,4,x,y-25);pd_canvas_pointer(c,3,x,y-25);CHECK(c->state.node[vertical].value==held);
    pd_canvas_pointer(c,1,x,y);pd_canvas_pointer(c,2,c->width+30,y);pd_canvas_pointer(c,2,x,y-25);pd_canvas_pointer(c,3,x,y-25);CHECK(c->state.node[vertical].value==held);puts("SLIDER held move reaches owner/downstream before UP; CANCEL and edge exit preserve delivered value; stale UP sends no later float");
    CHECK(pd_canvas_load(c,"/tmp/mpclearn-pd-touch/library/doc/5.reference/radio-help.pd"));horizontal=where(c,174,358);vertical=where(c,34,283);CHECK(horizontal>=0&&vertical>=0);
    control(c,horizontal,.58);CHECK(c->state.node[horizontal].value==3&&c->state.node[where(c,174,412)].value==3);control(c,vertical,.9);CHECK(c->state.node[vertical].value==5&&c->state.node[where(c,34,412)].value==5);
    printf("HRADIO actual6cells -> owner/downstream3; VRADIO -> owner5\n");CHECK(raster(c,"/work/adapter/widgets-radio.ppm"));
    pd_canvas_action(c,5);CHECK(!strncmp(c->status,"SAVED",5));pd_canvas_action(c,6);CHECK(c->state.node[where(c,174,358)].value==0);control(c,where(c,174,358),.75);CHECK(c->state.node[where(c,174,412)].value==4);
    CHECK(pd_canvas_load(c,"/tmp/mpclearn-pd-touch/library/doc/2.control.examples/12.PART2.subpatch.pd"));int panel=where(c,558,522),list=-1;CHECK(panel>=0&&c->state.node[panel].gop&&c->state.node[panel].pixel_width==100&&c->state.node[panel].pixel_height==80);
    for(int i=c->state.count;i<c->state.visible;++i)if(c->state.node[i].owner_parent==panel&&c->state.node[i].control==PD_CONTROL_LIST)list=i;
    CHECK(list>=0&&c->state.node[list].x==565&&c->state.node[list].y==568);frame(c,panel);tap(c,list);CHECK(type_value(c,"21 8 hello"));CHECK(!strcmp(c->state.node[list].text,"21 8 hello")&&!strcmp(c->state.node[where(c,558,625)].text,"21 8 hello"));
    pd_canvas_action(c,22);center(c,panel,x,y);int lx,ly;position(c,list,lx,ly);pd_canvas_pointer(c,1,x,y);pd_canvas_pointer(c,2,x+20,y+10);int previewX,previewY;position(c,list,previewX,previewY);CHECK(previewX==lx+20&&previewY==ly+10&&c->state.node[panel].x==558);
    pd_canvas_pointer(c,4,x+20,y+10);pd_canvas_pointer(c,3,x+20,y+10);position(c,list,previewX,previewY);CHECK(previewX==lx&&previewY==ly&&c->state.node[list].x==565);pd_canvas_action(c,22);
    puts("GOP move preview translates panel/child/clip together; CANCEL restores presentation and leaves Pd positions unchanged");
    printf("GOP actual100x80 margin100,150 -> parent list565,568; touch Apply21 8 hello -> same abstraction list and parent outlet list\n");CHECK(raster(c,"/work/adapter/widgets-gop.ppm"));
    c->selected=panel;pd_canvas_action(c,7);CHECK(c->state.readonly&&!strcmp(c->state.node[7].text,"21 8 hello"));frame(c,7);CHECK(raster(c,"/work/adapter/widgets-gop-inside.ppm"));pd_canvas_action(c,8);
    frame(c,panel);tap(c,list);CHECK(c->keyboard);pd_canvas_pointer(c,4,0,0);CHECK(!c->keyboard&&!strcmp(c->state.node[list].text,"21 8 hello"));
    pd_canvas_action(c,5);CHECK(!strncmp(c->status,"SAVED",5));pd_canvas_action(c,6);
    panel=where(c,558,522);list=-1;for(int i=c->state.count;i<c->state.visible;++i)if(c->state.node[i].owner_parent==panel&&c->state.node[i].control==PD_CONTROL_LIST)list=i;CHECK(list>=0&&!strcmp(c->state.node[list].text,"17 1 hi"));
    frame(c,panel);CHECK(raster(c,"/work/adapter/widgets-gop-reload.ppm"));pd_canvas_close(&c);
    c=pd_canvas_open_document(1216,425,"",0);CHECK(c);
    CHECK(pd_touch_create_text(c->pd,PD_CREATE_OBJECT,"nbx",100,100));read(c,"NUMBER CREATED");
    int number=where(c,100,100);CHECK(number>=0&&c->state.node[number].control==PD_CONTROL_NUMBER&&c->state.node[number].pixel_height==14);
    frame(c,number);CHECK(numberInk(c,number)>10);
    center(c,number,x,y);pd_canvas_pointer(c,1,x,y);pd_canvas_pointer(c,2,x,y-38);
    CHECK(c->state.node[number].value==19&&numberInk(c,number)>10);CHECK(raster(c,"/work/adapter/widgets-short-number-held.ppm"));
    pd_canvas_pointer(c,3,x,y-38);CHECK(c->state.node[number].value==19&&c->state.node[number].pixel_height==14);pd_canvas_close(&c);
    puts("NUMBER actual default 14px nbx: zero and held value19 have visible numeric pixels at working zoom, with canonical geometry unchanged");
    puts("PASS unchanged upstream slider/radio/GOP controls: canonical float/list methods, owner geometry, parent/subpatch same state, Pd Save Copy/fresh Reload; native host/input substituted; no audio/clock processing");return 0;
}
