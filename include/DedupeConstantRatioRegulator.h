//Copyright (c) 2019 Hitachi Vantara Corporation
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
//Authors: Stephen Morgan <stephen.morgan@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#pragma once

class Eyeo;

class DedupeConstantRatioRegulator
{
    public:
        DedupeConstantRatioRegulator(ivy_float dedupe, uint64_t block, ivy_float zero_blocks, uint64_t workloadID_hash);
        virtual ~DedupeConstantRatioRegulator();
        uint64_t get_seed(Eyeo *p_eyeo, uint64_t offset);

    protected:

    private:

    std::default_random_engine generator;
    std::uniform_real_distribution<ivy_float> distribution;

    const uint64_t min_dedupe_block = 8192;

    uint64_t workloadID;
    uint64_t block_size;
    uint64_t sides;
    uint64_t throws;
    uint64_t range;
    uint64_t tbpiob; // total blocks per i/o block
    ivy_float dedupe_ratio;
    ivy_float zero_blocks_ratio;

    void lookup_sides_and_throws(uint64_t &sides, uint64_t &throws);
    void compute_range(uint64_t &range);
};
