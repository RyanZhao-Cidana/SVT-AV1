/*
 * Copyright(c) 2019 Netflix, Inc.
 * SPDX - License - Identifier: BSD - 2 - Clause - Patent
 */

/******************************************************************************
 * @file VideoFrame.h
 *
 * @brief Defines video frame types and structure
 *
 * @author Cidana-Edmond
 *
 ******************************************************************************/
#ifndef _SVT_TEST_VIDEO_FRAME_H_
#define _SVT_TEST_VIDEO_FRAME_H_

#include <memory.h>

/** VideoColorFormat defines the format of YUV video */
typedef enum VideoColorFormat {
    IMG_FMT_YV12,
    IMG_FMT_420 = IMG_FMT_YV12,
    IMG_FMT_422,
    IMG_FMT_444,
    IMG_FMT_420P10_PACKED,
    IMG_FMT_422P10_PACKED,
    IMG_FMT_444P10_PACKED,
    IMG_FMT_NV12,
    IMG_FMT_YV12_CUSTOM_COLOR_SPACE,
    IMG_FMT_NV12_CUSTOM_COLOR_SPACE,
    IMG_FMT_444A,
} VideoColorFormat;

/** VideoFrameParam defines the basic parameters of video frame */
typedef struct VideoFrameParam {
    VideoColorFormat format;
    uint32_t width;
    uint32_t height;
} VideoFrameParam;

/** VideoFrame defines the full parameters of video frame */
typedef struct VideoFrame : public VideoFrameParam {
    uint32_t disp_width;
    uint32_t disp_height;
    uint32_t stride[4];
    uint8_t *planes[4];
    uint32_t bits_per_sample; /** for packed formats */
    void *context;
    uint64_t timestamp;
    bool is_own_buf; /**< flag of own video plane buffers*/
    VideoFrame() {
        /** do nothing */
    }
    VideoFrame(const VideoFrame &origin) {
        *this = origin;
        is_own_buf = true;
        const uint32_t luma_len =
            stride[0] * height * (bits_per_sample > 8 ? 2 : 1);
        for (int i = 0; i < 4; ++i) {
            if (i != 3 || origin.planes[i]) {
                const int buffer_len =
                    (i == 1 || i == 2) ? luma_len >> 2 : luma_len;
                planes[i] = new uint8_t[buffer_len];
                if (planes[i]) {
                    memcpy(planes[i], origin.planes[i], buffer_len);
                }
            }
        }
    }
    ~VideoFrame() {
        if (is_own_buf) {
            for (int i = 0; i < 4; ++i) {
                if (planes[i]) {
                    delete[] planes[i];
                    planes[i] = nullptr;
                }
            }
        }
    }
} VideoFrame;

#endif  //_SVT_TEST_VIDEO_FRAME_H_
