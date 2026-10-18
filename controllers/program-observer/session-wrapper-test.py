#!/usr/bin/env python3
"""Container-only production session-owner test; native MPC, protocol, route and
ALSA children are substitutes. Real shell/flock/process identity/wait/archive
paths run. A pre-owner-lock pause and smaller history constants are the only
injections. Run as root in a container with an init and a tmpfs /run:

  docker run --rm --init --network none --tmpfs /run \\
    -v "$PWD/controllers/program-observer:/src:ro" mpclearn-build:local \\
    python3 /src/session-wrapper-test.py

With --busybox ROOTFS (an ext4 root image mounted read-only in the container),
the session scripts run under that image's BusyBox shell and applets through
qemu-arm, as on the device.
"""
import ctypes, hashlib, json, mmap, os, pathlib, platform, shutil, signal, struct, subprocess, sys, time
P = pathlib.Path
assert P('/.dockerenv').exists() and os.getuid() == 0
home = P('/data/mpclearn'); dev = home/'dev'; session = home/'session'; history = home/'history'
image = P('/usr/share/mpclearn/mcu')
runtime = P('/run/mpclearn'); state = runtime/'state'
settings = P('/media/az01-internal/Settings/MPC/MPC.settings')
settings.parent.mkdir(parents=True); settings.write_text('original settings\n')
assert subprocess.run(['stat', '-f', '-c', '%T', '/run'], capture_output=True, text=True).stdout.strip() == 'tmpfs', 'run with --tmpfs /run'
NAMES = 'command-observer.so command-client mirror-input mirror-read config.h mcu-session.sh mcu mpclearn-controls main-button'.split()
EXECUTABLES = 'command-client mirror-input mirror-read mcu-session.sh mcu mpclearn-controls main-button'.split()

shell = '/bin/sh'
env = dict(os.environ)
if '--busybox' in sys.argv:
    rootfs = sys.argv[sys.argv.index('--busybox')+1]
    device = P('/tmp/device-root'); device.mkdir()
    for part in ('/usr', '/lib'):
        subprocess.run(['debugfs', '-R', f'rdump {part} {device}', rootfs], check=True, capture_output=True)
    busybox = device/'usr/bin/busybox.nosuid'
    bb = P('/tmp/bb'); bb.mkdir()
    for applet in 'sh awk basename cat chmod chown cmp cp cut dirname env find flock grep head id kill ls mkdir mkfifo mv printf readlink rm sed sha256sum sleep sort stat tail test touch tr wc'.split():
        # Docker Desktop's binfmt handler does not preserve the applet name
        # when it follows a symlink to this ARM BusyBox.  In particular,
        # sha256sum then returns a different digest even though an explicit
        # BusyBox applet invocation reads the same bytes correctly.  Preserve
        # argv[0] explicitly so this lane tests the device shell and applets.
        wrapper = bb/applet
        wrapper.write_text(
            '#!/bin/sh\n'
            f'exec qemu-arm-static -L {device} -0 {applet} {busybox} "$@"\n')
        wrapper.chmod(0o700)
    shell = str(bb/'sh')
    env['QEMU_LD_PREFIX'] = str(device)
    env['PATH'] = f'{bb}:' + env['PATH']
    assert 'BusyBox' in subprocess.run([shell, '-c', 'stat --help 2>&1; true'], env=env, capture_output=True, text=True).stdout
MODE = 'busybox' if shell != '/bin/sh' else 'host'
# Summary heads are read with dd in whole 4096-byte blocks.
for line in P('/src/mcu-session.sh').read_text().splitlines():
    if line.startswith('cap_') and '_head=' in line: assert int(line.split('=')[1]) % 4096 == 0, line

def script_text(path, replacements=()):
    text = P(path).read_text()
    for a, b in replacements:
        assert a in text, a
        text = text.replace(a, b)
    return text.replace('#!/bin/sh\n', f'#!{shell}\n', 1)

PAUSE = ('owner(){\n exec 9>session.lock', 'owner(){\n while [ -f pause-owner ];do sleep .05;done\n exec 9>session.lock')
PRE_IDENTITY_FAILURE = (
    'write_identity(){ p=$1;[ -r "/proc/$p/stat" ] || return 1;',
    'write_identity(){ p=$1;if [ "$2" = bridge.pid ] && [ -f bridge-pre-identity-fail ];then rm -f bridge-pre-identity-fail;sleep .1;return 1;fi;[ -r "/proc/$p/stat" ] || return 1;')
def session_source(**constants):
    replacements = [PAUSE, PRE_IDENTITY_FAILURE]
    for name, value in constants.items():
        line = next(l for l in P('/src/mcu-session.sh').read_text().splitlines() if l.startswith(name+'='))
        replacements.append((line+'\n', f'{name}={value}\n'))
    return script_text('/src/mcu-session.sh', replacements)

