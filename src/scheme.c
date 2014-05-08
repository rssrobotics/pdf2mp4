#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <stdarg.h>

#include "scheme.h"
#include "zhash.h"

sobject_t *SOBJECT_TRUE, *SOBJECT_FALSE;

static void runtime_exception_tok(scheme_t *scheme, const gt_token_t *tok, const char *format, ...);
static void runtime_exception_obj(scheme_t *scheme, sobject_t *obj, const char *format, ...);

sobject_t *scheme_eval_apply(scheme_t *scheme, sobject_t *env, sobject_t *target, sobject_t *valhead);
sobject_t *scheme_eval_real(scheme_t *scheme, sobject_t *env, sobject_t *expr);

void SCHEME_TYPE_CHECK(scheme_t *scheme, sobject_t *obj, const char *type)
{
    if (obj == NULL)
        runtime_exception_obj(scheme, obj,
                              "expected object of type %s, got NULL", type);

    if (!SOBJECT_IS_TYPE(obj, type))
        runtime_exception_obj(scheme, obj,
                              "expected object of type %s, got an object of type %s",
                              type, obj->type);
}

void scheme_stacktrace_push(scheme_t *scheme, sobject_t *target, sobject_t *valhead)
{
    if (scheme->stacktrace_size + 1 >= scheme->stacktrace_alloc) {
        scheme->stacktrace_alloc *= 2;
        if (scheme->stacktrace_alloc < 128)
            scheme->stacktrace_alloc = 128;

        scheme->stacktrace = realloc(scheme->stacktrace,
                                     scheme->stacktrace_alloc * sizeof(struct scheme_stacktrace_frame));
    }

    scheme->stacktrace[scheme->stacktrace_size].target = target;
    scheme->stacktrace[scheme->stacktrace_size].valhead = valhead;
    scheme->stacktrace_size++;
}

void scheme_stacktrace_pop(scheme_t *scheme, sobject_t *target, sobject_t *valhead)
{
    scheme->stacktrace_size--;
    assert(scheme->stacktrace_size >= 0);
    if (target) {
        if (scheme->stacktrace[scheme->stacktrace_size].target != target ||
            scheme->stacktrace[scheme->stacktrace_size].valhead != valhead) {
            assert(0);
            exit(-1);
        }
    }
}

/** The basic rule of push and pop:

   Don't call scheme_eval (or scheme_eval_list or scheme_eval_apply)
   (which can call scheme_gc) if there are any unreferenced
   sobjects. Create a reference via pushing and popping, if necessary.
**/
void scheme_roots_push(scheme_t *scheme, sobject_t *obj)
{
    if (scheme->roots_size + 1 >= scheme->roots_alloc) {
        scheme->roots_alloc *= 2;
        if (scheme->roots_alloc < 128)
            scheme->roots_alloc = 128;

        scheme->roots = realloc(scheme->roots, scheme->roots_alloc * sizeof(sobject_t*));
    }

    scheme->roots[scheme->roots_size++] = obj;
}

/* the second argument can be NULL, but otherwise, there's a safety
 * check to make sure you're unpopping the element you meant to. */
void scheme_roots_pop(scheme_t *scheme, sobject_t *obj)
{
    scheme->roots_size--;
    assert(scheme->roots_size >= 0);
    if (obj) {
        if (scheme->roots[scheme->roots_size] != obj) {
            assert(0);
            exit(-1);
        }
    }
}

/** Call this (exactly once) on every newly-created object to register
it; the GC will then be able to dispose of it. By convention, this
should be called by type-specific _create functions: e.g.,
scheme_pair_create calls this for you. You need this only if you
implement new types.

bytes should be an estimate of the memory footprint of this object, not counting any
reachable children.
**/
void scheme_gc_register(scheme_t *scheme, sobject_t *obj, int bytes)
{
    obj->gc_bytes = bytes;
    obj->gc_next = scheme->gc_head;
    scheme->gc_head = obj;
    scheme->gc_bytes_total += bytes;
    scheme->gc_objects_total ++;
}

static void generic_mark(sobject_t *obj, int32_t mark)
{
    obj->gc_mark = mark;
}

static void generic_destroy(sobject_t *s)
{
    free(s);
}

///////////////////////////////////////////////////
// object
//
// Objects are effectively a letrec containing a closure, with the
// advantage that the pointer to the env is explicitly maintained.
// This makes it easier for native code to reference the member
// variables. In addition, the name "self" is automatically bound to
// the object itself in the environment. (Note: this value of "self"
// is only locally visible; calls to a "super class" will have a
// different value of self.)
//
// (define myobj (object-create ((x 5) (y 7))
//                   (lambda (message . args) (...))))
//
// myobj can still be treated as a lambda expression (whose environment
// contains definitions for x and y.... e.g.,
//
// (myobj 'dosomething args)

///////////////////////////////////////////////////

static void object_to_string(sobject_t *s, FILE *out)
{
    fprintf(out, "[Object]");
}

static void object_mark(sobject_t *obj, int32_t mark)
{
    if (obj->gc_mark == mark)
        return;

    obj->gc_mark = mark;
    obj->u.object.env->mark(obj->u.object.env, mark);
    obj->u.object.closure->mark(obj->u.object.closure, mark);
}

static void object_destroy(sobject_t *s)
{
    free(s);
}

sobject_t *scheme_object_create(scheme_t *scheme, sobject_t *env, sobject_t *closure)
{
    SCHEME_TYPE_CHECK(scheme, env, "ENV");
    SCHEME_TYPE_CHECK(scheme, closure, "CLOSURE");

    sobject_t *obj = calloc(1, sizeof(sobject_t));
    obj->to_string = object_to_string;
    obj->mark = object_mark;
    obj->destroy = object_destroy;

    obj->type = "OBJECT";
    obj->u.object.env = env;
    obj->u.object.closure = closure;


    scheme_gc_register(scheme, obj, sizeof(sobject_t));

    return obj;
}

///////////////////////////////////////////////////
// real (number) type
///////////////////////////////////////////////////
static void real_to_string(sobject_t *s, FILE *out)
{
    if (s->u.real.v == (int) s->u.real.v)
        fprintf(out, "%d", (int) s->u.real.v);
    else
        fprintf(out, "%f", s->u.real.v);
}

sobject_t *scheme_real_create(scheme_t *scheme, double v)
{
    sobject_t *obj = calloc(1, sizeof(sobject_t));
    obj->to_string = real_to_string;
    obj->mark = generic_mark;
    obj->destroy = generic_destroy;

    obj->type = "REAL";
    obj->u.real.v = v;

    scheme_gc_register(scheme, obj, sizeof(sobject_t));

    return obj;
}

///////////////////////////////////////////////////
// pair type
///////////////////////////////////////////////////
static void pair_to_string_list(sobject_t *s, FILE *out)
{
    sobject_t *first = s->u.pair.first,
        *rest = s->u.pair.rest;

    if (rest == NULL) {
        if (first == NULL)
            fprintf(out, "()");
        else
            first->to_string(first, out);
        return;
    }

    if (SOBJECT_IS_TYPE(rest, "PAIR")) {
        if (first == NULL)
            fprintf(out, "()");
        else
            first->to_string(first, out);

        fprintf(out, " ");
        pair_to_string_list(rest, out);
        return;
    }

    if (first == NULL)
        fprintf(out, "null");
    else
        first->to_string(first, out);
    fprintf(out, " . ");
    if (rest == NULL)
        fprintf(out, "null");
    else
        rest->to_string(rest, out);
}

