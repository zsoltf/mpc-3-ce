#include "clip-request.h"
static int clip_interest_cli(const char *command_path,const char *mirror_path,const char *seconds_arg,const char *view_arg,const char *column_arg,const char *row_arg){
 unsigned view=0,column=0,row=0;
 if(view_arg){char *tail;errno=0;unsigned long v=strtoul(view_arg,&tail,10);if(errno||!*view_arg||*tail||v>=CLIP_VIEWS)return 2;view=v;
 unsigned long p=strtoul(column_arg,&tail,10);if(errno||!*column_arg||*tail||p>=128)return 2;column=p;
 unsigned long f=strtoul(row_arg,&tail,10);if(errno||!*row_arg||*tail||f>UINT32_MAX-7)return 2;row=f;}
 char *end;errno=0;unsigned long seconds=strtoul(seconds_arg,&end,10);if(errno||*end||seconds>300)return 2;
 int fd=-1,mfd=-1,result=1;struct stat st,mst;CommandState *c=map_file(command_path,sizeof(*c),1,&fd,&st);if(c==MAP_FAILED)return 1;
 const MirrorState *m=map_file(mirror_path,sizeof(*m),0,&mfd,&mst);if(m==MAP_FAILED)goto done;
 if(!header(c)||m->magic!=MIRROR_MAGIC||m->version!=MIRROR_VERSION||m->bytes!=sizeof(*m)||m->pid!=c->pid||m->origin_sec!=c->origin_sec||m->origin_nsec!=c->origin_nsec||flock(fd,LOCK_EX|LOCK_NB))goto unmap;
 uint64_t start=((uint64_t)c->start_hi<<32)|c->start_lo;uint32_t now=command_now(c->origin_sec,c->origin_nsec),ep=atomic_load(&m->epoch);
 if(!start||command_process_start(c->pid)!=start||!command_process_exact(c->pid)||!file_current(command_path,fd,&st)||!file_current(mirror_path,mfd,&mst)||now>UINT32_MAX-seconds*1000||!atomic_load(&m->ready)||!command_writer_enter(c))goto unmap;
 unsigned rev=atomic_load(&c->clip_interest.revision);if((rev&1)||rev>UINT32_MAX-2){command_writer_leave(c);goto unmap;}
 atomic_store(&c->clip_interest.revision,rev+1);atomic_thread_fence(memory_order_release);
 atomic_store(c->clip_interest.column+view,column);atomic_store(c->clip_interest.row+view,row);
 unsigned mask=atomic_load(&c->clip_interest.enabled);atomic_store(&c->clip_interest.enabled,seconds?mask|(1u<<view):mask&~(1u<<view));
 atomic_store(&c->clip_interest.epoch,ep);atomic_store(&c->clip_interest.until,seconds?now+(unsigned)seconds*1000:0);atomic_store_explicit(&c->clip_interest.revision,rev+2,memory_order_release);
 command_writer_leave(c);printf("{\"clip_interest_seconds\":%lu,\"epoch\":%u,\"view\":%u,\"column\":%u,\"native_row\":%u}\n",seconds,ep,view,column,row);result=0;
unmap:munmap((void*)m,sizeof(*m));close(mfd);
done:munmap(c,sizeof(*c));close(fd);return result;
}
static int clip_submit_cli(const char *command_path,const char *mirror_path,const char *view_arg,const char *intent_arg,const char *column_arg,const char *row_arg,const char *source_view_arg,const char *source_column_arg,const char *source_row_arg){
 char *end;errno=0;unsigned long view=strtoul(view_arg,&end,10);if(errno||!*view_arg||*end||view>=CLIP_VIEWS)return 2;
 unsigned long intent=strtoul(intent_arg,&end,10);if(errno||!*intent_arg||*end||intent<CLIP_LAUNCH||intent>CLIP_LAST)return 2;
 unsigned long column=strtoul(column_arg,&end,10);if(errno||!*column_arg||*end||column>=8)return 2;
 unsigned long row=strtoul(row_arg,&end,10);if(errno||!*row_arg||*end||row>=8)return 2;
 int fd=-1,mfd=-1,result=1;struct stat st,mst;CommandState *c=map_file(command_path,sizeof(*c),1,&fd,&st);if(c==MAP_FAILED)return 1;
 const MirrorState *m=map_file(mirror_path,sizeof(*m),0,&mfd,&mst);if(m==MAP_FAILED)goto done;
 if(flock(fd,LOCK_EX|LOCK_NB)||!header(c)||m->magic!=MIRROR_MAGIC||m->version!=MIRROR_VERSION||m->bytes!=sizeof(*m)||m->pid!=c->pid||m->origin_sec!=c->origin_sec||m->origin_nsec!=c->origin_nsec)goto unmap;
 uint64_t start=((uint64_t)c->start_hi<<32)|c->start_lo;
 if(!start||command_process_start(c->pid)!=start||!command_process_exact(c->pid)||!file_current(command_path,fd,&st)||!file_current(mirror_path,mfd,&mst)||!command_idle(c)||command_pending(c))goto unmap;
 MIRROR_COPY(s);uint32_t now;const char *reason=NULL;CommandRequest r;
 if(settlement_snapshot(m,&s,&now,&reason)!=1||!s.clips_available||command_process_start(c->pid)!=start)goto unmap;
 if(source_view_arg){
  unsigned long sv=strtoul(source_view_arg,&end,10);if(errno||!*source_view_arg||*end||sv>=CLIP_VIEWS)goto unmap;
  unsigned long sc=strtoul(source_column_arg,&end,10);if(errno||!*source_column_arg||*end||sc>=8)goto unmap;
  unsigned long sr=strtoul(source_row_arg,&end,10);if(errno||!*source_row_arg||*end||sr>=8)goto unmap;
  ClipEditSource source={0};if(!clip_edit_source(&source,&s.clips,sv,sc,sr,intent==CLIP_COPY_ROW,now)||!clip_copy_request(&r,&s.clips,view,&source,column,row,now))goto unmap;
 }else if(!clip_request_bind(&r,&s.clips,view,intent,column,row,now))goto unmap;
 unsigned seq=atomic_load_explicit(&c->published,memory_order_acquire);if(seq>=COMMAND_SEQUENCE_LAST)goto unmap;
 r.pid=c->pid;r.start_lo=c->start_lo;r.start_hi=c->start_hi;r.origin_sec=c->origin_sec;r.origin_nsec=c->origin_nsec;r.seq=seq+1;
 unsigned at=command_free(c);if(!command_publish_request_at(c,&r,at))goto unmap;
 printf("{\"submitted_clip_intent\":%lu,\"request\":%u,\"pad\":%u,\"before\":%u,\"desired\":%u,\"created\":%u,\"expires\":%u}\n",intent,r.seq,r.pad_index,r.before_bits,r.bits,r.created,r.expires);
 while(!stopped&&atomic_load_explicit(&c->alive,memory_order_acquire)&&atomic_load_explicit(&c->slots[at].processed,memory_order_acquire)!=r.seq){
  uint32_t tick=command_now(c->origin_sec,c->origin_nsec);if(tick==UINT32_MAX||tick<now||tick-now>=10000)break;
  struct timespec delay={0,10000000};nanosleep(&delay,NULL);
 }
 status(c);result=atomic_load_explicit(&c->slots[at].processed,memory_order_acquire)==r.seq?(atomic_load(&c->slots[at].rejected)?1:0):3;
unmap:munmap((void*)m,sizeof(*m));close(mfd);
done:munmap(c,sizeof(*c));close(fd);return result;
}
