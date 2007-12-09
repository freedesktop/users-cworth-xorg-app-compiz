/*
 * Copyright © 2007 Novell, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#ifndef _COMPIZ_BRANCH_H
#define _COMPIZ_BRANCH_H

#include <compiz/object.h>

#define COMPIZ_BRANCH_VERSION 20071116

COMPIZ_BEGIN_DECLS

typedef struct _CompBranch CompBranch;

typedef CompBool (*TypeCallBackProc) (CompBranch	   *branch,
				      const CompObjectType *type,
				      void		   *closure);

typedef CompBool (*ForEachTypeProc) (CompBranch	      *branch,
				     const char	      *interface,
				     TypeCallBackProc proc,
				     void	      *closure);

typedef CompBool (*RegisterTypeProc) (CompBranch	   *branch,
				      const char	   *interface,
				      const CompObjectType *type);

typedef struct _CompBranchVTable {
    CompObjectVTable base;

    /* type functions

       object types are provided by implementing forEachType
     */
    ForEachTypeProc  forEachType;
    RegisterTypeProc registerType;
} CompBranchVTable;

typedef struct _CompPrivatesSizeEntry {
    const char		   *name;
    CompObjectPrivatesSize size;
} CompPrivatesSizeEntry;

typedef struct _CompRootFactory {
    CompObjectFactory base;

    CompPrivatesSizeEntry *entry;
    int			  nEntry;
} CompRootFactory;

struct _CompBranch {
    union {
	CompObject	       base;
	const CompBranchVTable *vTable;
    } u;

    CompObjectVTableVec object;

    CompObjectFactory factory;
};

#define GET_BRANCH(object) ((CompBranch *) (object))
#define BRANCH(object) CompBranch *b = GET_BRANCH (object)

#define BRANCH_TYPE_NAME "branch"

CompObjectType *
getBranchObjectType (void);

COMPIZ_END_DECLS

#endif
