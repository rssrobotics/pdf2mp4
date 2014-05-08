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

/*
pdftoppm -scale-to-y 1080 original.pdf original

avconv  -i botlab_w13_pick_2.mp4 -f image2 -s 960x540 movie%04d.ppm

./vidscript vid.scheme | avconv -y -f image2pipe -vcodec ppm -i - -b 10M -r 30 foo.mp4

*/

static sobject_t *builtin_vs_transition_null(scheme_t *scheme, scheme_env_t *env, sobject_t *head);

/////////////////////////////////////////////////////
// core vstream stuff
typedef struct vstream vstream_t;
struct vstream
{
    // human-readable identifier
    char *name;

    // produce the next frame. caller frees image. returns NULL when
    // stream is empty.
    image_u8x3_t* (*next_image)(vstream_t *vs);

    // can be read by anyone... a stream is "used up" once.
    int nframes_remaining;

    void *impl;
};

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

static void parse_rgb(sobject_t *s, uint8_t rgbv[3])
{
    assert(SOBJECT_IS_TYPE(s, "STRING"));
    const char *rgb = s->u.string.v;

    assert(rgb[0] == '#' && strlen(rgb)==7);
    for (int cidx = 0; cidx < 3; cidx++) {
        rgbv[cidx] = hexchar2int(rgb[1+2*cidx])*16 + hexchar2int(rgb[1+2*cidx+1]);
    }

}

/////////////////////////////////////////////////////
// scheme vstream object type
static void vs_to_string(sobject_t *s, FILE *out)
{
    vstream_t *vs = (vstream_t*) s->u.other.impl;
    fprintf(out, "%s", vs->name);
}

static sobject_t* scheme_vs_create(scheme_t *scheme, vstream_t *vs)
{
    sobject_t *obj = calloc(1, sizeof(sobject_t));
    obj->to_string = vs_to_string;
    obj->type = "VSTREAM";
    obj->u.other.impl = vs;

    return obj;
}

/////////////////////////////////////////////////////
// vs-output implementation
// (vs-output vstream) ==> #t (on success)
static sobject_t *builtin_vs_output(scheme_t *scheme, scheme_env_t *env, sobject_t *head)
{
    assert(SOBJECT_IS_TYPE(scheme_first(head), "VSTREAM"));

    vstream_t *vs = (vstream_t*) scheme_first(head)->u.other.impl;

    for (int nframes = 0; 1; nframes++) {
        image_u8x3_t *im = vs->next_image(vs);
        if (im == NULL)
            break;

        if (1) {
            fprintf(stdout, "P6\n%d %d\n255\n", im->width, im->height);
            int linesz = im->width * 3;
            for (int y = 0; y < im->height; y++) {
                if (linesz != fwrite(&im->buf[y*im->stride], 1, linesz, stdout)) {
                    assert(0);
                }
            }
            fflush(stdout);
            fprintf(stderr, "frame %4d / %4d (%d x %d)\n", nframes, vs->nframes_remaining, im->width, im->height);
        }

        image_u8x3_destroy(im);
    }

    return SOBJECT_TRUE;
}

/////////////////////////////////////////////////////
// vstream_solid implementation
typedef struct vstream_solid vstream_solid_impl_t;
struct vstream_solid
{
    uint8_t rgb[3];
    int width, height;
};

static image_u8x3_t* vs_solid_next_image(vstream_t *vs)
{
    vstream_solid_impl_t *impl = (vstream_solid_impl_t*) vs->impl;

    if (vs->nframes_remaining == 0)
        return NULL;

    image_u8x3_t *im = image_u8x3_create(impl->width, impl->height);
    for (int y = 0; y < im->height; y++) {
        for (int x = 0; x < im->width; x++) {
            for (int cidx = 0; cidx < 3; cidx++)  {
                im->buf[y*im->stride + 3*x + cidx] = impl->rgb[cidx];
            }
        }
    }

    vs->nframes_remaining --;

    return im;
}

