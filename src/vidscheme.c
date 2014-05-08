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

static sobject_t *scheme_image_create(scheme_t *scheme, image_u8x3_t *im);

typedef struct image_source image_source_t;
struct image_source
{
    zarray_t *paths;
};

int imin(int a, int b)
{
    return (a < b) ? a : b;
}

int imax(int a, int b)
{
    return (a > b) ? a : b;
}

static int sort_alphanum(const void *_a, const void *_b)
{
    char *a = *((char**) _a);
    char *b = *((char**) _b);

    return strcmp(a, b);
}

image_source_t *image_source_create(const char *path)
{
    image_source_t *is = calloc(1, sizeof(image_source_t));
    is->paths = zarray_create(sizeof(char*));

    char *suffixes[] = { ".ppm", ".pnm", NULL };

    DIR *dir = opendir(path);
    struct dirent *dirent;
    while ((dirent = readdir(dir)) != NULL) {

        int okay = 0;

        for (int idx = 0; suffixes[idx] != NULL; idx++) {
            if (str_ends_with(dirent->d_name, suffixes[idx]))
                okay = 1;
        }

        if (!okay)
            continue;

        char buf[1024];
        snprintf(buf, sizeof(buf), "%s/%s", path, dirent->d_name);
        char *p = strdup(buf);
        zarray_add(is->paths, &p);
    }

    zarray_sort(is->paths, sort_alphanum);

    return is;
}

static int hexchar2int(char c)
{
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= '0' && c <= '9')
        return c - '0';

    assert(0);
    return 0;
}

static void parse_rgb(scheme_t *scheme, sobject_t *s, uint8_t rgbv[3])
{
    SCHEME_TYPE_CHECK(scheme, s, "STRING");
    const char *rgb = s->u.string.v;

    assert(rgb[0] == '#' && strlen(rgb)==7);
    for (int cidx = 0; cidx < 3; cidx++) {
        rgbv[cidx] = hexchar2int(rgb[1+2*cidx])*16 + hexchar2int(rgb[1+2*cidx+1]);
    }
}

////////////////////////////////////////////////////////////
// <image_source>
////////////////////////////////////////////////////////////
static void image_source_mark(sobject_t *obj, int32_t mark)
{
    obj->gc_mark = mark;
}

static void image_source_to_string(sobject_t *s, FILE *out)
{
    image_source_t *is = (image_source_t*) s->u.other.impl;
    fprintf(out, "[image_source %p]", is);
}

static void image_source_destroy(sobject_t *obj)
{
    image_source_t *is = (image_source_t*) obj->u.other.impl;

    zarray_vmap(is->paths, free);
    zarray_destroy(is->paths);
    free(obj);
}

static sobject_t *scheme_image_source_create(scheme_t *scheme, image_source_t *is)
{
    sobject_t *obj = calloc(1, sizeof(sobject_t));
    obj->type = "IMAGE_SOURCE";
    obj->to_string = image_source_to_string;
    obj->mark = image_source_mark;
    obj->destroy = image_source_destroy;
    obj->u.other.impl = is;

    scheme_gc_register(scheme, obj, sizeof(sobject_t));

    return obj;
}

static sobject_t *builtin_image_source_native(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    SCHEME_TYPE_CHECK(scheme, scheme_first(scheme, head), "SYMBOL");
    const char *msg = scheme_first(scheme, head)->u.symbol.v;

    // constructor messages go first.
    if (!strcmp(msg, "create-from-path")) {
        SCHEME_TYPE_CHECK(scheme, scheme_second(scheme, head), "STRING");
        image_source_t *is = image_source_create(scheme_second(scheme, head)->u.string.v);
        return scheme_image_source_create(scheme, is);
    }

    // instance methods always take a second argument, an image.
    SCHEME_TYPE_CHECK(scheme, scheme_second(scheme, head), "IMAGE_SOURCE");
    image_source_t *is = (image_source_t*) scheme_second(scheme, head)->u.other.impl;

    // swallow the image object and message.
    head = scheme_rest(scheme, scheme_rest(scheme, head));

    if (!strcmp(msg, "length"))
        return scheme_real_create(scheme, zarray_size(is->paths));


    if (!strcmp(msg, "get-frame")) {
        SCHEME_TYPE_CHECK(scheme, scheme_first(scheme, head), "REAL");

        int idx = (int) scheme_first(scheme, head)->u.real.v;

        char *path;
        zarray_get(is->paths, idx, &path);
        image_u8x3_t *im = image_u8x3_create_from_pnm(path);

        return scheme_image_create(scheme, im);
    }

    return SOBJECT_FALSE;
}