static void pair_to_string(sobject_t *s, FILE *out)
{
    sobject_t *first = s->u.pair.first,
        *rest = s->u.pair.rest;

    // pretty print without all the dots. if you want dots,
    // do if(0)

    if (1) {
        if (rest == NULL) {
            fprintf(out, "(");
            if (first == NULL)
                fprintf(out, "()");
            else
                first->to_string(first, out);
            fprintf(out, ")");
            return;
        }

        if (SOBJECT_IS_TYPE(rest, "PAIR")) {
            fprintf(out, "(");

            if (first == NULL)
                fprintf(out, "()");
            else
                first->to_string(first, out);

            fprintf(out, " ");
            pair_to_string_list(rest, out);
            fprintf(out, ")");
            return;
        }
    }

    // default simple pair
    fprintf(out, "(");
    if (first == NULL)
        fprintf(out, "null");
    else
        first->to_string(first, out);
    fprintf(out, " . ");
    if (rest == NULL)
        fprintf(out, "null");
    else
        rest->to_string(rest, out);

    fprintf(out, ")");
}

static void pair_mark(sobject_t *obj, int32_t mark)
{
    if (obj->gc_mark == mark)
        return;

    obj->gc_mark = mark;

    if (obj->u.pair.first)
        obj->u.pair.first->mark(obj->u.pair.first, mark);
    if (obj->u.pair.rest)
        obj->u.pair.rest->mark(obj->u.pair.rest, mark);
}

sobject_t *scheme_pair_create(scheme_t *scheme, sobject_t *first, sobject_t *rest)
{
    sobject_t *obj = calloc(1, sizeof(sobject_t));
    obj->type = "PAIR";
    obj->to_string = pair_to_string;
    obj->mark = pair_mark;
    obj->destroy = generic_destroy;
    obj->u.pair.first = first;
    obj->u.pair.rest = rest;

    scheme_gc_register(scheme, obj, sizeof(sobject_t));

    return obj;
}

///////////////////////////////////////////////////
// string type
///////////////////////////////////////////////////
static void string_to_string(sobject_t *obj, FILE *out)
{
    fprintf(out, "%s", obj->u.string.v);
}

static void string_destroy(sobject_t *s)
{
    free(s->u.string.v);
    free(s);
}

sobject_t *scheme_string_create(scheme_t *scheme, const char *v)
{
    sobject_t *obj = calloc(1, sizeof(sobject_t));
    obj->to_string = string_to_string;
    obj->mark = generic_mark;
    obj->destroy = string_destroy;
    obj->type = "STRING";
    obj->u.string.v = strdup(v);

    scheme_gc_register(scheme, obj, sizeof(sobject_t) + strlen(v));

    return obj;
}

///////////////////////////////////////////////////
// symbol type
///////////////////////////////////////////////////
static void symbol_to_string(sobject_t *obj, FILE *out)
{
    fprintf(out, "%s", obj->u.symbol.v);
}

static void symbol_destroy(sobject_t *s)
{
    free(s->u.symbol.v);
    free(s);
}

sobject_t *scheme_symbol_create(scheme_t *scheme, const char *v)
{
    sobject_t *obj = calloc(1, sizeof(sobject_t));
    obj->to_string = symbol_to_string;
    obj->mark = generic_mark;
    obj->destroy = symbol_destroy;
    obj->type = "SYMBOL";
    obj->u.symbol.v = strdup(v);

    scheme_gc_register(scheme, obj, sizeof(sobject_t) + strlen(v));

    return obj;
}

///////////////////////////////////////////////////
// bool type
///////////////////////////////////////////////////
static void bool_to_string(sobject_t *obj, FILE *out)
{
    fprintf(out, "%s", obj->u.bool.v ? "#t" : "#f");
}

sobject_t *scheme_bool_create(scheme_t *scheme, int v)
{
    sobject_t *obj = calloc(1, sizeof(sobject_t));
    obj->to_string = bool_to_string;
    obj->destroy = generic_destroy;
    obj->mark = generic_mark;
    obj->type = "BOOL";
    obj->u.bool.v = v;

    scheme_gc_register(scheme, obj, sizeof(sobject_t));

    return obj;
}

///////////////////////////////////////////////////
// closure type
///////////////////////////////////////////////////
static void closure_to_string(sobject_t *obj, FILE *out)
{
    fprintf(out, "(lambda ");
    if (obj->u.closure.params == NULL)
        fprintf(out, "()");
    else
        obj->u.closure.params->to_string(obj->u.closure.params, out);
    fprintf(out, " ");
    obj->u.closure.body->to_string(obj->u.closure.body, out);
    fprintf(out, ")");
}

static void closure_mark(sobject_t *obj, int32_t mark)
{
    if (obj->gc_mark == mark)
        return;

    obj->gc_mark = mark;

    if (obj->u.closure.params)
        obj->u.closure.params->mark(obj->u.closure.params, mark);
    obj->u.closure.body->mark(obj->u.closure.body, mark);
    obj->u.closure.env->mark(obj->u.closure.env, mark);
}

sobject_t *scheme_closure_create(scheme_t *scheme, sobject_t *env, sobject_t *params, sobject_t *body)
{
    sobject_t *obj = calloc(1, sizeof(sobject_t));
    obj->to_string = closure_to_string;
    obj->mark = closure_mark;
    obj->destroy = generic_destroy;
    obj->type = "CLOSURE";
    obj->u.closure.env = env;
    obj->u.closure.params = params;
    obj->u.closure.body = body;

    scheme_gc_register(scheme, obj, sizeof(sobject_t));

    return obj;
}

///////////////////////////////////////////////////
static void method_to_string(sobject_t *obj, FILE *out)
{
    fprintf(out, "[Method %s]", obj->u.method.name);
}

static void method_destroy(sobject_t *obj)
{
    free(obj->u.method.name);
    free(obj);
}

sobject_t *scheme_method_create(scheme_t *scheme, const char *name, sobject_t* (*eval)(scheme_t *scheme, sobject_t *env, sobject_t *params))
{
    sobject_t *obj = calloc(1, sizeof(sobject_t));
    obj->to_string = method_to_string;
    obj->destroy = method_destroy;
    obj->mark = generic_mark;
    obj->type = "METHOD";
    obj->u.method.eval = eval;
    obj->u.method.name = strdup(name);

    scheme_gc_register(scheme, obj, sizeof(sobject_t) + strlen(name));

    return obj;
}

///////////////////////////////////////////////////
scheme_t *scheme_create()
{
    scheme_t *scheme = calloc(1, sizeof(scheme_t));

    if (SOBJECT_TRUE == NULL) {
        SOBJECT_TRUE = scheme_bool_create(scheme, 1);
        SOBJECT_FALSE = scheme_bool_create(scheme, 0);

        // make objects allocated above immortal
        scheme->gc_head = NULL;
    }

//    scheme->gc_heap_growth_trigger = -999999999;
    scheme->gc_heap_growth_trigger = 1024*1024*16;

    scheme->gt = generic_tokenizer_create("ERROR");
    generic_tokenizer_add_ignore(scheme->gt, "\\s+");
    generic_tokenizer_add_ignore(scheme->gt, "\\$");
    generic_tokenizer_add_ignore(scheme->gt, ";[^\n]*\n"); // comments

    generic_tokenizer_add(scheme->gt, "NUMBER-HEX", "0x([0-9A-Fa-f]*"); // hexidecimal
    generic_tokenizer_add(scheme->gt, "NUMBER", "(-|+)?\\.[0-9]+((e|E)\\-?[0-9]+)?");
    generic_tokenizer_add(scheme->gt, "NUMBER", "(-|+)?[0-9]+[\\.0-9]*((e|E)\\-?[0-9]+)?");
    generic_tokenizer_add(scheme->gt, "IDENTIFIER", "[a-zA-Z\\!\\$\\%\\&\\*\\/\\:\\<\\=\\>\\?\\~\\_\\^][a-zA-Z\\!\\$\\%\\&\\*\\/\\:\\<\\=\\>\\?\\~\\_\\^0-9\\.\\+\\-]*");
    generic_tokenizer_add_escape(scheme->gt, "OP", "( ) ' # .");
    generic_tokenizer_add(scheme->gt, "ERROR", ".");
    generic_tokenizer_add(scheme->gt, "BOOLEAN", "(#t)|(#f)");
    generic_tokenizer_add(scheme->gt, "QUASITHING", ",@");
    generic_tokenizer_add(scheme->gt, "STRING", "\"[^\"]*\"");

    return scheme;
}