// (vs-solid width height "#ff00ff" nframes)
static sobject_t *builtin_vs_solid(scheme_t *scheme, scheme_env_t *env, sobject_t *head)
{
    vstream_solid_impl_t *impl = calloc(1, sizeof(vstream_solid_impl_t));

    vstream_t *vs = calloc(1, sizeof(vstream_t));
    vs->name = "vs_solid";
    vs->next_image = vs_solid_next_image;
    vs->impl = impl;

    assert(SOBJECT_IS_TYPE(scheme_first(head), "REAL"));
    assert(SOBJECT_IS_TYPE(scheme_second(head), "REAL"));
    assert(SOBJECT_IS_TYPE(scheme_third(head), "STRING"));
    assert(SOBJECT_IS_TYPE(scheme_fourth(head), "REAL"));
    impl->width  = (int) scheme_first(head)->u.real.v;
    impl->height = (int) scheme_second(head)->u.real.v;
    parse_rgb(scheme_third(head), impl->rgb);

    vs->nframes_remaining = (int) scheme_fourth(head)->u.real.v;

    sobject_t *obj = scheme_vs_create(scheme, vs);
    return obj;
}

/////////////////////////////////////////////////////
// vs-still implementation
typedef struct vstream_still vstream_still_impl_t;
struct vstream_still
{
    image_u8x3_t *im;
};

static image_u8x3_t* vs_still_next_image(vstream_t *vs)
{
    vstream_still_impl_t *impl = (vstream_still_impl_t*) vs->impl;

    if (vs->nframes_remaining == 0)
        return NULL;

    image_u8x3_t *im = image_u8x3_create(impl->im->width, impl->im->height);
    for (int y = 0; y < im->height; y++)
        memcpy(&im->buf[y*im->stride], &impl->im->buf[y*impl->im->stride], 3 * impl->im->width);

    vs->nframes_remaining --;
    return im;
}

// (vs-still "path" nframes)
static sobject_t *builtin_vs_still(scheme_t *scheme, scheme_env_t *env, sobject_t *head)
{
    vstream_still_impl_t *impl = calloc(1, sizeof(vstream_still_impl_t));

    vstream_t *vs = calloc(1, sizeof(vstream_t));
    vs->name = "vs_still";
    vs->next_image = vs_still_next_image;
    vs->impl = impl;

    assert(SOBJECT_IS_TYPE(scheme_first(head), "STRING"));
    assert(SOBJECT_IS_TYPE(scheme_second(head), "REAL"));

    impl->im = image_u8x3_create_from_pnm(scheme_first(head)->u.string.v);

    vs->nframes_remaining = (int) scheme_second(head)->u.real.v;

    sobject_t *obj = scheme_vs_create(scheme, vs);
    return obj;
}

/////////////////////////////////////////////////////
// vs-transition implementation
typedef struct vstream_transition vstream_transition_t;
struct vstream_transition
{
    // transition should produce frames up through the end of the transition (it should
    // not "deplete" b). Just returns NULL when the transition is ended.
    image_u8x3_t* (*transition)(vstream_transition_t *vt, vstream_t *a, vstream_t *b);

    char *name; // human readable
    int nframes; // most implementations will use this; duration of overlap
    void *impl;
};

/////////////////////////////////////////////////////
// scheme vstream_transition object type
static void vs_transition_to_string(sobject_t *s, FILE *out)
{
    vstream_transition_t *vt = (vstream_transition_t*) s->u.other.impl;
    fprintf(out, "%s", vt->name);
}

static sobject_t* scheme_vs_transition_create(scheme_t *scheme, vstream_transition_t *vt)
{
    sobject_t *obj = calloc(1, sizeof(sobject_t));
    obj->to_string = vs_transition_to_string;
    obj->type = "VSTREAM-TRANSITION";
    obj->u.other.impl = vt;

    return obj;
}

/////////////////////////////////////////////////////
// vs-concat
typedef struct vstream_concat vstream_concat_impl_t;
struct vstream_concat
{
    sobject_t *head;
};

