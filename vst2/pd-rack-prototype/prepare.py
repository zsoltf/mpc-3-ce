#!/usr/bin/env python3
"""Compose existing consumers by symbol renaming, without editing their logic."""
from pathlib import Path
import shutil
import sys
import subprocess
import os

kit = Path(__file__).resolve().parent
repo = kit.parent.parent
stage = Path(sys.argv[1])
examples = stage / "ui/examples"
for prefix, source, stem, app in (
    ("pd", repo / "vst2/pd-touch-prototype", "hosted-consumer", "MpclearnPdEditorApp"),
    ("cardinal", repo / "vst2/cardinal-touch-prototype", "full-consumer", "MpclearnCardinalEditorApp"),
):
    for extension in ("h", "cc"):
        text = (source / (stem + "." + extension)).read_text()
        text = text.replace('"toolkit-example.h"', f'"{prefix}-consumer.h"')
        text = text.replace("MpclearnToolkitExampleApp", app)
        text = text.replace("mpclearn_toolkit_example_", f"mpclearn_{prefix}_editor_")
        (examples / f"{prefix}-consumer.{extension}").write_text(text)
for filename in ("entry-api.h", "entry-wrapper.inc", "pd-entry.cc", "cardinal-entry.cc", "editor-view.h", "editor-shell.h"):
    shutil.copyfile(kit / filename, examples / filename)
for filename in ("hosted-api.h", "patch-files.h", "patch-files.c"):
    shutil.copyfile(repo / "vst2/pd-touch-prototype" / filename, examples / filename)
shutil.copyfile(repo / "vst2/cardinal-touch-prototype/full-touch-api.h", examples / "full-touch-api.h")
text = (repo / "vst2/cardinal-touch-prototype/full-host-controls.inc").read_text()
(examples / "full-host-controls.inc").write_text(text.replace("MpclearnToolkitExampleApp", "MpclearnCardinalEditorApp"))
path = stage / "controllers/program-observer/native-preferences-build.sh"
text = path.read_text()
anchor = 'docker run --rm --network none -v "$PWD:/src:ro"'
assert text.count(anchor) == 1
text = text.replace(anchor, '''docker run --rm --network none -v "$(cd ../../ui && pwd):/ui:ro" -v "$PWD/package/mirror-input:/model:ro" -v "$PWD/$out:/out" mpclearn-editor-build bash -ec '
flags="-include /model/model-build-profile.h -march=armv7-a -marm -mfloat-abi=hard -mfpu=neon -O2 -Wall -Wextra -Werror -fPIC -fvisibility=hidden"
for editor in pd cardinal; do
 arm-linux-gnueabihf-g++ -std=c++14 $flags -fexceptions -funwind-tables -c /ui/examples/$editor-entry.cc -o /out/$editor-entry.o
done
arm-linux-gnueabihf-gcc -std=c11 $flags -Wno-misleading-indentation -c /ui/examples/patch-files.c -o /out/patch-files.o
'
''' + anchor)
anchor = '/out/toolkit-example.o /out/plugin-controls-screen.o /usr/arm-linux-gnueabihf/lib/libstdc++.so.6'
assert text.count(anchor) == 1
text = text.replace(anchor, '/out/toolkit-example.o /out/plugin-controls-screen.o /out/pd-entry.o /out/cardinal-entry.o /out/patch-files.o /usr/arm-linux-gnueabihf/lib/libstdc++.so.6')
path.write_text(text)
if "--track" in sys.argv[2:]:
    subprocess.run([sys.executable,str(kit / "prepare-track-host.py"),str(stage)],check=True)
    subprocess.run([sys.executable,str(kit / "prepare-gpu.py"),str(stage)],check=True)
    if os.environ.get("MPCLEARN_TEXT_DIAGNOSTIC") == "1":
        subprocess.run([sys.executable,str(kit / "prepare-text-diagnostic.py"),str(stage)],check=True)
else:
    launcher = stage / "controllers/program-observer/native-preferences.cc"
    text = launcher.read_text()
    assert text.count('"UI TOOLKIT EXAMPLE"') == 1
    launcher.write_text(text.replace('"UI TOOLKIT EXAMPLE"', '"PATCH EDITORS"'))
    subprocess.run([sys.executable,str(kit / "prepare-navigation.py"),str(launcher)],check=True)
print("Prepared symbol-separated Pd and Cardinal consumers")