P('/tmp/session-stub.c').write_text(r'''#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
static int traced_fsync(int);
#define fsync traced_fsync
#include "/src/surface-preferences.h"
#include "/src/mcu-controller-preferences.h"
#include "/src/native-preferences.h"
#undef fsync
static int traced_fsync(int fd){struct stat st;if(fstat(fd,&st))return -1;int trace=open("restore-fsync.trace",O_WRONLY|O_CREAT|O_APPEND,0600);if(trace>=0){dprintf(trace,"%s\n",S_ISDIR(st.st_mode)?"directory":"file");close(trace);}return (int)syscall(SYS_fsync,fd);}
static volatile sig_atomic_t stopped;
static void stop(int sig){(void)sig;stopped=1;}
static void arm(int sig){(void)sig;int fd=open("armed",O_WRONLY|O_CREAT|O_APPEND,0600);if(fd>=0){write(fd,"1",1);close(fd);}}
static const char*endpoint_type(unsigned type){return type==MCU_ENDPOINT_KERNEL?"kernel":type==MCU_ENDPOINT_USER?"user":"any";}
static const char*hui_mode(unsigned policy){return policy==MCU_HUI_MOTORIZED?"motorized":"pickup";}
static int controller_set(int argc,char **argv){const McuProfile*profile=NULL;const char*ic=NULL,*ip=NULL,*oc=NULL,*op=NULL;unsigned it=1,ot=1,policy=MCU_HUI_NONE,legacy=0,split=0;
 for(int i=3;i<argc;i++){if(!strncmp(argv[i],"--profile=",10))profile=mcu_profile_named(argv[i]+10);else if(!strncmp(argv[i],"--endpoint-client=",18)){ic=oc=argv[i]+18;legacy=1;}else if(!strncmp(argv[i],"--endpoint-port=",16)){ip=op=argv[i]+16;legacy=1;}else if(!strcmp(argv[i],"--endpoint-type=kernel")){it=ot=MCU_ENDPOINT_KERNEL;legacy=1;}else if(!strcmp(argv[i],"--endpoint-type=user")){it=ot=MCU_ENDPOINT_USER;legacy=1;}else if(!strcmp(argv[i],"--endpoint-type=any")){it=ot=MCU_ENDPOINT_ANY;legacy=1;}else if(!strncmp(argv[i],"--input-client=",15)){ic=argv[i]+15;split=1;}else if(!strncmp(argv[i],"--input-port=",13)){ip=argv[i]+13;split=1;}else if(!strcmp(argv[i],"--input-type=kernel")){it=MCU_ENDPOINT_KERNEL;split=1;}else if(!strcmp(argv[i],"--input-type=user")){it=MCU_ENDPOINT_USER;split=1;}else if(!strcmp(argv[i],"--input-type=any")){it=MCU_ENDPOINT_ANY;split=1;}else if(!strncmp(argv[i],"--output-client=",16)){oc=argv[i]+16;split=1;}else if(!strncmp(argv[i],"--output-port=",14)){op=argv[i]+14;split=1;}else if(!strcmp(argv[i],"--output-type=kernel")){ot=MCU_ENDPOINT_KERNEL;split=1;}else if(!strcmp(argv[i],"--output-type=user")){ot=MCU_ENDPOINT_USER;split=1;}else if(!strcmp(argv[i],"--output-type=any")){ot=MCU_ENDPOINT_ANY;split=1;}else if(!strcmp(argv[i],"--hui-mode=pickup"))policy=MCU_HUI_PICKUP;else if(!strcmp(argv[i],"--hui-mode=motorized"))policy=MCU_HUI_MOTORIZED;else return 2;}
 if(!profile||legacy&&split||profile==&mcu_profile_hui&&((it!=MCU_ENDPOINT_KERNEL&&it!=MCU_ENDPOINT_USER)||(ot!=MCU_ENDPOINT_KERNEL&&ot!=MCU_ENDPOINT_USER)))return 2;if(profile==&mcu_profile_hui&&!policy)policy=MCU_HUI_PICKUP;else if(profile!=&mcu_profile_hui&&policy)return 2;McuEndpoint input={.profile=profile,.client=ic?ic:profile->default_client,.port=ip?ip:profile->default_port,.physical_only=it},output={.profile=profile,.client=oc?oc:profile->default_client,.port=op?op:profile->default_port,.physical_only=ot};return mcu_controller_preferences_write(argv[2],&input,&output,policy)?0:2;}
static NativePreferencesState *native_state(const char *path){int fd=open(path,O_RDWR|O_NOFOLLOW);if(fd<0)return MAP_FAILED;struct stat st;void*p=fstat(fd,&st)||st.st_size!=NATIVE_PREFERENCES_BYTES?MAP_FAILED:mmap(NULL,NATIVE_PREFERENCES_BYTES,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);close(fd);if(p==MAP_FAILED)return p;NativePreferencesState*s=p;if(s->magic!=NATIVE_PREFERENCES_MAGIC||s->version!=NATIVE_PREFERENCES_VERSION){munmap(p,NATIVE_PREFERENCES_BYTES);return MAP_FAILED;}return s;}
static void native_endpoint(NativePreferencesState*s,const McuControllerPreference*p){const McuEndpoint*e=&p->endpoint;size_t cn=strlen(e->client),pn=strlen(e->port);memset(s->endpoint_client,0,128);memset(s->endpoint_port,0,128);memcpy(s->endpoint_client,e->client,cn);memcpy(s->endpoint_port,e->port,pn);s->endpoint_client_length=cn;s->endpoint_port_length=pn;s->saved_endpoint_type=e->physical_only;s->saved_hui_policy=p->hui_policy;}
static void native_inventory(NativePreferencesState*s){s->inventory_revision++;memset(s->endpoints,0,sizeof(s->endpoints));s->endpoints[0]=(NativePreferencesEndpoint){.client_number=30,.port_number=1,.endpoint_type=NATIVE_ENDPOINT_KERNEL};strcpy((char*)s->endpoints[0].client,"Kernel HUI");strcpy((char*)s->endpoints[0].port,"DAW");s->endpoints[1]=(NativePreferencesEndpoint){.client_number=31,.port_number=3,.endpoint_type=NATIVE_ENDPOINT_USER};strcpy((char*)s->endpoints[1].client,"User HUI");strcpy((char*)s->endpoints[1].port,"DAW");s->inventory_count=2;s->inventory_generation++;s->inventory_status=NATIVE_PREFERENCES_ACTIVE;s->inventory_revision++;}
static void native_collection(NativePreferencesState*s,const McuControllerCollection*c){s->collection_revision++;s->collection_count=c->count;s->collection_generation++;s->active_controller_index=NATIVE_CONTROLLER_NEW;s->active_profile=s->active_endpoint_type=s->active_hui_policy=0;s->saved_profile=c->count?mcu_controller_profile_id(c->records[0].input.profile):0;memset(s->controllers,0,sizeof(s->controllers));if(c->count)native_endpoint(s,c->records);for(unsigned i=0;i<c->count;i++){const McuControllerPreference*p=c->records+i;s->controllers[i].enabled=p->enabled;s->controllers[i].profile=mcu_controller_profile_id(p->input.profile);s->controllers[i].input_type=p->input.physical_only;s->controllers[i].hui_policy=p->hui_policy;if(p->enabled&&s->active_controller_index==NATIVE_CONTROLLER_NEW){s->active_controller_index=i;s->active_profile=s->controllers[i].profile;s->active_endpoint_type=p->input.physical_only;s->active_hui_policy=p->hui_policy;}}s->collection_revision++;}
static int native_sync(const char*state,const char*preference,const char*active_name){NativePreferencesState*s=native_state(state);McuControllerCollection c;if(s==MAP_FAILED||!mcu_controller_preferences_read_collection(preference,&c))return 1;native_collection(s,&c);if(strcmp(active_name,"saved")){s->active_profile=mcu_controller_profile_id(mcu_profile_named(active_name));if(!s->active_profile){munmap(s,NATIVE_PREFERENCES_BYTES);return 1;}}native_inventory(s);s->status=NATIVE_PREFERENCES_ACTIVE;munmap(s,NATIVE_PREFERENCES_BYTES);return 0;}
static int native_apply(const char*state,const char*preference){NativePreferencesState*s=native_state(state);if(s==MAP_FAILED)return 11;unsigned seq=s->request_sequence,kind=s->request_kind,profile=s->request_profile;if(!seq||seq==s->claimed_sequence){munmap(s,NATIVE_PREFERENCES_BYTES);return 10;}s->claimed_sequence=seq;s->status=NATIVE_PREFERENCES_APPLYING;if(kind==NATIVE_REQUEST_REFRESH){if(!access("inventory-open-fail",F_OK)){s->inventory_count=0;s->inventory_generation++;s->inventory_status=s->status=NATIVE_PREFERENCES_UNAVAILABLE;}else{native_inventory(s);s->status=NATIVE_PREFERENCES_REFRESHED;}s->completed_sequence=seq;munmap(s,NATIVE_PREFERENCES_BYTES);return 10;}McuControllerCollection c;McuControllerPreference p={0};int result=mcu_controller_preferences_read_collection(preference,&c)?0:11;unsigned index=s->request_controller_index;
 if(!result&&s->request_collection_generation!=s->collection_generation)result=15;
 if(!result&&(kind==NATIVE_REQUEST_TOGGLE||kind==NATIVE_REQUEST_REMOVE)){if(index>=c.count||s->request_enabled>1)result=11;else if(kind==NATIVE_REQUEST_TOGGLE)c.records[index].enabled=s->request_enabled;else{for(unsigned i=index;i+1<c.count;i++)mcu_controller_preference_copy(c.records+i,c.records+i+1);c.count--;}goto save;}
 if(kind!=NATIVE_REQUEST_APPLY)result=11;
 if(profile==1||profile==2){const McuProfile*x=mcu_controller_profile(profile);p.endpoint=(McuEndpoint){.profile=x,.client=x->default_client,.port=x->default_port,.physical_only=1};}
 else if(profile==3){if(!access("generic-unavailable",F_OK))result=12;else if(!access("generic-ambiguous",F_OK))result=13;else{strcpy(p.client,"Generic Device");strcpy(p.port,"Generic Port");p.endpoint=(McuEndpoint){.profile=&mcu_profile_generic,.client=p.client,.port=p.port,.physical_only=1};}}
 else if(profile==4){if(s->request_input_inventory_generation!=s->inventory_generation||s->request_output_inventory_generation!=s->inventory_generation)result=15;else if(s->request_input_endpoint_index>=s->inventory_count||s->request_output_endpoint_index>=s->inventory_count||(s->request_hui_policy!=NATIVE_HUI_PICKUP&&s->request_hui_policy!=NATIVE_HUI_MOTORIZED)||s->endpoints[s->request_input_endpoint_index].endpoint_type!=s->request_input_endpoint_type||s->endpoints[s->request_output_endpoint_index].endpoint_type!=s->request_output_endpoint_type)result=11;else{NativePreferencesEndpoint*row=s->endpoints+s->request_input_endpoint_index;strcpy(p.client,(char*)row->client);strcpy(p.port,(char*)row->port);p.endpoint=(McuEndpoint){.profile=&mcu_profile_hui,.client=p.client,.port=p.port,.physical_only=row->endpoint_type};p.hui_policy=s->request_hui_policy;}}
 else result=11;
 p.enabled=s->request_enabled;p.input=p.output=p.endpoint;
 if(!result&&profile==4){NativePreferencesEndpoint*row=s->endpoints+s->request_output_endpoint_index;p.output=(McuEndpoint){.profile=&mcu_profile_hui,.client=(char*)row->client,.port=(char*)row->port,.physical_only=row->endpoint_type};}
 if(!result){if(index==NATIVE_CONTROLLER_NEW){if(c.count>=MCU_CONTROLLER_CAPACITY)result=11;else index=c.count++;}else if(index>=c.count)result=11;if(!result&&!mcu_controller_preference_copy(c.records+index,&p))result=11;}
 save:if(result){s->status=result==12?NATIVE_PREFERENCES_UNAVAILABLE:result==13?NATIVE_PREFERENCES_AMBIGUOUS:result==15?NATIVE_PREFERENCES_STALE:NATIVE_PREFERENCES_INVALID;s->completed_sequence=seq;munmap(s,NATIVE_PREFERENCES_BYTES);return result;}
 if(!mcu_controller_preferences_write_collection(preference,&c)){s->status=NATIVE_PREFERENCES_WRITE_FAILED;s->completed_sequence=seq;munmap(s,NATIVE_PREFERENCES_BYTES);return 14;}native_collection(s,&c);munmap(s,NATIVE_PREFERENCES_BYTES);return 0;}
static int native_finish(const char*state,int active){NativePreferencesState*s=native_state(state);if(s==MAP_FAILED)return 1;s->status=active?NATIVE_PREFERENCES_ACTIVE:NATIVE_PREFERENCES_RESTART_FAILED;s->completed_sequence=s->claimed_sequence;munmap(s,NATIVE_PREFERENCES_BYTES);return 0;}
static int native_watch(const char*path){NativePreferencesState*s=native_state(path);if(s==MAP_FAILED)return 1;pid_t owner=getppid();unsigned seen=s->completed_sequence;for(;;){unsigned requested=__atomic_load_n(&s->request_sequence,__ATOMIC_ACQUIRE);if(requested!=seen){if(getppid()!=owner||kill(owner,SIGUSR2))return 1;seen=requested;}long r=syscall(SYS_futex,&s->request_sequence,0,requested,NULL,NULL,0);if(r<0&&errno!=EAGAIN&&errno!=EINTR)return 1;}}
int main(int argc,char **argv){int bridge=strstr(argv[0],"mirror-input")!=0;
if(bridge&&(argc==3||argc==4)&&!strcmp(argv[1],"--native-preferences-invalidate")){NativePreferencesState*s=native_state(argv[2]);if(s==MAP_FAILED)return 1;if(argc==4){unsigned prior=(unsigned)strtoul(argv[3],0,10);__atomic_compare_exchange_n(&s->controller_lifetime,&prior,prior+1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);}else printf("%u\n",__atomic_add_fetch(&s->controller_lifetime,1u,__ATOMIC_ACQ_REL));munmap(s,NATIVE_PREFERENCES_BYTES);return 0;}
if(bridge&&argc==3&&!strcmp(argv[1],"--test-lifetime")){NativePreferencesState*s=native_state(argv[2]);if(s==MAP_FAILED)return 1;printf("%u\n",s->controller_lifetime);munmap(s,NATIVE_PREFERENCES_BYTES);return 0;}
if(bridge&&argc==3&&!strcmp(argv[1],"--native-preferences-watch"))return native_watch(argv[2]);
if(bridge&&argc==5&&!strcmp(argv[1],"--native-preferences-sync"))return native_sync(argv[2],argv[3],argv[4]);
if(bridge&&argc==4&&!strcmp(argv[1],"--native-preferences-apply"))return native_apply(argv[2],argv[3]);
if(bridge&&argc==4&&!strcmp(argv[1],"--native-preferences-finish"))return native_finish(argv[2],!strcmp(argv[3],"active"));
if(bridge&&argc==3&&!strcmp(argv[1],"--controller-any-enabled")){McuControllerCollection c;if(!mcu_controller_preferences_read_collection(argv[2],&c))return 2;for(unsigned i=0;i<c.count;i++)if(c.records[i].enabled)return 0;return 3;}
if(bridge&&argc==5&&!strcmp(argv[1],"--controller-restore"))return mcu_controller_preferences_restore(argv[2],argv[3],!strcmp(argv[4],"missing"))?0:1;
if(bridge&&argc==3&&!strcmp(argv[1],"--surface-status")){unsigned enabled;if(!surface_preferences_read(argv[2],&enabled))return 1;puts(enabled?"on":"off");return 0;}
if(bridge&&argc==3&&!strcmp(argv[1],"--controller-status")){McuControllerCollection c;if(!mcu_controller_preferences_read_collection(argv[2],&c))return 1;McuControllerPreference s;if(!mcu_controller_preferences_read(argv[2],&s)){printf("controllers=%u enabled=0 profile=none\n",c.count);return 0;}printf("profile=%s endpoint-type=%s",s.endpoint.profile->name,endpoint_type(s.endpoint.physical_only));if(!s.endpoint.profile->default_client)printf(" client=%s port=%s",s.endpoint.client,s.endpoint.port);if(s.endpoint.profile==&mcu_profile_hui)printf(" hui-mode=%s",hui_mode(s.hui_policy));printf(" to-endpoint-type=%s",endpoint_type(s.output.physical_only));if(!s.output.profile->default_client)printf(" to-client=%s to-port=%s",s.output.client,s.output.port);puts("");return 0;}
if(bridge&&argc>=4&&!strcmp(argv[1],"--controller-set"))return controller_set(argc,argv);
if(strstr(argv[0],"mpclearn-controls")){signal(SIGUSR1,arm);signal(SIGTERM,stop);puts("DISARMED fixture");fflush(stdout);if(!access("adapter-route-delay",F_OK)){usleep(1500000);int ready=open("adapter-route-ready",O_CREAT|O_WRONLY,0600);if(ready>=0)close(ready);}while(!stopped)pause();return 0;}
/* --startup-repro retains the rejected bridge admission for the negative lane. */
if(bridge&&!access("bridge-requires-route",F_OK)&&!access("adapter-route-delay",F_OK)&&access("adapter-route-ready",F_OK)){fputs("STOP Stop adapter route unavailable\n",stderr);return 1;}
if(bridge){signal(SIGTERM,stop);if(!access("bridge-immediate-fail",F_OK)){unlink("bridge-immediate-fail");return 7;}if(!access("bridge-start-fail",F_OK)){unlink("bridge-start-fail");usleep(200000);return 7;}printf("BRIDGE_READY pid=%u\n",(unsigned)getpid());for(int i=1;i<argc;i++){printf("ARG %s\n",argv[i]);if(!strncmp(argv[i],"--controller-preferences=",25)){McuControllerPreference s;if(!mcu_controller_preferences_read(argv[i]+25,&s))return 2;printf("SAVED profile=%s endpoint-type=%s client=%s port=%s",s.endpoint.profile->name,endpoint_type(s.endpoint.physical_only),s.endpoint.client,s.endpoint.port);if(s.endpoint.profile==&mcu_profile_hui)printf(" hui-mode=%s",hui_mode(s.hui_policy));printf(" to-endpoint-type=%s to-client=%s to-port=%s",endpoint_type(s.output.physical_only),s.output.client,s.output.port);puts("");}}fflush(stdout);while(!stopped){FILE*f=fopen("mpc.pid","r");int p=0;if(f){fscanf(f,"%d",&p);fclose(f);}if(p>0&&kill(p,0))break;usleep(10000);}printf("TOUCH_FINAL pid=%u mask=3\n",(unsigned)getpid());return stopped?0:1;}
prctl(PR_SET_NAME,"MPC Main Thread",0,0,0);signal(SIGTERM,stop);
FILE*f=fopen("launches","a");fputs("1",f);fclose(f);unlink("project-ready");unlink("native-intent");unlink("closed");
if(!access("start-project-ready",F_OK)){f=fopen("project-ready","w");if(f)fclose(f);}
f=fopen("/run/mpclearn/state/command.state","w");fprintf(f,"%u\n",(unsigned)getpid());fclose(f);f=fopen("/run/mpclearn/state/volume.state","w");fclose(f);
int nfd=open("/run/mpclearn/state/native-preferences.state",O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(nfd<0||ftruncate(nfd,NATIVE_PREFERENCES_BYTES))return 90;NativePreferencesState*ns=mmap(NULL,NATIVE_PREFERENCES_BYTES,PROT_READ|PROT_WRITE,MAP_SHARED,nfd,0);close(nfd);if(ns==MAP_FAILED)return 91;memset(ns,0,NATIVE_PREFERENCES_BYTES);ns->magic=NATIVE_PREFERENCES_MAGIC;ns->version=NATIVE_PREFERENCES_VERSION;ns->bytes=NATIVE_PREFERENCES_BYTES;ns->pid=getpid();munmap(ns,NATIVE_PREFERENCES_BYTES);
for(int i=0;i<1100000;i++)putchar('x');puts(" MPC diagnostic tail");fflush(stdout);
while(!stopped){f=fopen("exit-request","r");if(f){int code=0;fscanf(f,"%d",&code);fclose(f);unlink("exit-request");return code;}usleep(10000);}return 0;}
''')
subprocess.run(['gcc', '-D_GNU_SOURCE', '/tmp/session-stub.c', '-o', '/usr/bin/MPC'], check=True)
stub_bin = P('/tmp/session-bin'); stub_bin.mkdir()
def script(p, text, mode=0o700):
    p.write_text('#!/bin/sh\n'+text); p.chmod(mode)
