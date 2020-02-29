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

#include "ivytypes.h"
#include "DedupeRoundRobinSingleton.h"
#include "DedupeRoundRobinRegulator.h"
#include "Eyeo.h"
#include "Workload.h"
#include "WorkloadID.h"

DedupeRoundRobinRegulator::DedupeRoundRobinRegulator(
		Workload &workload,
		uint64_t my_blocks,
		uint64_t my_block_size,
		ivy_float my_dedupe_ratio,
		uint64_t my_dedupe_unit_bytes,
		LUN *pLUN
		)
{
	if (debugging) {
		assert(my_blocks > 0);
		assert(my_block_size > 0);
		assert(my_dedupe_ratio >= 1.0);
		assert((my_dedupe_unit_bytes > 0) && (my_dedupe_unit_bytes % dedupe_unit_bytes_default == 0));
		assert(pLUN != nullptr);
	}

    string workloadID = workload.workloadID.workloadID;

    host_part = WorkloadID(workloadID).getHostPart();
    lun_part = WorkloadID(workloadID).getLunPart();
    work_part = WorkloadID(workloadID).getWorkloadPart();

    host_hash = myHash(host_part);
    lun_hash = myHash(lun_part);
    work_hash = myHash(work_part);

    if (logging) {
   		ostringstream oss;
   		oss << __func__ << "(" << __LINE__ << "): "
   			<< "workloadID = \"" << workloadID << "\""
			<< ", host_part = \"" << host_part << "\""
			<< ", lun_part = \"" << lun_part << "\""
			<< ", work_part = \"" << work_part << "\""
		    << ", host_hash = 0x" << hex << showbase << internal << setfill('0') << setw(18) << host_hash << setfill(' ') << dec
		    << ", lun_hash = 0x" << hex << showbase << internal << setfill('0') << setw(18) << lun_hash << setfill(' ') << dec
		    << ", work_hash = 0x" << hex << showbase << internal << setfill('0') << setw(18) << work_hash << setfill(' ') << dec
			<< ", my_blocks = " << my_blocks
			<< ", my_block_size = " << my_block_size
			<< ", my_dedupe_ratio = " << my_dedupe_ratio
			<< ", my_dedupe_unit_bytes = " << my_dedupe_unit_bytes
			<< "." << endl;
   		logger logfile {"/tmp/spm.txt"};
   		log(logfile, oss.str());
    }

    DedupeRoundRobinSingleton& DRRS = DedupeRoundRobinSingleton::get_instance();

    // Add this LUN to the workload along with parameters.

    DRRS.add_LUN(host_part, lun_part, work_part, host_hash, lun_hash, work_hash,
    			 my_blocks, my_block_size, my_dedupe_ratio, my_dedupe_unit_bytes, pLUN);

    if (logging) {
   		ostringstream oss;
   		oss << __func__ << "(" << __LINE__ << "): "
   			<< "done"
			<< ", workloadID = \"" << workloadID << "\""
			<< "." << endl;
   		logger logfile {"/tmp/spm.txt"};
   		log(logfile, oss.str());
    }
}

DedupeRoundRobinRegulator::~DedupeRoundRobinRegulator()
{
    DedupeRoundRobinSingleton& DRRS = DedupeRoundRobinSingleton::get_instance();

    // Delete this LUN. If it's the last LUN in the workload, re-initialize the workload.

    if (logging) {
   		ostringstream oss;
   		oss << __func__ << "(" << __LINE__ << "): "
			<< "host_part = \"" << host_part << "\""
			<< ", lun_part = \"" << lun_part << "\""
			<< ", work_part = \"" << work_part << "\""
			<< "." << endl;
   		logger logfile {"/tmp/spm.txt"};
   		log(logfile, oss.str());
    }

    DRRS.del_LUN(host_part, lun_part, work_part, host_hash, lun_hash, work_hash);
}

uint64_t DedupeRoundRobinRegulator::get_seed(Eyeo *p_eyeo, uint64_t offset)
{
    uint64_t modblock;

    DedupeRoundRobinSingleton& DRRS = DedupeRoundRobinSingleton::get_instance();

    // Should this be a zero-filled block?

    modblock = p_eyeo->zero_pattern_filtered_sub_block_number(offset - (uint64_t) (p_eyeo->sqe.off));
    if (modblock == 0)
    {
        return 0; // Generate a zero-filled block.
    }

    // Not zero-filled, so use round-robin generator.

    return DRRS.get_seed(host_part, lun_part, work_part, host_hash, lun_hash, work_hash, modblock);
}
