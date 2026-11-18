#ifndef YUV_H
#define YUV_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 色度上采样方式 */
enum {
    YUV_UPSAMPLE_NEAREST = 0,
    YUV_UPSAMPLE_BILINEAR = 1
};

/* 字节序：输出 RGB 三通道紧排 */
typedef struct YuvFrame {
    const uint8_t *y;
    const uint8_t *u;
    const uint8_t *v;
    int y_stride;
    int uv_stride;
    int width;
    int height;
} YuvFrame;

/* 把 YUV420（BT.601 limited range）转成 RGB888。
 * rgb_stride 是输出每行字节数，必须 >= width*3。
 * 返回 0 成功，-1 参数非法。 */
int yuv420_to_rgb(const YuvFrame *frame, uint8_t *rgb, int rgb_stride, int upsample);

/* 计算色度平面尺寸（向上取整）。 */
int yuv_chroma_width(int width);
int yuv_chroma_height(int height);

#ifdef __cplusplus
}
#endif

#endif