script(stub_bin/'setarch', 'shift;shift;exec "$@"\n')
script(stub_bin/'systemctl', f'''case "$*" in *mpclearn-boot*) case "$1" in is-active) echo inactive;exit 3;; *) exit 0;; esac;; esac
case "$1" in stop) rm -f {session}/service-active;; start) touch {session}/service-active;; is-active) test -f {session}/service-active;; *) exit 2;; esac\n''')
env['PATH'] = f'{stub_bin}:' + env['PATH']
COMMAND_CLIENT = f'''cd {session}
case "$1" in
 session-status) test "$2" = "{state}/command.state" -a "$3" = "{state}/volume.state" || exit 9;test -f "$2" -a -f "$3" || exit 5;test ! -f source-failed || exit 1;test ! -f closed || exit 4;test ! -f native-intent || exit 6;test -f project-ready || exit 3;exit 0;;
 status) test "$2" = "{state}/command.state" || exit 9
  awk -v n="$(cat status-events 2>/dev/null || echo 2)" 'BEGIN{{printf "{{\\"format\\":\\"CMD31\\",\\"error\\":0,\\"requests\\":[{{\\"slot\\":0,\\"rejected\\":0}}],\\"events\\":[";for(i=0;i<n;i++)printf "%s{{\\"slot\\":0,\\"lane\\":0,\\"index\\":%d,\\"kind\\":1,\\"lr\\":1234567}}",(i?",":""),i;print "],\\"snapshot_atomic\\":false,\\"new_project_intent\\":0,\\"navigation\\":{{\\"watch\\":0}},\\"admission\\":{{\\"category\\":\\"none\\",\\"raw_result\\":0,\\"site\\":77}}}}"}}';;
 new-project-intent) test -f native-intent || exit 1;test "$2" = "{state}/command.state" || exit 9;test "$(cat "$2")" = "$3" || exit 1;test "$(cat native-intent)" = "$3" || exit 1;if test "${{5:-}}" = consume;then test ! -e /proc/$3/exe || exit 1;mv native-intent consumed-intent;fi;;
 stop) test "$2" = "{state}/command.state" -a "$3" = "{state}/volume.state" || exit 9;touch closed;;
 *) exit 2;; esac
'''
MIRROR_READ = f'''test "$1" = "{state}/volume.state" -a "$2" = --all || exit 9
echo '{{"format":"MMV17","stable":true,"fresh":false,"tracks":[]}}';exit 1
'''
def sha(path): return hashlib.sha256(P(path).read_bytes()).hexdigest()
def write_package(folder, location, source):
    folder.mkdir(parents=True, exist_ok=True)
    (folder/'mcu-session.sh').write_text(source)
    (folder/'mcu').write_text(script_text('/src/mcu.sh'))
    (folder/'config.h').write_text(f'#define OBSERVER_LOG "{state}/volume.state"\n#define COMMAND_PATH "{state}/command.state"\n#define OBSERVER_LIBRARY "{location}/command-observer.so"\n#define WINDOW_SECONDS 0u\n')
    script(folder/'command-client', COMMAND_CLIENT)
    script(folder/'mirror-read', MIRROR_READ)
    script(folder/'main-button', 'test ! -f route-fail && { test ! -f adapter-route-delay || test -f adapter-route-ready; }\n')
    for name in ['mirror-input', 'mpclearn-controls']:
        (folder/name).write_bytes(P('/usr/bin/MPC').read_bytes())
    (folder/'command-observer.so').write_bytes(b'')
    manifest(folder)
