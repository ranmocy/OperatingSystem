#include "filesys/path.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include <string.h>

struct file* path_goto(struct file* dir, const char* path){
	char name_buf[15];
	struct inode *inode;
	if (!filesys_isdir(dir)){
		file_close(dir);
		return NULL;
	}
	if (*path == '/'){
		file_close(dir);
		dir = dir_open_root();
		path++;
	}
	while(true){
		const char *next;
		if (!filesys_isdir(dir)){
			file_close(dir);
			return NULL;
		}
		while(*path == '/')
			path++;
		if (*path == '\0')
			return dir;
		next = path;
		while(*next != '/' && *next != '\0')
			next++;
		if (next - path > 14){
			file_close(dir);
			return NULL;
		}
		memcpy(name_buf, path, next - path);
		name_buf[next - path] = '\0';

		if (!strcmp(name_buf,"."))
			continue;
		if (!strcmp(name_buf,"..")){
			inode = inode_open(dir_parent(dir));
			file_close(dir);
			if (inode == NULL)
				return NULL;

		}else if (!dir_lookup(dir, name_buf, &inode)){
			file_close(dir);
			return NULL;
		}else
			file_close(dir);
		dir = file_open(inode);
		path = next;
		if (dir == NULL)
			return NULL;
	}
}

char *path_split(char* path){
	size_t i = strlen(path);
	while ((path[i]=='\0' || path[i]=='/') && i!=0)
		i--;
	while(path[i]!='/'&&i!=0)
		i--;
	if (path[i]!='/')
		return NULL;
	path[i] = '\0';
	return path + i + 1;
}