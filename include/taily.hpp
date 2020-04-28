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

#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>
#include <vector>
#include <limits>

#include <boost/math/distributions/gamma.hpp>

namespace taily {

struct Feature_Statistics {
    static constexpr std::size_t struct_size = 2 * sizeof(double) + sizeof(std::int64_t);
    double expected_value;
    double variance;
    std::int64_t frequency;

    [[nodiscard]] constexpr auto
    operator+(Feature_Statistics const& other) const -> Feature_Statistics
    {
        return Feature_Statistics{expected_value + other.expected_value,
                                  variance + other.variance,
                                  frequency + other.frequency};
    }

    auto to_stream(std::ostream& os) const -> std::ostream&
    {
        os.write(reinterpret_cast<const char*>(&expected_value), sizeof(expected_value));
        os.write(reinterpret_cast<const char*>(&variance), sizeof(variance));
        os.write(reinterpret_cast<const char*>(&frequency), sizeof(frequency));
        return os;
    }

    [[nodiscard]] static auto from_stream(std::istream& is) -> Feature_Statistics
    {
        Feature_Statistics stats;
        is.read(reinterpret_cast<char*>(&stats.expected_value), sizeof(stats.expected_value));
        is.read(reinterpret_cast<char*>(&stats.variance), sizeof(stats.variance));
        is.read(reinterpret_cast<char*>(&stats.frequency), sizeof(stats.frequency));
        return stats;
    }

    template<typename Feature_Range>
    [[nodiscard]] static constexpr auto
    from_features(Feature_Range const& features) -> Feature_Statistics
    {
        return from_features(std::begin(features), std::end(features));
    }

    template<typename Forward_Iterator>
    [[nodiscard]] static constexpr auto
    from_features(Forward_Iterator first, Forward_Iterator last) -> Feature_Statistics
    {
        if (first == last) return Feature_Statistics{0, 0, 0};
        std::int64_t count{0};
        auto accumulate_feature = [&count](double const& acc, double const& feature) {
            count += 1;
            return acc + feature;
        };
        double const sum = std::accumulate(first, last, double{0.0}, accumulate_feature);
        double const expected_value = sum / count;
        auto accumulate_squared = [&expected_value](double const& acc, double const& feature) {
            return acc + std::pow(expected_value - feature, 2.0);
        };
        double const variance = std::accumulate(first, last, double{0.0}, accumulate_squared)
            / count;
        return Feature_Statistics{expected_value, variance, count};
    }
};

struct Query_Statistics {
    std::vector<Feature_Statistics> term_stats;
    std::int64_t collection_size;
};

/// Extimates the number of documents containing **any** of the terms
/// represented by `term_stats` in a collection of size `collection_size`.
[[nodiscard]] auto any(Query_Statistics const& stats) -> double
{
    const auto collection_size = stats.collection_size;
    const double any_product = std::accumulate(
        stats.term_stats.begin(),
        stats.term_stats.end(),
        1.0,
        [collection_size](const auto& acc, const Feature_Statistics& stats) {
            return acc * (1.0 - double(stats.frequency) / collection_size);
        });
    return collection_size * (1.0 - any_product);
}

/// Extimates the number of documents containing **all** of the terms
/// represented by `term_stats` in a collection of size `collection_size`.
[[nodiscard]] auto all(const Query_Statistics& stats) -> double
{
    double const any = taily::any(stats);
    if (any == 0.0) {
        return 0.0;
    }
    double const all_product = std::accumulate(
        stats.term_stats.begin(),
        stats.term_stats.end(),
        1.0,
        [any](auto const& acc, Feature_Statistics const& stats) {
            return acc * (stats.frequency / any);
        });
    return any * all_product;
}

/// Returns a gamma distribution fitted to `term_stats`.
[[nodiscard]] auto
fit_distribution(Feature_Statistics const& query_term_stats) -> boost::math::gamma_distribution<>
{
    double epsilon = std::numeric_limits<double>::epsilon();
    double variance = std::max(epsilon, query_term_stats.variance);
    const double k = std::pow(query_term_stats.expected_value, 2.0) / variance;
    const double theta = variance / query_term_stats.expected_value;
    return boost::math::gamma_distribution<>(k, theta);
}

/// Returns a gamma distribution fitted to a vector of term stats.
///
/// The term statictics are accumulated before the distribution is fitted.
[[nodiscard]] auto fit_distribution(std::vector<Feature_Statistics> const& term_stats)
    -> boost::math::gamma_distribution<>
{
    Feature_Statistics query_stats = std::accumulate(
        term_stats.begin(), term_stats.end(), Feature_Statistics{0, 0, 0});
    return fit_distribution(query_stats);
}

/// Estimates the global cutoff score for the entire collection.
[[nodiscard]] auto estimate_cutoff(Query_Statistics const& stats, int ntop) -> double
{
    if(stats.term_stats.size() == 0)
    {
        return 0.0;
    }
    auto const dist = fit_distribution(stats.term_stats);
    double const all = taily::all(stats);
    double const p_c = std::min(1.0, ntop / all);
    return boost::math::quantile(complement(dist, p_c));
}

/// Calculates the probability that a document in a shard given by `stats`
/// has a score higher than `cutoff`.
[[nodiscard]] auto calculate_cdf(double const cutoff, Query_Statistics const& stats) -> double
// [[expects: cutoff >= 0.0]]
{
    if (cutoff <= 0) {
        return 1.0;
    }
    Feature_Statistics query_stats = std::accumulate(
        stats.term_stats.begin(), stats.term_stats.end(), Feature_Statistics{0, 0, 0});
    if (query_stats.expected_value == 0 || query_stats.variance == 0) {
        return 0.0;
    }
    auto dist = fit_distribution(query_stats);
    return boost::math::cdf(complement(dist, cutoff));
}

/// Scores shards given by `shard_stats`.
///
/// \param global_stats Term statistics for the entire collection
/// \param shard_stats Term statistics for individual shards
/// \param ntop The parameter to Taily algorithm saying how many top results we
/// are shooting for
[[nodiscard]] auto score_shards(Query_Statistics const& global_stats,
                                std::vector<Query_Statistics> const& shard_stats,
                                int const ntop) -> std::vector<double>
{
    int const shard_count = shard_stats.size();

    std::vector<double> shard_all(shard_count);
    std::transform(std::begin(shard_stats),
                   std::end(shard_stats),
                   std::begin(shard_all),
                   [](auto const& shard_stats) { return taily::all(shard_stats); });

    double const global_all = all(global_stats);
    double const global_cutoff = estimate_cutoff(global_stats, ntop);

    std::vector<double> shard_coefs(shard_count);
    std::transform(std::begin(shard_stats),
                   std::end(shard_stats),
                   std::begin(shard_coefs),
                   std::bind(calculate_cdf, global_cutoff, std::placeholders::_1));

    std::transform(std::begin(shard_coefs),
                   std::end(shard_coefs),
                   std::begin(shard_all),
                   std::begin(shard_coefs),
                   std::multiplies<double>());

    double const normalization_factor = std::accumulate(
        std::begin(shard_coefs), std::end(shard_coefs), 0.0);

    std::vector<double> estimates(shard_count);
    auto normalize = [ntop, normalization_factor](auto const& element) {
        return normalization_factor > 0 ? element * ntop / normalization_factor : 0.0;
    };
    std::transform(
        std::begin(shard_coefs), std::end(shard_coefs), std::begin(estimates), normalize);
    return estimates;
}

}  // namespace taily
