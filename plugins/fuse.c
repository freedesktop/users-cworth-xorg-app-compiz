/*
 * Copyright © 2007 David Reveman
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * David Reveman not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * David Reveman makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * DAVID REVEMAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DAVID REVEMAN BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <sys/mount.h>
#include <fuse.h>
#include <fuse_lowlevel.h>

#include <compiz-core.h>

static int corePrivateIndex;

typedef struct _FuseCore {
    CompObjectVTableVec object;

    struct fuse_session *session;
    struct fuse_chan    *channel;
    char		*mountPoint;
    CompWatchFdHandle   watchFdHandle;
    char		*buffer;
} FuseCore;

static int objectPrivateIndex;

typedef struct _FuseProp {
    struct _FuseProp *next;

    fuse_ino_t ino;
    int	       type;
    char       *interface;
    char       *member;
    char       *name;
} FuseProp;

typedef struct _FuseObject {
    fuse_ino_t ino;
    FuseProp   *prop;
} FuseObject;

typedef struct _FuseDirEntry {
    struct _FuseDirEntry *next;

    fuse_ino_t ino;
    char       *name;
} FuseDirEntry;

typedef struct _FuseFile {
    char *data;
    int  size;
} FuseFile;

#define GET_FUSE_CORE(c)			       \
    ((FuseCore *) (c)->privates[corePrivateIndex].ptr)

#define FUSE_CORE(c)		     \
    FuseCore *fc = GET_FUSE_CORE (c)

#define GET_FUSE_OBJECT(o)				   \
    ((FuseObject *) (o)->privates[objectPrivateIndex].ptr)

#define FUSE_OBJECT(o)			 \
    FuseObject *fo = GET_FUSE_OBJECT (o)


static fuse_ino_t currentIno = FUSE_ROOT_ID;

typedef void (*ForPropStringProc) (const char *str,
				   void	      *closure);

static void
fuseForPropString (CompObject	     *object,
		   FuseProp	     *prop,
		   ForPropStringProc proc,
		   void		     *closure)
{
    CompAnyValue  value[3];
    CompBasicArgs args;
    char	  tmp[256];

    compInitBasicArgs (&args, &value[0], &value[2]);

    value[0].s = prop->interface;
    value[1].s = prop->member;

    switch (prop->type) {
    case COMP_TYPE_BOOLEAN:
	if (compInvokeMethod (object,
			      "properties", "getBool", "ss", "b",
			      &args.base))
	{
	    (*proc) (value[2].b ? "true" : "false", closure);
	}
	break;
    case COMP_TYPE_INT32:
	if (compInvokeMethod (object,
			      "properties", "getInt", "ss", "i",
			      &args.base))
	{
	    snprintf (tmp, 256, "%d", value[2].i);
	    (*proc) (tmp, closure);
	}
	break;
    case COMP_TYPE_DOUBLE:
	if (compInvokeMethod (object,
			      "properties", "getDouble", "ss", "d",
			      &args.base))
	{
	    snprintf (tmp, 256, "%f", value[2].d);
	    (*proc) (tmp, closure);
	}
	break;
    case COMP_TYPE_STRING:
	if (compInvokeMethod (object,
			      "properties", "getString", "ss", "s",
			      &args.base))
	{
	    (*proc) (value[2].s, closure);
	    free (value[2].s);
	}
	break;
    }

    compFiniBasicArgs (&args);
}

static void
fuseStringToProp (CompObject *object,
		  FuseProp   *prop,
		  const char *str)
{
    CompAnyValue  value[3];
    CompBasicArgs args;

    compInitBasicArgs (&args, &value[0], NULL);

    value[0].s = prop->interface;
    value[1].s = prop->member;

    switch (prop->type) {
    case COMP_TYPE_BOOLEAN:
	value[2].b = *str == 't' ? TRUE : FALSE;
	compInvokeMethod (object,
			  "properties", "setBool", "ssb", "",
			  &args.base);
	break;
    case COMP_TYPE_INT32:
	value[2].i = strtol (str, NULL, 0);
	compInvokeMethod (object,
			  "properties", "setInt", "ssi", "",
			  &args.base);
	break;
    case COMP_TYPE_DOUBLE:
	value[2].d = strtod (str, NULL);
	compInvokeMethod (object,
			  "properties", "setDouble", "ssd", "",
			  &args.base);
	break;
    case COMP_TYPE_STRING:
	value[2].s = strdup (str);
	if (value[2].s)
	{
	    compInvokeMethod (object,
			      "properties", "setString", "sss", "",
			      &args.base);
	    free (value[2].s);
	}
	break;
    }

    compFiniBasicArgs (&args);
}

static void
propStringLength (const char *str,
		  void	     *closure)
{
    *((int *) closure) = strlen (str);
}

static void
copyPropString (const char *str,
		void	   *closure)
{
    *((char **) closure) = strdup (str);
}

typedef struct _LookupInodeContext {
    fuse_ino_t ino;
    FuseProp   **prop;
    CompObject *object;
} LookupInodeContext;

static CompBool
checkObject (CompObject *object,
	     void	*closure)
{
    LookupInodeContext *pCtx = (LookupInodeContext *) closure;

    FUSE_OBJECT (object);

    if (fo->ino == pCtx->ino)
    {
	pCtx->object = object;
	if (pCtx->prop)
	    *pCtx->prop = NULL;

	return FALSE;
    }
    else if (pCtx->prop)
    {
	FuseProp *prop;

	prop = fo->prop;
	while (prop)
	{
	    if (prop->ino == pCtx->ino)
	    {
		pCtx->object = object;
		*pCtx->prop = prop;
		return FALSE;
	    }

	    prop = prop->next;
	}
    }

    return (*object->vTable->forEachChildObject) (object,
						  checkObject,
						  closure);
}

static CompObject *
fuseLookupInode (CompObject *root,
		 fuse_ino_t ino,
		 FuseProp   **prop)
{
    LookupInodeContext ctx;

    ctx.ino  = ino;
    ctx.prop = prop;

    if (checkObject (root, (void *) &ctx))
	return NULL;

    return ctx.object;
}

typedef struct _ForEachPropContext {
    const char *interface;
    int	       length;
    CompBool   isPartOfType;
} ForEachPropContext;

static CompBool
fuseForEachProp (CompObject *object,
		 const char *name,
		 int	    type,
		 void	    *closure)
{
    ForEachPropContext *pCtx = (ForEachPropContext *) closure;
    FuseProp	       *prop;
    int		       size;

    FUSE_OBJECT (object);

    prop = fo->prop;
    while (prop)
    {
	if (strcmp (prop->interface, pCtx->interface) == 0 &&
	    strcmp (prop->member,    name)            == 0)
	    return TRUE;

	prop = prop->next;
    }

    size = sizeof (FuseProp) + pCtx->length * 2 + strlen (name) + 3;

    prop = malloc (size);
    if (!prop)
	return TRUE;

    prop->next = fo->prop;
    fo->prop = prop;

    prop->type	    = type;
    prop->interface = (char *) (prop + 1);
    prop->name      = prop->interface + pCtx->length + 1;
    prop->member    = prop->name + pCtx->length + 1;

    strcpy (prop->interface, pCtx->interface);
    sprintf (prop->name, "%s_%s", pCtx->interface, name);
    prop->member = prop->name + pCtx->length + 1;

    if (pCtx->isPartOfType)
	prop->name = prop->member;

    prop->ino = currentIno++;

    return TRUE;
}

static CompBool
fuseForEachInterfaceProp (CompObject	       *object,
			  const char	       *name,
			  void		       *interface,
			  size_t	       offset,
			  const CompObjectType *type,
			  void		       *closure)
{
    ForEachPropContext ctx;

    ctx.interface    = name;
    ctx.length       = strlen (name);
    ctx.isPartOfType = FALSE;

    if (type && strcmp (type->name, name) == 0)
	ctx.isPartOfType = TRUE;

    (*object->vTable->forEachProp) (object, interface,
				    fuseForEachProp,
				    (void *) &ctx);

    return TRUE;
}

static CompBool
incHardLinks (CompObject *object,
	      void       *closure)
{
    struct stat *stbuf = (struct stat *) closure;

    stbuf->st_nlink++;

    return TRUE;
}

typedef struct _StatContext {
    const char  *name;
    struct stat *stbuf;
} StatContext;

static CompBool
statObject (CompObject *object,
	    void       *closure)
{
    StatContext *pCtx = (StatContext *) closure;

    if (strcmp (object->name, pCtx->name) == 0)
    {
	FUSE_OBJECT (object);

	pCtx->stbuf->st_ino   = fo->ino;
	pCtx->stbuf->st_mode  = S_IFDIR | 0555;
	pCtx->stbuf->st_nlink = 2;

	(*object->vTable->forEachChildObject) (object,
					       incHardLinks,
					       (void *) pCtx->stbuf);

	return FALSE;
    }

    return TRUE;
}

static CompBool
fuseStatChild (CompObject  *object,
	       const char  *name,
	       struct stat *stbuf)
{
    StatContext ctx;

    ctx.name  = name;
    ctx.stbuf = stbuf;

    if ((*object->vTable->forEachChildObject) (object,
					       statObject,
					       (void *) &ctx))
    {
	FuseProp *prop;

	FUSE_OBJECT (object);

	(*object->vTable->forEachInterface) (object,
					     fuseForEachInterfaceProp,
					     NULL);

	prop = fo->prop;
	while (prop)
	{
	    if (strcmp (name, prop->name) == 0)
	    {
		int length = 0;

		fuseForPropString (object, prop,
				   propStringLength,
				   (void *) &length);

		stbuf->st_ino   = prop->ino;
		stbuf->st_mode  = S_IFREG | 0666;
		stbuf->st_nlink = 1;
		stbuf->st_size  = length;

		return TRUE;
	    }

	    prop = prop->next;
	}

	return FALSE;
    }

    return TRUE;
}

static void
fuseAddDirEntry (FuseDirEntry **entry,
		 const char   *name,
		 fuse_ino_t   ino)
{
    FuseDirEntry *e;

    e = malloc (sizeof (FuseDirEntry) + strlen (name) + 1);
    if (e)
    {
	e->next = NULL;
	e->ino  = ino;
	e->name = (char *) (e + 1);

	strcpy (e->name, name);

	while (*entry)
	    entry = &(*entry)->next;

	*entry = e;
    }
}

static void
fuseAddDirEntryObject (FuseDirEntry **entry,
		       const char   *name,
		       CompObject   *object)
{
    fuseAddDirEntry (entry, name, GET_FUSE_OBJECT (object)->ino);
}

static CompBool
fuseForEachChildObject (CompObject *object,
			void	   *closure)
{
    FuseDirEntry **entry = (FuseDirEntry **) closure;

    fuseAddDirEntryObject (entry, object->name, object);

    return TRUE;
}

static void
compiz_getattr (fuse_req_t	      req,
		fuse_ino_t	      ino,
		struct fuse_file_info *fi)
{
    CompCore   *c = (CompCore *) fuse_req_userdata (req);
    CompObject *object;
    FuseProp   *prop;

    object = fuseLookupInode (&c->u.base, ino, &prop);
    if (object)
    {
	struct stat stbuf;

	memset (&stbuf, 0, sizeof (stbuf));

	if (prop)
	{
	    int length = 0;

	    fuseForPropString (object, prop,
			       propStringLength,
			       (void *) &length);

	    stbuf.st_mode  = S_IFREG | 0666;
	    stbuf.st_nlink = 1;
	    stbuf.st_size  = length;
	}
	else
	{
	    stbuf.st_mode  = S_IFDIR | 0555;
	    stbuf.st_nlink = 2;

	    (*object->vTable->forEachChildObject) (object,
						   incHardLinks,
						   (void *) &stbuf);
	}

	fuse_reply_attr (req, &stbuf, 1.0);
    }
    else
    {
	fuse_reply_err (req, ENOENT);
    }
}

static void
compiz_setattr (fuse_req_t	      req,
		fuse_ino_t	      ino,
		struct stat	      *attr,
		int		      to_set,
		struct fuse_file_info *fi)
{
    CompCore   *c = (CompCore *) fuse_req_userdata (req);
    CompObject *object;
    FuseProp   *prop;

    object = fuseLookupInode (&c->u.base, ino, &prop);
    if (object && prop)
    {
	struct stat stbuf;
	int	    length = 0;

	if ((to_set & FUSE_SET_ATTR_SIZE) != FUSE_SET_ATTR_SIZE)
	{
	    fuse_reply_err (req, EACCES);
	    return;
	}

	if (attr->st_size != 0)
	{
	    fuse_reply_err (req, EACCES);
	    return;
	}

	memset (&stbuf, 0, sizeof (stbuf));

	fuseForPropString (object, prop, propStringLength, (void *) &length);

	stbuf.st_mode  = S_IFREG | 0666;
	stbuf.st_nlink = 1;
	stbuf.st_size  = length;

	fuse_reply_attr (req, &stbuf, 1.0);
    }
    else
    {
	fuse_reply_err (req, ENOENT);
    }
}

static void
compiz_lookup (fuse_req_t req,
	       fuse_ino_t parent,
	       const char *name)
{
    CompCore		    *c = (CompCore *) fuse_req_userdata (req);
    CompObject		    *object;
    struct fuse_entry_param e;
    struct stat		    stbuf;

    object = fuseLookupInode (&c->u.base, parent, NULL);
    if (!object)
    {
	fuse_reply_err (req, ENOENT);
	return;
    }

    memset (&stbuf, 0, sizeof (stbuf));

    if (!fuseStatChild (object, name, &stbuf))
    {
	fuse_reply_err (req, ENOENT);
	return;
    }

    memset (&e, 0, sizeof (e));

    e.ino	    = stbuf.st_ino;
    e.generation    = 1;
    e.attr_timeout  = 1.0;
    e.entry_timeout = 1.0;
    e.attr	    = stbuf;

    fuse_reply_entry (req, &e);
}

struct dirbuf {
    char   *p;
    size_t size;
};

static void
dirbuf_add (fuse_req_t	  req,
	    struct dirbuf *b,
	    FuseDirEntry  *e)
{
    struct stat stbuf;
    size_t	oldSize = b->size;

    b->size += fuse_add_direntry (req, NULL, 0, e->name, NULL, 0);
    b->p     = (char *) realloc (b->p, b->size);

    memset (&stbuf, 0, sizeof (stbuf));

    stbuf.st_ino = e->ino;

    fuse_add_direntry (req, b->p + oldSize, b->size - oldSize, e->name,
		       &stbuf, b->size);
}

static int
reply_buf_limited (fuse_req_t req,
		   const char *buf,
		   size_t     bufsize,
		   off_t      off,
		   size_t     maxsize)
{
    if (off < bufsize)
	return fuse_reply_buf (req, buf + off, MIN (bufsize - off, maxsize));
    else
	return fuse_reply_buf (req, NULL, 0);
}

static void
compiz_opendir (fuse_req_t	      req,
		fuse_ino_t	      ino,
		struct fuse_file_info *fi)
{
    CompCore   *c = (CompCore *) fuse_req_userdata (req);
    CompObject *object;

    object = fuseLookupInode (&c->u.base, ino, NULL);
    if (object)
    {
	FuseProp     *prop;
	FuseDirEntry *entry = NULL;

	FUSE_OBJECT (object);

	fuseAddDirEntryObject (&entry, ".", object);
	if (object->parent)
	    fuseAddDirEntryObject (&entry, "..", object->parent);

	(*object->vTable->forEachChildObject) (object,
					       fuseForEachChildObject,
					       (void *) &entry);

	(*object->vTable->forEachInterface) (object,
					     fuseForEachInterfaceProp,
					     NULL);

	prop = fo->prop;
	while (prop)
	{
	    fuseAddDirEntry (&entry, prop->name, prop->ino);
	    prop = prop->next;
	}

	fi->fh = (unsigned long) entry;

	fuse_reply_open (req, fi);
    }
    else
    {
	fuse_reply_err (req, ENOTDIR);
    }
}

static void
compiz_readdir (fuse_req_t	      req,
		fuse_ino_t	      ino,
		size_t		      size,
		off_t		      off,
		struct fuse_file_info *fi)
{
    FuseDirEntry  *entry = (FuseDirEntry *) (uintptr_t) fi->fh;
    struct dirbuf b;

    b.p    = NULL;
    b.size = 0;

    while (entry)
    {
	dirbuf_add (req, &b, entry);
	entry = entry->next;
    }

    reply_buf_limited (req, b.p, b.size, off, size);

    free (b.p);
}

static void
compiz_releasedir (fuse_req_t		 req,
		   fuse_ino_t		 ino,
		   struct fuse_file_info *fi)
{
    FuseDirEntry *next, *entry = (FuseDirEntry *) (uintptr_t) fi->fh;

    while (entry)
    {
	next = entry->next;
	free (entry);
	entry = next;
    }

    fuse_reply_err (req, 0);
}

static void
compiz_open (fuse_req_t		   req,
	     fuse_ino_t		   ino,
	     struct fuse_file_info *fi)
{
    CompCore   *c = (CompCore *) fuse_req_userdata (req);
    CompObject *object;
    FuseProp   *prop;
    FuseFile   *file;
    char       *data = NULL;

    object = fuseLookupInode (&c->u.base, ino, &prop);
    if (!object)
    {
	fuse_reply_err (req, ENOENT);
	return;
    }

    if (!prop)
    {
	fuse_reply_err (req, EISDIR);
	return;
    }

    if ((fi->flags & 3) != O_RDONLY && (fi->flags & O_TRUNC))
	data = strdup ("");
    else
	fuseForPropString (object, prop, copyPropString, (void *) &data);

    if (!data)
    {
	fuse_reply_err (req, ENOBUFS);
	return;
    }

    file = malloc (sizeof (FuseFile));
    if (!file)
    {
	free (data);
	fuse_reply_err (req, ENOBUFS);
	return;
    }

    file->size  = strlen (data);
    file->data  = data;

    fi->fh = (unsigned long) file;

    fuse_reply_open (req, fi);
}

static void
compiz_read (fuse_req_t		   req,
	     fuse_ino_t		   ino,
	     size_t		   size,
	     off_t		   off,
	     struct fuse_file_info *fi)
{
    FuseFile *file = (FuseFile *) (uintptr_t) fi->fh;

    reply_buf_limited (req, file->data, file->size, off, size);
}

static void
compiz_write (fuse_req_t	    req,
	      fuse_ino_t	    ino,
	      const char	    *buf,
	      size_t		    size,
	      off_t		    off,
	      struct fuse_file_info *fi)
{
    FuseFile *file = (FuseFile *) (uintptr_t) fi->fh;

    if (off + size > file->size)
    {
	char *data;

	data = realloc (file->data, off + size + 1);
	if (!data)
	{
	    fuse_reply_err (req, ENOBUFS);
	    return;
	}

	data[off + size] = '\0';

	file->data = data;
	file->size = off + size;
    }

    memcpy (file->data + off, buf, size);

    fuse_reply_write (req, size);
}

static void
compiz_release (fuse_req_t	      req,
		fuse_ino_t	      ino,
		struct fuse_file_info *fi)
{
    CompCore   *c = (CompCore *) fuse_req_userdata (req);
    FuseFile   *file = (FuseFile *) (uintptr_t) fi->fh;
    CompObject *object;
    FuseProp   *prop;

    object = fuseLookupInode (&c->u.base, ino, &prop);
    if (object && prop)
	fuseStringToProp (object, prop, file->data);

    free (file->data);
    free (file);

    fuse_reply_err (req, 0);
}

static void
compiz_fsync (fuse_req_t	    req,
	      fuse_ino_t	    ino,
	      int		    datasync,
	      struct fuse_file_info *fi)
{
    CompCore   *c = (CompCore *) fuse_req_userdata (req);
    FuseFile   *file = (FuseFile *) (uintptr_t) fi->fh;
    CompObject *object;
    FuseProp   *prop;

    object = fuseLookupInode (&c->u.base, ino, &prop);
    if (object && prop)
	fuseStringToProp (object, prop, file->data);

    fuse_reply_err (req, 0);
}

static struct fuse_lowlevel_ops compiz_ll_oper = {
    .lookup     = compiz_lookup,
    .getattr    = compiz_getattr,
    .setattr    = compiz_setattr,
    .opendir    = compiz_opendir,
    .readdir    = compiz_readdir,
    .releasedir = compiz_releasedir,
    .open       = compiz_open,
    .read       = compiz_read,
    .write      = compiz_write,
    .release    = compiz_release,
    .fsync      = compiz_fsync
};

static CompBool
fuseInitObject (CompObject *object)
{
    FUSE_OBJECT (object);

    fo->ino  = currentIno++;
    fo->prop = NULL;

    return TRUE;
}

static void
fuseFiniObject (CompObject *object)
{
    FUSE_OBJECT (object);

    while (fo->prop)
    {
	FuseProp *prop = fo->prop;

	fo->prop = prop->next;
	free (prop);
    }
}

static Bool
fuseProcessMessages (void *data)
{
    CompCore	     *c = (CompCore *) data;
    struct fuse_chan *channel;
    size_t	     bufferSize;
    int		     res = 0;

    FUSE_CORE (c);

    channel    = fuse_session_next_chan (fc->session, NULL);
    bufferSize = fuse_chan_bufsize (channel);

    if (fuse_session_exited (fc->session))
	return FALSE;

    for (;;)
    {
	struct fuse_chan *tmpch = channel;

	res = fuse_chan_recv (&tmpch, fc->buffer, bufferSize);
	if (res == -EINTR)
	    continue;

	if (res > 0)
	    fuse_session_process (fc->session, fc->buffer, res, tmpch);

	break;
    }

    return TRUE;
}

static void
fuseMount (CompCore *c)
{
    struct fuse_args args = FUSE_ARGS_INIT (0, NULL);

    FUSE_CORE (c);

    fuse_opt_add_arg (&args, "");
    fuse_opt_add_arg (&args, "-o");
    fuse_opt_add_arg (&args, "allow_root");

    if (!fc->mountPoint)
	return;

    fc->channel = fuse_mount (fc->mountPoint, &args);
    if (!fc->channel)
    {
	fuse_opt_free_args (&args);
	return;
    }

    fuse_opt_free_args (&args);

    fc->buffer = malloc (fuse_chan_bufsize (fc->channel));
    if (!fc->buffer)
    {
	fuse_unmount (fc->mountPoint, fc->channel);
	fc->channel = NULL;
	return;
    }

    fuse_session_add_chan (fc->session, fc->channel);

    fc->watchFdHandle = compAddWatchFd (fuse_chan_fd (fc->channel),
					POLLIN | POLLPRI | POLLHUP | POLLERR,
					fuseProcessMessages,
					c);
}

static void
fuseUnmount (CompCore *c)
{
    FUSE_CORE (c);

    if (fc->watchFdHandle)
    {
	compRemoveWatchFd (fc->watchFdHandle);
	fc->watchFdHandle = 0;
    }

    if (fc->channel)
    {
	/* unmount will destroy the channel */
	fuse_unmount (fc->mountPoint, fc->channel);
	fc->channel = NULL;
    }

    if (fc->buffer)
    {
	free (fc->buffer);
	fc->buffer = NULL;
    }
}

