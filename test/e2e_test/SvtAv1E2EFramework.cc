/*
 * Copyright(c) 2019 Netflix, Inc.
 * SPDX - License - Identifier: BSD - 2 - Clause - Patent
 */

/******************************************************************************
 * @file SvtAv1E2EFramework.cc
 *
 * @brief Impelmentation of End to End test framework
 *
 * @author Cidana-Edmond Cidana-Ryan
 *
 ******************************************************************************/

#include "EbSvtAv1Enc.h"
#include "Y4mVideoSource.h"
#include "YuvVideoSource.h"
#include "gtest/gtest.h"
#include "EbDefinitions.h"
#include "RefDecoder.h"
#include "SvtAv1E2EFramework.h"
#include "CompareTools.h"

#define EB_OUTPUTSTREAMBUFFERSIZE_MACRO(resolution_size) \
    ((resolution_size) < (INPUT_SIZE_1080i_TH)           \
         ? 0x1E8480                                      \
         : (resolution_size) < (INPUT_SIZE_1080p_TH)     \
               ? 0x2DC6C0                                \
               : (resolution_size) < (INPUT_SIZE_4K_TH) ? 0x2DC6C0 : 0x2DC6C0)

#if _WIN32
#define fseeko64 _fseeki64
#define ftello64 _ftelli64
#else
#define fseeko64 fseek
#define ftello64 ftell
#endif

// Copied from EbAppProcessCmd.c
#define LONG_ENCODE_FRAME_ENCODE 4000
#define SPEED_MEASUREMENT_INTERVAL 2000
#define START_STEADY_STATE 1000
#define AV1_FOURCC 0x31305641  // used for ivf header
#define IVF_STREAM_HEADER_SIZE 32
#define IVF_FRAME_HEADER_SIZE 12
#define OBU_FRAME_HEADER_SIZE 3
#define TD_SIZE 2

using namespace svt_av1_e2e_test;
using namespace svt_av1_e2e_tools;

SvtAv1E2ETestBase::SvtAv1E2ETestBase()
    : video_src_(SvtAv1E2ETestBase::prepare_video_src(GetParam())) {
    memset(&av1enc_ctx_, 0, sizeof(av1enc_ctx_));
    start_pos_ = std::get<7>(GetParam());
    frames_to_test_ = std::get<8>(GetParam());
    printf("start: %d, count: %d\n", start_pos_, frames_to_test_);
}

SvtAv1E2ETestBase::~SvtAv1E2ETestBase() {
    if (video_src_) {
        delete video_src_;
        video_src_ = nullptr;
    }
}

