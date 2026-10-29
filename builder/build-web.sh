#!/bin/sh
set -eu
cd "$(dirname "$0")"
rm -rf dist
mkdir dist
GOOS=js GOARCH=wasm go build -buildvcs=false -trimpath -ldflags '-s -w' -o dist/mpclearn-installer.wasm .
cp "$(go env GOROOT)/lib/wasm/wasm_exec.js" dist/wasm_exec.js
cp web/index.html web/builder.html web/guide.html web/launchpad-guide.html web/mini-guide.html web/xl3-guide.html web/worker.js web/site.css web/controller-guide.css web/controller-guide.js web/favicon.svg web/og.png web/xtouch-cheatsheet.md LICENSE THIRD_PARTY_NOTICES.md GO-LICENSE.txt XZ-LICENSE.txt X-CRYPTO-LICENSE.txt dist/
cp -R web/img dist/img

cp ../docs/controller-programs.md dist/CONTROLLERS.md
cp ../firmware/plugin-setup/README.md dist/PLUGIN-INSTALL.md
cp ../docs/releases/v0.4.3.md dist/RELEASE-NOTES.md