///////////////////////////////////////////////////
// simple utility functions
static const char* scheme_to_identifier(scheme_t *scheme, sobject_t *obj)
{
    SCHEME_TYPE_CHECK(scheme, obj, "SYMBOL");

    return obj->u.string.v;
}

sobject_t *scheme_first(scheme_t *scheme, sobject_t *obj)
{
    SCHEME_TYPE_CHECK(scheme, obj, "PAIR");

    return obj->u.pair.first;
}

sobject_t *scheme_rest(scheme_t *scheme, sobject_t *obj)
{
    SCHEME_TYPE_CHECK(scheme, obj, "PAIR");

    return obj->u.pair.rest;
}

sobject_t *scheme_second(scheme_t *scheme, sobject_t *obj)
{
    return scheme_first(scheme, scheme_rest(scheme, obj));
}

sobject_t *scheme_third(scheme_t *scheme, sobject_t *obj)
{
    return scheme_first(scheme, scheme_rest(scheme, scheme_rest(scheme, obj)));
}

sobject_t *scheme_fourth(scheme_t *scheme, sobject_t *obj)
{
    return scheme_first(scheme, scheme_rest(scheme, scheme_rest(scheme, scheme_rest(scheme, obj))));
}

sobject_t *scheme_fifth(scheme_t *scheme, sobject_t *obj)
{
    return scheme_first(scheme, scheme_rest(scheme, scheme_rest(scheme, scheme_rest(scheme, scheme_rest(scheme, obj)))));
}

int scheme_is_true(scheme_t *scheme, sobject_t *obj)
{
    return (obj != SOBJECT_FALSE);
}

static void runtime_exception_tok(scheme_t *scheme, const gt_token_t *tok, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    fprintf(stderr, "-------------------------------------------------------\n");
    fprintf(stderr, " runtime exception: ");
    vfprintf(stderr, format, ap);

    fprintf(stderr, " near token %s at line %d:%d\n", tok->token, tok->line, tok->column);

    va_end(ap);
    exit(-1);
}

static void stacktrace(scheme_t *scheme)
{
    for (int ago = 0; ago < 5; ago++) {
        int idx = scheme->stacktrace_size-1-ago;
        if (idx < 0)
            break;

        sobject_t *target = scheme->stacktrace[idx].target;
        sobject_t *valhead = scheme->stacktrace[idx].valhead;

        sobject_t *tmp = scheme_pair_create(scheme, target, valhead);

        fprintf(stderr, " [stack frame %d] ", ago);

        tmp->to_string(tmp, stderr);

        fprintf(stderr, "\n");
    }
}

static void runtime_exception_obj(scheme_t *scheme, sobject_t *obj, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    fprintf(stderr, "-------------------------------------------------------\n");
    fprintf(stderr, "runtime exception: ");
    vfprintf(stderr, format, ap);

    fprintf(stderr, " (object: ");
    if (obj)
        obj->to_string(obj, stderr);
    else
        fprintf(stderr, "NULL");

    fprintf(stderr, ")\n");

    va_end(ap);

    fprintf(stderr, "\n");
    stacktrace(scheme);
    fprintf(stderr, "\n");

    exit(-1);
}

static int scheme_eq(scheme_t *scheme, sobject_t *a, sobject_t *b)
{
    if (a == b)
        return 1;

    if (SOBJECT_IS_TYPE(a, "REAL") && SOBJECT_IS_TYPE(b, "REAL"))
        return a->u.real.v == b->u.real.v;

    if (SOBJECT_IS_TYPE(a, "STRING") && SOBJECT_IS_TYPE(b, "STRING"))
        return !strcmp(a->u.string.v, b->u.string.v);

    if (SOBJECT_IS_TYPE(a, "SYMBOL") && SOBJECT_IS_TYPE(b, "SYMBOL"))
        return !strcmp(a->u.symbol.v, b->u.symbol.v);

    return 0;
}

static int scheme_eqv(scheme_t *scheme, sobject_t *a, sobject_t *b)
{
    if (SOBJECT_IS_TYPE(a, "PAIR") && SOBJECT_IS_TYPE(b, "PAIR")) {
        return (scheme_eq(scheme, scheme_first(scheme, a), scheme_first(scheme, b)) &&
                scheme_eqv(scheme, scheme_rest(scheme, a), scheme_rest(scheme, b)));
    }

    return scheme_eq(scheme, a, b);
}


///////////////////////////////////////////////////
// environment
///////////////////////////////////////////////////

void env_destroy(sobject_t *env)
{
    // don't do a deep destroy- some of the objects inside us could
    // still be valid.
    if (env->u.env.hash_size) {
        for (int i = 0; i < env->u.env.hash_alloc; i++) {
            free(env->u.env.hash[i].name);
        }

        free(env->u.env.hash);
    }

    free(env);
}

static void env_to_string(sobject_t *env, FILE *out)
{
    fprintf(out, "** environment %p\n", env);

    if (env->u.env.hash_size) {

        for (int i = 0; i < env->u.env.hash_alloc; i++) {
            char *name = env->u.env.hash[i].name;
            sobject_t *value = env->u.env.hash[i].value;

            if (name != NULL) {
                fprintf(out, " %20s ", name);
                if (value == NULL)
                    fprintf(out,"NULL");
                else
                    value->to_string(value, out);
                fprintf(out, "\n");
            }
        }
    }
}

static void env_mark(sobject_t *env, int32_t mark)
{
    if (env->gc_mark == mark)
        return;

    env->gc_mark = mark;

    if (env->u.env.hash_size) {

        for (int i = 0; i < env->u.env.hash_alloc; i++) {
            char *name = env->u.env.hash[i].name;
            sobject_t *value = env->u.env.hash[i].value;

            if (name != NULL)
                value->mark(value, mark);
        }
    }

    if (env->u.env.parent)
        env->u.env.parent->mark(env->u.env.parent, mark);
}

sobject_t *scheme_env_create(scheme_t *scheme, sobject_t *parent)
{
    sobject_t *env = calloc(1, sizeof(sobject_t));
    env->to_string = env_to_string;
    env->destroy = env_destroy;
    env->mark = env_mark;
    env->type = "ENV";

    env->u.env.parent = parent;

    // hard to estimate the size of an environment... just take a crazy guess.
    scheme_gc_register(scheme, env, sizeof(sobject_t) + 128);

    return env;
}

