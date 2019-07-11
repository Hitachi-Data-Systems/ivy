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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#pragma once

#include "ivyhelpers.h"
#include "pattern.h"
#include "dedupe_method.h"

class IosequencerInput {

	// this is the object that gets sent from ivymaster

	// NOTE: Because this the object that the iosequencer refers while generating I/O,
	//       all values are in binary form for quickest access minimizing conversions or lookups.

public:
	std::string iosequencer_type; // random_steady, random_independent, sequential, ...
	bool iosequencerIsSet{false};

// Look in ivydefines.h for the default values, gathered together in one spot

	bool have_saved_IOPS{false};
    ivy_float saved_IOPS {0.0};
            // This is used with the cooldown_by_xxxx features.
            // The cooldown_by_xxx features set "IOPS=pause", which
            // saves the IOPS value, and then turns on have_saved_IOPS
            // and sets IOPS to 0.0.

            // run subinterval sequence(), before starting subinterval zero,
            // sends out "edit_rollup("all=all", "IOPS=restore")
            // and then if there is a saved value, it is restored.

            // If IOPS is set to any value other than "save" or "restore",
            // have_saved_IOPS is turned off.

	unsigned int
		blocksize_bytes{blocksize_bytes_default},

		maxTags{maxTags_default};   // make sure someone is going to notice if they haven't set this.

		// Note that the maxTags value is set when creating the I/O context.
		// From then on, nothing examines the value of the maxTags parameter.

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

		rangeStart {rangeStart_default}, // default is start at sector 1.  Sector 0 is considered "out of bounds".
		rangeEnd {rangeEnd_default}; // default is 1.0 maps to the last aligned block of that blocksize that fits.

	ivy_float // Only sequential looks at this
		seqStartPoint {seqStartPoint_default};
			// This defines where a sequential thread will start mapped from 0.0
			// at the rangeStart point up to 1.0 at the rangeEnd point.

    ivy_float dedupe          {dedupe_default};
    pattern   pat             {pattern_default};
    dedupe_method dedupe_type {dedupe_method_default};
    uint64_t dedupe_unit_bytes  {dedupe_unit_bytes_default};
    ivy_float compressibility {compressibility_default};  // compressibility is only referred to for pattern = trailing_blanks
    long double fraction_zero_pattern {0.0}; // long double has a 64 bit fractional part (mantissa) needed to hold (sub-)block

    unsigned int threads_in_workload_name {threads_in_workload_name_default};
    unsigned int this_thread_in_workload {this_thread_in_workload_default};
    uint64_t pattern_seed {pattern_seed_default};

    ivy_float skew_weight {skew_weight_default};

    uint64_t hot_zone_size_bytes {0};
    ivy_float hot_zone_IOPS_fraction {0};
    ivy_float hot_zone_read_fraction {0};
    ivy_float hot_zone_write_fraction {0};

public:
	inline IosequencerInput(){reset();}

private:
	std::pair<bool,std::string> setParameter(std::string parameterNameEqualsValue);
	    // This is made private, called only by setMultipleParameters(),
	    // so that in setMultipleParameters we can check that the
	    // set of parameter values is consistent, that is that there
	    // are no incompatible combinations of parameter settings,
	    // once all the individual parameter settings have been performed.

public:
	std::pair<bool,std::string> setMultipleParameters(std::string commaSeparatedList);
	std::string toStringFull();
	std::string toString();
	bool fromString(std::string, std::string logfilename);
	bool fromIstream(std::istream&, std::string logfile);
	std::string getParameterNameEqualsTextValueCommaSeparatedList();
	std::string getNonDefaultParameterNameEqualsTextValueCommaSeparatedList();
	void reset();
	void copy(const IosequencerInput& source);
	IosequencerInput(const IosequencerInput& source) { copy(source);}

	bool defaultBlocksize() { return blocksize_bytes_default == blocksize_bytes; }
	bool defaultMaxTags() { return maxTags_default == maxTags; }
	bool defaultIOPS() { return IOPS_default == IOPS; }
	bool defaultskew_weight() { return skew_weight_default == skew_weight; }
	bool defaultFractionRead() { return  fractionRead_default==fractionRead; }
	bool defaultRangeStart() { return rangeStart_default == rangeStart; }
	bool defaultRangeEnd() { return rangeEnd_default == rangeEnd; }
	bool defaultSeqStartPoint()
	{
		if (stringCaseInsensitiveEquality(std::string("sequential"),iosequencer_type))
		{
			return seqStartPoint_default == seqStartPoint;
		}
		else
		{
			return true;
		}
	}
	bool defaultPattern() { return pat == pattern_default; }
	bool defaultDedupeMethod() { return dedupe_type == dedupe_method_default; }
	bool defaultDedupe() { return dedupe == dedupe_default; }
	bool defaultCompressibility() { return compressibility == compressibility_default; }
	bool default_dedupe_unit() { return dedupe_unit_bytes == dedupe_unit_bytes_default; }
	bool default_fraction_zero_pattern() { return fraction_zero_pattern == 0.0; }
	bool defaultThreads_in_workload_name() { return threads_in_workload_name == threads_in_workload_name_default;}
	bool defaultThis_thread_in_workload() { return this_thread_in_workload == this_thread_in_workload_default;}
	bool defaultPattern_seed() { return pattern_seed == pattern_seed_default;}
};