void SvtAv1E2ETestBase::SetUp() {
    EbErrorType return_error = EB_ErrorNone;

    // check for video source
    ASSERT_NE(video_src_, nullptr) << "video source create failed!";
    return_error = video_src_->open_source(start_pos_, frames_to_test_);
    ASSERT_EQ(return_error, EB_ErrorNone)
        << "open_source return error:" << return_error;
    // Check input parameters
    uint32_t width = video_src_->get_width_with_padding();
    uint32_t height = video_src_->get_height_with_padding();
    uint32_t bit_depth = video_src_->get_bit_depth();
    ASSERT_GE(width, 0) << "Video vector width error.";
    ASSERT_GE(height, 0) << "Video vector height error.";
    ASSERT_TRUE(bit_depth == 10 || bit_depth == 8)
        << "Video vector bitDepth error.";

    //
    // Init handle
    //
    return_error = eb_init_handle(
        &av1enc_ctx_.enc_handle, &av1enc_ctx_, &av1enc_ctx_.enc_params);
    ASSERT_EQ(return_error, EB_ErrorNone)
        << "eb_init_handle return error:" << return_error;
    ASSERT_NE(av1enc_ctx_.enc_handle, nullptr)
        << "eb_init_handle return null handle.";

    av1enc_ctx_.enc_params.source_width = width;
    av1enc_ctx_.enc_params.source_height = height;
    av1enc_ctx_.enc_params.encoder_bit_depth = bit_depth;
    av1enc_ctx_.enc_params.compressed_ten_bit_format =
        video_src_->get_compressed_10bit_mode();
    av1enc_ctx_.enc_params.recon_enabled = 0;

    //
    // Prepare input and output buffer
    //
    // Input Buffer
    av1enc_ctx_.input_picture_buffer = new EbBufferHeaderType;
    ASSERT_NE(av1enc_ctx_.input_picture_buffer, nullptr)
        << "Malloc memory for inputPictureBuffer failed.";
    av1enc_ctx_.input_picture_buffer->p_buffer = nullptr;
    av1enc_ctx_.input_picture_buffer->size = sizeof(EbBufferHeaderType);
    av1enc_ctx_.input_picture_buffer->p_app_private = nullptr;
    av1enc_ctx_.input_picture_buffer->pic_type = EB_AV1_INVALID_PICTURE;

    // Output buffer
    av1enc_ctx_.output_stream_buffer = new EbBufferHeaderType;
    ASSERT_NE(av1enc_ctx_.output_stream_buffer, nullptr)
        << "Malloc memory for outputStreamBuffer failed.";
    av1enc_ctx_.output_stream_buffer->p_buffer =
        new uint8_t[EB_OUTPUTSTREAMBUFFERSIZE_MACRO(width * height)];
    ASSERT_NE(av1enc_ctx_.output_stream_buffer->p_buffer, nullptr)
        << "Malloc memory for outputStreamBuffer->p_buffer failed.";
    av1enc_ctx_.output_stream_buffer->size = sizeof(EbBufferHeaderType);
    av1enc_ctx_.output_stream_buffer->n_alloc_len =
        EB_OUTPUTSTREAMBUFFERSIZE_MACRO(width * height);
    av1enc_ctx_.output_stream_buffer->p_app_private = nullptr;
    av1enc_ctx_.output_stream_buffer->pic_type = EB_AV1_INVALID_PICTURE;
}

void SvtAv1E2ETestBase::TearDown() {
    EbErrorType return_error = EB_ErrorNone;

    // Destruct the component
    return_error = eb_deinit_handle(av1enc_ctx_.enc_handle);
    ASSERT_EQ(return_error, EB_ErrorNone)
        << "eb_deinit_handle return error:" << return_error;
    av1enc_ctx_.enc_handle = nullptr;

    // Clear
    if (av1enc_ctx_.output_stream_buffer != nullptr) {
        if (av1enc_ctx_.output_stream_buffer->p_buffer != nullptr) {
            delete[] av1enc_ctx_.output_stream_buffer->p_buffer;
        }
        delete[] av1enc_ctx_.output_stream_buffer;
        av1enc_ctx_.output_stream_buffer = nullptr;
    }

    ASSERT_NE(video_src_, nullptr);
    video_src_->close_source();
}

/** initialization for test */
void SvtAv1E2ETestBase::init_test() {
    EbErrorType return_error = EB_ErrorNone;
    /** TODO: encoder_color_format should be set with input source format*/
    av1enc_ctx_.enc_params.encoder_color_format = EB_YUV420;
    return_error = eb_svt_enc_set_parameter(av1enc_ctx_.enc_handle,
                                            &av1enc_ctx_.enc_params);
    ASSERT_EQ(return_error, EB_ErrorNone)
        << "eb_svt_enc_set_parameter return error:" << return_error;

    return_error = eb_init_encoder(av1enc_ctx_.enc_handle);
    ASSERT_EQ(return_error, EB_ErrorNone)
        << "eb_init_encoder return error:" << return_error;

    // Get ivf header
    return_error = eb_svt_enc_stream_header(av1enc_ctx_.enc_handle,
                                            &av1enc_ctx_.output_stream_buffer);
    ASSERT_EQ(return_error, EB_ErrorNone)
        << "eb_svt_enc_stream_header return error:" << return_error;
    ASSERT_NE(av1enc_ctx_.output_stream_buffer, nullptr)
        << "eb_svt_enc_stream_header return null output buffer."
        << return_error;
}

void SvtAv1E2ETestBase::close_test() {
    EbErrorType return_error = EB_ErrorNone;
    // Deinit
    return_error = eb_deinit_encoder(av1enc_ctx_.enc_handle);
    ASSERT_EQ(return_error, EB_ErrorNone)
        << "eb_deinit_encoder return error:" << return_error;
}

