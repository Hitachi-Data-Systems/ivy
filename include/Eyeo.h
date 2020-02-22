//Copyright (c) 2016, 2017, 2018 Hitachi Vantara Corporation
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
//Author: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#pragma once

// "As for allocating memory with an alignment greater than alignof(std::max_align_t), C++11 provides no direct way to do this.
// The only reliable way is to allocate at least size + alignment bytes and use std::align to get a correctly aligned location in this buffer.

// This can waste a lot of memory, so if you need a lot of these, you could create an allocator that allocates a chunk large enough
// for all of them and usestd::align on that. Your overhead is then amortized across all of the allocations."

// Ian: I was having problems with aligned memory allocation and freeing.  Crazy thing is that it worked sometimes
// and then when it fails there was no error return - the thread just exits.  Didn't try try/catch.

// Then reading up as quoted above it seems others have been down this path, and so I'm redesigning Eyeos
// so that the input/output buffer memory for all of them is going to be allocated in one piece,
// rounding up blocksize to a page (4096) boundary, and adding 4095 so that std::align can give us the
// aligned start of an array of I/O buffers.

#include "ivytypes.h"
#include "ivytime.h"
#include "ivydefines.h"
#include "Workload.h"
#include "WorkloadThread.h"

//////////////////////////////////////////////////////////////////////////////////////
//
//  Warning - resetForNextIO() only saves certain fields in the Eyeo object
//            before resetting the entire Eyeo object to all binary zeros
//            and then restores the specific saved values.
//
//  The original idea was to clear everything including the aio struct iocb to
//  make sure residual things left behind by the aio mechanism couldn't affect
//  subsequent I/Os, but then I got bit by a bug where I had forgotten to save the WorkloadThread pointer around resets ...
//
//  That way if you are having a weird problem, this might be why.
//
// To do - rewrite resetForNextIO() to remove memset to zero and initialize each field appropriately
//
//////////////////////////////////////////////////////////////////////////////////////



class Eyeo {
public:

// variables

    cqe_shim shim;

    io_uring_sqe sqe;

    char* p_buffer;

	ivytime scheduled_time {ivytime_zero};  // if we have iorate=max, this is indicated by setting scheduled_time=ivytime(0). // @suppress("Invalid arguments")
	ivytime start_time     {ivytime_zero}; // @suppress("Invalid arguments")
	ivytime end_time       {ivytime_zero}; // @suppress("Invalid arguments")
	int return_value {-1};
	uint32_t cqe_flags {0};
	//int errno_value  {-1};

	Workload* pWorkload;
//	bool iops_max_counted {false};

    uint64_t io_sequence_number {0};
    uint64_t completion_sequence_number {0};

// methods
	Eyeo(Workload*, uint64_t& index_reference);
	~Eyeo(){};

	std::string thumbnail();
	void resetForNextIO();
	ivy_float service_time_seconds() const;
	ivy_float response_time_seconds() const;

	ivy_float lookup_probabilistic_compressibility(ivy_float compressibility, uint64_t address);

	std::string buffer_first_last_16_hex();

	void generate_pattern();
	ivytime since_start_time(); // returns ivytime(0) if start_time is not in the past

    uint64_t zero_pattern_filtered_sub_block_number(uint64_t offset_within_this_Eyeo_buffer);
        // An (unfiltered) sub-block number is the offset within the LUN in bytes, divided by dedupe_unit_bytes.

        // "blocksize_bytes" is a multiple of "dedupe_unit_bytes", so there may be multiple sub-blocks within the Eyeo pattern.

        // Returns 0 if this is a zero pattern sub-block.

        // Otherwise, returns what the sub-block number would be when not counting zero pattern blocks.

        // For example, if we have a 25% fraction_zero_pattern, for sub-blocks 1, 2, 3, ... it returns
        // 0, 1, 2, 3, 0, 4, 5, 6, 0, 7, 8, 9,  ...

	uint64_t fixed_pattern_sub_block_starting_seed(uint64_t offset_within_this_Eyeo_buffer);

	    // Returns 0 if this should be an all-zeros sub-block, based on IosequencerInput's "fraction_zero_pattern" parameter.

        // Otherwise first calculates a sub-block number with the right number of duplicates
        // to hit the required dedupe ratio, once zero (sub-)blocks are removed.

        // For example, if we have a 25% fraction_zero_pattern, and a 1.5:1 dedupe ratio, for (sub-)blocks 1, 2, 3, ... it calculates
        // 0, 1, 2, 3, 0, 3, 4, 5, 0, 5, 6, 7, 0, 7, 8, 9, 0, ...

        // Then finally, for non-zero sub-blocks, it returns the 64-but hash of the workload ID
        // XORed with the sub-block number, including duplicates as shown in the series immediately above.

    uint64_t duplicate_set_filtered_sub_block_number(uint64_t offset_within_this_Eyeo_buffer);
	uint64_t duplicate_set_sub_block_starting_seed(uint64_t offset_within_this_Eyeo_buffer);

private:

    std::default_random_engine generator;
    std::uniform_real_distribution<ivy_float> distribution;
};

std::ostream& operator<<(std::ostream&, const struct Eyeo&);
std::ostream& operator<<(std::ostream&, const struct io_event&);
std::ostream& operator<<(std::ostream&, const io_uring_cqe&);
std::ostream& operator<<(std::ostream&, const io_uring_sqe&);
std::ostream& format_cqe_with_associated_shims_and_IOs(std::ostream&, const io_uring_cqe&);


