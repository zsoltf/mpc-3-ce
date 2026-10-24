package main

import "embed"

//go:embed resources/ssh/manifest.json resources/ssh/patch.bin resources/no-ssh/manifest.json resources/no-ssh/patch.bin resources/diagnostics/manifest.json resources/diagnostics/patch.bin resources/310/ssh/manifest.json resources/310/ssh/patch.bin resources/310/no-ssh/manifest.json resources/310/no-ssh/patch.bin resources/310/diagnostics/manifest.json resources/310/diagnostics/patch.bin
var assets embed.FS
