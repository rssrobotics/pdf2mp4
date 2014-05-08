#ifndef _FREEPOOL_H
#define _FREEPOOL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct freepool freepool_t;

freepool_t *freepool_create();

// destroy should be a function of one argument; it will be passed
// 'p' when the freepool is destroyed.
// returns 'p', so it can be used inline
void *freepool_add(freepool_t *fp, void *p, void (*destroy)());

void freepool_destroy(freepool_t *fp);

#ifdef __cplusplus
}
#endif

#endif
