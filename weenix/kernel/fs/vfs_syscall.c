/*
 *  FILE: vfs_syscall.c
 *  AUTH: mcc | jal
 *  DESC:
 *  DATE: Wed Apr  8 02:46:19 1998
 *  $Id: vfs_syscall.c,v 1.1 2012/10/10 20:06:46 william Exp $
 */

#include "kernel.h"
#include "errno.h"
#include "globals.h"
#include "fs/vfs.h"
#include "fs/file.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/open.h"
#include "fs/fcntl.h"
#include "fs/lseek.h"
#include "mm/kmalloc.h"
#include "util/string.h"
#include "util/printf.h"
#include "fs/stat.h"
#include "util/debug.h"
 
/* To read a file:
 *      o fget(fd)
 *      o call its virtual read f_op
 *      o update f_pos
 *      o fput() it
 *      o return the number of bytes read, or an error
 * 
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not a valid file descriptor or is not open for reading.
 *      o EISDIR
 *        fd refers to a directory.
 *
 * In all cases, be sure you do not leak file refcounts by returning before
 * you fput() a file that you fget()'ed.
 */
int
do_read(int fd, void *buf, size_t nbytes)
{
        file_t* file=fget(fd);
        if(file==NULL||!(file->f_mode&FMODE_READ))
        {
                fput(file);
                return -EBADF;
        }
        if(S_ISDIR(file->f_vnode->vn_mode)||file->f_vnode->vn_ops->read==NULL)
        {
                fput(file);
                return -EISDIR;
        }

        int bytes=file->f_vnode->vn_ops->read(file->f_vnode,file->f_pos,buf,nbytes);
        if(bytes==0)
        {
                bytes=file->f_vnode->vn_len-file->f_pos;
                do_lseek(fd,0,SEEK_END);
        }
        else
        {
                do_lseek(fd,bytes,SEEK_CUR);        
        }
        fput(file);
        return bytes;
}

/* Very similar to do_read.  Check f_mode to be sure the file is writable.  If
 * f_mode & FMODE_APPEND, do_lseek() to the end of the file, call the write
 * f_op, and fput the file.  As always, be mindful of refcount leaks.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not a valid file descriptor or is not open for writing.
 */
int
do_write(int fd, const void *buf, size_t nbytes)
{
        file_t* file=fget(fd);
        if(file==NULL||(!(file->f_mode&FMODE_WRITE)&&!(file->f_mode&FMODE_APPEND)))
        {
                fput(file);
                return -EBADF;
        }
        if(file->f_vnode->vn_ops->write==NULL)
        {
                fput(file);
                return -EISDIR;
        }

        int bytes;
        if(file->f_mode&FMODE_APPEND)
        {
                do_lseek(fd,0,SEEK_END);
                bytes=file->f_vnode->vn_ops->write(file->f_vnode,file->f_pos,buf,nbytes);
                do_lseek(fd,0,SEEK_END);
        }
        else if(file->f_mode&FMODE_WRITE)
        {
                bytes=file->f_vnode->vn_ops->write(file->f_vnode,file->f_pos,buf,nbytes);  
                do_lseek(fd,bytes,SEEK_CUR);     
        }
        fput(file);
        retrun bytes;
}

/*
 * Zero curproc->p_files[fd], and fput() the file. Return 0 on success
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd isn't a valid open file descriptor.
 */
int
do_close(int fd)
{
        file_t* file=fget(fd);
        if(file==NULL)
        {
                fput(file);
                return -EBADF;
        }
        fput(file);
        curproc->p_files[fd]=NULL;

        fput(file);
        /*
        while(vn_refcount!=0)
        {
                fput(fd);
        }
        */
        return 0;
        
}

/* To dup a file:
 *      o fget(fd) to up fd's refcount
 *      o get_empty_fd()
 *      o point the new fd to the same file_t* as the given fd
 *      o return the new file descriptor
 *
 * Don't fput() the fd unless something goes wrong.  Since we are creating
 * another reference to the file_t*, we want to up the refcount.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd isn't an open file descriptor.
 *      o EMFILE
 *        The process already has the maximum number of file descriptors open
 *        and tried to open a new one.
 */
int
do_dup(int fd)
{
        file_t* file=fget(fd);
        if(file==NULL)
        {
                fput(file);
                return -EBADF;
        }
        int new_fd=get_empty_fd(curproc);
        if(new_fd==-EMFILE)
        {
                fput(file);
                return -EMFILE;
        }
        curproc->p_files[new_fd]=file;
        return new_fd;
}

