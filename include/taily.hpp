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
#include <functional>
#include <numeric>
#include <vector>

#include <boost/math/distributions/gamma.hpp>

namespace taily {

struct FeatureStatistics {
    static constexpr size_t size = 2 * sizeof(double) + sizeof(int);
    double expected_value;
    double variance;
    int frequency;
    FeatureStatistics operator+(const FeatureStatistics& other) const
    {
        return FeatureStatistics{expected_value + other.expected_value,
                                 variance + other.variance,
                                 frequency + other.frequency};
    }

    std::ostream& to_stream(std::ostream& os) const
    {
        os.write(
            reinterpret_cast<const char*>(&expected_value),
            sizeof(expected_value));
        os.write(reinterpret_cast<const char*>(&variance), sizeof(variance));
        os.write(reinterpret_cast<const char*>(&frequency), sizeof(frequency));
        return os;
    }

    static FeatureStatistics from_stream(std::istream& is)
    {
        FeatureStatistics stats;
        is.read(
            reinterpret_cast<char*>(&stats.expected_value),
            sizeof(stats.expected_value));
        is.read(
            reinterpret_cast<char*>(&stats.variance), sizeof(stats.variance));
        is.read(
            reinterpret_cast<char*>(&stats.frequency), sizeof(stats.frequency));
        return stats;
    }

    template<typename FeatureRange>
    static FeatureStatistics
    from_features(const FeatureRange& features)
    {
        return from_features(std::begin(features), std::end(features));
    }

    template<typename ForwardIterator>
    static FeatureStatistics
    from_features(ForwardIterator first, ForwardIterator last)
    {
        if (first == last) return FeatureStatistics{0, 0, 0};
        int count = 0;
        const double sum = std::accumulate(
            first,
            last,
            double(0.0),
            [&count](const double& acc, const double& feature) {
                count += 1;
                return acc + feature;
            });
        const double expected_value = sum / count;
        const double variance =
            std::accumulate(
                first,
                last,
                double(0.0),
                [&expected_value](const double& acc, const double& feature) {
                    return acc + std::pow(expected_value - feature, 2.0);
                })
            / count;
        return FeatureStatistics{expected_value, variance, count};
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
            return acc * (1.0 - double(stats.frequency) / collection_size);
        });
    return collection_size * (1.0 - any_product);
}

/// Extimates the number of documents containing **all** of the terms
/// represented by `term_stats` in a collection of size `collection_size`.
double all(const CollectionStatistics& stats)
{
    const double any = taily::any(stats);
    if (any == 0.0) {
        return 0.0;
    }
    const double all_product = std::accumulate(
        stats.term_stats.begin(),
        stats.term_stats.end(),
        1.0,
        [any](const auto& acc, const FeatureStatistics& stats) {
            return acc * (stats.frequency / any);
        });
    return any * all_product;
}

/// Returns a gamma distribution fitted to `term_stats`.
auto fit_distribution(const FeatureStatistics& query_term_stats)
{
    const double k = std::pow(query_term_stats.expected_value, 2.0)
        / query_term_stats.variance;
    const double theta =
        query_term_stats.variance / query_term_stats.expected_value;
    return boost::math::gamma_distribution<>(k, theta);
}

/// Returns a gamma distribution fitted to a vector of term stats.
///
/// The term statictics are accumulated before the distribution is fitted.
auto fit_distribution(const std::vector<FeatureStatistics>& term_stats)
{
    FeatureStatistics query_stats = std::accumulate(
        term_stats.begin(), term_stats.end(), FeatureStatistics{0, 0, 0});
    return fit_distribution(query_stats);
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

/// Calculates the probability that a document in a shard given by `stats`
/// has a score higher than `cutoff`.
double calculate_cdf(const double cutoff, const CollectionStatistics& stats)
{
    FeatureStatistics query_stats = std::accumulate(
        stats.term_stats.begin(),
        stats.term_stats.end(),
        FeatureStatistics{0, 0, 0});
    if (query_stats.expected_value == 0 || query_stats.variance == 0) {
        return 0.0;
    }
    auto dist = fit_distribution(query_stats);
    return boost::math::cdf(complement(dist, cutoff));
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

    std::vector<double> shard_coefs(shard_count);
    std::transform(
        std::begin(shard_stats),
        std::end(shard_stats),
        std::begin(shard_coefs),
        std::bind(calculate_cdf, global_cutoff, std::placeholders::_1));

    std::transform(
        std::begin(shard_coefs),
        std::end(shard_coefs),
        std::begin(shard_all),
        std::begin(shard_coefs),
        std::multiplies<double>());

    const double normalization_factor =
        std::accumulate(std::begin(shard_coefs), std::end(shard_coefs), 0.0);

    std::vector<double> estimates(shard_count);
    std::transform(
        std::begin(shard_coefs),
        std::end(shard_coefs),
        std::begin(estimates),
        [ntop, normalization_factor](const auto& element) {
            return normalization_factor > 0
                ? element * ntop / normalization_factor
                : 0.0;
        });
    return estimates;
}

}  // namespace taily
