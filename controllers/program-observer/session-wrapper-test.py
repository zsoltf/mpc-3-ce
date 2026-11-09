#!/usr/bin/env python3
"""Container-only production session test under an acvs runner. Native MPC,
protocol, route, ALSA, systemd and journald are substitutes; the real image
launcher (mcu-boot.sh), session package (mcu-session.sh behind mcu), stock
az01-launch-MPC and shell/flock/process identity/archive paths run.

The acvs runner reads Akai's acvs.service from the stock root and the shipped
drop-in (acvs-mpclearn.conf), runs ExecStart, restarts on every exit
(Restart=always, systemd's default start limit), stops the whole process group
like KillMode=control-group, and runs ExecStopPost with SERVICE_RESULT,
EXIT_CODE and EXIT_STATUS. systemctl and journalctl are clients of it. Named
injections: a follower pause, a bridge pre-identity failure, launch-prepare
failure/hang and one forced dispatcher shell error, each behind a flag file,
plus smaller history constants. Run as root in a container with an init and a
tmpfs /run, with the stock 3.10 root image:

  docker run --rm --init --network none --tmpfs /run \\
    -v "$PWD/controllers/program-observer:/src:ro" \\
    -v "$PWD/build/310-recipes.3vgSMj/rootfs.original.ext:/stock.ext:ro" \\
    mpclearn-build:local python3 /src/session-wrapper-test.py --stock-root /stock.ext

With --busybox (and the same root), the image scripts run under that image's
BusyBox shell and applets through qemu-arm, as on the device.
"""
import ctypes, hashlib, json, mmap, os, pathlib, platform, shlex, shutil, signal, socket, struct, subprocess, sys, threading, time
P = pathlib.Path
assert P('/.dockerenv').exists() and os.getuid() == 0
home = P('/data/mpclearn'); dev = home/'dev'; session = home/'session'; history = home/'history'
image = P('/usr/share/mpclearn/mcu')
runtime = P('/run/mpclearn'); state = runtime/'state'; decision = runtime/'launch.decision'
settings = P('/media/az01-internal/Settings/MPC/MPC.settings')
settings.parent.mkdir(parents=True); settings.write_text('original settings\n')
assert subprocess.run(['stat', '-f', '-c', '%T', '/run'], capture_output=True, text=True).stdout.strip() == 'tmpfs', 'run with --tmpfs /run'
NAMES = 'command-observer.so command-client mirror-input mirror-read config.h mcu-session.sh mcu mpclearn-controls main-button'.split()
EXECUTABLES = 'command-client mirror-input mirror-read mcu-session.sh mcu mpclearn-controls main-button plugin-setup'.split()
PLUGINS = ['plugin-setup', 'plugin-bundle.json']

def option(name):
    if name not in sys.argv: return None
    return sys.argv[sys.argv.index(name)+1]
rootfs = option('--busybox') or option('--stock-root')
assert rootfs, 'pass --stock-root ROOTFS (the stock 3.10 root image), or --busybox ROOTFS'
device = P('/tmp/device-root'); device.mkdir()
for part in ('/usr', '/lib'):
    subprocess.run(['debugfs', '-R', f'rdump {part} {device}', rootfs], check=True, capture_output=True)
shell = '/bin/sh'
env = dict(os.environ)
if '--busybox' in sys.argv:
    busybox = device/'usr/bin/busybox.nosuid'
    bb = P('/tmp/bb'); bb.mkdir()
    for applet in 'sh awk basename cat chmod chown cmp cp cut dd dirname env find flock grep head id kill ls mkdir mkfifo mv printf readlink rm sed sha256sum sleep sort stat tail test touch tr wc'.split():
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

# Named injections, each inert unless its flag file exists.
PAUSE = ('follower(){\n exec 9>session.lock', 'follower(){\n while [ -f pause-follower ];do sleep .05;done\n exec 9>session.lock')
PRE_IDENTITY_FAILURE = (
    'write_identity(){ p=$1;[ -r "/proc/$p/stat" ] || return 1;',
    'write_identity(){ p=$1;if [ "$2" = bridge.pid ] && [ -f bridge-pre-identity-fail ];then rm -f bridge-pre-identity-fail;sleep .1;return 1;fi;[ -r "/proc/$p/stat" ] || return 1;')
PREPARE_FAULTS = ('launch_prepare(){\n', 'launch_prepare(){\n [ ! -f /tmp/prepare-fail ] || return 1\n [ ! -f /tmp/prepare-hang ] || sleep 60 9>&-\n')
SHELL_ERROR = ('launch_decision(){\n', 'launch_decision(){\n [ ! -f /tmp/force-shell-error ] || echo "$forced_unset_variable"\n')
CE_EXEC_FAILURE = (" reason='CE exec failed'\n", " reason='CE exec failed'\n [ ! -f /tmp/ce-exec-fail ] || exec /nonexistent/setarch\n")
# sysfs and procfs cannot be faked here: the mux state and the ALSA card list
# are read from fixture files. Akai's switch tool is a recording substitute.
USB_PATHS = [('usbmux=/sys/devices/platform/usb-mux/state\n', 'usbmux=/tmp/usb-mux/state\n'),
             ('asound_cards=/proc/asound/cards\n', 'asound_cards=/tmp/asound-cards\n'),
             ('block_root=/sys/class/block\n', 'block_root=/tmp/block\n'),
             ('sata_gpio=/sys/class/gpio/gpio237\n', 'sata_gpio=/tmp/gpio237\n'),
             ('sata_reset_dt=/sys/firmware/devicetree/base/pinctrl/gpio7@ff7e0000/sata-reset-asserted\n', 'sata_reset_dt=/tmp/sata-reset-asserted\n')]
def session_source(**constants):
    replacements = [PAUSE, PRE_IDENTITY_FAILURE, PREPARE_FAULTS]
    for name, value in constants.items():
        line = next(l for l in P('/src/mcu-session.sh').read_text().splitlines() if l.startswith(name+'='))
        replacements.append((line+'\n', f'{name}={value}\n'))
    return script_text('/src/mcu-session.sh', replacements)

P('/tmp/session-stub.c').write_text(r'''#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/resource.h>
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
/* The preload's admission, substituted: only LD_PRELOAD naming exactly one
   package's observer loads CE; it is consumed, and the shared state is created
   exclusively, so a leftover from an earlier instance leaves this one without CE. */
static int create_state(const char*path){int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0)return -1;close(fd);return 0;}
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
if(chdir("/data/mpclearn/session"))return 92;
prctl(PR_SET_NAME,"MPC Main Thread",0,0,0);signal(SIGTERM,stop);
char preload[512];snprintf(preload,sizeof preload,"%s",getenv("LD_PRELOAD")?getenv("LD_PRELOAD"):"");
int ce=!strcmp(preload,"/usr/share/mpclearn/mcu/command-observer.so")||!strcmp(preload,"/data/mpclearn/dev/command-observer.so");
if(ce){unsetenv("LD_PRELOAD");if(create_state("/run/mpclearn/state/command.state")||create_state("/run/mpclearn/state/volume.state"))ce=-1;}
struct rlimit stack;getrlimit(RLIMIT_STACK,&stack);
FILE*f=fopen("/tmp/launches","a");fprintf(f,"%s pid=%d preload=%s stack=%lu tunables=%s arena=%s cardinal=%s marker=%s\n",ce>0?"CE":ce<0?"CE-refused":"stock",(int)getpid(),preload[0]?preload:"-",(unsigned long)stack.rlim_cur,getenv("GLIBC_TUNABLES")?getenv("GLIBC_TUNABLES"):"-",getenv("MALLOC_ARENA_MAX")?getenv("MALLOC_ARENA_MAX"):"-",getenv("MPCLEARN_CARDINAL_RESOURCES")?getenv("MPCLEARN_CARDINAL_RESOURCES"):"-",getenv("ACVS_MARKER")?getenv("ACVS_MARKER"):"-");fclose(f);
if(ce>0){
 unlink("project-ready");unlink("native-intent");unlink("closed");
 if(!access("start-project-ready",F_OK)){f=fopen("project-ready","w");if(f)fclose(f);}
 f=fopen("/run/mpclearn/state/command.state","w");fprintf(f,"%u\n",(unsigned)getpid());fclose(f);
 int nfd=open("/run/mpclearn/state/native-preferences.state",O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(nfd<0||ftruncate(nfd,NATIVE_PREFERENCES_BYTES))return 90;NativePreferencesState*ns=mmap(NULL,NATIVE_PREFERENCES_BYTES,PROT_READ|PROT_WRITE,MAP_SHARED,nfd,0);close(nfd);if(ns==MAP_FAILED)return 91;memset(ns,0,NATIVE_PREFERENCES_BYTES);ns->magic=NATIVE_PREFERENCES_MAGIC;ns->version=NATIVE_PREFERENCES_VERSION;ns->bytes=NATIVE_PREFERENCES_BYTES;ns->pid=getpid();munmap(ns,NATIVE_PREFERENCES_BYTES);
}
for(int i=0;i<1100000;i++)putchar('x');puts(" MPC diagnostic tail");fflush(stdout);
while(!stopped){f=fopen("exit-request","r");if(f){char word[16]={0};fscanf(f,"%15s",word);fclose(f);unlink("exit-request");if(!strcmp(word,"kill"))raise(SIGKILL);return atoi(word);}usleep(10000);}return 0;}
''')
subprocess.run(['gcc', '-D_GNU_SOURCE', '/tmp/session-stub.c', '-o', '/usr/bin/MPC'], check=True)
subprocess.run(['gcc', '-shared', '-fPIC', '-x', 'c', '-', '-o', '/usr/lib/mpclearn-test-addin.so'], input='int addin_marker;\n', text=True, check=True)
stub_bin = P('/tmp/session-bin'); stub_bin.mkdir()
def script(p, text, mode=0o700):
    p.write_text('#!/bin/sh\n'+text); p.chmod(mode)
EVENTS = P('/tmp/events'); JOURNAL = P('/tmp/journal'); JOURNAL.mkdir()
def event(text):
    with EVENTS.open('a') as f: f.write(text+'\n')
