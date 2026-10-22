#include "pd-canvas.cc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char** argv){auto* c=pd_canvas_open(1216,480);CHECK(c&&c->state.count==2);
    c->selected=1;pd_canvas_action(c,4);c->selected=0;pd_canvas_action(c,4);CHECK(c->state.count==0);
    pd_canvas_action(c,1);pd_canvas_action(c,2);CHECK(c->state.count==2);
    pd_canvas_pointer(c,1,100,100);pd_canvas_pointer(c,2,180,200);pd_canvas_pointer(c,4,180,200);
    CHECK(c->state.node[0].x==20&&c->state.node[0].y==24);pd_canvas_pointer(c,3,180,200);CHECK(c->state.node[0].x==20);
    pd_canvas_pointer(c,1,100,100);pd_canvas_pointer(c,2,180,200);pd_canvas_pointer(c,3,180,200);
    CHECK(c->state.node[0].x==100&&c->state.node[0].y==124);
    pd_canvas_pointer(c,1,132,262);pd_canvas_pointer(c,2,342,36);pd_canvas_pointer(c,3,342,36);CHECK(c->state.edges==1);
    pd_canvas_action(c,5);pd_canvas_action(c,6);CHECK(c->state.edges==1&&c->state.node[0].x==100);
    // Native regression: a captured drag outside is MOVE/UP, not toolkit CANCEL.
    pd_canvas_pointer(c,1,180,200);pd_canvas_pointer(c,2,1247,479);pd_canvas_pointer(c,3,1247,479);
    printf("outside drag owner=%d,%d cables=%d\n",c->state.node[0].x,c->state.node[0].y,c->state.edges);
    CHECK(c->state.node[0].x==100&&c->state.node[0].y==124&&c->state.edges==1);
    const float outside[][2]={{-1,200},{1218,200},{180,-1},{180,482}};
    for(auto& edge:outside){
        // Preview, leave, reenter, release: cancellation stays latched until a new DOWN.
        pd_canvas_pointer(c,1,180,200);pd_canvas_pointer(c,2,220,240);pd_canvas_pointer(c,2,edge[0],edge[1]);
        CHECK(!c->contact&&!strncmp(c->status,"EDGE CANCEL",11));uint32_t revision=c->revision;
        pd_canvas_pointer(c,2,250,230);pd_canvas_pointer(c,3,250,230);CHECK(c->revision==revision);
        CHECK(c->state.node[0].x==100&&c->state.node[0].y==124);
        // An outside UP must also cancel if no preceding outside MOVE arrived.
        pd_canvas_pointer(c,1,180,200);pd_canvas_pointer(c,3,edge[0],edge[1]);CHECK(!c->contact);
        CHECK(c->state.node[0].x==100&&c->state.node[0].y==124);
        // A distinct new cable would be sink OUT -> oscillator IN, not a duplicate.
        pd_canvas_pointer(c,1,342,162);pd_canvas_pointer(c,2,edge[0],edge[1]);
        CHECK(!c->contact);revision=c->revision;
        pd_canvas_pointer(c,2,132,136);pd_canvas_pointer(c,3,132,136);CHECK(c->revision==revision&&c->state.edges==1);
    }
    // Both the unchanged Pd writer output and fresh recall must retain P1.
    pd_canvas_action(c,5);pd_canvas_action(c,6);CHECK(c->state.node[0].x==100&&c->state.node[0].y==124&&c->state.edges==1);
    FILE* f=fopen(argc==2?argv[1]:"/work/adapter/canvas.ppm","wb");CHECK(f);fprintf(f,"P6\n%d %d\n255\n",c->width,c->height);
    for(int i=0;i<c->width*c->height;++i)fwrite(c->pixels+i*4,1,3,f);CHECK(!fclose(f));
    c->selected=0;pd_canvas_action(c,3);CHECK(c->state.edges==0);pd_canvas_action(c,4);CHECK(c->state.count==1);
    pd_canvas_close(&c);CHECK(!c);puts("PASS real Pd touchscreen-intent journey: preview/cancel/four-edge cancel/stale up/move/wire/save/fresh reload/unwire/delete; native delivery substituted");return 0;}
