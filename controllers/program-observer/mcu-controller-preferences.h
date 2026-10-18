/* Session-owned controller collection. Fixed-size atomic file; no MPC state. */
#ifndef MPCLEARN_MCU_CONTROLLER_PREFERENCES_H
#define MPCLEARN_MCU_CONTROLLER_PREFERENCES_H

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mcu-profile.h"

#define MCU_CONTROLLER_NAME_MAX 128u
#define MCU_CONTROLLER_CAPACITY 16u

/* Read-only compatibility with the released singleton selection. */
typedef struct {
 char magic[8];
 uint32_t profile,physical_only,client_length,port_length,reserved;
 unsigned char client[MCU_CONTROLLER_NAME_MAX],port[MCU_CONTROLLER_NAME_MAX];
 uint32_t checksum;
} McuControllerPreferenceFile;

typedef struct {
 uint32_t physical_only,client_length,port_length,reserved;
 unsigned char client[MCU_CONTROLLER_NAME_MAX],port[MCU_CONTROLLER_NAME_MAX];
} McuControllerEndpointFile;

typedef struct {
 uint32_t enabled,profile,hui_policy,reserved;
 McuControllerEndpointFile input,output;
} McuControllerRecordFile;

typedef struct {
 char magic[8];
 uint32_t version,count,reserved[3];
 McuControllerRecordFile records[MCU_CONTROLLER_CAPACITY];
 uint32_t checksum;
} McuControllerCollectionFile;

typedef struct {
 McuEndpoint input,output;
 /* Compatibility view for the old singleton UI: it presents the input. */
 McuEndpoint endpoint;
 char input_client[MCU_CONTROLLER_NAME_MAX+1],input_port[MCU_CONTROLLER_NAME_MAX+1];
 char output_client[MCU_CONTROLLER_NAME_MAX+1],output_port[MCU_CONTROLLER_NAME_MAX+1];
 char client[MCU_CONTROLLER_NAME_MAX+1],port[MCU_CONTROLLER_NAME_MAX+1];
 unsigned enabled,hui_policy;
} McuControllerPreference;

typedef struct {
 McuControllerPreference records[MCU_CONTROLLER_CAPACITY];
 unsigned count,imported_legacy;
} McuControllerCollection;

_Static_assert(sizeof(McuControllerPreferenceFile)==288,"fixed legacy controller preference file");
_Static_assert(sizeof(McuControllerEndpointFile)==272,"fixed controller endpoint record");
_Static_assert(sizeof(McuControllerRecordFile)==560,"fixed controller record");
_Static_assert(sizeof(McuControllerCollectionFile)==8992,"fixed controller collection file");

static uint32_t mcu_controller_checksum_bytes(const void *data,size_t bytes){
 const unsigned char *p=(const unsigned char*)data;uint32_t value=2166136261u;
 for(size_t i=0;i<bytes;i++){value^=p[i];value*=16777619u;}
 return value;
}

static uint32_t mcu_controller_checksum(const McuControllerPreferenceFile *file){
 return mcu_controller_checksum_bytes(file,sizeof(*file)-sizeof(file->checksum));
}
static uint32_t mcu_controller_collection_checksum(const McuControllerCollectionFile *file){
 return mcu_controller_checksum_bytes(file,sizeof(*file)-sizeof(file->checksum));
}

static unsigned mcu_controller_profile_id(const McuProfile *profile){
 if(profile==&mcu_profile_xtouch)return 1;
 if(profile==&mcu_profile_xtouch_mini)return 2;
 if(profile==&mcu_profile_generic)return 3;
 if(profile==&mcu_profile_hui)return 4;
 if(profile==&mcu_profile_lcxl3)return 5;
 if(profile==&mcu_profile_lpx)return 6;
 if(profile==&mcu_profile_lpp3)return 7;
 if(profile==&mcu_profile_mpk_mini3)return 8;
 if(profile==&mcu_profile_lpd8_mk2)return 9;
 if(profile==&mcu_profile_mpk249)return 10;
 return 0;
}
static const McuProfile *mcu_controller_profile(unsigned id){
 return id==1?&mcu_profile_xtouch:id==2?&mcu_profile_xtouch_mini:id==3?&mcu_profile_generic:id==4?&mcu_profile_hui:id==5?&mcu_profile_lcxl3:id==6?&mcu_profile_lpx:id==7?&mcu_profile_lpp3:id==8?&mcu_profile_mpk_mini3:id==9?&mcu_profile_lpd8_mk2:id==10?&mcu_profile_mpk249:NULL;
}

