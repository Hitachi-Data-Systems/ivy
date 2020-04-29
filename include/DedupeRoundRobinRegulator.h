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
#pragma once

#include "ivytypes.h"
#include "Workload.h"

class Eyeo;

class DedupeRoundRobinRegulator
{
	private:

		static const bool debugging {true};
		static const bool logging {false};

		// Private fields to identify the workload, LUN, and host.

		string host_part;
		string lun_part;
		string work_part;

		// A hashing method used by this class.

		hash<string> myHash;

		// Private fields associated with the workload, LUN, and host strings, for fast access.

		uint64_t host_hash;
		uint64_t lun_hash;
		uint64_t work_hash;

    public:
    
		// Constructor for the round_robin deduplication class.

		DedupeRoundRobinRegulator(
				Workload &workload,
				uint64_t my_coverage_blocks,
				uint64_t my_block_size,
				ivy_float my_dedupe_ratio,
				uint64_t my_dedupe_unit_bytes,
				LUN *pLUN
				);

		// Destructor for the round_robin deduplication class.

		virtual ~DedupeRoundRobinRegulator();

		// Generate a seed to construct a block for a particular write.

		uint64_t get_seed(Eyeo *p_eyeo, uint64_t offset);

    protected:

};