////////////////////////////////////////////////////////////
// <image>
////////////////////////////////////////////////////////////

static void image_mark(sobject_t *obj, int32_t mark)
{
    obj->gc_mark = mark;
}

static void image_to_string(sobject_t *s, FILE *out)
{
    image_u8x3_t *im = (image_u8x3_t*) s->u.other.impl;
    fprintf(out, "[image_u8x3 %d x %d]", im->width, im->height);
}

static void image_destroy(sobject_t *obj)
{
    image_u8x3_destroy(obj->u.other.impl);
    free(obj);
}

static sobject_t *scheme_image_create(scheme_t *scheme, image_u8x3_t *im)
{
    sobject_t *obj = calloc(1, sizeof(sobject_t));
    obj->type = "IMAGE_U8X3";
    obj->to_string = image_to_string;
    obj->mark = image_mark;
    obj->destroy = image_destroy;
    obj->u.other.impl = im;

    scheme_gc_register(scheme, obj, sizeof(sobject_t) + im->stride*im->height*3);

    return obj;
}

static sobject_t *builtin_image_q(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    if (SOBJECT_IS_TYPE(scheme_first(scheme, head), "IMAGE_U8X3"))
        return SOBJECT_TRUE;
    return SOBJECT_FALSE;
}

static sobject_t *builtin_image_source_q(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
    if (SOBJECT_IS_TYPE(scheme_first(scheme, head), "IMAGE_SOURCE"))
        return SOBJECT_TRUE;
    return SOBJECT_FALSE;
}

