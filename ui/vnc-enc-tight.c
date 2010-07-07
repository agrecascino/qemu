/*
 * QEMU VNC display driver: tight encoding
 *
 * From libvncserver/libvncserver/tight.c
 * Copyright (C) 2000, 2001 Const Kaplinsky.  All Rights Reserved.
 * Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 *
 * Copyright (C) 2010 Corentin Chary <corentin.chary@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config-host.h"

#ifdef CONFIG_VNC_PNG
#include <png.h>
#endif
#ifdef CONFIG_VNC_JPEG
#include <stdio.h>
#include <jpeglib.h>
#endif

#include "qemu-common.h"

#include "bswap.h"
#include "qdict.h"
#include "qint.h"
#include "vnc.h"
#include "vnc-enc-tight.h"

/* Compression level stuff. The following array contains various
   encoder parameters for each of 10 compression levels (0..9).
   Last three parameters correspond to JPEG quality levels (0..9). */

static const struct {
    int max_rect_size, max_rect_width;
    int mono_min_rect_size, gradient_min_rect_size;
    int idx_zlib_level, mono_zlib_level, raw_zlib_level, gradient_zlib_level;
    int gradient_threshold, gradient_threshold24;
    int idx_max_colors_divisor;
    int jpeg_quality, jpeg_threshold, jpeg_threshold24;
} tight_conf[] = {
    {   512,   32,   6, 65536, 0, 0, 0, 0,   0,   0,   4,  5, 10000, 23000 },
    {  2048,  128,   6, 65536, 1, 1, 1, 0,   0,   0,   8, 10,  8000, 18000 },
    {  6144,  256,   8, 65536, 3, 3, 2, 0,   0,   0,  24, 15,  6500, 15000 },
    { 10240, 1024,  12, 65536, 5, 5, 3, 0,   0,   0,  32, 25,  5000, 12000 },
    { 16384, 2048,  12, 65536, 6, 6, 4, 0,   0,   0,  32, 37,  4000, 10000 },
    { 32768, 2048,  12,  4096, 7, 7, 5, 4, 150, 380,  32, 50,  3000,  8000 },
    { 65536, 2048,  16,  4096, 7, 7, 6, 4, 170, 420,  48, 60,  2000,  5000 },
    { 65536, 2048,  16,  4096, 8, 8, 7, 5, 180, 450,  64, 70,  1000,  2500 },
    { 65536, 2048,  32,  8192, 9, 9, 8, 6, 190, 475,  64, 75,   500,  1200 },
    { 65536, 2048,  32,  8192, 9, 9, 9, 6, 200, 500,  96, 80,   200,   500 }
};


static int tight_send_framebuffer_update(VncState *vs, int x, int y,
                                         int w, int h);

#ifdef CONFIG_VNC_PNG
static const struct {
    int png_zlib_level, png_filters;
} tight_png_conf[] = {
    { 0, PNG_NO_FILTERS },
    { 1, PNG_NO_FILTERS },
    { 2, PNG_NO_FILTERS },
    { 3, PNG_NO_FILTERS },
    { 4, PNG_NO_FILTERS },
    { 5, PNG_ALL_FILTERS },
    { 6, PNG_ALL_FILTERS },
    { 7, PNG_ALL_FILTERS },
    { 8, PNG_ALL_FILTERS },
    { 9, PNG_ALL_FILTERS },
};

static int send_png_rect(VncState *vs, int x, int y, int w, int h,
                         QDict *palette);

static bool tight_can_send_png_rect(VncState *vs, int w, int h)
{
    if (vs->tight_type != VNC_ENCODING_TIGHT_PNG) {
        return false;
    }

    if (ds_get_bytes_per_pixel(vs->ds) == 1 ||
        vs->clientds.pf.bytes_per_pixel == 1) {
        return false;
    }

    return true;
}
#endif

/*
 * Code to guess if given rectangle is suitable for smooth image
 * compression (by applying "gradient" filter or JPEG coder).
 */

static uint
tight_detect_smooth_image24(VncState *vs, int w, int h)
{
    int off;
    int x, y, d, dx;
    uint c;
    uint stats[256];
    int pixels = 0;
    int pix, left[3];
    uint errors;
    unsigned char *buf = vs->tight.buffer;

    /*
     * If client is big-endian, color samples begin from the second
     * byte (offset 1) of a 32-bit pixel value.
     */
    off = !!(vs->clientds.flags & QEMU_BIG_ENDIAN_FLAG);

    memset(stats, 0, sizeof (stats));

    for (y = 0, x = 0; y < h && x < w;) {
        for (d = 0; d < h - y && d < w - x - VNC_TIGHT_DETECT_SUBROW_WIDTH;
             d++) {
            for (c = 0; c < 3; c++) {
                left[c] = buf[((y+d)*w+x+d)*4+off+c] & 0xFF;
            }
            for (dx = 1; dx <= VNC_TIGHT_DETECT_SUBROW_WIDTH; dx++) {
                for (c = 0; c < 3; c++) {
                    pix = buf[((y+d)*w+x+d+dx)*4+off+c] & 0xFF;
                    stats[abs(pix - left[c])]++;
                    left[c] = pix;
                }
                pixels++;
            }
        }
        if (w > h) {
            x += h;
            y = 0;
        } else {
            x = 0;
            y += w;
        }
    }

    /* 95% smooth or more ... */
    if (stats[0] * 33 / pixels >= 95) {
        return 0;
    }

    errors = 0;
    for (c = 1; c < 8; c++) {
        errors += stats[c] * (c * c);
        if (stats[c] == 0 || stats[c] > stats[c-1] * 2) {
            return 0;
        }
    }
    for (; c < 256; c++) {
        errors += stats[c] * (c * c);
    }
    errors /= (pixels * 3 - stats[0]);

    return errors;
}

