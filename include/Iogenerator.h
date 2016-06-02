//Copyright (c) 2016 Hitachi Data Systems, Inc.
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
//Author: Allart Ian Vogelesang <ian.vogelesang@hds.com>
//
//Support:  "ivy" is not officially supported by Hitachi Data Systems.
//          Contact me (Ian) by email at ian.vogelesang@hds.com and as time permits, I'll help on a best efforts basis.
#pragma once

#include "iogenerator_stuff.h"
#include "Eyeo.h"
#include "IogeneratorInput.h"
#include "ivytime.h"
#include "LUN.h"

class Iogenerator
{

public:
	LUN* pLUN;
	std::string logfilename;
	WorkloadID workloadID;
	iogenerator_stuff* p_iogenerator_stuff;
	WorkloadThread* p_WorkloadThread;

	bool parameters_are_valid=false;  // this will be set by setFrom_IogeneratorInput()

	IogeneratorInput* p_IogeneratorInput;

	std::hash<std::string> string_hash;

	std::string threadKey;

	ivytime previous_scheduled_time = ivytime(0);

	long long int coverageStartLBA, coverageEndLBA, numberOfCoverageLBAs;
		// These are calculated from the LUN's maxLBA attribute,
		// and then modified according to the by setFromIogeneratorInput() parameters
		// VolumeCoverageStartFraction=0.0 (range from 0.0 to 1.0), VolumeCoverageEndFraction,
		// and lastIO_LBA is set according to SeqStartingPointFractionOfCoveredArea.
		// (The lastIO_LBA matters nothing to random_steady and random_independent, because
		// the lba for the next I/O is calculated without reference to the LBA for the previous I/O.)

		// Derived iogenerator instances need to call the base class setFrom_IogeneratorInput() function
		// before performing any other setting from IogeneratorInput parameter values.

		// LBA 0 (zero) is not eligible to perform I/O to.
		// The default is the maximum permitted coverage of from LBA 1 to the maxLBA of the LUN.

		// These values are set by iogenerator::setFrom_IogeneratorInput().

	long long int coverageStartBlock, coverageEndBlock, numberOfCoverageBlocks;
		// iogenerators are permitted to perform I/O anywhere from coverageStartLBA to coverageEndLBA and perform I/O on any alignment boundary.

		// However, for the convenience of those iogenerators that want to perform I/O
		// with a fixed blocksize (the IogeneratorInput parameter "blocksize_bytes" value),
		// the setFrom_IogeneratorInput() function sets these coverageStartBlock and coverageEndBlock values
		// which are in multiples of a "blocksize_bytes"-sized blocks aligned on blocksize_bytes boundaries.

		// Of course, the blocksize_bytes value itself must be a multiple of the LUN sector size. (512 bytes, 4096 bytes)

		// These values are set by iogenerator::setFrom_IogeneratorInput().

//methods:

	Iogenerator(LUN* pL, std::string lf, std::string wID, iogenerator_stuff* p_is, WorkloadThread* pWT)
		: pLUN(pL), logfilename(lf), workloadID(wID), p_iogenerator_stuff(p_is), p_WorkloadThread(pWT) {}

	virtual bool generate(Eyeo&)=0;
	virtual bool isRandom()=0;  // This is used to plug the I/O statistics into "random" and "sequential" categories.

	virtual bool setFrom_IogeneratorInput(IogeneratorInput*);  // This base class function should be called first
		// by every eponymous derived class function.

	virtual std::string instanceType()=0;
		// Returns "random_steady" or "random independent", or "sequential",
		// or in future things like trace playback or use of advanced statistical generators.

};


