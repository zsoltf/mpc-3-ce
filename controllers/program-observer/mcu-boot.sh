#!/bin/sh
# Image entry points. Akai's acvs.service is the only owner of the MPC process:
# its drop-in runs `launch` for each instance and `close` after each exit.
# `launch` is stock by default; it adds the CE runtime only for an admitted
# session and otherwise execs the stock launcher. Boot `start` only admits a
# session. The flashed image owns the runtime; the single development override
# replaces it only while it verifies and matches this image. Otherwise the
# override is ignored with a message; nothing is removed.
set -eu
image=/usr/share/mpclearn/mcu
override=/data/mpclearn/dev
home=/data/mpclearn
session=$home/session
runtime=/run/mpclearn
# MPC writes this marker before quitting into Controller Mode and removes it
# when it returns to Standalone.
controller_mode=/media/az01-internal/Settings/MPC/remoteScreen
# Controller Mode switches MPC's internal USB hub (card reader, disk, audio,
# MIDI, USB-A ports) away; Akai's tool switches it back and returns at once
# when the state already matches.
usbmux=/sys/devices/platform/usb-mux/state
usbmux_switch=/usr/bin/az01-usbmux-switch
asound_cards=/proc/asound/cards
usb_snapshot=$runtime/usb-internal.cards
block_root=/sys/class/block
# The tool's SATA bridge reset line, which its EXIT trap releases.
sata_gpio=/sys/class/gpio/gpio237
sata_reset_dt=/sys/firmware/devicetree/base/pinctrl/gpio7@ff7e0000/sata-reset-asserted
owned_dir(){ [ -d "$1" ] && [ ! -L "$1" ] && [ "$(stat -c %u "$1")" = 0 ] && [ "$(stat -c %a "$1")" = 700 ]; }
image_digest(){ set -- $(sha256sum "$image/payload.sha256");printf '%s\n' "$1"; }
override_verifies(){
 owned_dir "$override" && (cd "$override" && sha256sum -c session-package.sha256 >/dev/null 2>&1) &&
 [ -f "$override/for-image" ] && [ ! -L "$override/for-image" ] && [ "$(cat "$override/for-image")" = "$(image_digest)" ]
}
ignored(){ echo "Development override $override ignored: $1; using the image runtime." >&2; }
select_location(){
 location=$image
 [ -e "$override" ] || [ -L "$override" ] || return 0
 owned_dir "$override" || { ignored 'not a root-owned 0700 folder';return 0; }
 (cd "$override" && sha256sum -c session-package.sha256 >/dev/null 2>&1) || { ignored 'package does not verify';return 0; }
 [ -f "$override/for-image" ] && [ ! -L "$override/for-image" ] && [ "$(cat "$override/for-image")" = "$(image_digest)" ] || { ignored 'installed for a different image';return 0; }
 location=$override
}
# A running session belongs to the package that started it.
session_location(){
 location=
 [ ! -f "$session/session.location" ] || IFS= read -r location <"$session/session.location" || :
 case "$location" in "$image"|"$override") ;; *) select_location;; esac
}
# One decision per MPC instance, printed as `CE LOCATION KIND` or `stock REASON`.
# It runs in a subshell where errexit does not apply, so every step checks
# explicitly; an unset variable ends only the subshell.
# A package child with its output to a file (- for the journal), killed after
# LIMIT seconds; its status, or 137. Nothing it leaves behind can hold this
# decision open.
run_bounded(){
 limit=$1 output=$2;shift 2
 if [ "$output" = - ];then "$@" >&2 & else "$@" >"$output" & fi
 child=$!
 (sleep "$limit";kill -KILL "$child") >/dev/null 2>&1 &
 watchdog=$!
 code=0;wait "$child" || code=$?
 kill "$watchdog" 2>/dev/null || :
 return "$code"
}
# The USB sound cards present now (MPC audio and MIDI, USB-A devices).
usb_cards(){ sed -n 's/^.*\]: USB-Audio - //p' "$asound_cards"; }
# A USB card reader's partition by plugin-setup's criterion for it (removable
# disk, udev ID_DRIVE_FLASH_SD=1). Only models with the USB switch wait here,
# and their devicetrees all mark the SoC's SD slot as internal storage, so the
# card is in the reader. Its mount is plugin-setup's request to edisksd, made
# while admitting the card; edisksd mounts nothing unasked.
sd_partition(){
 for part in "$block_root"/*;do
  [ -e "$part/partition" ] || continue
  disk=$(readlink -f "$part") || continue
  [ "$(cat "${disk%/*}/removable" 2>/dev/null || :)" = 1 ] || continue
  udevadm info --query=property --name="/dev/${part##*/}" 2>/dev/null | grep -qx ID_DRIVE_FLASH_SD=1 && return 0
 done
 return 1
}
# Bounded wait (STEPS x 0.2 s) for the devices themselves: the USB sound cards
# recorded on entering Controller Mode and, when SD plugins were released, an
# SD card partition.
usb_back(){
 steps=$1 n=0
 while :;do
  missing=
  if [ -s "$usb_snapshot" ];then
   while IFS= read -r card;do usb_cards | grep -Fqx -- "$card" || missing=$card;done <"$usb_snapshot"
  fi
  if [ -z "$missing" ] && [ -e "$runtime/plugins.released" ] && ! sd_partition;then missing='SD card'
  fi
  [ -n "$missing" ] || return 0
  n=$((n+1));[ "$n" -le "$steps" ] || return 1
  sleep .2
 done
}
uptime_now(){ read -r up _ </proc/uptime;printf '%s\n' "$up"; }
launch_decision(){
 usb_note=
 refuse(){ printf 'stock %s%s\n' "$1" "${usb_note:+; $usb_note}";exit 0; }
 # Controller Mode runs stock, as MPC switches its USB devices (card reader,
 # disk) away at start. CE's SD plugin view is released first, so the card
 # leaves Linux unheld; this applies whether or not CE is enabled.
 if [ -e "$controller_mode" ] || [ -L "$controller_mode" ];then
  note='Controller Mode'
  # The USB devices to wait for on the return, recorded while still internal.
  if [ "$(cat "$usbmux" 2>/dev/null || :)" = internal ];then { usb_cards >"$usb_snapshot"; } 2>/dev/null || :;fi
  if grep -q ' /run/mpclearn/plugins ' /proc/self/mountinfo 2>/dev/null || [ -e "$runtime/plugin-mount.json" ] || [ -e "$runtime/plugins.released" ];then
   session_location
   if run_bounded 20 - "$location/mcu-session.sh" plugin-release;then note="$note; SD plugins released"
   else note="$note; SD plugin release failed (status $code)";fi
  fi
  refuse "$note"
 fi
 [ ! -e "$home/disabled" ] && [ ! -L "$home/disabled" ] || refuse 'CE disabled'
 [ -f "$session/session.id" ] && [ -f "$session/session.boot-id" ] || refuse 'no admitted session'
 [ "$(cat "$session/session.boot-id")" = "$(cat /proc/sys/kernel/random/boot_id)" ] || refuse 'no session from this boot'
 [ ! -e "$session/session.revoked" ] || refuse 'session revoked'
 location=
 [ ! -f "$session/session.location" ] || IFS= read -r location <"$session/session.location" || refuse 'session location unreadable'
 case "$location" in
  "$image") ;;
  "$override") override_verifies || refuse 'development override does not verify';;
  *) refuse 'session names no admitted package';;
 esac
 [ ! -e "$session/crash-guard" ] || refuse 'crash guard: two CE instances ended early'
 if command -v systemd-inhibit >/dev/null 2>&1;then refuse 'systemd-inhibit present';fi
 dfu=$(dfu-util -l 2>/dev/null | grep 0x08000000 || :)
 [ -z "$dfu" ] || refuse 'DFU device in update mode'
 # Returning from Controller Mode: switch MPC's USB back before MPC starts, so
 # its audio, MIDI, card and USB-A devices exist at launch as after a boot and
 # the SD plugins can be admitted again now. Bounded; on failure or timeout the
 # launch goes on as before (MPC switches back itself) and says why.
 mux=$(cat "$usbmux" 2>/dev/null || :)
 if [ -n "$mux" ] && [ "$mux" != internal ];then
  started=$(uptime_now)
  if run_bounded 5 - "$usbmux_switch" internal;then
   if usb_back 75;then usb_note="USB switched back to internal, devices back after $(awk -v a="$started" -v b="$(uptime_now)" 'BEGIN{printf "%.1f",b-a}') s"
   else usb_note="USB switched back to internal, ${missing:-devices} not back after 15 s";fi
  elif [ "$code" -eq 137 ];then
   usb_note='USB switch-back timed out after 5 s'
   # Killed, the tool's EXIT trap did not release the SATA bridge reset:
   # release it as the trap does, where the tool drives that line.
   if [ ! -e "$sata_reset_dt" ] && [ -d "$sata_gpio" ];then
    if { echo 0 >"$sata_gpio/value"; } 2>/dev/null;then usb_note="$usb_note, SATA reset released"
    else usb_note="$usb_note, SATA reset release failed";fi
   fi
  else usb_note="USB switch-back failed (status $code)";fi
 fi
 # The package writes this instance's identity: the launcher PID survives exec.
 # Bounded: a stuck child is killed and the instance runs stock. The bound
 # covers one SD plugin re-admission after Controller Mode (plugin-setup
 # bounds itself to 37 s); otherwise preparing takes about a second.
 result=$runtime/launch.prepared
 rm -f "$result" || refuse 'launch-prepare result not writable'
 run_bounded 45 "$result" "$location/mcu-session.sh" launch-prepare "$launcher" || refuse "launch-prepare refused (status $code)"
 prepared=$(sed -n 1p "$result" 2>/dev/null || :)
 reason=$(sed -n 's/^reason //p' "$result" 2>/dev/null || :)
 case "$prepared" in
  'prepared cardinal'|'prepared plain') printf 'CE %s %s\n' "$location" "${prepared#prepared }";;
  *) refuse 'launch-prepare gave no result';;
 esac
 [ -z "$reason" ] || printf 'plain %s\n' "$reason"
 [ -z "$usb_note" ] || printf 'usb %s\n' "$usb_note"
}

