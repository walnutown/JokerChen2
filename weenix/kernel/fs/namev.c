#include "kernel.h"
#include "globals.h"
#include "types.h"
#include "errno.h"
    
#include "util/string.h"
#include "util/printf.h"
#include "util/debug.h"
   
#include "fs/dirent.h"
#include "fs/fcntl.h"
#include "fs/stat.h"
#include "fs/vfs.h"
#include "fs/vnode.h"


/* This takes a base 'dir', a 'name', its 'len', and a result vnode.
 * Most of the work should be done by the vnode's implementation
 * specific lookup() function, but you may want to special case
 * "." and/or ".." here depnding on your implementation.
 * 
 * If dir has no lookup(), return -ENOTDIR.
 * 
 * Note: returns with the vnode refcount on *result incremented.
 */
int
lookup(vnode_t *dir, const char *name, size_t len, vnode_t **result)
{
        if(dir->vn_ops->lookup==NULL)
        {
            return -ENOTDIR;
        }
        else
        {
             /*don't have access to parent vnode*/
             /*if(name_match(name,".",1))
             {
                dirent_t *dit;
                dir->vn_ops->readdir(dir,vn_len,dit);
                
                *result=dir;
                return 0;
             }
             else if(name_match(name,"..",2))
             {
                *result=dir->
             }*/
            return dir->vn_ops->lookup(dir,name,len,result);
        }
        
}

/* When successful this function returns data in the following "out"-arguments:
 *  o res_vnode: the vnode of the parent directory of "name"
 *  o name: the `basename' (the element of the pathname)
 *  o namelen: the length of the basename
 *
 * For example: dir_namev("/s5fs/bin/ls", &namelen, &name, NULL,
 * &res_vnode) would put 2 in namelen, "ls" in name, and a pointer to the
 * vnode corresponding to "/s5fs/bin" in res_vnode.
 *
 * The "base" argument defines where we start resolving the path from:
 * A base value of NULL means to use the process's current working directory,
 * curproc->p_cwd.  If pathname[0] == '/', ignore base and start with
 * vfs_root_vn.  dir_namev() should call lookup() to take care of resolving each
 * piece of the pathname.
 *
 * Note: A successful call to this causes vnode refcount on *res_vnode to
 * be incremented.
 */
int
dir_namev(const char *pathname, size_t *namelen, const char **name,
          vnode_t *base, vnode_t **res_vnode)
{
        int err=0;
        base=base==NULL?curproc->p_cwd:base;
        base=pathname[0]=='/'?vfs_root_vn:base;
        int last=pathname[0]=='/'?1:0;
        do
        {
            while(pathname[i++]!='/')
            {
                if(pathname[i]=='\0')
                {
                    res_vnode=*base;
                    namelen=i-last;
                    name=&pathname[last];
                    return 0;
                }
            }

            if(pathname[i]!='\0')
            {
                if(i-last-1>STR_MAX)
                    return -ENAMETOOLONG;
                if(err=lookup(base,pathname[last],i-last-1,res_vnode))
                    return err;
                last=i;
                base=*res_vnode;
            }
            else
            {
                res_vnode=*base;
                namelen=0;
                name=NULL;
                return 0;
            }
        }while(1)
}

/* This returns in res_vnode the vnode requested by the other parameters.
 * It makes use of dir_namev and lookup to find the specified vnode (if it
 * exists).  flag is right out of the parameters to open(2); see
 * <weenix/fnctl.h>.  If the O_CREAT flag is specified, and the file does
 * not exist call create() in the parent directory vnode.
 *
 * Note: Increments vnode refcount on *res_vnode.
 */
int
open_namev(const char *pathname, int flag, vnode_t **res_vnode, vnode_t *base)
{
        size_t len;
        char *name;
        int err=0;
        err=dir_namev(pathname,&len,&name,base,res_vnode);
        if(!err)
        {
            err=lookup(*res_vnode,name,len,res_vnode);
            if(len>STR_MAX)
                err= -ENAMETOOLONG;
            if((err==-ENOENT)&&(O_CREAT&flag)&&((*res_vnode)->vn_ops->create!=NULL))
            {
                return (*res_vnode)->vn_ops->create(*res_vnode,name,len,res_vnode);
            }
        }
        return err;
}

#ifdef __GETCWD__
/* Finds the name of 'entry' in the directory 'dir'. The name is writen
 * to the given buffer. On success 0 is returned. If 'dir' does not
 * contain 'entry' then -ENOENT is returned. If the given buffer cannot
 * hold the result then it is filled with as many characters as possible
 * and a null terminator, -ERANGE is returned.
 *
 * Files can be uniquely identified within a file system by their
 * inode numbers. */
int
lookup_name(vnode_t *dir, vnode_t *entry, char *buf, size_t size)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_name");
        return -ENOENT;
}


/* Used to find the absolute path of the directory 'dir'. Since
 * directories cannot have more than one link there is always
 * a unique solution. The path is writen to the given buffer.
 * On success 0 is returned. On error this function returns a
 * negative error code. See the man page for getcwd(3) for
 * possible errors. Even if an error code is returned the buffer
 * will be filled with a valid string which has some partial
 * information about the wanted path. */
ssize_t
lookup_dirpath(vnode_t *dir, char *buf, size_t osize)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_dirpath");

        return -ENOENT;
}
#endif /* __GETCWD__ */
