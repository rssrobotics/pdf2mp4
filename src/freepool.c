#include <stdlib.h>

#include "freepool.h"
#include "zarray.h"

struct freepool_entry
{
    void *p;
    void (*destroy)(void *p);
};

struct freepool
{
    zarray_t *za; // struct freepool_entry
};

freepool_t *freepool_create()
{
    freepool_t *fp = calloc(1, sizeof(freepool_t));
    fp->za = zarray_create(sizeof(struct freepool_entry));

    return fp;
}

void *freepool_add(freepool_t *fp, void *p, void (*destroy)())
{
    struct freepool_entry e;
    e.p       = p;
    e.destroy = destroy;

    zarray_add(fp->za, &e);

    return p;
}

void freepool_destroy(freepool_t *fp)
{
    struct freepool_entry e;

    for (int i = 0; i < zarray_size(fp->za); i++) {
        zarray_get(fp->za, i, &e);

        // checking for NULL allows user to wrap function calls with
        // macros that add the pointer to freepool before any value checking.
        if(e.p != NULL)
            e.destroy(e.p);
    }

    zarray_destroy(fp->za);
    free(fp);
}