static CompBool
setMountPoint (CompObject *object,
	       const char *interface,
	       const char *name,
	       const char *value,
	       char	  **error)
{
    char *mountPoint;

    CORE (object);
    FUSE_CORE (c);

    if (strcmp (value, fc->mountPoint) == 0)
	return TRUE;

    mountPoint = strdup (value);
    if (!mountPoint)
    {
	if (error)
	    *error = strdup ("Failed to copy mount point value");

	return FALSE;
    }

    fuseUnmount (c);

    free (fc->mountPoint);
    fc->mountPoint = mountPoint;

    fuseMount (c);

    return TRUE;
}

static char *
getCoreData (CompObject *object)
{
    return (char *) GET_FUSE_CORE (GET_CORE (object));
}

static CommonStringProp fuseCoreStringProp[] = {
    C_PROP (mountPoint, FuseCore, .set = setMountPoint)
};
#define INTERFACE_VERSION_fuseCore 20071011

static CommonInterface fuseCoreInterface[] = {
    C_INTERFACE (fuse, Core, CompObjectVTable, _, X, _, _, _, _, _, X, _)
};

static CompBool
fuseCoreForBaseObject (CompObject	      *object,
		       BaseObjectCallBackProc proc,
		       void		      *closure)
{
    CompObjectVTableVec v = { object->vTable };
    CompBool		status;

    FUSE_CORE (GET_CORE (object));

    UNWRAP (&fc->object, object, vTable);
    status = (*proc) (object, closure);
    WRAP (&fc->object, object, vTable, v.vTable);

    return status;
}