script(stub_bin/'setarch', 'shift;shift;exec "$@"\n')
script(stub_bin/'dfu-util', 'test "$1" = -l || exit 2\necho \'Found DFU: [0483:df11] ver=2200, devnum=1, cfg=1, intf=0, path="1-1", alt=0, name="@Internal Flash  /0x08000000/04*016Kg"\' >/dev/null\ntest ! -f /tmp/dfu-present || echo \'Found DFU: [0483:df11] name="@Internal Flash  /0x08000000/04*016Kg"\'\n')
script(stub_bin/'udevadm', 'case "$*" in *--name=/dev/sdb1*) echo ID_DRIVE_FLASH_SD=1;; esac\n')
script(stub_bin/'journalctl', 'for a;do case "$a" in _PID=*) p=${a#_PID=};; esac;done\ncat "/tmp/journal/$p.log" 2>/dev/null\n')
inhibit_bin = P('/tmp/inhibit-bin'); inhibit_bin.mkdir()
# Internal USB as after a boot: MPC audio and MIDI, an X-Touch on a USB-A port,
# and the SD card with the plugin bundle (edisksd's mount, substituted).
P('/tmp/usb-mux').mkdir(); P('/tmp/usb-mux/state').write_text('internal\n')
INTERNAL_CARDS = (' 0 [Audio          ]: USB-Audio - MPC Live II Audio\n                      Akai Pro MPC Live II Audio at usb-ff500000.usb-1.5, high speed\n'
                  ' 1 [II             ]: USB-Audio - MPC Live II\n                      Akai Pro MPC Live II at usb-ff500000.usb-1.6, full speed\n'
                  ' 2 [XTouch         ]: USB-Audio - X-Touch\n                      BEHRINGER X-Touch at usb-ff500000.usb-1.2.3, full speed\n')
P('/tmp/asound-cards').write_text(INTERNAL_CARDS); P('/tmp/asound-cards.internal').write_text(INTERNAL_CARDS)
card_bundle = P('/media/MPC Card/MPCLEARN/plugins.squashfs'); card_bundle.parent.mkdir(parents=True); card_bundle.write_bytes(b'bundle')
# The card reader's partition in sysfs (removable disk) and udev's view of it.
P('/tmp/devices/sdb/sdb1').mkdir(parents=True); P('/tmp/devices/sdb/removable').write_text('1\n'); P('/tmp/block').mkdir()
P('/tmp/block/sdb1').symlink_to('/tmp/devices/sdb/sdb1')
def card_present(present):
    f = P('/tmp/devices/sdb/sdb1/partition')
    if present: f.write_text('1\n')
    elif f.exists(): f.unlink()
card_present(True)
P('/tmp/gpio237').mkdir(); P('/tmp/gpio237/value').write_text('0\n')
# Akai's az01-usbmux-switch, substituted: same no-op when the state matches;
# switching to internal brings the devices back after 1 s, as the hub
# re-enumerates. Flags inject a failure, a hang and devices that never return.
P('/usr/bin/az01-usbmux-switch').write_text(f'''#!/bin/sh
for exe in /proc/[0-9]*/exe;do [ "$(readlink "$exe" 2>/dev/null)" != /usr/bin/MPC ] || echo "usbmux $1 with MPC running" >>/tmp/events;done
echo "usbmux $1" >>/tmp/events
test "$1" = "$(cat /tmp/usb-mux/state)" && exit
test ! -f /tmp/usbmux-fail || exit 1
test ! -f /tmp/usbmux-hang || {{ test -e /tmp/sata-reset-asserted || echo 1 >/tmp/gpio237/value;sleep 30; }}
printf '%s\\n' "$1" >/tmp/usb-mux/state
test -f /tmp/usb-never-back || (sleep 1;cp /tmp/asound-cards.internal /tmp/asound-cards;echo 1 >/tmp/devices/sdb/sdb1/partition) >/dev/null 2>&1 &
''')
P('/usr/bin/az01-usbmux-switch').chmod(0o755)
def controller_mode_usb():
    """What the Controller-mode MPC does at start: USB external, devices gone."""
    P('/tmp/usb-mux/state').write_text('external\n'); P('/tmp/asound-cards').write_text(' 3 [Dummy          ]: Dummy - Dummy\n')
    card_present(False)
    if card_bundle.exists(): card_bundle.unlink()
def usb_events(since): return [l for l in EVENTS.read_text().splitlines()[since:] if l.startswith('usbmux')]
updater = P('/usr/share/Akai/SME0/Firmware/update.sh'); updater.parent.mkdir(parents=True)
updater.write_text('echo dfu-update >>/tmp/dfu-updates\n')
# Under qemu's -L prefix the device root's own updater would be found first.
(device/str(updater).lstrip('/')).write_text(updater.read_text())
(stub_bin/'systemctl').write_text('#!/usr/bin/env python3\nimport json,socket,sys\n'
    's=socket.socket(socket.AF_UNIX);s.connect("/tmp/systemctl.sock");s.sendall(json.dumps(sys.argv[1:]).encode()+b"\\n");s.shutdown(socket.SHUT_WR)\n'
    'data=b""\nwhile True:\n chunk=s.recv(65536)\n if not chunk: break\n data+=chunk\n'
    'r=json.loads(data);sys.stdout.write(r["out"]);sys.exit(r["code"])\n')
(stub_bin/'systemctl').chmod(0o700)
env['PATH'] = f'{stub_bin}:{inhibit_bin}:' + env['PATH']

# The stock launcher and unit, as shipped in the stock root.
stock_launcher = (device/'usr/bin/az01-launch-MPC').read_text()
P('/usr/bin/az01-launch-MPC').write_text(stock_launcher.replace('#!/bin/sh\n', f'#!{shell}\n', 1)); P('/usr/bin/az01-launch-MPC').chmod(0o755)
STOCK_UNIT = (device/'usr/lib/systemd/system/acvs.service').read_text()
DROP_IN = P('/src/acvs-mpclearn.conf').read_text()

def unit_commands():
    """Akai's unit with the shipped drop-in applied, as systemd merges them."""
    start, stoppost, restart, kind = [], [], None, 'simple'
    for text in (STOCK_UNIT, DROP_IN):
        section = None
        for line in text.splitlines():
            line = line.strip()
            if not line or line.startswith('#'): continue
            if line.startswith('['): section = line; continue
            key, value = line.split('=', 1)
            if section != '[Service]': continue
            if key == 'ExecStart': start = [] if not value else start+[value]
            elif key == 'ExecStopPost': stoppost = [] if not value else stoppost+[value]
            elif key == 'Restart': restart = value
            elif key == 'Type': kind = value
            else: raise AssertionError(('unemulated acvs setting', key))
    assert len(start) == 1 and restart == 'always' and kind == 'simple', (start, restart, kind)
    return shlex.split(start[0]), [shlex.split(c.lstrip('-')) for c in stoppost]

class Acvs:
    """acvs.service as systemd runs it: one main process per instance."""
    def __init__(self):
        self.start_cmd, self.stoppost = unit_commands()
        self.job = threading.RLock(); self.proc = None; self.starts = []; self.failed = False
        self.extra_env = {}; self.system_stopping = False; self.calls = []; self.invocation = 0; self.boot_unit_active = False
    def env(self, extra=()):
        e = dict(env); e.update(self.extra_env); e['INVOCATION_ID'] = f'{self.invocation:032x}'; e.update(extra); return e
    def _spawn(self, automatic=False):
        # systemd's default start limit (5 starts in 10 s), counted over the
        # automatic restarts only: the test starts and stops acvs faster than
        # any person, and the limit is here to catch a launch failure loop.
        now = time.monotonic(); self.starts = [t for t in self.starts if now-t < 10]+([now] if automatic else [])
        if len(self.starts) > 5:
            self.failed = True; event('acvs start-limit-hit'); return
        self.invocation += 1
        log = JOURNAL/f'invocation-{self.invocation}.log'
        with log.open('wb') as out:
            p = subprocess.Popen(self.start_cmd, env=self.env(), stdout=out, stderr=subprocess.STDOUT, start_new_session=True, cwd='/')
        link = JOURNAL/f'{p.pid}.log'
        if link.is_symlink() or link.exists(): link.unlink()
        link.symlink_to(log)
        self.proc = p; event(f'acvs started pid={p.pid}')
        threading.Thread(target=self._watch, args=(p,), daemon=True).start()
    def _watch(self, p):
        p.wait()
        with self.job:
            if self.proc is not p: return  # a stop owns this exit
            self._closed(p, False)
            if self.failed: return
            time.sleep(.1)  # RestartSec default
            self._spawn(automatic=True)
    def _closed(self, p, stopping):
        code = p.returncode
        if code >= 0: exit_code, status, result = 'exited', str(code), 'success' if code == 0 else 'exit-code'
        else:
            sig = signal.Signals(-code); exit_code, status = 'killed', sig.name[3:]
            result = 'success' if stopping and sig in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP, signal.SIGPIPE) else 'signal'
        for sig in (signal.SIGTERM, signal.SIGKILL):  # KillMode=control-group
            try: os.killpg(p.pid, sig)
            except ProcessLookupError: break
            time.sleep(.2)
        self.proc = None
        event(f'acvs exited pid={p.pid} result={result} code={exit_code} status={status}')
        for cmd in self.stoppost:
            with (JOURNAL/'close.log').open('ab') as out:
                r = subprocess.run(cmd, env=self.env({'SERVICE_RESULT': result, 'EXIT_CODE': exit_code, 'EXIT_STATUS': status}), stdout=out, stderr=subprocess.STDOUT, timeout=90, cwd='/')
            event(f'acvs close pid={p.pid} status={r.returncode}')
    def start(self):
        with self.job:
            if self.failed: return 1
            if self.proc is None: self._spawn()
            return 1 if self.failed else 0
    def stop(self):
        with self.job:
            p = self.proc
            if p is None: return 0
            self.proc = None
            try: os.killpg(p.pid, signal.SIGTERM)
            except ProcessLookupError: pass
            try: p.wait(timeout=90)
            except subprocess.TimeoutExpired: os.killpg(p.pid, signal.SIGKILL); p.wait()
            self.proc = p; self._closed(p, True)
            return 0
    def active(self): return self.proc is not None and self.proc.poll() is None
    def main_pid(self):
        p = self.proc; return p.pid if p is not None and p.poll() is None else None
