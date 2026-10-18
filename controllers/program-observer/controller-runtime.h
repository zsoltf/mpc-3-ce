/* One bounded snapshot/command cycle shared by every controller instance. */
#ifndef MPC_CONTROLLER_RUNTIME_H
#define MPC_CONTROLLER_RUNTIME_H

typedef struct ControllerRuntime {
 CopiedMirror *snapshot;
 uint32_t now;
 int snapshot_result;
 unsigned snapshot_attempted,snapshot_pads,settlement_done;
 uint64_t iterations,snapshot_requests,snapshot_copies,settlement_requests,settlement_scans,submission_visits,deferred_pad_views;
} ControllerRuntime;

static inline void runtime_cycle_begin(ControllerRuntime *runtime){
 runtime->iterations++;runtime->snapshot_attempted=runtime->snapshot_pads=runtime->settlement_done=0;runtime->snapshot_result=0;
}

static inline int runtime_snapshot(ControllerRuntime *runtime,const MirrorState *state,int pads){
 runtime->snapshot_requests++;
 if(runtime->snapshot_attempted){
  /* Entering the pad view during a MIDI batch is applied on the next cycle;
   * never pay for a second full snapshot or interpret absent pad rows. */
  if(pads&&!runtime->snapshot_pads){runtime->deferred_pad_views++;return 0;}
  return runtime->snapshot_result;
 }
 runtime->snapshot_attempted=1;runtime->snapshot_pads=(unsigned)(pads!=0);runtime->snapshot_copies++;
 runtime->snapshot_result=fresh_copy_view(state,runtime->snapshot,&runtime->now,pads);
 return runtime->snapshot_result;
}

static inline void runtime_settle(ControllerRuntime *runtime,MirrorInput *in,MirrorBank *bank){
 runtime->settlement_requests++;
 if(runtime->settlement_done)return;
 runtime->settlement_done=1;runtime->settlement_scans++;
 input_settle(in,bank,runtime->snapshot,runtime->now);
}

static inline void runtime_submit(ControllerRuntime *runtime,MirrorInput *in,MirrorBank *bank){
 runtime_settle(runtime,in,bank);
 if(in->error)return;
 runtime->submission_visits++;
 input_submit(in,bank,runtime->snapshot,runtime->now);
}

#endif