// note: can return NULL. Returns the hash record in which the symbol
// can be found (in this or in a parent environment). Returning the
// record allows set! to be used without having to search again.
inline struct scheme_env_record *scheme_env_lookup_hash(scheme_t *scheme, sobject_t *env, const char *s, int32_t hash)
{
    // does this environment have any mappings?
    if (env->u.env.hash_size) {
        int32_t idx = hash & (env->u.env.hash_alloc - 1);

        // we exploit the fact that there are never any deletions. (we
        // don't implement an "unset!" operator). Thus, to deal with
        // collisions, we just check successive entries in the hash
        // table until we get an empty cell.

        while (1) {
            // it's not in the hash table if we hit a NULL entry
            if (env->u.env.hash[idx].name == NULL)
                break;

            if (!strcmp(env->u.env.hash[idx].name, s)) {
                // found it!
                return &env->u.env.hash[idx];
            }

            // go to next bucket. We'll always terminate because we
            // never allow size to be equal to alloc.
            idx = (idx+1) & (env->u.env.hash_alloc - 1);
        }
    }

    // if we're here, then there was no hit. try the parent.
    if (env->u.env.parent)
        return scheme_env_lookup_hash(scheme, env->u.env.parent, s, hash);

    return NULL;
}

sobject_t *scheme_env_lookup(scheme_t *scheme, sobject_t *env, const char *s)
{
    int32_t hash = zhash_str_hash(&s);
    struct scheme_env_record *rec = scheme_env_lookup_hash(scheme, env, s, hash);

    if (rec)
        return rec->value;
    return NULL;
}

void scheme_env_define(scheme_t *scheme, sobject_t *env, const char *s, sobject_t *obj)
{
    int32_t hash = zhash_str_hash(&s);

    // rehash?
    if (2*(env->u.env.hash_size + 1) >= env->u.env.hash_alloc) {
        // yes, don't exceed target capacity of .5
        int new_alloc = 2*env->u.env.hash_alloc;
        if (new_alloc < 8)
            new_alloc = 8;
        int new_size = env->u.env.hash_size; // no change here
        struct scheme_env_record *new_hash = calloc(new_alloc, sizeof(struct scheme_env_record));

        for (int i = 0; i < env->u.env.hash_alloc; i++) {
            char *name = env->u.env.hash[i].name;
            sobject_t *value = env->u.env.hash[i].value;

            if (!name)
                continue;

            // now insert this element into the new hash table.
            int32_t idx = zhash_str_hash(&name) & (new_alloc - 1);

            while (new_hash[idx].name != NULL)
                idx = (idx+1) & (new_alloc - 1);

            new_hash[idx].name = name;
            new_hash[idx].value = value;
        }

        free(env->u.env.hash);

        env->u.env.hash_size = new_size;
        env->u.env.hash_alloc = new_alloc;
        env->u.env.hash = new_hash;
    }

    // this is mostly like put_env, except that it gracefully handles
    // the case where one of the keys already exists.
    int32_t idx = hash & (env->u.env.hash_alloc - 1);

    while (env->u.env.hash[idx].name != NULL && strcmp(env->u.env.hash[idx].name, s))
        idx = (idx+1) & (env->u.env.hash_alloc - 1);

    if (env->u.env.hash[idx].name) {
        // this is technically an error--- define called on an already set variable.
        // but we support it.
        env->u.env.hash[idx].value = obj;
    } else {
        env->u.env.hash[idx].name = strdup(s);
        env->u.env.hash[idx].value = obj;
        env->u.env.hash_size++;
    }
}

void scheme_env_set(scheme_t *scheme, sobject_t *env, const char *s, sobject_t *obj)
{
    int32_t hash = zhash_str_hash(&s);
    struct scheme_env_record *rec = scheme_env_lookup_hash(scheme, env, s, hash);

    if (rec != NULL) {
        rec->value = obj;
        return;
    }

    runtime_exception_obj(scheme, obj, "set called on unset variable '%s'", s);
}


void scheme_env_add_method(scheme_t *scheme, sobject_t *env,
                           const char *name, sobject_t* (eval)(scheme_t *scheme, sobject_t *env, sobject_t *params))
{
    sobject_t *obj = scheme_method_create(scheme, name, eval);

    scheme_env_define(scheme, env, name, obj);
}

///////////////////////////////////////////
// builtin functions
//
static sobject_t *builtin_output(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    for ( ; head != NULL; head = scheme_rest(scheme, head)) {
        sobject_t *obj = scheme_first(scheme, head);

        obj->to_string(obj, stderr);
    }

    return SOBJECT_TRUE;
}

// NB: these are passed a list of arguments. Functions that process
// only the first argument (e.g. car) must access the first element of
// the list explicitly.
static sobject_t* builtin_zero_q(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    for ( ; head != NULL; head = scheme_rest(scheme, head)) {
        sobject_t *obj = scheme_first(scheme, head);
        SCHEME_TYPE_CHECK(scheme, obj, "REAL");
        if (obj->u.real.v != 0)
            return SOBJECT_FALSE;
    }

    return SOBJECT_TRUE;
}

static sobject_t* builtin_exp(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    SCHEME_TYPE_CHECK(scheme, scheme_first(scheme, head), "REAL");

    return scheme_real_create(scheme, exp(scheme_first(scheme, head)->u.real.v));
}

static sobject_t* builtin_floor(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    SCHEME_TYPE_CHECK(scheme, scheme_first(scheme, head), "REAL");

    return scheme_real_create(scheme, (int) floor(scheme_first(scheme, head)->u.real.v));
}

static sobject_t* builtin_add(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    double acc = 0;
    for ( ; head != NULL; head = scheme_rest(scheme, head)) {
        sobject_t *obj = scheme_first(scheme, head);
        SCHEME_TYPE_CHECK(scheme, obj, "REAL");
        acc += obj->u.real.v;
    }

    return scheme_real_create(scheme, acc);
}

static sobject_t* builtin_subtract(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    sobject_t *tmp = scheme_first(scheme, head);
    SCHEME_TYPE_CHECK(scheme, tmp, "REAL");

    double acc = tmp->u.real.v;
    head = scheme_rest(scheme, head);

    // unary minus?
    if (head == NULL)
        return scheme_real_create(scheme, -acc);

    for ( ; head != NULL; head = scheme_rest(scheme, head)) {
        sobject_t *obj = scheme_first(scheme, head);
        SCHEME_TYPE_CHECK(scheme, obj, "REAL");
        acc -= obj->u.real.v;
    }

    return scheme_real_create(scheme, acc);
}

static sobject_t* builtin_assert(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    sobject_t *tmp = scheme_first(scheme, head);

    if (!scheme_is_true(scheme, tmp)) {
        printf("ASSERTION FAILED\n");
        exit(-1);
    }

    return SOBJECT_TRUE;
}

static sobject_t* builtin_equals(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    sobject_t *tmp = scheme_first(scheme, head);
    SCHEME_TYPE_CHECK(scheme, tmp, "REAL");

    double acc = tmp->u.real.v;
    head = scheme_rest(scheme, head);

    for ( ; head != NULL; head = scheme_rest(scheme, head)) {
        sobject_t *obj = scheme_first(scheme, head);
        SCHEME_TYPE_CHECK(scheme, obj, "REAL");
        if (obj->u.real.v != acc)
            return SOBJECT_FALSE;
    }

    return SOBJECT_TRUE;
}

static sobject_t* builtin_lessthan(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    sobject_t *tmp = scheme_first(scheme, head);
    SCHEME_TYPE_CHECK(scheme, tmp, "REAL");

    double acc = tmp->u.real.v;
    head = scheme_rest(scheme, head);

    for ( ; head != NULL; head = scheme_rest(scheme, head)) {
        sobject_t *obj = scheme_first(scheme, head);
        SCHEME_TYPE_CHECK(scheme, obj, "REAL");
        if (obj->u.real.v <= acc)
            return SOBJECT_FALSE;
        acc = obj->u.real.v;
    }

    return SOBJECT_TRUE;
}

