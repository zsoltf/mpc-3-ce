#define _XOPEN_SOURCE 700
#include "patch-files.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
static const char root[]="/tmp/mpclearn-pd-touch/library";
const char* pd_files_root(void){return root;}
static int compare(const void* a,const void* b){const PdFileEntry* x=a;const PdFileEntry* y=b;return x->directory!=y->directory?y->directory-x->directory:strcmp(x->name,y->name);}
int pd_files_child_path(const PdFiles* f,int index,char* path,unsigned capacity){if(!f||index<0||index>=f->count||!path)return 0;int n=snprintf(path,capacity,"%s/%s",f->path,f->entries[index].name);return n>0&&(unsigned)n<capacity;}
int pd_files_open(PdFiles* f,const char* path){
    if(!f||!path)return 0;char resolved[4096],resolved_root[4096];
    if(!realpath(root,resolved_root)||!realpath(path,resolved)||strncmp(resolved,resolved_root,strlen(resolved_root))||(resolved[strlen(resolved_root)]&&resolved[strlen(resolved_root)]!='/')||strlen(resolved)>=sizeof(f->path)){
        snprintf(f->message,sizeof(f->message),"Patch library unavailable");return 0;}
    DIR* dir=opendir(resolved);if(!dir){snprintf(f->message,sizeof(f->message),"Cannot open folder");return 0;}
    f->count=f->truncated=0;snprintf(f->path,sizeof(f->path),"%s",resolved);
    struct dirent* entry;unsigned visited=0;
    while((entry=readdir(dir))&&visited++<4096){
        if(entry->d_name[0]=='.')continue;size_t length=strlen(entry->d_name);
        if(length>=sizeof(f->entries[0].name)){f->truncated=1;continue;}
        char full[PD_PATH_MAX];int n=snprintf(full,sizeof(full),"%s/%s",resolved,entry->d_name);struct stat st;
        if(n<0||(unsigned)n>=sizeof(full)||lstat(full,&st))continue;
        if(!S_ISDIR(st.st_mode)&&!(S_ISREG(st.st_mode)&&length>3&&!strcmp(entry->d_name+length-3,".pd")))continue;
        if(f->count==PD_FILES_MAX){f->truncated=1;continue;}
        PdFileEntry* out=&f->entries[f->count++];strcpy(out->name,entry->d_name);out->directory=S_ISDIR(st.st_mode);
    }
    if(visited>=4096)f->truncated=1;closedir(dir);qsort(f->entries,f->count,sizeof(f->entries[0]),compare);
    snprintf(f->message,sizeof(f->message),f->truncated?"Showing first 128 entries; folder truncated":"%d patches/folders",f->count);return 1;
}
int pd_files_parent(PdFiles* f){char resolved_root[4096];if(!f||!realpath(root,resolved_root)||!strcmp(f->path,resolved_root))return 0;char path[PD_PATH_MAX];strcpy(path,f->path);char* slash=strrchr(path,'/');if(!slash)return 0;*slash=0;return pd_files_open(f,path);}
