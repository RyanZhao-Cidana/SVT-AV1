/*
 * Copyright(c) 2019 Netflix, Inc.
 * SPDX - License - Identifier: BSD - 2 - Clause - Patent
 */

/******************************************************************************
 * @file ReconSink.h
 *
 * @brief Defines a sink to collect reconstruction frames
 *
 * @author Cidana-Edmond
 *
 ******************************************************************************/

#ifndef _RECON_SINK_H_
#define _RECON_SINK_H_

#include <stdint.h>
#include <memory.h>
#include "VideoFrame.h"

/** ReconSink is a class designed to collect YUV video frames. It provides
 * interfaces for generating, store and destory frame containers. It can be
 * implemented with file-mode or buffer-mode to store the video frames, and it
 * also provides inside sorting by timestamp.
 */
class ReconSink {
  public:
    /** ReconSinkType is enumerate type of sink type, file or buffer mode */
    typedef enum ReconSinkType {
        RECON_SINK_BUFFER,
        RECON_SINK_FILE,
    } ReconSinkType;

    typedef struct ReconMug {
        uint32_t tag;         /**< tag of the frame */
        uint64_t time_stamp;  /**< timestamp of the frame, current should be the
                                 index of display order*/
        uint32_t mug_size;    /**< size of the frame container buffer*/
        uint32_t filled_size; /**< size of the actual data in buffer */
        uint8_t* mug_buf;     /**< memory buffer of the frame */
    } ReconMug;

  public:
    /** Constructor of ReconSink
     * @param param the parameters of the video frame
     */
    ReconSink(const VideoFrameParam& param) {
        sink_type_ = RECON_SINK_BUFFER;
        video_param_ = param;
        frame_size_ = calculate_frame_size(video_param_);
        frame_count_ = 0;
    }
    /** Destructor of ReconSink	  */
    virtual ~ReconSink() {
    }
    /** Get sink type
     * @return
     * ReconSinkType -- the type of sink
     */
    ReconSinkType get_type() {
        return sink_type_;
    }
    /** Get video parameter
     * @return
     * VideoFrameParam -- the parameter of video frame
     */
    VideoFrameParam get_video_param() {
        return video_param_;
    }
    /** Get total frame count in sink
     * @return
     * uint32_t -- the count of frame in sink
     */
    uint32_t get_frame_count() {
        return frame_count_;
    }
    /** Get maximum video frame number in sink
     * @param count  the maximum video frame number
     */
    void set_frame_count(const uint32_t count) {
        frame_count_ = count;
    }
    /** Get an empty video frame container from sink
     * @return
     * ReconMug -- a container of video frame
     * nullptr -- no available container
     */
    ReconMug* get_empty_mug() {
        ReconMug* new_mug = new ReconMug;
        if (new_mug) {
            memset(new_mug, 0, sizeof(ReconMug));
            new_mug->mug_size = frame_size_;
            new_mug->mug_buf = new uint8_t[frame_size_];
            if (new_mug->mug_buf == nullptr) {
                delete new_mug;
                new_mug = nullptr;
            }
        }
        return new_mug;
    }
    /** Interface of insert a container into sink
     * @param mug  the container to insert into sink
     */
    virtual void fill_mug(ReconMug* mug) = 0;
    /** Interface of get a container with video frame by the same timestamp
     * @param time_stamp  the timestamp of video frame to retreive
     * @return
     * ReconMug -- a container of video frame
     * nullptr -- no available container by this timestamp
     */
    virtual const ReconMug* take_mug(const uint64_t time_stamp) = 0;
    /** Interface of get a container with video frame by index
     * @param index  the index of container to retreive
     * @return
     * ReconMug -- a container of video frame
     * nullptr -- no available container by index
     */
    virtual const ReconMug* take_mug_inorder(const uint32_t index) = 0;
    /** Interface of destroy a container and remove from sink
     * @param mug  the container to distroy
     */
    virtual void pour_mug(ReconMug* mug) = 0;
    /** Interface of get whether the sink is compeletely filled
     * @return
     * true -- the sink is filled
     * false -- the sink is still available
     */
    virtual bool is_compelete() = 0;

  protected:
    /** Tool of video frame size caculation, with width, height and bit-depth
     * @param param  parameter of video frame
     * @return
     * the size in byte of the video frame
     */
    static uint32_t calculate_frame_size(const VideoFrameParam& param) {
        uint32_t lumaSize = param.width * param.height;
        uint32_t chromaSize = 0;
        switch (param.format) {
        case IMG_FMT_420: chromaSize = lumaSize >> 2; break;
        case IMG_FMT_422: chromaSize = lumaSize >> 1; break;
        case IMG_FMT_444: chromaSize = lumaSize; break;
        case IMG_FMT_420P10_PACKED:
            lumaSize = lumaSize << 1;
            chromaSize = lumaSize >> 2;
            break;
        case IMG_FMT_422P10_PACKED:
            lumaSize = lumaSize << 1;
            chromaSize = lumaSize >> 1;
            break;
        case IMG_FMT_444P10_PACKED:
            lumaSize = lumaSize << 1;
            chromaSize = lumaSize;
            break;
        default: break;
        }
        return lumaSize + (2 * chromaSize);
    }

  protected:
    ReconSinkType sink_type_;     /**< type of sink*/
    VideoFrameParam video_param_; /**< video frame parameters*/
    uint32_t frame_size_;         /**< size of video frame*/
    uint32_t frame_count_;        /**< maximun number of video frames*/
};

class ICompareSink {
  public:
    virtual ~ICompareSink(){};
    virtual bool compare_video(const VideoFrame& frame) = 0;
    virtual bool flush_video() = 0;
};

/** Interface of create a sink of reconstruction video frame with video
 * parameters and the file path to store
 * @param param  the parameter of video frame
 * @param file_path  the file path to store the containers
 * @return
 * ReconSink -- the sink created
 * nullptr -- creation failed
 */
ReconSink* create_recon_sink(const VideoFrameParam& param,
                             const char* file_path);

/** Interface of create a sink of reconstruction video frame with video
 * parameters
 * @param param  the parameter of video frame
 * @return
 * ReconSink -- the sink created
 * nullptr -- creation failed
 */
ReconSink* create_recon_sink(const VideoFrameParam& param);

/** Interface of create a sink of reference frames to compare with recon
 * parameters
 * @param param  the parameter of video frame
 * @param recon  the sink of recon video frame
 * @return
 * ReconSink -- the sink created
 * nullptr -- creation failed
 */
ICompareSink* create_ref_compare_sink(const VideoFrameParam& param,
                                      ReconSink* recon);

#endif  // !_RECON_SINK_H_
