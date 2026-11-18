#include "yuv.h"

#include <limits.h>
#include <stddef.h>

/* 色度平面尺寸：宽高不是偶数时向上取整。
 * 用 w / 2 + (w & 1) 而不是 (w + 1) / 2，避免 w 接近 INT_MAX 时溢出。 */
int yuv_chroma_width(int width) {
    return (width > 0) ? width / 2 + (width & 1) : 0;
}

int yuv_chroma_height(int height) {
    return (height > 0) ? height / 2 + (height & 1) : 0;
}

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int round_div(int num, int den) {
    return (num >= 0) ? (num + den / 2) / den : -((-num + den / 2) / den);
}

/* 单个像素的 YUV->RGB（BT.601 limited range）。内层循环里调用，static inline 可被完全内联。 */
static inline void store_rgb(int yy, int cu, int cv, uint8_t *dst) {
    int c = yy - 16;
    int d = cu - 128;
    int e = cv - 128;
    if (c < 0) c = 0;

    int r = round_div(298 * c + 409 * e + 128, 256);
    int g = round_div(298 * c - 100 * d - 208 * e + 128, 256);
    int b = round_div(298 * c + 516 * d + 128, 256);

    dst[0] = (uint8_t)clampi(r, 0, 255);
    dst[1] = (uint8_t)clampi(g, 0, 255);
    dst[2] = (uint8_t)clampi(b, 0, 255);
}

/* 双线性上采样用的整数坐标。
 * 亮度像素中心 (p + 0.5, ...) 映射到色度坐标 g = (p + 0.5) / 2 - 0.5 = (2p - 1) / 4，
 * 即 g = q + f4 / 4，q 向下取整。p >= 0，只有 p == 0 时 2p - 1 < 0，向下取整得 -1。 */
static inline void chroma_pos(int p, int *i0, int *i1, int *f4) {
    int t = 2 * p - 1;
    int q = (t >= 0) ? t / 4 : -1;
    *i0 = q;
    *i1 = q + 1;
    *f4 = t - 4 * q; /* 0..3 */
}

int yuv420_to_rgb(const YuvFrame *f, uint8_t *rgb, int rgb_stride, int upsample) {
    if (!f || !rgb) return -1;
    if (f->width <= 0 || f->height <= 0) return -1;
    if (!f->y || !f->u || !f->v) return -1;
    if (f->width > (INT_MAX - 2) / 3) return -1;
    if (rgb_stride < f->width * 3) return -1;
    if (f->y_stride < f->width) return -1;
    if (upsample != YUV_UPSAMPLE_NEAREST && upsample != YUV_UPSAMPLE_BILINEAR) return -1;

    const int cw = yuv_chroma_width(f->width);
    const int ch = yuv_chroma_height(f->height);
    if (f->uv_stride < cw) return -1;

    for (int y = 0; y < f->height; y++) {
        const uint8_t *yrow = f->y + (size_t)y * (size_t)f->y_stride;
        uint8_t *orow = rgb + (size_t)y * (size_t)rgb_stride;

        if (upsample == YUV_UPSAMPLE_NEAREST) {
            const int cy = clampi(y >> 1, 0, ch - 1);
            const uint8_t *urow = f->u + (size_t)cy * (size_t)f->uv_stride;
            const uint8_t *vrow = f->v + (size_t)cy * (size_t)f->uv_stride;

            for (int x = 0; x < f->width; x++) {
                const int cx = clampi(x >> 1, 0, cw - 1);
                store_rgb(yrow[x], urow[cx], vrow[cx], orow + (size_t)x * 3);
            }
        } else {
            int y0, y1, fy4;
            chroma_pos(y, &y0, &y1, &fy4);
            y0 = clampi(y0, 0, ch - 1);
            y1 = clampi(y1, 0, ch - 1);

            const uint8_t *u0 = f->u + (size_t)y0 * (size_t)f->uv_stride;
            const uint8_t *u1 = f->u + (size_t)y1 * (size_t)f->uv_stride;
            const uint8_t *v0 = f->v + (size_t)y0 * (size_t)f->uv_stride;
            const uint8_t *v1 = f->v + (size_t)y1 * (size_t)f->uv_stride;
            const int wy0 = 4 - fy4;
            const int wy1 = fy4;

            for (int x = 0; x < f->width; x++) {
                int x0, x1, fx4;
                chroma_pos(x, &x0, &x1, &fx4);
                x0 = clampi(x0, 0, cw - 1);
                x1 = clampi(x1, 0, cw - 1);

                const int wx0 = 4 - fx4;
                const int wx1 = fx4;
                int u16 = u0[x0] * wx0 * wy0 + u0[x1] * wx1 * wy0 +
                          u1[x0] * wx0 * wy1 + u1[x1] * wx1 * wy1;
                int v16 = v0[x0] * wx0 * wy0 + v0[x1] * wx1 * wy0 +
                          v1[x0] * wx0 * wy1 + v1[x1] * wx1 * wy1;

                store_rgb(yrow[x], (u16 + 8) >> 4, (v16 + 8) >> 4,
                          orow + (size_t)x * 3);
            }
        }
    }
    return 0;
}
