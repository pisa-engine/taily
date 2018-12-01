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
#include <random>
#include <vector>

std::vector<std::vector<double>> full_index = {{7, 2, 6, 11, 1, 1, 1, 3, 8, 15},
                                               {9, 2},
                                               {11, 7, 14, 15, 12, 2, 11, 5, 5, 15, 4, 10, 4, 10},
                                               {6, 8, 1, 4, 6},
                                               {1, 12, 15, 9, 8, 8, 2}};

std::vector<std::vector<std::vector<double>>> shards = {
    {{7, 2, 6}, {9}, {11, 7, 14, 15}, {6}, {}},
    {{11, 1, 1, 1}, {2}, {12, 2, 11, 5, 5, 15, 4, 10}, {8, 1, 4}, {}},
    {{3, 8, 15}, {}, {4, 10}, {6}, {1, 12, 15, 9, 8, 8, 2}}};

void write_stats_for_index(std::vector<std::vector<double>> const& scores,
                           std::string const& filename)
{
    std::ofstream ofs(filename);
    for (const std::vector<double>& term_scores : scores) {
        auto term_stats = taily::Feature_Statistics::from_features(term_scores);
        term_stats.to_stream(ofs);
    }
}

int main(int argc, char** argv)
{
    int term_count = full_index.size();
    int shard_count = shards.size();
    write_stats_for_index(full_index, "full_index.stats");
    for (int shard = 0; shard < shard_count; shard++) {
        write_stats_for_index(shards[shard], std::to_string(shard) + ".stats");
    }
}