static image_u8x3_t* vs_concat_next_image(vstream_t *vs)
{
    vstream_concat_impl_t *impl = (vstream_concat_impl_t*) vs->impl;

    while (1) {
        if (impl->head == NULL)
            return NULL;

        assert(SOBJECT_IS_TYPE(scheme_first(impl->head), "VSTREAM"));
        vstream_t *vs0 = (vstream_t*) scheme_first(impl->head)->u.other.impl;

        // handle case where there's no transition (i.e., the last one)
        if (scheme_rest(impl->head) == NULL) {
            image_u8x3_t *im = vs0->next_image(vs0);
            return im;
        }

        vstream_transition_t *vt = (vstream_transition_t*) scheme_second(impl->head)->u.other.impl;
        vstream_t *vs1 = (vstream_t*) scheme_third(impl->head)->u.other.impl;

        image_u8x3_t *im = vt->transition(vt, vs0, vs1);
        if (im == NULL) {
            impl->head = scheme_rest(scheme_rest(impl->head));
            continue;
        }

        return im;
    }
}

// (vs-concat <stream1> [transition] <stream2> [transition] <stream3> ....)
static sobject_t *builtin_vs_concat(scheme_t *scheme, scheme_env_t *env, sobject_t *head)
{
    vstream_concat_impl_t *impl = calloc(1, sizeof(vstream_concat_impl_t));

    vstream_t *vs = calloc(1, sizeof(vstream_t));
    vs->name = "vs_concat";
    vs->next_image = vs_concat_next_image;
    vs->impl = impl;

    impl->head = head;

    for (sobject_t *tmp = head; tmp != NULL; tmp = scheme_rest(tmp)) {
        tmp->to_string(tmp, stderr);

        assert(SOBJECT_IS_TYPE(scheme_first(tmp), "VSTREAM"));
        vs->nframes_remaining += ((vstream_t*) scheme_first(tmp)->u.other.impl)->nframes_remaining;

        if (scheme_rest(tmp) != NULL) {

            if (!SOBJECT_IS_TYPE(scheme_second(tmp), "VSTREAM-TRANSITION")) {
                // no transition specified... insert one.
                sobject_t *transition = builtin_vs_transition_null(scheme, env, NULL);
                sobject_t *pair = scheme_pair_create(scheme, transition, scheme_rest(tmp));
                tmp->u.pair.rest = pair;
            }

            // skip over the transition
            // adjust frames remaining: the transition takes time from multiple sequences
            vs->nframes_remaining -= ((vstream_transition_t*) scheme_second(tmp)->u.other.impl)->nframes;
            tmp = scheme_rest(tmp);
        }
    }

    return scheme_vs_create(scheme, vs);
}

// (vs-concat-list (<stream1> [transition] <stream2> [transition] <stream3> ....))
static sobject_t *builtin_vs_concat_list(scheme_t *scheme, scheme_env_t *env, sobject_t *head)
{
    vstream_concat_impl_t *impl = calloc(1, sizeof(vstream_concat_impl_t));

    vstream_t *vs = calloc(1, sizeof(vstream_t));
    vs->name = "vs_concat";
    vs->next_image = vs_concat_next_image;
    vs->impl = impl;

    head = scheme_first(head);
    impl->head = head;

    for (sobject_t *tmp = head; tmp != NULL; tmp = scheme_rest(tmp)) {

        assert(SOBJECT_IS_TYPE(scheme_first(tmp), "VSTREAM"));
        vs->nframes_remaining += ((vstream_t*) scheme_first(tmp)->u.other.impl)->nframes_remaining;

        if (scheme_rest(tmp) != NULL) {

            if (!SOBJECT_IS_TYPE(scheme_second(tmp), "VSTREAM-TRANSITION")) {
                // no transition specified... insert one.
                sobject_t *transition = builtin_vs_transition_null(scheme, env, NULL);
                sobject_t *pair = scheme_pair_create(scheme, transition, scheme_rest(tmp));
                tmp->u.pair.rest = pair;
            }

            // skip over the transition
            // adjust frames remaining: the transition takes time from multiple sequences
            vs->nframes_remaining -= ((vstream_transition_t*) scheme_second(tmp)->u.other.impl)->nframes;
            tmp = scheme_rest(tmp);
        }
    }

    return scheme_vs_create(scheme, vs);
}

/////////////////////////////////////////////////////
// vs-matte implementation
typedef struct vstream_matte vstream_matte_impl_t;
struct vstream_matte
{
    vstream_t *vs;
    int width, height;
    uint8_t rgb[3]; // matte color
    int rescale;
};