VideoSource *SvtAv1E2ETestBase::prepare_video_src(
    const TestVideoVector &vector) {
    VideoSource *video_src = nullptr;
    switch (std::get<1>(vector)) {
    case YUV_VIDEO_FILE:
        video_src = new YuvVideoSource(std::get<0>(vector),
                                       std::get<2>(vector),
                                       std::get<3>(vector),
                                       std::get<4>(vector),
                                       std::get<5>(vector));
        break;
    case Y4M_VIDEO_FILE:
        video_src = new Y4MVideoSource(std::get<0>(vector),
                                       std::get<2>(vector),
                                       std::get<3>(vector),
                                       std::get<4>(vector),
                                       std::get<5>(vector),
                                       std::get<6>(vector));
        break;
    default: assert(0); break;
    }
    return video_src;
}

SvtAv1E2ETestFramework::SvtAv1E2ETestFramework()
    : psnr_src_(SvtAv1E2ETestBase::prepare_video_src(GetParam())) {
    recon_sink_ = nullptr;
    refer_dec_ = nullptr;
    output_file_ = nullptr;
    obu_frame_header_size_ = 0;
    collect_ = nullptr;
    ref_compare_ = nullptr;
}

SvtAv1E2ETestFramework::~SvtAv1E2ETestFramework() {
    if (recon_sink_) {
        delete recon_sink_;
        recon_sink_ = nullptr;
    }
    if (refer_dec_) {
        delete refer_dec_;
        refer_dec_ = nullptr;
    }
    if (output_file_) {
        delete output_file_;
        output_file_ = nullptr;
    }
    if (collect_) {
        delete collect_;
        collect_ = nullptr;
    }
    if (psnr_src_) {
        psnr_src_->close_source();
        delete psnr_src_;
        psnr_src_ = nullptr;
    }
    if (ref_compare_) {
        delete ref_compare_;
        ref_compare_ = nullptr;
    }
}

/** initialization for test */
void SvtAv1E2ETestFramework::init_test() {
    SvtAv1E2ETestBase::init_test();
#if TILES
    EbBool has_tiles = (EbBool)(av1enc_ctx_.enc_params.tile_columns ||
                                av1enc_ctx_.enc_params.tile_rows);
#else
    EbBool has_tiles = (EbBool)EB_FALSE;
#endif
    obu_frame_header_size_ =
        has_tiles ? OBU_FRAME_HEADER_SIZE + 1 : OBU_FRAME_HEADER_SIZE;

    ASSERT_NE(psnr_src_, nullptr) << "PSNR source create failed!";
    EbErrorType err = psnr_src_->open_source(start_pos_, frames_to_test_);
    ASSERT_EQ(err, EB_ErrorNone) << "open_source return error:" << err;
}

