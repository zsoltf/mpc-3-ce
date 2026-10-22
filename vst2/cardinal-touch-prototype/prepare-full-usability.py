#!/usr/bin/env python3
"""Restore saved PatchMaster control types before loading parameter values."""
from pathlib import Path
import sys
root=Path(sys.argv[1])
def edit(path,old,new):
 p=root/path;s=p.read_text()
 if s.count(old)!=1:raise SystemExit('Pinned usability seam mismatch: '+path)
 p.write_text(s.replace(old,new))

# Rack's widget-before-load order is required by other bundled modules. Only
# PatchMaster knows that saved tile metadata changes its parameter snap policy.
edit('plugins/MindMeldModular/src/PatchSet/PatchMaster.cpp',
 '\tvoid dataFromJson(json_t *rootJ) override {',
 '''\tvoid fromJson(json_t *rootJ) override {
		// Establish the saved control type before Module restores parameters.
		// The initial widget can still describe the default momentary button.
		json_t* dataJ = json_object_get(rootJ, "data");
		json_t* tilesJ = dataJ ? json_object_get(dataJ, "tileInfos") : nullptr;
		for (int t = 0; t < NUM_CTRL; ++t) {
			json_t* tileJ = tilesJ ? json_array_get(tilesJ, t) : nullptr;
			uint8_t info = tileJ ? json_integer_value(tileJ) : tileInfos.infos[t];
			paramQuantities[TILE_PARAMS + t]->snapEnabled = isCtlrAButton(info);
		}
		Module::fromJson(rootJ);
	}

	void dataFromJson(json_t *rootJ) override {''')
print('Prepared PatchMaster saved tile snap policy before parameter restoration')
