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
//Author: Kumaran Subramaniam
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#pragma once

#include <random>
#include <unordered_map>
#include <sstream>

#include "ivydefines.h"

class DedupeTargetSpreadRegulator {
public:
    DedupeTargetSpreadRegulator(ivy_float target_dedupe = 1.0, uint64_t seed = universal_seed);
    ~DedupeTargetSpreadRegulator();

    // return rounded dedupe factor resets state
    inline ivy_float dedupe_distribution()
    {
        if (pos < high_percentage)
            state = target + spread;
        else if (pos < (high_percentage + unique_percentage))
            state = 1;
        else
            state = target;

        pos = (pos + 1) % 100;

        return (ivy_float) state;
    }

    bool decide_reuse();
    std::pair<uint64_t, uint64_t> reuse_seed();
    uint64_t random_seed();
    uint64_t get_distribution_pos() {return pos;}
    uint32_t get_spectrum() {return event_span;}
    uint32_t get_high_percentage() {return high_percentage;}

    std::string logmsg();
    uint64_t pattern_number_reuse_threshold {50000};
private:
    std::string generate_seeds();
    void record_seed(uint64_t, uint64_t);

    std::ostringstream o;
    // dedup distribution
    ivy_float target_dedupe;
    uint32_t target; // Target dedupe ratio (T = ceil(target_dedupe))
    uint32_t spread; // (T + spread) where  spread = 0 to 18
    ivy_float reuse_probability; // (1 - 1/T)

    ivy_float unique_probability;
    ivy_float target_probability;
    ivy_float high_probability;

    uint32_t unique_percentage;
    uint32_t target_percentage;
    uint32_t high_percentage;
    uint32_t event_span;

    // pattern state machine
    uint32_t state;
    uint32_t pos;

    std::uniform_int_distribution<uint64_t>* p_seed_locator_distribution {nullptr};
    std::uniform_int_distribution<uint64_t>* p_reuse_distribution {nullptr};
    std::default_random_engine defrangen;

    uint64_t max_seeds_in_pod;
    uint64_t starting_seed;
    bool seed_pod_initialized;
    std::vector<std::pair<uint64_t, uint64_t>> seed_pod;  // pair<pattern seed, pattern number>
    std::unordered_map<uint64_t, bool> seed_pod_map;
};