void SvtAv1E2ETestFramework::run_encode_process() {
    static const char READ_SRC[] = "read_src";
    static const char ENCODING[] = "encoding";
    static const char RECON[] = "recon";
    static const char CONFORMANCE[] = "conformance";

    EbErrorType return_error = EB_ErrorNone;

    uint32_t frame_count = video_src_->get_frame_count();
    ASSERT_GT(frame_count, 0) << "video srouce file does not contain frame!!";
    if (recon_sink_) {
        recon_sink_->set_frame_count(frame_count);
    }

    if (output_file_) {
        write_output_header();
    }

    uint8_t *frame = nullptr;
    bool src_file_eos = false;
    bool enc_file_eos = false;
    bool rec_file_eos = recon_sink_ ? false : true;
    do {
        if (!src_file_eos) {
            {
                TimeAutoCount counter(READ_SRC, collect_);
                frame = (uint8_t *)video_src_->get_next_frame();
            }
            {
                TimeAutoCount counter(ENCODING, collect_);
                if (frame != nullptr && frame_count) {
                    frame_count--;
                    // Fill in Buffers Header control data
                    av1enc_ctx_.input_picture_buffer->p_buffer = frame;
                    av1enc_ctx_.input_picture_buffer->n_filled_len =
                        video_src_->get_frame_size();
                    av1enc_ctx_.input_picture_buffer->flags = 0;
                    av1enc_ctx_.input_picture_buffer->p_app_private = nullptr;
                    av1enc_ctx_.input_picture_buffer->pts =
                        video_src_->get_frame_index();
                    av1enc_ctx_.input_picture_buffer->pic_type =
                        EB_AV1_INVALID_PICTURE;
                    // Send the picture
                    EXPECT_EQ(EB_ErrorNone,
                              return_error = eb_svt_enc_send_picture(
                                  av1enc_ctx_.enc_handle,
                                  av1enc_ctx_.input_picture_buffer))
                        << "eb_svt_enc_send_picture error at: "
                        << av1enc_ctx_.input_picture_buffer->pts;
                }
                if (frame_count == 0 || frame == nullptr) {
                    src_file_eos = true;
                    EbBufferHeaderType headerPtrLast;
                    headerPtrLast.n_alloc_len = 0;
                    headerPtrLast.n_filled_len = 0;
                    headerPtrLast.n_tick_count = 0;
                    headerPtrLast.p_app_private = nullptr;
                    headerPtrLast.flags = EB_BUFFERFLAG_EOS;
                    headerPtrLast.p_buffer = nullptr;
                    headerPtrLast.pic_type = EB_AV1_INVALID_PICTURE;
                    av1enc_ctx_.input_picture_buffer->flags = EB_BUFFERFLAG_EOS;
                    EXPECT_EQ(EB_ErrorNone,
                              return_error = eb_svt_enc_send_picture(
                                  av1enc_ctx_.enc_handle, &headerPtrLast))
                        << "eb_svt_enc_send_picture EOS error";
                }
            }
        }

        // recon
        if (recon_sink_ && !rec_file_eos) {
            TimeAutoCount counter(RECON, collect_);
            if (!rec_file_eos)
                get_recon_frame(rec_file_eos);
        }

        if (!enc_file_eos) {
            do {
                // non-blocking call
                EbBufferHeaderType *enc_out = nullptr;
                {
                    TimeAutoCount counter(ENCODING, collect_);
                    int pic_send_done = (src_file_eos && rec_file_eos) ? 1 : 0;
                    return_error = eb_svt_get_packet(
                        av1enc_ctx_.enc_handle, &enc_out, pic_send_done);
                    ASSERT_NE(return_error, EB_ErrorMax)
                        << "Error while encoding, code:" << enc_out->flags;
                }

                // process the output buffer
                if (return_error != EB_NoErrorEmptyQueue && enc_out) {
                    TimeAutoCount counter(CONFORMANCE, collect_);
                    process_compress_data(enc_out);
                    if (enc_out->flags & EB_BUFFERFLAG_EOS) {
                        enc_file_eos = true;
                        printf("Encoder EOS\n");
                        break;
                    }
                } else {
                    if (return_error != EB_NoErrorEmptyQueue) {
                        enc_file_eos = true;
                        GTEST_FAIL() << "decoder return: " << return_error;
                    }
                    break;
                }

                // Release the output buffer
                if (enc_out != nullptr) {
                    eb_svt_release_out_buffer(&enc_out);
                    // EXPECT_EQ(enc_out, nullptr)
                    //    << "enc_out buffer is not well released";
                }
            } while (src_file_eos);
        }
    } while (!rec_file_eos || !src_file_eos || !enc_file_eos);

    /** complete the reference buffers in list comparison with recon */
    if (ref_compare_) {
        TimeAutoCount counter(CONFORMANCE, collect_);
        ASSERT_TRUE(ref_compare_->flush_video());
        delete ref_compare_;
        ref_compare_ = nullptr;
    }

    /** PSNR report */
    int count = 0;
    double psnr[4];
    pnsr_statistics_.get_statistics(count, psnr[0], psnr[1], psnr[2], psnr[3]);
    if (count > 0) {
        printf(
            "PSNR: %d frames, total: %0.4f, luma: %0.4f, cb: %0.4f, cr: "
            "%0.4f\r\n",
            count,
            psnr[0],
            psnr[1],
            psnr[2],
            psnr[3]);
    }
    pnsr_statistics_.reset();

    /** performance report */
    if (collect_) {
        frame_count = video_src_->get_frame_count();
        uint64_t total_enc_time = collect_->read_count(ENCODING);
        if (total_enc_time) {
            printf("Enc Performance: %.2fsec/frame (%.4fFPS)\n",
                   (double)total_enc_time / frame_count / 1000,
                   (double)frame_count * 1000 / total_enc_time);
        }
    }
}

