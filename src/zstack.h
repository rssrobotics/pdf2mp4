#ifndef _ZSTACK_H
#define _ZSTACK_H

#include "zarray.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef zarray_t zstack_t;

#define zstack_create zarray_create
#define zstack_destroy zarray_destroy
#define zstack_vmap zarray_vmap
#define zstack_map zarray_map
#define zstack_push(zs, o) zarray_add(zs, o)

static void zstack_pop(zstack_t *zs, void *p)
{
    int idx = zarray_size(zs) - 1;
    zarray_get(zs, idx, p);
    zarray_remove_index(zs, idx, 0); // 0: no shuffle
}

#define zstack_empty(zs) (zarray_size(zs)==0)
#define zstack_size(zs) zarray_size(zs)

#ifdef __cplusplus
}
#endif

#endif

