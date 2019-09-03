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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.

#pragma once

#include <queue>
#include <set>

#include "Subinterval.h"
#include "IosequencerInput.h"
#include "WorkloadID.h"
#include "TestLUN.h"
#include "DedupeTargetSpreadRegulator.h"
#include "DedupeConstantRatioRegulator.h"

//#define IVYDRIVER_TRACE

typedef long double pattern_float_type;

class WorkloadThread;
class Eyeo;
class Iosequencer;

class Workload
{
public:
//variables

    WorkloadID workloadID;
    uint64_t uint64_t_hash_of_host_plus_LUN;

    TestLUN* pTestLUN;

    WorkloadThread* pWorkloadThread;

	Subinterval subinterval_array[2];

	Subinterval*       p_current_subinterval;
	IosequencerInput*  p_current_IosequencerInput;
	SubintervalOutput* p_current_SubintervalOutput;

	std::string iosequencerParameters; // just used to print a descriptive line when the thread fires up.

	ivytime nextIO_schedule_time;  // 0 means means right away.  (This is initialized later.)

	Iosequencer* p_my_iosequencer {nullptr};

    ivy_float sequential_fill_fraction {1.0};
    // IogeneratorSequential for writes sets this to a value expressing the progress from 0.0 to 1.0
    // of what fraction of all blocks in the coverage range within the LUN have been written to so far.

	std::list<Eyeo*> allEyeosThatExist;

	std::stack<Eyeo*> freeStack;

	std::list<Eyeo*> precomputeQ;  // We are using a std::list instead of a std::queue so that
	                               // if io_submit() doesn't successfully launch all of a batch
	                               // then we can put the ones that didn't launch back into
	                               // the precomputeQ in original sequence at the wrong end backwards.

	std::queue<Eyeo*> postprocessQ;


	std::set<struct iocb*> running_IOs;
	    // The iocb* also points to the containing ivy Eyeo object,
	    // as the iocb is the first thing in an Eyeo.  This way
	    // we can pass the address in as an iocb* when talking to Linux,
	    // but when it answers back about the I/O, we refer to what
	    // Linux gives us as an Eyeo*.

	ivy_int cancelled_IOs {0};

    ivy_int EyeoCount;
	int currentSubintervalIndex, otherSubintervalIndex, subinterval_number;  // The master thread never sets these or refers to them.
	int ioerrorcount=0;
	unsigned int precomputedepth;  // will be set to be equal to the maxTags spec when we get "Go!"
	int rc;
	unsigned int workload_queue_depth{0};
	unsigned int workload_max_queue_depth{0};
	int events_needed;
	int reaped;

	std::string hostname;

    DedupeTargetSpreadRegulator *dedupe_target_spread_regulator;
    DedupeConstantRatioRegulator *dedupe_constant_ratio_regulator;

    bool doing_dedupe;
    uint64_t constant_ratio_sides;
    uint64_t constant_ratio_throws;
    uint64_t throw_group_size;

    std::uniform_int_distribution<uint64_t>*   p_sides_distribution {nullptr};
    bool have_writes;
    ivy_float compressibility;

    pattern_float_type threads_in_workload_name;
    pattern_float_type serpentine_number;
    pattern_float_type serpentine_multiplier;

    pattern pat;
    uint64_t pattern_number;
    uint64_t pattern_alignment;
    uint64_t pattern_seed;
    uint64_t block_seed;
    uint64_t last_block_seeds[128]; // array of 8KiB pattern block_seeds to span upto 1 MiB xfer block_size
    uint8_t dedupeunits;
    int dedupe_count;
    ivy_float modified_dedupe_factor;

    uint64_t write_io_count;

    uint64_t iops_max_io_count {0};
    ivy_float iops_max_progress {0};

	std::unique_ptr<unsigned char[]> ewe_neek {}; // Points to a buffer pool for the Eyeos for this workload

    uint64_t workload_cumulative_launch_count {0};
    uint64_t workload_cumulative_completion_count {0};
    ivy_float workload_weighted_IOPS_max_skew_progress {0.0};

#ifdef IVYDRIVER_TRACE
    unsigned int workload_callcount_prepare_to_run {0};
    unsigned int workload_callcount_build_Eyeos {0};
    unsigned int workload_callcount_switchover {0};
    unsigned int workload_callcount_pop_and_process_an_Eyeo {0};
    unsigned int workload_callcount_front_launchable_IO {0};
    unsigned int workload_callcount_load_IOs_to_LaunchPad {0};
    unsigned int workload_callcount_generate_an_IO {0};

    unsigned int start_IOs_Workload_bookmark_count {0};
    unsigned int start_IOs_Workload_body_count {0};
    unsigned int pop_one_bookmark_count {0};
    unsigned int pop_one_body_count {0};
    unsigned int generate_one_bookmark_count {0};
    unsigned int generate_one_body_count {0};
#endif

//methods
    Workload();
    ~Workload();
    void prepare_to_run();
	void switchover();
	void build_Eyeos();
	unsigned int /* number of I/Os popped and processed */ pop_and_process_an_Eyeo();
	unsigned int generate_an_IO(); // returns 1 if we generated an I/O, zero if not
	void load_IOs_to_LaunchPad();
	Eyeo* front_launchable_IO(const ivytime& now);
	    // Returns precomputeQ.front() if all of the following are true:
        //   - queue_depth < maxTags
	    //   - precompute queue size > 0
	    //   - Scheduled time is less than or equal to "now" - always true for IOPS = max
	    // Returns nullptr otherwise.

    std::string brief_status();

    inline bool is_IOPS_max_with_positive_skew()
        { return p_current_IosequencerInput->IOPS == -1 && p_current_IosequencerInput->skew_weight > 0.0; }

    inline bool hold_back_this_time()
        { return workload_weighted_IOPS_max_skew_progress > pTestLUN->testLUN_furthest_behind_weighted_IOPS_max_skew_progress; }

    void post_warning(const std::string& msg) { pTestLUN->post_warning (workloadID.workloadID + " - "s + msg); }

};