static void mcu_controller_fixed_endpoint(McuEndpoint *endpoint,const McuProfile *profile){
 *endpoint=(McuEndpoint){.profile=profile,.client=profile->default_client,.port=profile->default_port,.physical_only=MCU_ENDPOINT_KERNEL};
}
static void mcu_controller_default(McuControllerPreference *selection){
 memset(selection,0,sizeof(*selection));selection->enabled=1;
 mcu_controller_fixed_endpoint(&selection->input,&mcu_profile_xtouch);
 mcu_controller_fixed_endpoint(&selection->output,&mcu_profile_xtouch);
 selection->endpoint=selection->input;
}
static void mcu_controller_collection_default(McuControllerCollection *collection){
 memset(collection,0,sizeof(*collection));collection->count=1;
 mcu_controller_default(collection->records);
}
static int mcu_controller_policy_valid(const McuProfile *profile,unsigned policy){
 return profile==&mcu_profile_hui?(policy==MCU_HUI_PICKUP||policy==MCU_HUI_MOTORIZED):policy==MCU_HUI_NONE;
}

static int mcu_controller_endpoint_copy(McuEndpoint *destination,char *client,char *port,const McuEndpoint *source){
 if(!destination||!client||!port||!mcu_endpoint_valid(source))return 0;
 memset(client,0,MCU_CONTROLLER_NAME_MAX+1);memset(port,0,MCU_CONTROLLER_NAME_MAX+1);
 size_t client_length=strlen(source->client),port_length=strlen(source->port);
 if(!client_length||!port_length||client_length>MCU_CONTROLLER_NAME_MAX||port_length>MCU_CONTROLLER_NAME_MAX)return 0;
 memcpy(client,source->client,client_length+1);memcpy(port,source->port,port_length+1);
 *destination=(McuEndpoint){.profile=source->profile,.client=client,.port=port,.physical_only=source->physical_only};
 return 1;
}

/* The runtime form owns its generic endpoint strings. Never assign it by
 * value: endpoint pointers must be rebound to the destination buffers. */
static int mcu_controller_preference_copy(McuControllerPreference *destination,const McuControllerPreference *source){
 if(!destination||!source)return 0;
 McuControllerPreference copy;memset(&copy,0,sizeof(copy));
 copy.enabled=source->enabled;copy.hui_policy=source->hui_policy;
 if(!mcu_controller_endpoint_copy(&copy.input,copy.input_client,copy.input_port,&source->input)||
    !mcu_controller_endpoint_copy(&copy.output,copy.output_client,copy.output_port,&source->output))return 0;
 copy.endpoint=copy.input;
 memcpy(copy.client,copy.input_client,sizeof(copy.client));memcpy(copy.port,copy.input_port,sizeof(copy.port));
 *destination=copy;
 if(destination->input.client==copy.input_client){destination->input.client=destination->input_client;destination->input.port=destination->input_port;}
 if(destination->output.client==copy.output_client){destination->output.client=destination->output_client;destination->output.port=destination->output_port;}
 destination->endpoint=destination->input;
 return 1;
}

static int mcu_controller_decode_endpoint(const McuControllerEndpointFile *file,const McuProfile *profile,McuEndpoint *endpoint,char *client,char *port){
 if(!file||!profile||file->reserved||file->physical_only>MCU_ENDPOINT_USER||
    file->client_length>MCU_CONTROLLER_NAME_MAX||file->port_length>MCU_CONTROLLER_NAME_MAX)return 0;
 if(profile->default_client&&!file->client_length&&!file->port_length){
  if(file->physical_only!=MCU_ENDPOINT_KERNEL)return 0;
  mcu_controller_fixed_endpoint(endpoint,profile);return 1;
 }
 if(!file->client_length||!file->port_length)return 0;
 memcpy(client,file->client,file->client_length);memcpy(port,file->port,file->port_length);
 *endpoint=(McuEndpoint){.profile=profile,.client=client,.port=port,.physical_only=file->physical_only};
 return mcu_endpoint_valid(endpoint);
}