static sobject_t *builtin_image_native(scheme_t *scheme, sobject_t *env, sobject_t *head)
{
//    fprintf(stderr, "\nArgs:");
//    head->to_string(head, stderr);
//    fprintf(stderr, "\n");

    SCHEME_TYPE_CHECK(scheme, scheme_first(scheme, head), "SYMBOL");
    const char *msg = scheme_first(scheme, head)->u.symbol.v;

    // constructor messages go first.
    if (!strcmp(msg, "create-from-file")) {
        SCHEME_TYPE_CHECK(scheme, scheme_second(scheme, head), "STRING");
        image_u8x3_t *im = image_u8x3_create_from_pnm(scheme_second(scheme, head)->u.string.v);
        return scheme_image_create(scheme, im);
    }

    if (!strcmp(msg, "create")) {
        SCHEME_TYPE_CHECK(scheme, scheme_second(scheme, head), "REAL");
        SCHEME_TYPE_CHECK(scheme, scheme_third(scheme, head), "REAL");
        SCHEME_TYPE_CHECK(scheme, scheme_fourth(scheme, head), "STRING");

        int width = (int) scheme_second(scheme, head)->u.real.v;
        int height = (int) scheme_third(scheme, head)->u.real.v;
        uint8_t rgb[3];
        parse_rgb(scheme, scheme_fourth(scheme, head), rgb);

        image_u8x3_t *im = image_u8x3_create(width, height);
        for (int x = 0; x < im->width; x++)
            memcpy(&im->buf[3*x], rgb, 3);

        for (int y = 1; y < im->height; y++)
            memcpy(&im->buf[y*im->stride], &im->buf[0], 3*im->width);

        return scheme_image_create(scheme, im);
    }

    // instance methods always take a second argument, an image.
    SCHEME_TYPE_CHECK(scheme, scheme_second(scheme, head), "IMAGE_U8X3");
    image_u8x3_t *im = (image_u8x3_t*) scheme_second(scheme, head)->u.other.impl;

    // swallow the image object and message.
    head = scheme_rest(scheme, scheme_rest(scheme, head));

    if (!strcmp(msg, "get-width"))
        return scheme_real_create(scheme, im->width);

    if (!strcmp(msg, "get-height"))
        return scheme_real_create(scheme, im->height);

    if (!strcmp(msg, "write-pnm")) {
        SCHEME_TYPE_CHECK(scheme, scheme_first(scheme, head), "STRING");
        image_u8x3_write_pnm(im, scheme_first(scheme, head)->u.string.v);
        return SOBJECT_TRUE;
    }

    if (1 && !strcmp(msg, "output")) {
        fprintf(stdout, "P6\n%d %d\n255\n", im->width, im->height);
        int linesz = im->width * 3;
        for (int y = 0; y < im->height; y++) {
            if (linesz != fwrite(&im->buf[y*im->stride], 1, linesz, stdout)) {
                assert(0);
            }
        }

        fflush(stdout);
        return SOBJECT_TRUE;
    }

    if (!strcmp(msg, "draw-pixel")) {
        SCHEME_TYPE_CHECK(scheme, scheme_first(scheme, head), "REAL");
        SCHEME_TYPE_CHECK(scheme, scheme_second(scheme, head), "REAL");
        SCHEME_TYPE_CHECK(scheme, scheme_third(scheme, head), "REAL");
        SCHEME_TYPE_CHECK(scheme, scheme_fourth(scheme, head), "REAL");
        SCHEME_TYPE_CHECK(scheme, scheme_fifth(scheme, head), "REAL");

        int x = scheme_first(scheme, head)->u.real.v;
        int y = scheme_second(scheme, head)->u.real.v;
        int r = scheme_third(scheme, head)->u.real.v;
        int g = scheme_fourth(scheme, head)->u.real.v;
        int b = scheme_fifth(scheme, head)->u.real.v;

        if (x < 0 || x >= im->width || y < 0 || y >= im->height)
            return SOBJECT_FALSE;

        im->buf[y*im->stride + 3*x + 0] = r;
        im->buf[y*im->stride + 3*x + 1] = g;
        im->buf[y*im->stride + 3*x + 2] = b;
        return SOBJECT_TRUE;
    }

    if (!strcmp(msg, "clone")) {
        image_u8x3_t *im2 = image_u8x3_create(im->width, im->height);
        for (int y = 0; y < im->height; y++)
            memcpy(&im2->buf[im2->stride*y],&im->buf[im->stride*y], im->width*3);
        return scheme_image_create(scheme, im2);
    }

    if (!strcmp(msg, "scale")) {
        SCHEME_TYPE_CHECK(scheme, scheme_first(scheme, head), "REAL");
        SCHEME_TYPE_CHECK(scheme, scheme_second(scheme, head), "REAL");
        int swidth = scheme_first(scheme, head)->u.real.v;
        int sheight = scheme_second(scheme, head)->u.real.v;

        if (swidth < 0)
            swidth = im->width * sheight / im->height;
        if (sheight < 0)
            sheight = im->height * swidth / im->width;

        if (swidth == im->width && sheight == im->height)
            return scheme_image_create(scheme, image_u8x3_copy(im));

        int SUPERSAMPLE = 3;

        // for quality, first do nearest-neighbor sampling to an image
        // SUPERSAMPLE times twice as big as we actually want.
        image_u8x3_t *s2 = image_u8x3_create(swidth*SUPERSAMPLE, sheight*SUPERSAMPLE);

        for (int s2y = 0; s2y < s2->height; s2y++) {
            for (int s2x = 0; s2x < s2->width; s2x++) {
                int y = s2y * im->height / s2->height;
                int x = s2x * im->width / s2->width;

                if (y >= im->height)
                    y = im->height - 1;
                if (x >= im->width)
                    x = im->width - 1;

                for (int cidx = 0; cidx < 3; cidx++)
                    s2->buf[s2y*s2->stride + 3*s2x + cidx] = im->buf[y*im->stride + 3*x + cidx];
            }
        }

        // now down sample
        image_u8x3_t *s = image_u8x3_create(swidth, sheight);
        for (int sy = 0; sy < s->height; sy++) {
            for (int sx = 0; sx < s->width; sx++) {
                for (int cidx = 0; cidx < 3; cidx++) {
                    int acc = 0;

                    for (int dy = 0; dy < SUPERSAMPLE; dy++) {
                        for (int dx = 0; dx < SUPERSAMPLE; dx++) {
                            acc += s2->buf[(SUPERSAMPLE*sy + dy)*s2->stride + 3*(SUPERSAMPLE*sx + dx) + cidx];
                        }
                    }

                    s->buf[sy*s->stride + 3*sx + cidx] = acc / (SUPERSAMPLE * SUPERSAMPLE);
                }
            }
        }

        image_u8x3_destroy(s2);
        return scheme_image_create(scheme, s);
    }

    if (!strcmp(msg, "draw-image")) {
        SCHEME_TYPE_CHECK(scheme, scheme_first(scheme, head), "REAL");
        SCHEME_TYPE_CHECK(scheme, scheme_second(scheme, head), "REAL");
        SCHEME_TYPE_CHECK(scheme, scheme_third(scheme, head), "OBJECT");

        int x0 = (int) scheme_first(scheme, head)->u.real.v;
        int y0 = (int) scheme_second(scheme, head)->u.real.v;
        sobject_t *tmp = scheme_env_lookup(scheme, scheme_third(scheme, head)->u.object.env, "_im");

        assert(tmp != NULL);

        image_u8x3_t *out = image_u8x3_copy(im);
        image_u8x3_t *im2 = (image_u8x3_t*) tmp->u.other.impl;

        int x2min = imax(0, -x0);
        int x2max = imin(im2->width, im->width-x0);

        for (int y2 = 0; y2 < im2->height; y2++) {

            int y = y0 + y2;
            if (y < 0 || y >= im->height)
                continue;

/*
            for (int x2 = 0; x2 < im2->width; x2++) {
                int x = x0 + x2;

                if (x < 0 || x >= im->width)
                    continue;

                assert(x2 >= x2min && x2 < x2max);

                for (int cidx = 0; cidx < 3; cidx++)
                    out->buf[y*im->stride + 3*x + cidx] = im2->buf[y2*im2->stride + 3*x2 + cidx];
            }
*/
            int x = x0 + x2min;
            memcpy(&out->buf[y*im->stride + 3*x], &im2->buf[y2*im2->stride + 3*x2min], (x2max-x2min)*3);
        }

        return scheme_image_create(scheme, out);
    }

    if (!strcmp(msg, "multiply")) {
        SCHEME_TYPE_CHECK(scheme, scheme_first(scheme, head), "REAL");

        double frac = scheme_first(scheme, head)->u.real.v;

        image_u8x3_t *im2 = image_u8x3_copy(im);
        for (int y = 0; y < im2->height; y++)
            for (int x = 0; x < im2->width; x++)
                for (int cidx = 0; cidx < 3; cidx++)
                    im2->buf[y*im2->stride+3*x+cidx] *= frac;

        return scheme_image_create(scheme, im2);
    }

    if (!strcmp(msg, "add")) {
        SCHEME_TYPE_CHECK(scheme, scheme_first(scheme, head), "OBJECT");

        sobject_t *tmp = scheme_env_lookup(scheme, scheme_first(scheme, head)->u.object.env, "_im");
        image_u8x3_t *im2 = (image_u8x3_t*) tmp->u.other.impl;
        image_u8x3_t *out = image_u8x3_copy(im);

        for (int y = 0; y < im->height; y++) {
            for (int x = 0; x < im->width; x++) {
                for (int cidx = 0; cidx < 3; cidx++)
                    out->buf[y*out->stride+3*x+cidx] +=
                        im2->buf[y*im2->stride+3*x+cidx];
            }
        }
        return scheme_image_create(scheme, out);
    }

    return SOBJECT_FALSE;
}

