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

#include <taily.hpp>

#include <fstream>
#include <iostream>
#include <random>
#include <vector>
#include <algorithm>

using namespace taily;

[[nodiscard]] auto read_stats(std::vector<int> const& terms, std::string const& file)
    -> std::vector<Feature_Statistics>
{
    std::ifstream ifs(file);
    std::vector<Feature_Statistics> stats;
    for (int term : terms) {
        ifs.seekg(term * Feature_Statistics::struct_size);
        stats.push_back(Feature_Statistics::from_stream(ifs));
    }
    return stats;
}

int main(int argc, char** argv)
{
    int const term_count = 5;
    int const shard_count = 3;
    /* All shards the same size */
    int const shard_size = 10;
    int const full_size = shard_size * shard_count;
    int const ntop = 50;
    int const query_count = 10;

    std::mt19937 gen(97);
    std::uniform_int_distribution<> query_len_dist(1, 3);
    for (int query = 0; query < query_count; query++) {
        /* Generate query */
        std::vector<int> terms = {0, 1, 2, 3, 4};

        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(terms.begin(), terms.end(), g);
        terms.resize(query_len_dist(gen));
        std::cout << "Query " << query << " with terms:";
        for (int term : terms) {
            std::cout << " " << term;
        }
        std::cout << '\n';

        Query_Statistics full_stats{read_stats(terms, "full_index.stats"), full_size};
        std::vector<Query_Statistics> shard_stats;
        for (int shard = 0; shard < shard_count; shard++) {
            Query_Statistics stats{read_stats(terms, std::to_string(shard) + ".stats"), shard_size};
            shard_stats.push_back(std::move(stats));
        }
        auto scored_shards = score_shards(full_stats, shard_stats, ntop);
        std::cout << "Scores: ";
        for (double score : scored_shards) {
            std::cout << score << " ";
        }
        std::cout << '\n';
    }
}