static image_u8x3_t* vs_matte_next_image(vstream_t *vs)
{
    vstream_matte_impl_t *impl = (vstream_matte_impl_t*) vs->impl;

    if (impl->vs->nframes_remaining == 0)
        return NULL;

    image_u8x3_t *src = impl->vs->next_image(impl->vs);

    image_u8x3_t *dst = image_u8x3_create(impl->width, impl->height);
    for (int y = 0; y < dst->height; y++) {
        for (int x = 0; x < dst->width; x++) {
            for (int cidx = 0; cidx < 3; cidx++)  {
                dst->buf[y*dst->stride + 3*x + cidx] = impl->rgb[cidx];
            }
        }
    }

    assert(impl->rescale == 0); // scaling not implemented

    int ox = (dst->width - src->width) / 2.0;
    int oy = (dst->height - src->height) / 2.0;
    for (int srcy = 0; srcy < src->height; srcy++) {
        for (int srcx = 0; srcx < src->width; srcx++) {
            int dstx = srcx + ox;
            int dsty = srcy + oy;

            if (dstx < 0 || dsty < 0 || dstx >= dst->width || dsty >= dst->height)
                continue;

            memcpy(&dst->buf[dsty*dst->stride + 3*dstx], &src->buf[srcy*src->stride + 3*srcx], 3);
        }
    }

    image_u8x3_destroy(src);
    vs->nframes_remaining--;
    return dst;
}

// MATTE: take the input stream and composite it on a background of constant
// size and color. The input stream can optionally be rescaled so that it
// fills the output image as much as possible (preserving aspect ratio).
//
// (vs-matte width height "#ff00ff" <rescale=0|1> <stream>)
static sobject_t *builtin_vs_matte(scheme_t *scheme, scheme_env_t *env, sobject_t *head)
{
    vstream_matte_impl_t *impl = calloc(1, sizeof(vstream_matte_impl_t));

    vstream_t *vs = calloc(1, sizeof(vstream_t));
    vs->name = "vs_matte";
    vs->next_image = vs_matte_next_image;
    vs->impl = impl;

    assert(SOBJECT_IS_TYPE(scheme_first(head), "REAL"));
    assert(SOBJECT_IS_TYPE(scheme_second(head), "REAL"));
    assert(SOBJECT_IS_TYPE(scheme_third(head), "STRING"));
    assert(SOBJECT_IS_TYPE(scheme_fourth(head), "REAL"));
    assert(SOBJECT_IS_TYPE(scheme_fifth(head), "VSTREAM"));

    impl->width  = (int) scheme_first(head)->u.real.v;
    impl->height = (int) scheme_second(head)->u.real.v;
    parse_rgb(scheme_third(head), impl->rgb);

    impl->rescale = (int) scheme_fourth(head)->u.real.v;
    impl->vs = (vstream_t*) scheme_fifth(head)->u.other.impl;

    vs->nframes_remaining = impl->vs->nframes_remaining;

    sobject_t *obj = scheme_vs_create(scheme, vs);
    return obj;
}

/////////////////////////////////////////////////////
// vs_progress_bar

typedef struct vstream_progress_bar vstream_progress_bar_impl_t;
struct vstream_progress_bar
{
    vstream_t *vs;
    int height;
    int y; // position of time bar (negative values map to bottom)
    int count; // how many frames have we shown so far?
    int total; // how many frames total?
    uint8_t rgb0[3], rgb1[3];
};

static image_u8x3_t* vs_progress_bar_next_image(vstream_t *vs)
{
    vstream_progress_bar_impl_t *impl = (vstream_progress_bar_impl_t*) vs->impl;

    if (impl->vs->nframes_remaining == 0)
        return NULL;

    image_u8x3_t *im = impl->vs->next_image(impl->vs);

    // where should we transition?
    int timebar_mark = impl->count * im->width / impl->total;

    // alpha controls the width of the transition region; small alphas
    // slow things down.
    // make the transition area scale with the amount of motion we
    // produce with every frame; approximate a motion blur.
    double alpha = 1.0 / (im->width / impl->total);

    for (int dy = 0; dy < impl->height; dy++) {
        for (int x = 0; x < im->width; x++) {
            double frac = 1.0 / (1.0 + expf(-alpha*(x - timebar_mark)));
            int y = impl->y + dy;
            if (y < 0)
                y += im->height;
            uint8_t *c = &im->buf[y*im->stride + 3*x];

            for (int cidx = 0; cidx < 3; cidx++)
                c[cidx] = impl->rgb0[cidx]*(1-frac)
                    + impl->rgb1[cidx]*(frac);

        }
    }

    impl->count++;
    vs->nframes_remaining--;
    return im;
}