#define DEFINE_DETECT_FUNCTION(bpp)                                     \
                                                                        \
    static uint                                                         \
    tight_detect_smooth_image##bpp(VncState *vs, int w, int h) {        \
        bool endian;                                                    \
        uint##bpp##_t pix;                                              \
        int max[3], shift[3];                                           \
        int x, y, d, dx;                                                \
        uint c;                                                         \
        uint stats[256];                                                \
        int pixels = 0;                                                 \
        int sample, sum, left[3];                                       \
        uint errors;                                                    \
        unsigned char *buf = vs->tight.buffer;                          \
                                                                        \
        endian = ((vs->clientds.flags & QEMU_BIG_ENDIAN_FLAG) !=        \
                  (vs->ds->surface->flags & QEMU_BIG_ENDIAN_FLAG));     \
                                                                        \
                                                                        \
        max[0] = vs->clientds.pf.rmax;                                  \
        max[1] = vs->clientds.pf.gmax;                                  \
        max[2] = vs->clientds.pf.bmax;                                  \
        shift[0] = vs->clientds.pf.rshift;                              \
        shift[1] = vs->clientds.pf.gshift;                              \
        shift[2] = vs->clientds.pf.bshift;                              \
                                                                        \
        memset(stats, 0, sizeof(stats));                                \
                                                                        \
        y = 0, x = 0;                                                   \
        while (y < h && x < w) {                                        \
            for (d = 0; d < h - y &&                                    \
                     d < w - x - VNC_TIGHT_DETECT_SUBROW_WIDTH; d++) {  \
                pix = ((uint##bpp##_t *)buf)[(y+d)*w+x+d];              \
                if (endian) {                                           \
                    pix = bswap_##bpp(pix);                             \
                }                                                       \
                for (c = 0; c < 3; c++) {                               \
                    left[c] = (int)(pix >> shift[c] & max[c]);          \
                }                                                       \
                for (dx = 1; dx <= VNC_TIGHT_DETECT_SUBROW_WIDTH;       \
                     dx++) {                                            \
                    pix = ((uint##bpp##_t *)buf)[(y+d)*w+x+d+dx];       \
                    if (endian) {                                       \
                        pix = bswap_##bpp(pix);                         \
                    }                                                   \
                    sum = 0;                                            \
                    for (c = 0; c < 3; c++) {                           \
                        sample = (int)(pix >> shift[c] & max[c]);       \
                        sum += abs(sample - left[c]);                   \
                        left[c] = sample;                               \
                    }                                                   \
                    if (sum > 255) {                                    \
                        sum = 255;                                      \
                    }                                                   \
                    stats[sum]++;                                       \
                    pixels++;                                           \
                }                                                       \
            }                                                           \
            if (w > h) {                                                \
                x += h;                                                 \
                y = 0;                                                  \
            } else {                                                    \
                x = 0;                                                  \
                y += w;                                                 \
            }                                                           \
        }                                                               \
                                                                        \
        if ((stats[0] + stats[1]) * 100 / pixels >= 90) {               \
            return 0;                                                   \
        }                                                               \
                                                                        \
        errors = 0;                                                     \
        for (c = 1; c < 8; c++) {                                       \
            errors += stats[c] * (c * c);                               \
            if (stats[c] == 0 || stats[c] > stats[c-1] * 2) {           \
                return 0;                                               \
            }                                                           \
        }                                                               \
        for (; c < 256; c++) {                                          \
            errors += stats[c] * (c * c);                               \
        }                                                               \
        errors /= (pixels - stats[0]);                                  \
                                                                        \
        return errors;                                                  \
    }

DEFINE_DETECT_FUNCTION(16)
DEFINE_DETECT_FUNCTION(32)

static int
tight_detect_smooth_image(VncState *vs, int w, int h)
{
    uint errors;
    int compression = vs->tight_compression;
    int quality = vs->tight_quality;

    if (!vs->vd->lossy) {
        return 0;
    }

    if (ds_get_bytes_per_pixel(vs->ds) == 1 ||
        vs->clientds.pf.bytes_per_pixel == 1 ||
        w < VNC_TIGHT_DETECT_MIN_WIDTH || h < VNC_TIGHT_DETECT_MIN_HEIGHT) {
        return 0;
    }

    if (vs->tight_quality != -1) {
        if (w * h < VNC_TIGHT_JPEG_MIN_RECT_SIZE) {
            return 0;
        }
    } else {
        if (w * h < tight_conf[compression].gradient_min_rect_size) {
            return 0;
        }
    }

    if (vs->clientds.pf.bytes_per_pixel == 4) {
        if (vs->tight_pixel24) {
            errors = tight_detect_smooth_image24(vs, w, h);
            if (vs->tight_quality != -1) {
                return (errors < tight_conf[quality].jpeg_threshold24);
            }
            return (errors < tight_conf[compression].gradient_threshold24);
        } else {
            errors = tight_detect_smooth_image32(vs, w, h);
        }
    } else {
        errors = tight_detect_smooth_image16(vs, w, h);
    }
    if (quality != -1) {
        return (errors < tight_conf[quality].jpeg_threshold);
    }
    return (errors < tight_conf[compression].gradient_threshold);
}

/*
 * Code to determine how many different colors used in rectangle.
 */

static void tight_palette_rgb2buf(uint32_t rgb, int bpp, uint8_t buf[6])
{
    memset(buf, 0, 6);

    if (bpp == 32) {
        buf[0] = ((rgb >> 24) & 0xFF);
        buf[1] = ((rgb >> 16) & 0xFF);
        buf[2] = ((rgb >>  8) & 0xFF);
        buf[3] = ((rgb >>  0) & 0xFF);
        buf[4] = ((buf[0] & 1) == 0) << 3 | ((buf[1] & 1) == 0) << 2;
        buf[4]|= ((buf[2] & 1) == 0) << 1 | ((buf[3] & 1) == 0) << 0;
        buf[0] |= 1;
        buf[1] |= 1;
        buf[2] |= 1;
        buf[3] |= 1;
    }
    if (bpp == 16) {
        buf[0] = ((rgb >> 8) & 0xFF);
        buf[1] = ((rgb >> 0) & 0xFF);
        buf[2] = ((buf[0] & 1) == 0) << 1 | ((buf[1] & 1) == 0) << 0;
        buf[0] |= 1;
        buf[1] |= 1;
    }
}

static uint32_t tight_palette_buf2rgb(int bpp, const uint8_t *buf)
{
    uint32_t rgb = 0;

    if (bpp == 32) {
        rgb |= ((buf[0] & ~1) | !((buf[4] >> 3) & 1)) << 24;
        rgb |= ((buf[1] & ~1) | !((buf[4] >> 2) & 1)) << 16;
        rgb |= ((buf[2] & ~1) | !((buf[4] >> 1) & 1)) <<  8;
        rgb |= ((buf[3] & ~1) | !((buf[4] >> 0) & 1)) <<  0;
    }
    if (bpp == 16) {
        rgb |= ((buf[0] & ~1) | !((buf[2] >> 1) & 1)) << 8;
        rgb |= ((buf[1] & ~1) | !((buf[2] >> 0) & 1)) << 0;
    }
    return rgb;
}


static int tight_palette_insert(QDict *palette, uint32_t rgb, int bpp, int max)
{
    uint8_t key[6];
    int idx = qdict_size(palette);
    bool present;

    tight_palette_rgb2buf(rgb, bpp, key);
    present = qdict_haskey(palette, (char *)key);
    if (idx >= max && !present) {
        return 0;
    }
    if (!present) {
        qdict_put(palette, (char *)key, qint_from_int(idx));
    }
    return qdict_size(palette);
}

#define DEFINE_FILL_PALETTE_FUNCTION(bpp)                               \
                                                                        \
    static int                                                          \
    tight_fill_palette##bpp(VncState *vs, int x, int y,                 \
                            int max, size_t count,                      \
                            uint32_t *bg, uint32_t *fg,                 \
                            struct QDict **palette) {                   \
        uint##bpp##_t *data;                                            \
        uint##bpp##_t c0, c1, ci;                                       \
        int i, n0, n1;                                                  \
                                                                        \
        data = (uint##bpp##_t *)vs->tight.buffer;                       \
                                                                        \
        c0 = data[0];                                                   \
        i = 1;                                                          \
        while (i < count && data[i] == c0)                              \
            i++;                                                        \
        if (i >= count) {                                               \
            *bg = *fg = c0;                                             \
            return 1;                                                   \
        }                                                               \
                                                                        \
        if (max < 2) {                                                  \
            return 0;                                                   \
        }                                                               \
                                                                        \
        n0 = i;                                                         \
        c1 = data[i];                                                   \
        n1 = 0;                                                         \
        for (i++; i < count; i++) {                                     \
            ci = data[i];                                               \
            if (ci == c0) {                                             \
                n0++;                                                   \
            } else if (ci == c1) {                                      \
                n1++;                                                   \
            } else                                                      \
                break;                                                  \
        }                                                               \
        if (i >= count) {                                               \
            if (n0 > n1) {                                              \
                *bg = (uint32_t)c0;                                     \
                *fg = (uint32_t)c1;                                     \
            } else {                                                    \
                *bg = (uint32_t)c1;                                     \
                *fg = (uint32_t)c0;                                     \
            }                                                           \
            return 2;                                                   \
        }                                                               \
                                                                        \
        if (max == 2) {                                                 \
            return 0;                                                   \
        }                                                               \
                                                                        \
        *palette = qdict_new();                                         \
        tight_palette_insert(*palette, c0, bpp, max);                   \
        tight_palette_insert(*palette, c1, bpp, max);                   \
        tight_palette_insert(*palette, ci, bpp, max);                   \
                                                                        \
        for (i++; i < count; i++) {                                     \
            if (data[i] == ci) {                                        \
                continue;                                               \
            } else {                                                    \
                ci = data[i];                                           \
                if (!tight_palette_insert(*palette, (uint32_t)ci,       \
                                          bpp, max)) {                  \
                    return 0;                                           \
                }                                                       \
            }                                                           \
        }                                                               \
                                                                        \
        return qdict_size(*palette);                                    \
    }

DEFINE_FILL_PALETTE_FUNCTION(8)
DEFINE_FILL_PALETTE_FUNCTION(16)
DEFINE_FILL_PALETTE_FUNCTION(32)

static int tight_fill_palette(VncState *vs, int x, int y,
                              size_t count, uint32_t *bg, uint32_t *fg,
                              struct QDict **palette)
{
    int max;

    max = count / tight_conf[vs->tight_compression].idx_max_colors_divisor;
    if (max < 2 &&
        count >= tight_conf[vs->tight_compression].mono_min_rect_size) {
        max = 2;
    }
    if (max >= 256) {
        max = 256;
    }

    switch(vs->clientds.pf.bytes_per_pixel) {
    case 4:
        return tight_fill_palette32(vs, x, y, max, count, bg, fg, palette);
    case 2:
        return tight_fill_palette16(vs, x, y, max, count, bg, fg, palette);
    default:
        max = 2;
        return tight_fill_palette8(vs, x, y, max, count, bg, fg, palette);
    }
    return 0;
}

/* Callback to dump a palette with qdict_iter
static void print_palette(const char *key, QObject *obj, void *opaque)
{
    uint8_t idx = qint_get_int(qobject_to_qint(obj));
    uint32_t rgb = tight_palette_buf2rgb(32, (uint8_t *)key);

    fprintf(stderr, "%.2x ", (unsigned char)*key);
    while (*key++)
        fprintf(stderr, "%.2x ", (unsigned char)*key);

    fprintf(stderr, ": idx: %x rgb: %x\n", idx, rgb);
}
*/

/*
 * Converting truecolor samples into palette indices.
 */
#define DEFINE_IDX_ENCODE_FUNCTION(bpp)                                 \
                                                                        \
    static void                                                         \
    tight_encode_indexed_rect##bpp(uint8_t *buf, int count,             \
                                   struct QDict *palette) {             \
        uint##bpp##_t *src;                                             \
        uint##bpp##_t rgb;                                              \
        uint8_t key[6];                                                 \
        int i, rep;                                                     \
        uint8_t idx;                                                    \
                                                                        \
        src = (uint##bpp##_t *) buf;                                    \
                                                                        \
        for (i = 0; i < count; i++) {                                   \
                                                                        \
            rgb = *src++;                                               \
            rep = 0;                                                    \
            while (i < count && *src == rgb) {                          \
                rep++, src++, i++;                                      \
            }                                                           \
            tight_palette_rgb2buf(rgb, bpp, key);                       \
            if (!qdict_haskey(palette, (char *)key)) {                  \
                /*                                                      \
                 * Should never happen, but don't break everything      \
                 * if it does, use the first color instead              \
                 */                                                     \
                idx = 0;                                                \
            } else {                                                    \
                idx = qdict_get_int(palette, (char *)key);              \
            }                                                           \
            while (rep >= 0) {                                          \
                *buf++ = idx;                                           \
                rep--;                                                  \
            }                                                           \
        }                                                               \
    }

DEFINE_IDX_ENCODE_FUNCTION(16)
DEFINE_IDX_ENCODE_FUNCTION(32)

#define DEFINE_MONO_ENCODE_FUNCTION(bpp)                                \
                                                                        \
    static void                                                         \
    tight_encode_mono_rect##bpp(uint8_t *buf, int w, int h,             \
                                uint##bpp##_t bg, uint##bpp##_t fg) {   \
        uint##bpp##_t *ptr;                                             \
        unsigned int value, mask;                                       \
        int aligned_width;                                              \
        int x, y, bg_bits;                                              \
                                                                        \
        ptr = (uint##bpp##_t *) buf;                                    \
        aligned_width = w - w % 8;                                      \
                                                                        \
        for (y = 0; y < h; y++) {                                       \
            for (x = 0; x < aligned_width; x += 8) {                    \
                for (bg_bits = 0; bg_bits < 8; bg_bits++) {             \
                    if (*ptr++ != bg) {                                 \
                        break;                                          \
                    }                                                   \
                }                                                       \
                if (bg_bits == 8) {                                     \
                    *buf++ = 0;                                         \
                    continue;                                           \
                }                                                       \
                mask = 0x80 >> bg_bits;                                 \
                value = mask;                                           \
                for (bg_bits++; bg_bits < 8; bg_bits++) {               \
                    mask >>= 1;                                         \
                    if (*ptr++ != bg) {                                 \
                        value |= mask;                                  \
                    }                                                   \
                }                                                       \
                *buf++ = (uint8_t)value;                                \
            }                                                           \
                                                                        \
            mask = 0x80;                                                \
            value = 0;                                                  \
            if (x >= w) {                                               \
                continue;                                               \
            }                                                           \
                                                                        \
            for (; x < w; x++) {                                        \
                if (*ptr++ != bg) {                                     \
                    value |= mask;                                      \
                }                                                       \
                mask >>= 1;                                             \
            }                                                           \
            *buf++ = (uint8_t)value;                                    \
        }                                                               \
    }

DEFINE_MONO_ENCODE_FUNCTION(8)
DEFINE_MONO_ENCODE_FUNCTION(16)
DEFINE_MONO_ENCODE_FUNCTION(32)

/*
 * ``Gradient'' filter for 24-bit color samples.
 * Should be called only when redMax, greenMax and blueMax are 255.
 * Color components assumed to be byte-aligned.
 */

static void
tight_filter_gradient24(VncState *vs, uint8_t *buf, int w, int h)
{
    uint32_t *buf32;
    uint32_t pix32;
    int shift[3];
    int *prev;
    int here[3], upper[3], left[3], upperleft[3];
    int prediction;
    int x, y, c;

    buf32 = (uint32_t *)buf;
    memset(vs->tight_gradient.buffer, 0, w * 3 * sizeof(int));

    if ((vs->clientds.flags & QEMU_BIG_ENDIAN_FLAG) ==
        (vs->ds->surface->flags & QEMU_BIG_ENDIAN_FLAG)) {
        shift[0] = vs->clientds.pf.rshift;
        shift[1] = vs->clientds.pf.gshift;
        shift[2] = vs->clientds.pf.bshift;
    } else {
        shift[0] = 24 - vs->clientds.pf.rshift;
        shift[1] = 24 - vs->clientds.pf.gshift;
        shift[2] = 24 - vs->clientds.pf.bshift;
    }

    for (y = 0; y < h; y++) {
        for (c = 0; c < 3; c++) {
            upper[c] = 0;
            here[c] = 0;
        }
        prev = (int *)vs->tight_gradient.buffer;
        for (x = 0; x < w; x++) {
            pix32 = *buf32++;
            for (c = 0; c < 3; c++) {
                upperleft[c] = upper[c];
                left[c] = here[c];
                upper[c] = *prev;
                here[c] = (int)(pix32 >> shift[c] & 0xFF);
                *prev++ = here[c];

                prediction = left[c] + upper[c] - upperleft[c];
                if (prediction < 0) {
                    prediction = 0;
                } else if (prediction > 0xFF) {
                    prediction = 0xFF;
                }
                *buf++ = (char)(here[c] - prediction);
            }
        }
    }
}


/*
 * ``Gradient'' filter for other color depths.
 */

#define DEFINE_GRADIENT_FILTER_FUNCTION(bpp)                            \
                                                                        \
    static void                                                         \
    tight_filter_gradient##bpp(VncState *vs, uint##bpp##_t *buf,        \
                               int w, int h) {                          \
        uint##bpp##_t pix, diff;                                        \
        bool endian;                                                    \
        int *prev;                                                      \
        int max[3], shift[3];                                           \
        int here[3], upper[3], left[3], upperleft[3];                   \
        int prediction;                                                 \
        int x, y, c;                                                    \
                                                                        \
        memset (vs->tight_gradient.buffer, 0, w * 3 * sizeof(int));     \
                                                                        \
        endian = ((vs->clientds.flags & QEMU_BIG_ENDIAN_FLAG) !=        \
                  (vs->ds->surface->flags & QEMU_BIG_ENDIAN_FLAG));     \
                                                                        \
        max[0] = vs->clientds.pf.rmax;                                  \
        max[1] = vs->clientds.pf.gmax;                                  \
        max[2] = vs->clientds.pf.bmax;                                  \
        shift[0] = vs->clientds.pf.rshift;                              \
        shift[1] = vs->clientds.pf.gshift;                              \
        shift[2] = vs->clientds.pf.bshift;                              \
                                                                        \
        for (y = 0; y < h; y++) {                                       \
            for (c = 0; c < 3; c++) {                                   \
                upper[c] = 0;                                           \
                here[c] = 0;                                            \
            }                                                           \
            prev = (int *)vs->tight_gradient.buffer;                    \
            for (x = 0; x < w; x++) {                                   \
                pix = *buf;                                             \
                if (endian) {                                           \
                    pix = bswap_##bpp(pix);                             \
                }                                                       \
                diff = 0;                                               \
                for (c = 0; c < 3; c++) {                               \
                    upperleft[c] = upper[c];                            \
                    left[c] = here[c];                                  \
                    upper[c] = *prev;                                   \
                    here[c] = (int)(pix >> shift[c] & max[c]);          \
                    *prev++ = here[c];                                  \
                                                                        \
                    prediction = left[c] + upper[c] - upperleft[c];     \
                    if (prediction < 0) {                               \
                        prediction = 0;                                 \
                    } else if (prediction > max[c]) {                   \
                        prediction = max[c];                            \
                    }                                                   \
                    diff |= ((here[c] - prediction) & max[c])           \
                        << shift[c];                                    \
                }                                                       \
                if (endian) {                                           \
                    diff = bswap_##bpp(diff);                           \
                }                                                       \
                *buf++ = diff;                                          \
            }                                                           \
        }                                                               \
    }

DEFINE_GRADIENT_FILTER_FUNCTION(16)
DEFINE_GRADIENT_FILTER_FUNCTION(32)

/*
 * Check if a rectangle is all of the same color. If needSameColor is
 * set to non-zero, then also check that its color equals to the
 * *colorPtr value. The result is 1 if the test is successfull, and in
 * that case new color will be stored in *colorPtr.
 */

#define DEFINE_CHECK_SOLID_FUNCTION(bpp)                                \
                                                                        \
    static bool                                                         \
    check_solid_tile##bpp(VncState *vs, int x, int y, int w, int h,     \
                          uint32_t* color, bool samecolor)              \
    {                                                                   \
        VncDisplay *vd = vs->vd;                                        \
        uint##bpp##_t *fbptr;                                           \
        uint##bpp##_t c;                                                \
        int dx, dy;                                                     \
                                                                        \
        fbptr = (uint##bpp##_t *)                                       \
            (vd->server->data + y * ds_get_linesize(vs->ds) +           \
             x * ds_get_bytes_per_pixel(vs->ds));                       \
                                                                        \
        c = *fbptr;                                                     \
        if (samecolor && (uint32_t)c != *color) {                       \
            return false;                                               \
        }                                                               \
                                                                        \
        for (dy = 0; dy < h; dy++) {                                    \
            for (dx = 0; dx < w; dx++) {                                \
                if (c != fbptr[dx]) {                                   \
                    return false;                                       \
                }                                                       \
            }                                                           \
            fbptr = (uint##bpp##_t *)                                   \
                ((uint8_t *)fbptr + ds_get_linesize(vs->ds));           \
        }                                                               \
                                                                        \
        *color = (uint32_t)c;                                           \
        return true;                                                    \
    }

DEFINE_CHECK_SOLID_FUNCTION(32)
DEFINE_CHECK_SOLID_FUNCTION(16)
DEFINE_CHECK_SOLID_FUNCTION(8)

static bool check_solid_tile(VncState *vs, int x, int y, int w, int h,
                             uint32_t* color, bool samecolor)
{
    VncDisplay *vd = vs->vd;

    switch(vd->server->pf.bytes_per_pixel) {
    case 4:
        return check_solid_tile32(vs, x, y, w, h, color, samecolor);
    case 2:
        return check_solid_tile16(vs, x, y, w, h, color, samecolor);
    default:
        return check_solid_tile8(vs, x, y, w, h, color, samecolor);
    }
}

static void find_best_solid_area(VncState *vs, int x, int y, int w, int h,
                                 uint32_t color, int *w_ptr, int *h_ptr)
{
    int dx, dy, dw, dh;
    int w_prev;
    int w_best = 0, h_best = 0;

    w_prev = w;

    for (dy = y; dy < y + h; dy += VNC_TIGHT_MAX_SPLIT_TILE_SIZE) {

        dh = MIN(VNC_TIGHT_MAX_SPLIT_TILE_SIZE, y + h - dy);
        dw = MIN(VNC_TIGHT_MAX_SPLIT_TILE_SIZE, w_prev);

        if (!check_solid_tile(vs, x, dy, dw, dh, &color, true)) {
            break;
        }

        for (dx = x + dw; dx < x + w_prev;) {
            dw = MIN(VNC_TIGHT_MAX_SPLIT_TILE_SIZE, x + w_prev - dx);

            if (!check_solid_tile(vs, dx, dy, dw, dh, &color, true)) {
                break;
            }
            dx += dw;
        }

        w_prev = dx - x;
        if (w_prev * (dy + dh - y) > w_best * h_best) {
            w_best = w_prev;
            h_best = dy + dh - y;
        }
    }

    *w_ptr = w_best;
    *h_ptr = h_best;
}

static void extend_solid_area(VncState *vs, int x, int y, int w, int h,
                              uint32_t color, int *x_ptr, int *y_ptr,
                              int *w_ptr, int *h_ptr)
{
    int cx, cy;

    /* Try to extend the area upwards. */
    for ( cy = *y_ptr - 1;
          cy >= y && check_solid_tile(vs, *x_ptr, cy, *w_ptr, 1, &color, true);
          cy-- );
    *h_ptr += *y_ptr - (cy + 1);
    *y_ptr = cy + 1;

    /* ... downwards. */
    for ( cy = *y_ptr + *h_ptr;
          cy < y + h &&
              check_solid_tile(vs, *x_ptr, cy, *w_ptr, 1, &color, true);
          cy++ );
    *h_ptr += cy - (*y_ptr + *h_ptr);

    /* ... to the left. */
    for ( cx = *x_ptr - 1;
          cx >= x && check_solid_tile(vs, cx, *y_ptr, 1, *h_ptr, &color, true);
          cx-- );
    *w_ptr += *x_ptr - (cx + 1);
    *x_ptr = cx + 1;

    /* ... to the right. */
    for ( cx = *x_ptr + *w_ptr;
          cx < x + w &&
              check_solid_tile(vs, cx, *y_ptr, 1, *h_ptr, &color, true);
          cx++ );
    *w_ptr += cx - (*x_ptr + *w_ptr);
}

static int tight_init_stream(VncState *vs, int stream_id,
                             int level, int strategy)
{
    z_streamp zstream = &vs->tight_stream[stream_id];

    if (zstream->opaque == NULL) {
        int err;

        VNC_DEBUG("VNC: TIGHT: initializing zlib stream %d\n", stream_id);
        VNC_DEBUG("VNC: TIGHT: opaque = %p | vs = %p\n", zstream->opaque, vs);
        zstream->zalloc = vnc_zlib_zalloc;
        zstream->zfree = vnc_zlib_zfree;

        err = deflateInit2(zstream, level, Z_DEFLATED, MAX_WBITS,
                           MAX_MEM_LEVEL, strategy);

        if (err != Z_OK) {
            fprintf(stderr, "VNC: error initializing zlib\n");
            return -1;
        }

        vs->tight_levels[stream_id] = level;
        zstream->opaque = vs;
    }

    if (vs->tight_levels[stream_id] != level) {
        if (deflateParams(zstream, level, strategy) != Z_OK) {
            return -1;
        }
        vs->tight_levels[stream_id] = level;
    }
    return 0;
}

static void tight_send_compact_size(VncState *vs, size_t len)
{
    int lpc = 0;
    int bytes = 0;
    char buf[3] = {0, 0, 0};

    buf[bytes++] = len & 0x7F;
    if (len > 0x7F) {
        buf[bytes-1] |= 0x80;
        buf[bytes++] = (len >> 7) & 0x7F;
        if (len > 0x3FFF) {
            buf[bytes-1] |= 0x80;
            buf[bytes++] = (len >> 14) & 0xFF;
        }
    }
    for (lpc = 0; lpc < bytes; lpc++) {
        vnc_write_u8(vs, buf[lpc]);
    }
}

static int tight_compress_data(VncState *vs, int stream_id, size_t bytes,
                               int level, int strategy)
{
    z_streamp zstream = &vs->tight_stream[stream_id];
    int previous_out;

    if (bytes < VNC_TIGHT_MIN_TO_COMPRESS) {
        vnc_write(vs, vs->tight.buffer, vs->tight.offset);
        return bytes;
    }

    if (tight_init_stream(vs, stream_id, level, strategy)) {
        return -1;
    }

    /* reserve memory in output buffer */
    buffer_reserve(&vs->tight_zlib, bytes + 64);

    /* set pointers */
    zstream->next_in = vs->tight.buffer;
    zstream->avail_in = vs->tight.offset;
    zstream->next_out = vs->tight_zlib.buffer + vs->tight_zlib.offset;
    zstream->avail_out = vs->tight_zlib.capacity - vs->tight_zlib.offset;
    zstream->data_type = Z_BINARY;
    previous_out = zstream->total_out;

    /* start encoding */
    if (deflate(zstream, Z_SYNC_FLUSH) != Z_OK) {
        fprintf(stderr, "VNC: error during tight compression\n");
        return -1;
    }

    vs->tight_zlib.offset = vs->tight_zlib.capacity - zstream->avail_out;
    bytes = zstream->total_out - previous_out;

    tight_send_compact_size(vs, bytes);
    vnc_write(vs, vs->tight_zlib.buffer, bytes);

    buffer_reset(&vs->tight_zlib);

    return bytes;
}

/*
 * Subencoding implementations.
 */
static void tight_pack24(VncState *vs, uint8_t *buf, size_t count, size_t *ret)
{
    uint32_t *buf32;
    uint32_t pix;
    int rshift, gshift, bshift;

    buf32 = (uint32_t *)buf;

    if ((vs->clientds.flags & QEMU_BIG_ENDIAN_FLAG) ==
        (vs->ds->surface->flags & QEMU_BIG_ENDIAN_FLAG)) {
        rshift = vs->clientds.pf.rshift;
        gshift = vs->clientds.pf.gshift;
        bshift = vs->clientds.pf.bshift;
    } else {
        rshift = 24 - vs->clientds.pf.rshift;
        gshift = 24 - vs->clientds.pf.gshift;
        bshift = 24 - vs->clientds.pf.bshift;
    }

    if (ret) {
        *ret = count * 3;
    }

    while (count--) {
        pix = *buf32++;
        *buf++ = (char)(pix >> rshift);
        *buf++ = (char)(pix >> gshift);
        *buf++ = (char)(pix >> bshift);
    }
}

static int send_full_color_rect(VncState *vs, int x, int y, int w, int h)
{
    int stream = 0;
    size_t bytes;

#ifdef CONFIG_VNC_PNG
    if (tight_can_send_png_rect(vs, w, h)) {
        return send_png_rect(vs, x, y, w, h, NULL);
    }
#endif

    vnc_write_u8(vs, stream << 4); /* no flushing, no filter */

    if (vs->tight_pixel24) {
        tight_pack24(vs, vs->tight.buffer, w * h, &vs->tight.offset);
        bytes = 3;
    } else {
        bytes = vs->clientds.pf.bytes_per_pixel;
    }

    bytes = tight_compress_data(vs, stream, w * h * bytes,
                                tight_conf[vs->tight_compression].raw_zlib_level,
                                Z_DEFAULT_STRATEGY);

    return (bytes >= 0);
}

static int send_solid_rect(VncState *vs)
{
    size_t bytes;

    vnc_write_u8(vs, VNC_TIGHT_FILL << 4); /* no flushing, no filter */

    if (vs->tight_pixel24) {
        tight_pack24(vs, vs->tight.buffer, 1, &vs->tight.offset);
        bytes = 3;
    } else {
        bytes = vs->clientds.pf.bytes_per_pixel;
    }

    vnc_write(vs, vs->tight.buffer, bytes);
    return 1;
}

static int send_mono_rect(VncState *vs, int x, int y,
                          int w, int h, uint32_t bg, uint32_t fg)
{
    size_t bytes;
    int stream = 1;
    int level = tight_conf[vs->tight_compression].mono_zlib_level;

#ifdef CONFIG_VNC_PNG
    if (tight_can_send_png_rect(vs, w, h)) {
        int ret;
        QDict *palette = qdict_new();
        int bpp = vs->clientds.pf.bytes_per_pixel * 8;

        tight_palette_insert(palette, bg, bpp, 2);
        tight_palette_insert(palette, fg, bpp, 2);
        ret = send_png_rect(vs, x, y, w, h, palette);
        QDECREF(palette);
        return ret;
    }
#endif

    bytes = ((w + 7) / 8) * h;

    vnc_write_u8(vs, (stream | VNC_TIGHT_EXPLICIT_FILTER) << 4);
    vnc_write_u8(vs, VNC_TIGHT_FILTER_PALETTE);
    vnc_write_u8(vs, 1);

    switch(vs->clientds.pf.bytes_per_pixel) {
    case 4:
    {
        uint32_t buf[2] = {bg, fg};
        size_t ret = sizeof (buf);

        if (vs->tight_pixel24) {
            tight_pack24(vs, (unsigned char*)buf, 2, &ret);
        }
        vnc_write(vs, buf, ret);

        tight_encode_mono_rect32(vs->tight.buffer, w, h, bg, fg);
        break;
    }
    case 2:
        vnc_write(vs, &bg, 2);
        vnc_write(vs, &fg, 2);
        tight_encode_mono_rect16(vs->tight.buffer, w, h, bg, fg);
        break;
    default:
        vnc_write_u8(vs, bg);
        vnc_write_u8(vs, fg);
        tight_encode_mono_rect8(vs->tight.buffer, w, h, bg, fg);
        break;
    }
    vs->tight.offset = bytes;

    bytes = tight_compress_data(vs, stream, bytes, level, Z_DEFAULT_STRATEGY);
    return (bytes >= 0);
}

struct palette_cb_priv {
    VncState *vs;
    uint8_t *header;
#ifdef CONFIG_VNC_PNG
    png_colorp png_palette;
#endif
};

static void write_palette(const char *key, QObject *obj, void *opaque)
{
    struct palette_cb_priv *priv = opaque;
    VncState *vs = priv->vs;
    uint32_t bytes = vs->clientds.pf.bytes_per_pixel;
    uint8_t idx = qint_get_int(qobject_to_qint(obj));

    if (bytes == 4) {
        uint32_t color = tight_palette_buf2rgb(32, (uint8_t *)key);

        ((uint32_t*)priv->header)[idx] = color;
    } else {
        uint16_t color = tight_palette_buf2rgb(16, (uint8_t *)key);

        ((uint16_t*)priv->header)[idx] = color;
    }
}

static bool send_gradient_rect(VncState *vs, int x, int y, int w, int h)
{
    int stream = 3;
    int level = tight_conf[vs->tight_compression].gradient_zlib_level;
    size_t bytes;

    if (vs->clientds.pf.bytes_per_pixel == 1)
        return send_full_color_rect(vs, x, y, w, h);

    vnc_write_u8(vs, (stream | VNC_TIGHT_EXPLICIT_FILTER) << 4);
    vnc_write_u8(vs, VNC_TIGHT_FILTER_GRADIENT);

    buffer_reserve(&vs->tight_gradient, w * 3 * sizeof (int));

    if (vs->tight_pixel24) {
        tight_filter_gradient24(vs, vs->tight.buffer, w, h);
        bytes = 3;
    } else if (vs->clientds.pf.bytes_per_pixel == 4) {
        tight_filter_gradient32(vs, (uint32_t *)vs->tight.buffer, w, h);
        bytes = 4;
    } else {
        tight_filter_gradient16(vs, (uint16_t *)vs->tight.buffer, w, h);
        bytes = 2;
    }

    buffer_reset(&vs->tight_gradient);

    bytes = w * h * bytes;
    vs->tight.offset = bytes;

    bytes = tight_compress_data(vs, stream, bytes,
                                level, Z_FILTERED);
    return (bytes >= 0);
}

static int send_palette_rect(VncState *vs, int x, int y,
                             int w, int h, struct QDict *palette)
{
    int stream = 2;
    int level = tight_conf[vs->tight_compression].idx_zlib_level;
    int colors;
    size_t bytes;

#ifdef CONFIG_VNC_PNG
    if (tight_can_send_png_rect(vs, w, h)) {
        return send_png_rect(vs, x, y, w, h, palette);
    }
#endif

    colors = qdict_size(palette);

    vnc_write_u8(vs, (stream | VNC_TIGHT_EXPLICIT_FILTER) << 4);
    vnc_write_u8(vs, VNC_TIGHT_FILTER_PALETTE);
    vnc_write_u8(vs, colors - 1);

    switch(vs->clientds.pf.bytes_per_pixel) {
    case 4:
    {
        size_t old_offset, offset;
        uint32_t header[qdict_size(palette)];
        struct palette_cb_priv priv = { vs, (uint8_t *)header };

        old_offset = vs->output.offset;
        qdict_iter(palette, write_palette, &priv);
        vnc_write(vs, header, sizeof(header));

        if (vs->tight_pixel24) {
            tight_pack24(vs, vs->output.buffer + old_offset, colors, &offset);
            vs->output.offset = old_offset + offset;
        }

        tight_encode_indexed_rect32(vs->tight.buffer, w * h, palette);
        break;
    }
    case 2:
    {
        uint16_t header[qdict_size(palette)];
        struct palette_cb_priv priv = { vs, (uint8_t *)header };

        qdict_iter(palette, write_palette, &priv);
        vnc_write(vs, header, sizeof(header));
        tight_encode_indexed_rect16(vs->tight.buffer, w * h, palette);
        break;
    }
    default:
        return -1; /* No palette for 8bits colors */
        break;
    }
    bytes = w * h;
    vs->tight.offset = bytes;

    bytes = tight_compress_data(vs, stream, bytes,
                                level, Z_DEFAULT_STRATEGY);
    return (bytes >= 0);
}

#if defined(CONFIG_VNC_JPEG) || defined(CONFIG_VNC_PNG)
static void rgb_prepare_row24(VncState *vs, uint8_t *dst, int x, int y,
                              int count)
{
    VncDisplay *vd = vs->vd;
    uint32_t *fbptr;
    uint32_t pix;

    fbptr = (uint32_t *)(vd->server->data + y * ds_get_linesize(vs->ds) +
                         x * ds_get_bytes_per_pixel(vs->ds));

    while (count--) {
        pix = *fbptr++;
        *dst++ = (uint8_t)(pix >> vs->ds->surface->pf.rshift);
        *dst++ = (uint8_t)(pix >> vs->ds->surface->pf.gshift);
        *dst++ = (uint8_t)(pix >> vs->ds->surface->pf.bshift);
    }
}

#define DEFINE_RGB_GET_ROW_FUNCTION(bpp)                                \
                                                                        \
    static void                                                         \
    rgb_prepare_row##bpp(VncState *vs, uint8_t *dst,                    \
                         int x, int y, int count)                       \
    {                                                                   \
        VncDisplay *vd = vs->vd;                                        \
        uint##bpp##_t *fbptr;                                           \
        uint##bpp##_t pix;                                              \
        int r, g, b;                                                    \
                                                                        \
        fbptr = (uint##bpp##_t *)                                       \
            (vd->server->data + y * ds_get_linesize(vs->ds) +           \
             x * ds_get_bytes_per_pixel(vs->ds));                       \
                                                                        \
        while (count--) {                                               \
            pix = *fbptr++;                                             \
                                                                        \
            r = (int)((pix >> vs->ds->surface->pf.rshift)               \
                      & vs->ds->surface->pf.rmax);                      \
            g = (int)((pix >> vs->ds->surface->pf.gshift)               \
                      & vs->ds->surface->pf.gmax);                      \
            b = (int)((pix >> vs->ds->surface->pf.bshift)               \
                      & vs->ds->surface->pf.bmax);                      \
                                                                        \
            *dst++ = (uint8_t)((r * 255 + vs->ds->surface->pf.rmax / 2) \
                               / vs->ds->surface->pf.rmax);             \
            *dst++ = (uint8_t)((g * 255 + vs->ds->surface->pf.gmax / 2) \
                               / vs->ds->surface->pf.gmax);             \
            *dst++ = (uint8_t)((b * 255 + vs->ds->surface->pf.bmax / 2) \
                               / vs->ds->surface->pf.bmax);             \
        }                                                               \
    }

DEFINE_RGB_GET_ROW_FUNCTION(16)
DEFINE_RGB_GET_ROW_FUNCTION(32)

static void rgb_prepare_row(VncState *vs, uint8_t *dst, int x, int y,
                            int count)
{
    if (vs->tight_pixel24)
        rgb_prepare_row24(vs, dst, x, y, count);
    else if (ds_get_bytes_per_pixel(vs->ds) == 4)
        rgb_prepare_row32(vs, dst, x, y, count);
    else
        rgb_prepare_row16(vs, dst, x, y, count);
}
#endif /* CONFIG_VNC_JPEG or CONFIG_VNC_PNG */

/*
 * JPEG compression stuff.
 */
#ifdef CONFIG_VNC_JPEG
/*
 * Destination manager implementation for JPEG library.
 */

/* This is called once per encoding */
static void jpeg_init_destination(j_compress_ptr cinfo)
{
    VncState *vs = cinfo->client_data;
    Buffer *buffer = &vs->tight_jpeg;

    cinfo->dest->next_output_byte = (JOCTET *)buffer->buffer + buffer->offset;
    cinfo->dest->free_in_buffer = (size_t)(buffer->capacity - buffer->offset);
}

/* This is called when we ran out of buffer (shouldn't happen!) */
static boolean jpeg_empty_output_buffer(j_compress_ptr cinfo)
{
    VncState *vs = cinfo->client_data;
    Buffer *buffer = &vs->tight_jpeg;

    buffer->offset = buffer->capacity;
    buffer_reserve(buffer, 2048);
    jpeg_init_destination(cinfo);
    return TRUE;
}

/* This is called when we are done processing data */
static void jpeg_term_destination(j_compress_ptr cinfo)
{
    VncState *vs = cinfo->client_data;
    Buffer *buffer = &vs->tight_jpeg;

    buffer->offset = buffer->capacity - cinfo->dest->free_in_buffer;
}

static int send_jpeg_rect(VncState *vs, int x, int y, int w, int h, int quality)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    struct jpeg_destination_mgr manager;
    JSAMPROW row[1];
    uint8_t *buf;
    int dy;

    if (ds_get_bytes_per_pixel(vs->ds) == 1)
        return send_full_color_rect(vs, x, y, w, h);

    buffer_reserve(&vs->tight_jpeg, 2048);

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    cinfo.client_data = vs;
    cinfo.image_width = w;
    cinfo.image_height = h;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, true);

    manager.init_destination = jpeg_init_destination;
    manager.empty_output_buffer = jpeg_empty_output_buffer;
    manager.term_destination = jpeg_term_destination;
    cinfo.dest = &manager;

    jpeg_start_compress(&cinfo, true);

    buf = qemu_malloc(w * 3);
    row[0] = buf;
    for (dy = 0; dy < h; dy++) {
        rgb_prepare_row(vs, buf, x, y + dy, w);
        jpeg_write_scanlines(&cinfo, row, 1);
    }
    qemu_free(buf);

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    vnc_write_u8(vs, VNC_TIGHT_JPEG << 4);

    tight_send_compact_size(vs, vs->tight_jpeg.offset);
    vnc_write(vs, vs->tight_jpeg.buffer, vs->tight_jpeg.offset);
    buffer_reset(&vs->tight_jpeg);

    return 1;
}
#endif /* CONFIG_VNC_JPEG */

/*
 * PNG compression stuff.
 */
#ifdef CONFIG_VNC_PNG
static void write_png_palette(const char *key, QObject *obj, void *opaque)
{
    struct palette_cb_priv *priv = opaque;
    VncState *vs = priv->vs;
    uint32_t bytes = vs->clientds.pf.bytes_per_pixel;
    uint8_t idx = qint_get_int(qobject_to_qint(obj));
    png_colorp color = &priv->png_palette[idx];
    uint32_t pix;

    if (bytes == 4) {
        pix = tight_palette_buf2rgb(32, (uint8_t *)key);
    } else {
        pix = tight_palette_buf2rgb(16, (uint8_t *)key);
    }

    if (vs->tight_pixel24)
    {
        color->red = (pix >> vs->clientds.pf.rshift) & vs->clientds.pf.rmax;
        color->green = (pix >> vs->clientds.pf.gshift) & vs->clientds.pf.gmax;
        color->blue = (pix >> vs->clientds.pf.bshift) & vs->clientds.pf.bmax;
    }
    else
    {
        int red, green, blue;

        red = (pix >> vs->clientds.pf.rshift) & vs->clientds.pf.rmax;
        green = (pix >> vs->clientds.pf.gshift) & vs->clientds.pf.gmax;
        blue = (pix >> vs->clientds.pf.bshift) & vs->clientds.pf.bmax;
        color->red = ((red * 255 + vs->clientds.pf.rmax / 2) /
                      vs->clientds.pf.rmax);
        color->green = ((green * 255 + vs->clientds.pf.gmax / 2) /
                        vs->clientds.pf.gmax);
        color->blue = ((blue * 255 + vs->clientds.pf.bmax / 2) /
                       vs->clientds.pf.bmax);
    }
}

static void png_write_data(png_structp png_ptr, png_bytep data,
                           png_size_t length)
{
    VncState *vs = png_get_io_ptr(png_ptr);

    buffer_reserve(&vs->tight_png, vs->tight_png.offset + length);
    memcpy(vs->tight_png.buffer + vs->tight_png.offset, data, length);

    vs->tight_png.offset += length;
}

static void png_flush_data(png_structp png_ptr)
{
}

static void *vnc_png_malloc(png_structp png_ptr, png_size_t size)
{
    return qemu_malloc(size);
}

static void vnc_png_free(png_structp png_ptr, png_voidp ptr)
{
    qemu_free(ptr);
}

static int send_png_rect(VncState *vs, int x, int y, int w, int h,
                         QDict *palette)
{
    png_byte color_type;
    png_structp png_ptr;
    png_infop info_ptr;
    png_colorp png_palette = NULL;
    size_t offset;
    int level = tight_png_conf[vs->tight_compression].png_zlib_level;
    int filters = tight_png_conf[vs->tight_compression].png_filters;
    uint8_t *buf;
    int dy;

    png_ptr = png_create_write_struct_2(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL,
                                        NULL, vnc_png_malloc, vnc_png_free);

    if (png_ptr == NULL)
        return -1;

    info_ptr = png_create_info_struct(png_ptr);

    if (info_ptr == NULL) {
        png_destroy_write_struct(&png_ptr, NULL);
        return -1;
    }

    png_set_write_fn(png_ptr, (void *) vs, png_write_data, png_flush_data);
    png_set_compression_level(png_ptr, level);
    png_set_filter(png_ptr, PNG_FILTER_TYPE_DEFAULT, filters);

    if (palette) {
        color_type = PNG_COLOR_TYPE_PALETTE;
    } else {
        color_type = PNG_COLOR_TYPE_RGB;
    }

    png_set_IHDR(png_ptr, info_ptr, w, h,
                 8, color_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        struct palette_cb_priv priv;

        png_palette = png_malloc(png_ptr, sizeof(*png_palette) *
                                 qdict_size(palette));

        priv.vs = vs;
        priv.png_palette = png_palette;
        qdict_iter(palette, write_png_palette, &priv);

        png_set_PLTE(png_ptr, info_ptr, png_palette, qdict_size(palette));

        offset = vs->tight.offset;
        if (vs->clientds.pf.bytes_per_pixel == 4) {
            tight_encode_indexed_rect32(vs->tight.buffer, w * h, palette);
        } else {
            tight_encode_indexed_rect16(vs->tight.buffer, w * h, palette);
        }
    }

    png_write_info(png_ptr, info_ptr);

    buffer_reserve(&vs->tight_png, 2048);
    buf = qemu_malloc(w * 3);
    for (dy = 0; dy < h; dy++)
    {
        if (color_type == PNG_COLOR_TYPE_PALETTE) {
            memcpy(buf, vs->tight.buffer + (dy * w), w);
        } else {
            rgb_prepare_row(vs, buf, x, y + dy, w);
        }
        png_write_row(png_ptr, buf);
    }
    qemu_free(buf);

    png_write_end(png_ptr, NULL);

    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_free(png_ptr, png_palette);
    }

    png_destroy_write_struct(&png_ptr, &info_ptr);

    vnc_write_u8(vs, VNC_TIGHT_PNG << 4);

    tight_send_compact_size(vs, vs->tight_png.offset);
    vnc_write(vs, vs->tight_png.buffer, vs->tight_png.offset);
    buffer_reset(&vs->tight_png);
    return 1;
}
#endif /* CONFIG_VNC_PNG */