if [ "${1:-}" = launch ];then
 shift
 # Every refusal or error before the CE exec, including a shell error or a
 # failed exec, ends here: the stock launcher with the inherited environment.
 reason='launch error'
 foreign=${LD_PRELOAD:-}
 trap 'note="stock MPC: $reason";[ -z "$foreign" ] || note="$note; add-on LD_PRELOAD passes through: $foreign";echo "mpclearn launch: $note" >&2;{ printf "%s\n" "$note" | cut -c1-512 >"$runtime/launch.decision"; } 2>/dev/null || :;exec /usr/bin/az01-launch-MPC "$@"' EXIT
 launcher=$$
 decision=$(launch_decision) || decision="stock launch decision failed (status $?)"
 plain_reason=$(printf '%s\n' "$decision" | sed -n 's/^plain //p')
 usb_note=$(printf '%s\n' "$decision" | sed -n 's/^usb //p')
 decision=$(printf '%s\n' "$decision" | sed -n 1p)
 case "$decision" in
  "CE $image cardinal"|"CE $image plain"|"CE $override cardinal"|"CE $override plain") ;;
  'stock '*) reason=${decision#stock };exit 0;;
  *) reason='unexpected launch decision';exit 0;;
 esac
 kind=${decision##* };location=${decision#CE };location=${location% *}
 note="CE MPC from $location, plugins $kind"
 [ -z "$plain_reason" ] || note="$note: $plain_reason"
 [ -z "$usb_note" ] || note="$usb_note; $note"
 [ -z "$foreign" ] || note="$note; add-on LD_PRELOAD ignored: $foreign"
 echo "mpclearn launch: $note" >&2
 { printf '%s\n' "$note" | cut -c1-512 >"$runtime/launch.decision"; } 2>/dev/null || :
 reason='CE exec failed'
 # The stock launcher's final branch (3.10 has no systemd-inhibit), with the
 # preload as the only LD_PRELOAD, consumed by the first process: MPC.
 ulimit -S -s 1024 || :
 export GLIBC_TUNABLES=glibc.malloc.hugetlb=2
 export MALLOC_ARENA_MAX=1
 if [ "$kind" = cardinal ];then
  exec setarch -R -- env LD_PRELOAD="$location/command-observer.so" MPCLEARN_CARDINAL_RESOURCES=/run/mpclearn/plugins/Cardinal.vst/resources MESA_SHADER_CACHE_DISABLE=true /usr/bin/MPC "$@"
 fi
 exec setarch -R -- env LD_PRELOAD="$location/command-observer.so" /usr/bin/MPC "$@"
fi

[ "$(id -u)" -eq 0 ] || exit 2
case "${1:-}" in
 start)
  [ "$#" -eq 1 ] || exit 2
  select_location
  # Admission only, ordered before acvs: no MPC is running and nothing here
  # stops or starts acvs. The session follower stays in this unit's cgroup.
  exec "$location/mcu-session.sh" admit;;
 close)
  [ "$#" -eq 1 ] || exit 2
  # ExecStopPost after every instance exit. Only a CE instance left an identity.
  [ -f "$session/mpc.pid" ] || exit 0
  session_location
  exec "$location/mcu-session.sh" close;;
 stop)
  # Explicit disable only: stop the session through its own package.
  [ "$#" -eq 1 ] || exit 2
  session_location || exit 2
  exec "$location/mcu" stop;;
 *) echo 'mcu-boot.sh start|stop|launch|close' >&2;exit 2;;
esac
