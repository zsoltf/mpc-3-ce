#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
kit=$PWD
repo=$(cd ../.. && pwd)
location=${1:-/data/mpclearn/dev}
[ "$#" -le 1 ] || { echo 'build.sh [/data/mpclearn/dev|/usr/share/mpclearn/mcu]' >&2; exit 2; }
# Build this source tree; historical releases retain their own source snapshot.
# compose.sh is the single composition owner and also works without Git.
bash "$kit/compose.sh" "$location" "$repo"
