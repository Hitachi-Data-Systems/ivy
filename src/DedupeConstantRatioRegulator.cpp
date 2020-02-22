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

#include <assert.h>

#include "ivytypes.h"
#include "ivyhelpers.h"
#include "DedupeConstantRatioTable.h"
#include "DedupeConstantRatioRegulator.h"
#include "Eyeo.h"

// Simple linear lookup as table is short and lookup is done only once.

void DedupeConstantRatioRegulator::lookup_sides_and_throws(uint64_t &sides, uint64_t &throws)
{
    // Just do linear search.  Assumes table is in sorted order already.

    auto prev = table_by_dedupe_ratio.begin();
    for (auto it = table_by_dedupe_ratio.begin(); it != table_by_dedupe_ratio.end(); ++it)
    {
        // Find the closest match.

        if (it->first > dedupe_ratio)
        {
            if ((it->first - dedupe_ratio) < (dedupe_ratio - prev->first))
            {
                sides = it->second.first;
                throws = it->second.second;
            }
            else
            {
                sides = prev->second.first;
                throws = prev->second.second;
            }
            return;
        }
        prev = it;
    }

    // Out of range (too big).

    sides = prev->second.first;
    throws = prev->second.second * dedupe_ratio / prev->first + 0.5;  // Good estimate.
}

void DedupeConstantRatioRegulator::compute_range(uint64_t &range)
{
    range = throws * block_size;
}

DedupeConstantRatioRegulator::DedupeConstantRatioRegulator(ivy_float dedupe, uint64_t block, ivy_float zero_blocks, uint64_t workloadID_hash)
{
    assert(dedupe >= 1.0);
    assert((block % min_dedupe_block) == 0);
    assert(zero_blocks >= 0.0 && zero_blocks <= 1.0);

    dedupe_ratio = dedupe;
    block_size = block;
    zero_blocks_ratio = zero_blocks;

    // Remember the hash of the workloadID

    workloadID = workloadID_hash;

    // Lookup sides and throws from computed table.

    lookup_sides_and_throws(sides, throws);

    // Compute a range of blocks.

    compute_range(range);
}

DedupeConstantRatioRegulator::~DedupeConstantRatioRegulator()
{
    //dtor
}

// This method returns the seed to use when generating an I/O for a write to the present offset.
// The algorithm is simple.  A pattern index from the range [1..sides] is chosen, indicating which
// pattern seed to use within the present range. A range (comprising throw # of blocks) is filled
// with patterns randomly chosen.  This means that on average, dedupe_ratio will be attained.
// Once a seed for the block has been chosen, the seed is xor-shifted until a "random" number has
// been selected as the seed for the pattern for the block.

uint64_t DedupeConstantRatioRegulator::get_seed(Eyeo *p_eyeo, uint64_t offset)
{
    uint64_t modbase;
    uint64_t seed;
    uint64_t blockno;
    uint64_t congruent;
    uint64_t modblock;
    uint64_t modoffs;
    int index;

    // Should this be a zero-filled block?

    modblock = p_eyeo->zero_pattern_filtered_sub_block_number(offset - (uint64_t) (p_eyeo->sqe.off));
    if (modblock == 0)
    {
        return 0; // Generate a zero-filled block.
    }

    // For this range, select the appropriate "modulo block." A range comprises a "(throws * block_size)"
    // non-overlapping region.  A modulo block is the address of the ith block in the range modulo the
    // number of throws.  The offset of the current I/O has an associated modulo block, which is the
    // congruency class for the current block.  Compute the base address of the current modulo block.

    modoffs = modblock * min_dedupe_block;
    modbase = (modoffs / range) * range;
    blockno = (modoffs - modbase) / min_dedupe_block;

    // This is a nonsense number corresponding to the congruency class.

    congruent = modbase + blockno / throws;

    // Seed a random number generator with the congruency class and
    // generate a first seed.

    xorshift64star(congruent);
    srand48(congruent);
    seed = lrand48();  // LFSR-based.

    // Determine which pattern to generate.

    index = distribution(generator) * sides;

    // Compute the pattern for the ith allowed pattern in the chunk.
    // Note: Always call xorshift64star() at least once!

    for (int i = 0; i < 1 + index; i++)
        xorshift64star(seed);

    // Account for workloadID on different volumes.

    seed ^= workloadID;

    // And that's that.

    return seed;
}
