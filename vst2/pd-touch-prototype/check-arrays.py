"""Compare actual Pd writer output with unchanged upstream table policy/data."""
import math
import pathlib
import re
import sys

def arrays(path):
    text = pathlib.Path(path).read_text()
    result = {}
    current = None
    for message in re.split(r"(?<!\\);", text):
        fields = message.split()
        if fields[:2] == ["#X", "array"]:
            current = {"header": fields[2:], "data": {}}
            result[fields[2]] = current
        elif fields[:1] == ["#A"] and current is not None and fields[1].isdigit():
            start = int(fields[1])
            current["data"].update((start + i, float(v)) for i, v in enumerate(fields[2:]))
    return result

source = pathlib.Path(sys.argv[1])
output = pathlib.Path(sys.argv[2])
for filename, saved in [("B02.two-wavetables.pd", "arrays-B02.pd"),
                        ("B07.sampler.scratch.pd", "arrays-B07.pd")]:
    before, after = arrays(source / filename), arrays(output / saved)
    assert before.keys() == after.keys()
    for name, original in before.items():
        actual = after[name]
        assert original["header"] == actual["header"], (name, "array name/size/type/save/style changed")
        assert original["data"].keys() == actual["data"].keys(), (name, "persistence policy changed")
        changed = [i for i, v in original["data"].items()
                   if not math.isclose(v, actual["data"][i], rel_tol=1e-5, abs_tol=1e-6)]
        if filename.startswith("B02"):
            assert changed and all(78 <= i <= 101 for i in changed), (name, changed)
            assert len(actual["data"]) == 259
            print(f"PASS {name}: same header/persisted259; changed indices{min(changed)}..{max(changed)} only; actual[85]={actual['data'][85]}")
        else:
            assert not actual["data"] and not changed
            print(f"PASS {name}: unchanged non-save header and no embedded #A values")
