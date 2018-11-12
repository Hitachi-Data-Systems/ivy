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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>, Kumaran Subramaniam <kumaran.subramaniam@hitachivantara.com>
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

#include <linux/aio_abi.h>

#include "ivytime.h"
#include "ivydefines.h"
#include "pattern.h"
#include "Workload.h"

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
	struct iocb eyeocb;  // see #include <linux/aio_abi.h>
		// On my development machine, I put a symbolic link "aio_abi.h" in the ivy src
		// folder pointing to /usr/include/linux/aio_abi.h to make easy to reference when coding.
		// Browsing the web indicates there's other locations and forms of aio header file on different systems.  Hope the code is portable, resigned if not.

	ivytime scheduled_time {ivytime(0)};  // if we have iorate=max, this is indicated by setting scheduled_time=ivytime(0).
	ivytime start_time     {ivytime(0)};
	ivytime running_time   {ivytime(0)};
	ivytime end_time       {ivytime(0)};
	int return_value {-1};
	int errno_value  {-1};

	Workload* pWorkload;
	bool iops_max_counted {false};

    uint64_t io_sequence_number {0};

// methods
	Eyeo(Workload*);
	~Eyeo(){};

	std::string toString();
	std::string thumbnail();
	void resetForNextIO();
	ivy_float service_time_seconds();
	ivy_float response_time_seconds();
	ivy_float submit_time_seconds();
	ivy_float running_time_seconds();

	std::string buffer_first_last_16_hex();

	void generate_pattern();
	ivytime since_start_time(); // returns ivytime(0) if start_time is not in the past
};

std::ostream& operator<<(std::ostream&, const struct io_event&);
