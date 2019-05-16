/*
 * Copyright(c) 2019 Netflix, Inc.
 * SPDX - License - Identifier: BSD - 2 - Clause - Patent
 */

/******************************************************************************
 * @file PerformanceCollect.cc
 *
 * @brief Impelmentation of performance tool for timing collection
 *
 * @author Cidana-Edmond
 *
 ******************************************************************************/

#include <thread>
#include "PerformanceCollect.h"
#include "gtest/gtest.h"
#include "../util.h"

uint64_t PerformanceCollect::get_time_tick() {
    using namespace std::chrono;
    steady_clock::time_point tp = steady_clock::now();
    steady_clock::duration dtn = tp.time_since_epoch();
    return dtn.count() / 1000000;
}

static void sleep_ms(uint64_t time_ms) {
    std::chrono::milliseconds dura(time_ms);
    std::this_thread::sleep_for(dura);
}

/**
 * @brief Simple tesg of PerformanceCollect class
 *
 * Test strategy: <br>
 * Set three time counting of 500ms: <br>
 * test1 500ms at once <br>
 * test2 250ms in twice <br>
 * test3 100ms in 5 times <br>
 *
 * Expect result:
 * The 3 collectors counts the time compared with 500ms in a very small
 * difference (difference from OS)
 *
 */

TEST(PerformanceCollectTest, run_check) {
    const uint32_t test1_count = 1;
    const uint32_t test2_count = 2;
    const uint32_t test3_count = 5;
    const uint32_t test1_sleep = 500;
    const uint32_t test2_sleep = 250;
    const uint32_t test3_sleep = 100;
    size_t test_max_count = max(test1_count, max(test2_count, test3_count));

    PerformanceCollect *collect = new PerformanceCollect("self_test");
    ASSERT_NE(collect, nullptr);
    for (size_t i = 0; i < test_max_count; i++) {
        if (i < test1_count) {
            TimeAutoCount count("test1", collect);
            sleep_ms(test1_sleep);
        }
        if (i < test2_count) {
            TimeAutoCount count("test2", collect);
            sleep_ms(test2_sleep);
        }
        if (i < test3_count) {
            TimeAutoCount count("test3", collect);
            sleep_ms(test3_sleep);
        }
    }
    EXPECT_LE(abs((long long)collect->read_count("test1") -
                  (test1_count * test1_sleep)),
              10);
    EXPECT_LE(abs((long long)collect->read_count("test2") -
                  (test2_count * test2_sleep)),
              10);
    EXPECT_LE(abs((long long)collect->read_count("test3") -
                  (test3_count * test3_sleep)),
              10);

    delete collect;
}
