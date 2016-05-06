//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include "iogenerator_stuff.h"
#include "Eyeo.h"
#include "IogeneratorInput.h"
#include "ivytime.h"
#include "LUN.h"

class Iogenerator
{

protected:
	LUN* pLUN;
	WorkloadID workloadID;

    uint64_t generate_count=0;

public:
	Iogenerator(LUN* pL, std::string lf, std::string wID, iogenerator_stuff* p_is)
		: pLUN(pL), workloadID(wID), p_iogenerator_stuff(p_is), logfilename(lf) {}

	virtual bool generate(Eyeo&)=0;
	virtual bool isRandom()=0;  // This is used to plug the I/O statistics into "random" and "sequential" categories.

	bool parameters_are_valid=false;  // this will be set by setFrom_IogeneratorInput()

	IogeneratorInput* p_IogeneratorInput;

	iogenerator_stuff* p_iogenerator_stuff;

	std::string logfilename, threadKey;

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

	virtual bool setFrom_IogeneratorInput(IogeneratorInput*);  // This base class function should be called first
		// by every eponymous derived class function.

	virtual std::string instanceType()=0;
		// Returns "random_steady" or "random independent", or "sequential",
		// or in future things like trace playback or use of advanced statistical generators.
	std::hash<std::string> string_hash;

};