// (vs-progress_bar "#rgb0" "#rgb1" height y <stream>)
static sobject_t *builtin_vs_progress_bar(scheme_t *scheme, scheme_env_t *env, sobject_t *head)
{
    vstream_progress_bar_impl_t *impl = calloc(1, sizeof(vstream_progress_bar_impl_t));

    vstream_t *vs = calloc(1, sizeof(vstream_t));
    vs->name = "vs_progress_bar";
    vs->next_image = vs_progress_bar_next_image;
    vs->impl = impl;

    assert(SOBJECT_IS_TYPE(scheme_first(head), "STRING"));
    assert(SOBJECT_IS_TYPE(scheme_second(head), "STRING"));
    assert(SOBJECT_IS_TYPE(scheme_third(head), "REAL"));
    assert(SOBJECT_IS_TYPE(scheme_fourth(head), "REAL"));
    assert(SOBJECT_IS_TYPE(scheme_fifth(head), "VSTREAM"));

    parse_rgb(scheme_first(head), impl->rgb0);
    parse_rgb(scheme_second(head), impl->rgb1);
    impl->height = (int) scheme_third(head)->u.real.v;
    impl->y = (int) scheme_fourth(head)->u.real.v;
    impl->vs = (vstream_t*) scheme_fifth(head)->u.other.impl;
    impl->count = 0;
    impl->total = impl->vs->nframes_remaining;
    vs->nframes_remaining = impl->vs->nframes_remaining;

    sobject_t *obj = scheme_vs_create(scheme, vs);
    return obj;
}

/////////////////////////////////////////////////////
// vs_images
typedef struct vstream_images_impl vstream_images_impl_t;
struct vstream_images_impl
{
    int pos;
    zarray_t *paths;
};

static image_u8x3_t *vs_images_next_image(vstream_t *vs)
{
    vstream_images_impl_t *impl = (vstream_images_impl_t*) vs->impl;

    if (impl->pos == zarray_size(impl->paths))
        return NULL;

    char *path;
    zarray_get(impl->paths, impl->pos, &path);
    image_u8x3_t *im = image_u8x3_create_from_pnm(path);
    impl->pos++;
    vs->nframes_remaining --;
    return im;
}

static int sort_alphanum(const void *_a, const void *_b)
{
    char *a = *((char**) _a);
    char *b = *((char**) _b);

    return strcmp(a, b);
}

static sobject_t *builtin_vs_images(scheme_t *scheme, scheme_env_t *env, sobject_t *head)
{
    vstream_images_impl_t *impl = calloc(1, sizeof(vstream_images_impl_t));
    impl->paths = zarray_create(sizeof(char*));

    vstream_t *vs = calloc(1, sizeof(vstream_t));
    vs->name = "vs_images";
    vs->next_image = vs_images_next_image;
    vs->impl = impl;

    assert(SOBJECT_IS_TYPE(scheme_first(head), "STRING"));

    char *suffixes[] = { ".ppm", ".pnm", NULL };

    DIR *dir = opendir(scheme_first(head)->u.string.v);
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
        snprintf(buf, sizeof(buf), "%s/%s", scheme_first(head)->u.string.v, dirent->d_name);
        char *p = strdup(buf);
        zarray_add(impl->paths, &p);
    }

    zarray_sort(impl->paths, sort_alphanum);
/*
    for (int i = 0; i < zarray_size(impl->paths); i++) {
        char *p;
        zarray_get(impl->paths, i, &p);
        printf("path %s\n", p);
    }
*/
    vs->nframes_remaining = zarray_size(impl->paths);

    sobject_t *obj = scheme_vs_create(scheme, vs);
    return obj;
}

/////////////////////////////////////////////////////
// vs_overlay
//
// (vs_overlay <waitbackground> <backgroundstream>  ((xoff yoff wait <vstream> )
//                                                  (xoff yoff wait <vstream> )))

struct vstream_overlay_record
{
    int xoff, yoff;

    vstream_t *vs;
};

