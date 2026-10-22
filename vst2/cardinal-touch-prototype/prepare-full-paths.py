#!/usr/bin/env python3
"""Keep all canonical Cardinal transient writers in the bounded trial tmpfs."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path)
args = parser.parse_args()

def edit(relative, replacements):
    path = args.source / relative
    source = path.read_text()
    for old, new in replacements:
        if source.count(old) != 1:
            raise SystemExit("Pinned path mismatch: " + str(path) + ": " + old)
        source = source.replace(old, new)
    path.write_text(source)

edit("src/CardinalPlugin.cpp", [
    ("const std::string tmp = rack::system::getTempDirectory();",
     'const std::string tmp = "/tmp/mpclearn-cardinal-touch/autosaves";\n            rack::system::createDirectories(tmp);'),
])
edit("src/CardinalCommon.cpp", [
    ('    if (asset::systemDir.empty())\n    {\n        if (const char* const bundlePath',
     '    if (const char* const resources = std::getenv("MPCLEARN_CARDINAL_RESOURCES"))\n'
     '    {\n'
     '        asset::systemDir = resources;\n'
     '        asset::bundlePath = system::join(asset::systemDir, "PluginManifests");\n'
     '    }\n\n'
     '    if (asset::systemDir.empty())\n    {\n        if (const char* const bundlePath'),
    ('asset::userDir = system::join(getSpecialDir(kSpecialDirDocuments), "Cardinal");',
     'asset::userDir = "/tmp/mpclearn-cardinal-touch/user";'),
    ('asset::configDir = system::join(getSpecialDir(kSpecialDirConfig), "Cardinal");',
     'asset::configDir = "/tmp/mpclearn-cardinal-touch/config";'),
])
print("Canonical user/config/autosave paths use the bounded trial tmpfs root")