static void vnc_tight_start(VncState *vs)
{
    buffer_reset(&vs->tight);

    // make the output buffer be the zlib buffer, so we can compress it later
    vs->tight_tmp = vs->output;
    vs->output = vs->tight;
}

static void vnc_tight_stop(VncState *vs)
{
    // switch back to normal output/zlib buffers
    vs->tight = vs->output;
    vs->output = vs->tight_tmp;
}

static int send_sub_rect(VncState *vs, int x, int y, int w, int h)
{
    struct QDict *palette = NULL;
    uint32_t bg = 0, fg = 0;
    int colors;
    int ret = 0;

    vnc_framebuffer_update(vs, x, y, w, h, vs->tight_type);

    vnc_tight_start(vs);
    vnc_raw_send_framebuffer_update(vs, x, y, w, h);
    vnc_tight_stop(vs);

    colors = tight_fill_palette(vs, x, y, w * h, &fg, &bg, &palette);

    if (colors == 0) {
        if (tight_detect_smooth_image(vs, w, h)) {
            if (vs->tight_quality == -1) {
                ret = send_gradient_rect(vs, x, y, w, h);
            } else {
#ifdef CONFIG_VNC_JPEG
                int quality = tight_conf[vs->tight_quality].jpeg_quality;

                ret = send_jpeg_rect(vs, x, y, w, h, quality);
#else
                ret = send_full_color_rect(vs, x, y, w, h);
#endif
            }
        } else {
            ret = send_full_color_rect(vs, x, y, w, h);
        }
    } else if (colors == 1) {
        ret = send_solid_rect(vs);
    } else if (colors == 2) {
        ret = send_mono_rect(vs, x, y, w, h, bg, fg);
    } else if (colors <= 256) {
#ifdef CONFIG_VNC_JPEG
        if (colors > 96 && vs->tight_quality != -1 && vs->tight_quality <= 3 &&
            tight_detect_smooth_image(vs, w, h)) {
            int quality = tight_conf[vs->tight_quality].jpeg_quality;

            ret = send_jpeg_rect(vs, x, y, w, h, quality);
        } else {
            ret = send_palette_rect(vs, x, y, w, h, palette);
        }
#else
        ret = send_palette_rect(vs, x, y, w, h, palette);
#endif
    }
    QDECREF(palette);
    return ret;
}

