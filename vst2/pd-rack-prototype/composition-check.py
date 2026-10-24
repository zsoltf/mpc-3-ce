#!/usr/bin/env python3
"""Check that the host contains exactly the current presentation composition."""
from pathlib import Path
import sys

kit = Path(__file__).resolve().parent
repo = kit.parent.parent
host = Path(sys.argv[1])
examples = host / "ui/examples"
for source, target in (("consumer.h", "toolkit-example.h"), ("consumer.cc", "toolkit-example.cc"),
                       ("editor-view.h", "editor-view.h"), ("editor-shell.h", "editor-shell.h"),
                       ("entry-api.h", "entry-api.h"), ("entry-wrapper.inc", "entry-wrapper.inc"),
                       ("screen-qlinks.inc", "screen-qlinks.inc"),
                       ("pd-entry.cc", "pd-entry.cc"), ("cardinal-entry.cc", "cardinal-entry.cc")):
    assert (kit / source).read_bytes() == (examples / target).read_bytes(), target
for prefix, folder, stem, app in (
    ("pd", "pd-touch-prototype", "hosted-consumer", "MpclearnPdEditorApp"),
    ("cardinal", "cardinal-touch-prototype", "full-consumer", "MpclearnCardinalEditorApp"),
):
    for extension in ("h", "cc"):
        original = (repo / "vst2" / folder / (stem + "." + extension)).read_text()
        expected = original.replace('"toolkit-example.h"', f'"{prefix}-consumer.h"')
        expected = expected.replace("MpclearnToolkitExampleApp", app)
        expected = expected.replace("mpclearn_toolkit_example_", f"mpclearn_{prefix}_editor_")
        assert expected == (examples / f"{prefix}-consumer.{extension}").read_text(), prefix
for folder, filename in (("pd-touch-prototype", "hosted-api.h"),
                         ("pd-touch-prototype", "patch-files.h"),
                         ("pd-touch-prototype", "patch-files.c"),
                         ("cardinal-touch-prototype", "full-touch-api.h")):
    assert (repo / "vst2" / folder / filename).read_bytes() == (examples / filename).read_bytes(), filename
expected = (repo / "vst2/cardinal-touch-prototype/full-host-controls.inc").read_text().replace("MpclearnToolkitExampleApp", "MpclearnCardinalEditorApp")
assert expected == (examples / "full-host-controls.inc").read_text(), "host parameter consumer"
if "--production" in sys.argv[2:]:
    # Native diagnostic builds deliberately instrument some of these sources.
    # Preserve those as trial evidence; the final package uses the plain path.
    for filename in ("track-host.cc", "track-host-entry.cc"):
        assert (kit / "track-host.cc").read_bytes() == (examples / filename).read_bytes(), filename
    assert (kit / "track-host-profile.h").read_bytes() == (examples / "track-host-profile.h").read_bytes()
    for filename in ("mpclearn-ui.cc", "include/mpclearn-ui.h", "private/image-surface.inc",
                     "private/list-drag.inc", "private/text-entry.inc",
                     "private/mpclearn-ui-private.h", "private/mpc-profile.h",
                     "private/mpc-3.9.1-profile.h", "private/mpc-3.10-profile.h"):
        assert (repo / "ui" / filename).read_bytes() == (host / "ui" / filename).read_bytes(), filename
    print("PASS: production track attachment and current toolkit sources match exactly")
    for source in (kit / "gpu").rglob("*"):
        if source.is_file() and source.suffix != '.cc':
            target = host / "ui/private/cardinal-gpu" / source.relative_to(kit / "gpu")
            assert source.read_bytes() == target.read_bytes(), str(target)
    print("PASS: normal GPU presenter composition matches canonical source")
print("PASS: entry/API sources exact; both existing consumers differ only by symbol/header namespace")
