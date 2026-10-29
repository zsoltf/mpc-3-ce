/* Real pad-prepare CLI/file/request path and real pad membership projection.
 * Process identity, clock and top-level Track snapshot are substituted. */
#define _GNU_SOURCE
#define COMMAND_CLIENT
#include "mirror-motor-core.h"
#include "mirror-input-core.h"
#include "command-process.h"
static uint32_t selected=7;
static int exact_ok=1;
static uint64_t pad_test_start(unsigned pid){(void)pid;return 1;}
static int pad_test_exact(unsigned pid){(void)pid;return exact_ok;}
static int pad_test_file(unsigned pid,int full){(void)pid;(void)full;return 1;}
static int pad_test_clock(clockid_t id,struct timespec *t){(void)id;t->tv_sec=11;t->tv_nsec=0;return 0;}
static unsigned settlement_bound;
static void pad_test_pump(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *s,uint32_t now){
 (void)bank;(void)s;(void)now;const InputFlight *f=in->flights;
 settlement_bound=f->target.pad_owner==1&&f->target.pad_generation==3&&f->property==0xa014&&f->field_incarnation==PF_COUNT+PF_MUTE;
 /* Inspect the actual CLI's reconstructed flight, then stop before any
  * receipt settlement. This substitute cannot certify a native command. */
 in->error=C_IDENTITY;
}
static int pad_test_copy(const MirrorState *m,CopiedMirror *s){
 memset(s,0,sizeof(*s));s->ready=s->alive=1;s->heartbeat=1000;s->epoch=4;s->project_owner=9;
 s->selected_serial=selected;s->pad_source=&m->pads;s->count=2;
 s->tracks[0]=(CopiedTrack){.serial=6,.vptr=MODEL_MIDIPROGRAM_VPTR};
 s->tracks[1]=(CopiedTrack){.serial=7,.track=0x8000,.program=0x9000,.vptr=MODEL_DRUMPROGRAM_VPTR,
  .binding=11,.incarnation=12,.track_owner=13,.program_owner=14};return 1;
}
#define command_process_start pad_test_start
#define command_process_exact pad_test_exact
#define command_process_file pad_test_file
#define clock_gettime pad_test_clock
#define copy_mirror pad_test_copy
#define input_pump pad_test_pump
#define main pad_client_main
#include "command-client.c"
#undef main
#undef copy_mirror
#undef command_process_start
#undef command_process_exact
#undef command_process_file
#undef input_pump
static void need(int ok,const char *why){if(!ok){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
int main(int argc,char **argv){
 (void)command_process_start;(void)command_process_exact;(void)command_process_file;(void)copy_mirror;(void)input_pump;
 if(argc!=3)return 2;
 int fd=open(argv[1],O_CREAT|O_EXCL|O_RDWR,0600);need(fd>=0&&!ftruncate(fd,sizeof(MirrorState)),"exclusive fixture");
 MirrorState *m=mmap(NULL,sizeof(*m),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);need(m!=MAP_FAILED,"fixture mapping");
 m->magic=MIRROR_MAGIC;m->version=MIRROR_VERSION;m->bytes=sizeof(*m);m->capacity=MIRROR_CELLS;m->pid=1;m->origin_sec=10;
 PadParent *p=m->pads.parents;unsigned h=((0x9000u>>4)*2654435761u)&(PAD_PARENT_HASH-1);
 atomic_store(m->pads.parent_directory+h,1);atomic_store(&p->address,0x9000);atomic_store(&p->owner,14);atomic_store(&p->live,1);atomic_store(&p->revision,2);
 atomic_store(&p->slots[0].owner,1);atomic_store(&p->slots[0].generation,3);
 PadOwner *o=m->pads.owners;atomic_store(&o->incarnation,1);atomic_store(&o->live,1);atomic_store(&o->complete,1);atomic_store(&o->address,0xa000);atomic_store(&o->parent,0x9000);atomic_store(&o->parent_owner,14);
 for(unsigned f=PF_MUTE;f<=PF_SOLO_AUDIO;f++){atomic_store(&o->fields[f].live,1);atomic_store(&o->fields[f].seed,1);atomic_store(&o->fields[f].revision,2);}
 char *args[]={"command-client","pad-prepare",argv[1],argv[2],"0","mute","1"};
 need(!pad_client_main(7,args),"actual CLI accepts selected Drum pad");
 CommandRequest r;int request=open(argv[2],O_RDONLY);need(request>=0&&read(request,&r,sizeof(r))==sizeof(r),"request file readback");close(request);
 need(r.pad_owner==1&&r.pad_index==0&&r.pad_generation==3&&r.serial==7&&r.program_owner==14&&r.track_owner==13&&r.binding==11&&r.incarnation==12&&r.field_incarnation==PF_COUNT+PF_MUTE&&r.reserved==CF_MUTE&&r.bits==1&&!r.before_bits&&r.before_revision==2&&r.expires==121000,"typed native pad identity and desired mute");
 need(pad_client_main(7,args)==1,"existing request never overwritten");unlink(argv[2]);
 selected=6;need(pad_client_main(7,args)==1&&access(argv[2],F_OK)<0,"selected MIDI cannot silently target other Drum");selected=7;
 atomic_store(&p->slots[0].generation,0);need(pad_client_main(7,args)==1,"unavailable membership rejected");atomic_store(&p->slots[0].generation,3);
 exact_ok=0;need(pad_client_main(7,args)==1,"process admission retained");exact_ok=1;
 args[4]="128";need(pad_client_main(7,args)==2,"pad range rejected");args[4]="0";args[5]="volume";need(pad_client_main(7,args)==2,"unsupported field rejected");args[5]="solo";args[6]="0.5";need(pad_client_main(7,args)==2,"nonboolean rejected");args[6]="1";
 need(!pad_client_main(7,args),"native solo request shares typed path");unlink(argv[2]);
 int cfd=open(argv[2],O_CREAT|O_EXCL|O_RDWR,0600);need(cfd>=0&&!ftruncate(cfd,sizeof(CommandState)),"command bootstrap fixture");
 CommandState *c=mmap(NULL,sizeof(*c),PROT_READ|PROT_WRITE,MAP_SHARED,cfd,0);need(c!=MAP_FAILED,"command map");
 c->magic=COMMAND_MAGIC;c->version=COMMAND_VERSION;c->bytes=sizeof(*c);c->capacity=COMMAND_SLOTS;c->pid=1;c->start_lo=1;c->origin_sec=10;c->owner_token=1;atomic_store(&c->alive,1);
 r.seq=1;need(command_publish_request_at(c,&r,0),"typed mute publication");
 /* Retire submitted Instrument membership: settlement must bootstrap from
  * its retained receipt scalar, not the parent Track or replacement pad. */
 atomic_store(&p->slots[0].owner,0);atomic_store(&p->slots[0].generation,4);atomic_store(&o->live,0);atomic_store(&m->heartbeat,1000);atomic_store(&m->alive,1);
 need(settle(argv[2],argv[1])==1&&settlement_bound&&!atomic_load(&c->slots[0].settled),"actual settlement bootstrap retains requested Instrument after replacement; no fake acknowledgment");
 munmap(c,sizeof(*c));close(cfd);unlink(argv[2]);
 munmap(m,sizeof(*m));close(fd);unlink(argv[1]);
 puts("PASS pad CLI/file path and retained Instrument settlement bootstrap; process/clock/Track snapshot and receipt pump substituted, native setter and settlement unobserved");return 0;
}
