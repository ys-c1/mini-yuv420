#include "yuv.h"

#include <math.h>
#include <stddef.h>

int yuv_chroma_width(int width) {
    return width / 2;
}

int yuv_chroma_height(int height) {
    return height / 2;
}

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int round_div(int num, int den) {
    return (num >= 0) ? (num + den / 2) / den : -((-num + den / 2) / den);
}

/* 取某点的色度值。nearest 直接按 /2 取整；bilinear 按像素中心在 2x2 色度格上插值。 */
static void sample_chroma(const YuvFrame *f, int x, int y, int upsample, int *cu, int *cv) {
    int cw = yuv_chroma_width(f->width);
    int ch = yuv_chroma_height(f->height);

    if (upsample == YUV_UPSAMPLE_NEAREST) {
        int cx = clampi(x / 2, 0, cw - 1);
        int cy = clampi(y / 2, 0, ch - 1);
        *cu = f->u[cy * f->uv_stride + cx];
        *cv = f->v[cy * f->uv_stride + cx];
        return;
    }

    /* 像素中心：亮度 (x+0.5, y+0.5) 对应色度坐标 (x+0.5)/2 - 0.5 */
    float gx = ((float)x + 0.5f) * 0.5f - 0.5f;
    float gy = ((float)y + 0.5f) * 0.5f - 0.5f;
    int x0 = (int)floorf(gx);
    int y0 = (int)floorf(gy);
    float fx = gx - (float)x0;
    float fy = gy - (float)y0;
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    x0 = clampi(x0, 0, cw - 1);
    x1 = clampi(x1, 0, cw - 1);
    y0 = clampi(y0, 0, ch - 1);
    y1 = clampi(y1, 0, ch - 1);

    float u00 = f->u[y0 * f->uv_stride + x0];
    float u10 = f->u[y0 * f->uv_stride + x1];
    float u01 = f->u[y1 * f->uv_stride + x0];
    float u11 = f->u[y1 * f->uv_stride + x1];
    float v00 = f->v[y0 * f->uv_stride + x0];
    float v10 = f->v[y0 * f->uv_stride + x1];
    float v01 = f->v[y1 * f->uv_stride + x0];
    float v11 = f->v[y1 * f->uv_stride + x1];

    float u = (u00 * (1 - fx) + u10 * fx) * (1 - fy) + (u01 * (1 - fx) + u11 * fx) * fy;
    float v = (v00 * (1 - fx) + v10 * fx) * (1 - fy) + (v01 * (1 - fx) + v11 * fx) * fy;
    *cu = (int)lrintf(u);
    *cv = (int)lrintf(v);
}

int yuv420_to_rgb(const YuvFrame *f, uint8_t *rgb, int rgb_stride, int upsample) {
    if (!f || !rgb) return -1;
    if (f->width <= 0 || f->height <= 0) return -1;
    if (!f->y || !f->u || !f->v) return -1;
    if (rgb_stride < f->width * 3) return -1;
    if (f->y_stride < f->width) return -1;
    if (f->uv_stride < yuv_chroma_width(f->width)) return -1;
    if (upsample != YUV_UPSAMPLE_NEAREST && upsample != YUV_UPSAMPLE_BILINEAR) return -1;

    for (int y = 0; y < f->height; y++) {
        const uint8_t *row = f->y + (size_t)y * (size_t)f->y_stride;
        uint8_t *out = rgb + (size_t)y * (size_t)(f->width * 3);
        for (int x = 0; x < f->width; x++) {
            int cu = 0;
            int cv = 0;
            sample_chroma(f, x, y, upsample, &cu, &cv);

            int yy = row[x];
            int c = yy - 16;
            int d = cu - 128;
            int e = cv - 128;
            if (c < 0) c = 0;

            int r = round_div(298 * c + 409 * e + 128, 256);
            int g = round_div(298 * c - 100 * d - 208 * e + 128, 256);
            int b = round_div(298 * c + 516 * d + 128, 256);

            out[x * 3 + 0] = (uint8_t)(r > 255 ? 255 : r);
            out[x * 3 + 1] = (uint8_t)(g > 255 ? 255 : g);
            out[x * 3 + 2] = (uint8_t)(b > 255 ? 255 : b);
        }
    }
    return 0;
}