void SvtAv1E2ETestFramework::write_output_header() {
    char header[IVF_STREAM_HEADER_SIZE];
    header[0] = 'D';
    header[1] = 'K';
    header[2] = 'I';
    header[3] = 'F';
    mem_put_le16(header + 4, 0);                                // version
    mem_put_le16(header + 6, 32);                               // header size
    mem_put_le32(header + 8, AV1_FOURCC);                       // fourcc
    mem_put_le16(header + 12, av1enc_ctx_.enc_params.source_width);   // width
    mem_put_le16(header + 14, av1enc_ctx_.enc_params.source_height);  // height
    if (av1enc_ctx_.enc_params.frame_rate_denominator != 0 &&
        av1enc_ctx_.enc_params.frame_rate_numerator != 0) {
        mem_put_le32(header + 16,
                     av1enc_ctx_.enc_params.frame_rate_numerator);  // rate
        mem_put_le32(header + 20,
                     av1enc_ctx_.enc_params.frame_rate_denominator);  // scale
    } else {
        mem_put_le32(header + 16,
                     (av1enc_ctx_.enc_params.frame_rate >> 16) * 1000);  // rate
        mem_put_le32(header + 20, 1000);                           // scale
    }
    mem_put_le32(header + 24, 0);  // length
    mem_put_le32(header + 28, 0);  // unused
    if (output_file_ && output_file_->file)
        fwrite(header, 1, IVF_STREAM_HEADER_SIZE, output_file_->file);
}

static void update_prev_ivf_header(
    svt_av1_e2e_test::SvtAv1E2ETestFramework::IvfFile *ivf) {
    char header[4];  // only for the number of bytes
    if (ivf && ivf->file && ivf->byte_count_since_ivf != 0) {
        fseeko64(
            ivf->file,
            (-(int32_t)(ivf->byte_count_since_ivf + IVF_FRAME_HEADER_SIZE)),
            SEEK_CUR);
        mem_put_le32(&header[0], (int32_t)(ivf->byte_count_since_ivf));
        fwrite(header, 1, 4, ivf->file);
        fseeko64(ivf->file,
                 (ivf->byte_count_since_ivf + IVF_FRAME_HEADER_SIZE - 4),
                 SEEK_CUR);
        ivf->byte_count_since_ivf = 0;
    }
}

static void write_ivf_frame_header(
    svt_av1_e2e_test::SvtAv1E2ETestFramework::IvfFile *ivf,
    uint32_t byte_count) {
    char header[IVF_FRAME_HEADER_SIZE];
    int32_t write_location = 0;

    mem_put_le32(&header[write_location], (int32_t)byte_count);
    write_location = write_location + 4;
    mem_put_le32(&header[write_location],
                 (int32_t)((ivf->ivf_count) & 0xFFFFFFFF));
    write_location = write_location + 4;
    mem_put_le32(&header[write_location], (int32_t)((ivf->ivf_count) >> 32));
    write_location = write_location + 4;

    ivf->byte_count_since_ivf = (byte_count);

    ivf->ivf_count++;
    fflush(stdout);

    if (ivf->file)
        fwrite(header, 1, IVF_FRAME_HEADER_SIZE, ivf->file);
}

