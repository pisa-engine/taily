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

#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>

#include <boost/math/distributions/gamma.hpp>

double gamma(double x);
double incomplete_gamma(double x, double y);

namespace taily {

struct FeatureStatistics {
    double expected_value;
    double variance;
    double frequency;
    FeatureStatistics operator+(const FeatureStatistics& other) const
    {
        return FeatureStatistics{expected_value + other.expected_value,
                                 variance + other.variance,
                                 frequency + other.frequency};
    }
};

struct CollectionStatistics {
    std::vector<FeatureStatistics> term_stats;
    int size;
};

/// Extimates the number of documents containing **any** of the terms
/// represented by `term_stats` in a collection of size `collection_size`.
double any(const CollectionStatistics& stats)
{
    const auto collection_size = stats.size;
    const double any_product = std::accumulate(
        stats.term_stats.begin(),
        stats.term_stats.end(),
        1.0,
        [collection_size](const auto& acc, const FeatureStatistics& stats) {
            return 1.0 - stats.frequency / collection_size;
        });
    return collection_size * (1.0 - any_product);
}

/// Extimates the number of documents containing **all** of the terms
/// represented by `term_stats` in a collection of size `collection_size`.
///
/// If `any` is provided, it should be the same value as that calculated
/// by executing `any()`.
/// Otherwise, `any()` will be executed within this function.
double
all(const CollectionStatistics& stats,
    const std::optional<double> any = std::nullopt)
{
    const double any_value = any.value_or(taily::any(stats));
    const double all_product = std::accumulate(
        stats.term_stats.begin(),
        stats.term_stats.end(),
        1.0,
        [any_value](const auto& acc, const FeatureStatistics& stats) {
            return stats.frequency / any_value;
        });
    return any_value * all_product;
}

/// Returns a gamma distribution fitted to `term_stats`.
auto fit_distribution(const std::vector<FeatureStatistics>& term_stats)
{
    FeatureStatistics query_stats = std::accumulate(
        term_stats.begin(), term_stats.end(), FeatureStatistics{0, 0, 0});
    const double k =
        std::pow(query_stats.expected_value, 2.0) / query_stats.variance;
    const double theta = query_stats.variance / query_stats.expected_value;
    return boost::math::gamma_distribution<>(k, theta);
}

/// Estimates the global cutoff score for the entire collection.
double
estimate_cutoff(const CollectionStatistics& stats, int ntop)
{
    auto dist = fit_distribution(stats.term_stats);
    auto all = taily::all(stats);
    const double p_c = std::min(1.0, ntop / all);
    return boost::math::quantile(complement(dist, p_c));
}

double estimate_number_above_cutoff(
    const std::vector<FeatureStatistics>& term_stats,
    const double global_score_cutoff,
    const double all_in_shard,
    const double normalization_factor = 1.0)
{
    auto dist = fit_distribution(term_stats);
    double p = boost::math::cdf(complement(dist, global_score_cutoff));
    return p * all_in_shard * normalization_factor;
}

/// Scores shards given by `shard_stats`.
///
/// \param  global_stats    Term statistics for the entire collection
/// \param  shard_stats     Term statistics for individual shards
/// \param  ntop            The parameter to Taily algorithm saying how many
///                         top results we are shooting for
std::vector<double> score_shards(
    const CollectionStatistics& global_stats,
    const std::vector<CollectionStatistics>& shard_stats,
    const int ntop)
{
    const int shard_count = shard_stats.size();

    std::vector<double> shard_all(shard_count);
    std::transform(
        std::begin(shard_stats),
        std::end(shard_stats),
        std::begin(shard_all),
        [](const auto& shard_stats) { return taily::all(shard_stats); });

    const double global_all = all(global_stats);
    const double global_cutoff = estimate_cutoff(global_stats, ntop);

    std::vector<double> shard_probs(shard_count);
    std::transform(
        std::begin(shard_stats),
        std::end(shard_stats),
        std::begin(shard_probs),
        [global_cutoff](const auto& shard_stats) {
            auto dist = fit_distribution(shard_stats.term_stats);
            return boost::math::cdf(complement(dist, global_cutoff));
        });

    std::transform(
        std::begin(shard_probs),
        std::end(shard_probs),
        std::begin(shard_all),
        std::begin(shard_probs),
        std::multiplies<double>());

    const double normalization_factor =
        std::accumulate(std::begin(shard_probs), std::end(shard_probs), 0.0);

    std::vector<double> estimates(shard_count);
    std::transform(
        std::begin(shard_probs),
        std::end(shard_probs),
        std::begin(estimates),
        [normalization_factor](const auto& element) {
            return element * normalization_factor;
        });
    return estimates;
}

}  // namespace taily