static int mcu_controller_decode_record(const McuControllerRecordFile *file,McuControllerPreference *selection){
 const McuProfile *profile=file?mcu_controller_profile(file->profile):NULL;
 if(!profile||file->enabled>1||file->reserved||!mcu_controller_policy_valid(profile,file->hui_policy))return 0;
 memset(selection,0,sizeof(*selection));selection->enabled=file->enabled;selection->hui_policy=file->hui_policy;
 if(!mcu_controller_decode_endpoint(&file->input,profile,&selection->input,selection->input_client,selection->input_port)||
    !mcu_controller_decode_endpoint(&file->output,profile,&selection->output,selection->output_client,selection->output_port))return 0;
 selection->endpoint=selection->input;
 memcpy(selection->client,selection->input_client,sizeof(selection->client));
 memcpy(selection->port,selection->input_port,sizeof(selection->port));
 return 1;
}

static int mcu_controller_decode_legacy(const McuControllerPreferenceFile *file,McuControllerPreference *selection){
 const McuProfile *profile=file?mcu_controller_profile(file->profile):NULL;
 int ok=file&&!memcmp(file->magic,"MCUCFG1",8)&&file->checksum==mcu_controller_checksum(file)&&profile&&
  file->physical_only<=MCU_ENDPOINT_USER&&file->client_length<=MCU_CONTROLLER_NAME_MAX&&file->port_length<=MCU_CONTROLLER_NAME_MAX;
 if(!ok)return 0;
 memset(selection,0,sizeof(*selection));selection->enabled=1;
 if(profile->default_client){
  if(!file->physical_only||file->client_length||file->port_length)return 0;
  mcu_controller_fixed_endpoint(&selection->input,profile);mcu_controller_fixed_endpoint(&selection->output,profile);
 }else{
  if(!file->client_length||!file->port_length)return 0;
  memcpy(selection->input_client,file->client,file->client_length);memcpy(selection->input_port,file->port,file->port_length);
  memcpy(selection->output_client,file->client,file->client_length);memcpy(selection->output_port,file->port,file->port_length);
  selection->input=(McuEndpoint){.profile=profile,.client=selection->input_client,.port=selection->input_port,.physical_only=file->physical_only};
  selection->output=(McuEndpoint){.profile=profile,.client=selection->output_client,.port=selection->output_port,.physical_only=file->physical_only};
 }
 selection->hui_policy=profile==&mcu_profile_hui?(file->reserved?file->reserved:MCU_HUI_PICKUP):MCU_HUI_NONE;
 if(!mcu_controller_policy_valid(profile,selection->hui_policy)||!mcu_endpoint_valid(&selection->input)||!mcu_endpoint_valid(&selection->output))return 0;
 selection->endpoint=selection->input;
 memcpy(selection->client,selection->input_client,sizeof(selection->client));memcpy(selection->port,selection->input_port,sizeof(selection->port));
 return 1;
}

static int mcu_controller_read_exact(int fd,void *bytes,size_t length){
 size_t used=0;
 while(used<length){ssize_t count=read(fd,(unsigned char*)bytes+used,length-used);if(count<0&&errno==EINTR)continue;if(count<=0)return 0;used+=(size_t)count;}
 unsigned char extra;return read(fd,&extra,1)==0;
}

/* Missing means the historical X-Touch default. Existing malformed files are
 * rejected. MCUCFG1 is imported in memory and is not rewritten until Apply. */
