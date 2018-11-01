[![Build Status](https://travis-ci.org/elshize/taily.svg?branch=master)](https://travis-ci.org/elshize/taily)

This library implements Taily algorithm as described by Aly et al.
in the 2013 paper
[Taily: shard selection using the tail of score distributions](https://dl.acm.org/citation.cfm?id=2484033).

### Disclaimer

At this early stage of development, the library interface is subject to changes. If you rely on it now, I advise to use a specific git tag.

# Installation

`taily` is a header-only library. For now, copy and include `include/taily.hpp` file.

`cmake` and `conan` to come...

# Dependencies

Library compiles with GCC >= 4.9 and Clang >= 4, and it requires C++14.
The only other dependency is [Boost.Math](https://www.boost.org/doc/libs/1_68_0/libs/math/doc/html/index.html)
library used for Gamma distribution.

# Usage

Chances are you will only need to call one function that scores all
shards with respect to one query:

```c++
std::vector<double> score_shards(
    const CollectionStatistics& global_stats,
    const std::vector<CollectionStatistics>& shard_stats,
    const int ntop)
```

`global_stats` contains statistics for the entire index, while `shard_stats`
vector represents the shards, and `ntop` is the parameter of Taily---the
number top results for which a score threshold will be estimated.

`CollectionStatistics` is a simple structure that contains the collection size
and a vector of of length equal to the number of query terms.

```c++
struct CollectionStatistics {
    std::vector<FeatureStatistics> term_stats;
    int size;
};
```

Each element of `term_stats` contains the values needed for computations:

```c++
struct FeatureStatistics {
    double expected_value;
    double variance;
    int frequency;

    template<typename FeatureRange>
    static FeatureStatistics from_features(const FeatureRange& features);

    template<typename ForwardIterator>
    static FeatureStatistics from_features(ForwardIterator first, ForwardIterator last);
};
```

## Generating and Writing Features

In case you want to use this library for storing features as well,
you can use the helper functions `from_features()` to computes statistics:

```c++
const std::vector<double>& features = fetch_or_generate_features(term);
auto stats = FeatureStatistics::from_features(features);
```

or

```c++
double* features = fetch_or_generate_features(term);
auto stats = FeatureStatistics::from_features(features, features + len);
```

The first one takes any forward range, such as `std::vector`, `std::array`,
that overload `std::begin()` and `std::end()` that return a forward iterator
of `double`s. The latter takes two of such iterators.