static CompBool
fuseCoreForEachInterface (CompObject		*object,
			  InterfaceCallBackProc proc,
			  void		        *closure)
{
    return handleForEachInterface (object,
				   fuseCoreInterface,
				   N_ELEMENTS (fuseCoreInterface),
				   NULL, proc, closure);
}

static CompCoreVTable fuseCoreObjectVTable = {
    .base.forBaseObject        = fuseCoreForBaseObject,
    .base.forEachInterface     = fuseCoreForEachInterface,
    .base.forEachProp          = commonForEachProp,
    .base.version.get          = commonGetVersion,
    .base.properties.getString = commonGetStringProp,
    .base.properties.setString = commonSetStringProp,
    .base.metadata.get	       = commonGetMetadata
};

static CompBool
fuseInitCore (CompCore *c)
{
    struct sigaction sa;

    FUSE_CORE (c);

    if (!compObjectCheckVersion (&c->u.base, "object", CORE_ABIVERSION))
	return FALSE;

    if (!commonObjectInterfaceInit (&c->u.base,
				    fuseCoreInterface,
				    N_ELEMENTS (fuseCoreInterface)))
	return FALSE;

    memset (&sa, 0, sizeof (struct sigaction));

    sa.sa_handler = SIG_IGN;
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction (SIGPIPE, &sa, NULL) == -1)
    {
	commonObjectInterfaceFini (&c->u.base,
				   fuseCoreInterface,
				   N_ELEMENTS (fuseCoreInterface));
	return FALSE;
    }

    fc->session = fuse_lowlevel_new (NULL,
				     &compiz_ll_oper, sizeof (compiz_ll_oper),
				     (void *) c);
    if (!fc->session)
    {
	commonObjectInterfaceFini (&c->u.base,
				   fuseCoreInterface,
				   N_ELEMENTS (fuseCoreInterface));
	return FALSE;
    }

    WRAP (&fc->object, &c->u.base, vTable, &fuseCoreObjectVTable.base);

    fc->watchFdHandle = 0;
    fc->channel	      = NULL;
    fc->buffer	      = NULL;

    commonInterfacesAdded (&c->u.base,
			   fuseCoreInterface,
			   N_ELEMENTS (fuseCoreInterface));

    fuseMount (c);

    return TRUE;
}

