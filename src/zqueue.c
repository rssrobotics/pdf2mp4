#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "zqueue.h"

#define MIN_ALLOC 8

struct zqueue
{
    size_t el_sz; // size of each element

    // the convention is that front==back means
    // the queue is either full or empty
    size_t front;
    size_t back;
    int is_full;

    size_t alloc;
    char *data;
};


static inline int is_empty(zqueue_t *zq){
    return zq->front == zq->back && !zq->is_full;
}

zqueue_t *zqueue_create(size_t el_sz)
{
    assert(el_sz > 0);

    zqueue_t *zq = calloc(1, sizeof(zqueue_t));
    zq->el_sz = el_sz;
    zq->is_full = 1;  // true... nothing is allocated
    return zq;
}

void zqueue_destroy(zqueue_t *zq)
{
    if (zq == NULL)
        return;

    if (zq->data != NULL)
        free(zq->data);
    free(zq);
}

int zqueue_size(const zqueue_t *zq)
{
    assert(zq != NULL);

    if (zq->front <= zq->back && !zq->is_full)
        return zq->back - zq->front;
    else
        return zq->alloc - zq->front + zq->back;
}

static inline void ensure_capacity(zqueue_t *zq, size_t capacity)
{
    assert(zq != NULL);

    if(zq->alloc >= capacity)
        return;

    // some alias's
    size_t el_sz = zq->el_sz;
    size_t sz = zqueue_size(zq);

    // compute new size
    size_t newalloc = zq->alloc > MIN_ALLOC ? zq->alloc : MIN_ALLOC;
    while (newalloc < capacity)
        newalloc *= 2;

    // allocate and copy
    char *newdata = malloc(newalloc * el_sz);
    if (zq->front <= zq->back && !zq->is_full) {
        memcpy(newdata, zq->data + zq->front*el_sz, sz*el_sz);
    } else {
        size_t front_sz = zq->alloc - zq->front;
        size_t back_sz = zq->back;
        memcpy(newdata, zq->data + zq->front*el_sz, front_sz*el_sz);
        memcpy(newdata + front_sz*el_sz, zq->data, back_sz*el_sz);
    }

    // free old data and set new ptr
    free(zq->data);
    zq->data = newdata;
    zq->alloc = newalloc;

    // update meta-data
    zq->front = 0;
    zq->back = sz;
    zq->is_full = 0;  // false
}

void zqueue_add_back(zqueue_t *zq, const void *p)
{
    assert(zq != NULL);
    assert(p != NULL);

    ensure_capacity(zq, zqueue_size(zq)+1);
    memcpy(&zq->data[zq->back*zq->el_sz], p, zq->el_sz);
    zq->back = (zq->back+1) % zq->alloc;
    if (zq->back == zq->front)
        zq->is_full = 1; // true;
}

void zqueue_get(const zqueue_t *zq, int idx, void *p)
{
    assert(zq != NULL);
    assert(p != NULL);
    assert(idx >= 0);
    assert(idx < zqueue_size(zq));

    size_t i = (zq->front + idx) % zq->alloc;
    memcpy(p, &zq->data[i*zq->el_sz], zq->el_sz);
}

void zqueue_get_volatile(const zqueue_t *zq, int idx, void *p)
{
    assert(zq != NULL);
    assert(p != NULL);
    assert(idx >= 0);
    assert(idx < zqueue_size(zq));

    size_t i = (zq->front + idx) % zq->alloc;
    *((void**) p) = &zq->data[i*zq->el_sz];
}

void zqueue_remove_front(zqueue_t *zq)
{
    assert(zq != NULL);
    assert(zqueue_size(zq) > 0);

    zq->front = (zq->front + 1) % zq->alloc;
    zq->is_full = 0; // false
}

void zqueue_clear(zqueue_t *zq)
{
    assert(zq != NULL);

    while (zqueue_size(zq) > 0)
        zqueue_remove_front(zq);
}

void zqueue_map(zqueue_t *zq, void (*f)())
{
    assert(zq != NULL);
    assert(f != NULL);

    if (is_empty(zq))
        return;

    for (size_t i = zq->front; i != zq->back; i = (i+1) % zq->alloc)
        f(&zq->data[i*zq->el_sz]);

}

void zqueue_vmap(zqueue_t *zq, void (*f)())
{
    assert(zq != NULL);
    assert(f != NULL);
    assert(zq->el_sz == sizeof(void*));

    for (size_t i = zq->front; i != zq->back; i = (i+1) % zq->alloc)
        f(*(void **)&zq->data[i*zq->el_sz]);
}
