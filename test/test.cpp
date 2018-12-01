// MIT License
//
// Copyright (c) 2018 Michal Siedlaczek
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/// \file
/// \author Michal Siedlaczek
/// \copyright MIT License

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <taily.hpp>

namespace {

using namespace taily;

class Taily : public ::testing::Test {
protected:
    // obama family tree
    Query_Statistics global_stats = {
        {{30.57, 102.64, 732'226}, {12.64, 16.02, 6'172'261}, {21.84, 66.17, 1'597'720}},
        37'512'555};
    Query_Statistics shard1_stats = {
        {{30.57, 102.64, 732'226}, {14.0, 10.0, 4'172'261}, {15.0, 70.0, 597'720}}, 12'504'185};
    Query_Statistics shard2_stats = {
        {{0.0, 0.0, 0}, {11.00, 20.0, 2'000'000}, {25.0, 50.0, 1'000'000}}, 12'504'185};
    Query_Statistics shard3_stats = {{{0.0, 0.0, 0}, {0.0, 0.0, 0}, {0.0, 0.0, 0}}, 12'504'185};
};

TEST(Feature_Statistics, add)
{
    // given
    Feature_Statistics lhs{1.0, 2.0, 3};
    Feature_Statistics rhs{4.0, 5.0, 6};

    // when
    Feature_Statistics sum = lhs + rhs;

    ASSERT_EQ(sum.expected_value, 5.0);
    ASSERT_EQ(sum.variance, 7.0);
    ASSERT_EQ(sum.frequency, 9);
}

TEST(Feature_Statistics, from_vector)
{
    std::vector<double> features = {2, 3, 1, 4, 5, 3};
    auto stats = Feature_Statistics::from_features(features);
    ASSERT_THAT(stats.expected_value, ::testing::DoubleEq(3));
    ASSERT_THAT(stats.variance, ::testing::DoubleEq(1.6666666666666667));
    ASSERT_EQ(stats.frequency, 6);
}

TEST(Feature_Statistics, from_pointers)
{
    double features[6] = {2, 3, 1, 4, 5, 3};
    auto stats = Feature_Statistics::from_features(features, features + 6);
    ASSERT_THAT(stats.expected_value, ::testing::DoubleEq(3));
    ASSERT_THAT(stats.variance, ::testing::DoubleEq(1.6666666666666667));
    ASSERT_EQ(stats.frequency, 6);
}

TEST_F(Taily, any)
{
    ASSERT_THAT(any(global_stats), ::testing::DoubleEq(8092785.817906557));
    ASSERT_THAT(any(shard1_stats), ::testing::DoubleEq(5035122.3990347795));
    ASSERT_THAT(any(shard2_stats), ::testing::DoubleEq(2840053.550071435));
    ASSERT_THAT(any(shard3_stats), ::testing::DoubleEq(0.0));
}

TEST_F(Taily, all)
{
    ASSERT_THAT(all(global_stats), ::testing::DoubleEq(110253.9116689363));
    ASSERT_THAT(all(shard1_stats), ::testing::DoubleEq(72026.835974918));
    ASSERT_THAT(all(shard2_stats), ::testing::DoubleEq(0.0));
    ASSERT_THAT(all(shard3_stats), ::testing::DoubleEq(0.0));
}

TEST_F(Taily, fit_distribution)
{
    auto glob_dist = fit_distribution(global_stats.term_stats);
    ASSERT_THAT(glob_dist.shape(), ::testing::DoubleEq(22.894024238489422));
    ASSERT_THAT(glob_dist.scale(), ::testing::DoubleEq(2.8413528055342043));
    auto shard1_dist = fit_distribution(shard1_stats.term_stats);
    ASSERT_THAT(shard1_dist.shape(), ::testing::DoubleEq(19.429396079719666));
    ASSERT_THAT(shard1_dist.scale(), ::testing::DoubleEq(3.0659728051032396));
    auto shard2_dist = fit_distribution(shard2_stats.term_stats);
    ASSERT_THAT(shard2_dist.shape(), ::testing::DoubleEq(18.514285714285716));
    ASSERT_THAT(shard2_dist.scale(), ::testing::DoubleEq(1.9444444444444444));
    ASSERT_ANY_THROW(void(fit_distribution(shard3_stats.term_stats)));
}

TEST_F(Taily, estimate_cutoff)
{
    ASSERT_THAT(
        estimate_cutoff(global_stats, 50),
        ::testing::DoubleNear(119.7979980410835, 0.001));
    ASSERT_THAT(
        estimate_cutoff(global_stats, 10000),
        ::testing::DoubleNear(83.84815493221593, 0.001));
}

TEST_F(Taily, calculate_cdf)
{
    ASSERT_THAT(calculate_cdf(50, shard1_stats), ::testing::DoubleNear(0.749616934825099, 0.0001));
    ASSERT_THAT(calculate_cdf(80, shard1_stats), ::testing::DoubleNear(0.07483776061459, 0.0001));
    ASSERT_THAT(calculate_cdf(119.7979980410835, shard1_stats),
                ::testing::DoubleNear(0.000189069131111, 0.000001));
    ASSERT_THAT(calculate_cdf(50, shard2_stats), ::testing::DoubleNear(0.0581330331658248, 0.0001));
    ASSERT_THAT(calculate_cdf(80, shard2_stats),
                ::testing::DoubleNear(0.00002757183562934, 0.0001));
}

TEST_F(Taily, calculate_cdf_cutoff_0)  // Issue #1
{
    ASSERT_EQ(calculate_cdf(0, shard1_stats), 1.0);
    ASSERT_EQ(calculate_cdf(0, shard2_stats), 1.0);
    ASSERT_EQ(calculate_cdf(0, shard3_stats), 1.0);
}

TEST_F(Taily, score_shards)
{
    auto scores = score_shards(
        global_stats, std::vector<Query_Statistics>{shard1_stats, shard2_stats, shard3_stats}, 50);
    ASSERT_THAT(scores, ::testing::ElementsAre(50, 0, 0));
    scores = score_shards(
        global_stats, std::vector<Query_Statistics>{shard1_stats, shard1_stats, shard1_stats}, 50);
    ASSERT_THAT(scores[0], ::testing::DoubleNear(16.666666666666664, 0.00001));
    ASSERT_THAT(scores[1], ::testing::DoubleNear(16.666666666666664, 0.00001));
    ASSERT_THAT(scores[2], ::testing::DoubleNear(16.666666666666664, 0.00001));
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
