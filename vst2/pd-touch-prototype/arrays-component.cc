#define main controls_regression_main
#include "controls-component.cc"
#undef main
static int array(PdCanvas* c,const char* name){for(int i=0;i<c->state.visible;++i)if(c->state.node[i].array&&!strcmp(c->state.node[i].text,name))return i;return -1;}
static float sample(PdCanvas* c,int n,int i){return c->state.sample[c->state.node[n].array_offset+i];}
static void point(PdCanvas* c,int n,float ix,float v,int& x,int& y){auto& a=c->state.node[n];position(c,n,x,y);x+=(ix-a.array_x1)*nodeWidth(c,n)/(a.array_x2-a.array_x1);y+=(v-a.array_y1)*nodeHeight(c,n)/(a.array_y2-a.array_y1);}
static void frameArray(PdCanvas* c,int n){c->zoom=1.5;c->panX=c->state.node[n].x-30;c->panY=c->state.node[n].y-35;draw(c,"ARRAY TEST VIEWPORT");}
static int copyOwner(const char* path){FILE* in=fopen("/tmp/mpclearn-pd-touch/state/prototype.pd","rb"),*out=fopen(path,"wb");if(!in||!out)return 0;char bytes[4096];size_t n;while((n=fread(bytes,1,sizeof(bytes),in)))if(fwrite(bytes,1,n,out)!=n)return 0;int ok=!ferror(in)&&!ferror(out);fclose(in);fclose(out);return ok;}
int main(){auto* c=pd_canvas_open_document(1216,425,"",0);CHECK(c);
    CHECK(pd_canvas_load(c,"/tmp/mpclearn-pd-touch/library/doc/3.audio.examples/B02.two-wavetables.pd"));CHECK(c->interact);
    int w=array(c,"waveform11"),p=array(c,"pitch11");CHECK(w>=0&&p>=0);CHECK(c->state.node[w].array_count==259&&c->state.node[p].array_count==259&&c->state.node[w].array_save&&c->state.node[p].array_save);
    printf("B02 real arrays: waveform domain %g..%g, pitch %g..%g; counts259/259, save1/1, edit%d/%d\n",c->state.node[w].array_y1,c->state.node[w].array_y2,c->state.node[p].array_y1,c->state.node[p].array_y2,c->state.node[w].array_edit,c->state.node[p].array_edit);
    frameArray(c,w);CHECK(raster(c,"/work/adapter/arrays-before.ppm"));float old=sample(c,w,80);int x,y,xx,yy;point(c,w,80,.6,x,y);point(c,w,100,-.6,xx,yy);
    pd_canvas_pointer(c,1,x,y);CHECK(c->contact==8);pd_canvas_pointer(c,2,xx,yy);CHECK(sample(c,w,80)==old);pd_canvas_pointer(c,4,xx,yy);pd_canvas_pointer(c,3,xx,yy);CHECK(sample(c,w,80)==old);
    pd_canvas_pointer(c,1,x,y);pd_canvas_pointer(c,2,c->width+10,y);pd_canvas_pointer(c,2,xx,yy);pd_canvas_pointer(c,3,xx,yy);CHECK(sample(c,w,80)==old);
    pd_canvas_pointer(c,1,x,y);pd_canvas_pointer(c,2,x-400,y);pd_canvas_pointer(c,3,xx,yy);CHECK(sample(c,w,80)==old);
    pd_canvas_pointer(c,1,x,y);pd_canvas_pointer(c,2,xx,yy);pd_canvas_pointer(c,3,xx,yy);CHECK(sample(c,w,85)>.1&&sample(c,w,95)<-.1);float a=sample(c,w,85),b=sample(c,w,95);CHECK(raster(c,"/work/adapter/arrays-waveform.ppm"));
    frameArray(c,p);point(c,p,80,750,x,y);point(c,p,100,250,xx,yy);pd_canvas_pointer(c,1,x,y);CHECK(c->contact==8);pd_canvas_pointer(c,2,xx,yy);pd_canvas_pointer(c,3,xx,yy);float pitch=sample(c,p,85);CHECK(pitch>500&&pitch<750);CHECK(raster(c,"/work/adapter/arrays-pitch.ppm"));
    printf("B02 real touch stroke: waveform[85]=%g [95]=%g pitch[85]=%g; CANCEL/edge/stale UP unchanged before commit\n",a,b,pitch);
    float invalid=2;CHECK(!pd_touch_array(c->pd,w,c->state.revision-1,85,1,&invalid));CHECK(!pd_touch_array(c->pd,w,c->state.revision,259,1,&invalid));CHECK(!pd_touch_array(c->pd,w,c->state.revision,-1,1,&invalid));read(c,"VALIDATION");CHECK(sample(c,w,85)==a);puts("STALE revision and invalid ranges refused without entering libpd array API; next owner snapshot remains available");
    pd_canvas_action(c,5);CHECK(!strncmp(c->status,"SAVED",5));CHECK(copyOwner("/work/adapter/arrays-B02.pd"));pd_canvas_action(c,6);w=array(c,"waveform11");p=array(c,"pitch11");CHECK(w>=0&&p>=0&&fabs(sample(c,w,85)-a)<1e-5&&fabs(sample(c,w,95)-b)<1e-5&&fabs(sample(c,p,85)-pitch)<.001);frameArray(c,p);CHECK(raster(c,"/work/adapter/arrays-reload.ppm"));
    puts("B02 Pd Save Copy/fresh owner Reload: both changed sample domains persist; original source remains untouched");
    CHECK(pd_canvas_load(c,"/tmp/mpclearn-pd-touch/library/doc/3.audio.examples/B07.sampler.scratch.pd"));int s=array(c,"sample-table");CHECK(s>=0&&c->state.node[s].array_count==48003&&!c->state.node[s].array_save);frameArray(c,s);point(c,s,24000,.7,x,y);pd_canvas_pointer(c,1,x,y);CHECK(c->contact==8);pd_canvas_pointer(c,3,x,y);int changed=c->arrayPrevious;float value=sample(c,s,changed);CHECK(value>.6);pd_canvas_action(c,5);CHECK(!strncmp(c->status,"SAVED",5));CHECK(copyOwner("/work/adapter/arrays-B07.pd"));pd_canvas_action(c,6);s=array(c,"sample-table");CHECK(s>=0&&!c->state.node[s].array_save&&sample(c,s,changed)!=value);printf("B07 non-save table: edited[%d]=%g, fresh owner startup value=%g; save flag remains0, sample edit not persisted\n",changed,value,sample(c,s,changed));CHECK(raster(c,"/work/adapter/arrays-nonpersistent.ppm"));
    pd_canvas_close(&c);puts("PASS actual pinned Pd arrays and touch preview/list dispatch/save/recall; ARM Docker, native host/input substituted, DSP/clock processing absent");return 0;
}
