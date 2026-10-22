#ifndef MPCLEARN_PD_PATCH_FILES_H
#define MPCLEARN_PD_PATCH_FILES_H
#define PD_FILES_MAX 128
#define PD_PATH_MAX 512
typedef struct {char name[192];int directory;} PdFileEntry;
typedef struct {char path[PD_PATH_MAX];char message[96];int count,truncated;PdFileEntry entries[PD_FILES_MAX];} PdFiles;
#ifdef __cplusplus
extern "C" {
#endif
int pd_files_open(PdFiles*,const char* path);
int pd_files_parent(PdFiles*);
int pd_files_child_path(const PdFiles*,int index,char* path,unsigned capacity);
const char* pd_files_root(void);
#ifdef __cplusplus
}
#endif
#endif
