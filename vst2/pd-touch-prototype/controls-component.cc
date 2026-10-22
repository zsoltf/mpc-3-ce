#include "pd-canvas.cc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s: %s\n",__LINE__,#x,c?pd_canvas_status(c):"no canvas");return 1;}}while(0)
static void frame(PdCanvas* c,int node){c->zoom=2;c->panX=c->state.node[node].x-80;c->panY=c->state.node[node].y-70;draw(c,"CONTROL VIEW");}
static void center(PdCanvas* c,int node,int& x,int& y){position(c,node,x,y);x+=nodeWidth(c,node)/2;y+=nodeHeight(c,node)/2;}
static void tap(PdCanvas* c,int node){int x,y;center(c,node,x,y);pd_canvas_pointer(c,1,x,y);pd_canvas_pointer(c,3,x,y);}
static bool type_value(PdCanvas* c,const char* value){if(!c->keyboard)return false;int h=(c->height-60)/6,buttonY=60+5*h+20;
    size_t old=strlen(c->input);for(size_t i=0;i<old;++i){pd_canvas_pointer(c,1,c->width/4+20,buttonY);pd_canvas_pointer(c,3,c->width/4+20,buttonY);}
    for(const char* key=value;*key;++key){int x=20,y=buttonY,page=c->keypage;bool found=*key==' ';for(int p=0;p<3&&!found;++p)for(int r=0;r<5&&!found;++r)for(int k=0;k<10&&!found;++k)if(keyboardKey(p,r,k)==*key){x=k*(c->width/10)+20;y=60+r*h+20;page=p;found=true;}if(!found)return false;
        while(c->keypage!=page){pd_canvas_pointer(c,1,c->width-60,20);pd_canvas_pointer(c,3,c->width-60,20);}pd_canvas_pointer(c,1,x,y);pd_canvas_pointer(c,3,x,y);}
    pd_canvas_pointer(c,1,2*(c->width/4)+20,buttonY);pd_canvas_pointer(c,3,2*(c->width/4)+20,buttonY);return !c->keyboard;}
