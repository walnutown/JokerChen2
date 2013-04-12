#pragma once

#include "dirent.h"

#include "types.h"

#include "fs/open.h"
#include "fs/stat.h"

/* return 0 or error */
int do_close(int fd);
/* return bytes or error */
int do_read(int fd, void *buf, size_t nbytes);
/* return bytes or error */
int do_write(int fd, const void *buf, size_t nbytes);
/* return new fd or error */
int do_dup(int fd);
/* return new fd or error */
int do_dup2(int ofd, int nfd);
/* return 0 or error */
int do_mknod(const char *path, int mode, unsigned devid);
/* return 0 or error */
int do_mkdir(const char *path);
/* return 0 or error */
int do_rmdir(const char *path);
/* return 0 or error */
int do_unlink(const char *path);
/* return 0 or error */
int do_link(const char *from, const char *to);
/* return 0 or error */
int do_rename(const char *oldname, const char *newname);
/* return 0 or error */
int do_chdir(const char *path);
/* return either 0 or sizeof(dirent_t), or -errno */
int do_getdent(int fd, struct dirent *dirp);
/* return ref */
int do_lseek(int fd, int offset, int whence);
/* return 0 or error */
int do_stat(const char *path, struct stat *uf);

#ifdef __MOUNTING__
/* for mounting implementations only, not required */
int do_mount(const char *source, const char *target, const char *type);
int do_umount(const char *target);
#endif
