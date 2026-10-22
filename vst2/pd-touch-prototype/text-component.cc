#define main controls_regression_main
#include "controls-component.cc"
#undef main
static int at(PdCanvas* c,int x,int y){for(int i=0;i<c->state.count;++i)if(c->state.node[i].x==x&&c->state.node[i].y==y)return i;return -1;}
static bool edit(PdCanvas* c,int node,const char* value){c->selected=node;pd_canvas_action(c,23);return c->keyboard==4&&type_value(c,value);}
int main(){auto* c=pd_canvas_open_document(1216,425,"",0);CHECK(c);
    CHECK(pd_canvas_load(c,"/tmp/mpclearn-pd-touch/library/doc/2.control.examples/04.messages.pd"));
    int count=c->state.count,edges=c->state.edges;CHECK(count==26&&edges==13);
    CHECK(edit(c,at(c,87,86),"* 3"));CHECK(c->state.count==count&&c->state.edges==edges&&!strcmp(c->state.node[c->selected].text,"* 3"));
    CHECK(c->interact);frame(c,at(c,87,55));tap(c,at(c,87,55));CHECK(c->state.node[at(c,87,115)].value==15);
    puts("RECREATE actual upstream [+ 9] -> [* 3]: 26 nodes, all 13 cables retained; existing message5 -> actual atom15");
    CHECK(edit(c,at(c,87,55),"8"));tap(c,at(c,87,55));CHECK(c->state.node[at(c,87,115)].value==24&&c->state.edges==13);
    CHECK(edit(c,at(c,33,21),"Touch edited real patch comment!"));CHECK(!strcmp(c->state.node[at(c,33,21)].text,"Touch edited real patch comment!"));
    CHECK(edit(c,at(c,217,86),"print"));CHECK(c->state.count==26&&c->state.edges==11&&strstr(c->status,"13 -> 11"));
    puts("RECREATE actual [unpack] -> [print]: Pd removed two unavailable outlet cables, retained incoming cable; UI explicitly reports 13 -> 11");
    frame(c,at(c,87,55));CHECK(raster(c,"/work/adapter/text-edit.ppm"));
    pd_canvas_action(c,5);CHECK(!strncmp(c->status,"SAVED",5));pd_canvas_action(c,6);CHECK(c->state.count==26&&c->state.edges==11);
    CHECK(!strcmp(c->state.node[at(c,87,86)].text,"* 3")&&!strcmp(c->state.node[at(c,87,55)].text,"8")&&!strcmp(c->state.node[at(c,33,21)].text,"Touch edited real patch comment!"));
    frame(c,at(c,87,55));tap(c,at(c,87,55));CHECK(c->state.node[at(c,87,115)].value==24);
    /* Repeat on the fresh owner; text cancel has no Pd edit or graph mutation. */
    c->selected=at(c,87,86);pd_canvas_action(c,23);CHECK(c->keyboard==4);pd_canvas_pointer(c,4,0,0);CHECK(!c->keyboard&&c->state.edges==11);
    CHECK(edit(c,at(c,87,86),"+ 2"));tap(c,at(c,87,55));CHECK(c->state.node[at(c,87,115)].value==10&&c->state.edges==11);
    pd_canvas_action(c,5);CHECK(!strncmp(c->status,"SAVED",5));pd_canvas_action(c,6);CHECK(c->state.edges==11);tap(c,at(c,87,55));CHECK(c->state.node[at(c,87,115)].value==10);
    CHECK(edit(c,at(c,87,55),"8, 9"));tap(c,at(c,87,55));CHECK(c->state.node[at(c,87,115)].value==11);
    pd_canvas_action(c,5);CHECK(!strncmp(c->status,"SAVED",5));pd_canvas_action(c,6);CHECK(c->state.edges==11);tap(c,at(c,87,55));CHECK(c->state.node[at(c,87,115)].value==11);
    puts("MESSAGE punctuation keyboard: actual comma-separated 8,9 dispatch -> [+ 2] -> atom11, retained after fresh owner recall");
    CHECK(raster(c,"/work/adapter/text-reload.ppm"));
    /* Extend the real sketch through the same keyboard and pointer path. */
    pd_canvas_action(c,22);CHECK(!c->interact);c->panX=120;c->panY=-10;c->zoom=2;
    pd_canvas_action(c,21);CHECK(c->keyboard==1&&c->createKind==PD_CREATE_OBJECT);
    pd_canvas_pointer(c,1,70,20);pd_canvas_pointer(c,3,70,20);CHECK(c->createKind==PD_CREATE_MESSAGE);
    CHECK(type_value(c,"20, 30"));int message=c->selected;CHECK(c->state.count==27&&c->state.node[message].control==PD_CONTROL_MESSAGE&&!strcmp(c->state.node[message].text,"20, 30"));
    frame(c,message);int x,y,xx,yy;portPosition(c,message,0,true,x,y);portPosition(c,at(c,87,86),0,false,xx,yy);
    pd_canvas_pointer(c,1,x,y);pd_canvas_pointer(c,2,xx,yy);pd_canvas_pointer(c,3,xx,yy);CHECK(c->state.edges==12);
    pd_canvas_action(c,22);tap(c,message);CHECK(c->state.node[at(c,87,115)].value==32);
    pd_canvas_action(c,21);pd_canvas_pointer(c,1,70,20);pd_canvas_pointer(c,1,70,20);CHECK(c->createKind==PD_CREATE_COMMENT);
    CHECK(raster(c,"/work/adapter/create-comment-keyboard.ppm"));CHECK(type_value(c,"New message drives the existing sketch"));CHECK(c->state.count==28&&c->state.node[c->selected].kind==0);
    pd_canvas_action(c,21);pd_canvas_pointer(c,1,70,20);pd_canvas_pointer(c,4,0,0);CHECK(c->state.count==28&&!c->keyboard);
    pd_canvas_action(c,5);CHECK(!strncmp(c->status,"SAVED",5));pd_canvas_action(c,6);CHECK(c->state.count==28&&c->state.edges==12);
    message=at(c,160,50);CHECK(message>=0&&c->state.node[message].control==PD_CONTROL_MESSAGE);frame(c,message);tap(c,message);CHECK(c->state.node[at(c,87,115)].value==32);
    CHECK(raster(c,"/work/adapter/create-reload.ppm"));
    puts("CREATE message/comment via touchscreen keyboard in real upstream sketch: new 20,30 message wired via pointer to existing [+ 2] -> 32; Pd saved and freshly recalled 28 nodes/12 cables; canceled creation unchanged");
    pd_canvas_close(&c);
    puts("PASS real upstream object/message/comment keyboard edits, actual Pd recreate topology and downstream values, explicit port-loss report, canceled edit, writer/fresh reload and repeated editor closure; native host/input substituted; no audio/clock processing");return 0;
}
