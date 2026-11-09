#!/bin/sh
# The session package. acvs.service owns every MPC process; this script admits a
# session, prepares and closes each CE instance for acvs, and runs the follower
# that starts the controller for each instance. It never launches MPC.
set -eu
# Runs in place (image or dev override). Locators/logs stay on /data to survive
# a power-off; shared state is tmpfs.
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
home=/data/mpclearn
history=$home/history
preferences=$home/surface-preferences
controller_preferences=$home/controller-preferences
runtime=/run/mpclearn
state=$runtime/state
decision=$runtime/launch.decision
# The SD plugin view was released for Controller Mode (`released`); `due`
# after a launch that could not admit the card again yet.
plugins_released=$runtime/plugins.released
staging=$runtime/archive
controller_recovery=$runtime/controller-recovery
adapter=$here/mpclearn-controls
inspector=$here/main-button
# History bound (bytes, entries) and per-file caps; head caps are 4096 multiples.
history_budget=16777216
history_entries=8
cap_mpc_log=1048576
cap_log=262144
cap_locator=4096
cap_controller_preferences=8992
cap_status_head=1835008
cap_status_tail=262144
cap_mirror_head=458752
cap_mirror_tail=65536
cap_session_head=196608
cap_session_tail=65536
case "$here" in
 /usr/share/mpclearn/mcu|/data/mpclearn/dev) ;;
 *) echo "Unsupported package location $here; use /usr/share/mpclearn/mcu or /data/mpclearn/dev." >&2;exit 2;;
esac
owned_dir(){ [ -d "$1" ] && [ ! -L "$1" ] && [ "$(stat -c %u "$1")" = 0 ] && [ "$(stat -c %a "$1")" = 700 ]; }
tmpfs_dir(){ owned_dir "$1" && [ "$(stat -f -c %T "$1")" = tmpfs ]; }
[ "$(id -u)" -eq 0 ] && owned_dir "$here" || exit 2
cd "$here"
[ -f session-package.sha256 ] && sha256sum -c session-package.sha256 >/dev/null || exit 1
if [ -e plugin-setup ] || [ -e plugin-bundle.json ];then
 for name in plugin-setup plugin-bundle.json;do
  [ -f "$name" ] && [ ! -L "$name" ] && awk -v n="$name" '$2==n{f=1}END{exit !f}' session-package.sha256 || exit 1
 done
fi
grep -Fqx '#define WINDOW_SECONDS 0u' config.h || exit 1
grep -Fqx "#define OBSERVER_LIBRARY \"$here/command-observer.so\"" config.h || exit 1
grep -Fqx "#define OBSERVER_LOG \"$state/volume.state\"" config.h || exit 1
grep -Fqx "#define COMMAND_PATH \"$state/command.state\"" config.h || exit 1
probe_state=
probe_definitions=$(grep -c '^#define PLUGIN_HOST_PROBE_STATE ' config.h || :)
case "$probe_definitions" in
 0) ;;
 1) grep -Fqx "#define PLUGIN_HOST_PROBE_STATE \"$state/plugin-host-probe.state\"" config.h || exit 1;probe_state=$state/plugin-host-probe.state;;
 *) exit 1;;
esac
command=${1:-status};case "$command" in start|status|stop|bridge-start|bridge-stop|list-midi|configure|admit|launch-prepare|close|plugin-release) ;; *) echo 'mcu-session.sh start|status|stop|bridge-start|bridge-stop|list-midi|configure|admit|launch-prepare PID|close|plugin-release [--profile=xtouch|xtouch-mini|generic|hui|lcxl3-daw|launchpad-x-programmer|launchpad-pro-mk3-programmer|mpk-mini-mk3|lpd8-mk2|mpk249 (--endpoint-client=NAME --endpoint-port=NAME --endpoint-type=kernel|user|any | --input-client=NAME --input-port=NAME --input-type=kernel|user|any --output-client=NAME --output-port=NAME --output-type=kernel|user|any) --hui-mode=pickup|motorized] [--verbose]' >&2;exit 2;; esac
[ "$#" -eq 0 ] || shift
# Instance operations run for acvs: no controller options.
launcher=
case "$command" in
 launch-prepare) [ "$#" -eq 1 ] || exit 2;launcher=$1;shift;case "$launcher" in ''|*[!0-9]*) exit 2;; esac;[ "$launcher" -gt 1 ] || exit 2;;
 admit|close|plugin-release) [ "$#" -eq 0 ] || exit 2;;
esac
verbose=;controller_profile=xtouch;endpoint_client=;endpoint_port=;endpoint_type=kernel;input_client=;input_port=;input_type=kernel;output_client=;output_port=;output_type=kernel;legacy_endpoint_options=0;split_endpoint_options=0;hui_mode=pickup;hui_mode_option=0;selection_options=0;profile_option=0
for option in "$@";do
 case "$option" in
  --verbose) verbose=--verbose;;
  --profile=xtouch|--profile=xtouch-mini|--profile=generic|--profile=hui|--profile=lcxl3-daw|--profile=launchpad-x-programmer|--profile=launchpad-pro-mk3-programmer|--profile=mpk-mini-mk3|--profile=lpd8-mk2|--profile=mpk249) controller_profile=${option#--profile=};selection_options=$((selection_options+1));profile_option=1;;
  --endpoint-client=?*) endpoint_client=${option#--endpoint-client=};legacy_endpoint_options=1;selection_options=$((selection_options+1));;
  --endpoint-port=?*) endpoint_port=${option#--endpoint-port=};legacy_endpoint_options=1;selection_options=$((selection_options+1));;
  --endpoint-type=kernel|--endpoint-type=user|--endpoint-type=any) endpoint_type=${option#--endpoint-type=};legacy_endpoint_options=1;selection_options=$((selection_options+1));;
  --input-client=?*) input_client=${option#--input-client=};split_endpoint_options=1;selection_options=$((selection_options+1));;
  --input-port=?*) input_port=${option#--input-port=};split_endpoint_options=1;selection_options=$((selection_options+1));;
  --input-type=kernel|--input-type=user|--input-type=any) input_type=${option#--input-type=};split_endpoint_options=1;selection_options=$((selection_options+1));;
  --output-client=?*) output_client=${option#--output-client=};split_endpoint_options=1;selection_options=$((selection_options+1));;
  --output-port=?*) output_port=${option#--output-port=};split_endpoint_options=1;selection_options=$((selection_options+1));;
  --output-type=kernel|--output-type=user|--output-type=any) output_type=${option#--output-type=};split_endpoint_options=1;selection_options=$((selection_options+1));;
  --hui-mode=pickup|--hui-mode=motorized) hui_mode=${option#--hui-mode=};hui_mode_option=1;selection_options=$((selection_options+1));;
  *) exit 2;;
 esac