static sobject_t* builtin_greaterthan(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    sobject_t *tmp = scheme_first(scheme, head);
    SCHEME_TYPE_CHECK(scheme, tmp, "REAL");

    double acc = tmp->u.real.v;
    head = scheme_rest(scheme, head);

    for ( ; head != NULL; head = scheme_rest(scheme, head)) {
        sobject_t *obj = scheme_first(scheme, head);
        SCHEME_TYPE_CHECK(scheme, obj, "REAL");
        if (obj->u.real.v >= acc)
            return SOBJECT_FALSE;
        acc = obj->u.real.v;
    }

    return SOBJECT_TRUE;
}

static sobject_t* builtin_multiply(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    double acc = 1;
    for ( ; head != NULL; head = scheme_rest(scheme, head)) {
        sobject_t *obj = scheme_first(scheme, head);
        SCHEME_TYPE_CHECK(scheme, obj, "REAL");
        acc *= obj->u.real.v;
    }

    return scheme_real_create(scheme, acc);
}

static sobject_t* builtin_divide(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    sobject_t *tmp = scheme_first(scheme, head);
    SCHEME_TYPE_CHECK(scheme, tmp, "REAL");

    double acc = tmp->u.real.v;
    head = scheme_rest(scheme, head);

    for ( ; head != NULL; head = scheme_rest(scheme, head)) {
        sobject_t *obj = scheme_first(scheme, head);
        SCHEME_TYPE_CHECK(scheme, obj, "REAL");
        acc /= obj->u.real.v;
    }

    return scheme_real_create(scheme, acc);
}

static sobject_t* builtin_min(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    double acc = HUGE;
    for ( ; head != NULL; head = scheme_rest(scheme, head)) {
        sobject_t *obj = scheme_first(scheme, head);
        SCHEME_TYPE_CHECK(scheme, obj, "REAL");
        if (obj->u.real.v < acc)
            acc = obj->u.real.v;
    }

    return scheme_real_create(scheme, acc);
}

static sobject_t* builtin_max(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    double acc = -HUGE;
    for ( ; head != NULL; head = scheme_rest(scheme, head)) {
        sobject_t *obj = scheme_first(scheme, head);
        SCHEME_TYPE_CHECK(scheme, obj, "REAL");
        if (obj->u.real.v > acc)
            acc = obj->u.real.v;
    }

    return scheme_real_create(scheme, acc);
}

static sobject_t* builtin_and(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    for ( ; head != NULL; head = scheme_rest(scheme, head)) {
        sobject_t *obj = scheme_first(scheme, head);
        if (!scheme_is_true(scheme, obj))
            return SOBJECT_FALSE;
    }
    return SOBJECT_TRUE;
}

static sobject_t* builtin_or(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    for ( ; head != NULL; head = scheme_rest(scheme, head)) {
        sobject_t *obj = scheme_first(scheme, head);
        if (scheme_is_true(scheme, obj))
            return SOBJECT_TRUE;
    }
    return SOBJECT_FALSE;
}

static sobject_t* builtin_not(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    return scheme_is_true(scheme, scheme_first(scheme, head)) ? SOBJECT_FALSE : SOBJECT_TRUE;
}

static sobject_t* builtin_boolean_q(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    return SOBJECT_IS_TYPE(scheme_first(scheme, head), "BOOL") ? SOBJECT_TRUE : SOBJECT_FALSE;
}

static sobject_t* builtin_real_q(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    return SOBJECT_IS_TYPE(scheme_first(scheme, head), "REAL") ? SOBJECT_TRUE : SOBJECT_FALSE;
}

static sobject_t* builtin_pair_q(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    return SOBJECT_IS_TYPE(scheme_first(scheme, head), "PAIR") ? SOBJECT_TRUE : SOBJECT_FALSE;
}

static sobject_t* builtin_null_q(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    return (scheme_first(scheme, head) == NULL) ? SOBJECT_TRUE : SOBJECT_FALSE;
}

static sobject_t* builtin_eq_q(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    sobject_t *a = scheme_first(scheme, head), *b = scheme_second(scheme, head);

    return scheme_eq(scheme, a, b) ? SOBJECT_TRUE : SOBJECT_FALSE;
}

static sobject_t* builtin_eqv_q(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    sobject_t *a = scheme_first(scheme, head), *b = scheme_second(scheme, head);

    return scheme_eqv(scheme, a, b) ? SOBJECT_TRUE : SOBJECT_FALSE;
}

static sobject_t* builtin_list(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    return head;
}

static sobject_t* builtin_cons(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    sobject_t *a = scheme_first(scheme, head), *b = scheme_first(scheme, scheme_rest(scheme, head));
    return scheme_pair_create(scheme, a, b);
}

static sobject_t* builtin_car(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    return scheme_first(scheme, scheme_first(scheme, head));
}

static sobject_t* builtin_cdr(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    return scheme_rest(scheme, scheme_first(scheme, head));
}

static sobject_t* builtin_second(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    return scheme_second(scheme, scheme_first(scheme, head));
}

static sobject_t* builtin_third(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    return scheme_third(scheme, scheme_first(scheme, head));
}

static sobject_t* builtin_fourth(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    return scheme_fourth(scheme, scheme_first(scheme, head));
}

static sobject_t* builtin_apply(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    scheme_roots_push(scheme, head);

    sobject_t *apply = scheme_symbol_create(scheme, "apply");
    scheme_stacktrace_push(scheme, apply, head);

    sobject_t *value = scheme_eval_apply(scheme, env,
                                         scheme_first(scheme, head), scheme_second(scheme, head));

    scheme_stacktrace_pop(scheme, apply, head);

    scheme_roots_pop(scheme, head);
    return value;
}

void scheme_env_setup_basic(scheme_t *scheme, sobject_t *env)
{
    scheme_env_add_method(scheme, env, "boolean?", builtin_boolean_q);
    scheme_env_add_method(scheme, env, "real?", builtin_real_q);
    scheme_env_add_method(scheme, env, "pair?", builtin_pair_q);
    scheme_env_add_method(scheme, env, "null?", builtin_null_q);
    scheme_env_add_method(scheme, env, "eq?", builtin_eq_q);
    scheme_env_add_method(scheme, env, "eqv?", builtin_eqv_q);
    scheme_env_add_method(scheme, env, "zero?", builtin_zero_q);
    scheme_env_add_method(scheme, env, "assert", builtin_assert);
    scheme_env_add_method(scheme, env, "exp", builtin_exp);
    scheme_env_add_method(scheme, env, "floor", builtin_floor);
    scheme_env_add_method(scheme, env, "+", builtin_add);
    scheme_env_add_method(scheme, env, "-", builtin_subtract);
    scheme_env_add_method(scheme, env, "=", builtin_equals);
    scheme_env_add_method(scheme, env, ">", builtin_greaterthan);
    scheme_env_add_method(scheme, env, "<", builtin_lessthan);
    scheme_env_add_method(scheme, env, "*", builtin_multiply);
    scheme_env_add_method(scheme, env, "/", builtin_divide);
    scheme_env_add_method(scheme, env, "min", builtin_min);
    scheme_env_add_method(scheme, env, "max", builtin_max);
    scheme_env_add_method(scheme, env, "and", builtin_and);
    scheme_env_add_method(scheme, env, "or", builtin_or);
    scheme_env_add_method(scheme, env, "not", builtin_not);
    scheme_env_add_method(scheme, env, "list", builtin_list);
    scheme_env_add_method(scheme, env, "cons", builtin_cons);
    scheme_env_add_method(scheme, env, "car", builtin_car);
    scheme_env_add_method(scheme, env, "cdr", builtin_cdr);
    scheme_env_add_method(scheme, env, "first", builtin_car);
    scheme_env_add_method(scheme, env, "rest", builtin_cdr);
    scheme_env_add_method(scheme, env, "second", builtin_second);
    scheme_env_add_method(scheme, env, "third", builtin_third);
    scheme_env_add_method(scheme, env, "fourth", builtin_fourth);
    scheme_env_add_method(scheme, env, "apply", builtin_apply);
    scheme_env_add_method(scheme, env, "output", builtin_output);

}

