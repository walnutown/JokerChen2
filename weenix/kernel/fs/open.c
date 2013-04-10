/*
 *  FILE: open.c
 *  AUTH: mcc | jal
 *  DESC:
 *  DATE: Mon Apr  6 19:27:49 1998
 */

#include "globals.h"
#include "errno.h"
#include "fs/fcntl.h"
#include "util/string.h"
#include "util/printf.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/vfs_syscall.h"
#include "fs/open.h"
#include "fs/stat.h"
#include "util/debug.h"

/* find empty index in p->p_files[] */
int
get_empty_fd(proc_t *p)
{
        int fd;

        for (fd = 0; fd < NFILES; fd++) {
                if (!p->p_files[fd])
                        return fd;
        }

        dbg(DBG_ERROR | DBG_VFS, "ERROR: get_empty_fd: out of file descriptors "
            "for pid %d\n", curproc->p_pid);
        return -EMFILE;
}

/*
 * There a number of steps to opening a file:
 *      1. Get the next empty file descriptor.
 *      2. Call fget to get a fresh file_t.
 *      3. Save the file_t in curproc's file descriptor table.
 *      4. Set file_t->f_mode to OR of FMODE_(READ|WRITE|APPEND) based on
 *         oflags, which can be O_RDONLY, O_WRONLY or O_RDWR, possibly OR'd with
 *         O_APPEND.
 *      5. Use open_namev() to get the vnode for the file_t.
 *      6. Fill in the fields of the file_t.
 *      7. Return new fd.
 *
 * If anything goes wrong at any point (specifically if the call to open_namev
 * fails), be sure to remove the fd from curproc, fput the file_t and return an
 * error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        oflags is not valid.
 *      o EMFILE
 *        The process already has the maximum number of files open.
 *      o ENOMEM
 *        Insufficient kernel memory was available.
 *      o ENAMETOOLONG
 *        A component of filename was too long.
 *      o ENOENT
 *        O_CREAT is not set and the named file does not exist.  Or, a
 *        directory component in pathname does not exist.
 *      o EISDIR
 *        pathname refers to a directory and the access requested involved
 *        writing (that is, O_WRONLY or O_RDWR is set).
 *      o ENXIO
 *        pathname refers to a device special file and no corresponding device
 *        exists.
 */

 /* added by taohu*/
#ifndef MAX_PATH
#define MAX_PATH 256
#endif

int
do_open(const char *filename, int oflags)
{
    NOT_YET_IMPLEMENTED("VFS: do_open");
   
    
    if (!(oflag == O_RDONLY || oflag == O_WRONLY || oflag == O_RDWR || (oflag == O_RDONLY | O_APPEND) || (oflag == O_WRONLY | O_APPEND) || (oflag == O_RDWR | O_APPEND)))
        return -EINVAL;
    /*-- 1. Get the next empty file descriptor --*/
    int fd = get_empty_fd(curproc);
    if (fd == -EMFILE)
        return -EMFILE;
 
    /*-- 2. Call fget to get a fresh file_t --*/
    file_t *f = fget(-1);
    if (f == NULL)
        return -ENOMEM;

    /*-- 3. Save the file_t in curproc's file descriptor table --*/
    KASSERT(!curproc->p_files[fd]);
    curproc->p_files[fd] = f;
    
    /*-- 4. Set file_t->f_mode to OR of FMODE_(READ|WRITE|APPEND) based on oflags --*/
    switch(oflags)
    {
        case O_RDONLY:
            f->f_mode = FMODE_READ;
            break;
        case O_WRONLY:
            f->f_mode = FMODE_WRITE;
            break;
        case O_RDWR:
            f->f_mode = FMODE_READ | FMODE_WRITE;
            break;
        case (O_RDONLY | O_APPEND):
            f->f_mode = FMODE_READ | FMODE_APPEND;
            break;
        case (O_WRONLY | O_APPEND):
            f->f_mode = FMODE_WRITE | FMODE_APPEND;
            break;
        case (O_RDWR | O_APPEND):
            f->f_mode = FMODE_READ | FMODE_WRITE | FMODE_APPEND;
            break;
        default:
            break;      
    }
    
    /* 5. Use open_namev() to get the vnode for the file_t; 
     * if fails, remove the fd from curproc, fput the file_t
     * and return an error. 
     */
    vnode_t **res_vnode;
    if(open_namev(filename, oflags, &res_vnode, NULL) == #error )  
    {
        /* to do */
        {ENAMETOOLONG}
        {ENOENT, EISDIR, ENXIO}

        curproc->p_files[fd] = NULL;
        fput(f);
        return 
    }

    /*-- 6. Fill in the fields of the file_t --*/
    f->f_vnode = *res_vnode;
    
        return fd;
}