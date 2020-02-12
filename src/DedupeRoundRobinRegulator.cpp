//Copyright (c) 2020 Hitachi Vantara Corporation
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
#include <stdlib.h>

#include <sstream>

#include "DedupeRoundRobinRegulator.h"
#include "DedupeRoundRobinSingleton.h"
#include "Eyeo.h"

DedupeRoundRobinRegulator::DedupeRoundRobinRegulator(uint64_t my_coverage_blocks, uint64_t my_block_size,
													 ivy_float my_dedupe_ratio, uint64_t my_dedupe_unit_bytes,
													 LUN *pLUN)
{
	assert(my_coverage_blocks > 0);
	assert(my_block_size > 0);
	assert(my_dedupe_ratio >= 1.0);
	assert((my_dedupe_unit_bytes > 0) && (my_dedupe_unit_bytes % dedupe_unit_bytes_default == 0));
	assert(pLUN != nullptr);

    DedupeRoundRobinSingleton& DRRS = DedupeRoundRobinSingleton::get_instance();

    // Add this LUN to the workload along with parameters.

    LUN_number = DRRS.add_LUN(my_coverage_blocks, my_block_size, my_dedupe_ratio, my_dedupe_unit_bytes, pLUN);
}

DedupeRoundRobinRegulator::~DedupeRoundRobinRegulator()
{
    DedupeRoundRobinSingleton& DRRS = DedupeRoundRobinSingleton::get_instance();

    // Delete this LUN. If it's the last LUN in the workload, re-initialize the workload.

    DRRS.del_LUN(LUN_number);
}

uint64_t DedupeRoundRobinRegulator::get_seed(Eyeo *p_eyeo, uint64_t offset)
{
    uint64_t modblock;

    DedupeRoundRobinSingleton& DRRS = DedupeRoundRobinSingleton::get_instance();

    // Should this be a zero-filled block?

    modblock = p_eyeo->zero_pattern_filtered_sub_block_number(offset - (uint64_t) (p_eyeo->eyeocb.aio_offset));
    if (modblock == 0)
    {
        return 0; // Generate a zero-filled block.
    }

    // Not zero-filled, so use round-robin generator.

    return DRRS.get_seed(LUN_number, modblock);
}