typedef struct vstream_overlay_impl vstream_overlay_impl_t;
struct vstream_overlay_impl
{
    vstream_t *vs;

    zarray_t *records;
};

static image_u8x3_t *vs_overlay_next_image(vstream_t *vs)
{
    vstream_overlay_impl_t *impl = (vstream_overlay_impl_t*) vs->impl;

    if (vs->nframes_remaining == 0)
        return NULL;

    image_u8x3_t *dst = impl->vs->next_image(impl->vs);
    assert(dst != NULL); // background isn't allowed to be short

    vs->nframes_remaining--;
    for (int i = 0; i < zarray_size(impl->records); i++) {
        struct vstream_overlay_record rec;
        zarray_get(impl->records, i, &rec);

        image_u8x3_t *src = rec.vs->next_image(rec.vs);
        if (src == NULL)
            continue;

        for (int srcy = 0; srcy < src->height; srcy++) {
            for (int srcx = 0; srcx < src->width; srcx++) {
                int dsty = srcy + rec.yoff;
                int dstx = srcx + rec.xoff;

                if (dstx < 0 || dsty < 0 || dstx >= dst->width || dsty >= dst->height) {
                    continue;
                }

                uint8_t *sp = &src->buf[srcy*src->stride + 3*srcx];
                uint8_t *dp = &dst->buf[dsty*dst->stride + 3*dstx];
                memcpy(dp, sp, 3);
            }
        }

        image_u8x3_destroy(src);
    }

    return dst;
}

static int imax(int a, int b)
{
    return (a > b) ? a : b;
}

static sobject_t *builtin_vs_overlay(scheme_t *scheme, scheme_env_t *env, sobject_t *head)
{
    vstream_overlay_impl_t *impl = calloc(1, sizeof(vstream_overlay_impl_t));
    impl->records = zarray_create(sizeof(struct vstream_overlay_record));

    vstream_t *vs = calloc(1, sizeof(vstream_t));
    vs->name = "vs_overlay";
    vs->next_image = vs_overlay_next_image;
    vs->impl = impl;
    vs->nframes_remaining = 0;

    assert(SOBJECT_IS_TYPE(scheme_first(head),"REAL"));
    assert(SOBJECT_IS_TYPE(scheme_second(head), "VSTREAM"));
    assert(SOBJECT_IS_TYPE(scheme_third(head), "PAIR"));

    impl->vs = (vstream_t*) scheme_second(head)->u.other.impl;

    if (scheme_first(head)->u.real.v)
        vs->nframes_remaining = imax(vs->nframes_remaining, impl->vs->nframes_remaining);

    for (sobject_t *list = scheme_third(head); list != NULL; list = scheme_rest(list)) {
        sobject_t *entry = scheme_first(list);

        fprintf(stderr, "List:\n");
        list->to_string(list, stderr);
        fprintf(stderr, "\n");

        fprintf(stderr, "Entry:\n");
        entry->to_string(entry, stderr);
        fprintf(stderr, "\n");

        assert(SOBJECT_IS_TYPE(scheme_first(entry), "REAL"));
        assert(SOBJECT_IS_TYPE(scheme_second(entry), "REAL"));
        assert(SOBJECT_IS_TYPE(scheme_third(entry), "REAL"));
        assert(SOBJECT_IS_TYPE(scheme_fourth(entry), "VSTREAM"));

        struct vstream_overlay_record rec;
        memset(&rec, 0, sizeof(rec));

        rec.xoff = (int) scheme_first(entry)->u.real.v;
        rec.yoff = (int) scheme_second(entry)->u.real.v;
        rec.vs = (vstream_t*) scheme_fourth(entry)->u.other.impl;
        if (scheme_third(entry)->u.real.v)
            vs->nframes_remaining = imax(vs->nframes_remaining, rec.vs->nframes_remaining);

        zarray_add(impl->records, &rec);
    }

    sobject_t *obj = scheme_vs_create(scheme, vs);
    return obj;
}

/////////////////////////////////////////////////////
// vs_trim <first frame> <length> <stream>

/////////////////////////////////////////////////////
// vs_scale width height <stream>

