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

#include "DedupeConstantRatioRegulator.h"

#include <assert.h>
#include <stdlib.h>
#include "ivy_engine.h"
#include "ivyhelpers.h"

DedupeConstantRatioRegulator::DedupeConstantRatioRegulator(ivy_float dedupe)
{
    assert(dedupe >= 1.0);

    dedupe_ratio = dedupe;
}

DedupeConstantRatioRegulator::~DedupeConstantRatioRegulator()
{
    //dtor
}

// This method returns the seed to use when generating an I/O for a write to the present LBA.
// The basic algorithm is simple.  The range of LBAs to be written is divided up into a series
// of chunks, each max_buckets in length.  Within a chunk, a pattern is chosen at random from
// among a constrained universe of patterns that have a constant deduplication ratio.  For
// example, a chunk might comprise 128 blocks.  At a (say) 2.0 dedupe ratio, the constrained
// universe comprises only 64 allowed patterns.  One of these is chosen at random (uniformly
// distributed).  Thus, if all of the blocks in the chunk are written, then with high likelihood,
// the chunk will dedupe 2:1.  If (say) a dedupe ratio of 1.5 is desired, then 128/1.5, or
// 85 patterns are allowed, and so forth.

uint64_t DedupeConstantRatioRegulator::get_seed(uint64_t lba)
{
    uint64_t seed;
    uint64_t index;

    // Which of the allowed patterns shall we use?  Chooses one from [0..(uint64_t)(max_patterns/dedupe_ratio)].

    index = (uint64_t) (distribution(generator) * ((ivy_float) max_seeds) / dedupe_ratio);

    // For this chunk, select the starting seed.  All LBAs in a given range have the same starting seed.
    //
    // Note: This is a relatively ugly way to seed the pattern generator.  A nicer way might involve
    // cryptography, e.g., 3DES(lba/max_patterns, FIXED_KEY), but I don't think we can use cryptographic
    // methods due to export control issues.  So this will suffice.

    srand48(lba/max_seeds);
    seed = lrand48();  // LFSR-based.

    // Compute the pattern for the ith allowed pattern in the chunk.
    // Note: Always call xorshift64star() at least once!

    for (uint64_t i = 0; i < 1 + index; i++)
        xorshift64star(seed);

    // And that's that.

    return seed;
}
