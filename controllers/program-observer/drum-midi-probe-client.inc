#ifdef MODEL_MIDI_PROBE
static int drum_midi_watch(const char *path,const char *duration){
 char *end=NULL;unsigned long seconds=strtoul(duration,&end,10);
 if(!*duration||*end||seconds<1||seconds>30)return 2;
 int fd=-1,result=1;struct stat st;CommandState *s=map_file(path,sizeof(*s),1,&fd,&st);if(s==MAP_FAILED){fputs("MIDI probe requires matching diagnostic CMD3201 state size and owner\n",stderr);return 1;}
 uint32_t now=command_now(s->origin_sec,s->origin_nsec);uint64_t start=((uint64_t)s->start_hi<<32)|s->start_lo;
 if(!header(s)||!start||command_process_start(s->pid)!=start||!command_process_exact(s->pid)||!file_current(path,fd,&st)||now>UINT32_MAX-(uint32_t)seconds*1000u||!command_writer_enter(s))goto done;
 if(!atomic_load(&s->drum_midi.started)&&!atomic_load(&s->closed)&&atomic_load(&s->alive)){
  atomic_store(&s->drum_midi.started,1);atomic_store_explicit(&s->drum_midi.until,now+(uint32_t)seconds*1000u,memory_order_release);result=0;
 }
 command_writer_leave(s);
 if(!result)printf("Read-only MIDI diagnostic: %lu seconds, 1024 records maximum; one capture per observer lifetime; enrolled threads only.\n",seconds);
 done:munmap(s,sizeof(*s));close(fd);return result;
}
static int drum_midi_end(const char *path){
 int fd=-1,result=1;struct stat st;CommandState *s=map_file(path,sizeof(*s),1,&fd,&st);if(s==MAP_FAILED){fputs("MIDI probe requires matching diagnostic CMD3201 state size and owner\n",stderr);return 1;}
 uint64_t start=((uint64_t)s->start_hi<<32)|s->start_lo;
 if(header(s)&&start&&command_process_start(s->pid)==start&&command_process_exact(s->pid)&&file_current(path,fd,&st)&&command_writer_enter(s)){
  atomic_store(&s->drum_midi.until,0);command_writer_leave(s);result=0;
 }
 munmap(s,sizeof(*s));close(fd);return result;
}
static const char *drum_midi_site(unsigned site){
 static const char *names[]={"root","root_end","map","map_end","send","send_end","on_bytes","on_sink","off_bytes","off_sink","pending","due","due_end","off_root","off_end","mute","drain","drain_end","cleanup","cleanup_end"};
 return site>=DMP_ROOT&&site<=DMP_CLEANUP_END?names[site-DMP_ROOT]:"unknown";
}
static int drum_midi_export(const char *path){
 int fd=-1;struct stat st;CommandState *s=map_file(path,sizeof(*s),0,&fd,&st);if(s==MAP_FAILED){fputs("MIDI probe requires matching diagnostic CMD3201 state size and owner\n",stderr);return 1;}
 if(!header(s)||!file_current(path,fd,&st)){munmap(s,sizeof(*s));close(fd);fputs("MIDI probe state version or file identity mismatch\n",stderr);return 1;}
 DrumMidiProbe *d=&s->drum_midi;unsigned count,open=0,missing=0,writers=0;
 uint32_t stopped_admission=atomic_load(&d->until)==0;
 for(unsigned lane=0;lane<COMMAND_LANES;lane++){writers+=atomic_load(d->active+lane);open+=atomic_load(d->open+lane);}
 count=atomic_load_explicit(&d->count,memory_order_acquire);
 uint32_t until=atomic_load(&d->until),now=command_now(s->origin_sec,s->origin_nsec);
 unsigned active=until&&now<until,started=atomic_load(&d->started);
 if(count>DMP_RECORDS)count=DMP_RECORDS;
 printf("{\"format\":\"MIDI_NATIVE_PROBE1\",\"command_version\":%u,\"pid\":%u,\"start_lo\":%u,\"start_hi\":%u,\"image_bias\":%u,\"started\":%u,\"active\":%u,\"until\":%u,\"error\":%u,\"open_frames\":%u,\"physical_delivery_observed\":false,\"records\":[",s->version,s->pid,s->start_lo,s->start_hi,atomic_load(&d->bias),started,active,until,atomic_load(&d->error),open);
 unsigned first=1;
 for(unsigned i=0;i<count;i++){
  DrumMidiRecord *r=d->records+i;if(atomic_load_explicit(&r->sequence,memory_order_acquire)!=i+1){missing++;continue;}
  uint32_t w[DMP_WORDS];for(unsigned j=0;j<DMP_WORDS;j++)w[j]=atomic_load_explicit(r->words+j,memory_order_relaxed);
  printf("%s{\"sequence\":%u,\"site\":\"%s\",\"site_id\":%u,\"tick\":%u,\"token\":%u,\"epoch\":%u,\"sp\":%u,\"root\":%u,\"frame\":%u,\"parent\":%u,\"payload\":[",first?"":",",i+1,drum_midi_site(w[0]),w[0],w[1],w[2],w[3],w[4],w[5],w[6],w[7]);first=0;
  for(unsigned j=8;j<DMP_WORDS;j++)printf("%s%u",j==8?"":",",w[j]);
  fputs("]}",stdout);
 }
 printf("],\"reserved\":%u,\"missing_records\":%u,\"active_writers\":%u,\"admission_stopped\":%u,\"capture_complete\":%s}\n",count,missing,writers,stopped_admission,started&&count&&stopped_admission&&!writers&&!active&&!open&&!missing&&!atomic_load(&d->error)?"true":"false");
 int result=ferror(stdout)?1:0;munmap(s,sizeof(*s));close(fd);return result;
}
#endif
