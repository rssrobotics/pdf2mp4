#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <png.h>

#include "image_u8x3.h"
#include "pnm.h"

// least common multiple of 32 (cache line) and 24 (stride needed for
// 8byte-wide RGB processing). (It's possible that 24 would be enough).
#define DEFAULT_ALIGNMENT 96

image_u8x3_t *image_u8x3_create(int width, int height)
{
    return image_u8x3_create_alignment(width, height, DEFAULT_ALIGNMENT);
}

image_u8x3_t *image_u8x3_copy(image_u8x3_t *im)
{
    image_u8x3_t *im2 = image_u8x3_create(im->width, im->height);

    for (int y = 0; y < im->height; y++) {
        memcpy(&im2->buf[y*im2->stride], &im->buf[y*im->stride], 3*im->width);
    }

    return im2;
}

// force stride to be a multiple of 'alignment' bytes.
image_u8x3_t *image_u8x3_create_alignment(int width, int height, int alignment)
{
    assert(alignment > 0);

    image_u8x3_t *im = (image_u8x3_t*) calloc(1, sizeof(image_u8x3_t));

    im->width  = width;
    im->height = height;
    im->stride = width*3;

    if ((im->stride % alignment) != 0)
        im->stride += alignment - (im->stride % alignment);

    im->buf = (uint8_t*) calloc(1, im->height*im->stride);

    return im;
}

void image_u8x3_destroy(image_u8x3_t *im)
{
    if (!im)
        return;

    free(im->buf);
    free(im);
}

int image_u8x3_write_pnm(const image_u8x3_t *im, const char *path)
{
    FILE *f = fopen(path, "wb");
    int res = 0;

    if (f == NULL) {
        res = -1;
        goto finish;
    }

    // Only outputs to RGB
    fprintf(f, "P6\n%d %d\n255\n", im->width, im->height);
    int linesz = im->width * 3;
    for (int y = 0; y < im->height; y++) {
        if (linesz != fwrite(&im->buf[y*im->stride], 1, linesz, f)) {
            res = -1;
            goto finish;
        }
    }

finish:
    if (f != NULL)
        fclose(f);

    return res;
}

image_u8x3_t *image_u8x3_create_from_png(const char *path)
{
    image_u8x3_t *im = NULL;

    png_structp png_ptr;
    png_infop info_ptr;

    unsigned char header[8];    // 8 is the maximum size that can be checked

    /* open file and test for it being a png */
    FILE *fp = fopen(path, "rb");
    if (!fp)
        goto finish;

    if (8 != fread(header, 1, 8, fp))
        goto finish;
    if (png_sig_cmp(header, 0, 8))
        goto finish;

    /* initialize stuff */
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png_ptr)
        goto finish;

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
        goto finish;

    if (setjmp(png_jmpbuf(png_ptr)))
        goto finish;

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);

    png_read_info(png_ptr, info_ptr);

    int width = png_get_image_width(png_ptr, info_ptr);
    int height = png_get_image_height(png_ptr, info_ptr);
    png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    //number_of_passes = png_set_interlace_handling(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    /* read file */
    if (setjmp(png_jmpbuf(png_ptr)))
        goto finish;

    assert(bit_depth == 8);

    if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY) {

        im = image_u8x3_create(width, height);

        uint8_t *buf = malloc(width * height);
        png_bytep *row_pointers = malloc(sizeof(png_bytep) * height);
        for (int y = 0; y < height; y++)
            row_pointers[y] = &buf[y*width];

        png_read_image(png_ptr, row_pointers);

        for (int y = 0; y < im->height; y++) {
            for (int x = 0; x < im->width; x++) {
                im->buf[y*im->stride + 3*x + 0] = row_pointers[y][x];
                im->buf[y*im->stride + 3*x + 1] = row_pointers[y][x];
                im->buf[y*im->stride + 3*x + 2] = row_pointers[y][x];
            }
        }

        free(buf);
        goto finish;
    }

    if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGBA) {

        uint8_t *buf = malloc(width * height * 4);
        png_bytep *row_pointers = malloc(sizeof(png_bytep) * height);
        for (int y = 0; y < height; y++)
            row_pointers[y] = &buf[y*width * 4];

        png_read_image(png_ptr, row_pointers);
        for (int y = 0; y < im->height; y++) {
            for (int x = 0; x < im->width; x++) {
                im->buf[y*im->stride + 3*x + 0] = row_pointers[y][4*x+0];
                im->buf[y*im->stride + 3*x + 1] = row_pointers[y][4*x+1];
                im->buf[y*im->stride + 3*x + 2] = row_pointers[y][4*x+2];
            }
        }

        free(buf);
        goto finish;
    }

    if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB) {

        png_bytep *row_pointers = malloc(sizeof(png_bytep) * height);
        for (int y = 0; y < height; y++)
            row_pointers[y] = &im->buf[y*im->stride];

        png_read_image(png_ptr, row_pointers);

        goto finish;
    }

    printf("image_u8x3: Unknown PNG image format.\n");

finish:

    fclose(fp);
    return im;
}

image_u8x3_t *image_u8x3_create_from_pnm(const char *path)
{
    pnm_t *pnm = pnm_create_from_file(path);
    if (pnm == NULL)
        return NULL;

    image_u8x3_t *im = image_u8x3_create(pnm->width, pnm->height);

    switch (pnm->format) {
        case 5: {
            for (int y = 0; y < im->height; y++) {
                for (int x = 0; x < im->width; x++) {
                    uint8_t gray = pnm->buf[y*im->width + x];

                    im->buf[y*im->stride + 3*x + 0] = gray;
                    im->buf[y*im->stride + 3*x + 1] = gray;
                    im->buf[y*im->stride + 3*x + 2] = gray;
                }
            }

            pnm_destroy(pnm);
            return im;
        }

        case 6: {
            for (int y = 0; y < im->height; y++) {
                memcpy(&im->buf[y*im->stride], &pnm->buf[y*im->width*3], 3*im->width);
            }

            pnm_destroy(pnm);
            return im;
        }
    }

    pnm_destroy(pnm);
    return NULL;
}
