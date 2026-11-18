#include "../src/yuv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        g_checks++;                                       \
        if (!(cond)) {                                    \
            g_failures++;                                 \
            printf("FAIL %s:%d  ", __FILE__, __LINE__);   \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

typedef struct Frame {
    uint8_t *y;
    uint8_t *u;
    uint8_t *v;
    YuvFrame view;
    int y_stride;
    int uv_stride;
    int w;
    int h;
} Frame;

static int frame_alloc(Frame *f, int w, int h, int y_pad, int uv_pad) {
    memset(f, 0, sizeof(*f));
    f->w = w;
    f->h = h;
    f->y_stride = w + y_pad;
    f->uv_stride = yuv_chroma_width(w) + uv_pad;
    f->y = (uint8_t *)calloc((size_t)f->y_stride * (size_t)h, 1);
    f->u = (uint8_t *)calloc((size_t)f->uv_stride * (size_t)yuv_chroma_height(h), 1);
    f->v = (uint8_t *)calloc((size_t)f->uv_stride * (size_t)yuv_chroma_height(h), 1);
    if (!f->y || !f->u || !f->v) return -1;
    f->view.y = f->y;
    f->view.u = f->u;
    f->view.v = f->v;
    f->view.y_stride = f->y_stride;
    f->view.uv_stride = f->uv_stride;
    f->view.width = w;
    f->view.height = h;
    return 0;
}

static void frame_free(Frame *f) {
    free(f->y);
    free(f->u);
    free(f->v);
    memset(f, 0, sizeof(*f));
}

/* 全平面常数：Y=126, U=V=128 时接近中性灰 (128,128,128)。 */
static void test_flat_neutral(void) {
    Frame f;
    CHECK(frame_alloc(&f, 4, 4, 0, 0) == 0, "分配失败");
    memset(f.y, 126, (size_t)f.y_stride * 4);
    memset(f.u, 128, (size_t)f.uv_stride * 2);
    memset(f.v, 128, (size_t)f.uv_stride * 2);

    uint8_t rgb[4 * 4 * 3];
    CHECK(yuv420_to_rgb(&f.view, rgb, 4 * 3, YUV_UPSAMPLE_NEAREST) == 0, "转换失败");
    for (int i = 0; i < 16; i++) {
        CHECK(abs((int)rgb[i * 3 + 0] - 128) <= 2, "R 偏离中性灰：%d", rgb[i * 3 + 0]);
        CHECK(abs((int)rgb[i * 3 + 1] - 128) <= 2, "G 偏离中性灰：%d", rgb[i * 3 + 1]);
        CHECK(abs((int)rgb[i * 3 + 2] - 128) <= 2, "B 偏离中性灰：%d", rgb[i * 3 + 2]);
    }
    frame_free(&f);
}

/* 带行填充的输入，输出必须与无填充完全一致。 */
static void test_input_stride(void) {
    Frame tight, padded;
    CHECK(frame_alloc(&tight, 6, 4, 0, 0) == 0, "分配失败");
    CHECK(frame_alloc(&padded, 6, 4, 7, 5) == 0, "分配失败");

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 6; x++) {
            int val = (x * 31 + y * 17) & 0xFF;
            tight.y[y * tight.y_stride + x] = (uint8_t)val;
            padded.y[y * padded.y_stride + x] = (uint8_t)val;
        }
    }
    for (int y = 0; y < 2; y++) {
        for (int x = 0; x < 3; x++) {
            int val = (x * 23 + y * 41 + 7) & 0xFF;
            tight.u[y * tight.uv_stride + x] = (uint8_t)val;
            padded.u[y * padded.uv_stride + x] = (uint8_t)val;
            val = (x * 13 + y * 29 + 3) & 0xFF;
            tight.v[y * tight.uv_stride + x] = (uint8_t)val;
            padded.v[y * padded.uv_stride + x] = (uint8_t)val;
        }
    }

    uint8_t a[6 * 4 * 3];
    uint8_t b[6 * 4 * 3];
    CHECK(yuv420_to_rgb(&tight.view, a, 6 * 3, YUV_UPSAMPLE_NEAREST) == 0, "紧排转换失败");
    CHECK(yuv420_to_rgb(&padded.view, b, 6 * 3, YUV_UPSAMPLE_NEAREST) == 0, "填充转换失败");
    CHECK(memcmp(a, b, sizeof(a)) == 0, "行填充影响了输出");
    frame_free(&tight);
    frame_free(&padded);
}

