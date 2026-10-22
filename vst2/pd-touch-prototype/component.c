#include "pd-touch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line%d %s: %s\n",__LINE__,#x,pd_touch_error(p));exit(1);}}while(0)
int main(void) {
    PdTouch* p=pd_touch_open();CHECK(p);PdTouchSnapshot s;
    CHECK(pd_touch_snapshot(p,&s)&&s.count==0);
    CHECK(pd_touch_add(p,0,40,70));CHECK(pd_touch_add(p,1,360,70));
    CHECK(pd_touch_snapshot(p,&s)&&s.count==2&&!strcmp(s.node[0].text,"osc~ 220")&&!strcmp(s.node[1].text,"*~ 0.1"));
    CHECK(s.node[0].outputs==1&&s.node[1].inputs==2);
    CHECK(pd_touch_move(p,0,70,140));CHECK(pd_touch_connect(p,0,0,1,0));
    CHECK(!pd_touch_connect(p,-1,0,1,0));CHECK(!pd_touch_connect(p,0,12,1,0));
    CHECK(!(s.node[0].signal_inputs&2));CHECK(!pd_touch_connect(p,1,0,0,1));
    CHECK(pd_touch_save(p));CHECK(pd_touch_reload(p));
    CHECK(pd_touch_snapshot(p,&s)&&s.count==2&&s.edges==1&&s.node[0].x==70&&s.node[0].y==140);
    CHECK(s.edge[0].source==0&&s.edge[0].target==1);
    CHECK(pd_touch_unwire(p,0));CHECK(pd_touch_snapshot(p,&s)&&s.edges==0);
    CHECK(pd_touch_connect(p,0,0,1,0));CHECK(pd_touch_delete(p,0));
    CHECK(pd_touch_snapshot(p,&s)&&s.count==1&&s.edges==0&&!strcmp(s.node[0].text,"*~ 0.1"));
    /* Keep actual engine output byte-for-byte; use a separate malformed fixture. */
    const char* path="/tmp/mpclearn-pd-touch/state/prototype.pd";
    const char* original="/tmp/mpclearn-pd-touch/state/component-original.pd";
    CHECK(!rename(path,original));FILE* fixture=fopen(path,"wb");CHECK(fixture);
    CHECK(fputs("#N canvas 0 0 400 300 10;\n#X obj 10 10 missing_touch_object;\n",fixture)>=0&&fclose(fixture)==0);
    CHECK(pd_touch_reload(p));CHECK(pd_touch_snapshot(p,&s)&&s.count==1&&s.edges==0&&s.unresolved==1);
    CHECK(!pd_touch_save(p)); /* Actual missing class remains visible, never silently saved away. */
    CHECK(!unlink(path)&&!rename(original,path));
    pd_touch_close(&p);CHECK(!p);p=pd_touch_open();CHECK(p);
    CHECK(pd_touch_snapshot(p,&s)&&s.count==2&&s.edges==1);pd_touch_close(&p);
    puts("PASS actual Pd create/move/live ports/connect/unwire/delete/save/fresh instance reload/reopen; DSP stopped; touchscreen/rendering not exercised");return 0;
}
