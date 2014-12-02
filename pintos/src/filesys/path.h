#include "filesys/directory.h"

#define PATH_SIZE_LIMIT 256

struct file* path_goto(struct file* dir, const char* path);

char *path_split(char* path);