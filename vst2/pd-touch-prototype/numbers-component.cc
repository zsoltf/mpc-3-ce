#define main controls_regression_main
#include "controls-component.cc"
#undef main
static int number(PdCanvas* c){for(int i=c->state.count;i<c->state.visible;++i)if(c->state.node[i].owner_parent==1&&c->state.node[i].control==PD_CONTROL_NUMBER)return i;return -1;}
static bool array_is(PdCanvas* c,float first){for(int i=0;i<c->state.visible;++i){const auto& n=c->state.node[i];if(n.array&&n.array_count==3&&n.array_offset>=0)return c->state.sample[n.array_offset]==first&&c->state.sample[n.array_offset+1]==3&&c->state.sample[n.array_offset+2]==4;}return false;}
int main(){auto* c=pd_canvas_open_document(1216,425,"",0);CHECK(c);
    // Focused real-Pd synthetic fixture; not the musical/native acceptance patch.
    const char* path="/tmp/mpclearn-pd-touch/state/numeric-route.pd";
    FILE* f=fopen(path,"wb");CHECK(f);
    CHECK(fputs(
        "#N canvas 0 0 600 400 12;\n"
        "#X msg 250 20 \\; numeric-target range 2 8;\n"
        "#N canvas 0 0 200 200 controls 0;\n"
        "#X obj 10 10 nbx 5 14 0 100 0 0 empty numeric-target empty 0 -8 0 10 -262144 -1 -1 0 256;\n"
        "#X obj 10 160 outlet;\n"
        "#X msg 10 130 \\; numeric-array 0 \\$1 3 4;\n"
        "#X connect 0 0 1 0;\n#X connect 0 0 2 0;\n"
        "#X coords 0 -1 1 1 140 60 1 0 0;\n"
        "#X restore 50 80 pd controls;\n"
        "#X floatatom 50 210 5 0 0 0 - - - 0;\n"
        "#N canvas 0 0 300 200 graph 0;\n#X array numeric-array 3 float 3;\n#A 0 0 0 0;\n"
        "#X coords 0 100 2 0 140 60 1 0 0;\n#X restore 250 80 graph;\n"
        "#X msg 250 230 \\; pd-numeric-route.pd menuclose 1;\n"
        "#X msg 250 260 \\; numeric-target range 4 12;\n"
        "#X connect 1 0 2 0;\n",f)>=0&&!fclose(f));
    CHECK(pd_canvas_load(c,path));int n=number(c);CHECK(n>=0&&c->state.count==6);
    static PdTouchSnapshot route,fresh,before;route=c->state;
    // Change the live bounds without refreshing the copied route.
    CHECK(pd_touch_activate(c->pd,0));CHECK(c->state.node[n].maximum==100);
    CHECK(pd_touch_number_from_snapshot(c->pd,&route,n,90));read(c,"NUMBER");
    CHECK(c->state.node[n].value==8&&c->state.node[n].minimum==2&&c->state.node[n].maximum==8);
    CHECK(c->state.node[2].value==8&&array_is(c,8));
    CHECK(pd_touch_snapshot(c->pd,&fresh)&&!memcmp(&fresh,&c->state,sizeof(fresh)));
    // Q-Link must use its refreshed bounds even when the value stayed unchanged.
    c->selected=n;CHECK(pd_touch_activate(c->pd,5));MpcEditorParameters parameters{};
    CHECK(pd_canvas_parameters(c,&parameters)&&parameters.count==1);
    CHECK(pd_canvas_parameter(c,parameters.binding,0,MPC_EDITOR_PARAMETER_SET,.25f));
    CHECK(c->state.node[n].value==6&&c->state.node[2].value==6&&array_is(c,6));
    CHECK(pd_touch_snapshot(c->pd,&fresh)&&!memcmp(&fresh,&c->state,sizeof(fresh)));
    // Touch and text use the same route; the full fresh view includes downstream arrays.
    frame(c,n);int x,y;center(c,n,x,y);pd_canvas_pointer(c,1,x,y);pd_canvas_pointer(c,2,x,y-8);pd_canvas_pointer(c,3,x,y-8);
    CHECK(c->state.node[n].value==10&&c->state.node[2].value==10&&array_is(c,10));
    tap(c,n);CHECK(type_value(c,"5")&&c->state.node[n].value==5&&c->state.node[2].value==5&&array_is(c,5));
    CHECK(pd_touch_snapshot(c->pd,&fresh)&&!memcmp(&fresh,&c->state,sizeof(fresh)));
    before=c->state;route=c->state;
    CHECK(!pd_touch_number_from_snapshot(c->pd,&route,n,NAN));
    CHECK(!pd_touch_number_from_snapshot(c->pd,&route,n,INFINITY));
    route.node[n].control=PD_CONTROL_SLIDER;CHECK(!pd_touch_number_from_snapshot(c->pd,&route,n,7));
    route=c->state;snprintf(route.node[n].class_name,sizeof(route.node[n].class_name),"gatom");CHECK(!pd_touch_number_from_snapshot(c->pd,&route,n,7));
    route=c->state;++route.node[1].identity;CHECK(!pd_touch_number_from_snapshot(c->pd,&route,n,7));
    CHECK(pd_touch_snapshot(c->pd,&fresh)&&!memcmp(&before,&fresh,sizeof(fresh)));
    // Real structural change shifts the stored route; never fall back to its index.
    route=c->state;CHECK(pd_touch_delete(c->pd,0));CHECK(pd_touch_snapshot(c->pd,&before));
    CHECK(!pd_touch_number_from_snapshot(c->pd,&route,n,7));CHECK(pd_touch_snapshot(c->pd,&fresh)&&!memcmp(&before,&fresh,sizeof(fresh)));
    // Actual owner closure is checked before following any retained route pointer.
    CHECK(pd_canvas_load(c,path));n=number(c);CHECK(n>=0);route=c->state;
    CHECK(pd_touch_activate(c->pd,4));CHECK(!pd_touch_number_from_snapshot(c->pd,&route,n,7));
    read(c,"CLOSED");PdTouchSnapshot empty{};CHECK(!memcmp(&c->state,&empty,sizeof(empty))&&strstr(c->status,"Patch closed by Pd"));
    CHECK(pd_canvas_load(c,path));n=number(c);CHECK(n>=0&&pd_touch_number(c->pd,n,9));read(c,"INDEX API");
    CHECK(c->state.node[n].value==9&&c->state.node[2].value==9&&array_is(c,9));
    pd_canvas_close(&c);puts("PASS real Pd numeric route: live bounds, GOP downstream/array freshness, touch/text/Q-Link, nonfinite/type/stale route refusal, actual closure and index API; synthetic fixture, no native timing claim");return 0;
}