/* Same as do_dup, but insted of using get_empty_fd() to get the new fd,
 * they give it to us in 'nfd'.  If nfd is in use (and not the same as ofd)
 * do_close() it first.  Then return the new file descriptor.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        ofd isn't an open file descriptor, or nfd is out of the allowed
 *        range for file descriptors.
 */
int
do_dup2(int ofd, int nfd)
{
        file_t* file=fget(ofd);
        if(file==NULL)
        {
                fput(file);
                return -EBADF;
        }
        if(curproc->p_files[nfd]!=NULL&&nfd!=ofd)
        {
                do_close(nfd);
        }
        curproc->p_files[nfd]=file;
        return nfd;
}

/*
 * This routine creates a special file of the type specified by 'mode' at
 * the location specified by 'path'. 'mode' should be one of S_IFCHR or
 * S_IFBLK (you might note that mknod(2) normally allows one to create
 * regular files as well-- for simplicity this is not the case in Weenix).
 * 'devid', as you might expect, is the device identifier of the device
 * that the new special file should represent.
 *
 * You might use a combination of dir_namev, lookup, and the fs-specific
 * mknod (that is, the containing directory's 'mknod' vnode operation).
 * Return the result of the fs-specific mknod, or an error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        mode requested creation of something other than a device special
 *        file.
 *      o EEXIST
 *        path already exists.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
/* make block or character special files 
  * S_IFCHR: character special file
  * S_IFBLK: block special file
  * 
  */
