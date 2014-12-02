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
		path = next;
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

		if (dir == NULL)
			return NULL;
	}
}

bool path_split(char *path, char *fname){
	size_t i = strlen(path), j;
	if (!strcmp(path,"."))
		return false;
	while(true){
		if (i == 0 || path[i-1]!='/')
			break;
		i--;
	}
	if (i == 0)
		return false;
	j = i;
	while(true){
		if (j == 0 || path[j-1]=='/')
			break;
		j--;
	}

	memcpy(fname, path + j, i-j);
	fname[i-j] = '\0';
	if (j == 0)
		memcpy(path, ".", 2);
	else
		path[j]='\0'; 
	return true;
}