static int send_sub_rect_solid(VncState *vs, int x, int y, int w, int h)
{
    vnc_framebuffer_update(vs, x, y, w, h, vs->tight_type);

    vnc_tight_start(vs);
    vnc_raw_send_framebuffer_update(vs, x, y, w, h);
    vnc_tight_stop(vs);

    return send_solid_rect(vs);
}

static int send_rect_simple(VncState *vs, int x, int y, int w, int h)
{
    int max_size, max_width;
    int max_sub_width, max_sub_height;
    int dx, dy;
    int rw, rh;
    int n = 0;

    max_size = tight_conf[vs->tight_compression].max_rect_size;
    max_width = tight_conf[vs->tight_compression].max_rect_width;

    if (w > max_width || w * h > max_size) {
        max_sub_width = (w > max_width) ? max_width : w;
        max_sub_height = max_size / max_sub_width;

        for (dy = 0; dy < h; dy += max_sub_height) {
            for (dx = 0; dx < w; dx += max_width) {
                rw = MIN(max_sub_width, w - dx);
                rh = MIN(max_sub_height, h - dy);
                n += send_sub_rect(vs, x+dx, y+dy, rw, rh);
            }
        }
    } else {
        n += send_sub_rect(vs, x, y, w, h);
    }

    return n;
}

