/*
 * Copyright(c) 2019 Netflix, Inc.
 * SPDX - License - Identifier: BSD - 2 - Clause - Patent
 */

/******************************************************************************
 * @file SvtAv1E2EFramework.h
 *
 * @brief Defines a test framework for End to End test
 *
 * @author Cidana-Edmond
 *
 ******************************************************************************/

#ifndef _SVT_AV1_E2E_FRAMEWORK_H_
#define _SVT_AV1_E2E_FRAMEWORK_H_

#include "E2eTestVectors.h"
#include "ReconSink.h"
#include "PerformanceCollect.h"
#include "CompareTools.h"

using namespace svt_av1_video_source;
using svt_av1_e2e_tools::PsnrStatistics;
class RefDecoder;
extern RefDecoder *create_reference_decoder();

/** @defgroup svt_av1_e2e_test Test framework for E2E test
 *  Defines the framework body of E2E test for the mainly test progress
 *  @{
 */
namespace svt_av1_e2e_test {

using namespace svt_av1_e2e_test_vector;

/** SvtAv1Context is a set of test contexts in whole test progress */
typedef struct {
    EbComponentType
        *enc_handle; /**< encoder handle, created from encoder library */
    EbSvtAv1EncConfiguration enc_params; /**< encoder parameter set */
    EbBufferHeaderType
        *output_stream_buffer; /**< output buffer of encoder in test */
    EbBufferHeaderType
        *input_picture_buffer; /**< input buffer of encoder in test */
} SvtAv1Context;

/** SvtAv1E2ETestBase is a basic class for only impelmention of setup, teardown,
 * init and close with normal setting */
class SvtAv1E2ETestBase : public ::testing::TestWithParam<TestVideoVector> {
  public:
    SvtAv1E2ETestBase();
    virtual ~SvtAv1E2ETestBase();

  protected:
    void SetUp() override;
    void TearDown() override;
    /** initialization for test */
    virtual void init_test();
    /** close for test */
    virtual void close_test();
    /** test processing body */
    virtual void run_encode_process() = 0;

  protected:
    static VideoSource *prepare_video_src(const TestVideoVector &vector);

  protected:
    VideoSource *video_src_;  /**< video source context */
    SvtAv1Context av1enc_ctx_; /**< AV1 encoder context */
    uint32_t start_pos_;      /**< start position of video frame */
    uint32_t frames_to_test_; /**< frame count for this test */
};

/** SvtAv1E2ETestFramework is a class with impelmention of video source control,
 * encoding progress, decoding progress, data collection and data comparision */
class SvtAv1E2ETestFramework : public SvtAv1E2ETestBase {
  public:
    typedef struct IvfFile {
        FILE *file;
        uint64_t byte_count_since_ivf;
        uint64_t ivf_count;
        IvfFile(std::string path);
        ~IvfFile() {
            if (file) {
                fclose(file);
                file = nullptr;
            }
            byte_count_since_ivf = 0;
            ivf_count = 0;
        }
    } IvfFile;

  protected:
    SvtAv1E2ETestFramework();
    virtual ~SvtAv1E2ETestFramework();

  protected:
    /** initialization for test */
    virtual void init_test() override;
    /** test processing body */
    void run_encode_process() override;

  private:
    /** write ivf header to output file */
    void write_output_header();
    /** write compressed data into file
     * @param output  compressed data from encoder
     */
    void write_compress_data(const EbBufferHeaderType *output);
    /** process compressed data by write to file for send to decoder
     * @param data  compressed data from encoder
     */
    void process_compress_data(const EbBufferHeaderType *data);
    /** send compressed data to decoder
     * @param data  compressed data from encoder, single OBU
     * @param size  size of compressed data
     */
    void decode_compress_data(const uint8_t *data, const uint32_t size);
    /** check video frame psnr with source
     * @param frame  video frame from reference decoder
     */
    void check_psnr(const VideoFrame &frame);

  protected:
    /** get reconstruction frame from encoder, it should call after send data
     * @param is_eos  flag of recon frames is eos
     * into decoder */
    virtual void get_recon_frame(bool &is_eos);

  protected:
    ReconSink *recon_sink_; /**< reconstruction frame collection */
    RefDecoder *refer_dec_; /**< reference decoder context */
    IvfFile *output_file_;  /**< file handle for save encoder output data */
    uint8_t obu_frame_header_size_; /**< size of obu frame header */
    PerformanceCollect *collect_;   /**< performance and time collection*/
    VideoSource *psnr_src_;         /**< video source context for psnr */
    ICompareSink *ref_compare_; /**< sink of reference to compare with recon*/
    svt_av1_e2e_tools::PsnrStatistics
        pnsr_statistics_; /**< psnr statistics recorder.*/
};

}  // namespace svt_av1_e2e_test
/** @} */  // end of svt_av1_e2e_test_vector

#endif  //_SVT_AV1_E2E_FRAMEWORK_H_
