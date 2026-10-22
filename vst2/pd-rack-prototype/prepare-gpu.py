#!/usr/bin/env python3
"""Compose the admitted Cardinal presenter into the existing track host."""
from pathlib import Path
import shutil
import sys

kit = Path(__file__).resolve().parent
stage = Path(sys.argv[1])
shutil.copytree(kit / 'gpu', stage / 'ui/private/cardinal-gpu',
                ignore=shutil.ignore_patterns('*.cc'))

def edit(name, before, after, count=1):
    path = stage / name
    text = path.read_text()
    assert text.count(before) == count, (name, before[:70], text.count(before))
    path.write_text(text.replace(before, after, 1))

host = 'controllers/program-observer/'
edit(host + 'native-preferences.cc', '#include "native-preferences.h"',
     '#include "native-preferences.h"\n#include "../../ui/private/cardinal-gpu/presenter.inc"')
edit(host + 'native-preferences.cc', ' increment(&state->hook_entries);',
     ' if(kind>=10&&kind<=13){cardinal_presenter_hook(saved,kind);return;}\n increment(&state->hook_entries);')
path = stage / host / 'native-preferences.cc'
path.write_text(path.read_text() + '\nextern "C" uintptr_t cardinal_presenter_root_candidate(){return global_capture.ui_thread_id==(uint32_t)syscall(224)?uintptr_t(global_capture.app):0;}\n')

# These exact native owners and displaced instructions were admitted in presenter9.
ranges = [
 (0x0332b5b4,0x6144,'TMI constructor member ownership'),
 (0x033316f8,0xb14,'TMI child destruction and destructor thunks'),
 (0x035b4e28,0xe4,'Akai overlay container constructor'),
 (0x035d88a0,0x81c,'Akai overlay stack show hide'),
 (0x03678414,0xcd4,'Function panel bounds and animation'),
 (0x03676f84,0x1490,'Function panel drag'),
 (0x035d8120,0x3a4,'Akai keyboard provider'),
 (0x00c356e8,0x62c,'DRM atomic submit enclosing function'),
 (0x00c264b0,0xf8,'true DRM flip callback'),
 (0x00c63458,0x3c0,'DrmScreen association constructor'),
 (0x00b392f8,0xe0,'Modal active count and singleton'),
 (0x00bc327c,0x130,'Modal item publication'),
 (0x00c30fa0,0x158,'Modal deleted-component cancellation'),
 (0x03679fe0,0x1fc,'Shortcut panel constructor'),
 (0x03678f3c,0x128,'Shared panel constructor'),
 (0x0332f2b4,0x50,'TMI shortcut ownership'),
 (0x0332f628,0x20,'TMI shortcut parent'),
 (0x03332680,0x30,'TMI shortcut layout ordering'),
 (0x036759c0,0x258,'Shared panel collapsed callbacks'),
]
pointers = [
 (0x068d7bf8,0x00e46700,'Engine root'),
 (0x0699efd0,0x033316f8,'TMI'),
 (0x069c3090,0x035b4528,'Overlay container'),
 (0x069c3138,0x035b4660,'Overlay container secondary'),
 (0x069a03c4,0x0334d900,'Function panel'),
 (0x0688e7a0,0x00c2c3b8,'DrmScreen complete destructor'),
 (0x06a8a990,0x00c264b0,'true page flip callback GOT'),
 (0x0688ea3c,0x00b2ed44,'Modal manager primary vptr'),
 (0x0688ea50,0x00b2eea4,'Modal manager second vptr'),
 (0x0688bf0c,0x00c310f8,'Modal item destructor'),
 (0x069cc03c,0x03679134,'Shortcut panel vptr'),
]
edit(host + 'native-preferences-prepare.py', 'for address,length,name in ranges:',
     'ranges += ' + repr(ranges) + '\nfor address,length,name in ranges:', 2)
edit(host + 'native-preferences-prepare.py', 'for slot,target,name in pointers:',
     'pointers += ' + repr(pointers) + '\nfor slot,target,name in pointers:', 2)
edit(host + 'native-preferences-prepare.py', 'site_bytes=[]', '''sites += [
 (0x00c359b8,'0430a0e10a20a0e1','DRM precommit'),
 (0x00c359c8,'104f6fe1a442a0e1','DRM postcommit'),
 (0x00c264c8,'28409de50020a0e3','DRM true event'),
 (0x03331700,'0040a0e1035a80e2','TMI before child retirement'),
]
site_bytes=[]''')
edit(host + 'native-preferences-prepare.py', "'#define NATIVE_PREFERENCES_SITE_COUNT 10u'",
     "'#define NATIVE_PREFERENCES_SITE_COUNT 14u'")
path = stage / host / 'native-preferences-build.sh'
text = path.read_text().replace('-std=c++14 ', '-std=c++14 -DMPCLEARN_CARDINAL_GPU=1 ')
text = text.replace('-c /ui/examples/${editor}-entry.cc', '-I/ui/private/cardinal-gpu/include -c /ui/examples/${editor}-entry.cc')
path.write_text(text)
print('Prepared normal Cardinal GPU presenter: exact native admission, no trial clock or output')