static int find_large_solid_color_rect(VncState *vs, int x, int y,
                                       int w, int h, int max_rows)
{
    int dx, dy, dw, dh;
    int n = 0;

    /* Try to find large solid-color areas and send them separately. */

    for (dy = y; dy < y + h; dy += VNC_TIGHT_MAX_SPLIT_TILE_SIZE) {

        /* If a rectangle becomes too large, send its upper part now. */

        if (dy - y >= max_rows) {
            n += send_rect_simple(vs, x, y, w, max_rows);
            y += max_rows;
            h -= max_rows;
        }

        dh = MIN(VNC_TIGHT_MAX_SPLIT_TILE_SIZE, (y + h - dy));

        for (dx = x; dx < x + w; dx += VNC_TIGHT_MAX_SPLIT_TILE_SIZE) {
            uint32_t color_value;
            int x_best, y_best, w_best, h_best;

            dw = MIN(VNC_TIGHT_MAX_SPLIT_TILE_SIZE, (x + w - dx));

            if (!check_solid_tile(vs, dx, dy, dw, dh, &color_value, false)) {
                continue ;
            }

            /* Get dimensions of solid-color area. */

            find_best_solid_area(vs, dx, dy, w - (dx - x), h - (dy - y),
                                 color_value, &w_best, &h_best);

            /* Make sure a solid rectangle is large enough
               (or the whole rectangle is of the same color). */

            if (w_best * h_best != w * h &&
                w_best * h_best < VNC_TIGHT_MIN_SOLID_SUBRECT_SIZE) {
                continue;
            }

            /* Try to extend solid rectangle to maximum size. */

            x_best = dx; y_best = dy;
            extend_solid_area(vs, x, y, w, h, color_value,
                              &x_best, &y_best, &w_best, &h_best);

            /* Send rectangles at top and left to solid-color area. */

            if (y_best != y) {
                n += send_rect_simple(vs, x, y, w, y_best-y);
            }
            if (x_best != x) {
                n += tight_send_framebuffer_update(vs, x, y_best,
                                                   x_best-x, h_best);
            }

            /* Send solid-color rectangle. */
            n += send_sub_rect_solid(vs, x_best, y_best, w_best, h_best);

            /* Send remaining rectangles (at right and bottom). */

            if (x_best + w_best != x + w) {
                n += tight_send_framebuffer_update(vs, x_best+w_best,
                                                   y_best,
                                                   w-(x_best-x)-w_best,
                                                   h_best);
            }
            if (y_best + h_best != y + h) {
                n += tight_send_framebuffer_update(vs, x, y_best+h_best,
                                                   w, h-(y_best-y)-h_best);
            }

            /* Return after all recursive calls are done. */
            return n;
        }
    }
    return n + send_rect_simple(vs, x, y, w, h);
}