int main(int argc, char *argv[])
{
    scheme_t *scheme = scheme_create();
    sobject_t *env = scheme_env_create(scheme, NULL);
    scheme_env_setup_basic(scheme, env);

    scheme_env_add_method(scheme, env, "image-source-native", builtin_image_source_native);
    scheme_env_add_method(scheme, env, "image-native", builtin_image_native);
    scheme_env_add_method(scheme, env, "image?", builtin_image_q);
    scheme_env_add_method(scheme, env, "image-source?", builtin_image_source_q);

    zarray_t *toks = generic_tokenizer_tokenize_path(scheme->gt, argv[1]);
    generic_tokenizer_feeder_t *feeder = generic_tokenizer_feeder_create(toks);

    while (generic_tokenizer_feeder_has_next(feeder)) {
        // note: expr and eval belong to the scheme object. They will
        // be garbage collected as appropriate, or---at the latest--
        // freed by scheme_destroy.
        sobject_t *expr = scheme_read(scheme, feeder);

        // at this point in time, the C pointer above is the ONLY
        // reference to expr. It will be GC'd if scheme_
/*        fprintf(stderr, "expr: ");
        expr->to_string(expr, stderr);
        fprintf(stderr, "\n");
*/
        sobject_t *eval = scheme_eval(scheme, env, expr);

        (void) eval;

        /*      fprintf(stderr, "eval: ");
        eval->to_string(eval, stderr);

        fprintf(stderr, "\n");
*/
    }

    generic_tokenizer_tokens_destroy(toks);
    generic_tokenizer_feeder_destroy(feeder);
    scheme_destroy(scheme);

    SOBJECT_TRUE->destroy(SOBJECT_TRUE);
    SOBJECT_FALSE->destroy(SOBJECT_FALSE);

    return 0;
}
