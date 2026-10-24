#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
kit=$PWD
repo=$(cd ../.. && pwd)
host=${1:?Pass the completed combined host stage}
cardinal=${2:?Pass the frozen full Cardinal package directory}
pd=${3:?Pass the frozen hosted Pd package directory}
python3 "$kit/composition-check.py" "$host" --production
(cd "$cardinal/cardinal-package" && shasum -a 256 -c "$cardinal/PACKAGE.sha256" >/dev/null)
(cd "$pd/pd-package" && shasum -a 256 -c "$pd/PACKAGE.sha256" >/dev/null)
cmp "$kit/../cardinal-touch-prototype/full-touch-api.h" "$cardinal/source-adapter/full-touch-api.h"
cmp "$kit/../pd-touch-prototype/hosted-api.h" "$pd/source/hosted-api.h"
cmp "$kit/../cardinal-touch-prototype/full-consumer.cc" "$cardinal/source-adapter/full-consumer.cc"
cmp "$kit/../pd-touch-prototype/hosted-consumer.cc" "$pd/source/hosted-consumer.cc"
stage=$(mktemp -d "$repo/build/pd-rack-prototype/package.XXXXXX")
mkdir -p "$stage/patch-editors-package/pd" "$stage/source/entry" "$stage/source/generated" "$stage/source/toolkit/include" "$stage/source/toolkit/private"
cp -R "$host/controllers/program-observer/package/native-preferences/"{runtime,tools} "$stage/patch-editors-package/"
cp -R "$cardinal/cardinal-package/Cardinal.vst" "$stage/patch-editors-package/"
cp "$cardinal/cardinal-package/Cardinal-LICENSE" "$stage/patch-editors-package/"
cp "$cardinal/cardinal-package/tools/full-component" "$stage/patch-editors-package/tools/"
cp -R "$pd/pd-package/"{plugins,library,assets} "$stage/patch-editors-package/pd/"
cp "$pd/pd-package/tools/hosted-component" "$stage/patch-editors-package/tools/"
cp "$cardinal/PACKAGE.sha256" "$stage/CARDINAL-INPUT.sha256"
cp "$pd/PACKAGE.sha256" "$stage/PD-INPUT.sha256"
cp -R "$kit"/* "$stage/source/entry/"
cp -R "$cardinal/source-adapter" "$stage/source/cardinal"
cp -R "$pd/source" "$stage/source/pd"
cp "$host/ui/examples/"{toolkit-example.cc,toolkit-example.h,entry-api.h,entry-wrapper.inc,editor-view.h,editor-shell.h,track-host.cc,track-host-profile.h,track-host-entry.cc,screen-qlinks.inc,pd-entry.cc,cardinal-entry.cc,pd-consumer.cc,pd-consumer.h,cardinal-consumer.cc,cardinal-consumer.h,hosted-api.h,full-touch-api.h,patch-files.h,patch-files.c} "$stage/source/generated/"
cp "$host/ui/mpclearn-ui.cc" "$stage/source/toolkit/"
cp "$host/ui/include/mpclearn-ui.h" "$stage/source/toolkit/include/"
cp "$host/ui/private/"{image-surface.inc,list-drag.inc,text-entry.inc,mpclearn-ui-private.h,mpc-profile.h,mpc-3.9.1-profile.h,mpc-3.10-profile.h} "$stage/source/toolkit/private/"
cp -R "$host/ui/private/cardinal-gpu" "$stage/source/toolkit/private/"
cp "$host/controllers/program-observer/native-preferences.cc" "$stage/source/generated/"
cp "$kit/README.md" "$stage/"
for path in "$stage/patch-editors-package/"{runtime,tools,Cardinal.vst,pd};do du -sk "$path";done > "$stage/package-sizes.log"
(cd "$stage/patch-editors-package" && find . -type f -exec shasum -a 256 {} + > "$stage/PACKAGE.sha256")
(cd "$stage/source" && find . -type f -exec shasum -a 256 {} + > "$stage/SOURCE.sha256")
echo "PD_RACK_PACKAGE=$stage"
