#!/usr/bin/env python3
"""Specialize the unchanged Bogaudio sample loop for four final module types.

The shared loop retains every operation, modulation interval and channel
transition. Only the receiver type changes, letting the compiler remove
virtual calls and empty hooks. Modules with intervening process overrides
(notably Matrix expanders) must keep their existing dispatch path.
"""
import argparse
from pathlib import Path
import re


def prepare(source):
    root = source / "plugins/BogaudioModules/src"
    cpp, hpp = root / "module.cpp", root / "module.hpp"
    original, header = cpp.read_text(), hpp.read_text()
    begin = "void BGModule::process(const ProcessArgs& args) {"
    wrapper = begin + "\n\tprocessTyped<BGModule>(args);\n}\n"
    needle = "\tvoid process(const ProcessArgs& args) override;"
    marker = "\ttemplate<class T> void processTyped(const ProcessArgs& args) {"
    types = {"VCO": "VCOBase", "VCF": "BGModule",
             "VCM": "DisableOutputLimitModule", "LFO": "LFOBase"}
    hooks = ("onReset", "onSampleRateChange", "modulateAlways", "processAlways",
             "active", "channels", "channelsChanged", "addChannel", "removeChannel",
             "modulate", "modulateChannel", "processAll", "processChannel",
             "postProcess", "postProcessAlways")

    # These are the complete intermediate base chains at the pinned revision.
    # Never bypass an inherited process() such as ExpandableModule::process().
    for name, base in (("vco_base", "VCOBase"), ("lfo_base", "LFOBase"),
                       ("disable_output_limit", "DisableOutputLimitModule")):
        text = (root / (name + ".hpp")).read_text()
        if text.count(f"struct {base} : BGModule {{") != 1 or re.search(r"\bprocess\s*\(", text):
            raise SystemExit("Pinned dispatch base mismatch: " + name)

    declarations = {}
    for name, base in types.items():
        before = f"struct {name} : {base} {{"
        after = (f"struct {name} final : {base} {{\n"
                 f"\tvoid process(const ProcessArgs& args) override {{ processTyped<{name}>(args); }}")
        declarations[root / (name + ".hpp")] = before, after

    if wrapper in original:
        if header.count(marker) != 1 or any(path.read_text().count(after) != 1
                                           for path, (_, after) in declarations.items()):
            raise SystemExit("Incomplete existing dispatch preparation")
        print("Full Bogaudio dispatch preparation already present")
        return
    if original.count(begin) != 1 or header.count(needle) != 1 or marker in header:
        raise SystemExit("Pinned BGModule dispatch seam mismatch")
    start = original.index(begin)
    stop = original.index("\nvoid BGModule::setSkin(", start)
    body = original[start + len(begin):stop]
    if not body.endswith("}\n"):
        raise SystemExit("Pinned BGModule process body mismatch")
    body = body[:-2]
    for name in hooks:
        body, count = re.subn(r"\b" + name + r"\(", "self->" + name + "(", body)
        if count != 1:
            raise SystemExit("Pinned BGModule hook mismatch: " + name)
    definition = ("\n\t// Keep one sample loop; final receiver types allow devirtualization.\n"
                  + marker + "\n\t\tT* self = static_cast<T*>(this);\n")
    definition += "\n".join("\t" + line if line else "" for line in body.strip("\n").splitlines())
    definition += "\n\t}\n"
    pending = {hpp: header.replace(needle, needle + definition),
               cpp: original[:start] + wrapper + original[stop:]}
    for path, (before, after) in declarations.items():
        text = path.read_text()
        if text.count(before) != 1 or re.search(r"\bprocess\s*\(", text):
            raise SystemExit("Pinned final module mismatch: " + path.name)
        pending[path] = text.replace(before, after)
    # Validate every seam before modifying any file.
    for path, text in pending.items():
        path.write_text(text)
    print("Prepared VCO/VCF/VCM/LFO final dispatch; shared sample loop unchanged")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    prepare(parser.parse_args().source)