done
if [ "$command" = configure ] && [ "$profile_option" -eq 0 ];then echo 'configure requires an explicit controller profile.' >&2;exit 2;fi
if [ "$legacy_endpoint_options" -eq 1 ] && [ "$split_endpoint_options" -eq 1 ];then echo 'Use either the same-port endpoint options or the separate input/output options, not both.' >&2;exit 2
elif [ "$controller_profile" != xtouch ] && [ "$controller_profile" != xtouch-mini ];then
 if [ "$split_endpoint_options" -eq 1 ];then
  [ -n "$input_client" ] && [ -n "$input_port" ] && [ -n "$output_client" ] && [ -n "$output_port" ] || { echo 'The selected protocol requires complete input and output endpoint names.' >&2;exit 2; }
  if [ "$controller_profile" = hui ];then case "$input_type:$output_type" in kernel:kernel|kernel:user|user:kernel|user:user) ;; *) echo 'HUI requires exact kernel or user input and output endpoint types.' >&2;exit 2;; esac;fi
 else
  [ -n "$endpoint_client" ] && [ -n "$endpoint_port" ] || { echo 'The selected protocol requires exact endpoint client and port names.' >&2;exit 2; }
  [ "$controller_profile" != hui ] || [ "$endpoint_type" = kernel ] || [ "$endpoint_type" = user ] || { echo 'HUI requires an exact kernel or user endpoint type.' >&2;exit 2; }
 fi
else
 [ "$legacy_endpoint_options" -eq 0 ] && [ "$split_endpoint_options" -eq 0 ] && [ "$hui_mode_option" -eq 0 ] || { echo 'Selected hardware profile has a fixed physical endpoint.' >&2;exit 2; }
fi
[ "$controller_profile" = hui ] || [ "$hui_mode_option" -eq 0 ] || { echo 'HUI behavior applies only to the HUI profile.' >&2;exit 2; }
owned_dir "$home" || { echo "$home is not a root-owned 0700 folder; refusing to write there." >&2;exit 2; }
for folder in "$home/session" "$history";do
 [ -e "$folder" ] || [ -L "$folder" ] || mkdir -m 700 "$folder" 2>/dev/null || :
 owned_dir "$folder" || { echo "$folder is not a root-owned 0700 folder." >&2;exit 2; }
done
[ -e "$runtime" ] || [ -L "$runtime" ] || mkdir -m 700 "$runtime" 2>/dev/null || :
tmpfs_dir "$runtime" || { echo "$runtime is not a root-owned 0700 tmpfs folder." >&2;exit 1; }
cd "$home/session"
exec 9>session.lock
# BusyBox flock has no timeout. The instance operations run inside acvs and
# must not wait without bound; the follower holds the lock only briefly.
lock_bounded(){ n=0;until flock -n 9;do n=$((n+1));[ "$n" -le "$1" ] || return 1;sleep .1;done; }
case "$command" in
 launch-prepare) lock_bounded 100 || { echo 'session.lock busy; this instance runs stock.' >&2;exit 1; };;
 plugin-release) lock_bounded 100 || { echo 'session.lock busy; SD plugins not released.' >&2;exit 1; };;
 close) lock_bounded 300 || { echo 'session.lock busy; instance not closed, the next launch runs stock.' >&2;exit 1; };;
 *) flock 9;;
