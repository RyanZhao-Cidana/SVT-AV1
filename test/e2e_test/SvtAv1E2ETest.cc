/*
 * Copyright(c) 2019 Netflix, Inc.
 * SPDX - License - Identifier: BSD - 2 - Clause - Patent
 */

/******************************************************************************
 * @file SvtAv1E2ETest.cc
 *
 * @brief Impelmentation of SVT-AV1 encoder E2E test
 *
 * @author Cidana-Edmond
 *
 ******************************************************************************/

#include "EbSvtAv1Enc.h"
#include "gtest/gtest.h"
#include "SvtAv1E2EFramework.h"

using namespace svt_av1_e2e_test;
using namespace svt_av1_e2e_test_vector;

/**
 * @brief SVT-AV1 encoder simple E2E test
 *
 * Test strategy:
 * Setup SVT-AV1 encoder with default parameter, and encode the input YUV data
 * frames
 *
 * Expect result:
 * No error from encoding progress and output compressed data exist
 *
 * Test coverage:
 * All test vectors
 */
class SvtAv1E2ESimpleTest : public SvtAv1E2ETestFramework {};

TEST_P(SvtAv1E2ESimpleTest, run_smoking_test) {
    init_test();
    run_encode_process();
    close_test();
}

INSTANTIATE_TEST_CASE_P(
    SVT_AV1, SvtAv1E2ESimpleTest,
    ::testing::ValuesIn(generate_vector_from_config("video_src.cfg")));

/**
 * @brief SVT-AV1 encoder simple E2E test with save compressed data in file
 *
 * Test strategy:
 * Setup SVT-AV1 encoder with default parameter, and encode the input YUV data
 * frames. Save the compressed data into IVF file
 *
 * Expect result:
 * No error from encoding progress and output compressed data is saved into IVF
 * file
 *
 * Test coverage:
 * Smoking test vectors
 */
class SvtAv1E2ESimpleFileTest : public SvtAv1E2ETestFramework {
  protected:
    /** initialization for test */
    void init_test() override {
        output_file_ = new IvfFile("output.av1");
        SvtAv1E2ETestFramework::init_test();
    }
};

TEST_P(SvtAv1E2ESimpleFileTest, run_smoking_with_output_test) {
    init_test();
    run_encode_process();
    close_test();
}

INSTANTIATE_TEST_CASE_P(
    SVT_AV1, SvtAv1E2ESimpleFileTest,
    ::testing::ValuesIn(generate_vector_from_config("smoking_test.cfg")));

/**
 * @brief SVT-AV1 encoder E2E test with save the reconstruction frames in
 * file
 *
 * Test strategy:
 * Setup SVT-AV1 encoder with default parameter, and encode the input YUV data
 * frames. Save the reconstruction frame data in file
 *
 * Expect result:
 * No error from encoding progress and reconstruction frame data saved in file.
 *
 * Test coverage:
 * Smoking test vectors
 */
class SvtAv1E2EReconFileTest : public SvtAv1E2ETestFramework {
  protected:
    /** initialization for test */
    void init_test() override {
        // create recon sink before setup parameter of encoder
        VideoFrameParam param;
        memset(&param, 0, sizeof(param));
        param.format = video_src_->get_image_format();
        param.width = video_src_->get_width_with_padding();
        param.height = video_src_->get_height_with_padding();
        recon_sink_ = create_recon_sink(param, "enc_recon.rcs");
        ASSERT_NE(recon_sink_, nullptr) << "can not create recon sink!!";
        if (recon_sink_)
            av1enc_ctx_.enc_params.recon_enabled = 1;
        SvtAv1E2ETestFramework::init_test();
    }
};

TEST_P(SvtAv1E2EReconFileTest, run_recon_collect_test) {
    init_test();
    run_encode_process();
    close_test();
}

INSTANTIATE_TEST_CASE_P(
    SVT_AV1, SvtAv1E2EReconFileTest,
    ::testing::ValuesIn(generate_vector_from_config("smoking_test.cfg")));

/**
 * @brief SVT-AV1 encoder E2E test with save the reconstruction frames in
 * buffer list
 *
 * Test strategy:
 * Setup SVT-AV1 encoder with default parameter, and encode the input YUV data
 * frames. Save the reconstruction frame data in buffer list
 *
 * Expect result:
 * No error from encoding progress and reconstruction frame data saved in
 * buffer.
 *
 * Test coverage:
 * Smoking test vectors
 */
class SvtAv1E2EReconBufferTest : public SvtAv1E2ETestFramework {
  protected:
    /** initialization for test */
    void init_test() override {
        // create recon sink before setup parameter of encoder
        VideoFrameParam param;
        memset(&param, 0, sizeof(param));
        param.format = video_src_->get_image_format();
        param.width = video_src_->get_width_with_padding();
        param.height = video_src_->get_height_with_padding();
        recon_sink_ = create_recon_sink(param);
        ASSERT_NE(recon_sink_, nullptr) << "can not create recon sink!!";
        if (recon_sink_)
            av1enc_ctx_.enc_params.recon_enabled = 1;
        SvtAv1E2ETestFramework::init_test();
    }
};

TEST_P(SvtAv1E2EReconBufferTest, run_recon_collect_test) {
    init_test();
    run_encode_process();
    close_test();
}

INSTANTIATE_TEST_CASE_P(
    SVT_AV1, SvtAv1E2EReconBufferTest,
    ::testing::ValuesIn(generate_vector_from_config("smoking_test.cfg")));

/**
 * @brief SVT-AV1 encoder E2E test with comparing the reconstruction frame with
 * output frame from decoder buffer list
 *
 * Test strategy:
 * Setup SVT-AV1 encoder with default parameter, and encode the input YUV data
 * frames. Collect the reconstruction frames and compared with reference decoder
 * output
 *
 * Expect result:
 * No error from encoding progress and reconstruction frame data is same as the
 * output frame from reference decoder
 *
 * Test coverage:
 * All test vectors
 */
class SvtAv1E2EConformanceTest : public SvtAv1E2ETestFramework {
  protected:
    /** initialization for test */
    void init_test() override {
        // create recon sink before setup parameter of encoder
        VideoFrameParam param;
        memset(&param, 0, sizeof(param));
        param.format = video_src_->get_image_format();
        param.width = video_src_->get_width_with_padding();
        param.height = video_src_->get_height_with_padding();
        recon_sink_ = create_recon_sink(param);
        ASSERT_NE(recon_sink_, nullptr) << "can not create recon sink!!";
        if (recon_sink_)
            av1enc_ctx_.enc_params.recon_enabled = 1;

        // create reference decoder
        refer_dec_ = create_reference_decoder();
        ASSERT_NE(refer_dec_, nullptr) << "can not create reference decoder!!";

        collect_ = new PerformanceCollect(typeid(this).name());

        SvtAv1E2ETestFramework::init_test();
    }
};

TEST_P(SvtAv1E2EConformanceTest, run_conformance_test) {
    init_test();
    run_encode_process();
    close_test();
}

INSTANTIATE_TEST_CASE_P(
    SVT_AV1, SvtAv1E2EConformanceTest,
    ::testing::ValuesIn(generate_vector_from_config("comformance_test.cfg")));
