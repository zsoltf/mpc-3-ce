#include "pd-canvas.cc"
#include "patch-files.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s: %s\n",__LINE__,#x,c?pd_canvas_status(c):"no canvas");return 1;}}while(0)
int main(){
    auto* c=pd_canvas_open(1216,425);CHECK(c);
    const char* path="/tmp/mpclearn-pd-touch/library/doc/2.control.examples/12.PART2.subpatch.pd";
    PdFiles files={};CHECK(pd_files_open(&files,"/tmp/mpclearn-pd-touch/library/doc/2.control.examples"));
    CHECK(pd_canvas_load(c,path));printf("ROOT %s\n",pd_canvas_status(c));
    int nested=-1,abstraction=-1;
    for(int i=0;i<c->state.count;++i){auto& n=c->state.node[i];printf("NODE %d %d,%d kind%d class=%s text=%s ports=%d/%d child=%d unresolved=%d\n",i,n.x,n.y,n.kind,n.class_name,n.text,n.inputs,n.outputs,n.child,n.unresolved);
        if(!strcmp(n.text,"pd eager-adder"))nested=i;
        if(!strcmp(n.text,"sendnumber 45 cookies"))abstraction=i;}
    CHECK(nested>=0&&abstraction>=0&&!c->state.unresolved);
    int rootCount=c->state.count,rootEdges=c->state.edges;
    FILE* f=fopen("/work/adapter/real-patch.ppm","wb");CHECK(f);fprintf(f,"P6\n%d %d\n255\n",c->width,c->height);
    for(int i=0;i<c->width*c->height;++i)fwrite(c->pixels+4*i,1,3,f);CHECK(!fclose(f));
    c->selected=nested;pd_canvas_action(c,7);CHECK(c->state.depth==1&&!c->state.readonly);printf("CHILD %s\n",pd_canvas_status(c));
    int count=c->state.count,edges=c->state.edges,x=c->state.node[0].x,y=c->state.node[0].y;
    CHECK(pd_touch_move(c->pd,0,x+17,y+11));pd_canvas_action(c,5);CHECK(!strncmp(c->status,"SAVED",5));
    pd_canvas_action(c,6);CHECK(c->state.count==rootCount&&c->state.edges==rootEdges);
    c->selected=nested;pd_canvas_action(c,7);CHECK(c->state.count==count&&c->state.edges==edges&&c->state.node[0].x==x+17&&c->state.node[0].y==y+11);
    pd_canvas_action(c,8);c->selected=abstraction;pd_canvas_action(c,7);CHECK(c->state.readonly&&c->state.count>0&&!c->state.unresolved);
    CHECK(!pd_touch_move(c->pd,0,100,100));printf("ABSTRACTION %s\n",pd_canvas_status(c));
    pd_canvas_close(&c);c=pd_canvas_open_document(1216,425,path,1);CHECK(c&&c->state.count==rootCount&&c->state.edges==rootEdges&&!c->state.unresolved);
    c->selected=nested;pd_canvas_action(c,7);CHECK(c->state.node[0].x==x+17&&c->state.node[0].y==y+11);
    pd_canvas_action(c,21);CHECK(c->keyboard);const char* keys[]={"1234567890","qwertyuiop","asdfghjkl~","zxcvbnm.*-","_/+<>$%!?="};
    for(const char* key="osc~ 330";*key;++key){int xx=20,yy=60+5*((c->height-60)/6)+20;if(*key!=' '){bool found=false;for(int r=0;r<5;++r){const char* match=strchr(keys[r],*key);if(match){xx=(match-keys[r])*(c->width/10)+20;yy=60+r*((c->height-60)/6)+20;found=true;break;}}CHECK(found);}pd_canvas_pointer(c,1,xx,yy);pd_canvas_pointer(c,3,xx,yy);}
    CHECK(!strcmp(c->input,"osc~ 330"));pd_canvas_pointer(c,1,2*(c->width/4)+20,60+5*((c->height-60)/6)+20);CHECK(!c->keyboard&&c->state.count==count+1&&!strcmp(c->state.node[count].text,"osc~ 330"));
    pd_canvas_close(&c);puts("PASS unchanged upstream subpatch sketch: actual objects/ports/cables, embedded move, Pd Save Copy/fresh reload/reopen with original relative abstraction context, read-only abstraction navigation; host UI/input substituted; no audio processing");return 0;
}
