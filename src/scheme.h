#ifndef _SCHEME_H
#define _SCHEME_H

#include <stdio.h>

#include "generic_tokenizer.h"
#include "zhash.h"

typedef struct sobject sobject_t;
typedef struct scheme scheme_t;

struct scheme_stacktrace_frame
{
    sobject_t *target; // a method, object, or closure
    sobject_t *valhead; // head of argument list
};

struct scheme
{
    generic_tokenizer_t *gt;

    sobject_t *gc_head;
    int gc_mark;
    int gc_bytes_total;
    int gc_objects_total;
    int gc_bytes_after_last_gc;
    int gc_heap_growth_trigger;

    sobject_t **roots;
    int roots_size;
    int roots_alloc;

    struct scheme_stacktrace_frame *stacktrace;
    int stacktrace_size;
    int stacktrace_alloc;
};

struct scheme_env_record
{
    char      *name;
    sobject_t *value;
};

struct sobject
{
    const char *type;
    sobject_t *gc_next;

    int32_t gc_mark;
    int32_t gc_bytes;

    void (*to_string)(sobject_t *obj, FILE *outs);

    // recursively mark all children (following any embedded pointers)
    // and set this->gc_mark
    void (*mark)(sobject_t *obj, int mark);

    void (*destroy)(sobject_t *obj);

    union {
        struct {
            struct scheme_env_record *hash;
            sobject_t *parent;
            int32_t  hash_size;
            int32_t  hash_alloc; // always power of two
        } env;
        struct {
            char *v;
        } varref;
        struct {
            double v;
        } real;
        struct {
            sobject_t *first, *rest;
        } pair;
        struct {
            char *v;
        } string;
        struct {
            int v;
        } bool;
        struct {
            sobject_t *env;
            sobject_t *params;
            sobject_t *body;
        } closure;
        struct {
            sobject_t* (*eval)(scheme_t *scheme, sobject_t *env, sobject_t *params);
            char *name;
        } method;
        struct {
            char *v;
        } symbol;
        struct {
            sobject_t *env;
            sobject_t *closure;
        } object;
        struct {
            void *impl;
        } other;
    } u;
};

extern sobject_t *SOBJECT_TRUE, *SOBJECT_FALSE;

void SCHEME_TYPE_CHECK(scheme_t *scheme, sobject_t *obj, const char *type);

static inline int SOBJECT_IS_TYPE(sobject_t *obj, const char *t)
{
    if (obj)
        return !strcmp(obj->type, t);
    return !strcmp(t, "BOOL");
}

scheme_t *scheme_create();
void scheme_destroy(scheme_t *scheme);
zarray_t *scheme_read_path(scheme_t *scheme, const char *path);

// consume one object and return it. (possibly leaving additional
// objects yet-to-be parsed).
sobject_t *scheme_read(scheme_t *scheme, generic_tokenizer_feeder_t *feeder);

// evaluate a scheme statement read in by scheme_read.
sobject_t *scheme_eval(scheme_t *scheme, sobject_t *env, sobject_t *expr);

// create an empty scheme environment. (Most users will want to call
// "scheme_env_setup_basic" on this environment).
sobject_t *scheme_env_create(scheme_t *scheme, sobject_t *parent);

sobject_t *scheme_env_lookup(scheme_t *scheme, sobject_t *env, const char *s);
void scheme_env_define(scheme_t *scheme, sobject_t *env, const char *_s, sobject_t *obj);
void scheme_env_set(scheme_t *scheme, sobject_t *env, const char *_s, sobject_t *obj);

// populate an environment with a basic set of scheme procedures
void scheme_env_setup_basic(scheme_t *scheme, sobject_t *env);


void scheme_env_add_method(scheme_t *scheme, sobject_t *env,
                           const char *name, sobject_t* (eval)(scheme_t *scheme, sobject_t *env, sobject_t *params));

sobject_t *scheme_real_create(scheme_t *scheme, double v);
sobject_t *scheme_pair_create(scheme_t *scheme, sobject_t *first, sobject_t *rest);
sobject_t *scheme_string_create(scheme_t *scheme, const char *v);
sobject_t *scheme_symbol_create(scheme_t *scheme, const char *v);

sobject_t *scheme_first(scheme_t *scheme, sobject_t *obj);
sobject_t *scheme_rest(scheme_t *scheme, sobject_t *obj);
sobject_t *scheme_second(scheme_t *scheme, sobject_t *obj);
sobject_t *scheme_third(scheme_t *scheme, sobject_t *obj);
sobject_t *scheme_fourth(scheme_t *scheme, sobject_t *obj);
sobject_t *scheme_fifth(scheme_t *scheme, sobject_t *obj);
int scheme_is_true(scheme_t *scheme, sobject_t *obj);

void scheme_gc_register(scheme_t *scheme, sobject_t *obj, int bytes);

#endif