esac
# Only live shared pages use RAM; receipts/logs stay in the session folder.
# Refuse aliases and unexpected files rather than replacing another producer.
state_check(){
 tmpfs_dir "$state" || return 1
 for file in "$state"/* "$state"/.[!.]* "$state"/..?*;do
  [ -e "$file" ] || { [ ! -L "$file" ] || return 1;continue; }
  case "$file" in
   "$state/command.state"|"$state/volume.state"|"$state/native-preferences.state") [ ! -L "$file" ] && [ -f "$file" ] && [ "$(stat -c %u "$file")" = 0 ] || return 1;;
   "$probe_state") [ -n "$probe_state" ] && [ ! -L "$file" ] && [ -f "$file" ] && [ "$(stat -c '%u:%a:%s' "$file")" = 0:600:256 ] || return 1;;
   *) return 1;;
  esac
 done
}
probe_absent(){ [ -z "$probe_state" ] || [ ! -e "$probe_state" ]; }
probe_present(){ [ -n "$probe_state" ] && [ -e "$probe_state" ]; }
remove_probe_state(){ [ -z "$probe_state" ] || rm -f "$probe_state"; }
if [ -e "$state" ] || [ -L "$state" ];then state_check || exit 1
else case "$command" in start|admit|launch-prepare) mkdir -m 700 "$state" && state_check || exit 1;; esac
fi
boot_id=$(cat /proc/sys/kernel/random/boot_id)
same_boot(){ [ ! -f session.boot-id ] || [ "$(cat session.boot-id)" = "$boot_id" ]; }
# A live session belongs to the package that started it (identity uses its paths).
if same_boot && [ -f session.location ] && [ ! -f session.revoked ] && [ "$(cat session.location)" != "$here" ];then
 echo "The current session belongs to $(cat session.location); use its mcu." >&2;exit 2
fi
start_tick(){ sed 's/.*) //' "/proc/$1/stat" 2>/dev/null | awk '{print $20}'; }
mpc_running(){
 for exe in /proc/[0-9]*/exe;do
  case "$(readlink "$exe" 2>/dev/null || :)" in */MPC|*/MPC\ \(deleted\)) return 0;; esac
 done
 return 1
}
write_identity(){ p=$1;[ -r "/proc/$p/stat" ] || return 1;printf '%s %s\n' "$p" "$(start_tick "$p")" > "$2"; }
identity(){
 same_boot || return 1
 [ -f "$1" ] || return 1
 read -r pid tick < "$1"
 case "$pid:$tick" in *[!0-9:]*|:*) return 1;; esac
 [ "$pid" -gt 1 ] || return 1
 [ -n "$pid" ] && [ -n "$tick" ] && [ -r "/proc/$pid/stat" ] && [ "$(start_tick "$pid")" = "$tick" ] && [ "$(readlink "/proc/$pid/exe")" = "$2" ]
}
status(){ "$here/command-client" session-status "$state/command.state" "$state/volume.state"; }
bridge_stop(){
 if identity bridge.pid "$here/mirror-input";then
  kill -TERM "$pid"
  n=0;while identity bridge.pid "$here/mirror-input";do n=$((n+1));[ "$n" -le 150 ] || { echo 'Bridge did not finish bounded drain; incomplete, no producer stop.' >&2;return 1; };sleep .1;done
 fi
 if [ -f bridge.pid ];then
  n=0;while [ ! -f bridge.exit ];do n=$((n+1));[ "$n" -le 20 ] || { echo 'Missing bridge exit result; stop is incomplete.' >&2;return 1; };sleep .1;done
  [ "$(cat bridge.exit)" = 0 ] || { echo 'Bridge reported incomplete/error; inspect source before recovery.' >&2;return 1; }
 fi
}
bridge_start(){
 bridge_expected=1
 identity mpc.pid /usr/bin/MPC || return 1
 if identity bridge.pid "$here/mirror-input";then echo 'Bridge already active.' >&2;return 1;fi
 if [ -f bridge.pid ] && [ ! -f bridge.exit ];then
  n=0;while [ ! -f bridge.exit ];do n=$((n+1));[ "$n" -le 20 ] || { echo 'Prior bridge waiter did not finish; restart is incomplete.' >&2;return 1; };sleep .1;done
 fi
 bridge_lifetime=0
 if [ -f "$state/native-preferences.state" ];then bridge_lifetime=$("$here/mirror-input" --native-preferences-invalidate "$state/native-preferences.state") || return 1;fi
 if [ "$selection_options" -eq 0 ];then
  enabled_code=0;"$here/mirror-input" --controller-any-enabled "$controller_preferences" || enabled_code=$?
  case "$enabled_code" in 0) ;; 3) bridge_expected=0;rm -f bridge.pid bridge.exit;return 0;; *) return 1;; esac
 fi
 source_code=0;status >/dev/null || source_code=$?
 case "$source_code" in 0|3) ;; *) return 1;; esac
 motor_choice=$("$here/mirror-input" --surface-status "$preferences") || { echo "Invalid surface preference; refusing automatic motor enable." >&2;return 1; }
 rm -f bridge.pid bridge.exit
 # This child only waits for its explicit bridge and records its exit status.
 # It never restarts it. The operation lock is not inherited.
 (
  set +e
  if [ "$selection_options" -eq 0 ];then
   "$here/mirror-input" "$state/volume.state" "$state/command.state" manual $verbose "--controller-preferences=$controller_preferences" "--held-mask=${handoff_holds:-0}" "--motors=$motor_choice" "--preferences=$preferences" "--stop-adapter-exe=$adapter" &
  elif [ "$split_endpoint_options" -eq 1 ];then
   if [ "$controller_profile" = hui ];then
    "$here/mirror-input" "$state/volume.state" "$state/command.state" manual $verbose "--profile=$controller_profile" "--input-client=$input_client" "--input-port=$input_port" "--input-type=$input_type" "--output-client=$output_client" "--output-port=$output_port" "--output-type=$output_type" "--hui-mode=$hui_mode" "--held-mask=${handoff_holds:-0}" "--motors=$motor_choice" "--preferences=$preferences" "--stop-adapter-exe=$adapter" &
   else
    "$here/mirror-input" "$state/volume.state" "$state/command.state" manual $verbose "--profile=$controller_profile" "--input-client=$input_client" "--input-port=$input_port" "--input-type=$input_type" "--output-client=$output_client" "--output-port=$output_port" "--output-type=$output_type" "--held-mask=${handoff_holds:-0}" "--motors=$motor_choice" "--preferences=$preferences" "--stop-adapter-exe=$adapter" &
   fi
  elif [ "$controller_profile" = hui ];then
   "$here/mirror-input" "$state/volume.state" "$state/command.state" manual $verbose "--profile=$controller_profile" "--endpoint-client=$endpoint_client" "--endpoint-port=$endpoint_port" "--endpoint-type=$endpoint_type" "--hui-mode=$hui_mode" "--held-mask=${handoff_holds:-0}" "--motors=$motor_choice" "--preferences=$preferences" "--stop-adapter-exe=$adapter" &
  elif [ "$controller_profile" != xtouch ] && [ "$controller_profile" != xtouch-mini ];then
   "$here/mirror-input" "$state/volume.state" "$state/command.state" manual $verbose "--profile=$controller_profile" "--endpoint-client=$endpoint_client" "--endpoint-port=$endpoint_port" "--endpoint-type=$endpoint_type" "--held-mask=${handoff_holds:-0}" "--motors=$motor_choice" "--preferences=$preferences" "--stop-adapter-exe=$adapter" &
  else
   "$here/mirror-input" "$state/volume.state" "$state/command.state" manual $verbose "--profile=$controller_profile" "--held-mask=${handoff_holds:-0}" "--motors=$motor_choice" "--preferences=$preferences" "--stop-adapter-exe=$adapter" &
  fi
  b=$!
  if ! write_identity "$b" bridge.pid;then
   # The direct child cannot be PID-reused before this shell reaps it. If it
   # survived an identity-read failure, stop only that owned child and bound
   # the wait; either way publish a terminal receipt for rollback.
   if kill -0 "$b" 2>/dev/null;then
    kill -TERM "$b" 2>/dev/null || :
    n=0;while kill -0 "$b" 2>/dev/null;do n=$((n+1));if [ "$n" -gt 20 ];then kill -KILL "$b" 2>/dev/null || :;break;fi;sleep .1;done
   fi
   child_code=0;wait "$b" || child_code=$?
   if [ "$bridge_lifetime" -ne 0 ];then "$here/mirror-input" --native-preferences-invalidate "$state/native-preferences.state" "$bridge_lifetime" || :;fi
   echo "BRIDGE_LAUNCH_IDENTITY_FAILED child=$b exit=$child_code" >&2
   printf '%s\n' 125 >bridge.exit
   exit 1
  fi
  wait "$b";code=$?
  if [ "$bridge_lifetime" -ne 0 ];then "$here/mirror-input" --native-preferences-invalidate "$state/native-preferences.state" "$bridge_lifetime" || :;fi
  printf '%s\n' "$code" > bridge.exit
 ) 9>&- >bridge.log 2>&1 &
 n=0
 while :;do
  if identity bridge.pid "$here/mirror-input" && grep -Fqx "BRIDGE_READY pid=$pid" bridge.log;then
   source_code=0;status >/dev/null || source_code=$?
   case "$source_code" in 0|3) [ ! -f bridge.exit ] && identity bridge.pid "$here/mirror-input" && break;; esac
   echo 'Bridge/source became unavailable during startup; no retry.' >&2
   tail -n 8 bridge.log >&2;return 1
  fi
  n=$((n+1))
  if [ "$n" -gt 50 ] || [ -f bridge.exit ];then
   echo 'Bridge did not complete startup; no retry. Inspect bridge.log and bridge.exit.' >&2
   tail -n 8 bridge.log >&2;return 1
  fi
  sleep .1
 done
 echo "Bridge running pid=$pid; explicit bridge-stop or stop required."
}