void SvtAv1E2ETestFramework::write_compress_data(
    const EbBufferHeaderType *output) {
    // Check for the flags EB_BUFFERFLAG_HAS_TD and
    // EB_BUFFERFLAG_SHOW_EXT
    switch (output->flags & 0x00000006) {
    case (EB_BUFFERFLAG_HAS_TD | EB_BUFFERFLAG_SHOW_EXT):
        // terminate previous ivf packet, update the combined size of
        // packets sent
        update_prev_ivf_header(output_file_);

        // Write a new IVF frame header to file as a TD is in the packet
        write_ivf_frame_header(
            output_file_,
            output->n_filled_len - (obu_frame_header_size_ + TD_SIZE));
        fwrite(output->p_buffer,
               1,
               output->n_filled_len - (obu_frame_header_size_ + TD_SIZE),
               output_file_->file);

        // An EB_BUFFERFLAG_SHOW_EXT means that another TD has been added to
        // the packet to show another frame, a new IVF is needed
        write_ivf_frame_header(output_file_,
                               (obu_frame_header_size_ + TD_SIZE));
        fwrite(output->p_buffer + output->n_filled_len -
                   (obu_frame_header_size_ + TD_SIZE),
               1,
               (obu_frame_header_size_ + TD_SIZE),
               output_file_->file);

        break;
    case (EB_BUFFERFLAG_HAS_TD):
        // terminate previous ivf packet, update the combined size of
        // packets sent
        update_prev_ivf_header(output_file_);

        // Write a new IVF frame header to file as a TD is in the packet
        write_ivf_frame_header(output_file_, output->n_filled_len);
        fwrite(output->p_buffer, 1, output->n_filled_len, output_file_->file);
        break;
    case (EB_BUFFERFLAG_SHOW_EXT):
        // this case means that there's only one TD in this packet and is
        // relater
        fwrite(output->p_buffer,
               1,
               output->n_filled_len - (obu_frame_header_size_ + TD_SIZE),
               output_file_->file);
        // this packet will be part of the previous IVF header
        output_file_->byte_count_since_ivf +=
            (output->n_filled_len - (obu_frame_header_size_ + TD_SIZE));

        // terminate previous ivf packet, update the combined size of
        // packets sent
        update_prev_ivf_header(output_file_);

        // An EB_BUFFERFLAG_SHOW_EXT means that another TD has been added to
        // the packet to show another frame, a new IVF is needed
        write_ivf_frame_header(output_file_,
                               (obu_frame_header_size_ + TD_SIZE));
        fwrite(output->p_buffer + output->n_filled_len -
                   (obu_frame_header_size_ + TD_SIZE),
               1,
               (obu_frame_header_size_ + TD_SIZE),
               output_file_->file);

        break;
    default:
        // This is a packet without a TD, write it straight to file
        fwrite(output->p_buffer, 1, output->n_filled_len, output_file_->file);

        // this packet will be part of the previous IVF header
        output_file_->byte_count_since_ivf += (output->n_filled_len);
        break;
    }
}

void SvtAv1E2ETestFramework::process_compress_data(
    const EbBufferHeaderType *data) {
    ASSERT_NE(data, nullptr);
    if (refer_dec_ == nullptr) {
        if (output_file_) {
            write_compress_data(data);
        }
        return;
    }

    if (data->flags & EB_BUFFERFLAG_SHOW_EXT) {
        uint32_t first_part_size =
            data->n_filled_len - obu_frame_header_size_ - TD_SIZE;
        decode_compress_data(data->p_buffer, first_part_size);
        decode_compress_data(data->p_buffer + first_part_size,
                             obu_frame_header_size_ + TD_SIZE);
    } else {
        decode_compress_data(data->p_buffer, data->n_filled_len);
    }
}

void SvtAv1E2ETestFramework::decode_compress_data(const uint8_t *data,
                                                  const uint32_t size) {
    ASSERT_NE(data, nullptr);
    ASSERT_GT(size, 0);

    // input the compressed data into decoder
    ASSERT_EQ(refer_dec_->process_data(data, size), RefDecoder::REF_CODEC_OK);

    VideoFrame ref_frame;
    memset(&ref_frame, 0, sizeof(ref_frame));
    while (refer_dec_->get_frame(ref_frame) == RefDecoder::REF_CODEC_OK) {
        if (recon_sink_) {
            // compare tools
            if (ref_compare_ == nullptr) {
                ref_compare_ = create_ref_compare_sink(ref_frame, recon_sink_);
                ASSERT_NE(ref_compare_, nullptr);
            }
            // Compare ref decode output with recon output.
            ASSERT_TRUE(ref_compare_->compare_video(ref_frame))
                << "image compare failed on " << ref_frame.timestamp;

            // PSNR tool
            check_psnr(ref_frame);
        }
    }
}

