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

#include "Eyeo.h"
#include "IosequencerInput.h"
#include "ivytime.h"
#include "LUN.h"
#include "WorkloadThread.h"

class TestLUN;
class WorkloadThread;
class Workload;
class Eyeo;

class Iosequencer
{

public:
	LUN* pLUN;

	std::string logfilename;
	WorkloadID workloadID;
	WorkloadThread* pWorkloadThread;
	TestLUN* pTestLUN;
	Workload* pWorkload;

	bool parameters_are_valid=false;  // this will be set by setFrom_IosequencerInput()

	IosequencerInput* p_IosequencerInput;

	std::hash<std::string> string_hash;

	std::string threadKey;

	ivytime previous_scheduled_time = ivytime(0);

	long long int coverageStartLBA, coverageEndLBA, numberOfCoverageLBAs;
		// These are calculated from the LUN's maxLBA attribute,
		// and then modified according to the by setFromIosequencerInput() parameters
		// RangeStart=0.0 (range from 0.0 to 1.0), RangeEnd,
		// and lastIO_LBA is set according to SeqStartPoint.
		// (The lastIO_LBA matters nothing to random_steady and random_independent, because
		// the lba for the next I/O is calculated without reference to the LBA for the previous I/O.)

		// Derived iosequencer instances need to call the base class setFrom_IosequencerInput() function
		// before performing any other setting from IosequencerInput parameter values.

		// LBA 0 (zero) is not eligible to perform I/O to.
		// The default is the maximum permitted coverage of from LBA 1 to the maxLBA of the LUN.

		// These values are set by iosequencer::setFrom_IosequencerInput().

	long long int coverageStartBlock, coverageEndBlock, numberOfCoverageBlocks;
		// iosequencers are permitted to perform I/O anywhere from coverageStartLBA to coverageEndLBA and perform I/O on any alignment boundary.

		// However, for the convenience of those iosequencers that want to perform I/O
		// with a fixed blocksize (the IosequencerInput parameter "blocksize_bytes" value),
		// the setFrom_IosequencerInput() function sets these coverageStartBlock and coverageEndBlock values
		// which are in multiples of a "blocksize_bytes"-sized blocks aligned on blocksize_bytes boundaries.

		// Of course, the blocksize_bytes value itself must be a multiple of the LUN sector size. (512 bytes, 4096 bytes)

		// These values are set by iosequencer::setFrom_IosequencerInput().

    uint64_t number_of_IOs_generated{0}; // A fresh Iosequencer object is created for each "go".  Don't need to further initialize.

//methods:

	Iosequencer(LUN* pL, std::string lf, std::string wID, WorkloadThread* pWT, TestLUN* p_tl, Workload* p_w)
		: pLUN(pL), logfilename(lf), workloadID(wID), pWorkloadThread(pWT), pTestLUN(p_tl), pWorkload(p_w) {}

	virtual bool generate(Eyeo& e);

	virtual bool isRandom()=0;  // This is used to plug the I/O statistics into "random" and "sequential" categories.

	virtual bool setFrom_IosequencerInput(IosequencerInput*);  // This base class function should be called first
		// by every eponymous derived class function.

	virtual std::string instanceType()=0;
		// Returns "random_steady" or "random independent", or "sequential",
		// or in future things like trace playback or use of advanced statistical generators.

    virtual ~Iosequencer() { std::ostringstream o; o << "^^^^^^^debug ~Iosequencer() at this = " << this; log(pWorkloadThread->slavethreadlogfile, o.str()); }
};