///////////////////////////////////////////////////
// reading
///////////////////////////////////////////////////

static sobject_t *read_tail(scheme_t *scheme, const gt_token_t *opentoken, generic_tokenizer_feeder_t *feeder)
{
    if (generic_tokenizer_feeder_consume(feeder, ")"))
        return NULL; // empty list

    if (generic_tokenizer_feeder_consume(feeder, ".")) {
        sobject_t *obj = scheme_read(scheme, feeder);
        generic_tokenizer_feeder_require(feeder, ")");
        return obj;
    }

    if (generic_tokenizer_feeder_has_next(feeder)) {
        sobject_t *first = scheme_read(scheme, feeder);
        sobject_t *rest = read_tail(scheme, opentoken, feeder);

        return scheme_pair_create(scheme,
                                  first, rest);
    }

    runtime_exception_tok(scheme, opentoken, "Missing closing paren.");
    return NULL;
}

// returns an sobject, not yet evaluated
sobject_t *scheme_read(scheme_t *scheme, generic_tokenizer_feeder_t *feeder)
{
    while (generic_tokenizer_feeder_has_next(feeder)) {

        if (!strcmp(generic_tokenizer_feeder_peek(feeder), ")")) {
            const gt_token_t *tok = generic_tokenizer_feeder_next_token(feeder);
            runtime_exception_tok(scheme, tok, "expected closing paren");
            return NULL;
        }

        if (!strcmp(generic_tokenizer_feeder_peek(feeder), "(")) {
            const gt_token_t *tok = generic_tokenizer_feeder_next_token(feeder);
            return read_tail(scheme, tok, feeder); // closing paren consumed by read_tail
        }

        if (generic_tokenizer_feeder_consume(feeder, "#t")) {
            return SOBJECT_TRUE;
        }

        if (generic_tokenizer_feeder_consume(feeder, "#f")) {
            return SOBJECT_FALSE;
        }

        if (!strcmp(generic_tokenizer_feeder_peek_type(feeder), "NUMBER-HEX")) {
            const char *tok = generic_tokenizer_feeder_next(feeder);

            double v = strtol(&tok[2], NULL, 16);
            return scheme_real_create(scheme, v);
        }

        if (!strcmp(generic_tokenizer_feeder_peek_type(feeder), "STRING")) {
            const char *tok = generic_tokenizer_feeder_next(feeder);
            char *s = strdup(tok); // skip leading and trailing quotation mark
            int inpos = 1, outpos = 0;
            while (inpos+1 < strlen(tok)) {
                switch (tok[inpos]) {
                    case '\\':
                        s[outpos] = tok[inpos+1];
                        if (s[outpos] == 'n')
                            s[outpos] = '\n';
                        if (s[outpos] == 'r')
                            s[outpos] = '\r';
                        outpos++;
                        inpos += 2;
                        break;

                    default:
                        s[outpos++] = tok[inpos++];
                        break;
                }
            }
            s[outpos] = 0;

            // scheme_string_create strdups s
            sobject_t *obj = scheme_string_create(scheme, s);
            free(s);
            return obj;
        }

        if (!strcmp(generic_tokenizer_feeder_peek_type(feeder), "NUMBER")) {
            double v = atof(generic_tokenizer_feeder_next(feeder));
            return scheme_real_create(scheme, v);
        }

        if (generic_tokenizer_feeder_consume(feeder, "'")) {
            return scheme_pair_create(scheme,
                                      scheme_symbol_create(scheme, "quote"),
                                      scheme_pair_create(scheme,
                                                         scheme_read(scheme, feeder),
                                                         NULL));
        }

        if (generic_tokenizer_feeder_consume(feeder, "`")) {
            return scheme_pair_create(scheme,
                                      scheme_symbol_create(scheme, "quasiquote"),
                                      scheme_pair_create(scheme,
                                                         scheme_read(scheme, feeder),
                                                         NULL));
        }

        if (generic_tokenizer_feeder_consume(feeder, ",")) {
            return scheme_pair_create(scheme,
                                      scheme_symbol_create(scheme, "unquote"),
                                      scheme_pair_create(scheme,
                                                         scheme_read(scheme, feeder),
                                                         NULL));
        }

        if (generic_tokenizer_feeder_consume(feeder, ",@")) {
            return scheme_pair_create(scheme,
                                      scheme_symbol_create(scheme, "unquote-splicing"),
                                      scheme_pair_create(scheme,
                                                         scheme_read(scheme, feeder),
                                                         NULL));
        }

        return scheme_symbol_create(scheme, generic_tokenizer_feeder_next(feeder));
    }

    // missing stuff
    return NULL;
}

// caller's responsibility to push env/head if they might be unreachable.
sobject_t *scheme_eval_list(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    if (head == NULL)
        return NULL;

    sobject_t *first = scheme_eval_real(scheme, env, scheme_first(scheme, head));

    // must save 'first' for the call to eval_list below.
    scheme_roots_push(scheme, first);

    sobject_t *rest = scheme_eval_list(scheme, env, scheme_rest(scheme, head));

    scheme_roots_pop(scheme, first);

    return scheme_pair_create(scheme, first, rest);
}

void scheme_gc(scheme_t *scheme)
{
    // When marking, we use an incrementing counter. This saves us
    // from having to set all objects to zero every time. However,
    // when the counter wraps around, we need to reset.
    int32_t MAX_MARK = INT32_MAX-1;

    scheme->gc_mark++;

    if (scheme->gc_mark == MAX_MARK) {
        for (sobject_t *p = scheme->gc_head; p != NULL; p = p->gc_next)
            p->gc_mark = 0;

        scheme->gc_mark = 1;
    }

    for (int idx = 0; idx < scheme->roots_size; idx++) {
        if (scheme->roots[idx])
            scheme->roots[idx]->mark(scheme->roots[idx], scheme->gc_mark);
    }

    // do sweep. For ease of debugging, we first split our list of
    // objects into separate lists of live and dead objects. This
    // allows us to examine the objects we're about to destroy
    // without worrying about them having free'd pointers in them.
    sobject_t *alive_head = NULL;
    sobject_t *p = scheme->gc_head;

    sobject_t *dead_head = NULL;

    while (p != NULL) {
        sobject_t *next = p->gc_next;

        if (p->gc_mark == scheme->gc_mark) {
            p->gc_next = alive_head;
            alive_head = p;
        } else {
            p->gc_next = dead_head;
            dead_head = p;
/*
            fprintf(stderr, "\nDESTROY: ");
            p->to_string(p, stderr);
            fprintf(stderr, "\n");
*/
        }

        p = next;
    }

    scheme->gc_head = alive_head;

    // now delete the dead objects.
    p = dead_head;
    while (p != NULL) {
        sobject_t *next = p->gc_next;
        scheme->gc_bytes_total -= p->gc_bytes;
        scheme->gc_objects_total--;

        p->destroy(p);
        p = next;
    }

    scheme->gc_bytes_after_last_gc = scheme->gc_bytes_total;
}