void SvtAv1E2ETestFramework::check_psnr(const VideoFrame &frame) {
    // Calculate psnr with input frame and
    EbSvtIOFormat *src_frame =
        psnr_src_->get_frame_by_index((const uint32_t)frame.timestamp);
    if (src_frame) {
        double luma_psnr = 0.0;
        double cb_psnr = 0.0;
        double cr_psnr = 0.0;

        if (video_src_->get_bit_depth() == 8) {
            luma_psnr = psnr_8bit(src_frame->luma,
                                  src_frame->y_stride,
                                  frame.planes[0],
                                  frame.stride[0],
                                  frame.width,
                                  frame.height);
            cb_psnr = psnr_8bit(src_frame->cb,
                                src_frame->cb_stride,
                                frame.planes[1],
                                frame.stride[1],
                                frame.width >> 1,
                                frame.height >> 1);
            cr_psnr = psnr_8bit(src_frame->cr,
                                src_frame->cr_stride,
                                frame.planes[2],
                                frame.stride[2],
                                frame.width >> 1,
                                frame.height >> 1);
        }
        if (video_src_->get_bit_depth() == 10) {
            luma_psnr = psnr_10bit((const uint16_t *)src_frame->luma,
                                   src_frame->y_stride,
                                   (const uint16_t *)frame.planes[0],
                                   frame.stride[0] / 2,
                                   frame.width,
                                   frame.height);
            cb_psnr = psnr_10bit((const uint16_t *)src_frame->cb,
                                 src_frame->cb_stride,
                                 (const uint16_t *)frame.planes[1],
                                 frame.stride[1] / 2,
                                 frame.width >> 1,
                                 frame.height >> 1);
            cr_psnr = psnr_10bit((const uint16_t *)src_frame->cr,
                                 src_frame->cr_stride,
                                 (const uint16_t *)frame.planes[2],
                                 frame.stride[2] / 2,
                                 frame.width >> 1,
                                 frame.height >> 1);
        }
        pnsr_statistics_.add(luma_psnr, cb_psnr, cr_psnr);
        // TODO: Check PSNR value reasonable here?
    }
}

void SvtAv1E2ETestFramework::get_recon_frame(bool &is_eos) {
    do {
        ReconSink::ReconMug *new_mug = recon_sink_->get_empty_mug();
        ASSERT_NE(new_mug, nullptr) << "can not get new mug for recon frame!!";
        ASSERT_NE(new_mug->mug_buf, nullptr)
            << "can not get new mug for recon frame!!";

        EbBufferHeaderType recon_frame = {0};
        recon_frame.size = sizeof(EbBufferHeaderType);
        recon_frame.p_buffer = new_mug->mug_buf;
        recon_frame.n_alloc_len = new_mug->mug_size;
        recon_frame.p_app_private = nullptr;
        // non-blocking call until all input frames are sent
        EbErrorType recon_status =
            eb_svt_get_recon(av1enc_ctx_.enc_handle, &recon_frame);
        ASSERT_NE(recon_status, EB_ErrorMax)
            << "Error while outputing recon, code:" << recon_frame.flags;
        if (recon_status == EB_NoErrorEmptyQueue) {
            recon_sink_->pour_mug(new_mug);
            break;
        } else {
            ASSERT_EQ(recon_frame.n_filled_len, new_mug->mug_size)
                << "recon frame size incorrect@" << recon_frame.pts;
            // mark the recon eos flag
            if (recon_frame.flags & EB_BUFFERFLAG_EOS)
                is_eos = true;
            new_mug->filled_size = recon_frame.n_filled_len;
            new_mug->time_stamp = recon_frame.pts;
            new_mug->tag = recon_frame.flags;
            recon_sink_->fill_mug(new_mug);
        }
    } while (true);
}

SvtAv1E2ETestFramework::IvfFile::IvfFile(std::string path) {
    FOPEN(file, path.c_str(), "wb");
    byte_count_since_ivf = 0;
    ivf_count = 0;
}
