//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include "ivyhelpers.h"

class IogeneratorInput {

	// this is the object that gets sent from ivymaster

	// NOTE: Because this the object that the iogenerator refers while generating I/O,
	//       all values are in binary form for quickest access minimizing conversions or lookups.

public:
	std::string iogenerator_type; // random_steady, random_independent, sequential, ...
	bool iogeneratorIsSet{false};

// Look in ivydefines.h for the default values, gathered together in one spot

	unsigned int
		blocksize_bytes{blocksize_bytes_default},

		maxTags{maxTags_default};   // make sure someone is going to notice if they haven't set this.

		// Note that the maxTags value is set when creating the I/O context.
		// From then on, nothing examines the value of the maxTags parameter.
		// If you want to do a series where the maxtags changes from interval to interval,
		// delete and recreate the workloads each time with the new maxTags value.

	ivy_float

		IOPS {IOPS_default};  // Default is 1.0 I/Os per second
			// -1.0 means "drive I/Os as fast as possible up to the the "maxTags" limit or the underlying limit of the I/O system"
			// when using a text field saying "IOPS = + 10" means increase IOPS by 10.  saying "IOPS = -10.0" means decrease by 10.0.
			// saying "IOPS = +x .25" means increase IOPS by 25%, and "IOPS = -x25%" means reduce IOPS by 25%.

			// 0.0 means don't issue any new I/Os.
	ivy_float

		fractionRead{fractionRead},
			// For random can be any value from 0.0 to 1.0.  For sequential must be either 0.0 or 1.0.
		        // Actually, we could use sequential values between 0 and 1 to emulate pathological vdbench random ... later maybe

		volCoverageFractionStart {volCoverageFractionStart_default}, // default is start at sector 1.  Sector 0 is considered "out of bounds".
		volCoverageFractionEnd {volCoverageFractionEnd_default}; // default is 1.0 maps to the last aligned block of that blocksize that fits.

	ivy_float // Only sequential looks at this
		seqStartFractionOfCoverage{seqStartFractionOfCoverage_default};
			// This defines where a sequential thread will start mapped from 0.0
			// at the volCoverageFractionStart point up to 1.0 at the volCoverageFractionEnd point.

public:
	inline IogeneratorInput(){reset();}

	bool setParameter(std::string& callers_error_message, std::string parameterNameEqualsValue);
	bool setMultipleParameters(std::string& callers_error_message, std::string commaSeparatedList);
	std::string toStringFull();
	std::string toString();
	bool fromString(std::string, std::string logfilename);
	bool fromIstream(std::istream&, std::string logfile);
	std::string getParameterNameEqualsTextValueCommaSeparatedList();
	std::string getNonDefaultParameterNameEqualsTextValueCommaSeparatedList();
	void reset();
	void copy(const IogeneratorInput& source);

	bool defaultBlocksize() { return 4096 == blocksize_bytes; }
	bool defaultMaxTags() { return 1 == maxTags; }
	bool defaultIOPS() { return 1.0 == IOPS; }
	bool defaultFractionRead() { return  1.0==fractionRead; }
	bool defaultVolCoverageFractionStart() { return 0.0 == volCoverageFractionStart; }
	bool defaultVolCoverageFractionEnd() { return 0.0 == volCoverageFractionEnd; }
	bool defaultSeqStartFractionOfCoverage()
	{
		if (stringCaseInsensitiveEquality(std::string("sequential"),iogenerator_type))
		{
			return 0.0 == seqStartFractionOfCoverage;
		}
		else
		{
			return true;
		}
	}
};