def manifest(folder):
    (folder/'session-package.sha256').write_text(''.join(f'{sha(folder/n)}  {n}\n' for n in NAMES))
def set_modes(folder):
    os.chown(folder, 0, 0); folder.chmod(0o700)
    for n in NAMES+['session-package.sha256']:
        os.chown(folder/n, 0, 0); (folder/n).chmod(0o700 if n in EXECUTABLES else 0o600)

def until(pred, why, seconds=15):
    end = time.monotonic()+seconds
    while time.monotonic() < end:
        try:
            if pred(): return
        except FileNotFoundError:  # a locator between archive and relaunch
            pass
        time.sleep(.05)
    log = session/'owner.log'
    raise AssertionError(why+'\n'+(log.read_text() if log.exists() else '(no owner.log)'))
def call(path, *args, expected=0):
    p = subprocess.run([str(path), *args], env=env, capture_output=True, text=True, timeout=60)
    assert p.returncode == expected, (str(path), args, p.returncode, p.stdout, p.stderr)
    return p
def run(op, expected=0, where=dev):
    return call(where/('mcu-session.sh' if op.startswith('bridge-') else 'mcu'), op, expected=expected)
def count(name): return len((session/name).read_text()) if (session/name).exists() else 0
def app_pid(): return int((session/'mpc.pid').read_text().split()[0])
def owner_gone():
    try: p = int((session/'owner.pid').read_text().split()[0]); return not P(f'/proc/{p}/exe').exists()
    except FileNotFoundError: return True
def depart(intent, code=0):
    if intent: (session/'native-intent').write_text(str(app_pid()))
    (session/'exit-request').write_text(str(code))
def entries(): return sorted(p.name for p in history.iterdir())
def du(path): return int(subprocess.run(['du', '-sb', str(path)], capture_output=True, text=True, check=True).stdout.split()[0])
def no_raw_state():
    found = [str(p) for p in home.rglob('*.state')]
    assert not found, found
def native_lifetime():
    return int(subprocess.check_output([str(dev/'mirror-input'), '--test-lifetime', str(state/'native-preferences.state')], env=env, text=True))
def native_values():
    data = (state/'native-preferences.state').read_bytes()
    assert len(data) == 16384 and struct.unpack_from('<III', data) == (0x3355504e, 14, 16384)
    return struct.unpack_from('<'+'I'*60, data)
def native_request(profile, *, kind=1, generation=0, endpoint_index=0, endpoint_type=0, hui_policy=0, controller_index=0, enabled=1, output_index=None, output_type=None):
    path = state/'native-preferences.state'
    with path.open('r+b', buffering=0) as f, mmap.mmap(f.fileno(), 16384) as page:
        sequence = struct.unpack_from('<I', page, 80)[0] + 1
        struct.pack_into('<IIIIII', page, 84, kind, profile, generation, endpoint_index, endpoint_type, hui_policy)
        collection_generation = struct.unpack_from('<I', page, 52*4)[0]
        struct.pack_into('<IIIIIIIII', page, 29*4, controller_index, enabled, collection_generation,
                         generation, endpoint_index, endpoint_type, generation,
                         endpoint_index if output_index is None else output_index,
                         endpoint_type if output_type is None else output_type)
        struct.pack_into('<I', page, 80, sequence)
        word = ctypes.c_uint32.from_buffer(page, 80)
        syscall_number = 98 if platform.machine() in ('aarch64', 'arm64') else 202
        assert ctypes.CDLL(None, use_errno=True).syscall(syscall_number, ctypes.byref(word), 1, 0x7fffffff, 0, 0, 0) >= 0
        del word
    return sequence
def make_hui_legacy_pickup(path):
    current = path.read_bytes(); assert len(current) == 8992 and current[:8] == b'MCUCFG2\0'
    data = bytearray(288); data[:8] = b'MCUCFG1\0'
    client, port = b'HUI Software', b'DAW Port'
    struct.pack_into('<IIIII', data, 8, 4, 2, len(client), len(port), 0)
    data[28:28+len(client)] = client; data[156:156+len(port)] = port
    value = 2166136261
    for byte in data[:-4]: value = ((value ^ byte) * 16777619) & 0xffffffff
    struct.pack_into('<I', data, 284, value); path.write_bytes(data); path.chmod(0o600)
def cleanup():
    for exe in P('/proc').glob('[0-9]*/exe'):
        try:
            if os.readlink(exe) == '/usr/bin/MPC' or os.readlink(exe).endswith(('/mirror-input', '/mpclearn-controls')): os.kill(int(exe.parent.name), signal.SIGKILL)
        except (FileNotFoundError, ProcessLookupError): pass

def outside():
    out = {}
    for base in [P('/data'), P('/etc')]:
        for f in sorted(base.rglob('*')):
            if base.name == 'etc' and not f.name.startswith('mpclearn'): continue
            if str(f) == str(home) or str(f).startswith(str(home)+'/') or f.is_dir() and not f.is_symlink(): continue
            out[str(f)] = (f.lstat().st_mode, f.read_bytes() if f.is_file() and not f.is_symlink() else os.readlink(f) if f.is_symlink() else None)
    return out
# Leftovers of earlier releases on the development device: inert under this design.
for name in ['mpclearn-model.v0_2_0', 'mpclearn-model.v0_2_2', 'mpclearn-model.mouse-r5']:
    old = P('/data')/name; (old/'history/1').mkdir(parents=True)
    for f in NAMES+['session-package.sha256', 'session.id', 'mpc.pid']: (old/f).write_text(f'old {f}\n')
    (old/'surface-preferences').write_text('MCU-SURFACE1 motors=0\n')
    (old/'history/1/volume.state').write_bytes(b'v'*4096)
for f in ['mpclearn-boot/mcu-boot.sh', 'mpclearn-boot/mcu-session.sh', 'mpclearn-image/installed', 'mpclearn-image/previous-stage']:
    (P('/data')/f).parent.mkdir(exist_ok=True); (P('/data')/f).write_text('old\n')
for f in ['mpclearn-boot-stage', 'mpclearn-boot-stage.next', 'mpclearn-boot-stage.tmp']:
    (P('/etc')/f).write_text('/data/mpclearn-model.v0_2_0\n')
before_outside = outside()

