#!/usr/bin/env python3
"""Apply the operator-selected four-voice default to bundled Etherpad."""
import json
from pathlib import Path
import sys
p = Path(sys.argv[1]) / "patches/examples/DRMR_-_Etherpad.vcv"
graph = json.loads(p.read_text())
modules = [m for m in graph["modules"] if m["plugin"] == "Cardinal" and m["model"] == "HostMIDI"]
assert len(modules) == 1
assert modules[0]["data"]["channels"] in (4, 8)
modules[0]["data"]["channels"] = 4
p.write_text(json.dumps(graph, indent=2) + "\n")
print("Bundled Etherpad: four MIDI voices; other patch settings preserved")
