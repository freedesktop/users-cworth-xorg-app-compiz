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

#include <stdlib.h>
#include <string.h>

#include <compiz/branch.h>
#include <compiz/c-object.h>

static const CInterface branchInterface[] = {
    C_INTERFACE (branch, Type, CompObjectVTable, _, _, _, _, _, _, _, _)
};

typedef struct _ForEachTypeContext {
    const char       *interface;
    TypeCallBackProc proc;
    void	     *closure;
} ForEachTypeContext;

static CompBool
baseObjectForEachType (CompObject *object,
		       void       *closure)
{
    ForEachTypeContext *pCtx = (ForEachTypeContext *) closure;

    BRANCH (object);

    return (*b->u.vTable->forEachType) (b,
					pCtx->interface,
					pCtx->proc,
					pCtx->closure);
}

static CompBool
noopForEachType (CompBranch	  *b,
		 const char       *interface,
		 TypeCallBackProc proc,
		 void	          *closure)
{
    ForEachTypeContext ctx;

    ctx.interface = interface;
    ctx.proc      = proc;
    ctx.closure   = closure;

    return (*b->u.base.vTable->forBaseObject) (&b->u.base,
					       baseObjectForEachType,
					       (void *) &ctx);
}

static CompBool
forEachType (CompBranch	      *b,
	     const char       *interface,
	     TypeCallBackProc proc,
	     void	      *closure)
{
    return TRUE;
}

typedef struct _RegisterTypeContext {
    const char           *interface;
    const CompObjectType *type;
} RegisterTypeContext;

static CompBool
baseObjectRegisterType (CompObject *object,
			void       *closure)
{
    RegisterTypeContext *pCtx = (RegisterTypeContext *) closure;

    BRANCH (object);

    return (*b->u.vTable->registerType) (b, pCtx->interface, pCtx->type);
}

static CompBool
noopRegisterType (CompBranch	       *b,
		  const char           *interface,
		  const CompObjectType *type)
{
    RegisterTypeContext ctx;

    ctx.interface = interface;
    ctx.type      = type;

    return (*b->u.base.vTable->forBaseObject) (&b->u.base,
					       baseObjectRegisterType,
					       (void *) &ctx);
}

static const CompObjectInstantiator *
lookupObjectInstantiator (const CompObjectFactory *factory,
			  const char		  *name)
{
    int i;

    for (i = 0; i < factory->nInstantiator; i++)
	if  (strcmp (name, factory->instantiator[i].type->name))
	    return &factory->instantiator[i];

    if (factory->master)
	return lookupObjectInstantiator (factory->master, name);

    return NULL;
}

static CompBool
registerType (CompBranch	   *b,
	      const char	   *interface,
	      const CompObjectType *type)
{
    const CompObjectInstantiator *base;
    CompObjectInstantiator	 *instantiator;
    const CompObjectFactory	 *factory;
    const CompRootFactory	 *root;
    int				 i;

    base = lookupObjectInstantiator (&b->factory, type->baseName);
    if (!base)
	return FALSE;

    instantiator = realloc (b->factory.instantiator,
			    sizeof (CompObjectInstantiator) *
			    (b->factory.nInstantiator + 1));
    if (!instantiator)
	return FALSE;

    b->factory.instantiator = instantiator;

    instantiator[b->factory.nInstantiator].interface = strdup (interface);
    if (!instantiator[b->factory.nInstantiator].interface)
	return FALSE;

    instantiator[b->factory.nInstantiator].base		   = base;
    instantiator[b->factory.nInstantiator].type		   = type;
    instantiator[b->factory.nInstantiator].size.len	   = 0;
    instantiator[b->factory.nInstantiator].size.sizes      = 0;
    instantiator[b->factory.nInstantiator].size.totalSize  = 0;
    instantiator[b->factory.nInstantiator].privates.funcs  = 0;
    instantiator[b->factory.nInstantiator].privates.nFuncs = 0;

    for (factory = &b->factory; factory->master; factory = factory->master);
    root = (const CompRootFactory *) factory;

    for (i = 0; i < root->nEntry; i++)
    {
	if (strcmp (root->entry[i].name, type->name) == 0)
	{
	    instantiator[b->factory.nInstantiator].size = root->entry[i].size;
	    break;
	}
    }

    b->factory.nInstantiator++;

    return TRUE;
}

static CompBranchVTable branchObjectVTable = {
    .forEachType  = forEachType,
    .registerType = registerType
};

static CompBool
branchInitObject (const CompObjectFactory *factory,
		  CompObject		  *object)
{
    BRANCH (object);

    if (!cObjectInterfaceInit (factory, object, &branchObjectVTable.base))
	return FALSE;

    b->factory.master        = factory;
    b->factory.instantiator  = NULL;
    b->factory.nInstantiator = 0;

    return TRUE;
}

static void
branchFiniObject (const CompObjectFactory *factory,
		  CompObject		  *object)
{
    cObjectInterfaceFini (factory, object);
}

static void
branchInitVTable (CompBranchVTable *vTable)
{
    (*getObjectType ()->initVTable) (&vTable->base);

    ENSURE (vTable, forEachType,  noopForEachType);
    ENSURE (vTable, registerType, noopRegisterType);
}

static CompObjectType branchObjectType = {
    BRANCH_TYPE_NAME, OBJECT_TYPE_NAME,
    {
	branchInitObject,
	branchFiniObject
    },
    0,
    (InitVTableProc) branchInitVTable
};

static void
branchGetCContext (CompObject *object,
		   CContext   *ctx)
{
    BRANCH (object);

    ctx->interface  = branchInterface;
    ctx->nInterface = N_ELEMENTS (branchInterface);
    ctx->type	    = &branchObjectType;
    ctx->data	    = (char *) b;
    ctx->svOffset   = 0;
    ctx->vtStore    = &b->object;
    ctx->version    = COMPIZ_BRANCH_VERSION;
}

CompObjectType *
getBranchObjectType (void)
{
    static CompBool init = FALSE;

    if (!init)
    {
	cInitObjectVTable (&branchObjectVTable.base, branchGetCContext,
			   branchObjectType.initVTable);
	init = TRUE;
    }

    return &branchObjectType;
}