/* 输出行填充：跨度大于 width*3 时，每行前 width*3 字节必须正确，行间不串。 */
static void test_output_stride(void) {
    Frame f;
    CHECK(frame_alloc(&f, 5, 3, 0, 0) == 0, "分配失败");
    memset(f.y, 100, (size_t)f.y_stride * 3);
    memset(f.u, 140, (size_t)f.uv_stride * 2);
    memset(f.v, 120, (size_t)f.uv_stride * 2);

    const int stride = 5 * 3 + 8;
    uint8_t *rgb = (uint8_t *)malloc((size_t)stride * 3);
    memset(rgb, 0xAB, (size_t)stride * 3);
    CHECK(yuv420_to_rgb(&f.view, rgb, stride, YUV_UPSAMPLE_NEAREST) == 0, "转换失败");
    for (int y = 0; y < 3; y++) {
        for (int i = 15; i < stride; i++) {
            CHECK(rgb[y * stride + i] == 0xAB, "第 %d 行填充区被写了", y);
        }
    }
    /* 每行内容应一致（整幅图常数）。 */
    for (int y = 1; y < 3; y++) {
        CHECK(memcmp(rgb, rgb + y * stride, 15) == 0, "行内容不一致");
    }
    free(rgb);
    frame_free(&f);
}

/* 奇数尺寸：宽高非偶数也要能算，色度向上取整。 */
static void test_odd_size(void) {
    CHECK(yuv_chroma_width(5) == 3, "宽 5 的色度宽应为 3，实际 %d", yuv_chroma_width(5));
    CHECK(yuv_chroma_height(3) == 2, "高 3 的色度高应为 2，实际 %d", yuv_chroma_height(3));

    Frame f;
    CHECK(frame_alloc(&f, 5, 3, 0, 0) == 0, "分配失败");
    memset(f.y, 90, (size_t)f.y_stride * 3);
    memset(f.u, 128, (size_t)f.uv_stride * 2);
    memset(f.v, 128, (size_t)f.uv_stride * 2);
    uint8_t rgb[5 * 3 * 3];
    CHECK(yuv420_to_rgb(&f.view, rgb, 5 * 3, YUV_UPSAMPLE_NEAREST) == 0, "奇数尺寸转换失败");
    frame_free(&f);
}

/* 双线性上采样：色度渐变时中间像素应落在两侧之间，且不越界。 */
static void test_bilinear(void) {
    Frame f;
    CHECK(frame_alloc(&f, 8, 4, 0, 0) == 0, "分配失败");
    memset(f.y, 126, (size_t)f.y_stride * 4);
    for (int y = 0; y < 2; y++) {
        for (int x = 0; x < 4; x++) {
            f.u[y * f.uv_stride + x] = (uint8_t)(x == 0 ? 60 : 200);
            f.v[y * f.uv_stride + x] = 128;
        }
    }
    uint8_t near_rgb[8 * 4 * 3];
    uint8_t lin_rgb[8 * 4 * 3];
    CHECK(yuv420_to_rgb(&f.view, near_rgb, 8 * 3, YUV_UPSAMPLE_NEAREST) == 0, "最近邻失败");
    CHECK(yuv420_to_rgb(&f.view, lin_rgb, 8 * 3, YUV_UPSAMPLE_BILINEAR) == 0, "双线性失败");
    int diff = 0;
    for (int i = 0; i < 8 * 4 * 3; i++) {
        if (near_rgb[i] != lin_rgb[i]) diff++;
    }
    CHECK(diff > 0, "双线性与最近邻结果完全一样，插值没生效");
    /* 双线性结果必须落在最近邻结果的最小/最大蓝通道之间（允许 2 的量化误差）。 */
    int near_min = 255;
    int near_max = 0;
    for (int i = 0; i < 8 * 4; i++) {
        int b = near_rgb[i * 3 + 2];
        if (b < near_min) near_min = b;
        if (b > near_max) near_max = b;
    }
    for (int i = 0; i < 8 * 4; i++) {
        int b = lin_rgb[i * 3 + 2];
        CHECK(b >= near_min - 2 && b <= near_max + 2, "双线性蓝通道越界：%d 不在 [%d,%d]", b, near_min, near_max);
    }
    frame_free(&f);
}

/* 参数非法要拒绝。 */
static void test_invalid_args(void) {
    Frame f;
    CHECK(frame_alloc(&f, 4, 4, 0, 0) == 0, "分配失败");
    uint8_t rgb[4 * 4 * 3];
    CHECK(yuv420_to_rgb(&f.view, rgb, 4 * 3 - 1, YUV_UPSAMPLE_NEAREST) == -1, "跨度不足应报错");
    CHECK(yuv420_to_rgb(&f.view, rgb, 4 * 3, 9) == -1, "非法上采样方式应报错");
    frame_free(&f);
}

int main(void) {
    test_flat_neutral();
    test_input_stride();
    test_output_stride();
    test_odd_size();
    test_bilinear();
    test_invalid_args();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    if (g_failures) {
        printf("TESTS FAILED\n");
        return 1;
    }
    printf("ALL TESTS PASSED\n");
    return 0;
}