static bool raster(PdCanvas* c,const char* path){uint32_t revision;const auto* pixels=pd_canvas_pixels(c,&revision);if(!pixels)return false;FILE* f=fopen(path,"wb");if(!f)return false;fprintf(f,"P6\n%d %d\n255\n",c->width,c->height);for(int i=0;i<c->width*c->height;++i)fwrite(pixels+4*i,1,3,f);return !fclose(f);}
int main(){auto* c=pd_canvas_open_document(1216,425,"",0);CHECK(c);
    CHECK(pd_canvas_load(c,"/tmp/mpclearn-pd-touch/library/doc/2.control.examples/04.messages.pd"));CHECK(c->interact);
    frame(c,3);tap(c,3);CHECK(c->state.node[17].value==14);printf("MESSAGE 5 -> actual [+ 9] -> atom=%g\n",c->state.node[17].value);
    tap(c,9);CHECK(c->state.node[16].value==11);tap(c,1);CHECK(c->state.node[15].value==5&&c->state.node[0].value==6);
    CHECK(raster(c,"/work/adapter/controls-messages.ppm"));
    CHECK(pd_canvas_load(c,"/tmp/mpclearn-pd-touch/library/doc/2.control.examples/05.counter.pd"));frame(c,4);
    tap(c,4);tap(c,4);tap(c,4);CHECK(c->state.node[19].value==3);printf("MESSAGE counter: actual atom=%g\n",c->state.node[19].value);
    frame(c,3);int x,y;center(c,3,x,y);pd_canvas_pointer(c,1,x,y);pd_canvas_pointer(c,2,x,y-14);pd_canvas_pointer(c,3,x,y-14);CHECK(c->state.node[3].value==7);
    tap(c,20);CHECK(c->state.node[17].value==7);
    for(int repeat=0;repeat<2;++repeat){center(c,3,x,y);pd_canvas_pointer(c,1,x,y);pd_canvas_pointer(c,2,x,y-8);pd_canvas_pointer(c,3,x,y-8);CHECK(c->state.node[3].value==11+repeat*4&&c->state.node[17].value==7+repeat*4);tap(c,20);CHECK(c->state.node[17].value==11+repeat*4);}
    printf("NUMBER drag -> cold inlet, BANG three times -> actual atoms 7,11,%g\n",c->state.node[17].value);
    center(c,3,x,y);pd_canvas_pointer(c,1,x,y);pd_canvas_pointer(c,2,x,y-20);CHECK(c->state.node[3].value==25&&c->state.node[17].value==15);
    pd_canvas_pointer(c,4,x,y-20);pd_canvas_pointer(c,3,x,y-20);CHECK(c->state.node[3].value==25&&c->state.node[17].value==15);
    tap(c,3);CHECK(type_value(c,"22")&&c->state.node[3].value==22&&c->state.node[17].value==15);tap(c,20);CHECK(c->state.node[17].value==22);
    CHECK(raster(c,"/work/adapter/controls-counter.ppm"));pd_canvas_action(c,5);CHECK(!strncmp(c->status,"SAVED",5));pd_canvas_action(c,6);CHECK(c->state.count==31&&c->state.node[3].value==0); /* Pd atoms do not save runtime values. */
    CHECK(pd_canvas_load(c,"/tmp/mpclearn-pd-touch/library/doc/2.control.examples/12.PART2.subpatch.pd"));frame(c,17);tap(c,17);CHECK(c->state.node[8].value==67);
    frame(c,16);tap(c,16);CHECK(c->state.node[0].value==45);printf("BANG -> actual relative sendnumber abstractions -> cookies=%g pretzels=%g\n",c->state.node[0].value,c->state.node[8].value);
    pd_canvas_action(c,9);CHECK(raster(c,"/work/adapter/controls-subpatch.ppm"));
    CHECK(pd_canvas_load(c,"/tmp/mpclearn-pd-touch/library/doc/2.control.examples/14.dollarsigns.pd"));c->selected=0;pd_canvas_action(c,7);CHECK(c->state.readonly);frame(c,34);tap(c,34);CHECK(!strcmp(c->state.node[9].text,"three"));
    frame(c,9);tap(c,9);CHECK(type_value(c,"hello")&&!strcmp(c->state.node[9].text,"hello"));
    frame(c,27);tap(c,27);CHECK(type_value(c,"7 8 hello")&&!strcmp(c->state.node[27].text,"7 8 hello"));
    puts("SYMBOL/LIST touchscreen Apply -> real abstraction atoms hello / 7 8 hello; no structural abstraction write");
    CHECK(raster(c,"/work/adapter/controls-symbol-list.ppm"));
    /* Separate synthetic lifetime regression, not the real-sketch acceptance. */
    FILE* closing=fopen("/tmp/mpclearn-pd-touch/state/self-close.pd","wb");CHECK(closing);
    CHECK(fputs("#N canvas 0 0 400 300 12;\n#X msg 50 50 \\; pd-self-close.pd menuclose 1;\n",closing)>=0&&!fclose(closing));
    CHECK(pd_canvas_load(c,"/tmp/mpclearn-pd-touch/state/self-close.pd"));frame(c,0);tap(c,0);CHECK(c->state.count==0&&strstr(c->status,"Patch closed by Pd"));
    // A failed fresh read can have partially filled the direct destination.
    // No stale graph, samples, selection or contact may survive publication.
    PdTouchSnapshot cleared{};CHECK(!memcmp(&c->state,&cleared,sizeof(cleared))&&c->selected==-1&&!c->contact);
    pd_canvas_action(c,5);CHECK(strstr(c->status,"Patch closed by Pd"));CHECK(pd_canvas_load(c,"/tmp/mpclearn-pd-touch/library/doc/2.control.examples/04.messages.pd"));frame(c,3);tap(c,3);CHECK(c->state.node[17].value==14);
    puts("PASS separate actual Pd self-close message: cleared stale view, save refused, replacement sketch controls work");
    pd_canvas_close(&c);puts("PASS upstream real controls: canonical message clicks, held live atoms/cold inlet, repeated bng clicks, cancel preserves already delivered values, relative abstraction signal flow, Pd-owned save/reload; viewport setup and native delivery substituted; no audio/clock processing");return 0;
}
