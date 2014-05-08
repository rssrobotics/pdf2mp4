#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <dirent.h>

#include "image_u8x3.h"
#include "string_util.h"
#include "zarray.h"
#include "scheme.h"

/////////////////////////////////////////////////////
// main
int main(int argc, char *argv[])
{
    scheme_t *scheme = scheme_create();

    // note: env belongs to the scheme object. Do not call destroy on
    // it yourself!
    sobject_t *env = scheme_env_create(scheme, NULL);
    scheme_env_setup_basic(scheme, env);

    zarray_t *toks = generic_tokenizer_tokenize_path(scheme->gt, argv[1]);
    generic_tokenizer_feeder_t *feeder = generic_tokenizer_feeder_create(toks);

    while (generic_tokenizer_feeder_has_next(feeder)) {
        // note: expr and eval belong to the scheme object. They will
        // be garbage collected as appropriate, or---at the latest--
        // freed by scheme_destroy.
        sobject_t *expr = scheme_read(scheme, feeder);

        // at this point in time, the C pointer above is the ONLY
        // reference to expr. It will be GC'd if scheme_
        fprintf(stderr, "expr: ");
        expr->to_string(expr, stderr);
        fprintf(stderr, "\n");

        sobject_t *eval = scheme_eval(scheme, env, expr);
        fprintf(stderr, "eval: ");
        eval->to_string(eval, stderr);

        fprintf(stderr, "\n");
    }

    generic_tokenizer_tokens_destroy(toks);
    generic_tokenizer_feeder_destroy(feeder);
    scheme_destroy(scheme);

    SOBJECT_TRUE->destroy(SOBJECT_TRUE);
    SOBJECT_FALSE->destroy(SOBJECT_FALSE);

    return 0;
}
