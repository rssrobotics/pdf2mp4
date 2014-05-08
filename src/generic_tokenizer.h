#ifndef _GENERIC_TOKENIZER_H
#define _GENERIC_TOKENIZER_H

#include "zarray.h"

#ifdef __cplusplus
extern "C" {
#endif

struct gt_token
{
    char *type;
    char *token;
    int line;
    int column;
};

typedef struct gt_token gt_token_t;

typedef struct generic_tokenizer generic_tokenizer_t;

generic_tokenizer_t *generic_tokenizer_create(const char *error_type);
void generic_tokenizer_destroy(generic_tokenizer_t *gt);

void generic_tokenizer_add_escape(generic_tokenizer_t *gt, const char *type, const char *regex);
void generic_tokenizer_add(generic_tokenizer_t *gt, const char *type, const char *regex);
void generic_tokenizer_add_ignore(generic_tokenizer_t *gt, const char *regex);

// returns an array of struct gt_tokens*
zarray_t *generic_tokenizer_tokenize(generic_tokenizer_t *gt, const char *s);
zarray_t *generic_tokenizer_tokenize_path(generic_tokenizer_t *gt, const char *path);
void generic_tokenizer_tokens_destroy(zarray_t *tokens);

void generic_tokenizer_debug(generic_tokenizer_t *gt);

/////////////////////////////////////////////////////////////
// generic_tokenizer_feeder
//
// token feeder class is an easy way to deal with a zarray_t of
// gt_tokens.
//

typedef struct generic_tokenizer_feeder generic_tokenizer_feeder_t;

// You are loaning the 'tokens' (returned from _tokenize) to this
// object. It makes no copy internally.
generic_tokenizer_feeder_t *generic_tokenizer_feeder_create(zarray_t *tokens);
void generic_tokenizer_feeder_destroy(generic_tokenizer_feeder_t *feeder);

int generic_tokenizer_feeder_has_next(generic_tokenizer_feeder_t *feeder);

// you are loaned a pointer that will be valid for the lifetime of the
// underlying zarray_t. Do not free it; it will be freed by
// generic_tokenizer_tokens_destroy, which you should call once the
// feeder is done.)
//
// These two methods return the string for the token.
const char *generic_tokenizer_feeder_next(generic_tokenizer_feeder_t *feeder);
const char *generic_tokenizer_feeder_peek(generic_tokenizer_feeder_t *feeder);
const char *generic_tokenizer_feeder_last(generic_tokenizer_feeder_t *feeder);

// These two methods return the whole token.
const struct gt_token *generic_tokenizer_feeder_next_token(generic_tokenizer_feeder_t *feeder);
const struct gt_token *generic_tokenizer_feeder_peek_token(generic_tokenizer_feeder_t *feeder);
const struct gt_token *generic_tokenizer_feeder_last_token(generic_tokenizer_feeder_t *feeder);
    const char *generic_tokenizer_feeder_peek_type(generic_tokenizer_feeder_t *feeder);

// if the next token matches tok, it is consumed and 1 is
// returned. Else, 0 is returned.
int generic_tokenizer_feeder_consume(generic_tokenizer_feeder_t *feeder, const char *tok);
    int generic_tokenizer_feeder_consume_type(generic_tokenizer_feeder_t *feeder, const char *s);

// the next token must match tok and is consumed. If the token does
// not match, a debugging message is output and exit() is called.
void generic_tokenizer_feeder_require(generic_tokenizer_feeder_t *feeder, const char *tok);

#ifdef __cplusplus
}
#endif

#endif