try:
    # Our folder is adopted only as root 0700; nothing is written into anything else.
    home.mkdir(parents=True); home.chmod(0o755)
    write_package(dev, dev, session_source()); set_modes(dev)
    p = run('status', 2)
    assert 'not a root-owned 0700 folder' in p.stderr and not session.exists() and not history.exists() and not runtime.exists()
    home.chmod(0o700); shutil.rmtree(dev)
    # The image package, and a development package built on another machine
    # (non-root owner, permissive modes) that only the installer may place.
    write_package(image, image, session_source())
    for n in ['mcu-boot.sh', 'mcu-boot-install.sh']: (image/n).write_text(script_text('/src/'+n))
    set_modes(image)
    for n in ['mcu-boot.sh', 'mcu-boot-install.sh']: (image/n).chmod(0o700)
    (image/'payload.sha256').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(image.iterdir())))
    source = P('/tmp/dev-package'); write_package(source, dev, session_source())
    for f in [source, *source.iterdir()]: os.chown(f, 1000, 1000); f.chmod(0o755 if f.is_dir() else 0o644)
    wrong = P('/tmp/wrong-package'); write_package(wrong, image, session_source())
    p = call(image/'mcu-boot-install.sh', 'override', str(wrong), expected=1); assert 'not built for /data/mpclearn/dev' in p.stderr
    shutil.copytree(source, '/tmp/tampered'); (P('/tmp/tampered')/'mirror-input').write_bytes(b'tampered')
    call(image/'mcu-boot-install.sh', 'override', '/tmp/tampered', expected=1)
    assert not dev.exists()
    call(image/'mcu-boot-install.sh', 'override', str(source))
    assert all(os.stat(dev/n).st_uid == 0 and (dev/n).stat().st_mode & 0o777 == (0o700 if n in EXECUTABLES else 0o600) for n in NAMES+['session-package.sha256'])
    assert dev.stat().st_mode & 0o777 == 0o700 and (dev/'for-image').read_text() == sha(image/'payload.sha256')+'\n', (
        oct(dev.stat().st_mode & 0o777), (dev/'for-image').read_text(), sha(image/'payload.sha256'))
    # Boot admission ignores an override for another image, says so, and removes nothing.
    good = (dev/'for-image').read_text()
    for bad in ('0'*64+'\n', None):
        if bad is None: (dev/'for-image').unlink()
        else: (dev/'for-image').write_text(bad)
        p = call(image/'mcu-boot.sh', 'stop', expected=1)
        assert 'ignored: installed for a different image; using the image runtime' in p.stderr and 'No owned session' in p.stderr, p.stderr
        assert (dev/'mcu').exists() and not (session/'launches').exists()
    (dev/'for-image').write_text(good)
    p = call(image/'mcu-boot.sh', 'stop', expected=1); assert 'ignored' not in p.stderr
    # Only the image and the override location may run.
    stray = P('/data/mpclearn-model.wrapper-test'); shutil.copytree(dev, stray); stray.chmod(0o700)
    p = call(stray/'mcu', 'status', expected=2); assert 'Unsupported package location' in p.stderr
    assert sorted(x.name for x in stray.iterdir()) == sorted(x.name for x in dev.iterdir()); shutil.rmtree(stray)
    # Status before any start creates only the tmpfs folder; the preference path is fixed.
    for entry in (dev/'mcu', dev/'mcu-session.sh'):
        p = call(entry, expected=5)
        assert 'motor_follow=on' in p.stdout and 'profile=xtouch endpoint-type=kernel' in p.stdout and runtime.stat().st_mode & 0o777 == 0o700 and not state.exists()
    print(f'PASS [{MODE}] fixed locations: 0700 folder adoption, override install with shipped owner/modes, mismatched override ignored and kept, stray location refusal, no-argument mcu/session status on fresh tmpfs')
    if '--startup-repro' in sys.argv:
        call(dev/'mirror-input', '--controller-set', str(home/'controller-preferences'), '--profile=xtouch-mini')
        saved = (home/'controller-preferences').read_bytes()
        (session/'adapter-route-delay').touch()
        (session/'bridge-requires-route').touch()
        run('start', 1)
        assert 'STOP Stop adapter route unavailable' in (session/'bridge.log').read_text()
        until(lambda: (session/'adapter-route-ready').exists(), 'late MPC route readiness')
        run('bridge-start')
        assert native_values()[53] == 0 and (session/'generation.failed').exists()
        assert (home/'controller-preferences').read_bytes() == saved
        print(f'REPRO [{MODE}] first bridge fails before delayed MPC route; later explicit bridge succeeds but owner stays failed and native collection remains empty')
        sys.exit(0)
    if '--startup-check' in sys.argv:
        call(dev/'mirror-input', '--controller-set', str(home/'controller-preferences'), '--profile=xtouch-mini')
        saved = (home/'controller-preferences').read_bytes()
        (session/'adapter-route-delay').touch()
        (session/'start-project-ready').touch()
        run('start')
        assert native_values()[53] == 1 and native_values()[52] > 0 and native_values()[49] > 0
        assert (home/'controller-preferences').read_bytes() == saved
        (session/'project-ready').touch()
        until(lambda: count('armed') == 1, 'single arming attempt waits for delayed MPC route')
        assert not (session/'generation.failed').exists()
        sequence = native_request(0, kind=2)
        until(lambda: native_values()[28] == sequence, 'absent-controller watcher services Refresh')
        run('stop'); until(owner_gone, 'delayed-start owner stops')
        (session/'route-fail').touch(); armed_before = count('armed'); run('start')
        until(lambda: (session/'generation.failed').exists(), 'bounded route admission fails without restarting bridge')
        retained_bridge = (session/'bridge.pid').read_text()
        (session/'route-fail').unlink(); time.sleep(1)
        assert count('armed') == armed_before and (session/'bridge.pid').read_text() == retained_bridge, 'late readiness cannot automatically repeat failed arming'
        run('bridge-stop'); run('bridge-start')
        assert count('armed') == armed_before+1 and not (session/'generation.failed').exists()
        run('stop'); until(owner_gone, 'explicit route recovery stops')
        # A real bridge failure remains terminal until this explicit recovery.
        (session/'bridge-immediate-fail').touch(); run('start', 1)
        assert native_values()[53] == 1 and (session/'generation.failed').exists()
        failed_bridge = (session/'bridge.pid').read_text()
        time.sleep(1)
        assert (session/'bridge.pid').read_text() == failed_bridge and (session/'bridge.exit').exists(), 'no automatic bridge restart'
        (session/'project-ready').touch(); run('bridge-start')
        until(lambda: not (session/'generation.failed').exists(), 'explicit recovery rejoins resident owner')
        assert (session/'generation.ready').read_text() == 'ready\n' and native_values()[53] == 1
        sequence = native_request(0, kind=2)
        until(lambda: native_values()[28] == sequence, 'recovered owner watcher services Refresh')
        assert (home/'controller-preferences').read_bytes() == saved
        run('stop'); until(owner_gone, 'recovered owner stops')
        assert not (runtime/'controller-recovery').exists()
        print(f'PASS [{MODE}] absent saved Mini startup: private bridge before delayed MPC route, bounded arming, collection/inventory/watch ready, true first bridge failure has no automatic retry, explicit late recovery rejoins owner and services Refresh without preference changes')
        sys.exit(0)
    # Runtime admission refuses symlinks, unknown entries and unowned state.
    state.symlink_to('/tmp', target_is_directory=True); run('start', 1); state.unlink()
    state.mkdir(mode=0o700); (state/'unexpected').touch(); run('start', 1); (state/'unexpected').unlink()
    (state/'command.state').symlink_to('/tmp/missing-state'); run('start', 1); (state/'command.state').unlink()
    (state/'command.state').write_text('unowned'); os.chown(state/'command.state', 1000, 1000); run('start', 1); (state/'command.state').unlink()
    assert not (session/'launches').exists()
    # Stop wins even before the owner has acquired its launch admission.
    (session/'pause-owner').touch(); started = subprocess.Popen([str(dev/'mcu'), 'start'], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    until(lambda: (session/'owner.pid').exists(), 'owner launch locator'); run('stop', 1); (session/'pause-owner').unlink(); started.communicate(timeout=10)
    until(owner_gone, 'revoked unlaunched owner exits'); assert count('launches') == 0 and (session/'service-active').exists()
    # The canceled initial generation has no app locator; a new explicit start
    # must retain its session diagnostics before replacing them. Boot admission
    # selects the verified override.
    call(image/'mcu-boot.sh', 'start'); until(lambda: (session/'generation.ready').exists(), 'chooser controller ready'); assert count('launches') == 1 and count('armed') == 0
    assert any('-unlaunched-' in e for e in entries()), entries()
    bridge_log = (session/'bridge.log').read_text()
    assert '--motors=on' in bridge_log and '--preferences=/data/mpclearn/surface-preferences' in bridge_log
    assert 'ARG --controller-preferences=/data/mpclearn/controller-preferences' in bridge_log and 'SAVED profile=xtouch endpoint-type=kernel client=X-Touch port=X-TOUCH_INT' in bridge_log
    assert (session/'session.location').read_text() == f'{dev}\n'
    p = run('status', 2, where=image); assert 'belongs to /data/mpclearn/dev' in p.stderr
    (home/'surface-preferences').write_text('MCU-SURFACE1 motors=0\n'); (home/'surface-preferences').chmod(0o600)
    original = (runtime/'settings-before').read_bytes()
    # Reproduce the native miss: bridge deliberately absent when chooser becomes
    # ready, then explicit bridge-start must repair arming under the same owner.
    run('bridge-stop'); (session/'project-ready').touch()
    arming_failure = 'Route/source unavailable at arming; no automatic retry.\n'
    until(lambda: (session/'generation.failed').exists(), 'missing bridge arming failure')
    assert (session/'generation.failed').read_text() == arming_failure and count('armed') == 0 and (session/'manual-adapter-unarmed.pid').exists()
    (session/'route-fail').touch(); run('bridge-start', 1)
    assert count('armed') == 0 and (session/'manual-adapter-unarmed.pid').exists() and (session/'generation.failed').read_text() == arming_failure
    run('bridge-stop'); (session/'route-fail').unlink(); run('bridge-start'); until(lambda: count('armed') == 1, 'explicit bridge-start restores missed arming')
    assert not (session/'generation.failed').exists() and not (session/'manual-adapter-unarmed.pid').exists()
    state_inodes = [(state/n).stat().st_ino for n in ('command.state', 'volume.state')]
    lifetime = native_lifetime()
    run('bridge-stop'); assert native_lifetime() == lifetime + 1, 'exact child exit invalidates admission'
    run('bridge-start'); assert native_lifetime() == lifetime + 2, 'replacement bridge has fresh lifetime'
    assert count('armed') == 1
    lifetime = native_lifetime(); bridge_pid = int((session/'bridge.pid').read_text().split()[0])
    os.kill(bridge_pid, signal.SIGKILL)
    until(lambda: (session/'bridge.exit').exists(), 'killed bridge child receipt')
    assert native_lifetime() == lifetime + 1, 'killed bridge cannot retain same-MPC admission'
    run('bridge-start'); assert native_lifetime() == lifetime + 2, 'restart after killed bridge fences its publication'
    assert state_inodes == [(state/n).stat().st_ino for n in ('command.state', 'volume.state')]
    run('start'); assert count('launches') == 1
    # A latched source error must not turn the idempotent `mcu start` into a
    # silent exit 1: the status is reported, the outcome line is still printed.
    (session/'source-failed').touch()
    retained = run('start')
    assert 'Session source status exit 1' in retained.stdout and 'Existing owned session retained' in retained.stdout, retained.stdout
    (session/'source-failed').unlink(); assert count('launches') == 1 and count('armed') == 1
    # The injected callback is represented by one atomic state write plus the
    # same shared futex wake. The real owner watcher claims it outside the UI,
    # preserves the live bridge on errors, and uses its normal drain/restart.
    native_preferences = state/'native-preferences.state'; assert native_preferences.stat().st_size == 16384
    run('status'); bridge_before = int((session/'bridge.pid').read_text().split()[0])
    sequence = native_request(99); until(lambda: native_values()[28] == sequence, 'invalid native request completes')
    assert native_values()[38] == 6 and int((session/'bridge.pid').read_text().split()[0]) == bridge_before and not (home/'controller-preferences').exists()
    (session/'generic-unavailable').touch(); sequence = native_request(3); until(lambda: native_values()[28] == sequence, 'unavailable Generic completes')
    assert native_values()[38] == 4 and int((session/'bridge.pid').read_text().split()[0]) == bridge_before and not (home/'controller-preferences').exists(); (session/'generic-unavailable').unlink()
    (session/'generic-ambiguous').touch(); sequence = native_request(3); until(lambda: native_values()[28] == sequence, 'ambiguous Generic completes')
    assert native_values()[38] == 5 and int((session/'bridge.pid').read_text().split()[0]) == bridge_before and not (home/'controller-preferences').exists(); (session/'generic-ambiguous').unlink()
    restore_trace = session/'restore-fsync.trace'
    if restore_trace.exists(): restore_trace.unlink()
    (session/'bridge-start-fail').touch(); sequence = native_request(3)
    until(lambda: native_values()[28] == sequence and native_values()[38] == 8, 'failed Generic replacement rolls back missing default')
    restored_default_pid = int((session/'bridge.pid').read_text().split()[0]); values = native_values()
    assert restored_default_pid != bridge_before and not (home/'controller-preferences').exists(), (restored_default_pid, bridge_before, (home/'controller-preferences').exists(), (session/'bridge.log').read_text())
    os.kill(restored_default_pid, 0)
    assert values[39] == values[40] == 1 and values[43] == values[44] == 1 and values[45] == values[46] == 0
    assert 'SAVED profile=xtouch endpoint-type=kernel client=X-Touch port=X-TOUCH_INT' in (session/'bridge.log').read_text()
    assert restore_trace.read_text().splitlines() == ['file', 'directory', 'directory']
    assert not (runtime/'controller-preferences.rollback').exists() and not (home/'.controller-preferences.restore').exists()
    sequence = native_request(3); until(lambda: native_values()[28] == sequence and native_values()[38] == 3, 'unique Generic applies')
    generic_pid = int((session/'bridge.pid').read_text().split()[0]); assert generic_pid != restored_default_pid
    assert 'SAVED profile=generic endpoint-type=kernel client=Generic Device port=Generic Port' in (session/'bridge.log').read_text()
    generic_preference = (home/'controller-preferences').read_bytes()
    inventory_generation = native_values()[48]
    restore_trace.unlink(); (session/'bridge-immediate-fail').touch(); (session/'bridge-pre-identity-fail').touch()
    sequence = native_request(4, generation=inventory_generation, endpoint_index=1, endpoint_type=2, hui_policy=2)
    until(lambda: native_values()[28] == sequence and native_values()[38] == 8, 'pre-identity HUI failure restores exact Generic bytes')
    restored_generic_pid = int((session/'bridge.pid').read_text().split()[0]); values = native_values()
    assert restored_generic_pid != generic_pid and (home/'controller-preferences').read_bytes() == generic_preference
    os.kill(restored_generic_pid, 0)
    assert values[39] == values[40] == 3 and values[43] == values[44] == 1 and values[45] == values[46] == 0
    assert 'SAVED profile=generic endpoint-type=kernel client=Generic Device port=Generic Port' in (session/'bridge.log').read_text()
    assert 'BRIDGE_LAUNCH_IDENTITY_FAILED' in (session/'owner.log').read_text()
    assert restore_trace.read_text().splitlines() == ['file', 'directory', 'file', 'directory']
    assert not (runtime/'controller-preferences.rollback').exists() and not (home/'.controller-preferences.restore').exists()
    inventory_generation = native_values()[48]
    sequence = native_request(4, generation=inventory_generation-1, endpoint_index=1, endpoint_type=2, hui_policy=2); until(lambda: native_values()[28] == sequence, 'stale HUI request completes')
    assert native_values()[38] == 10 and int((session/'bridge.pid').read_text().split()[0]) == restored_generic_pid
    (session/'inventory-open-fail').touch()
    for attempt in range(3):
        sequence = native_request(4, kind=2)
        until(lambda: native_values()[28] == sequence and native_values()[38] == 4, 'failed inventory open completes UNAVAILABLE')
        assert int((session/'bridge.pid').read_text().split()[0]) == restored_generic_pid and (home/'controller-preferences').read_bytes() == generic_preference
        os.kill(restored_generic_pid, 0)
    (session/'inventory-open-fail').unlink()
    print(f'PASS [{MODE}] failed Refresh: deterministic UNAVAILABLE completion retains exact collection bytes and the same live bridge (ALSA open failure/helper substituted)')
    sequence = native_request(4, kind=2); until(lambda: native_values()[28] == sequence and native_values()[38] == 9, 'HUI endpoint refresh completes')
    assert int((session/'bridge.pid').read_text().split()[0]) == restored_generic_pid and native_values()[48] != inventory_generation
    sequence = native_request(4, generation=native_values()[48], endpoint_index=1, endpoint_type=2, hui_policy=2); until(lambda: native_values()[28] == sequence and native_values()[40] == 4, 'exact user Motorized HUI request applies')
    hui_pid = int((session/'bridge.pid').read_text().split()[0]); assert hui_pid != generic_pid
    assert native_values()[44] == 2 and native_values()[46] == 2 and 'SAVED profile=hui endpoint-type=user client=User HUI port=DAW hui-mode=motorized' in (session/'bridge.log').read_text()
    sequence = native_request(2); until(lambda: native_values()[28] == sequence and native_values()[40] == 2, 'Mini request applies')
    mini_pid = int((session/'bridge.pid').read_text().split()[0]); assert mini_pid != generic_pid and 'SAVED profile=xtouch-mini' in (session/'bridge.log').read_text()
    sequence = native_request(1); until(lambda: native_values()[28] == sequence and native_values()[40] == 1, 'X-Touch return applies')
    assert int((session/'bridge.pid').read_text().split()[0]) != mini_pid and 'SAVED profile=xtouch endpoint-type=kernel client=X-Touch port=X-TOUCH_INT' in (session/'bridge.log').read_text()
    print(f'PASS [{MODE}] native controller requests: shared futex wake, owner claim, invalid/unavailable/ambiguous/stale bridge retention, post-identity and immediate pre-identity replacement failure rollback to exact saved bytes or missing X-Touch default with file/directory fsync and a working prior bridge, endpoint refresh without restart, exact user Motorized HUI persistence, active Mini restart and return to X-Touch (UI click, native MPC and ALSA enumeration substituted)')
    print(f'PASS [{MODE}] explicit bridge-start: actual missed chooser arming recovery, route-failure refusal, exact failure cleanup and no duplicate signal, a retained unhealthy source, and admission of the fixed native Preferences state file (native adapter/protocol/UI substituted)')
    for expected in (2, 3):
        previous = app_pid(); before = set(entries()); depart(True)
        until(lambda: count('launches') == expected and (session/'generation.ready').exists() and app_pid() != previous, 'accepted New Project restarts once')
        assert count('armed') == expected-1 and (runtime/'settings-before').read_bytes() == original
        bridge_log = (session/'bridge.log').read_text()
        assert '--held-mask=3' in bridge_log and '--motors=off' in bridge_log
        entry = history/(set(entries())-before).pop()
        assert entry.name.split('-', 2)[2] == f'{previous}-' + entry.name.split('-')[-1]
        assert {'mpc.pid', 'mpc.exit', 'mpc.log', 'bridge.log', 'command-status.json', 'session-status.txt', 'mirror-read.json'} <= {p.name for p in entry.iterdir()}
        json.loads((entry/'command-status.json').read_text()); json.loads((entry/'mirror-read.json').read_text())
        assert not (entry/'command-status.json.truncated').exists()
        assert (entry/'mpc.log').stat().st_size == 1048576 and (entry/'mpc.log').read_bytes().endswith(b' MPC diagnostic tail\n')
        if expected == 2:
            run('bridge-stop'); run('bridge-start'); assert count('armed') == expected-1 # chooser stays disarmed
            run('bridge-stop'); (session/'project-ready').touch(); until(lambda: (session/'generation.failed').exists(), 'second missed arming')
            (session/'generation.failed').write_text('Different retained failure.\n'); run('bridge-start')
            until(lambda: count('armed') == expected, 'explicit recovery with other failure')
            assert (session/'generation.failed').read_text() == 'Different retained failure.\n'
        else:
            (session/'project-ready').touch(); until(lambda: count('armed') == expected, 'new saved template arms')
    no_raw_state()
    print(f'PASS [{MODE}] actual owner: repeatable accepted exit -> distinct chooser bridge/disarmed adapter -> saved template arm; text-only generation archive and original settings retained (native protocol and MIDI substituted)')
    # Positive marker cannot authorize a crash restart; generic zero exit neither.
    for positive, code in ((True, 17), (False, 0)):
        before = count('launches'); depart(positive, code); until(owner_gone, 'non-NewProject owner ends'); assert count('launches') == before
        run('stop', 1); assert settings.read_text() == 'original settings\n'
        # Incomplete-stop evidence is text in the session folder, never raw state.
        assert (session/'stop-incomplete-status.txt').exists() and json.loads((session/'incomplete-command-status.json').read_text())['admission']['site'] == 77
        assert (session/'incomplete-mirror-read.json').exists() and (session/'incomplete-bridge.log').read_text() == (session/'bridge.log').read_text()
        assert not list(session.glob('incomplete-before*'))
        names = set(entries()); run('start'); new = history/(set(entries())-names).pop()
        assert {'incomplete-command-status.json', 'stop-incomplete-status.txt', 'incomplete-mirror-read.json', 'incomplete-bridge.log', 'session.revoked', 'session.location'} <= {p.name for p in new.iterdir()}
        assert not (session/'incomplete-command-status.json').exists()
        assert '--motors=off' in (session/'bridge.log').read_text(); (session/'project-ready').touch(); until(lambda: count('armed') == before+1, 'fresh explicit session arms')
    # Durable stop during a pending handoff leaves no later launch/arming.
    # The owner may or may not have relaunched before stop takes the lock (a slower
    # shell changes the order); either way nothing launches after the stop.
    depart(True); p = subprocess.run([str(dev/'mcu'), 'stop'], env=env, capture_output=True, text=True, timeout=60); assert p.returncode in (0, 1), p
    until(owner_gone, 'stop winner owner exits'); before = count('launches'); time.sleep(.7); assert count('launches') == before
    assert (session/'session.revoked').exists() and (session/'service-active').exists()
    assert settings.read_text() == 'original settings\n'
    # A stopped session no longer binds its location.
    p = subprocess.run([str(image/'mcu'), 'status'], env=env, capture_output=True, text=True, timeout=60)
    assert 'motor_follow=off' in p.stdout and 'belongs to' not in p.stderr, (p.stdout, p.stderr)
    print(f'PASS [{MODE}] actual owner: crash and generic exit do not restart; explicit stop revokes before launch and during handoff; exact child waits, settings recovery and text incomplete-stop evidence (native protocol substituted)')
    # Simulate power-off: tmpfs vanishes entirely; the session folder on /data stays
    # and the next boot's start archives it, including the bridge's last lines.
    with (session/'bridge.log').open('a') as log: log.write('INPUT STOP error=10 reason=source_failure pending_request=1\n')
    last_bridge = (session/'bridge.log').read_text(); last_id = (session/'session.id').read_text()
    shutil.rmtree(runtime)
    (session/'session.boot-id').write_text('previous-boot\n'); names = set(entries()); run('start')
    rebooted = history/sorted(set(entries())-names)[0]
    assert (rebooted/'bridge.log').read_text() == last_bridge and last_bridge.endswith('pending_request=1\n') and (rebooted/'session.id').read_text() == last_id
    assert (rebooted/'session.boot-id').read_text() == 'previous-boot\n' and (rebooted/'mpc.exit').exists() and not (rebooted/'command-status.json').exists()
    assert (state/'command.state').exists() and not (session/'command.state').exists()
    (session/'project-ready').touch(); until(lambda: not (session/'manual-adapter-unarmed.pid').exists(), 'reboot session arming'); run('stop')
    no_raw_state()
    assert len(entries()) <= 8 and du(history) <= 16777216
    print(f'PASS [{MODE}] real tmpfs admission, unchanged mappings on bridge restart, power-off evidence archived from /data at the next boot, no raw state on /data (boot id and native app substituted)')

    # History budget with injected small constants.
    budget = 400000
    def small(**extra):
        values = dict(history_budget=budget, history_entries=4, cap_mpc_log=65536, cap_status_head=8192, cap_status_tail=2048)
        values.update(extra)
        (dev/'mcu-session.sh').write_text(session_source(**values)); manifest(dev)
    def within():
        assert du(history) <= budget, (du(history), entries())
        assert not (history/'.new').exists()
    small()
    for e in entries(): shutil.rmtree(history/e)
    for name in ('000001-aaaaaaaa-old', '000002-aaaaaaaa-old'):
        (history/name).mkdir(mode=0o700); (history/name/'bridge.log').write_bytes(b'o'*170000)
    (history/'.new').mkdir(); (history/'.new'/'partial').write_bytes(b'p'*500000)
    (session/'status-events').write_text('3000')
    run('start')
    within()
    assert '000001-aaaaaaaa-old' not in entries() and '000002-aaaaaaaa-old' in entries(), entries()
    newest = history/entries()[-1]; assert newest.name.startswith('000003-')
    cut = (newest/'command-status.json').read_text()
    assert cut.startswith('{"format":"CMD31","error":0,"requests":[{"slot":0,"rejected":0}],"events":[') and '[mpclearn history: ' in cut
    assert cut.endswith('"admission":{"category":"none","raw_result":0,"site":77}}\n') and 'original_bytes=' in (newest/'command-status.json.truncated').read_text()
    assert len(cut) < 8192+2048+200 and (newest/'mpc.log').stat().st_size == 65536
    print(f'PASS [{MODE}] history budget: crash leftover removed, oldest pruned before the write, du <= budget, head-and-tail status summary keeps the closing admission object')
    # Zero-padded 000008/000009 are invalid octal: the sequence must stay decimal.
    for n in (8, 9, 10, 11, 12, 13, 14):
        (history/f'{n:06d}-aaaaaaaa-tiny').mkdir(mode=0o700)
    (session/'project-ready').touch(); until(lambda: count('armed') >= 1 and not (session/'manual-adapter-unarmed.pid').exists(), 'budget session arms')
    for _ in range(3):
        before = count('launches'); previous = app_pid(); depart(True)
        until(lambda: count('launches') == before+1 and app_pid() != previous and (session/'generation.ready').exists(), 'handoff within budget')
        within(); assert len(entries()) <= 4, entries()
        (session/'project-ready').touch()
    seq = [int(e.split('-')[0]) for e in entries()]; assert seq == sorted(seq) and seq[-1] == 17, entries()
    # A write that fails refuses the successor launch; nothing is retried.
    shutil.rmtree(history); history.write_text('not a folder')
    before = count('launches'); depart(True); until(owner_gone, 'owner refuses successor after failed archive')
    assert count('launches') == before and (session/'mpc.pid').exists()
    history.unlink(); history.mkdir(mode=0o700)
    run('stop', 1)
    # An entry that cannot fit alone is dropped without pruning anything.
    (history/'000001-aaaaaaaa-keep').mkdir(mode=0o700)
    (history/'.new').mkdir(); (history/'.new'/'partial').write_bytes(b'p'*100)  # removed even when nothing is pruned
    small(history_budget=40000)
    p = run('start'); assert 'exceeds the budget; not retained' in p.stderr, p.stderr
    assert entries() == ['000001-aaaaaaaa-keep'] and du(history) <= 40000
    (session/'project-ready').touch(); until(lambda: not (session/'manual-adapter-unarmed.pid').exists(), 'oversize session arms')
    run('stop')
    no_raw_state()
    print(f'PASS [{MODE}] history budget: entry count bound over handoffs, failed write refuses the successor, oversized entry dropped without pruning')
    # Explicit one-session selection remains available and exact names,
    # including spaces, survive the existing owner and New Project respawn.
    before = count('launches')
    call(dev/'mcu', 'start', '--profile=generic', '--endpoint-client=Virtual MCU', '--endpoint-port=Port Name', '--endpoint-type=any')
    until(lambda: count('launches') == before+1 and (session/'generation.ready').exists(), 'generic selected session starts')
    selected = (session/'bridge.log').read_text()
    for arg in ('ARG --profile=generic', 'ARG --endpoint-client=Virtual MCU', 'ARG --endpoint-port=Port Name', 'ARG --endpoint-type=any'):
        assert arg in selected, (arg, selected)
    assert native_values()[39:41] == (1, 3), native_values()[39:41]  # saved X-Touch, active one-session Generic
    previous = app_pid(); depart(True)
    until(lambda: count('launches') == before+2 and app_pid() != previous and (session/'generation.ready').exists(), 'generic selected New Project respawn')
    selected = (session/'bridge.log').read_text()
    for arg in ('ARG --profile=generic', 'ARG --endpoint-client=Virtual MCU', 'ARG --endpoint-port=Port Name', 'ARG --endpoint-type=any'):
        assert arg in selected, (arg, selected)
    assert native_values()[39:41] == (1, 3), native_values()[39:41]
    run('stop')
    no_raw_state()
    # The same session owner writes one bounded preference and a no-option
    # start consumes it. Invalid replacement leaves the prior selection intact.
    call(dev/'mcu', 'configure', '--profile=generic', '--endpoint-client=Saved MCU', '--endpoint-port=Saved Port', '--endpoint-type=any')
    controller_preference = home/'controller-preferences'
    assert controller_preference.stat().st_size == 8992 and controller_preference.stat().st_mode & 0o777 == 0o600
    p = call(dev/'mcu', 'configure', '--profile=generic', expected=2)
    assert 'The selected protocol requires exact endpoint client and port names.' in p.stderr
    before = count('launches'); call(image/'mcu-boot.sh', 'start')
    until(lambda: count('launches') == before+1 and (session/'generation.ready').exists(), 'saved generic boot session starts')
    selected = (session/'bridge.log').read_text()
    assert 'ARG --controller-preferences=/data/mpclearn/controller-preferences' in selected and 'SAVED profile=generic endpoint-type=any client=Saved MCU port=Saved Port' in selected, selected
    previous = app_pid(); depart(True)
    until(lambda: count('launches') == before+2 and app_pid() != previous and (session/'generation.ready').exists(), 'saved generic New Project respawn')
    assert 'SAVED profile=generic endpoint-type=any client=Saved MCU port=Saved Port' in (session/'bridge.log').read_text()
    run('stop'); no_raw_state()
    before = count('launches')
    call(dev/'mcu', 'configure', '--profile=generic', '--input-client=Split Controller', '--input-port=From Port', '--input-type=kernel', '--output-client=Split Controller', '--output-port=To Port', '--output-type=user')
    call(dev/'mcu', 'start')
    until(lambda: count('launches') == before+1 and (session/'generation.ready').exists(), 'saved split generic session starts')
    selected = (session/'bridge.log').read_text()
    assert 'SAVED profile=generic endpoint-type=kernel client=Split Controller port=From Port' in selected and 'to-endpoint-type=user to-client=Split Controller to-port=To Port' in selected, selected
    run('stop'); no_raw_state()
    # Native DAW/CE programs use manual exact port selection through one owner.
    for profile in ('lcxl3-daw', 'launchpad-x-programmer', 'launchpad-pro-mk3-programmer', 'mpk-mini-mk3', 'lpd8-mk2', 'mpk249'):
        call(dev/'mcu', 'configure', '--profile='+profile, expected=2)
        call(dev/'mcu', 'configure', '--profile='+profile,
             '--input-client=Native DAW', '--input-port=From DAW', '--input-type=user',
             '--output-client=Native DAW', '--output-port=To DAW', '--output-type=user')
        before = count('launches'); call(dev/'mcu', 'start')
        until(lambda: count('launches') == before+1 and (session/'generation.ready').exists(), 'saved native DAW session starts')
        selected = (session/'bridge.log').read_text()
        assert 'SAVED profile='+profile+' endpoint-type=user client=Native DAW port=From DAW' in selected, selected
        assert 'to-endpoint-type=user to-client=Native DAW to-port=To DAW' in selected, selected
        run('bridge-stop')
        call(dev/'mcu-session.sh', 'bridge-start', '--profile='+profile,
             '--input-client=Native DAW', '--input-port=From DAW', '--input-type=user',
             '--output-client=Native DAW', '--output-port=To DAW', '--output-type=user')
        assert (session/'bridge.pid').exists()
        run('stop'); no_raw_state()
    print(f'PASS [{MODE}] six manual controller profiles: split saved ports, no-port refusal, startup and explicit bridge recovery through the existing owner (native MPC/ALSA substituted)')
    # HUI uses the same fixed preference owner and exact typed endpoint handoff.
    # The mode is explicit data; it is never inferred from the endpoint name.
    before = count('launches')
    p = call(dev/'mcu', 'configure', '--profile=hui', '--endpoint-client=HUI Software', '--endpoint-port=DAW Port', '--endpoint-type=any', expected=2)
    assert 'HUI requires an exact kernel or user endpoint type.' in p.stderr
    call(dev/'mcu', 'configure', '--profile=hui', '--endpoint-client=HUI Software', '--endpoint-port=DAW Port', '--endpoint-type=user', '--hui-mode=motorized')
    call(dev/'mcu', 'start')
    until(lambda: count('launches') == before+1 and (session/'generation.ready').exists(), 'saved HUI session starts')
    selected = (session/'bridge.log').read_text()
    assert 'SAVED profile=hui endpoint-type=user client=HUI Software port=DAW Port hui-mode=motorized' in selected, selected
    previous = app_pid(); depart(True)
    until(lambda: count('launches') == before+2 and app_pid() != previous and (session/'generation.ready').exists(), 'saved HUI New Project respawn')
    assert 'SAVED profile=hui endpoint-type=user client=HUI Software port=DAW Port hui-mode=motorized' in (session/'bridge.log').read_text()
    run('stop'); no_raw_state()
    make_hui_legacy_pickup(controller_preference); before = count('launches'); call(dev/'mcu', 'start')
    until(lambda: count('launches') == before+1 and (session/'generation.ready').exists(), 'old HUI preference session starts')
    assert 'SAVED profile=hui endpoint-type=user client=HUI Software port=DAW Port hui-mode=pickup' in (session/'bridge.log').read_text()
    run('stop'); no_raw_state()
    before = count('launches')
    call(dev/'mcu', 'configure', '--profile=xtouch-mini')
    call(dev/'mcu', 'start')
    until(lambda: count('launches') == before+1 and (session/'generation.ready').exists(), 'Mini selected session starts')
    selected = (session/'bridge.log').read_text()
    assert 'SAVED profile=xtouch-mini endpoint-type=kernel client=X-TOUCH MINI port=X-TOUCH MINI MIDI 1' in selected, selected
    adapter_pid = int((session/'manual-adapter-unarmed.pid').read_text().split()[0])
    assert os.readlink(f'/proc/{adapter_pid}/exe').endswith('/mpclearn-controls')
    previous = app_pid(); depart(True)
    until(lambda: count('launches') == before+2 and app_pid() != previous and (session/'generation.ready').exists(), 'Mini selected New Project respawn')
    selected = (session/'bridge.log').read_text()
    assert 'SAVED profile=xtouch-mini endpoint-type=kernel client=X-TOUCH MINI port=X-TOUCH MINI MIDI 1' in selected, selected
    adapter_pid = int((session/'manual-adapter-unarmed.pid').read_text().split()[0])
    assert os.readlink(f'/proc/{adapter_pid}/exe').endswith('/mpclearn-controls')
    call(dev/'mcu', 'configure', '--profile=xtouch')
    selected = (session/'bridge.log').read_text(); assert 'SAVED profile=xtouch endpoint-type=kernel client=X-Touch port=X-TOUCH_INT' in selected, selected
    run('stop')
    no_raw_state()
    p = subprocess.run([str(dev/'mcu'), 'status'], env=env, capture_output=True, text=True, timeout=60); assert 'profile=xtouch endpoint-type=kernel' in p.stdout
    print(f'PASS [{MODE}] controller selection: no-setting X-Touch default, fixed MCUCFG2 collection, split From/To ports, bounded saved generic/HUI/Mini selection across boot and New Project, old MCUCFG1 Pickup compatibility, exact rollback, live return to X-Touch, invalid replacement refusal, and separate normal-MIDI adapter (native ALSA/MPC substituted)')
    before = count('launches'); run('start')
    until(lambda: count('launches') == before+1 and (session/'generation.ready').exists(), 'collection test starts')
    sequence = native_request(1, kind=4, enabled=0)
    until(lambda: native_values()[28] == sequence and native_values()[38] == 3, 'last enabled record disables')
    assert native_values()[40] == 0 and not (session/'bridge.pid').exists()
    disabled_preference = controller_preference.read_bytes()
    (session/'bridge-start-fail').touch(); sequence = native_request(1, kind=4, enabled=1)
    until(lambda: native_values()[28] == sequence and native_values()[38] == 8, 'failed enable restores stopped disabled collection')
    assert controller_preference.read_bytes() == disabled_preference and not (session/'bridge.pid').exists() and not (session/'bridge.exit').exists()
    sequence = native_request(1, kind=3)
    until(lambda: native_values()[28] == sequence and native_values()[38] == 3, 'last record removed')
    assert native_values()[53] == 0 and not (session/'bridge.pid').exists()
    run('stop'); before = count('launches'); run('start')
    until(lambda: count('launches') == before+1 and (session/'generation.ready').exists(), 'empty collection survives startup')
    assert native_values()[53] == 0 and not (session/'bridge.pid').exists()
    (session/'project-ready').touch(); run('status'); time.sleep(1)
    assert not (session/'generation.failed').exists(), 'empty collection must not fail adapter arming'
    sequence = native_request(1, controller_index=0xffffffff)
    until(lambda: native_values()[28] == sequence and native_values()[40] == 1 and (session/'armed').exists(), 'Add starts and arms from an empty collection')
    run('stop'); no_raw_state()
    print(f'PASS [{MODE}] Controllers collection: disable last, failed enable restores exact stopped bytes, remove last, empty startup/status, Add restarts and arms through the sole session owner (collection helper, MPC and ALSA substituted)')
    # The getter probe owns one exact development-only tmpfs record. Ordinary
    # packages still reject that name. A package with the exact define admits
    # only its root-owned 0600/256-byte record through status, idempotent start
    # and stop; unknown or malformed files remain refusals.
    probe_state = state/'plugin-host-probe.state'
    probe_state.write_bytes(bytes(256)); probe_state.chmod(0o600)
    run('status', 1); probe_state.unlink()
    normal_config = (dev/'config.h').read_text()
    (dev/'config.h').write_text(normal_config + f'#define PLUGIN_HOST_PROBE_STATE "{probe_state}"\n'); manifest(dev)
    before = count('launches'); run('start')
    until(lambda: count('launches') == before+1 and (session/'generation.ready').exists(), 'configured getter-probe session starts')
    probe_state.write_bytes(bytes(255)); probe_state.chmod(0o600); run('status', 1)
    probe_state.write_bytes(bytes(256)); probe_state.chmod(0o644); run('status', 1)
    probe_state.chmod(0o600); run('status', 3); run('start'); assert count('launches') == before+1
    (state/'unexpected-probe-neighbor').touch(); run('status', 1); (state/'unexpected-probe-neighbor').unlink()
    run('stop'); assert probe_state.exists()  # retained with other raw state until the next owned archive
    before = count('launches'); run('start')
    until(lambda: count('launches') == before+1 and (session/'generation.ready').exists(), 'configured getter-probe state is archived before restart')
    assert not probe_state.exists(); run('stop'); no_raw_state()
    (dev/'config.h').write_text(normal_config); manifest(dev)
    print(f'PASS [{MODE}] getter probe state: ordinary rejection, exact configured 0600/256 admission through status/idempotent start/stop, next-start archive cleanup, malformed/unknown refusal (native getter route substituted)')
    # Override removal goes through our scratch folder only.
    small(); call(image/'mcu-boot-install.sh', 'override', 'clear')
    assert not dev.exists() and sorted(p.name for p in home.iterdir()) == ['controller-preferences', 'history', 'session', 'surface-preferences'], list(home.iterdir())
    # Nothing outside /data/mpclearn and /run/mpclearn changed, although earlier
    # release folders and the old selector were present throughout.
    assert outside() == before_outside
    print(f'PASS [{MODE}] explicit override clear; earlier release folders, old boot/image folders and old selector untouched and unused')
finally:
    cleanup()
