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

void DedupeConstantRatioRegulator::lookup_sides_and_throws(uint64_t &sides, uint64_t &throws)
{
    // Just do linear search.

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

DedupeConstantRatioRegulator::DedupeConstantRatioRegulator(ivy_float dedupe, uint64_t block, ivy_float compression)
{
    assert(dedupe >= 1.0);
    assert((block % min_dedupe_block) == 0);
    assert(compression >= 0.0 && compression <= 1.0);

    dedupe_ratio = dedupe;
    block_size = block;
    compression_ratio = compression;

    // We don't support compression ratios greater than 99.9% (i.e., 1000:1).

    if (compression_ratio > (ivy_float) 0.999)
        compression_ratio = (ivy_float) 0.999;

    // Compute total blocks (tbpiob), data blocks (dbpiob) and zero blocks (zbpiob) per I/O block.

    tbpiob = block_size / min_dedupe_block;
    dbpiob = ceil((ivy_float) tbpiob * (1.0 - compression_ratio));
    zbpiob = tbpiob - dbpiob;

    // Compute a range of blocks (not counting compression ratio).

    lookup_sides_and_throws(sides, throws);
    compute_range(range);
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
    uint64_t base;
    uint64_t seed;
    uint64_t blockno;
    uint64_t modbase;
    uint64_t modblock;
    int index;

    // Which of the allowed patterns shall we use?

    index = (int) (distribution(generator) * sides);

    // For this range, select the appropriate "modulo block." A range comprises a "(throws * block_size)"
    // non-overlapping region.  A modulo block is the address of the ith block in the range modulo the
    // number of throws.  The offset of the current I/O has an associated modulo block, which is the
    // congruency class for the current block.  Compute the base address of the current modulo block.

    base = (offset / range) * range;
    blockno = (offset - base) / min_dedupe_block;
    modblock = (blockno / tbpiob) * dbpiob + (blockno % tbpiob);
    modbase = base + modblock / throws;  // This is a nonsense number corresponding to the congruency class.

    xorshift64star(modbase);

    srand48(modbase);
    seed = lrand48();  // LFSR-based.

    // Compute the pattern for the ith allowed pattern in the chunk.
    // Note: Always call xorshift64star() at least once!

    for (int i = 0; i < 1 + index; i++)
        xorshift64star(seed);

    // And that's that.

    return seed;
}