void scheme_destroy(scheme_t *scheme)
{
    sobject_t *p = scheme->gc_head;
    while (p != NULL) {
        sobject_t *next = p->gc_next;
        scheme->gc_bytes_total -= p->gc_bytes;
        scheme->gc_objects_total--;
        p->destroy(p);
        p = next;
    }

    generic_tokenizer_destroy(scheme->gt);
    free(scheme->roots);
    free(scheme);
}

void X(sobject_t *p)
{
    if (p == NULL)
        fprintf(stderr, "NULL");
    else
        p->to_string(p, stderr);
    fprintf(stderr, "\n");
}

sobject_t *scheme_eval_apply(scheme_t *scheme, sobject_t *env, sobject_t *_target, sobject_t *_valhead)
{
    sobject_t *value = NULL;
    sobject_t *target = _target;
    sobject_t *valhead = _valhead;

    if (SOBJECT_IS_TYPE(target, "METHOD")) {
        value = target->u.method.eval(scheme, env, valhead);
        goto finished;
    }

    if (SOBJECT_IS_TYPE(target, "OBJECT")) {
        env = target->u.object.env;
        target = target->u.object.closure;
        // fall through; we'll take the CLOSURE branch below....
    }

    // handle closure
    if (SOBJECT_IS_TYPE(target, "CLOSURE")) {
        sobject_t *childenv = scheme_env_create(scheme, target->u.closure.env);
        sobject_t *paramhead = target->u.closure.params;

        while (paramhead != NULL) {
            if (!SOBJECT_IS_TYPE(paramhead, "PAIR")) {
                // varargs notation was used
                scheme_env_define(scheme, childenv,
                                  scheme_to_identifier(scheme, paramhead),
                                  valhead);
                break;
            }

            if (valhead == NULL) {
                runtime_exception_obj(scheme, target->u.closure.params, "Expected more parameters in call");
            }

            scheme_env_define(scheme, childenv,
                              scheme_to_identifier(scheme, scheme_first(scheme, paramhead)),
                              scheme_first(scheme, valhead));
            paramhead = scheme_rest(scheme, paramhead);
            valhead = scheme_rest(scheme, valhead);
        }

        scheme_roots_push(scheme, childenv);
        value = scheme_eval_real(scheme, childenv, target->u.closure.body);
        scheme_roots_pop(scheme, childenv);
        goto finished;
    }

    runtime_exception_obj(scheme, target, "don't know how to evaluate object");
    exit(-1);

  finished:
    return value;
}

sobject_t *scheme_eval(scheme_t *scheme, sobject_t *env, sobject_t *expr)
{
    scheme_roots_push(scheme, expr);
    scheme_roots_push(scheme, env);

    sobject_t *value = scheme_eval_real(scheme, env, expr);

    scheme_roots_pop(scheme, env);
    scheme_roots_pop(scheme, expr);

    return value;
}