int
do_mknod(const char *path, int mode, unsigned devid)
{
        /* validate mode type */
        if (!(S_ISCHR(mode) || (S_ISBLK(mode))))
                return -EINVAL;
        /* get directory vnode*/
        size_t namelen;
        const char *name;
        vnode_t *dir;
        int error;
        if((error = dir_namev(path, &namelen, &name, NULL, &dir) != 0) 
                return error;
        vnode_t *result;
        if ((error = lookup(dir, name, namelen, &result)) != 0)
        {
                /* check if result vnode is a directory vnode*/
                if (error == -ENOTDIR || dir->vn_ops->mknod==NULL||!S_ISDIR(dir->vn_mode))/* ?bug fixed here*/
                        return -ENOTDIR;

                KASSERT(S_ISDIR(result->vn_mode));
                /* path doesn't exist */
                if (error == -ENOENT)
                {
                        if(namelen>STR_MAX)
                                return -ENAMETOOLONG; /*?we need to check length*/
                        int ret = dir->vn_ops->mknod(dir, name, namelen, mode, (devid_t)devid);
                        vput(dir);/*?*/
                        return ret;
                }
                else 
                        return error;
        }
        else
                return -EEXIST;
}

/* Use dir_namev() to find the vnode of the dir we want to make the new
 * directory in.  Then use lookup() to make sure it doesn't already exist.
 * Finally call the dir's mkdir vn_ops. Return what it returns.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EEXIST
 *        path already exists.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_mkdir(const char *path)
{
        size_t namelen;
        const char *name;
        vnode_t *dir_vnode;
        vnode_t *res_vnode;
        int err;
        /*Use dir_namev() to find the vnode*/
        /* Err includeing, ENOENT, ENOTDIR, ENAMETOOLONG*/
        if((err = dir_namev(path, &namelen, &name, NULL, &dir_vnode) != 0)
                return err;
        /* Use lookup() to make sure it doesn't already exist.*/
        if(lookup(dir_vnode, name, namelen, &res_vnode) == 0)
                return EEXIST;
        /* Call the dir's mkdir vn_ops. Return what it returns.*/
        err = dir_vnode -> vn_ops -> mkdir(dir_vnode, name, namelen);
        return err;
}

/* Use dir_namev() to find the vnode of the directory containing the dir to be
 * removed. Then call the containing dir's rmdir v_op.  The rmdir v_op will
 * return an error if the dir to be removed does not exist or is not empty, so
 * you don't need to worry about that here. Return the value of the v_op,
 * or an error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        path has "." as its final component.
 *      o ENOTEMPTY
 *        path has ".." as its final component.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_rmdir(const char *path)
{
        size_t namelen;
        const char *name;
        vnode_t *dir_vnode;
        vnode_t *res_vnode;
        int err, len = 0;
        // Check whether path has ".", ".." as its final component.
        while(name[len++] != '\0');
        len -= 2;
        if(name[len] == '.') {
                if(name[--len] == '/')
                        return EINVAL;
                else if(name[len] == '.')
                        if(name[--len] == '/')
                                return ENOTEMPTY;
        }
        // Use dir_namev() to find the vnode of the directory containing the dir to be removed.
        // Err includeing, ENOENT, ENOTDIR, ENAMETOOLONG
        if((err = dir_namev(path, &namelen, &name, NULL, &dir_vnode) != 0)
                return err;
        // Call the containing dir's rmdir v_op.
        err = dir_vnode -> vn_ops -> rmdir(dir_vnode, name, namelen);
        return err;
        // NOT_YET_IMPLEMENTED("VFS: do_rmdir");
        // return -1;
}

/*
 * Same as do_rmdir, but for files.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EISDIR
 *        path refers to a directory.
 *      o ENOENT
 *        A component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */

/*
 * unlink removes the link to the vnode in dir specified by name
 */
int
do_unlink(const char *path)
{
        NOT_YET_IMPLEMENTED("VFS: do_unlink");
        /* get directory vnode*/
        size_t *namelen;
        const char *name;
        vnode_t *dir;
        int error;
        if ((error = dir_namev(path, namelen, &name, NULL, &dir) != 0) 
                return error;
        /* check if path refers to a directory */
        vnode_t *result;
        if ((error = lookup(dir, name, *namelen, &result)) != 0)
                return error;
        if (!S_ISDIR(result->vn_mode))
                return -EISDIR;
        
        /* reomve the result vnode from the directory*/
        int ret = dir->vn_ops->unlink(dir, name, *namelen);

        vput(result);
        vput(dir);

        return ret;
}

/* To link:
 *      o open_namev(from)
 *      o dir_namev(to)
 *      o call the destination dir's (to) link vn_ops.
 *      o return the result of link, or an error
 *
 * Remember to vput the vnodes returned from open_namev and dir_namev.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EEXIST
 *        to already exists.
 *      o ENOENT
 *        A directory component in from or to does not exist.
 *      o ENOTDIR
 *        A component used as a directory in from or to is not, in fact, a
 *        directory.
 *      o ENAMETOOLONG
 *        A component of from or to was too long.
 */

/*link sets up a hard link. it links oldvnode into dir with the
 * specified name.
 */
int
do_link(const char *from, const char *to)
{
        /* get 'from' vnode */
        int error;
        vnode_t *from_vnode;
        if ((error = open_namev(from, 0, &from_vnode, NULL))!= 0) /*? flag=0*/
                return error;

        /* get 'to' directory vnode*/
        size_t namelen;
        const char *name;
        vnode_t *to_dir;
        if ((error = dir_namev(to, &namelen, &name, NULL, &to_dir)!= 0))
                return error;

        /* check if to_vnode has already existed */
        vnode_t *to_vnode;
        if (lookup(to_dir, name, namelen, &to_vnode) == 0)
                return -EEXIST;

        /* call the destination dir's (to) link vn_ops */
        int ret = dir->vn_ops->link(from_vnode, to_dir, name, namelen);

        /*  vput the vnodes returned from open_namev and dir_namev */
        vput(from_vnode);
        vput(to_dir);

        return ret;
}

/*      o link newname to oldname
 *      o unlink oldname
 *      o return the value of unlink, or an error
 *
 * Note that this does not provide the same behavior as the
 * Linux system call (if unlink fails then two links to the
 * file could exist).
 */
int
do_rename(const char *oldname, const char *newname)
{
        
         NOT_YET_IMPLEMENTED("VFS: do_rename");

         /* link newname to oldname */
         int error;
         if ( (error = do_link(oldname, newname)) != 0)
                return error;
         /* unlink oldname */
        return do_unlink(oldname);
}

/* Make the named directory the current process's cwd (current working
 * directory).  Don't forget to down the refcount to the old cwd (vput()) and
 * up the refcount to the new cwd (open_namev() or vget()). Return 0 on
 * success.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o ENOENT
 *        path does not exist.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 *      o ENOTDIR
 *        A component of path is not a directory.
 */
int
do_chdir(const char *path)
{
        // Get the current process's cwd
        vnode_t *old_cwd = curproc -> p_cwd;
        // Down the refcount to the old cwd
        vput(old_cwd);

        vnode_t *new_cwd;
        int err;
        // Up the refcount; flag!?; Err includeing, ENOENT, ENOTDIR, ENAMETOOLONG
        if((err = open_namev(path, 0, &new_cwd, NULL) != 0)
            return err;
        curproc -> p_cwd = new_cwd;
        return 0;
        // NOT_YET_IMPLEMENTED("VFS: do_chdir");
        // return -1;
}

/* Call the readdir f_op on the given fd, filling in the given dirent_t*.
 * If the readdir f_op is successful, it will return a positive value which
 * is the number of bytes copied to the dirent_t.  You need to increment the
 * file_t's f_pos by this amount.  As always, be aware of refcounts, check
 * the return value of the fget and the virtual function, and be sure the
 * virtual function exists (is not null) before calling it.
 *
 * Return either 0 or sizeof(dirent_t), or -errno.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        Invalid file descriptor fd.
 *      o ENOTDIR
 *        File descriptor does not refer to a directory.
 */
int
do_getdent(int fd, struct dirent *dirp)
{
        file_t *file=fget(fd);
        if(file==NULL)
        {
                fput(fd);
                return -EBADF;
        }
        if(!S_ISDIR(file->f_vnode->vn_mode)||file->f_vnode->vn_ops->readdir==NULL)
        {
                fput(fd);
                return -ENOTDIR;
        }

        int bytes;
        bytes=file->f_vnode->vn_ops->readdir(file->f_vnode,file->f_pos,dirp);
        do_lseek(file,bytes,SEEK_CUR);
        fput(file);
        return bytes;
}

/*
 * Modify f_pos according to offset and whence.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not an open file descriptor.
 *      o EINVAL
 *        whence is not one of SEEK_SET, SEEK_CUR, SEEK_END; or the resulting
 *        file offset would be negative.
 */
int
do_lseek(int fd, int offset, int whence)
{
        if(!(whence==SEEK_SET||whence==SEEK_CUR||whence==SEEK_END))
        {
                return -EINVAL;
        }
        else if(offset<0)
        {
                return -EINVAL;
        }

        file_t *file=fget(fd);
        if(file==NULL)
        {
                fput(file);
                return -EBADF;
        }

        if(whence==SEEK_SET)
        {
                file->f_pos=offset;
        }
        else if(whence==SEEK_CUR)
        {
                file->f_pos=file->f_pos+offset;
        }
        else if(whence==SEEK_END)
        {
                file->f_pos=file->f_vnode->vn_len+offset;
        }
        int ref=file->f_pos;
        fput(file);
        return ref;
}

/*
 * Find the vnode associated with the path, and call the stat() vnode operation.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o ENOENT
 *        A component of path does not exist.
 *      o ENOTDIR
 *        A component of the path prefix of path is not a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_stat(const char *path, struct stat *buf)
{
        size_t len;
        char *name;
        vnode_t* par;
        vnode_t* chd;
        int err=0;
        err=dir_namev(path,&len,&name,NULL,&par);
        if(!err)
        {
            err=lookup(par,name,len,&chd);
            if(!err)
            {
                return chd->vn_ops->stat(chd,buf);
            }
            return err;
        }
        return err;
}

#ifdef __MOUNTING__
/*
 * Implementing this function is not required and strongly discouraged unless
 * you are absolutely sure your Weenix is perfect.
 *
 * This is the syscall entry point into vfs for mounting. You will need to
 * create the fs_t struct and populate its fs_dev and fs_type fields before
 * calling vfs's mountfunc(). mountfunc() will use the fields you populated
 * in order to determine which underlying filesystem's mount function should
 * be run, then it will finish setting up the fs_t struct. At this point you
 * have a fully functioning file system, however it is not mounted on the
 * virtual file system, you will need to call vfs_mount to do this.
 *
 * There are lots of things which can go wrong here. Make sure you have good
 * error handling. Remember the fs_dev and fs_type buffers have limited size
 * so you should not write arbitrary length strings to them.
 */
int
do_mount(const char *source, const char *target, const char *type)
{
        NOT_YET_IMPLEMENTED("MOUNTING: do_mount");
        return -EINVAL;
}

/*
 * Implementing this function is not required and strongly discouraged unless
 * you are absolutley sure your Weenix is perfect.
 *
 * This function delegates all of the real work to vfs_umount. You should not worry
 * about freeing the fs_t struct here, that is done in vfs_umount. All this function
 * does is figure out which file system to pass to vfs_umount and do good error
 * checking.
 */
int
do_umount(const char *target)
{
        NOT_YET_IMPLEMENTED("MOUNTING: do_umount");
        return -EINVAL;
}
#endif