/////////////////////////////////////////////////////
// transitions
static image_u8x3_t* transition_null(vstream_transition_t *vt, vstream_t *a, vstream_t *b)
{
    if (a->nframes_remaining)
        return a->next_image(a);

    return NULL;
}

static sobject_t *builtin_vs_transition_null(scheme_t *scheme, scheme_env_t *env, sobject_t *head)
{
    vstream_transition_t *vt = calloc(1, sizeof(vstream_transition_t));
    vt->name = "transition-null";
    vt->transition = transition_null;
    vt->nframes = 0;

    return scheme_vs_transition_create(scheme, vt);
}

static image_u8x3_t* transition_crossfade(vstream_transition_t *vt, vstream_t *a, vstream_t *b)
{
    // not yet at the transition.
    if (a->nframes_remaining > vt->nframes)
        return a->next_image(a);

    // we're in the transition region (overlapping the last nframes of a and the first nframes of b)
    if (a->nframes_remaining > 0 && b->nframes_remaining > 0) {

        image_u8x3_t *im0 = a->next_image(a);
        image_u8x3_t *im1 = b->next_image(b);

        assert(im0->width == im1->width);
        assert(im0->height == im1->height);

        image_u8x3_t *im = image_u8x3_create(im0->width, im0->height);

        int remaining = a->nframes_remaining < b->nframes_remaining ? a->nframes_remaining : b->nframes_remaining;
        double frac = remaining * 1.0 / vt->nframes;

        for (int y = 0; y < im0->height; y++) {
            for (int x = 0; x < im0->width; x++) {
                uint8_t *c = &im->buf[y*im->stride + 3*x];

                for (int cidx = 0; cidx < 3; cidx++)
                    c[cidx] = im0->buf[y*im0->stride + 3*x + cidx] * frac +
                        im1->buf[y*im1->stride + 3*x + cidx] * (1-frac);
            }
        }

        image_u8x3_destroy(im0);
        image_u8x3_destroy(im1);

        return im;
    }

    // we're past the transition.
    return NULL;
}

static sobject_t *builtin_vs_transition_crossfade(scheme_t *scheme, scheme_env_t *env, sobject_t *head)
{
    vstream_transition_t *vt = calloc(1, sizeof(vstream_transition_t));
    vt->name = "transition-crossfade";
    vt->transition = transition_crossfade;

    assert(SOBJECT_IS_TYPE(scheme_first(head), "REAL"));
    vt->nframes = (int) scheme_first(head)->u.real.v;

    return scheme_vs_transition_create(scheme, vt);
}

/////////////////////////////////////////////////////
// main
int main(int argc, char *argv[])
{
    scheme_t *scheme = scheme_create();
    scheme_env_t *env = scheme_env_create(scheme, NULL);
    scheme_env_setup_basic(scheme, env);
    scheme_env_add_method(scheme, env, "vs-output", builtin_vs_output);
    scheme_env_add_method(scheme, env, "vs-solid", builtin_vs_solid);
    scheme_env_add_method(scheme, env, "vs-concat", builtin_vs_concat);
    scheme_env_add_method(scheme, env, "vs-concat-list", builtin_vs_concat_list);
    scheme_env_add_method(scheme, env, "vs-still", builtin_vs_still);
    scheme_env_add_method(scheme, env, "vs-images", builtin_vs_images);
    scheme_env_add_method(scheme, env, "vs-matte", builtin_vs_matte);
    scheme_env_add_method(scheme, env, "vs-overlay", builtin_vs_overlay);
    scheme_env_add_method(scheme, env, "vs-progress-bar", builtin_vs_progress_bar);
    scheme_env_add_method(scheme, env, "vs-transition-null", builtin_vs_transition_null);
    scheme_env_add_method(scheme, env, "vs-transition-crossfade", builtin_vs_transition_crossfade);

    zarray_t *objs = scheme_read_path(scheme, argv[1]);
    for (int i = 0; i < zarray_size(objs); i++) {
        sobject_t *obj;
        zarray_get(objs, i, &obj);

        fprintf(stderr, "expr: ");
        obj->to_string(obj, stderr);
        fprintf(stderr, "\n");

        sobject_t *eval = scheme_eval(scheme, env, obj);
        fprintf(stderr, "eval: ");
        eval->to_string(eval, stderr);

        fprintf(stderr, "\n");
    }

    return 0;
}
