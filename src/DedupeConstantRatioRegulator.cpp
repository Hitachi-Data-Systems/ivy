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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>, Kumaran Subramaniam <kumaran.subramaniam@hitachivantara.com>, Stephen Morgan <stephen.morgan@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.

#include <assert.h>
#include <stdlib.h>
#include "ivy_engine.h"
#include "ivyhelpers.h"
#include "DedupeConstantRatioRegulator.h"
#include "DedupeConstantRatioTable.h"

// Simple linear lookup as table is short and lookup is done only once.

void DedupeConstantRatioRegulator::lookup_sides_and_throws(ivy_float dedupe_ratio, ivy_float &sides, uint64_t &throws)
{
        // Out of range (too small).

        if (dedupe_ratio < 1.0)
                dedupe_ratio = 1.0;

        // Just do linear search.

        auto prev = table_by_dedupe_ratio.begin();
        for (auto it = table_by_dedupe_ratio.begin(); it != table_by_dedupe_ratio.end(); ++it)
        {
                // Find the closest match.

                if (it->first > dedupe_ratio)
                {
                        if ((it->first - dedupe_ratio) < (dedupe_ratio - prev->first))
                        {
                                sides = (ivy_float) it->second.first;
                                throws = it->second.second;
                        }
                        else
                        {
                                sides = (ivy_float) prev->second.first;
                                throws = prev->second.second;
                        }
                        return;
                }
                prev = it;
        }

        // Out of range (too big).

        sides = (ivy_float) prev->second.first;
        throws = (unsigned int) prev->second.first * dedupe_ratio;  // Good estimate.
}

void DedupeConstantRatioRegulator::compute_range(ivy_float &sides, uint64_t &throws, uint64_t block_size, ivy_float compression_ratio, uint64_t &range)
{
    // Complexity comes in because of compression, especially for large blocks.

    if (compression_ratio == 0.0)
    {
        range = throws * 8192;
    }
    else
    {
        uint64_t data_blocks_per_io_block = (uint64_t) ceil((ivy_float) ((uint64_t) ((block_size + 8191) / 8192)) * (1.0 - compression_ratio));
        ivy_float expansion_ratio = (ivy_float) throws / data_blocks_per_io_block;

        if (expansion_ratio >= 1.0)
        {
            range = throws * block_size / data_blocks_per_io_block;
        }
        else
        {
            // Assumes that maintaining the sides:throws ratio will approximate the desired dedupe ratio.

            sides /= expansion_ratio;
            throws = data_blocks_per_io_block;
            range = block_size;
        }
    }
}

DedupeConstantRatioRegulator::DedupeConstantRatioRegulator(ivy_float dedupe, uint64_t block, ivy_float compression)
{
    assert(dedupe >= 1.0);
    assert(block > 0);
    assert(compression >= 0 && compression <= 1.0);

    dedupe_ratio = dedupe;
    block_size = block;
    compression_ratio = compression;

    // We don't support compression ratios greater than 99.9%

    if (compression_ratio > (ivy_float) 0.999)
        compression_ratio = (ivy_float) 0.999;

    // Compute a range of blocks (not counting compression ratio).

    lookup_sides_and_throws(dedupe_ratio, sides, throws);
    compute_range(sides, throws, block_size, compression_ratio, range);
}

DedupeConstantRatioRegulator::~DedupeConstantRatioRegulator()
{
    //dtor
}

// This method returns the seed to use when generating an I/O for a write to the present offset.
// The algorithm is simple.  A pattern index from the range [0..multiplier-1] is chosen, indicating
// which pattern seed to use within the present range.  (Multiplier is currently set to 64 but can
// be any reasonably small integer.)  A range of (dedupe_ratio * multiplier) blocks is filled with
// patterns randomly chosen.  This means that on average, dedupe_ratio will be attained.  Note that
// special care must be taken for blocks larger than 8 KiB that are filled with trailing zeroes (in
// the case of compression, hence the complex math formula for seeding srand48()).  Once a seed for
// the range has been chosen, the seed is xor-shifted until a "random" number is chosen as the seed
// for the pattern for the block.

uint64_t DedupeConstantRatioRegulator::get_seed(uint64_t offset)
{
    uint64_t seed;
    int index;

    // Which of the allowed patterns shall we use?

    index = (int) (distribution(generator) * sides);

    // For this chunk, select the starting seed.  All offsets in a given range have the same starting seed.

    srand48(offset / range);
    seed = lrand48();  // LFSR-based.

    // Compute the pattern for the ith allowed pattern in the chunk.
    // Note: Always call xorshift64star() at least once!

    for (int i = 0; i < 1 + index; i++)
        xorshift64star(seed);

    // And that's that.

    return seed;
}