static int mcu_controller_preferences_read_collection(const char *path,McuControllerCollection *collection){
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
 if(fd<0){if(errno==ENOENT){mcu_controller_collection_default(collection);return 1;}return 0;}
 struct stat st;
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid!=geteuid()||(st.st_mode&0777)!=0600){close(fd);return 0;}
 union {McuControllerPreferenceFile legacy;McuControllerCollectionFile current;} image;
 size_t bytes=st.st_size==(off_t)sizeof(image.legacy)?sizeof(image.legacy):st.st_size==(off_t)sizeof(image.current)?sizeof(image.current):0;
 memset(&image,0,sizeof(image));int exact=bytes&&mcu_controller_read_exact(fd,&image,bytes);close(fd);if(!exact)return 0;
 memset(collection,0,sizeof(*collection));
 if(bytes==sizeof(image.legacy)){
  if(!mcu_controller_decode_legacy(&image.legacy,collection->records))return 0;
  collection->count=1;collection->imported_legacy=1;return 1;
 }
 McuControllerCollectionFile *file=&image.current;
 if(memcmp(file->magic,"MCUCFG2",8)||file->version!=2||file->count>MCU_CONTROLLER_CAPACITY||
    file->reserved[0]||file->reserved[1]||file->reserved[2]||file->checksum!=mcu_controller_collection_checksum(file))return 0;
 for(unsigned i=0;i<file->count;i++)if(!mcu_controller_decode_record(file->records+i,collection->records+i))return 0;
 collection->count=file->count;return 1;
}

static int mcu_controller_preferences_read(const char *path,McuControllerPreference *selection){
 McuControllerCollection collection;
 if(!mcu_controller_preferences_read_collection(path,&collection)||!collection.count)return 0;
 for(unsigned i=0;i<collection.count;i++)if(collection.records[i].enabled)
  return mcu_controller_preference_copy(selection,collection.records+i);
 return 0;
}

static int mcu_controller_encode_endpoint(McuControllerEndpointFile *file,const McuEndpoint *endpoint,const McuProfile *profile){
 if(!file||!endpoint||endpoint->profile!=profile||endpoint->physical_only>MCU_ENDPOINT_USER||!mcu_endpoint_valid(endpoint))return 0;
 memset(file,0,sizeof(*file));file->physical_only=endpoint->physical_only;
 size_t client=strlen(endpoint->client),port=strlen(endpoint->port);
 if(!client||!port||client>MCU_CONTROLLER_NAME_MAX||port>MCU_CONTROLLER_NAME_MAX)return 0;
 file->client_length=(uint32_t)client;file->port_length=(uint32_t)port;
 memcpy(file->client,endpoint->client,client);memcpy(file->port,endpoint->port,port);return 1;
}

static int mcu_controller_encode_record(McuControllerRecordFile *file,const McuControllerPreference *selection){
 const McuProfile *profile=selection?selection->input.profile:NULL;unsigned profile_id=mcu_controller_profile_id(profile);
 if(!profile_id||selection->output.profile!=profile||selection->enabled>1||!mcu_controller_policy_valid(profile,selection->hui_policy))return 0;
 memset(file,0,sizeof(*file));file->enabled=selection->enabled;file->profile=profile_id;file->hui_policy=selection->hui_policy;
 return mcu_controller_encode_endpoint(&file->input,&selection->input,profile)&&mcu_controller_encode_endpoint(&file->output,&selection->output,profile);
}