static void
fuseFiniCore (CompCore *c)
{
    FUSE_CORE (c);

    fuseUnmount (c);

    UNWRAP (&fc->object, &c->u.base, vTable);

    fuse_session_destroy (fc->session);

    commonInterfacesRemoved (&c->u.base,
			     fuseCoreInterface,
			     N_ELEMENTS (fuseCoreInterface));
    commonObjectInterfaceFini (&c->u.base,
			       fuseCoreInterface,
			       N_ELEMENTS (fuseCoreInterface));
}

static CompObjectPrivate fuseObj[] = {
    {
	"object",
	&objectPrivateIndex, sizeof (FuseObject), NULL,
	(InitObjectProc) fuseInitObject,
	(FiniObjectProc) fuseFiniObject
    }, {
	"core",
	&corePrivateIndex, sizeof (FuseCore), &fuseCoreObjectVTable,
	(InitObjectProc) fuseInitCore,
	(FiniObjectProc) fuseFiniCore
    }
};

static Bool
fuseInit (CompPlugin *p)
{
    if (!commonInterfaceInit (fuseCoreInterface,
			      N_ELEMENTS (fuseCoreInterface)))
	return FALSE;

    if (!compObjectInitPrivates (fuseObj, N_ELEMENTS (fuseObj)))
    {
	commonInterfaceFini (fuseCoreInterface,
			     N_ELEMENTS (fuseCoreInterface));
	return FALSE;
    }

    return TRUE;
}

static void
fuseFini (CompPlugin *p)
{
    compObjectFiniPrivates (fuseObj, N_ELEMENTS (fuseObj));
    commonInterfaceFini (fuseCoreInterface,
			 N_ELEMENTS (fuseCoreInterface));
}

CompPluginVTable fuseVTable = {
    "fs",
    0, /* GetMetadata */
    fuseInit,
    fuseFini,
    0, /* InitObject */
    0, /* FiniObject */
    0, /* GetObjectOptions */
    0  /* SetObjectOption */
};

CompPluginVTable *
getCompPluginInfo20070830 (void)
{
    return &fuseVTable;
}
