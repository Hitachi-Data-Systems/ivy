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

#include "Accumulators_by_io_type.h"
#include "RunningStat.h"

class SubintervalRollup;

class SubintervalOutput {

public:
// variables
	union u_type
	{
		struct a_type
		{
			Accumulators_by_io_type
				bytes_transferred,
				response_time,		// Time from the scheduled time to the end of the I/O.
				service_time,		// Time from just before AIO submit to start the I/O until it ends.
                submit_time,		// Time from just before submitting I/O to just after submitting I/O.  This includes "waiting for an underlying tag".
				running_time;		// Time from just after AIO submit to when the I/O ends

		} a;

		RunningStat<double, long int> accumulator_array[sizeof(a)/sizeof(RunningStat<double, long int>)];

		u_type()
		{
			int n = sizeof(a)/sizeof(RunningStat<double,long int>);
			for (int i=0; i<n; i++) accumulator_array[i].clear();
		}
		~u_type(){}
	} u;


// methods
	constexpr static unsigned int RunningStatCount() { return(sizeof(u)/sizeof(RunningStat<double,long int>)); }
	std::string toString() const;

	bool toBuffer(char* buffer, size_t buffer_size); // same as toString(), but faster.

	bool fromIstream(std::istream&);
	bool fromString(const std::string&);
	void add(const SubintervalOutput&);
	void clear();

    // When we print csv titles and csv values for each category (categories are overall, random, random read, etc.),
    // there is an extra column added only for the measurement rollup where we have a set of "samples", one for each
    // subinterval of average IOPS and average service time.
        // Not so much a set as a time series, but we apply a correction factor m_s.non_random_sample_correction_factor
        // to exaggerate local variation from subinterval to subinterval to allow for a kind of "fractal"
        // sense of how much variation there might be were subintervals randomly included out of a much larger population.
    // This extra column shows for each of IOPS and service time the estimated plus/minus accuracy.
    // Then when the measurement csv file line is printed, two overall "settings" values are shown:
    //  - m_s.non_random_sample_correction_factor
    //        * at present, this factor is hard-coded in ivy_engine.
    //  - +/- statistical confidence
    //        * at present, hard coded to a fixed value originally "95%"
    // Extra columns for measurement rollup csv columns are indicated by a pointer to a SubintervalRollup object.
	static std::string csvTitles(bool measurement_columns = false);
	std::string csvValues(ivy_float seconds, SubintervalRollup* = nullptr, ivy_float non_random_sample_correction_factor = non_random_sample_correction_factor_default);

	std::string thumbnail(ivy_float seconds);
	RunningStat<double,long int> overall_service_time_RS();
	RunningStat<double,long int> overall_submit_time_RS();
	RunningStat<double,long int> overall_running_time_RS();
	RunningStat<double,long int> overall_bytes_transferred_RS();
};

