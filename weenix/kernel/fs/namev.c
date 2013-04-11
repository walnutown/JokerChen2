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
        dbg(DBG_VFS,"VFS: Enter lookup(), look for %s\n", name);
        KASSERT(NULL != dir);
        KASSERT(NULL != name);
        KASSERT(NULL != result);
        if(dir->vn_ops->lookup==NULL)
        {
            dbg(DBG_VFS,"VFS: Leave lookup(), return error ENOTDIR\n");
            return -ENOTDIR;
        }
        else if(len>STR_MAX)
        {
            dbg(DBG_VFS,"VFS: Leave lookup(), return error ENAMETOOLONG\n");
            return -ENAMETOOLONG;
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
            dbg(DBG_VFS,"VFS: Leave lookup(), find %s\n", name);
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
        dbg(DBG_VFS,"##########VFS: Enter dir_namev(), look for path %s\n", pathname);
        KASSERT(NULL != pathname);
        KASSERT(NULL != namelen);
        KASSERT(NULL != name);
        KASSERT(NULL != res_vnode);
        int err=0;
        vnode_t* basic;
        basic=base==NULL?curproc->p_cwd:base;
        basic=pathname[0]=='/'?vfs_root_vn:basic;
        int last=pathname[0]=='/'?1:0;
        int i = last;
        do
        {
            while(pathname[i++]!='/')
            {
                if(pathname[i]=='\0')
                {
                    if(i-last>STR_MAX)
                    {
                        vput(basic);
                        dbg(DBG_VFS,"##########VFS: Leave dir_namev(), return error ENAMETOOLONG\n");
                        return -ENAMETOOLONG;
                    }
                    *res_vnode=basic;
                    *namelen=i-last;
                    *name=&pathname[last];
                    dbg(DBG_VFS,"##########VFS: Leave dir_namev(), find path %s\n", *name);
                    return 0;
                }
            }

            if(pathname[i]!='\0')
            {
                if(i-last-1>STR_MAX) {
                    dbg(DBG_VFS,"##########VFS: Leave dir_namev(), return error ENAMETOOLONG\n");
                    return -ENAMETOOLONG;
                }
                if((err=lookup(basic,pathname + last,i-last-1,res_vnode)))
                {
                    vput(basic);
                    dbg(DBG_VFS,"##########VFS: Leave dir_namev(), return lookup error, can't find path %s\n", pathname + last);
                    return err;
                }
                vput(basic);
                dbg(DBG_VFS,"##########VFS: Parent path:%s\n",pathname+last);
                dbg(DBG_VFS,"##########VFS: Child name:%s\n",pathname+i);
                last=i;
                basic=*res_vnode;
            }
            else/*handle*/
            {
                *res_vnode=basic;
                namelen=0;
                name=NULL;
                dbg(DBG_VFS,"##########VFS: Leave dir_namev(), find /\n");
                return 0;
            }
        }while(1);
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
        dbg(DBG_VFS,"VFS: Enter open_namev()\n");

        size_t len;
        const char *name;
        int err=0;
        vnode_t *par;
        err=dir_namev(pathname,&len,&name,base,&par);
        if(!err)
        {
            err=lookup(par,name,len,res_vnode);
            if(!err)
            {
                vput(par);
                dbg(DBG_VFS,"VFS: Leave open_namev()\n");
                return 0;
            }
            else if((err==-ENOENT)&&(O_CREAT&flag)&&(par->vn_ops->create!=NULL))
            {
                KASSERT(NULL != par->vn_ops->create(par,name,len,res_vnode));  
                vput(par);
                dbg(DBG_VFS,"VFS: Leave open_namev()\n");
                return 0;
            }
            else if(err!=0)
            {
                vput(par);

                dbg(DBG_VFS,"VFS: Leave open_namev(), return error\n");

                return err;
            }
        }
        dbg(DBG_VFS,"VFS: Leave open_namev()\n");
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