static int tight_send_framebuffer_update(VncState *vs, int x, int y,
                                         int w, int h)
{
    int max_rows;

    if (vs->clientds.pf.bytes_per_pixel == 4 && vs->clientds.pf.rmax == 0xFF &&
        vs->clientds.pf.bmax == 0xFF && vs->clientds.pf.gmax == 0xFF) {
        vs->tight_pixel24 = true;
    } else {
        vs->tight_pixel24 = false;
    }

    if (w * h < VNC_TIGHT_MIN_SPLIT_RECT_SIZE)
        return send_rect_simple(vs, x, y, w, h);

    /* Calculate maximum number of rows in one non-solid rectangle. */

    max_rows = tight_conf[vs->tight_compression].max_rect_size;
    max_rows /= MIN(tight_conf[vs->tight_compression].max_rect_width, w);

    return find_large_solid_color_rect(vs, x, y, w, h, max_rows);
}

int vnc_tight_send_framebuffer_update(VncState *vs, int x, int y,
                                      int w, int h)
{
    vs->tight_type = VNC_ENCODING_TIGHT;
    return tight_send_framebuffer_update(vs, x, y, w, h);
}

int vnc_tight_png_send_framebuffer_update(VncState *vs, int x, int y,
                                          int w, int h)
{
    vs->tight_type = VNC_ENCODING_TIGHT_PNG;
    return tight_send_framebuffer_update(vs, x, y, w, h);
}

void vnc_tight_clear(VncState *vs)
{
    int i;
    for (i=0; i<ARRAY_SIZE(vs->tight_stream); i++) {
        if (vs->tight_stream[i].opaque) {
            deflateEnd(&vs->tight_stream[i]);
        }
    }

    buffer_free(&vs->tight);
    buffer_free(&vs->tight_zlib);
    buffer_free(&vs->tight_gradient);
#ifdef CONFIG_VNC_JPEG
    buffer_free(&vs->tight_jpeg);
#endif
}
