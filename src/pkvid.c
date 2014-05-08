#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include "image_u8x3.h"
#include "string_util.h"
#include "zarray.h"

typedef struct stream stream_t;
struct stream
{
    image_u8x3_t* (*next_frame)(stream_t *s);
};

/*
pdftoppm -scale-to-y 1080 original.pdf original

./pkvid script.txt | avconv -y -f image2pipe -vcodec ppm -i - -b 10M -r 30 foo.mp4

*/
typedef struct pkvid pkvid_t;
struct pkvid
{
    FILE *out;

    int     timebar_height;
    uint8_t timebar_rgb0[3];
    uint8_t timebar_rgb1[3];
};

void write_image(pkvid_t *pkvid, image_u8x3_t *im)
{
    fprintf(stderr, "frame %p\n", im);

    fprintf(pkvid->out, "P6\n%d %d\n255\n", im->width, im->height);
    int linesz = im->width * 3;
    for (int y = 0; y < im->height; y++) {
        if (linesz != fwrite(&im->buf[y*im->stride], 1, linesz, pkvid->out)) {
            assert(0);
        }
    }
    fflush(pkvid->out);
}

// show <nframes> <pnmpath>
void do_show(pkvid_t *pkvid, zarray_t *toks)
{
    assert(zarray_size(toks)==3);

    char *t;
    zarray_get(toks, 1, &t);

    int nframes = atoi(t);

    char *path;
    zarray_get(toks, 2, &path);

    image_u8x3_t *im = image_u8x3_create_from_pnm(path);
    if (im == NULL) {
        fprintf(stderr, "couldn't read image %s\n", path);
        exit(-1);
    }

    for (int i = 0; i < nframes; i++) {
        write_image(pkvid, im);
    }

    image_u8x3_destroy(im);
}

void do_show_timebar(pkvid_t *pkvid, zarray_t *toks)
{
    assert(zarray_size(toks)==3);

    char *t;
    zarray_get(toks, 1, &t);

    int nframes = atoi(t);

    char *path;
    zarray_get(toks, 2, &path);

    image_u8x3_t *im = image_u8x3_create_from_pnm(path);
    if (im == NULL) {
        fprintf(stderr, "couldn't read image %s\n", path);
        exit(-1);
    }

    for (int i = 0; i < nframes; i++) {
        // where should we transition?
        int timebar_mark = im->width * i / nframes;

        // alpha controls the width of the transition region; small alphas
        // slow things down.
        // make the transition area scale with the amount of motion we
        // produce with every frame; approximate a motion blur.
        double alpha = 1.0 / (im->width / nframes);

        for (int y = im->height - pkvid->timebar_height; y < im->height; y++) {
            for (int x = 0; x < im->width; x++) {
                double frac = 1.0 / (1.0 + expf(-alpha*(x - timebar_mark)));

                uint8_t *c = &im->buf[y*im->stride + 3*x];

                for (int cidx = 0; cidx < 3; cidx++)
                    c[cidx] = pkvid->timebar_rgb0[cidx]*(1-frac)
                        + pkvid->timebar_rgb1[cidx]*(frac);

            }
        }
        write_image(pkvid, im);
    }

    image_u8x3_destroy(im);
}

// crossfade <frames> <pnm0> <pnm1>
void do_crossfade(pkvid_t *pkvid, zarray_t *toks)
{
    assert(zarray_size(toks)==4);
    char *t;
    zarray_get(toks, 1, &t);

    int nframes = atoi(t);
    char *path0, *path1;
    zarray_get(toks, 2, &path0);
    zarray_get(toks, 3, &path1);

    image_u8x3_t *im0 = image_u8x3_create_from_pnm(path0);
    assert(im0 != NULL);
    image_u8x3_t *im1 = image_u8x3_create_from_pnm(path1);
    assert(im1 != NULL);

    assert(im0->width == im1->width);
    assert(im0->height == im1->height);

    image_u8x3_t *im = image_u8x3_create(im0->width, im1->height);

    for (int i = 0; i < nframes; i++) {
        double frac = 1.0*i/nframes;

        for (int y = 0; y < im0->height; y++) {
            for (int x = 0; x < im0->width; x++) {
                uint8_t *c = &im->buf[y*im->stride + 3*x];

                for (int cidx = 0; cidx < 3; cidx++)
                    c[cidx] = im0->buf[y*im0->stride + 3*x + cidx] * (1-frac) +
                        im1->buf[y*im1->stride + 3*x + cidx] * frac;
            }
        }

        write_image(pkvid, im);
    }

    image_u8x3_destroy(im);
    image_u8x3_destroy(im0);
    image_u8x3_destroy(im1);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("usage: %s script.txt\n", argv[0]);
        exit(-1);
    }

    pkvid_t *pkvid = calloc(1, sizeof(pkvid_t));
    pkvid->out = stdout;
    pkvid->timebar_height = 8;
    memcpy(pkvid->timebar_rgb0, (uint8_t[]) { 255, 0, 0}, 3);
    memcpy(pkvid->timebar_rgb1, (uint8_t[]) { 0, 0, 0}, 3);

    FILE *f = fopen(argv[1], "r");
    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {

        if (buf[0]=='#' || strlen(buf) == 0)
            continue;

        buf[strlen(buf)-1] = 0; // kill trailing newline
        zarray_t *toks = str_split(buf, " ");

        if (zarray_size(toks) == 0)
            goto cleanup;

        char *command;
        zarray_get(toks, 0, &command);

        if (!strcmp(command, "show")) {
            do_show(pkvid, toks);
        }

        if (!strcmp(command, "timebar_height")) {
            assert(zarray_size(toks)==2);
            char *height;
            zarray_get(toks, 1, &height);
            pkvid->timebar_height = atoi(height);
        }

        if (!strcmp(command, "timebar_rgb0")) {
            assert(zarray_size(toks)==4);
            for (int i = 0; i < 3; i++) {
                char *v;
                zarray_get(toks, 1+i, &v);
                pkvid->timebar_rgb0[i] = atoi(v);
            }
        }

        if (!strcmp(command, "timebar_rgb1")) {
            assert(zarray_size(toks)==4);
            for (int i = 0; i < 3; i++) {
                char *v;
                zarray_get(toks, 1+i, &v);
                pkvid->timebar_rgb1[i] = atoi(v);
            }
        }

        if (!strcmp(command, "show_timebar")) {
            do_show_timebar(pkvid, toks);
        }

        if (!strcmp(command, "crossfade")) {
            do_crossfade(pkvid, toks);
        }

      cleanup:
        zarray_destroy(toks);
    }

    return 0;
}
