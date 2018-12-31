//Copyright (c) 2018 Hitachi Vantara Corporation
//All Rights Reserved.
//
//   Licensed under the Apache License, Version 2.0 (the "License"); you may
//   not use this file except in compliance with the License. You may obtain
//   a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
//   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
//   License for the specific language governing permissions and limitations
//   under the License.
//
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>, Kumaran Subramaniam <kumaran.subramaniam@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.

#include <assert.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <functional>
#include "DedupePatternRegulator.h"
#include "ivytime.h"
#include "ivyhelpers.h"

/*
 * For a given target deduplication ratio T, create a dataset with number of copies
 * C = {unique(1), target(T), target+spread(T+S)} at a probability distribution N = {N1, N2, N3}
 *
 * Constraints:
 * 	N1 + N2 + N3 = 1
 * 	T = N1 + N2*T + N3*(T + S)
 *
 * Given:
 *     N2 = 0.0 or 0.5 (probability assignment at the center with copies = target)
 *         -- Limiting case N2=1.0 in IVY 2.03
 *
 * Derived:
 *     N1 = (1 - N2) / (1 + (T-1)/S)
 *     N3 = N1 * (T-1)/S
 *
 * Tunables:
 *     Spread (S)
 *     N2
 */

DedupePatternRegulator:: DedupePatternRegulator(ivy_float dedupe, uint64_t seed) : target_dedupe(dedupe), starting_seed(seed)
{
    state = 0;
    pos = 0;

    target = ceil(dedupe);
    spread = 8; // default


    reuse_probability = 1.0 - (1.0/dedupe);
    target_probability = 0.0;

    // derived probabilities
    unique_probability = (1.0 - target_probability)/(1.0 + (dedupe - 1.0)/spread);
    high_probability = unique_probability * ((dedupe -1.0)/spread);

    // derived percentages
    unique_percentage = (uint32_t) round(100 * unique_probability);
    target_percentage = (uint32_t) round(100 * target_probability);
    high_percentage = (uint32_t) round(100 * high_probability);
    event_span = unique_percentage + (target + spread) * high_percentage;

    max_seeds_in_pod = 32;

    // dedupe dependent threshold to keep pattern numbers in the range [0..pattern_number_reuse_threshold]
    if (target_dedupe < 2.0)
        pattern_number_reuse_threshold = (1.0 - reuse_probability) * 100000;
    else if (target_dedupe > 10.0)
        pattern_number_reuse_threshold = (1.0 - reuse_probability) * 65000;
    else
        pattern_number_reuse_threshold = (1.0 - reuse_probability) * 50000;

    std::hash<std::string> string_hash;

    ivytime now;
    now.setToNow();
    defrangen.seed(string_hash(std::string("DedupePatternRegulator") + now.format_as_datetime_with_ns()));

    p_seed_locator_distribution = new std::uniform_int_distribution<uint64_t> (0, max_seeds_in_pod);
    p_reuse_distribution = new std::uniform_int_distribution<uint64_t> (0,100);

    generate_seeds();
}

DedupePatternRegulator::~DedupePatternRegulator()
{
    seed_pod.clear();
    seed_pod_map.clear();
    seed_pod_initialized = false;
}

std::string
DedupePatternRegulator::generate_seeds()
{
    uint64_t pattern_seed = starting_seed;
    uint64_t pattern_number = 0;
    o << "generate: starting_seed " << pattern_seed << " pattern number: " << pattern_number << std::endl;

    seed_pod.push_back(std::make_pair(pattern_seed, pattern_number));
    seed_pod_map[pattern_seed] = true;

    if (!seed_pod_initialized)
    {
        for (uint32_t i = 0, pos = 0; i < 100*max_seeds_in_pod; i++, pos = (pos + 1) % 100)
        {
            int state;
            if (pos < high_percentage)
                state = target + spread;
            else if (pos < (high_percentage + unique_percentage))
                state = 1;
            else
                state = target;

            pattern_number += state;
            while (state--) xorshift64star(pattern_seed);

            if (pos == 0)
            {
                o << "generate: pattern_seed " << pattern_seed << " pattern number: " << pattern_number << std::endl;
                seed_pod.push_back(std::make_pair(pattern_seed, pattern_number));
                seed_pod_map[pattern_seed] = true;
            }
        }
        seed_pod_initialized = true;
    }
    return o.str();
}

void DedupePatternRegulator::record_seed(uint64_t seed, uint64_t pattern_number)
{
    // do not record if already in map
    auto loc = seed_pod_map.find(seed);

    if (loc != seed_pod_map.end())
        return;
    else if (seed_pod.size() < max_seeds_in_pod)
    {
        seed_pod.push_back(std::make_pair(seed, pattern_number));
        seed_pod_map[seed] = true;
    }
}

bool DedupePatternRegulator::decide_reuse()
{
    int toss = 0;
    bool reuse {false};

    // decide based on reuse probability
    toss = (*p_reuse_distribution)(defrangen);

    if (toss >= 0 && toss <= (int)100*reuse_probability)
        reuse = true;

    return reuse;
}

uint64_t DedupePatternRegulator::random_seed()
{
    ivytime t;
    t.setToNow();

    uint64_t seed = (uint64_t) std::hash<std::string>{}(std::string("DedupePattern")) ^ t.getAsNanoseconds();
    record_seed(seed, 0);

    return seed;
}

std::pair<uint64_t, uint64_t> DedupePatternRegulator::reuse_seed()
{
    std::pair<uint64_t, uint64_t> seed;
    uint64_t index = 0;

    // pick a seed to reuse randomly from the pod
    index = (*p_seed_locator_distribution)(defrangen);

    assert(seed_pod.size() != 0);
    seed = seed_pod[index % seed_pod.size()];

    return std::make_pair(seed.first, seed.second);
}

std::string
DedupePatternRegulator::logmsg()
{
    o<< "target=" << target
     << ", spread=" << spread
     << ", event spectrum=" << event_span
     << ", reuse_probability=" << reuse_probability
     << ", unique_probability=" << unique_probability
     << ", target_probability=" << target_probability
     << ", high_probability=" << high_probability
     << ", unique_percentage=" << unique_percentage
     << ", target_percentage=" << target_percentage
     << ", high_percentage=" << high_percentage << std::endl;

    return o.str();
}
