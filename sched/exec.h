#ifndef _EXEC_H
#define _EXEC_H
#include <fs/vfs.h>
#include <include/types.h>
int exec_from_inode(struct inode *ifile,bool replace,char **argv);
void exec_init();
#define MAX_ARGS 64

#endif