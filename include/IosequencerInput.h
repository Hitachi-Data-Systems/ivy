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

#include "ivyhelpers.h"
#include "pattern.h"

class IosequencerInput {

	// this is the object that gets sent from ivymaster

	// NOTE: Because this the object that the iosequencer refers while generating I/O,
	//       all values are in binary form for quickest access minimizing conversions or lookups.

public:
	std::string iosequencer_type; // random_steady, random_independent, sequential, ...
	bool iosequencerIsSet{false};

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

		fractionRead{fractionRead_default},
			// For random can be any value from 0.0 to 1.0.  For sequential must be either 0.0 or 1.0.
		        // Actually, we could use sequential values between 0 and 1 to emulate pathological vdbench random ... later maybe

		volCoverageFractionStart {volCoverageFractionStart_default}, // default is start at sector 1.  Sector 0 is considered "out of bounds".
		volCoverageFractionEnd {volCoverageFractionEnd_default}; // default is 1.0 maps to the last aligned block of that blocksize that fits.

	ivy_float // Only sequential looks at this
		seqStartFractionOfCoverage{seqStartFractionOfCoverage_default};
			// This defines where a sequential thread will start mapped from 0.0
			// at the volCoverageFractionStart point up to 1.0 at the volCoverageFractionEnd point.

    ivy_float dedupe          {dedupe_default};
    pattern   pat             {pattern_default};
    ivy_float compressibility {compressibility_default};  // compressibility is only referred to for pattern = trailing_zeros

    unsigned int threads_in_workload_name {threads_in_workload_name_default};
    unsigned int this_thread_in_workload {this_thread_in_workload_default};
    uint64_t pattern_seed {pattern_seed_default};

    uint64_t hot_zone_size_bytes {0};
    ivy_float hot_zone_IOPS_fraction {0};
    ivy_float hot_zone_read_fraction {0};
    ivy_float hot_zone_write_fraction {0};

public:
	inline IosequencerInput(){reset();}

	std::pair<bool,std::string> setParameter(std::string parameterNameEqualsValue);
	std::pair<bool,std::string> setMultipleParameters(std::string commaSeparatedList);
	std::string toStringFull();
	std::string toString();
	bool fromString(std::string, std::string logfilename);
	bool fromIstream(std::istream&, std::string logfile);
	std::string getParameterNameEqualsTextValueCommaSeparatedList();
	std::string getNonDefaultParameterNameEqualsTextValueCommaSeparatedList();
	void reset();
	void copy(const IosequencerInput& source);

	bool defaultBlocksize() { return blocksize_bytes_default == blocksize_bytes; }
	bool defaultMaxTags() { return maxTags_default == maxTags; }
	bool defaultIOPS() { return IOPS_default == IOPS; }
	bool defaultFractionRead() { return  fractionRead_default==fractionRead; }
	bool defaultVolCoverageFractionStart() { return volCoverageFractionStart_default == volCoverageFractionStart; }
	bool defaultVolCoverageFractionEnd() { return volCoverageFractionEnd_default == volCoverageFractionEnd; }
	bool defaultSeqStartFractionOfCoverage()
	{
		if (stringCaseInsensitiveEquality(std::string("sequential"),iosequencer_type))
		{
			return seqStartFractionOfCoverage_default == seqStartFractionOfCoverage;
		}
		else
		{
			return true;
		}
	}
	bool defaultPattern() { return pat == pattern_default; }
	bool defaultDedupe() { return dedupe == dedupe_default; }
	bool defaultCompressibility() { return compressibility == compressibility_default; }
	bool defaultThreads_in_workload_name() { return threads_in_workload_name == threads_in_workload_name_default;}
	bool defaultThis_thread_in_workload() { return this_thread_in_workload == this_thread_in_workload_default;}
	bool defaultPattern_seed() { return pattern_seed == pattern_seed_default;}
};