acvs = Acvs()
def systemctl(args):
    acvs.calls.append(list(args)); event('systemctl '+' '.join(args))
    unit = args[-1] if args else ''
    if args == ['is-system-running']: return ('stopping\n', 1) if acvs.system_stopping else ('running\n', 0)
    if unit.startswith('mpclearn-boot'):
        if args[0] == 'is-active': return ('active\n', 0) if acvs.boot_unit_active else ('inactive\n', 3)
        if args[0] == 'stop' and acvs.boot_unit_active:  # its cgroup holds the follower
            acvs.boot_unit_active = False
            try: os.kill(int((session/'follower.pid').read_text().split()[0]), signal.SIGKILL)
            except (FileNotFoundError, ProcessLookupError): pass
        return ('', 0)
    if unit not in ('acvs', 'acvs.service'): return (f'unemulated {args}\n', 2)
    if args[0] == 'is-active': return ('' if '--quiet' in args else ('active\n' if acvs.active() else 'inactive\n'), 0 if acvs.active() else 3)
    if args[0] == 'start': return ('', acvs.start())
    if args[0] == 'stop': return ('', acvs.stop())
    if args[0] == 'restart': acvs.stop(); return ('', acvs.start())
    return (f'unemulated {args}\n', 2)
def serve():
    server = socket.socket(socket.AF_UNIX); server.bind('/tmp/systemctl.sock'); server.listen(16)
    def handle(c):
        data = b''
        while not data.endswith(b'\n'):
            chunk = c.recv(65536)
            if not chunk: break
            data += chunk
        out, code = systemctl(json.loads(data))
        c.sendall(json.dumps({'out': out, 'code': code}).encode()); c.close()
    while True:
        c, _ = server.accept(); threading.Thread(target=handle, args=(c,), daemon=True).start()
threading.Thread(target=serve, daemon=True).start()
while not P('/tmp/systemctl.sock').exists(): time.sleep(.01)

COMMAND_CLIENT = f'''cd {session}
case "$1" in
 session-status) test "$2" = "{state}/command.state" -a "$3" = "{state}/volume.state" || exit 9;test -f "$2" -a -f "$3" || exit 5;test ! -f source-failed || exit 1;test ! -f source-initializing || exit 5;test ! -f closed || exit 4;test ! -f native-intent || exit 6;test -f project-ready || exit 3;exit 0;;
 status) test "$2" = "{state}/command.state" || exit 9
  awk -v n="$(cat status-events 2>/dev/null || echo 2)" 'BEGIN{{printf "{{\\"format\\":\\"CMD32\\",\\"error\\":0,\\"requests\\":[{{\\"slot\\":0,\\"rejected\\":0}}],\\"events\\":[";for(i=0;i<n;i++)printf "%s{{\\"slot\\":0,\\"lane\\":0,\\"index\\":%d,\\"kind\\":1,\\"lr\\":1234567}}",(i?",":""),i;print "],\\"snapshot_atomic\\":false,\\"new_project_intent\\":0,\\"navigation\\":{{\\"watch\\":0}},\\"admission\\":{{\\"category\\":\\"none\\",\\"raw_result\\":0,\\"site\\":77}}}}"}}';;
 new-project-intent) test -f native-intent || exit 1;test "$2" = "{state}/command.state" || exit 9;test "$(cat "$2")" = "$3" || exit 1;test "$(cat native-intent)" = "$3" || exit 1;;
 stop) test "$2" = "{state}/command.state" -a "$3" = "{state}/volume.state" || exit 9;touch closed;;
 *) exit 2;; esac
'''
MIRROR_READ = f'''test "$1" = "{state}/volume.state" -a "$2" = --all || exit 9
echo '{{"format":"MMV17","stable":true,"fresh":false,"tracks":[]}}';exit 1
'''
# The optional SD plugin admission, substituted: every operation refuses while
# MPC runs; prepare needs the card; ready needs the receipt and a live view;
# release drops both (the real release is firmware/plugin-setup/release-check.sh).
PLUGIN_SETUP = '''for exe in /proc/[0-9]*/exe;do [ "$(readlink "$exe" 2>/dev/null)" != /usr/bin/MPC ] || { echo "plugin-setup $1 refused: MPC running" >>/tmp/events;exit 3; };done
case "$1" in
 prepare) test -f /tmp/devices/sdb/sdb1/partition || { echo 'plugin-setup prepare: no card' >>/tmp/events;echo 'Pd/Cardinal unavailable: expected one SD-card bundle, found 0' >&2;exit 3; };mkdir -p "/media/MPC Card/MPCLEARN";printf bundle >"/media/MPC Card/MPCLEARN/plugins.squashfs";echo 'plugin-setup prepare' >>/tmp/events;touch /tmp/plugins-ready;echo '{}' >/run/mpclearn/plugin-mount.json;;
 ready) test -f /run/mpclearn/plugin-mount.json || { echo 'Pd/Cardinal unavailable: no admission receipt' >&2;exit 3; };test -f /tmp/plugins-ready || { echo 'Pd/Cardinal unavailable: admitted SD mount changed' >&2;exit 3; };;
 release) echo 'plugin-setup release' >>/tmp/events;rm -f /tmp/plugins-ready /run/mpclearn/plugin-mount.json;echo 'Pd/Cardinal SD bundle released';;
 *) exit 2;; esac
'''
def sha(path): return hashlib.sha256(P(path).read_bytes()).hexdigest()
def package_names(folder): return NAMES + (PLUGINS if (folder/'plugin-setup').exists() else [])
def write_package(folder, location, source, plugins=False):
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
    if plugins:
        script(folder/'plugin-setup', PLUGIN_SETUP); (folder/'plugin-bundle.json').write_text('{}\n')
    manifest(folder)
def manifest(folder):
    (folder/'session-package.sha256').write_text(''.join(f'{sha(folder/n)}  {n}\n' for n in package_names(folder)))
def set_modes(folder):
    os.chown(folder, 0, 0); folder.chmod(0o700)
    for n in package_names(folder)+['session-package.sha256']:
        os.chown(folder/n, 0, 0); (folder/n).chmod(0o700 if n in EXECUTABLES else 0o600)

def until(pred, why, seconds=20):
    end = time.monotonic()+seconds
    while time.monotonic() < end:
        try:
            if pred(): return
        except FileNotFoundError:  # a locator between archive and relaunch
            pass
        time.sleep(.05)
    logs = '\n'.join(f'--- {p}\n'+p.read_text(errors='replace')[-3000:] for p in [session/'follower.log', JOURNAL/'close.log', EVENTS, P('/tmp/launches')] if p.exists())
    raise AssertionError(why+'\n'+logs)
def call(path, *args, expected=0):
    p = subprocess.run([str(path), *args], env=env, capture_output=True, text=True, timeout=90)
    assert p.returncode == expected, (str(path), args, p.returncode, p.stdout, p.stderr)
    return p
def run(op, expected=0, where=dev):
    return call(where/('mcu-session.sh' if op.startswith('bridge-') else 'mcu'), op, expected=expected)
def count(name): return len((session/name).read_text()) if (session/name).exists() else 0
def launches(): return P('/tmp/launches').read_text().splitlines() if P('/tmp/launches').exists() else []
def ce_launches(): return sum(1 for l in launches() if l.startswith('CE '))
def last_launch(): return launches()[-1]
def app_pid(): return int((session/'mpc.pid').read_text().split()[0])
def bridge_pid(): return int((session/'bridge.pid').read_text().split()[0])
def follower_gone():
    try: p = int((session/'follower.pid').read_text().split()[0]); return not P(f'/proc/{p}/exe').exists()
    except FileNotFoundError: return True
def ready_instance(previous=None):
    """acvs's main process is the exact CE instance named by mpc.pid, with a ready controller."""
    return (acvs.main_pid() is not None and (session/'mpc.pid').exists() and app_pid() == acvs.main_pid() and app_pid() != previous
            and os.readlink(f'/proc/{app_pid()}/exe') == '/usr/bin/MPC' and (session/'generation.ready').exists())
def depart(intent, code=0):
    # A person cannot quit MPC six times in ten seconds; keep the automatic
    # restarts this test causes under systemd's start limit, so that only a
    # launch failure loop can reach it.
    if acvs.starts: time.sleep(max(0, acvs.starts[-1]+2.1-time.monotonic()))
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
    assert len(data) == 16384 and struct.unpack_from('<III', data) == (0x3355504e, 16, 16384)
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
def stock_since(n, final_branch=True):
    """Every launch after the first n was stock, with the stock launcher's environment."""
    later = launches()[n:]
    return bool(later) and all(l.startswith('stock ') and (not final_branch or ' tunables=glibc.malloc.hugetlb=2 arena=1 ' in l) for l in later)
def run_either(op):
    p = subprocess.run([str(dev/'mcu'), op], env=env, capture_output=True, text=True, timeout=90); assert p.returncode in (0, 1), p; return p
def systemctl_call(*args): return call('/tmp/session-bin/systemctl', *args)
def acvs_calls_during(action):
    before = len(acvs.calls); action(); return acvs.calls[before:]
