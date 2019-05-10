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

// This table indicates the number of slots needed to achieve a given dedupe ratio.
// It was computed assuming a "multiplier" value of 64, i.e., a 64-sided die.  For
// each entry, it indicates the number of rolls of a 64-sided die needed to achieve
// the associated deduplication ration.
//
// Note: Table must be maintained in sorted order as we use binary search on it.

const DedupeConstantRatioRegulator::slots_ratio_t dedupe_table[] = {
    {1, 1.000000},
    {12, 1.099388},
    {24, 1.198259},
    {36, 1.306506},
    {46, 1.405164},
    {56, 1.496989},
    {66, 1.601150},
    {76, 1.706145},
    {85, 1.805573},
    {93, 1.900876},
    {102, 1.999506},
    {111, 2.106796},
    {119, 2.201853},
    {127, 2.301546},
    {135, 2.397511},
    {143, 2.499986},
    {151, 2.599669},
    {158, 2.700606},
    {166, 2.798501},
    {174, 2.909903},
    {181, 3.004809},
    {188, 3.100742},
    {195, 3.195051},
    {202, 3.293802},
    {210, 3.406981},
    {217, 3.507099},
    {224, 3.607673},
    {230, 3.696804},
    {237, 3.794198},
    {244, 3.900303},
    {251, 4.001688},
    {258, 4.105323},
    {265, 4.206345},
    {271, 4.296486},
    {278, 4.397910},
    {285, 4.503196},
    {292, 4.606941},
    {298, 4.701113},
    {305, 4.806613},
    {311, 4.895962},
    {318, 5.001684},
    {325, 5.107522},
    {331, 5.202206},
    {338, 5.308755},
    {344, 5.401302},
    {351, 5.505001},
    {357, 5.597496},
    {364, 5.706888},
    {370, 5.799379},
    {377, 5.906568},
    {383, 6.000768},
    {390, 6.106739},
    {396, 6.200374},
    {403, 6.308596},
    {409, 6.402702},
    {416, 6.508884},
    {422, 6.601501},
    {428, 6.697162},
    {435, 6.804755},
    {441, 6.896972},
    {448, 7.006013},
    {454, 7.100521},
    {461, 7.208619},
    {467, 7.300930},
    {473, 7.394855},
    {480, 7.505966},
    {486, 7.597730},
    {493, 7.706302},
    {499, 7.799719},
    {506, 7.909642},
    {512, 8.002797},
    {528, 8.252360},
    {544, 8.501348},
    {560, 8.750972},
    {576, 9.001286},
    {592, 9.250881},
    {608, 9.500452},
    {624, 9.750310},
    {640, 10.000318},
};

int DedupeConstantRatioRegulator::compute_max_seeds(const slots_ratio_t *array, int size, ivy_float target)
{
    assert(array != nullptr);
    assert(size > 0);

    // Corner cases.

    if (target < array[0].ratio)
        return array[0].slots;

    // Out of range.

    if (target > array[size - 1].ratio)
        return (int) (target * multiplier);  // Very close approximation.

    // Binary search table for closest match.

    int i = 0, j = size, mid = 0;
    while (i < j) {
        mid = (i + j) / 2;
        if (array[mid].ratio == target)
            return array[mid].slots;

        if (target < array[mid].ratio)
        {
            if ((mid > 0) && (target > array[mid - 1].ratio))
            {
                if (target - array[mid - 1].ratio <= array[mid].ratio - target)
                    return array[mid - 1].slots;
                else
                    return array[mid].slots;
            }
            j = mid;
        }
        else
        {
            if ((mid < size - 1) && (target < array[mid + 1].ratio))
            {
                if (target - array[mid].ratio <= array[mid + 1].ratio - target)
                    return array[mid].slots;
                else
                    return array[mid + 1].slots;
            }
            i = mid + 1;
        }
    }
    return array[mid].slots;
}

uint64_t DedupeConstantRatioRegulator::compute_range(void)
{
    return max_seeds * 8192 * min((int) (block_size + 8191) / 8192, (int) (1.0 / (1.0 - compression_ratio)));
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

    max_seeds = compute_max_seeds(dedupe_table, (int) sizeof(dedupe_table)/sizeof(dedupe_table[0]), dedupe_ratio);

    range = compute_range();
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

    index = (int) (distribution(generator) * multiplier);

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
