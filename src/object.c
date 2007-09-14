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

#include <compiz-core.h>

CompObjectType objectInfo[] = {
    {
	"core",
	allocCoreObjectPrivateIndex,
	freeCoreObjectPrivateIndex,
	forEachCoreObject,
	nameCoreObject,
	findCoreObject
    }, {
	"display",
	allocDisplayObjectPrivateIndex,
	freeDisplayObjectPrivateIndex,
	forEachDisplayObject,
	nameDisplayObject,
	findDisplayObject
    }, {
	"screen",
	allocScreenObjectPrivateIndex,
	freeScreenObjectPrivateIndex,
	forEachScreenObject,
	nameScreenObject,
	findScreenObject
    }, {
	"window",
	allocWindowObjectPrivateIndex,
	freeWindowObjectPrivateIndex,
	forEachWindowObject,
	nameWindowObject,
	findWindowObject
    }
};

void
compObjectInit (CompObject     *object,
		CompPrivate    *privates,
		CompObjectType *type,
		CompObjectTypeID id)
{
    object->id       = id;
    object->privates = privates;
    object->parent   = NULL;
    object->type     = type;
}

int
compObjectAllocatePrivateIndex (CompObject     *parent,
				CompObjectTypeID type)
{
    return (*objectInfo[type].allocPrivateIndex) (parent);
}

void
compObjectFreePrivateIndex (CompObject     *parent,
			    CompObjectTypeID type,
			    int	           index)
{
    (*objectInfo[type].freePrivateIndex) (parent, index);
}

CompBool
compObjectForEach (CompObject	      *parent,
		   CompObjectTypeID     type,
		   ObjectCallBackProc proc,
		   void		      *closure)
{
    return (*objectInfo[type].forEachObject) (parent, proc, closure);
}

CompBool
compObjectForEachType (CompObject	      *parent,
		       ObjectTypeCallBackProc proc,
		       void		      *closure)
{
    int i;

    for (i = 0; i < sizeof (objectInfo) / sizeof (objectInfo[0]); i++)
	if (!(*proc) (i, parent, closure))
	    return FALSE;

    return TRUE;
}

const char *
compObjectTypeName (CompObjectTypeID type)
{
    return objectInfo[type].name;
}

char *
compObjectName (CompObject *object)
{
    return (*objectInfo[object->id].nameObject) (object);
}

CompObject *
compObjectFind (CompObject     *parent,
		CompObjectTypeID type,
		const char     *name)
{
    return (*objectInfo[type].findObject) (parent, name);
}