exact(){ [ -n "$1" ] && [ -n "$2" ] && [ "$(readlink "/proc/$1/exe" 2>/dev/null || :)" = "$3" ] && [ "$(start_tick "$1")" = "$2" ]; }
adapter_scan(){
 adapter_pid=;adapter_tick=
 for exe in /proc/[0-9]*/exe;do
  [ "$(readlink "$exe" 2>/dev/null || :)" = "$adapter" ] || continue
  p=${exe#/proc/};p=${p%/exe};t=$(start_tick "$p")
  exact "$p" "$t" "$adapter" || continue
  [ -z "$adapter_pid" ] || { echo 'Multiple exact adapter processes; refusing action.' >&2;return 1; }
  adapter_pid=$p;adapter_tick=$t
 done
}
stop_adapter(){
 adapter_scan || return 1
 [ -n "$adapter_pid" ] || return 0
 exact "$adapter_pid" "$adapter_tick" "$adapter" || return 1
 kill -TERM "$adapter_pid"
 n=0;while exact "$adapter_pid" "$adapter_tick" "$adapter";do n=$((n+1));[ "$n" -le 50 ] || return 1;sleep .1;done
}
start_adapter(){
 for file in "$adapter" "$inspector";do [ -x "$file" ] && [ "$(stat -c %u "$file")" = 0 ] || return 1;done
 adapter_scan && [ -z "$adapter_pid" ] || return 1
 (exec env -u LD_PRELOAD -u LD_LIBRARY_PATH LD_BIND_NOW=1 "$adapter" "--stop-bridge-exe=$here/mirror-input") 9>&- >manual-adapter.log 2>&1 &
 launched=$!;n=0
 while :;do
  adapter_scan || return 1
  [ "$adapter_pid" = "$launched" ] && grep -q '^DISARMED ' manual-adapter.log && break
  kill -0 "$launched" 2>/dev/null || return 1
  n=$((n+1));[ "$n" -le 50 ] || return 1;sleep .1
 done
 printf '%s %s
' "$adapter_pid" "$adapter_tick" >manual-adapter-unarmed.pid
}
route(){
 adapter_scan && [ -n "$adapter_pid" ] || return 1
 "$inspector" --inspect --adapter-exe "$adapter" --action play 9>&- || return 1
 exact "$adapter_pid" "$adapter_tick" "$adapter"
}
wait_adapter_route(){
 # DISARMED proves the adapter's own port, not MPC's later subscription.
 # This is one bounded arming admission, never a bridge/process restart loop.
 read -r route_clock route_idle </proc/uptime
 route_started=${route_clock%%.*};route_attempts=0
 while :;do
  authorized && identity bridge.pid "$here/mirror-input" && status >/dev/null || return 1
  route >/dev/null 2>&1 && return 0
  read -r route_clock route_idle </proc/uptime
  route_attempts=$((route_attempts+1))
  [ "$route_attempts" -lt 50 ] && [ $((${route_clock%%.*}-route_started)) -lt 5 ] || return 1
  sleep .1
 done
}
authorized(){ [ -f session.id ] && [ "$(cat session.id)" = "$session_id" ] && [ ! -e session.revoked ]; }
arm_adapter(){
 authorized && [ -f manual-adapter-unarmed.pid ] || return 1
 read -r unarmed_pid unarmed_tick <manual-adapter-unarmed.pid
 wait_adapter_route && [ "$adapter_pid" = "$unarmed_pid" ] && [ "$adapter_tick" = "$unarmed_tick" ] || return 1
 status >/dev/null && identity bridge.pid "$here/mirror-input" && authorized || return 1
 read -r app_pid app_tick <mpc.pid
 if "$here/command-client" new-project-intent "$state/command.state" "$app_pid" "$app_tick" >/dev/null;then return 1;fi
 exact "$adapter_pid" "$adapter_tick" "$adapter" || return 1
 kill -USR1 "$adapter_pid"
 rm -f manual-adapter-unarmed.pid
}
# History is text only: capped logs, locators and summaries; never raw state.
# One CE instance: identity, exit, receipts and the capped MPC output from the journal.
generation_files='mpc.pid mpc.exit bridge.pid bridge.exit bridge.log mpc.log manual-adapter.log manual-adapter-unarmed.pid generation.ready generation.failed'
session_files='session.id session.boot-id session.location session.revoked follower.pid follower.log crash-count crash-guard handoff-holds stop-incomplete-status.txt incomplete-command-status.json incomplete-command-status.json.truncated incomplete-mirror-read.json incomplete-mirror-read.json.truncated incomplete-bridge.log'
cap_for(){
 case "$1" in
  mpc.log) echo "$cap_mpc_log";;
  *.log|stop-incomplete-status.txt) echo "$cap_log";;
  incomplete-command-status.json) echo $((cap_status_head+cap_status_tail+256));;
  incomplete-mirror-read.json) echo $((cap_mirror_head+cap_mirror_tail+256));;
  *) echo "$cap_locator";;
 esac
}
# Keep head+tail around a marker line; a cut file is not JSON (.truncated says so).
# Runs where set -e does not apply. BusyBox head lacks -c, so dd reads 4K blocks.
cap_summary(){
 size=$(stat -c %s "$1") || return 1
 if [ "$size" -le $(($3+$4)) ];then rm -f "$2.truncated" && mv "$1" "$2";return;fi
 { dd if="$1" bs=4096 count=$(($3/4096)) 2>/dev/null && printf '\n[mpclearn history: %s bytes omitted here; this file is not valid JSON]\n' $((size-$3-$4)) && tail -c "$4" "$1"; } >"$2" || return 1
 printf 'original_bytes=%s kept_head=%s kept_tail=%s\n' "$size" "$3" "$4" >"$2.truncated" || return 1
 rm -f "$1"
}
# Summaries into DIR; mirror-read exits 1 on a closed producer, still evidence.
summarize_state(){
 raw=$runtime/summary.raw
 if [ -f "$state/command.state" ];then
  "$here/command-client" status "$state/command.state" >"$raw" 2>&1 || :
  cap_summary "$raw" "$1/$2" "$cap_status_head" "$cap_status_tail" || return 1
 fi
 if [ -f "$state/command.state" ] || [ -f "$state/volume.state" ];then
  "$here/command-client" session-status "$state/command.state" "$state/volume.state" >"$raw" 2>&1 || :
  cap_summary "$raw" "$1/$3" "$cap_session_head" "$cap_session_tail" || return 1
 fi
 if [ -f "$state/volume.state" ];then
  "$here/mirror-read" "$state/volume.state" --all >"$raw" 2>&1 || :
  cap_summary "$raw" "$1/$4" "$cap_mirror_head" "$cap_mirror_tail" || return 1
 fi
}
tree_bytes(){
 # Apparent bytes, each folder 4096, so tmpfs and ext4 measure alike.
 find "$1" \( -type f -o -type d \) -exec stat -c '%F %s' {} + | awk '{if($1=="directory")n+=4096;else n+=$NF}END{print n+0}'
}
history_names(){
 for entry in "$history"/* "$history"/.[!.]* "$history"/..?*;do
  [ -e "$entry" ] || [ -L "$entry" ] || continue
  printf '%s\n' "${entry##*/}"
 done | sort -n
}
# Drop a crash leftover, measure, prune oldest until it fits, copy, rename.
history_commit(){
 owned_dir "$history" || { echo "$history is not a root-owned 0700 folder; entry not written." >&2;return 1; }
 rm -rf "$history/.new" && [ ! -e "$history/.new" ] || return 1
 new_bytes=$(tree_bytes "$staging") || return 1
 [ $((4096+new_bytes)) -le "$history_budget" ] || { echo "History entry of $new_bytes bytes exceeds the budget; not retained." >&2;archive=;return 0; }
 while :;do
  total=4096;count=0;oldest=;last=0
  for name in $(history_names);do
   total=$((total+$(tree_bytes "$history/$name")));count=$((count+1))
   [ -n "$oldest" ] || oldest=$name
   # Strip leading zeros: shell arithmetic reads them as octal.
   number=${name%%-*};case "$number" in ''|*[!0-9]*) ;; *) number=${number#"${number%%[!0]*}"};[ "${number:-0}" -le "$last" ] || last=$number;; esac
  done
  [ $((total+new_bytes)) -gt "$history_budget" ] || [ $((count+1)) -gt "$history_entries" ] || break
  [ -n "$oldest" ] || return 1
  rm -rf "$history/$oldest";[ ! -e "$history/$oldest" ] && [ ! -L "$history/$oldest" ] || return 1
 done
 boot=$(cut -c1-8 session.boot-id 2>/dev/null || :);case "$boot" in ''|*[!0-9a-f]*) boot=unknown;; esac
 archive=$history/$(printf '%06d' $((last+1)))-$boot-$1
 mkdir -m 700 "$history/.new" || return 1
 for file in "$staging"/*;do [ ! -f "$file" ] || cp "$file" "$history/.new/" || return 1;done
 mv "$history/.new" "$archive"
}
stage_file(){
 [ -f "$1" ] && [ ! -L "$1" ] || return 0
 tail -c "$(cap_for "$1")" "$1" >"$staging/$1"
}
archive_entry(){
 rm -rf "$staging";mkdir -m 700 "$staging" || return 1
 generation=0
 if [ -f mpc.pid ];then
  generation=1
  read -r prior_pid prior_tick <mpc.pid
  label=$prior_pid-$prior_tick
  for file in $generation_files;do stage_file "$file" || return 1;done
  summarize_state "$staging" command-status.json session-status.txt mirror-read.json || return 1
 else
  label=unlaunched-$(cat session.id)
 fi
 if [ "$1" = session ];then for file in $session_files;do stage_file "$file" || return 1;done;fi
 case "$label" in *[!0-9a-z-]*) label=unnamed;; esac
 history_commit "$label" || return 1
 rm -rf "$staging" || return 1
 if [ "$generation" = 1 ];then
  rm -f "$controller_recovery"
  for file in $generation_files;do rm -f "$file";done
  rm -f "$state/command.state" "$state/volume.state" "$state/native-preferences.state"
  remove_probe_state
 fi
 if [ "$1" = session ];then
  for file in $session_files;do rm -f "$file";done
 fi
}
archive_generation(){
 archive_entry generation || return 1
 echo "Exact departed app retained as text summaries in ${archive:-no history entry}."
}
archive_session(){ archive_entry session; }
state_empty(){ state_check && [ ! -e "$state/command.state" ] && [ ! -e "$state/volume.state" ] && [ ! -e "$state/native-preferences.state" ] && probe_absent; }
follower_alive(){
 same_boot && [ -f follower.pid ] || return 1
 read -r follower_pid follower_tick <follower.pid || return 1
 case "$follower_pid:$follower_tick" in *[!0-9:]*|:*) return 1;; esac
 [ "$follower_pid" -gt 1 ] && [ "$(start_tick "$follower_pid")" = "$follower_tick" ]
}
# Runs for acvs before each CE instance, as a bounded child of the image's
# launcher. The launcher PID becomes MPC's, so it is the instance identity.
launch_prepare(){
 session_id=$(cat session.id 2>/dev/null) || return 1
 same_boot && authorized || { echo 'No admitted session.' >&2;return 1; }
 [ ! -e mpc.pid ] || { echo 'The previous instance was not closed.' >&2;return 1; }
 state_empty || { echo "$state is not empty." >&2;return 1; }
 if identity bridge.pid "$here/mirror-input";then echo 'A bridge still runs.' >&2;return 1;fi
 for file in $generation_files;do rm -f "$file";done
 kind=plain;reason=
 if [ -x "$here/plugin-setup" ];then
  # After Controller Mode the launcher switched MPC's USB back and waited for
  # the card: admit it again now, while MPC is stopped, bounded by
  # plugin-setup, which asks edisksd to mount it. Otherwise this instance runs
  # plain and a later launch tries.
  if [ -e "$plugins_released" ];then
   if admitted=$("$here/plugin-setup" prepare 9>&- 2>&1 >/dev/null);then rm -f "$plugins_released"
   else printf 'due\n' >"$plugins_released";reason=$(printf '%s\n' "$admitted" | tail -n 1);fi
  fi
  # Bounded by plugin-setup itself; it must not keep session.lock if this
  # child is killed by the launcher's watchdog.
  if unavailable=$("$here/plugin-setup" ready 9>&- 2>&1 >/dev/null);then kind=cardinal;reason=
  elif [ -z "$reason" ];then reason=$(printf '%s\n' "$unavailable" | tail -n 1);fi
 else reason='this package has no SD plugin admission'
 fi
 write_identity "$launcher" mpc.pid.next && mv mpc.pid.next mpc.pid || { rm -f mpc.pid.next;return 1; }
 echo "prepared $kind"
 # One line for the launch decision: why there are no Pd/Cardinal plugins.
 reason=${reason#Pd/Cardinal unavailable: }
 [ "$kind" = cardinal ] || [ -z "$reason" ] || printf 'reason %s\n' "$reason" | cut -c1-240
}
# acvs ExecStopPost after an instance exit (quit, mode switch, New Project,
# crash, systemctl stop/restart): stop its controller processes, record the
# exit, archive it and empty the shared state. A failure leaves the instance
# behind, so the next launch runs stock.
close_instance(){
 # An instance from an earlier boot is the next admission's to archive intact.
 [ -f mpc.pid ] && same_boot || return 0
 read -r departed_pid departed_tick <mpc.pid
 if identity mpc.pid /usr/bin/MPC;then echo 'The instance still runs; not closing it.' >&2;return 1;fi
 stop_adapter || { echo 'Adapter did not stop; instance not closed.' >&2;return 1; }
 bridge_stop || :
 if identity bridge.pid "$here/mirror-input";then
  kill -KILL "$pid"
  n=0;while identity bridge.pid "$here/mirror-input";do n=$((n+1));[ "$n" -le 50 ] || return 1;sleep .1;done
 fi
 # Recorded once: a close that failed later in an earlier attempt keeps the
 # departed instance's own exit when a later close archives it.
 if [ ! -f mpc.exit ];then
  printf 'service_result=%s exit_code=%s exit_status=%s\n' "${SERVICE_RESULT:-unknown}" "${EXIT_CODE:-unknown}" "${EXIT_STATUS:-unknown}" >mpc.exit
  echo "mpclearn close: instance $departed_pid ended: $(cat mpc.exit)" >&2
  # Faders still held at the end are handed to the next instance's bridge.
  rm -f handoff-holds
  if [ -f bridge.pid ] && [ -f bridge.log ];then
   read -r old_bridge old_bridge_tick <bridge.pid
   final_mask=$(sed -n "s/^TOUCH_FINAL pid=$old_bridge mask=\\([0-9][0-9]*\\)$/\\1/p" bridge.log)
   case "$final_mask" in ''|*[!0-9]*) ;; *) [ "$final_mask" -gt 511 ] || printf '%s\n' "$final_mask" >handoff-holds;; esac
  fi
  # Crash guard: two consecutive CE instances that end abnormally before their
  # source became ready leave the rest of this session's boot to stock MPC.
  crashes=0;[ ! -f crash-count ] || read -r crashes <crash-count || crashes=0
  case "$crashes" in ''|*[!0-9]*) crashes=0;; esac
  if [ "${SERVICE_RESULT:-success}" != success ] && [ ! -f generation.ready ];then crashes=$((crashes+1));else crashes=0;fi
  printf '%s\n' "$crashes" >crash-count
  if [ "$crashes" -ge 2 ] && [ ! -e crash-guard ];then
   printf '%s\n' "$crashes" >crash-guard
   echo 'mpclearn close: two CE instances ended before ready; the rest of this boot runs stock MPC.' >&2
  fi
 fi
 # At system shutdown the session folder keeps this instance; the next boot's
 # admission archives it.
 [ "$(systemctl is-system-running 2>/dev/null || :)" != stopping ] || return 0
 journalctl -q -b -o cat --no-pager _SYSTEMD_UNIT=acvs.service "_PID=$departed_pid" 2>/dev/null | tail -c "$cap_mpc_log" >mpc.log || :
 archive_generation
}
native_preferences_watch_start(){
 preferences_watch_pid=
 [ -f "$state/native-preferences.state" ] || return 0
 active_profile=${1:-saved};[ "$#" -gt 0 ] || { [ "$selection_options" -eq 0 ] || active_profile=$controller_profile; }
 "$here/mirror-input" --native-preferences-sync "$state/native-preferences.state" "$controller_preferences" "$active_profile" || return 1
 "$here/mirror-input" --native-preferences-watch "$state/native-preferences.state" 9>&- &
 preferences_watch_pid=$!
}
native_preferences_watch_stop(){
 [ -n "${preferences_watch_pid:-}" ] || return 0
 kill -TERM "$preferences_watch_pid" 2>/dev/null || :
 wait "$preferences_watch_pid" 2>/dev/null || :
 preferences_watch_pid=
}
native_preferences_recover_owner(){
 # The resident follower must parent the watcher and acknowledge its setup state.
 # A short-lived bridge-start helper cannot leave a watcher signaling itself.
 read -r recovery_app recovery_tick <mpc.pid
 recovery_bridge_pid=;recovery_bridge_tick=
 if [ "$bridge_expected" -eq 1 ];then read -r recovery_bridge_pid recovery_bridge_tick <bridge.pid;fi
 read -r recovery_owner recovery_owner_tick <follower.pid
 case "$recovery_owner:$recovery_owner_tick" in *[!0-9:]*|:*) return 1;; esac
 [ "$recovery_owner" -gt 1 ] && same_boot && [ "$(start_tick "$recovery_owner")" = "$recovery_owner_tick" ] || return 1
 recovery_profile=saved;[ "$selection_options" -eq 0 ] || recovery_profile=$controller_profile
 printf 'pending %s %s %s %s\n' "$recovery_app" "$recovery_tick" "$bridge_expected" "$recovery_profile" >"$controller_recovery"
 chmod 600 "$controller_recovery"
 kill -USR2 "$recovery_owner" || { rm -f "$controller_recovery";return 1; }
 flock -u 9
 recovery_ok=0;recovery_wait=0
 while authorized;do
  if [ "$(cat "$controller_recovery" 2>/dev/null || :)" = "ready $recovery_app $recovery_tick" ];then recovery_ok=1;break;fi
  [ "$(cat "$controller_recovery" 2>/dev/null || :)" != "failed $recovery_app $recovery_tick" ] || break
  recovery_wait=$((recovery_wait+1));[ "$recovery_wait" -le 50 ] || break;sleep .1
 done
 flock 9
 rm -f "$controller_recovery"
 [ "$recovery_ok" -eq 1 ] && authorized && identity mpc.pid /usr/bin/MPC && [ "$pid:$tick" = "$recovery_app:$recovery_tick" ] || { echo 'Explicit bridge recovery was not acknowledged by the same session generation.' >&2;return 1; }
 [ "$bridge_expected" -eq 0 ] || { identity bridge.pid "$here/mirror-input" && [ "$pid:$tick" = "$recovery_bridge_pid:$recovery_bridge_tick" ]; }
}
owner_controller_recovery(){
 [ -f "$controller_recovery" ] || return 0
 read -r recovery_state recovery_app recovery_tick recovery_expected recovery_profile <"$controller_recovery" || return 0
 [ "$recovery_state" = pending ] || return 0
 preferences_pending=0
 recovery_valid=0
 case "$recovery_expected:$recovery_profile" in 0:saved|1:saved|1:xtouch|1:xtouch-mini|1:generic|1:hui|1:lcxl3-daw|1:launchpad-x-programmer|1:launchpad-pro-mk3-programmer|1:mpk-mini-mk3|1:lpd8-mk2|1:mpk249) recovery_valid=1;; esac
 if [ "$recovery_valid" -eq 1 ] && [ "$recovery_app:$recovery_tick" = "$app_pid:$app_tick" ] && { [ "$recovery_expected" -eq 0 ] || identity bridge.pid "$here/mirror-input"; };then
  recovery_code=0;status >/dev/null 2>&1 || recovery_code=$?
  case "$recovery_code" in 0|3)
   native_preferences_watch_stop
   if native_preferences_watch_start "$recovery_profile";then
    setup=1;bridge_expected=$recovery_expected;arm_attempted=1;[ ! -f manual-adapter-unarmed.pid ] || arm_attempted=0
    for recovered_failure in 'Controller startup failed; no automatic retry.' 'Route/source unavailable at arming; no automatic retry.';do
     if printf '%s\n' "$recovered_failure" | cmp -s - generation.failed;then rm -f generation.failed;fi
    done
    printf '%s\n' ready >generation.ready
    printf 'ready %s %s\n' "$recovery_app" "$recovery_tick" >"$controller_recovery"
    return 0
   fi;;
  esac
 fi
 printf 'failed %s %s\n' "$recovery_app" "$recovery_tick" >"$controller_recovery"
 return 1
}
native_rollback=$runtime/controller-preferences.rollback
native_restore=$home/.controller-preferences.restore
native_preferences_snapshot(){
 rm -f "$native_rollback" "$native_restore"
 native_prior_missing=1
 if [ -e "$controller_preferences" ] || [ -L "$controller_preferences" ];then
  [ ! -L "$controller_preferences" ] && [ -f "$controller_preferences" ] && [ "$(stat -c '%u:%a' "$controller_preferences")" = 0:600 ] || return 1
  prior_size=$(stat -c %s "$controller_preferences")
  [ "$prior_size" -eq 288 ] || [ "$prior_size" -eq "$cap_controller_preferences" ] || return 1
  cp "$controller_preferences" "$native_rollback" && chmod 600 "$native_rollback" && cmp -s "$controller_preferences" "$native_rollback" || return 1
  native_prior_missing=0
 else
  : >"$native_rollback" && chmod 600 "$native_rollback" || return 1
 fi
}
native_preferences_discard_snapshot(){ rm -f "$native_rollback" "$native_restore"; }
native_preferences_restore(){
 restore_mode=present;[ "$native_prior_missing" -eq 0 ] || restore_mode=missing
 "$here/mirror-input" --controller-restore "$controller_preferences" "$native_rollback" "$restore_mode" || return 1
 rm -f "$native_rollback"
}
native_preferences_apply(){
 preferences_pending=0
 [ -f "$state/native-preferences.state" ] || return 0
 native_preferences_snapshot || { echo 'Could not retain the prior controller preference; active bridge retained.' >&2;return 1; }
 apply_code=0
 "$here/mirror-input" --native-preferences-apply "$state/native-preferences.state" "$controller_preferences" || apply_code=$?
 case "$apply_code" in
  0)
   # The preference is durable before changing the live bridge. This owner then
   # uses its existing bounded drain/start path and all future generations read
   # the same saved selection rather than an earlier one-session CLI override.
   prior_bridge_stopped=0
   if bridge_stop;then
    prior_bridge_stopped=1
    selection_options=0
    if bridge_start;then native_preferences_discard_snapshot;"$here/mirror-input" --native-preferences-finish "$state/native-preferences.state" active || :;return 0;fi
   fi
   if [ "$prior_bridge_stopped" -eq 1 ] && ! bridge_stop;then
    if identity bridge.pid "$here/mirror-input" || [ ! -f bridge.exit ];then "$here/mirror-input" --native-preferences-finish "$state/native-preferences.state" failed || :;echo 'Replacement controller bridge could not be stopped for rollback.' >generation.failed;return 1;fi
    # A non-zero exit is the expected replacement-start failure. With its
    # exact process gone and its receipt present, rollback may safely proceed.
   fi
   if native_preferences_restore;then
    selection_options=0
    prior_bridge_ready=0
    if [ "$prior_bridge_stopped" -eq 0 ] && identity bridge.pid "$here/mirror-input";then prior_bridge_ready=1
    elif bridge_start;then prior_bridge_ready=1
    fi
    if [ "$prior_bridge_ready" -eq 1 ] && "$here/mirror-input" --native-preferences-sync "$state/native-preferences.state" "$controller_preferences" saved;then
     "$here/mirror-input" --native-preferences-finish "$state/native-preferences.state" failed || :
     echo 'Native controller replacement failed; prior saved selection and bridge restored.' >&2
     return 0
    fi
   fi
   "$here/mirror-input" --native-preferences-finish "$state/native-preferences.state" failed || :
   echo 'Native controller replacement and prior-controller rollback failed.' >generation.failed
   return 1;;
  10) native_preferences_discard_snapshot;return 0;;
  11) native_preferences_discard_snapshot;echo 'Native controller selection request was invalid; active bridge retained.' >&2;;
  12) native_preferences_discard_snapshot;echo 'The selected external MIDI endpoint is unavailable; active bridge retained.' >&2;;
  13) native_preferences_discard_snapshot;echo 'The selected external MIDI endpoint is ambiguous; active bridge retained.' >&2;;
  14) native_preferences_discard_snapshot;echo 'Controller selection could not be saved; active bridge retained.' >&2;;
  15) native_preferences_discard_snapshot;echo 'The endpoint list changed; refresh and select the endpoint again. Active bridge retained.' >&2;;
  *) native_preferences_discard_snapshot;echo 'Native controller selection state is unavailable; active bridge retained.' >&2;;
 esac
 return 0
}
# Follows acvs's CE instances for one session and never launches. A cheap
# identity poll (no hashing): when mpc.pid names a live MPC it starts the native
# Preferences watcher, adapter and bridge once the source reports 0 or 3, then
# arms; when that MPC is gone it waits for the next. It exits on revocation.
follower(){
 exec 9>session.lock
 preferences_pending=0;preferences_watch_pid=;followed=
 trap 'preferences_pending=1' USR2
 while authorized;do
  if identity mpc.pid /usr/bin/MPC;then
   if [ "$pid $tick" != "$followed" ];then
    native_preferences_watch_stop
    followed="$pid $tick";setup=0;arm_attempted=0;intent_seen=0;initializing=0
    handoff_holds=0;[ ! -f handoff-holds ] || read -r handoff_holds <handoff-holds || handoff_holds=0
    case "$handoff_holds" in ''|*[!0-9]*) handoff_holds=0;; esac
   fi
   # Never block here: close and stop take this lock for each instance.
   if flock -n 9;then
    if authorized && identity mpc.pid /usr/bin/MPC && [ "$pid $tick" = "$followed" ];then
     read -r app_pid app_tick <mpc.pid
     if [ "$preferences_pending" -eq 1 ];then owner_controller_recovery || setup=2;fi
     if "$here/command-client" new-project-intent "$state/command.state" "$app_pid" "$app_tick" >/dev/null 2>&1;then
      if [ "$intent_seen" -eq 0 ];then stop_adapter || :;intent_seen=1;fi
     elif [ "$setup" -ne 2 ];then
      code=0;status >/dev/null 2>&1 || code=$?
      if [ "$code" -eq 6 ];then stop_adapter || :;intent_seen=1;fi
      if [ "$setup" -eq 0 ];then
       case "$code" in
        0|3)
         if native_preferences_watch_start && start_adapter && bridge_start;then setup=1;printf '%s\n' ready >generation.ready
         else setup=2;"$here/mirror-input" --native-preferences-finish "$state/native-preferences.state" failed || :;echo 'Controller startup failed; no automatic retry.' >generation.failed;fi;;
        5|1) initializing=$((initializing+1));if [ "$initializing" -gt 40 ];then setup=2;echo 'Source initialization failed; no automatic retry.' >generation.failed;fi;;
        *) setup=2;echo 'Source terminal before controller startup.' >generation.failed;;
       esac
      fi
      if [ "$setup" -eq 1 ] && [ "$preferences_pending" -eq 1 ];then native_preferences_apply || setup=2;fi
      if [ "$setup" -eq 1 ] && [ "$code" -eq 0 ] && [ "$arm_attempted" -eq 0 ] && [ "$bridge_expected" -eq 1 ];then
       arm_attempted=1
       arm_adapter || echo 'Route/source unavailable at arming; no automatic retry.' >generation.failed
      fi
     fi
    fi
    flock -u 9
   fi
  elif [ -n "$followed" ];then
   # The instance is gone; acvs's close stops its adapter and bridge.
   native_preferences_watch_stop;followed=
  fi
  sleep .5 || : # SIGUSR2 may interrupt this child; the trap owns that wakeup.
 done
 native_preferences_watch_stop
}
# Admission, under session.lock with no MPC running: archive the previous
# session, prepare the optional SD plugins, write the session identity and fork
# the follower. acvs then launches each instance and reads this session.
admit_session(){
 # Prior-boot locators cannot authorize signals. Preserve them before any
 # current-boot action, then admit a new session normally.
 # Manual start calls this where errexit does not apply: every step checks.
 if ! same_boot;then [ -f session.id ] && archive_session || return 1;fi
 if [ -f session.id ];then archive_session || return 1;fi
 # Raw state is explained only by a recorded instance, archived above.
 state_empty || { echo "Unexplained state in $state; reboot to clear it." >&2;return 1; }
 # Optional SD plugins are admitted once with MPC stopped. No card must still
 # leave the controller/Global Learn app usable.
 if [ -x "$here/plugin-setup" ];then
  if "$here/plugin-setup" prepare;then rm -f "$plugins_released";else echo 'Starting without the optional Pd/Cardinal SD bundle.' >&2;fi
 fi
 session_id="$$-$(start_tick $$)"
 printf '%s\n' "$session_id" >session.id && printf '%s\n' "$boot_id" >session.boot-id && printf '%s\n' "$here" >session.location && rm -f session.revoked "$decision" || return 1
 (follower) 9>&- >follower.log 2>&1 &
 write_identity "$!" follower.pid
}
case "$command" in
 list-midi) exec "$here/mirror-input" --list-midi;;
 status)
  if identity bridge.pid "$here/mirror-input";then echo "bridge_running pid=$pid";else echo bridge_not_running;fi
  adapter_scan || exit 1
  echo "adapter_pid=${adapter_pid:-none}; arming state is not queried"
  motor_choice=$("$here/mirror-input" --surface-status "$preferences") || { echo motor_follow_preference_invalid;exit 1; }
  echo "motor_follow=$motor_choice"
  "$here/mirror-input" --controller-status "$controller_preferences" || { echo controller_preference_invalid;exit 1; }
  [ -z "$adapter_pid" ] || route || exit 1
  [ ! -f session.revoked ] || echo session_revoked
  [ ! -f crash-guard ] || echo 'crash_guard: two CE instances ended before ready; this boot runs stock MPC'
  if follower_alive;then echo "follower pid=$follower_pid";else echo follower_not_running;fi
  # The image launcher's last decision, including any ignored add-on preload.
  if [ -f "$decision" ];then echo "last_launch: $(cat "$decision")";else echo 'last_launch: none recorded'; fi
  [ ! -f "$plugins_released" ] || echo "sd_plugins: released for Controller Mode ($(cat "$plugins_released")); the next launch with the card admits them again"
  status;;
 configure)
  if [ "$controller_profile" != xtouch ] && [ "$controller_profile" != xtouch-mini ];then
   if [ "$split_endpoint_options" -eq 1 ] && [ "$controller_profile" = hui ];then
    "$here/mirror-input" --controller-set "$controller_preferences" "--profile=$controller_profile" "--input-client=$input_client" "--input-port=$input_port" "--input-type=$input_type" "--output-client=$output_client" "--output-port=$output_port" "--output-type=$output_type" "--hui-mode=$hui_mode"
   elif [ "$split_endpoint_options" -eq 1 ];then
    "$here/mirror-input" --controller-set "$controller_preferences" "--profile=$controller_profile" "--input-client=$input_client" "--input-port=$input_port" "--input-type=$input_type" "--output-client=$output_client" "--output-port=$output_port" "--output-type=$output_type"
   elif [ "$controller_profile" = hui ];then
    "$here/mirror-input" --controller-set "$controller_preferences" "--profile=$controller_profile" "--endpoint-client=$endpoint_client" "--endpoint-port=$endpoint_port" "--endpoint-type=$endpoint_type" "--hui-mode=$hui_mode"
   else
    "$here/mirror-input" --controller-set "$controller_preferences" "--profile=$controller_profile" "--endpoint-client=$endpoint_client" "--endpoint-port=$endpoint_port" "--endpoint-type=$endpoint_type"
   fi
  else
   "$here/mirror-input" --controller-set "$controller_preferences" "--profile=$controller_profile"
  fi
  selection_options=0
  if identity mpc.pid /usr/bin/MPC;then
   session_id=$(cat session.id);authorized || exit 1
   bridge_stop;bridge_start
   if [ -f manual-adapter-unarmed.pid ] && status >/dev/null;then arm_adapter || :;fi
   echo 'Saved controller selection applied to the active session.'
  else echo 'Saved controller selection; it will apply at the next owned session.';fi;;
 bridge-stop) bridge_stop;;
 bridge-start)
  session_id=$(cat session.id);authorized || exit 1
  bridge_start
  # Explicit recovery of a missed chooser-to-project arming attempt. This is
  # under session.lock and reuses all live route/identity/intent checks.
  if [ -f manual-adapter-unarmed.pid ] && identity bridge.pid "$here/mirror-input" && status >/dev/null;then
   arm_adapter || { echo 'Explicit bridge-start could not arm the existing adapter.' >&2;exit 1; }
   if printf '%s\n' 'Route/source unavailable at arming; no automatic retry.' | cmp -s - generation.failed;then rm -f generation.failed;fi
  fi
  if ! native_preferences_recover_owner;then
   if [ "$bridge_expected" -eq 1 ] && identity bridge.pid "$here/mirror-input" && [ "$pid:$tick" = "$recovery_bridge_pid:$recovery_bridge_tick" ];then bridge_stop || :;fi
   exit 1
  fi;;
 admit)
  # Boot admission, ordered before acvs: nothing here stops or starts acvs.
  if same_boot && [ -f session.id ] && [ ! -f session.revoked ] && follower_alive;then echo 'Existing session retained.';exit 0;fi
  identity bridge.pid "$here/mirror-input" && exit 1
  admit_session
  echo 'MCU session admitted; acvs launches MPC with the controller runtime.';;
 launch-prepare) launch_prepare;;
 plugin-release)
  # For a Controller Mode launch, before MPC switches its USB devices away.
  [ -x "$here/plugin-setup" ] || exit 0
  "$here/plugin-setup" release 9>&- >&2
  printf 'released\n' >"$plugins_released";;
 close) close_instance;;
 start)
  if same_boot && [ -f session.id ] && [ ! -f session.revoked ] && follower_alive && identity mpc.pid /usr/bin/MPC;then
   # The status is reported here, not acted on: this branch is the idempotent
   # "already running" answer and exits 0, also with an unhealthy source.
   source_code=0;status || source_code=$?
   case "$source_code" in 0|3) ;; *) echo "Session source status exit $source_code; the surface stays down until the source is healthy again.";; esac
   echo 'Existing owned session retained; no restart or signal.';exit 0
  fi
  identity bridge.pid "$here/mirror-input" && exit 1
  echo 'Save the project before starting: this explicit operation restarts MPC.'
  # acvs runs close for the departing instance, which needs this lock.
  flock -u 9
  systemctl stop acvs
  flock 9
  ! systemctl is-active --quiet acvs && ! mpc_running || { echo 'MPC or acvs still running; not admitted.' >&2;exit 1; }
  admitted=1;admit_session || admitted=0
  flock -u 9
  # acvs comes back either way: without an admitted session it runs stock.
  systemctl start acvs
  [ "$admitted" -eq 1 ] || { echo 'Session not admitted; acvs runs stock MPC.' >&2;exit 1; }
  n=0;while [ ! -f generation.ready ];do
   [ ! -f generation.failed ] || { cat generation.failed;exit 1; }
   case "$(cat "$decision" 2>/dev/null || :)" in "stock MPC"*) cat "$decision";exit 1;; esac
   [ "$(cat session.id 2>/dev/null || :)" = "$session_id" ] && [ ! -f session.revoked ] || exit 1
   n=$((n+1));[ "$n" -le 300 ] || { echo 'Session admitted; the controller is not ready yet. See mcu status.' >&2;exit 1; };sleep .1
  done
  echo 'MCU session active. Chooser clears controls; saved User Template reconnects automatically.';;
 stop)
  [ -f session.id ] || { echo 'No owned session.' >&2;exit 1; }
  # Durable revocation precedes every slow drain/stop: the follower exits and
  # every later launch is stock. MPC.settings is MPC's own; nothing is restored.
  cp session.id session.revoked
  stop_adapter || exit 1
  clean=1;bridge_stop || clean=0
  owned_mpc=0
  if identity mpc.pid /usr/bin/MPC;then owned_mpc=1;fi
  if [ "$clean" -eq 1 ] && [ "$owned_mpc" -eq 1 ];then "$here/command-client" stop "$state/command.state" "$state/volume.state" || clean=0;fi
  if [ "$clean" -ne 1 ];then
   echo 'INCOMPLETE: graceful source closure was not certified; preserving evidence before the restart.' >&2
   # Text only, in the session folder, so it survives a power-off.
   summarize_state . incomplete-command-status.json stop-incomplete-status.txt incomplete-mirror-read.json || :
   [ ! -f bridge.log ] || tail -c "$cap_log" bridge.log >incomplete-bridge.log || :
  fi
  # A timed-out bridge must be gone before the restart, not merely signaled.
  if identity bridge.pid "$here/mirror-input";then
   clean=0;kill -KILL "$pid"
   n=0;while identity bridge.pid "$here/mirror-input";do n=$((n+1));[ "$n" -le 50 ] || exit 1;sleep .1;done
  fi
  # acvs runs close for the CE instance, which needs this lock; the revoked
  # session makes the next launch stock.
  flock -u 9
  if [ -f mpc.pid ] && same_boot;then systemctl restart acvs
  elif ! systemctl is-active --quiet acvs;then systemctl start acvs
  fi
  echo "Session stopped; clean=$clean; acvs runs stock MPC; MPC settings left as they are."
  [ "$clean" -eq 1 ];;
esac