sobject_t *scheme_eval_real(scheme_t *scheme, sobject_t *_env, sobject_t *_expr)
{
    sobject_t *env = _env, *expr = _expr;

    // when a value should be returned, set this variable and goto finished.
    sobject_t *value = NULL;

  tailrecurse:

    if (1) {
        if (scheme->gc_bytes_total - scheme->gc_bytes_after_last_gc > scheme->gc_heap_growth_trigger) {

            int verbose = 0;
            if (verbose)
                fprintf(stderr, "\n[GC-before nroots %-10d nobjs %-10d bytes %-10d]\n",
                        scheme->roots_size, scheme->gc_objects_total, scheme->gc_bytes_total);
            // XXX garbage collect every time?? consider this stress testing.
            scheme_gc(scheme);

            if (verbose)
                fprintf(stderr, "[GC-after  nroots %-10d nobjs %-10d bytes %-10d]\n",
                        scheme->roots_size, scheme->gc_objects_total, scheme->gc_bytes_total);
        } else {
        }
    }

    if (expr == NULL) {
        value = NULL;
        goto finished;
    }

    if (!strcmp(expr->type, "SYMBOL")) {
        sobject_t *obj = scheme_env_lookup(scheme, env, scheme_to_identifier(scheme, expr));
        if (obj == NULL) {
            // it's legal for a name to be bound to NULL
            value = NULL;
            goto finished;
        }

        value = obj;
        goto finished;
    }

    // handle constants
    if (!SOBJECT_IS_TYPE(expr, "PAIR")) {
        value = expr;
        goto finished;
    }

    // We know it's a pair.
    sobject_t *first = expr->u.pair.first;
    sobject_t *rest = expr->u.pair.rest;

    // handle methods that are "special"--- that operate on the
    // UN-evaluated argument list.
    if (SOBJECT_IS_TYPE(first, "SYMBOL")) {
        const char *s = first->u.string.v;

        if (!strcmp(s, "quote")) {
            value = scheme_first(scheme, rest);
            goto finished;
        }

        // TODO quasiquote

        if (!strcmp(s, "begin")) {

            for ( ; rest != NULL; rest = scheme_rest(scheme, rest)) {
                value = scheme_eval_real(scheme, env, scheme_first(scheme, rest));
            }

            goto finished;
        }

        if (!strcmp(s, "if")) {
            sobject_t *cond = scheme_eval_real(scheme, env, scheme_first(scheme, rest));

            // tail-recurse on the taken branch
            if (scheme_is_true(scheme, cond)) {
                expr = scheme_second(scheme, rest);
            } else if (scheme_rest(scheme, rest)->u.pair.rest != NULL) {
                expr = scheme_third(scheme, rest);
            } else {
                value = cond;
                goto finished;
            }

            goto tailrecurse;
        }

        if (!strcmp(s, "cond")) {
            for (sobject_t *head = rest; head != NULL; head = scheme_rest(scheme, head)) {
                sobject_t *clause = scheme_first(scheme, head);
                sobject_t *cond = scheme_first(scheme, clause);
                sobject_t *body = scheme_second(scheme, clause);

                if ((SOBJECT_IS_TYPE(cond, "SYMBOL") && !strcmp(cond->u.string.v, "else")) ||
                    scheme_is_true(scheme, scheme_eval_real(scheme, env, cond))) {
                    expr = body;
                    goto tailrecurse;
                }
            }

            value = SOBJECT_FALSE;
            goto finished;
        }

        if (!strcmp(s, "case")) {
            sobject_t *key = scheme_eval_real(scheme, env, scheme_first(scheme, rest));

            for (sobject_t *head = scheme_rest(scheme, rest); head != NULL; head = scheme_rest(scheme, head)) {
                sobject_t *datums = scheme_first(scheme, scheme_first(scheme, head));
                sobject_t *body = scheme_second(scheme, scheme_first(scheme, head));

                if (SOBJECT_IS_TYPE(datums, "SYMBOL") && !strcmp(datums->u.string.v, "else")) {
                    expr = body;
                    goto tailrecurse;
                }

                for ( ; datums != NULL; datums = scheme_rest(scheme, datums)) {
                    sobject_t *datum = scheme_first(scheme, datums);
                    if (scheme_eqv(scheme, datum, key)) {
                        expr = body;
                        goto tailrecurse;
                    }
                }
            }

            value = SOBJECT_FALSE;
            goto finished;
        }

        if (!strcmp(s, "define")) {
            sobject_t *p = scheme_first(scheme, rest);
            if (!SOBJECT_IS_TYPE(p, "PAIR")) {
                // (define <variable> <expression>)
                value = scheme_eval_real(scheme, env, scheme_second(scheme, rest));
                scheme_env_define(scheme, env, scheme_to_identifier(scheme, scheme_first(scheme, rest)), value);

                goto finished;

            } else {
                // (define (<variable> <formals>) <body>)
                sobject_t *lambdapair = scheme_pair_create(scheme,
                                                           scheme_symbol_create(scheme, "lambda"),
                                                           scheme_pair_create(scheme,
                                                                              scheme_rest(scheme, scheme_first(scheme, rest)),
                                                                              scheme_rest(scheme, rest)));

                // no need to push-- lambdapair is "live" in scheme_eval call.
                scheme_roots_push(scheme, lambdapair);
                value = scheme_eval_real(scheme, env, lambdapair);
                scheme_roots_pop(scheme, lambdapair);

                scheme_env_define(scheme, env, scheme_to_identifier(scheme, scheme_first(scheme, scheme_first(scheme, rest))), value);
                goto finished;
            }
        }

        if (!strcmp(s, "object-create")) {
            sobject_t *childenv = scheme_env_create(scheme, env);
            sobject_t *bindhead = scheme_first(scheme, rest);
            sobject_t *body = scheme_second(scheme, rest);

            scheme_roots_push(scheme, childenv);

            for ( ; bindhead != NULL; bindhead = scheme_rest(scheme, bindhead)) {
                sobject_t *bind = scheme_first(scheme, bindhead);
                sobject_t *val = scheme_eval_real(scheme, childenv, scheme_second(scheme, bind));
                scheme_env_define(scheme, childenv,
                                  scheme_to_identifier(scheme, scheme_first(scheme, bind)),
                                  val);
            }

            sobject_t *closure = scheme_eval_real(scheme, childenv, body);

            value = scheme_object_create(scheme, childenv, closure);

            scheme_env_define(scheme, childenv, "self", value);

            scheme_roots_pop(scheme, childenv);
            goto finished;
        }

        if (!strcmp(s, "lambda")) {

            sobject_t *beginpair = scheme_pair_create(scheme,
                                                      scheme_symbol_create(scheme, "begin"),
                                                      scheme_rest(scheme, rest));
            value = scheme_closure_create(scheme, env, scheme_first(scheme, rest), beginpair);
            goto finished;
        }

        if (!strcmp(s, "let")) {
            sobject_t *bindhead = scheme_first(scheme, rest);
            sobject_t *body = scheme_rest(scheme, rest);
            sobject_t *childenv = scheme_env_create(scheme, env);

            scheme_roots_push(scheme, childenv);

            // all the bindings are defined in a single
            // 'child' environment, but are evaluted in their
            // own copies of the original environment. Their
            // local definitions should be isolated.
            for ( ; bindhead != NULL; bindhead = scheme_rest(scheme, bindhead)) {
                sobject_t *bind = scheme_first(scheme, bindhead);

                sobject_t *evalenv = scheme_env_create(scheme, env);

                scheme_roots_push(scheme, evalenv);
                sobject_t *val = scheme_eval_real(scheme, evalenv, scheme_second(scheme, bind));
                scheme_roots_pop(scheme, evalenv);

                scheme_env_define(scheme, childenv,
                                  scheme_to_identifier(scheme, scheme_first(scheme, bind)),
                                  val);
            }

            sobject_t *beginpair = scheme_pair_create(scheme,
                                                      scheme_symbol_create(scheme, "begin"),
                                                      body);

            scheme_roots_push(scheme, beginpair);
            value = scheme_eval_real(scheme, childenv, beginpair);
            scheme_roots_pop(scheme, beginpair);

            scheme_roots_pop(scheme, childenv);

            goto finished;
        }

        if (!strcmp(s, "let*")) {
            sobject_t *bindhead = scheme_first(scheme, rest);
            sobject_t *body = scheme_rest(scheme, rest);
            sobject_t *childenv = env;

            // let*: all bindings are evaluated in an
            // environment that contains (only) those bindings
            // previously listed. (The "only" forces us to use
            // a sequence of hierarchical environment frames.)
            for ( ; bindhead != NULL; bindhead = scheme_rest(scheme, bindhead)) {
                sobject_t *bind = scheme_first(scheme, bindhead);

                childenv = scheme_env_create(scheme, childenv);

                scheme_roots_push(scheme, childenv);
                sobject_t *val = scheme_eval_real(scheme, childenv, scheme_second(scheme, bind));
                scheme_roots_pop(scheme, childenv);

                scheme_env_define(scheme, childenv,
                                  scheme_to_identifier(scheme, scheme_first(scheme, bind)),
                                  val);
            }

            sobject_t *beginpair = scheme_pair_create(scheme,
                                                      scheme_symbol_create(scheme, "begin"),
                                                      body);

            scheme_roots_push(scheme, childenv);
            scheme_roots_push(scheme, beginpair);
            value = scheme_eval_real(scheme, childenv, beginpair);
            scheme_roots_pop(scheme, beginpair);
            scheme_roots_pop(scheme, childenv);
            goto finished;
        }

        if (!strcmp(s, "letrec")) {
            sobject_t *childenv = scheme_env_create(scheme, env);
            sobject_t *bindhead = scheme_first(scheme, rest);
            sobject_t *body = scheme_rest(scheme, rest);

            scheme_roots_push(scheme, childenv);

            for ( ; bindhead != NULL; bindhead = scheme_rest(scheme, bindhead)) {
                sobject_t *bind = scheme_first(scheme, bindhead);
                scheme_env_define(scheme, childenv,
                                  scheme_to_identifier(scheme, scheme_first(scheme, bind)),
                                  SOBJECT_FALSE);
            }

            bindhead = scheme_first(scheme, rest);
            for ( ; bindhead != NULL; bindhead = scheme_rest(scheme, bindhead)) {
                sobject_t *bind = scheme_first(scheme, bindhead);
                sobject_t *val = scheme_eval_real(scheme, childenv, scheme_second(scheme, bind));
                scheme_env_define(scheme, childenv,
                                  scheme_to_identifier(scheme, scheme_first(scheme, bind)),
                                  val);
            }

            sobject_t *beginpair = scheme_pair_create(scheme,
                                                      scheme_symbol_create(scheme, "begin"),
                                                      body);

            scheme_roots_push(scheme, beginpair);
            value = scheme_eval_real(scheme, childenv, beginpair);
            scheme_roots_pop(scheme, beginpair);

            scheme_roots_pop(scheme, childenv);
            goto finished;
        }

        if (!strcmp(s, "set!")) {
            sobject_t *name = scheme_first(scheme, rest);
            value = scheme_eval_real(scheme, env, scheme_second(scheme, rest));

            scheme_env_set(scheme, env, scheme_to_identifier(scheme, name), value);
            goto finished;
        }
    }

    // Now, we know it's a method call. Evaluate the first argument so we get the function.
    sobject_t *target = scheme_eval_real(scheme, env, first);

    if (target == NULL) {
        runtime_exception_obj(scheme, first, "unable to resolve object");
        value = NULL;
        goto finished;
    }

    scheme_roots_push(scheme, target);

    sobject_t *evallist = scheme_eval_list(scheme, env, rest);
    scheme_roots_push(scheme, evallist);
    scheme_stacktrace_push(scheme, first, evallist);
    value = scheme_eval_apply(scheme, env, target, evallist);
    scheme_stacktrace_pop(scheme, first, evallist);
    scheme_roots_pop(scheme, evallist);

    scheme_roots_pop(scheme, target);

    goto finished;

  finished:

    return value;
}
