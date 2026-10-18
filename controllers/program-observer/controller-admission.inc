/* Admission is bridge-owned; configuration and lifetime remain session-owned.
 * Reuse the existing fixed Preferences page, no discovery/polling owner. */
#ifndef CONTROLLER_ADMISSION_CHECKPOINT
#define CONTROLLER_ADMISSION_CHECKPOINT() ((void)0)
#endif
static int collection_admission_matches(SurfaceCollection *all,uint32_t *generation){
 NativePreferencesState *s=all->preferences;
 for(unsigned attempt=0;attempt<4;attempt++){
  uint32_t before=__atomic_load_n(&s->collection_revision,__ATOMIC_ACQUIRE);if(before&1)continue;
  uint32_t current=s->collection_generation;unsigned match=s->collection_count==all->count;
  for(unsigned i=0;match&&i<all->count;i++){
   McuControllerPreference record={0};record.enabled=all->enabled[i];record.input=all->contexts[i].controller;record.output=all->contexts[i].controller_output;record.hui_policy=all->contexts[i].controller_hui_policy;
   NativePreferencesController expected;native_state_controller(&expected,&record,s->controllers[i].status);
   match=!memcmp(&expected,s->controllers+i,sizeof(expected));
  }
  uint32_t after=__atomic_load_n(&s->collection_revision,__ATOMIC_ACQUIRE);
  if(before==after&&!(after&1)){if(match)*generation=current;return match;}
 }
 return 0;
}
static void collection_admission_publish(SurfaceCollection *all){
 NativePreferencesState *s=all->preferences;if(!s||!all->preferences_lifetime||all->preferences_lifetime!=__atomic_load_n(&s->controller_lifetime,__ATOMIC_ACQUIRE))return;
 uint32_t generation=__atomic_load_n(&s->collection_generation,__ATOMIC_ACQUIRE);
 if(generation!=all->preferences_generation){if(!collection_admission_matches(all,&generation))return;all->preferences_generation=generation;}
 uint32_t statuses[16]={0};
 for(unsigned i=0;i<all->count;i++){
  Surface *surface=all->surfaces+i;
  statuses[i]=!all->enabled[i]?NATIVE_CONTROLLER_DISABLED:!surface->seq?NATIVE_CONTROLLER_DISCONNECTED:NATIVE_CONTROLLER_ACTIVE;
  if(surface->seq&&mcu_profile_native(all->contexts[i].controller.profile)&&!novation_admitted(&surface->novation))
   statuses[i]=(surface->novation.admission_reported||surface->novation.stage>=16)?NATIVE_CONTROLLER_UNKNOWN_MODE:NATIVE_CONTROLLER_WAIT_MODE;
 }
 NativeControllerAdmission *a=&s->admission;
 uint32_t revision=__atomic_load_n(&a->revision,__ATOMIC_ACQUIRE);
 if(!(revision&1)&&a->lifetime==all->preferences_lifetime&&a->collection_generation==generation&&a->count==all->count&&!memcmp(a->status,statuses,sizeof(statuses)))return;
 /* The session has reaped the prior bridge before admitting this sole writer.
  * An interrupted predecessor may leave any payload and an odd revision. Keep
  * it odd throughout replacement, then publish the next even revision. */
 revision|=1u;__atomic_exchange_n(&a->revision,revision,__ATOMIC_ACQ_REL);CONTROLLER_ADMISSION_CHECKPOINT();
 a->lifetime=all->preferences_lifetime;CONTROLLER_ADMISSION_CHECKPOINT();
 a->collection_generation=generation;CONTROLLER_ADMISSION_CHECKPOINT();
 a->count=all->count;CONTROLLER_ADMISSION_CHECKPOINT();
 memcpy(a->status,statuses,sizeof(statuses));CONTROLLER_ADMISSION_CHECKPOINT();
 __atomic_store_n(&a->revision,revision+1u,__ATOMIC_RELEASE);
}
static int collection_admission_open(SurfaceCollection *all,const char *source_path,unsigned mpc_pid){
 char path[PATH_MAX];const char *slash=strrchr(source_path,'/');if(!slash)return 1;
 size_t prefix=(size_t)(slash-source_path)+1;const char suffix[]="native-preferences.state";
 if(prefix+sizeof(suffix)>sizeof(path))return 0;
 memcpy(path,source_path,prefix);memcpy(path+prefix,suffix,sizeof(suffix));
 if(access(path,F_OK)&&errno==ENOENT)return 1; /* Standalone bridge CLI. */
 if(native_state_open(path,&all->preferences))return 0;
 if(all->preferences->pid!=mpc_pid){munmap(all->preferences,sizeof(*all->preferences));all->preferences=NULL;return 0;}
 all->preferences_lifetime=__atomic_load_n(&all->preferences->controller_lifetime,__ATOMIC_ACQUIRE);
 /* A manual CLI override must not project onto unrelated saved rows. */
 if(!collection_admission_matches(all,&all->preferences_generation)){all->preferences_lifetime=0;return 1;}
 collection_admission_publish(all);return 1;
}
