#!/bin/sh
# Exercise production native UI construction/callback/teardown with host substitutes.
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d /tmp/mpclearn-native-ui-check.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
profile_flags=
case "${MPCLEARN_FIRMWARE:-3.9.1}" in
 3.9.1) ;;
 3.10)
  grep -Fqx '#define MODEL_FIRMWARE_310 1' package/mirror-input/model-build-profile.h
  profile_flags='-Ipackage/native-preferences -Ipackage/mirror-input -include package/mirror-input/model-build-profile.h';;
 *) echo 'Unsupported explicit firmware profile' >&2;exit 2;;
esac
clang++ $profile_flags --target=arm-linux-gnueabihf -march=armv7-a -marm -mfloat-abi=hard -mfpu=neon \
 -std=c++14 -O2 -Wall -Wextra -Werror -fexceptions -funwind-tables \
 -DMPCLEARN_NATIVE_UI_EXAMPLE=0 -c native-preferences-test.cc -o "$tmp/native-preferences-test.o"
clang++ $profile_flags --target=arm-linux-gnueabihf -march=armv7-a -marm -mfloat-abi=hard -mfpu=neon \
 -std=c++14 -O2 -Wall -Wextra -Werror -fexceptions -funwind-tables \
 -DMPCLEARN_NATIVE_UI_EXAMPLE=1 -c native-preferences-test.cc -o "$tmp/native-preferences-example-test.o"
clang++ $profile_flags --target=arm-linux-gnueabihf -march=armv7-a -marm -mfloat-abi=hard -mfpu=neon \
 -std=c++14 -O2 -Wall -Wextra -Werror -fexceptions -funwind-tables \
 -c ../../ui/mpclearn-ui.cc -o "$tmp/mpclearn-ui.o"
clang++ $profile_flags --target=arm-linux-gnueabihf -march=armv7-a -marm -mfloat-abi=hard -mfpu=neon \
 -std=c++14 -O2 -Wall -Wextra -Werror -fexceptions -funwind-tables \
 -c ../../ui/screens/global-midi-learn-groups.cc -o "$tmp/global-midi-learn-groups.o"
clang++ $profile_flags --target=arm-linux-gnueabihf -march=armv7-a -marm -mfloat-abi=hard -mfpu=neon \
 -std=c++14 -O2 -Wall -Wextra -Werror -fexceptions -funwind-tables \
 -c ../../ui/screens/global-midi-learn-screen.cc -o "$tmp/global-midi-learn-screen.o"
clang++ $profile_flags --target=arm-linux-gnueabihf -march=armv7-a -marm -mfloat-abi=hard -mfpu=neon \
 -std=c++14 -O2 -Wall -Wextra -Werror -fexceptions -funwind-tables \
 -c ../../ui/examples/toolkit-example.cc -o "$tmp/toolkit-example.o"
docker run --rm --network none -v "$tmp:/out" mpclearn-controls-build sh -ec '
arm-linux-gnueabihf-gcc /out/native-preferences-test.o /out/mpclearn-ui.o /out/global-midi-learn-groups.o /out/global-midi-learn-screen.o /out/toolkit-example.o /usr/arm-linux-gnueabihf/lib/libstdc++.so.6 -o /out/native-preferences-test
arm-linux-gnueabihf-gcc /out/native-preferences-example-test.o /out/mpclearn-ui.o /out/global-midi-learn-groups.o /out/global-midi-learn-screen.o /out/toolkit-example.o /usr/arm-linux-gnueabihf/lib/libstdc++.so.6 -o /out/native-preferences-example-test
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /out/native-preferences-test
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /out/native-preferences-example-test
'