static int mcu_controller_preference_directory(const char *path,char *directory,size_t capacity){
 const char *slash=path?strrchr(path,'/'):NULL;
 if(!slash||slash==path||strcmp(slash+1,"controller-preferences")||(size_t)(slash-path)>=capacity)return 0;
 memcpy(directory,path,(size_t)(slash-path));directory[slash-path]=0;return 1;
}
static int mcu_controller_write_exact(int fd,const void *bytes,size_t length){
 size_t used=0;
 while(used<length){ssize_t count=write(fd,(const unsigned char*)bytes+used,length-used);if(count<0&&errno==EINTR)continue;if(count<=0)return 0;used+=(size_t)count;}
 return 1;
}
static int mcu_controller_replace(const char *path,const void *bytes,size_t length,const char *temporary){
 char directory[PATH_MAX];if(!mcu_controller_preference_directory(path,directory,sizeof(directory)))return 0;
 int dir=open(directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(dir<0)return 0;
 struct stat st,old;int exists=fstatat(dir,"controller-preferences",&old,AT_SYMLINK_NOFOLLOW)==0,old_error=errno;
 int ok=!fstat(dir,&st)&&S_ISDIR(st.st_mode)&&st.st_uid==geteuid()&&(st.st_mode&0777)==0700&&
  (!exists||(S_ISREG(old.st_mode)&&old.st_uid==geteuid()&&(old.st_mode&0777)==0600));
 if(!exists&&old_error!=ENOENT)ok=0;
 struct stat pending;
 if(ok&&!fstatat(dir,temporary,&pending,AT_SYMLINK_NOFOLLOW))
  ok=S_ISREG(pending.st_mode)&&pending.st_uid==geteuid()&&(pending.st_mode&0777)==0600&&!unlinkat(dir,temporary,0);
 else if(ok&&errno!=ENOENT)ok=0;
 if(!ok){close(dir);return 0;}
 int fd=openat(dir,temporary,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
 if(fd<0){close(dir);return 0;}
 ok=mcu_controller_write_exact(fd,bytes,length)&&!fsync(fd);if(close(fd))ok=0;
 if(ok)ok=!renameat(dir,temporary,dir,"controller-preferences");
 if(!ok)unlinkat(dir,temporary,0);else if(fsync(dir))fprintf(stderr,"Controller preference renamed; directory sync failed: %d\n",errno);
 close(dir);return ok;
}

static int mcu_controller_preferences_write_collection(const char *path,const McuControllerCollection *collection){
 if(!collection||collection->count>MCU_CONTROLLER_CAPACITY)return 0;
 McuControllerCollectionFile file={.magic="MCUCFG2",.version=2,.count=collection->count};
 for(unsigned i=0;i<collection->count;i++)if(!mcu_controller_encode_record(file.records+i,collection->records+i))return 0;
 file.checksum=mcu_controller_collection_checksum(&file);
 return mcu_controller_replace(path,&file,sizeof(file),".controller-preferences.new");
}

static int mcu_controller_preferences_write(const char *path,const McuEndpoint *input,const McuEndpoint *output,unsigned hui_policy){
 if(!input||!output)return 0;
 McuControllerCollection collection;memset(&collection,0,sizeof(collection));collection.count=1;
 collection.records[0].enabled=1;collection.records[0].input=*input;collection.records[0].output=*output;
 collection.records[0].endpoint=*input;collection.records[0].hui_policy=hui_policy;
 return mcu_controller_preferences_write_collection(path,&collection);
}

/* Restore an exact missing, MCUCFG1 or MCUCFG2 session snapshot. */
static int mcu_controller_preferences_restore(const char *path,const char *snapshot,int missing){
 if(!snapshot)return 0;
 int source=open(snapshot,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);if(source<0)return 0;
 struct stat backup;
 if(fstat(source,&backup)||!S_ISREG(backup.st_mode)||backup.st_uid!=geteuid()||(backup.st_mode&0777)!=0600||
    (missing?backup.st_size!=0:(backup.st_size!=(off_t)sizeof(McuControllerPreferenceFile)&&backup.st_size!=(off_t)sizeof(McuControllerCollectionFile)))){
  close(source);return 0;
 }
 unsigned char bytes[sizeof(McuControllerCollectionFile)];size_t total=(size_t)backup.st_size;
 int exact=mcu_controller_read_exact(source,bytes,total);close(source);if(!exact)return 0;
 if(!missing){McuControllerCollection checked;if(!mcu_controller_preferences_read_collection(snapshot,&checked))return 0;}
 if(missing){
  char directory[PATH_MAX];if(!mcu_controller_preference_directory(path,directory,sizeof(directory)))return 0;
  int dir=open(directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(dir<0)return 0;
  struct stat st,old;int exists=fstatat(dir,"controller-preferences",&old,AT_SYMLINK_NOFOLLOW)==0,old_error=errno;
  int ok=!fstat(dir,&st)&&S_ISDIR(st.st_mode)&&st.st_uid==geteuid()&&(st.st_mode&0777)==0700&&
   (!exists||(S_ISREG(old.st_mode)&&old.st_uid==geteuid()&&(old.st_mode&0777)==0600));
  if(!exists&&old_error!=ENOENT)ok=0;
  if(ok&&exists)ok=!unlinkat(dir,"controller-preferences",0);
  if(ok)ok=!fsync(dir);
  close(dir);return ok;
 }
 return mcu_controller_replace(path,bytes,total,".controller-preferences.restore");
}

#endif