def boot(previous_boot=False):
    """Shutdown, then a boot: admission before acvs, which systemd then starts."""
    acvs.system_stopping = True; acvs.stop(); acvs.system_stopping = False
    cleanup()
    try: os.kill(int((session/'follower.pid').read_text().split()[0]), signal.SIGKILL)
    except (FileNotFoundError, ProcessLookupError): pass
    if previous_boot:
        shutil.rmtree(runtime)
        if (session/'session.boot-id').exists(): (session/'session.boot-id').write_text('previous-boot\n')
    calls = acvs_calls_during(lambda: call(image/'mcu-boot.sh', 'start'))
    assert calls == [], ('boot admission touched acvs', calls)
    assert acvs.start() == 0

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
    (image/'mcu-boot.sh').write_text(script_text('/src/mcu-boot.sh', [SHELL_ERROR, CE_EXEC_FAILURE, *USB_PATHS]))
    (image/'mcu-boot-install.sh').write_text(script_text('/src/mcu-boot-install.sh'))
    set_modes(image)
    for n in ['mcu-boot.sh', 'mcu-boot-install.sh']: (image/n).chmod(0o700)
    (image/'payload.sha256').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(image.iterdir())))
    source = P('/tmp/dev-package'); write_package(source, dev, session_source(), plugins=True)
    for f in [source, *source.iterdir()]: os.chown(f, 1000, 1000); f.chmod(0o755 if f.is_dir() else 0o644)
    wrong = P('/tmp/wrong-package'); write_package(wrong, image, session_source())
    p = call(image/'mcu-boot-install.sh', 'override', str(wrong), expected=1); assert 'not built for /data/mpclearn/dev' in p.stderr
    shutil.copytree(source, '/tmp/tampered'); (P('/tmp/tampered')/'mirror-input').write_bytes(b'tampered')
    call(image/'mcu-boot-install.sh', 'override', '/tmp/tampered', expected=1)
    assert not dev.exists()
    call(image/'mcu-boot-install.sh', 'override', str(source))
    assert all(os.stat(dev/n).st_uid == 0 and (dev/n).stat().st_mode & 0o777 == (0o700 if n in EXECUTABLES else 0o600) for n in package_names(dev)+['session-package.sha256'])
    assert dev.stat().st_mode & 0o777 == 0o700 and (dev/'for-image').read_text() == sha(image/'payload.sha256')+'\n', (
        oct(dev.stat().st_mode & 0o777), (dev/'for-image').read_text(), sha(image/'payload.sha256'))
    # The image ignores an override for another image, says so, and removes nothing.
    good = (dev/'for-image').read_text()
    for bad in ('0'*64+'\n', None):
        if bad is None: (dev/'for-image').unlink()
        else: (dev/'for-image').write_text(bad)
        p = call(image/'mcu-boot.sh', 'stop', expected=1)
        assert 'ignored: installed for a different image; using the image runtime' in p.stderr and 'No owned session' in p.stderr, p.stderr
        assert (dev/'mcu').exists() and not launches()
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
    # With no session, acvs launches stock MPC through the real stock launcher.
    assert acvs.start() == 0; until(lambda: launches(), 'stock launch without a session')
    assert stock_since(0) and 'stock MPC: no admitted session' in decision.read_text(), (launches(), decision.read_text() if decision.exists() else None)
    if MODE == 'host': assert ' stack=1048576 ' in last_launch(), last_launch()
    print(f'PASS [{MODE}] acvs without a session: the shipped drop-in launches stock MPC through the pinned stock launcher (ulimit, tunables), decision recorded')
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
        print(f'REPRO [{MODE}] first bridge fails before delayed MPC route; later explicit bridge succeeds but follower stays failed and native collection remains empty')
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
        run('stop'); until(follower_gone, 'delayed-start follower stops')
        (session/'route-fail').touch(); armed_before = count('armed'); run('start')
        until(lambda: (session/'generation.failed').exists(), 'bounded route admission fails without restarting bridge')
        retained_bridge = (session/'bridge.pid').read_text()
        (session/'route-fail').unlink(); time.sleep(1)
        assert count('armed') == armed_before and (session/'bridge.pid').read_text() == retained_bridge, 'late readiness cannot automatically repeat failed arming'
        run('bridge-stop'); run('bridge-start')
        assert count('armed') == armed_before+1 and not (session/'generation.failed').exists()
        run('stop'); until(follower_gone, 'explicit route recovery stops')
        # A real bridge failure remains terminal until this explicit recovery.
        (session/'bridge-immediate-fail').touch(); run('start', 1)
        assert native_values()[53] == 1 and (session/'generation.failed').exists()
        failed_bridge = (session/'bridge.pid').read_text()
        time.sleep(1)
        assert (session/'bridge.pid').read_text() == failed_bridge and (session/'bridge.exit').exists(), 'no automatic bridge restart'
        (session/'project-ready').touch(); run('bridge-start')
        until(lambda: not (session/'generation.failed').exists(), 'explicit recovery rejoins resident follower')
        assert (session/'generation.ready').read_text() == 'ready\n' and native_values()[53] == 1
        sequence = native_request(0, kind=2)
        until(lambda: native_values()[28] == sequence, 'recovered follower watcher services Refresh')
        assert (home/'controller-preferences').read_bytes() == saved
        run('stop'); until(follower_gone, 'recovered follower stops')
        assert not (runtime/'controller-recovery').exists()
        print(f'PASS [{MODE}] absent saved Mini startup: private bridge before delayed MPC route, bounded arming, collection/inventory/watch ready, true first bridge failure has no automatic retry, explicit late recovery rejoins the follower and services Refresh without preference changes')
        sys.exit(0)
    # Runtime admission refuses symlinks, unknown entries and unowned state.
    state.symlink_to('/tmp', target_is_directory=True); run('start', 1); state.unlink()
    state.mkdir(mode=0o700); (state/'unexpected').touch(); run('start', 1); (state/'unexpected').unlink()
    (state/'command.state').symlink_to('/tmp/missing-state'); run('start', 1); (state/'command.state').unlink()
    (state/'command.state').write_text('unowned'); os.chown(state/'command.state', 1000, 1000); run('start', 1); (state/'command.state').unlink()
    assert ce_launches() == 0 and not (session/'session.id').exists()
    # Stop wins even before the follower acts: the revoked session gets no
    # bridge and acvs comes back stock.
    (session/'pause-follower').touch(); started = subprocess.Popen([str(dev/'mcu'), 'start'], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    until(lambda: ce_launches() == 1 and (session/'mpc.pid').exists(), 'first CE instance under a paused follower')
    n = len(launches()); run('stop'); (session/'pause-follower').unlink(); out, err = started.communicate(timeout=60)
    assert started.returncode == 1, (out, err)
    until(follower_gone, 'revoked follower exits'); until(lambda: stock_since(n), 'stop gives stock')
    assert not (session/'bridge.pid').exists() and not (session/'mpc.pid').exists() and ce_launches() == 1
    # Manual start orders: acvs stop (its close runs), plugin prepare with MPC
    # stopped, admission, acvs start. It archives the revoked session first.
    events_before = len(EVENTS.read_text().splitlines())
    run('start')
    order = [l for l in EVENTS.read_text().splitlines()[events_before:] if l.startswith(('systemctl stop acvs', 'systemctl start acvs', 'plugin-setup', 'acvs started', 'acvs exited'))]
    assert order[:2] == ['systemctl stop acvs', order[1]] and order[1].startswith('acvs exited') and order[2] == 'plugin-setup prepare' and order[3] == 'systemctl start acvs' and order[4].startswith('acvs started'), order
    assert 'refused: MPC running' not in EVENTS.read_text()
    assert ready_instance() and entries()
    assert last_launch().startswith('CE ') and ' cardinal=/run/mpclearn/plugins/Cardinal.vst/resources ' in last_launch() and ' preload=/data/mpclearn/dev/command-observer.so ' in last_launch()
    if MODE == 'host': assert ' stack=1048576 tunables=glibc.malloc.hugetlb=2 arena=1 ' in last_launch(), last_launch()
    assert app_pid() == acvs.main_pid() and 'CE MPC from /data/mpclearn/dev, plugins cardinal' in decision.read_text()
    print(f'PASS [{MODE}] manual start: stop with close, plugin prepare with MPC stopped, admission, start; acvs MainPID is the exact CE instance with the stock final-branch environment and plugins; stop before the follower acts leaves no bridge and stock MPC')
    # Boot admission archives the session, never touches acvs, and acvs then
    # launches CE with the verified override.
    boot(); until(ready_instance, 'boot CE instance ready'); assert count('armed') == 0
    bridge_log = (session/'bridge.log').read_text()
    assert '--motors=on' in bridge_log and '--preferences=/data/mpclearn/surface-preferences' in bridge_log
    assert 'ARG --controller-preferences=/data/mpclearn/controller-preferences' in bridge_log and 'SAVED profile=xtouch endpoint-type=kernel client=X-Touch port=X-TOUCH_INT' in bridge_log
    assert (session/'session.location').read_text() == f'{dev}\n'
    p = run('status', 2, where=image); assert 'belongs to /data/mpclearn/dev' in p.stderr
    p = run('status', 3); assert 'last_launch: CE MPC from /data/mpclearn/dev' in p.stdout and 'follower pid=' in p.stdout, p.stdout
    print(f'PASS [{MODE}] boot admission: no acvs call; acvs then starts one CE instance as MainPID with a chooser bridge')
    (home/'surface-preferences').write_text('MCU-SURFACE1 motors=0\n'); (home/'surface-preferences').chmod(0o600)
    # Reproduce the native miss: bridge deliberately absent when chooser becomes
    # ready, then explicit bridge-start must repair arming under the same follower.
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
    lifetime = native_lifetime(); os.kill(bridge_pid(), signal.SIGKILL)
    until(lambda: (session/'bridge.exit').exists(), 'killed bridge child receipt')
    assert native_lifetime() == lifetime + 1, 'killed bridge cannot retain same-MPC admission'
    run('bridge-start'); assert native_lifetime() == lifetime + 2, 'restart after killed bridge fences its publication'
    assert state_inodes == [(state/n).stat().st_ino for n in ('command.state', 'volume.state')]
    before = ce_launches(); run('start'); assert ce_launches() == before
    # A latched source error must not turn the idempotent `mcu start` into a
    # silent exit 1: the status is reported, the outcome line is still printed.
    (session/'source-failed').touch()
    retained = run('start')
    assert 'Session source status exit 1' in retained.stdout and 'Existing owned session retained' in retained.stdout, retained.stdout
    (session/'source-failed').unlink(); assert ce_launches() == before and count('armed') == 1
    # The injected callback is represented by one atomic state write plus the
    # same shared futex wake. The real follower watcher claims it outside the UI,
    # preserves the live bridge on errors, and uses its normal drain/restart.
    native_preferences = state/'native-preferences.state'; assert native_preferences.stat().st_size == 16384
    run('status'); bridge_before = bridge_pid()
    sequence = native_request(99); until(lambda: native_values()[28] == sequence, 'invalid native request completes')
    assert native_values()[38] == 6 and bridge_pid() == bridge_before and not (home/'controller-preferences').exists()
    (session/'generic-unavailable').touch(); sequence = native_request(3); until(lambda: native_values()[28] == sequence, 'unavailable Generic completes')
    assert native_values()[38] == 4 and bridge_pid() == bridge_before and not (home/'controller-preferences').exists(); (session/'generic-unavailable').unlink()
    (session/'generic-ambiguous').touch(); sequence = native_request(3); until(lambda: native_values()[28] == sequence, 'ambiguous Generic completes')
    assert native_values()[38] == 5 and bridge_pid() == bridge_before and not (home/'controller-preferences').exists(); (session/'generic-ambiguous').unlink()
    restore_trace = session/'restore-fsync.trace'
    if restore_trace.exists(): restore_trace.unlink()
    (session/'bridge-start-fail').touch(); sequence = native_request(3)
    until(lambda: native_values()[28] == sequence and native_values()[38] == 8, 'failed Generic replacement rolls back missing default')
    restored_default_pid = bridge_pid(); values = native_values()
    assert restored_default_pid != bridge_before and not (home/'controller-preferences').exists(), (restored_default_pid, bridge_before, (home/'controller-preferences').exists(), (session/'bridge.log').read_text())
    os.kill(restored_default_pid, 0)
    assert values[39] == values[40] == 1 and values[43] == values[44] == 1 and values[45] == values[46] == 0
    assert 'SAVED profile=xtouch endpoint-type=kernel client=X-Touch port=X-TOUCH_INT' in (session/'bridge.log').read_text()
    assert restore_trace.read_text().splitlines() == ['file', 'directory', 'directory']
    assert not (runtime/'controller-preferences.rollback').exists() and not (home/'.controller-preferences.restore').exists()
    sequence = native_request(3); until(lambda: native_values()[28] == sequence and native_values()[38] == 3, 'unique Generic applies')
    generic_pid = bridge_pid(); assert generic_pid != restored_default_pid
    assert 'SAVED profile=generic endpoint-type=kernel client=Generic Device port=Generic Port' in (session/'bridge.log').read_text()
    generic_preference = (home/'controller-preferences').read_bytes()
    inventory_generation = native_values()[48]
    restore_trace.unlink(); (session/'bridge-immediate-fail').touch(); (session/'bridge-pre-identity-fail').touch()
    sequence = native_request(4, generation=inventory_generation, endpoint_index=1, endpoint_type=2, hui_policy=2)
    until(lambda: native_values()[28] == sequence and native_values()[38] == 8, 'pre-identity HUI failure restores exact Generic bytes')
    restored_generic_pid = bridge_pid(); values = native_values()
    assert restored_generic_pid != generic_pid and (home/'controller-preferences').read_bytes() == generic_preference
    os.kill(restored_generic_pid, 0)
    assert values[39] == values[40] == 3 and values[43] == values[44] == 1 and values[45] == values[46] == 0
    assert 'SAVED profile=generic endpoint-type=kernel client=Generic Device port=Generic Port' in (session/'bridge.log').read_text()
    assert 'BRIDGE_LAUNCH_IDENTITY_FAILED' in (session/'follower.log').read_text()
    assert restore_trace.read_text().splitlines() == ['file', 'directory', 'file', 'directory']
    assert not (runtime/'controller-preferences.rollback').exists() and not (home/'.controller-preferences.restore').exists()
    inventory_generation = native_values()[48]
    sequence = native_request(4, generation=inventory_generation-1, endpoint_index=1, endpoint_type=2, hui_policy=2); until(lambda: native_values()[28] == sequence, 'stale HUI request completes')
    assert native_values()[38] == 10 and bridge_pid() == restored_generic_pid
    (session/'inventory-open-fail').touch()
    for attempt in range(3):
        sequence = native_request(4, kind=2)
        until(lambda: native_values()[28] == sequence and native_values()[38] == 4, 'failed inventory open completes UNAVAILABLE')
        assert bridge_pid() == restored_generic_pid and (home/'controller-preferences').read_bytes() == generic_preference
        os.kill(restored_generic_pid, 0)
    (session/'inventory-open-fail').unlink()
    print(f'PASS [{MODE}] failed Refresh: deterministic UNAVAILABLE completion retains exact collection bytes and the same live bridge (ALSA open failure/helper substituted)')
    sequence = native_request(4, kind=2); until(lambda: native_values()[28] == sequence and native_values()[38] == 9, 'HUI endpoint refresh completes')
    assert bridge_pid() == restored_generic_pid and native_values()[48] != inventory_generation
    sequence = native_request(4, generation=native_values()[48], endpoint_index=1, endpoint_type=2, hui_policy=2); until(lambda: native_values()[28] == sequence and native_values()[40] == 4, 'exact user Motorized HUI request applies')
    hui_pid = bridge_pid(); assert hui_pid != generic_pid
    assert native_values()[44] == 2 and native_values()[46] == 2 and 'SAVED profile=hui endpoint-type=user client=User HUI port=DAW hui-mode=motorized' in (session/'bridge.log').read_text()
    sequence = native_request(2); until(lambda: native_values()[28] == sequence and native_values()[40] == 2, 'Mini request applies')
    mini_pid = bridge_pid(); assert mini_pid != generic_pid and 'SAVED profile=xtouch-mini' in (session/'bridge.log').read_text()
    sequence = native_request(1); until(lambda: native_values()[28] == sequence and native_values()[40] == 1, 'X-Touch return applies')
    assert bridge_pid() != mini_pid and 'SAVED profile=xtouch endpoint-type=kernel client=X-Touch port=X-TOUCH_INT' in (session/'bridge.log').read_text()
    print(f'PASS [{MODE}] native controller requests: shared futex wake, follower claim, invalid/unavailable/ambiguous/stale bridge retention, post-identity and immediate pre-identity replacement failure rollback to exact saved bytes or missing X-Touch default with file/directory fsync and a working prior bridge, endpoint refresh without restart, exact user Motorized HUI persistence, active Mini restart and return to X-Touch (UI click, native MPC and ALSA enumeration substituted)')
    print(f'PASS [{MODE}] explicit bridge-start: actual missed chooser arming recovery, route-failure refusal, exact failure cleanup and no duplicate signal, a retained unhealthy source, and admission of the fixed native Preferences state file (native adapter/protocol/UI substituted)')

    # Every exit returns to an acvs-owned CE instance: New Project (intent), a
    # mode switch (deliberate quit without intent), a crash (exit code, signal).
    # MPC changes its own settings; nothing restores them.
    settings.write_text('MPC switched to Controller Mode\n')
    for label, intent, code in (('New Project', True, 0), ('mode switch', False, 0), ('crash exit', False, 17), ('crash signal', False, 'kill')):
        previous, previous_bridge, before, names = app_pid(), bridge_pid(), ce_launches(), set(entries())
        (session/'project-ready').touch(); until(lambda: count('armed') >= 1 and not (session/'manual-adapter-unarmed.pid').exists(), f'{label}: armed before exit')
        armed = count('armed'); depart(intent, code)
        until(lambda: ready_instance(previous) and ce_launches() == before+1, f'{label}: new CE instance and bridge')
        assert bridge_pid() != previous_bridge and count('armed') == armed and (session/'manual-adapter-unarmed.pid').exists()
        bridge_log = (session/'bridge.log').read_text()
        assert '--held-mask=3' in bridge_log and '--motors=off' in bridge_log
        entry = history/(set(entries())-names).pop()
        assert entry.name.split('-', 2)[2] == f'{previous}-' + entry.name.split('-')[-1]
        assert {'mpc.pid', 'mpc.exit', 'mpc.log', 'bridge.log', 'command-status.json', 'session-status.txt', 'mirror-read.json'} <= {p.name for p in entry.iterdir()}
        expected_exit = {0: 'service_result=success exit_code=exited exit_status=0', 17: 'service_result=exit-code exit_code=exited exit_status=17', 'kill': 'service_result=signal exit_code=killed exit_status=KILL'}[code]
        assert (entry/'mpc.exit').read_text() == expected_exit+'\n', (entry/'mpc.exit').read_text()
        json.loads((entry/'command-status.json').read_text()); json.loads((entry/'mirror-read.json').read_text())
        assert not (entry/'command-status.json.truncated').exists()
        assert (entry/'mpc.log').stat().st_size == 1048576 and (entry/'mpc.log').read_bytes().endswith(b' MPC diagnostic tail\n')
        assert settings.read_text() == 'MPC switched to Controller Mode\n' and not acvs.failed
    no_raw_state()
    print(f'PASS [{MODE}] acvs restarts every exit: New Project, mode-switch quit without intent, crash by exit code and by signal each give a new acvs-owned CE instance and bridge with held faders handed over; close archives each departed instance with its service result and a 1 MiB journal tail; MPC.settings left as MPC changed it')
    # Controller Mode: MPC writes remoteScreen and quits. The next launch runs
    # stock after the SD plugin view is released (also when the view is already
    # dead), with no bridge, recording the internal USB sound cards. MPC removes
    # the marker and quits to return; the launch switches the USB back to
    # internal before MPC starts, waits for those devices and the card, and
    # admits the SD plugins again in the same launch.
    remote = settings.parent/'remoteScreen'
    def enter_controller_mode(dead=False):
        assert (runtime/'plugin-mount.json').exists() and ' cardinal=/run/mpclearn/plugins/Cardinal.vst/resources ' in last_launch(), last_launch()
        if dead: P('/tmp/plugins-ready').unlink()  # the view no longer reads; its receipt remains
        n = len(launches()); events = len(EVENTS.read_text().splitlines())
        remote.write_text(''); depart(False, 0)
        until(lambda: len(launches()) > n and acvs.main_pid() is not None and f'pid={acvs.main_pid()} ' in last_launch(), 'Controller Mode instance')
        assert stock_since(n) and decision.read_text() == 'stock MPC: Controller Mode; SD plugins released\n', decision.read_text()
        later = EVENTS.read_text().splitlines()[events:]
        # Released inside that launch with no MPC alive: the stub refuses and logs otherwise.
        assert 'plugin-setup release' in later and not any('refused: MPC running' in l for l in later), later
        assert not (runtime/'plugin-mount.json').exists() and (runtime/'plugins.released').read_text() == 'released\n'
        assert (runtime/'usb-internal.cards').read_text() == 'MPC Live II Audio\nMPC Live II\nX-Touch\n' and not usb_events(events)
        time.sleep(1); assert not (session/'mpc.pid').exists() and not (session/'bridge.pid').exists()
        p = run('status', 5); assert 'sd_plugins: released for Controller Mode (released)' in p.stdout, p.stdout
        controller_mode_usb()
    def return_to_standalone():
        n = len(launches()); events = len(EVENTS.read_text().splitlines()); t0 = time.monotonic()
        remote.unlink(); (session/'exit-request').write_text('0')
        until(lambda: ready_instance() and len(launches()) > n, 'Standalone instance after the return', 40)
        return events, time.monotonic()-t0
    # Return with the USB external: switched back before MPC starts, the
    # devices awaited, the SD plugins admitted again in the same launch.
    enter_controller_mode()
    events, took = return_to_standalone()
    assert usb_events(events) == ['usbmux internal'] and P('/tmp/usb-mux/state').read_text() == 'internal\n', usb_events(events)
    assert last_launch().startswith('CE ') and ' cardinal=/run/mpclearn/plugins/Cardinal.vst/resources ' in last_launch(), last_launch()
    note = decision.read_text()
    assert note.startswith('USB switched back to internal, devices back after ') and note.endswith('; CE MPC from /data/mpclearn/dev, plugins cardinal\n'), note
    later = EVENTS.read_text().splitlines()[events:]
    assert later.index('usbmux internal') < later.index('plugin-setup prepare') and not (runtime/'plugins.released').exists()
    print(f'PASS [{MODE}] Controller Mode return with USB external: Akai switch called before MPC (no MPC alive), devices and card awaited, SD plugins admitted again in the same launch (cardinal) in {took:.1f} s; entry ran stock after releasing the SD view and recorded the internal USB sound cards')
    # Already internal (New Project, restarts): no switch call and no USB note.
    previous = app_pid(); events = len(EVENTS.read_text().splitlines()); depart(True); until(lambda: ready_instance(previous), 'New Project with USB internal')
    assert not usb_events(events) and decision.read_text() == 'CE MPC from /data/mpclearn/dev, plugins cardinal\n', decision.read_text()
    print(f'PASS [{MODE}] USB already internal: no switch call, no wait, decision unchanged')
    # The switch fails; the launch goes on plain and says why. MPC then
    # switches back itself, and the next launch admits the card again.
    enter_controller_mode(dead=True); P('/tmp/usbmux-fail').touch()
    events, took = return_to_standalone(); P('/tmp/usbmux-fail').unlink()
    assert ' cardinal=- ' in last_launch() and decision.read_text() == 'USB switch-back failed (status 1); CE MPC from /data/mpclearn/dev, plugins plain: expected one SD-card bundle, found 0\n', decision.read_text()
    assert (runtime/'plugins.released').read_text() == 'due\n' and P('/tmp/usb-mux/state').read_text() == 'external\n'
    P('/tmp/usb-mux/state').write_text('internal\n'); P('/tmp/asound-cards').write_text(INTERNAL_CARDS); card_present(True)
    previous = app_pid(); events = len(EVENTS.read_text().splitlines()); depart(True); until(lambda: ready_instance(previous), 'next launch admits the card')
    assert not usb_events(events) and ' cardinal=/run/mpclearn/plugins/Cardinal.vst/resources ' in last_launch() and not (runtime/'plugins.released').exists()
    # The switch hangs: killed after 5 s, the launch goes on.
    enter_controller_mode(); P('/tmp/usbmux-hang').touch()
    events, took = return_to_standalone(); P('/tmp/usbmux-hang').unlink()
    assert 5 <= took < 20 and decision.read_text() == 'USB switch-back timed out after 5 s, SATA reset released; CE MPC from /data/mpclearn/dev, plugins plain: expected one SD-card bundle, found 0\n', (took, decision.read_text())
    assert P('/tmp/gpio237/value').read_text().strip() == '0', 'the killed tool left the SATA reset asserted'
    # Where the devicetree says the reset is asserted by hardware, the tool and
    # CE leave the line alone.
    P('/tmp/sata-reset-asserted').touch(); P('/tmp/gpio237/value').write_text('1\n')
    P('/tmp/usb-mux/state').write_text('internal\n'); P('/tmp/asound-cards').write_text(INTERNAL_CARDS); card_present(True)
    previous = app_pid(); depart(True); until(lambda: ready_instance(previous), 'card admitted after the hang')
    enter_controller_mode(); P('/tmp/usbmux-hang').touch()
    events, took = return_to_standalone(); P('/tmp/usbmux-hang').unlink()
    assert decision.read_text().startswith('USB switch-back timed out after 5 s; CE MPC') and P('/tmp/gpio237/value').read_text().strip() == '1', decision.read_text()
    P('/tmp/sata-reset-asserted').unlink(); P('/tmp/gpio237/value').write_text('0\n')
    P('/tmp/usb-mux/state').write_text('internal\n'); P('/tmp/asound-cards').write_text(INTERNAL_CARDS); card_present(True)
    previous = app_pid(); depart(True); until(lambda: ready_instance(previous), 'card admitted after the second hang')
    # Switched, but the devices never come back: the wait ends after 15 s.
    enter_controller_mode(); P('/tmp/usb-never-back').touch()
    events, took = return_to_standalone(); P('/tmp/usb-never-back').unlink()
    assert 15 <= took < 30 and decision.read_text().startswith('USB switched back to internal, '), (took, decision.read_text())
    assert ' not back after 15 s; CE MPC from /data/mpclearn/dev, plugins plain: expected one SD-card bundle, found 0' in decision.read_text(), decision.read_text()
    P('/tmp/asound-cards').write_text(INTERNAL_CARDS); card_present(True)
    previous = app_pid(); depart(True); until(lambda: ready_instance(previous), 'card admitted after the timeout')
    assert ' cardinal=/run/mpclearn/plugins/Cardinal.vst/resources ' in last_launch()
    print(f'PASS [{MODE}] Controller Mode return: switch failure, a hanging switch (killed after 5 s; its SATA reset released unless the devicetree asserts it) and devices that never return (15 s bound) each launch CE plain with the reason recorded; the next launch admits the card again')
    # Plain without Controller Mode: the plugin check's own reason is recorded.
    P('/tmp/plugins-ready').unlink(); previous = app_pid(); depart(True); until(lambda: ready_instance(previous), 'plain with a reason')
    assert decision.read_text() == 'CE MPC from /data/mpclearn/dev, plugins plain: admitted SD mount changed\n', decision.read_text()
    P('/tmp/plugins-ready').touch(); previous = app_pid(); depart(True); until(lambda: ready_instance(previous), 'cardinal again')
    assert decision.read_text() == 'CE MPC from /data/mpclearn/dev, plugins cardinal\n'
    print(f'PASS [{MODE}] plugins plain records plugin-setup\'s one-line reason in the launch decision')
    # Installer path: systemctl stop then start, and restart.
    previous = app_pid(); names = set(entries()); systemctl_call('stop', 'acvs')
    assert not acvs.active() and not (session/'mpc.pid').exists() and not (session/'bridge.pid').exists() and len(set(entries())-names) == 1
    entry = history/(set(entries())-names).pop(); assert (entry/'mpc.exit').read_text() == 'service_result=success exit_code=exited exit_status=0\n'
    assert not any(state.iterdir())
    systemctl_call('start', 'acvs'); until(lambda: ready_instance(previous), 'installer start gives CE')
    previous = app_pid(); systemctl_call('restart', 'acvs'); until(lambda: ready_instance(previous), 'installer restart gives CE')
    print(f'PASS [{MODE}] installer systemctl stop (no MPC, no bridge, instance archived, state empty), start and restart each give one acvs-owned CE instance')
    # A foreign add-on preload: ignored and reported on the CE path, passes
    # through on the stock path. The launcher's inherited environment survives.
    acvs.extra_env = {'LD_PRELOAD': '/usr/lib/mpclearn-test-addin.so', 'ACVS_MARKER': 'inherited'}
    previous = app_pid(); systemctl_call('restart', 'acvs'); until(lambda: ready_instance(previous), 'CE with an add-on preload')
    assert last_launch().startswith('CE ') and ' preload=/data/mpclearn/dev/command-observer.so ' in last_launch() and ' marker=inherited' in last_launch()
    assert 'add-on LD_PRELOAD ignored: /usr/lib/mpclearn-test-addin.so' in decision.read_text()
    p = run('status', 3); assert 'add-on LD_PRELOAD ignored' in p.stdout, p.stdout
    print(f'PASS [{MODE}] add-on acvs LD_PRELOAD: CE path ignores and reports it (mcu status); the inherited environment reaches MPC')

    # Every refusal or error before the CE exec runs the stock launcher with the
    # inherited environment: one cause at a time, with acvs stopped around it,
    # then CE again once the cause is gone.
    def refusal(label, reason, setup, undo, check=lambda: True, final_branch=True, restore=None):
        previous = app_pid()
        systemctl_call('stop', 'acvs'); setup(); n = len(launches())
        systemctl_call('start', 'acvs'); until(lambda: len(launches()) > n, f'{label}: a launch', 60)
        assert stock_since(n, final_branch) and ' preload=/usr/lib/mpclearn-test-addin.so ' in last_launch() and ' marker=inherited' in last_launch(), (label, launches()[n:])
        assert reason in decision.read_text() and 'add-on LD_PRELOAD passes through: /usr/lib/mpclearn-test-addin.so' in decision.read_text(), (label, decision.read_text())
        assert check(), label
        assert (session/'mpc.pid').exists() == (label == 'CE exec failure'), label
        systemctl_call('stop', 'acvs'); undo()
        if restore: restore()
        else: systemctl_call('start', 'acvs')
        until(lambda: ready_instance(previous), f'{label}: CE again')
    marker = home/'disabled'
    refusal('disabled marker', 'stock MPC: CE disabled', marker.touch, marker.unlink)
    boot_id = (session/'session.boot-id').read_text()
    refusal('previous-boot session', 'stock MPC: no session from this boot', lambda: (session/'session.boot-id').write_text('previous-boot\n'), lambda: (session/'session.boot-id').write_text(boot_id))
    tampered = (dev/'mirror-read').read_bytes()
    refusal('package fails verification', 'stock MPC: development override does not verify', lambda: (dev/'mirror-read').write_bytes(tampered+b'#\n'), lambda: (dev/'mirror-read').write_bytes(tampered))
    refusal('launch-prepare failure', 'stock MPC: launch-prepare refused (status 1)', P('/tmp/prepare-fail').touch, P('/tmp/prepare-fail').unlink)
    t0 = time.monotonic()
    refusal('launch-prepare timeout', 'stock MPC: launch-prepare refused (status 137)', P('/tmp/prepare-hang').touch, P('/tmp/prepare-hang').unlink)
    assert time.monotonic()-t0 >= 45
    refusal('DFU device', 'stock MPC: DFU device in update mode', P('/tmp/dfu-present').touch, P('/tmp/dfu-present').unlink, lambda: P('/tmp/dfu-updates').read_text() == 'dfu-update\n')
    refusal('systemd-inhibit', 'stock MPC: systemd-inhibit present', lambda: script(inhibit_bin/'systemd-inhibit', 'echo "inhibit $*" >>/tmp/events;shift;exec "$@"\n'),
            (inhibit_bin/'systemd-inhibit').unlink, lambda: 'inhibit --what=handle-power-key /usr/bin/MPC' in EVENTS.read_text(), final_branch=False)
    refusal('forced shell error', 'stock MPC: launch decision failed (status 2)', P('/tmp/force-shell-error').touch, P('/tmp/force-shell-error').unlink)
    refusal('CE exec failure', 'stock MPC: CE exec failed', P('/tmp/ce-exec-fail').touch, P('/tmp/ce-exec-fail').unlink)
    # Revocation and a missing session end the follower: only a new session
    # brings CE back.
    refusal('revoked session', 'stock MPC: session revoked', lambda: shutil.copy(session/'session.id', session/'session.revoked'), lambda: None, restore=lambda: run('start'))
    refusal('missing session', 'stock MPC: no admitted session', (session/'session.id').unlink, lambda: None, restore=lambda: run('start'))
    acvs.extra_env = {}
    print(f'PASS [{MODE}] launch failure injection: disabled marker, previous-boot, revoked and missing session, override failing verification, launch-prepare failure and 45 s timeout, DFU device (stock updater ran), systemd-inhibit present (stock inhibit branch), a forced shell error and a failed CE exec each exec the stock launcher with the inherited environment and add-on preload; CE returns when the cause is gone')

    # Leftover shared state makes the next launch stock until it is empty.
    systemctl_call('stop', 'acvs'); (state/'command.state').write_text('leftover'); n = len(launches())
    systemctl_call('start', 'acvs'); until(lambda: len(launches()) > n, 'leftover state: a launch')
    assert stock_since(n) and 'launch-prepare refused (status 1)' in decision.read_text() and 'is not empty' in (JOURNAL/f'invocation-{acvs.invocation}.log').read_text()
    systemctl_call('stop', 'acvs'); (state/'command.state').unlink()
    systemctl_call('start', 'acvs'); until(ready_instance, 'CE after leftover removed')
    print(f'PASS [{MODE}] leftover shared state before a launch: launch-prepare refuses, the instance runs stock, CE returns once state is empty')

    # Crash guard: two consecutive CE instances that end abnormally before the
    # source is ready leave the rest of the boot to stock MPC.
    (session/'source-initializing').touch(); previous = app_pid()
    systemctl_call('restart', 'acvs')
    def live_instance(previous):
        return (session/'mpc.pid').exists() and app_pid() != previous and acvs.main_pid() == app_pid() and os.readlink(f'/proc/{app_pid()}/exe') == '/usr/bin/MPC'
    until(lambda: live_instance(previous), 'never-ready CE instance')
    previous = app_pid(); depart(False, 9); until(lambda: live_instance(previous), 'first early crash relaunches CE')
    assert (session/'crash-count').read_text() == '1\n' and not (session/'crash-guard').exists()
    n = len(launches()); depart(False, 9)
    until(lambda: len(launches()) > n, 'launch after the second early crash')
    assert stock_since(n) and (session/'crash-guard').exists() and 'stock MPC: crash guard' in decision.read_text()
    p = run('status', 5); assert 'crash_guard:' in p.stdout, p.stdout
    (session/'source-initializing').unlink()
    run('start'); assert ready_instance() and not (session/'crash-guard').exists()
    print(f'PASS [{MODE}] crash guard: two early abnormal CE exits trip it, the next launch is stock with a recorded reason, mcu status reports it, a new session clears it')

    # mcu stop revokes, closes the source while MPC lives, releases the lock and
    # restarts acvs, which comes up stock. MPC.settings stays as MPC left it.
    settings.write_text('changed during the session\n'); n = len(launches()); previous = app_pid(); names = set(entries())
    calls = acvs_calls_during(lambda: run('stop'))
    until(lambda: stock_since(n), 'mcu stop gives stock')
    assert ['restart', 'acvs'] in calls and (session/'closed').exists() and settings.read_text() == 'changed during the session\n'
    assert len(set(entries())-names) == 1 and not (session/'mpc.pid').exists() and acvs.active()
    until(follower_gone, 'stopped follower exits')
    p = subprocess.run([str(image/'mcu'), 'status'], env=env, capture_output=True, text=True, timeout=60)
    assert 'motor_follow=off' in p.stdout and 'belongs to' not in p.stderr, (p.stdout, p.stderr)
    print(f'PASS [{MODE}] mcu stop: graceful source close, lock released before acvs restart (no deadlock with close), stock MPC, MPC.settings left as changed')
    # A manual start whose admission fails still brings acvs back, stock.
    (state/'command.state').write_text('unexplained'); n = len(launches())
    p = run('start', 1); assert 'Unexplained state' in p.stderr and 'Session not admitted; acvs runs stock MPC.' in p.stderr, p.stderr
    until(lambda: stock_since(n) and acvs.active(), 'failed admission leaves stock MPC running')
    assert not (session/'session.id').exists() and not (session/'follower.pid').exists()
    (state/'command.state').unlink()
    print(f'PASS [{MODE}] manual start with a failed admission: acvs starts again and runs stock MPC')
    # Unclean stop: a dead bridge leaves text evidence in the session folder.
    run('start'); os.kill(bridge_pid(), signal.SIGKILL); until(lambda: (session/'bridge.exit').exists(), 'killed bridge receipt')
    run('stop', 1)
    assert (session/'stop-incomplete-status.txt').exists() and json.loads((session/'incomplete-command-status.json').read_text())['admission']['site'] == 77
    assert (session/'incomplete-mirror-read.json').exists() and (session/'incomplete-bridge.log').exists()
    names = set(entries()); run('start'); new = history/sorted(set(entries())-names)[-1]
    assert {'incomplete-command-status.json', 'stop-incomplete-status.txt', 'incomplete-mirror-read.json', 'incomplete-bridge.log', 'session.revoked', 'session.location'} <= {p.name for p in new.iterdir()}, sorted(p.name for p in new.iterdir())
    assert not (session/'incomplete-command-status.json').exists()
    # Durable stop during an exit: whether or not the next instance had started,
    # nothing launches CE after the stop.
    depart(True); p = subprocess.run([str(dev/'mcu'), 'stop'], env=env, capture_output=True, text=True, timeout=60); assert p.returncode in (0, 1), p
    until(follower_gone, 'stop winner follower exits')
    def stock_main():
        main = acvs.main_pid(); return main is not None and launches() and last_launch().startswith(f'stock pid={main} ')
    until(stock_main, 'stop during an exit: acvs settles on a stock instance'); n = len(launches()); time.sleep(1)
    assert len(launches()) == n and stock_main() and (session/'session.revoked').exists() and not (session/'mpc.pid').exists(), (launches()[-3:], sorted(x.name for x in session.iterdir()))
    print(f'PASS [{MODE}] unclean stop keeps text evidence that the next start archives; stop during an exit leaves stock MPC')
    # Power-off: tmpfs vanishes; the session folder on /data stays and the next
    # boot's admission archives it, including the bridge's last lines.
    run('start')
    acvs.system_stopping = True; acvs.stop(); acvs.system_stopping = False
    assert (session/'mpc.pid').exists() and (session/'mpc.exit').exists() and (session/'bridge.log').exists(), 'shutdown close keeps the instance for the next boot'
    with (session/'bridge.log').open('a') as log: log.write('INPUT STOP error=10 reason=source_failure pending_request=1\n')
    last_bridge = (session/'bridge.log').read_text(); last_id = (session/'session.id').read_text(); names = set(entries())
    boot(previous_boot=True)
    rebooted = history/sorted(set(entries())-names)[0]
    assert (rebooted/'bridge.log').read_text() == last_bridge and last_bridge.endswith('pending_request=1\n') and (rebooted/'session.id').read_text() == last_id
    assert (rebooted/'session.boot-id').read_text() == 'previous-boot\n' and (rebooted/'mpc.exit').exists() and not (rebooted/'command-status.json').exists()
    until(ready_instance, 'CE after power-off boot'); assert (state/'command.state').exists() and not (session/'command.state').exists()
    (session/'project-ready').touch(); until(lambda: not (session/'manual-adapter-unarmed.pid').exists(), 'reboot session arming'); run('stop')
    no_raw_state()
    assert len(entries()) <= 8 and du(history) <= 16777216
    print(f'PASS [{MODE}] shutdown close skips history; power-off evidence archived from /data at the next boot admission; no raw state on /data (boot id and native app substituted)')

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
    run('start'); previous = app_pid(); depart(True); until(lambda: ready_instance(previous), 'budget New Project')
    within()
    assert '000001-aaaaaaaa-old' not in entries() and '000002-aaaaaaaa-old' in entries(), entries()
    newest = history/entries()[-1]; assert newest.name.startswith('000004-'), entries()
    cut = (newest/'command-status.json').read_text()
    assert cut.startswith('{"format":"CMD32","error":0,"requests":[{"slot":0,"rejected":0}],"events":[') and '[mpclearn history: ' in cut
    assert cut.endswith('"admission":{"category":"none","raw_result":0,"site":77}}\n') and 'original_bytes=' in (newest/'command-status.json.truncated').read_text()
    assert len(cut) < 8192+2048+200 and (newest/'mpc.log').stat().st_size == 65536
    print(f'PASS [{MODE}] history budget: crash leftover removed, oldest pruned before the write, du <= budget, head-and-tail status summary keeps the closing admission object')
    # Zero-padded 000008/000009 are invalid octal: the sequence must stay decimal.
    for n in (8, 9, 10, 11, 12, 13, 14):
        (history/f'{n:06d}-aaaaaaaa-tiny').mkdir(mode=0o700)
    for _ in range(3):
        previous = app_pid(); depart(True)
        until(lambda: ready_instance(previous), 'handoff within budget')
        within(); assert len(entries()) <= 4, entries()
    seq = [int(e.split('-')[0]) for e in entries()]; assert seq == sorted(seq) and seq[-1] == 17, entries()
    # A close that cannot archive leaves the instance: the next launch is stock.
    shutil.rmtree(history); history.write_text('not a folder')
    previous = app_pid(); n = len(launches()); depart(True)
    until(lambda: len(launches()) > n, 'launch after a failed close')
    assert stock_since(n) and (session/'mpc.pid').exists() and int((session/'mpc.pid').read_text().split()[0]) == previous
    assert 'launch-prepare refused (status 2)' in decision.read_text() and 'history is not a root-owned 0700 folder' in (JOURNAL/f'invocation-{acvs.invocation}.log').read_text(), decision.read_text()
    history.unlink(); history.mkdir(mode=0o700)
    run_either('stop')  # the departed bridge reported its own exit; clean or not, the next close archives
    assert not (session/'mpc.pid').exists() and len(entries()) == 1 and (history/entries()[0]/'mpc.exit').read_text().startswith('service_result=success exit_code=exited exit_status=0')
    # An entry that cannot fit alone is dropped without pruning anything.
    small(history_budget=40000)
    run('start')
    for e in entries(): shutil.rmtree(history/e)
    (history/'000001-aaaaaaaa-keep').mkdir(mode=0o700)
    (history/'.new').mkdir(); (history/'.new'/'partial').write_bytes(b'p'*100)  # removed even when nothing is pruned
    previous = app_pid(); depart(True); until(lambda: ready_instance(previous), 'oversize New Project')
    assert 'exceeds the budget; not retained' in (JOURNAL/'close.log').read_text()
    assert entries() == ['000001-aaaaaaaa-keep'] and du(history) <= 40000
    run('stop')
    no_raw_state()
    print(f'PASS [{MODE}] history budget: entry count bound over handoffs, a failed close archive makes the next launch stock and keeps the instance for the next close, oversized entry dropped without pruning')
    small()
    # Explicit one-session selection remains available and exact names,
    # including spaces, survive New Project.
    call(dev/'mcu', 'start', '--profile=generic', '--endpoint-client=Virtual MCU', '--endpoint-port=Port Name', '--endpoint-type=any')
    selected = (session/'bridge.log').read_text()
    for arg in ('ARG --profile=generic', 'ARG --endpoint-client=Virtual MCU', 'ARG --endpoint-port=Port Name', 'ARG --endpoint-type=any'):
        assert arg in selected, (arg, selected)
    assert native_values()[39:41] == (1, 3), native_values()[39:41]  # saved X-Touch, active one-session Generic
    previous = app_pid(); depart(True)
    until(lambda: ready_instance(previous), 'generic selected New Project respawn')
    selected = (session/'bridge.log').read_text()
    for arg in ('ARG --profile=generic', 'ARG --endpoint-client=Virtual MCU', 'ARG --endpoint-port=Port Name', 'ARG --endpoint-type=any'):
        assert arg in selected, (arg, selected)
    assert native_values()[39:41] == (1, 3), native_values()[39:41]
    run('stop')
    no_raw_state()
    # The same session package writes one bounded preference and a no-option
    # start consumes it. Invalid replacement leaves the prior selection intact.
    call(dev/'mcu', 'configure', '--profile=generic', '--endpoint-client=Saved MCU', '--endpoint-port=Saved Port', '--endpoint-type=any')
    controller_preference = home/'controller-preferences'
    assert controller_preference.stat().st_size == 8992 and controller_preference.stat().st_mode & 0o777 == 0o600
    p = call(dev/'mcu', 'configure', '--profile=generic', expected=2)
    assert 'The selected protocol requires exact endpoint client and port names.' in p.stderr
    boot(); until(ready_instance, 'saved generic boot session starts')
    selected = (session/'bridge.log').read_text()
    assert 'ARG --controller-preferences=/data/mpclearn/controller-preferences' in selected and 'SAVED profile=generic endpoint-type=any client=Saved MCU port=Saved Port' in selected, selected
    previous = app_pid(); depart(True)
    until(lambda: ready_instance(previous), 'saved generic New Project respawn')
    assert 'SAVED profile=generic endpoint-type=any client=Saved MCU port=Saved Port' in (session/'bridge.log').read_text()
    run('stop'); no_raw_state()
    call(dev/'mcu', 'configure', '--profile=generic', '--input-client=Split Controller', '--input-port=From Port', '--input-type=kernel', '--output-client=Split Controller', '--output-port=To Port', '--output-type=user')
    call(dev/'mcu', 'start')
    selected = (session/'bridge.log').read_text()
    assert 'SAVED profile=generic endpoint-type=kernel client=Split Controller port=From Port' in selected and 'to-endpoint-type=user to-client=Split Controller to-port=To Port' in selected, selected
    run('stop'); no_raw_state()
    # Native DAW/CE programs use manual exact port selection through one follower.
    for profile in ('lcxl3-daw', 'launchpad-x-programmer', 'launchpad-pro-mk3-programmer', 'mpk-mini-mk3', 'lpd8-mk2', 'mpk249'):
        call(dev/'mcu', 'configure', '--profile='+profile, expected=2)
        call(dev/'mcu', 'configure', '--profile='+profile,
             '--input-client=Native DAW', '--input-port=From DAW', '--input-type=user',
             '--output-client=Native DAW', '--output-port=To DAW', '--output-type=user')
        call(dev/'mcu', 'start')
        selected = (session/'bridge.log').read_text()
        assert 'SAVED profile='+profile+' endpoint-type=user client=Native DAW port=From DAW' in selected, selected
        assert 'to-endpoint-type=user to-client=Native DAW to-port=To DAW' in selected, selected
        run('bridge-stop')
        call(dev/'mcu-session.sh', 'bridge-start', '--profile='+profile,
             '--input-client=Native DAW', '--input-port=From DAW', '--input-type=user',
             '--output-client=Native DAW', '--output-port=To DAW', '--output-type=user')
        assert (session/'bridge.pid').exists()
        run('stop'); no_raw_state()
    print(f'PASS [{MODE}] six manual controller profiles: split saved ports, no-port refusal, startup and explicit bridge recovery through the follower (native MPC/ALSA substituted)')
    # HUI uses the same fixed preference owner and exact typed endpoint handoff.
    # The mode is explicit data; it is never inferred from the endpoint name.
    p = call(dev/'mcu', 'configure', '--profile=hui', '--endpoint-client=HUI Software', '--endpoint-port=DAW Port', '--endpoint-type=any', expected=2)
    assert 'HUI requires an exact kernel or user endpoint type.' in p.stderr
    call(dev/'mcu', 'configure', '--profile=hui', '--endpoint-client=HUI Software', '--endpoint-port=DAW Port', '--endpoint-type=user', '--hui-mode=motorized')
    call(dev/'mcu', 'start')
    selected = (session/'bridge.log').read_text()
    assert 'SAVED profile=hui endpoint-type=user client=HUI Software port=DAW Port hui-mode=motorized' in selected, selected
    previous = app_pid(); depart(True)
    until(lambda: ready_instance(previous), 'saved HUI New Project respawn')
    assert 'SAVED profile=hui endpoint-type=user client=HUI Software port=DAW Port hui-mode=motorized' in (session/'bridge.log').read_text()
    run('stop'); no_raw_state()
    make_hui_legacy_pickup(controller_preference); call(dev/'mcu', 'start')
    assert 'SAVED profile=hui endpoint-type=user client=HUI Software port=DAW Port hui-mode=pickup' in (session/'bridge.log').read_text()
    run('stop'); no_raw_state()
    call(dev/'mcu', 'configure', '--profile=xtouch-mini')
    call(dev/'mcu', 'start')
    selected = (session/'bridge.log').read_text()
    assert 'SAVED profile=xtouch-mini endpoint-type=kernel client=X-TOUCH MINI port=X-TOUCH MINI MIDI 1' in selected, selected
    adapter_pid = int((session/'manual-adapter-unarmed.pid').read_text().split()[0])
    assert os.readlink(f'/proc/{adapter_pid}/exe').endswith('/mpclearn-controls')
    previous = app_pid(); depart(True)
    until(lambda: ready_instance(previous), 'Mini selected New Project respawn')
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
    run('start')
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
    run('stop'); run('start')
    assert native_values()[53] == 0 and not (session/'bridge.pid').exists()
    (session/'project-ready').touch(); run('status'); time.sleep(1)
    assert not (session/'generation.failed').exists(), 'empty collection must not fail adapter arming'
    sequence = native_request(1, controller_index=0xffffffff)
    until(lambda: native_values()[28] == sequence and native_values()[40] == 1 and (session/'armed').exists(), 'Add starts and arms from an empty collection')
    run('stop'); no_raw_state()
    print(f'PASS [{MODE}] Controllers collection: disable last, failed enable restores exact stopped bytes, remove last, empty startup/status, Add restarts and arms through the follower (collection helper, MPC and ALSA substituted)')
    # The getter probe owns one exact development-only tmpfs record. Ordinary
    # packages still reject that name. A package with the exact define admits
    # only its root-owned 0600/256-byte record through status, idempotent start
    # and stop; unknown or malformed files remain refusals.
    probe_state = state/'plugin-host-probe.state'
    probe_state.write_bytes(bytes(256)); probe_state.chmod(0o600)
    run('status', 1); probe_state.unlink()
    normal_config = (dev/'config.h').read_text()
    (dev/'config.h').write_text(normal_config + f'#define PLUGIN_HOST_PROBE_STATE "{probe_state}"\n'); manifest(dev)
    before = ce_launches(); run('start')
    probe_state.write_bytes(bytes(255)); probe_state.chmod(0o600); run('status', 1)
    probe_state.write_bytes(bytes(256)); probe_state.chmod(0o644); run('status', 1)
    probe_state.chmod(0o600); run('status', 3); run('start'); assert ce_launches() == before+1
    (state/'unexpected-probe-neighbor').touch(); run('status', 1); (state/'unexpected-probe-neighbor').unlink()
    run('stop'); assert not probe_state.exists()  # close archives the instance and empties state
    no_raw_state()
    (dev/'config.h').write_text(normal_config); manifest(dev)
    print(f'PASS [{MODE}] getter probe state: ordinary rejection, exact configured 0600/256 admission through status/idempotent start, close cleanup, malformed/unknown refusal (native getter route substituted)')
    # Disable: the marker makes acvs stock; override removal goes through our folder only.
    run('start'); n = len(launches()); acvs.boot_unit_active = True
    calls = acvs_calls_during(lambda: call(image/'mcu-boot-install.sh', 'disable'))
    until(lambda: stock_since(n), 'disable gives stock'); until(follower_gone, 'disable ends the follower')
    assert (home/'disabled').exists() and not (session/'mpc.pid').exists() and (session/'session.revoked').exists()
    assert sum(1 for c in calls if c[0] == 'restart') == 1, calls
    call(image/'mcu-boot-install.sh', 'enable'); assert not (home/'disabled').exists() and stock_since(n)
    small(); call(image/'mcu-boot-install.sh', 'override', 'clear')
    assert not dev.exists() and sorted(p.name for p in home.iterdir()) == ['controller-preferences', 'history', 'session', 'surface-preferences'], list(home.iterdir())
    assert not acvs.failed and 'start-limit-hit' not in EVENTS.read_text()
    # Nothing outside /data/mpclearn and /run/mpclearn changed, although earlier
    # release folders and the old selector were present throughout.
    assert outside() == before_outside
    print(f'PASS [{MODE}] disable gives stock through the marker, enable leaves the current app, explicit override clear; earlier release folders, old boot/image folders and old selector untouched; acvs never hit its start limit')
finally:
    try: acvs.stop()
    except Exception: pass
    cleanup()
