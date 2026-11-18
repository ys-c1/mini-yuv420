#include "../src/yuv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 生成一帧渐变测试图，转成 RGB 后打印若干像素，并给出最近邻/双线性的差异统计。 */
int main(void) {
    const int w = 64;
    const int h = 48;
    const int cw = yuv_chroma_width(w);
    const int ch = yuv_chroma_height(h);

    YuvFrame f;
    memset(&f, 0, sizeof(f));
    f.width = w;
    f.height = h;
    f.y_stride = w;
    f.uv_stride = cw;

    uint8_t *y = (uint8_t *)malloc((size_t)w * h);
    uint8_t *u = (uint8_t *)malloc((size_t)cw * ch);
    uint8_t *v = (uint8_t *)malloc((size_t)cw * ch);
    if (!y || !u || !v) {
        printf("alloc failed\n");
        return 1;
    }
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            y[j * w + i] = (uint8_t)((i * 4 + j * 2) & 0xFF);
        }
    }
    for (int j = 0; j < ch; j++) {
        for (int i = 0; i < cw; i++) {
            u[j * cw + i] = (uint8_t)(64 + i * 2);
            v[j * cw + i] = (uint8_t)(192 - j * 3);
        }
    }
    f.y = y;
    f.u = u;
    f.v = v;

    int stride = w * 3;
    uint8_t *near_rgb = (uint8_t *)malloc((size_t)stride * h);
    uint8_t *lin_rgb = (uint8_t *)malloc((size_t)stride * h);
    if (!near_rgb || !lin_rgb) {
        printf("alloc failed\n");
        return 1;
    }
    if (yuv420_to_rgb(&f, near_rgb, stride, YUV_UPSAMPLE_NEAREST) != 0) {
        printf("nearest failed\n");
        return 1;
    }
    if (yuv420_to_rgb(&f, lin_rgb, stride, YUV_UPSAMPLE_BILINEAR) != 0) {
        printf("bilinear failed\n");
        return 1;
    }

    int diff = 0;
    for (int i = 0; i < stride * h; i++) {
        if (near_rgb[i] != lin_rgb[i]) diff++;
    }
    printf("frame=%dx%d chroma=%dx%d differing_bytes=%d\n", w, h, cw, ch, diff);
    printf("px(0,0)=%d,%d,%d px(32,24)=%d,%d,%d\n",
           near_rgb[0], near_rgb[1], near_rgb[2],
           near_rgb[(24 * stride) + 32 * 3], near_rgb[(24 * stride) + 32 * 3 + 1],
           near_rgb[(24 * stride) + 32 * 3 + 2]);

    free(y);
    free(u);
    free(v);
    free(near_rgb);
    free(lin_rgb);
    return 0;
}
