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

#include <map>

#include "ivytime.h"
#include "ivydefines.h"

struct metric_value
{

//	The idea here is twofold:

//	1) Where a loop is used to iterate over a configuration, issuing a subsystem interface command to gather data,
//	   here we preserve the information about the time of data gather for each particular item of data.

//	2) With collection time inextricably tied to each metric value, it becomes trivial to compute
//	   for metrics in the form of cumulative event counters, the number of events per second on average
//	   between any two data points you have.

public:
	ivytime start;
	ivytime complete;
	std::string value;

//methods
	metric_value(){};
	metric_value(ivytime s, ivytime c, std::string v) : start(s), complete(c), value(v) {};
	metric_value(const metric_value& mv) { start=mv.start; complete=mv.complete; value=mv.value; }

	metric_value& operator=(const metric_value& mv) { start=mv.start; complete=mv.complete; value=mv.value; return *this;}

    std::string toString();


 	ivytime gather_start() const { return start; }
	ivytime gather_midpoint() const { return start + ivytime(0.5 * (ivytime(complete-start).getlongdoubleseconds())); }
	ivytime gather_complete() const { return complete; }

	std::string string_value() const {return value;}

	uint64_t uint64_t_value(std::string name_associated_with_value_for_error_message = std::string(""));
		// Throws std::invalid_argument if the original string value didn't represent a 64 bit unsigned integer value.

	ivy_float numeric_value(const std::string& name_associated_with_value_for_error_message);
		// Throws std::invalid_argument if the original string value didn't represent a floating point value.

		// The string representing the floating point value, that is, "value", if it has a percent sign '%' appended, the numeric value is divided by 100.

//	... catch (const std::invalid_argument& ia) {
//		std::cerr << "Invalid argument: " << ia.what() << std::endl;
//	}


// If we wanted to minimize memory space, a particular integer or floating point value instance would not keep the original source string.
// We use get() methods for starting/ending time so we could maybe just keep the data gather midpoint time.

// I was thinking that we may need a way to compactly store a binary blob received from subsystem control interface,
// and then use a data structure template to decode the contents at access time.  Come back to this later.

};


class GatherData
{
public:
	std::map
	<
        std::string, // Configuration element type name, e.g. LDEV
		std::map
		<
            std::string, // Configuration element instance, e.g. 00:FF
			std::map
			<
                std::string, // Metric, e.g. "total I/O count"
				metric_value
            >
        >
    >
    data;

//methods
	GatherData() {};
	metric_value& get(const std::string& element, const std::string& instance, const std::string& metric); // throws std::out_of_range
	friend std::ostream& operator<<(std::ostream&, const GatherData&);
	// {
	//    { element="LDEV", instance="33:FF", metric="read_IOPS", value="14567", start="2015-03-05 16:40:54.123456789", complete="2015-03-05 16:40:54.123456789" }
	//    ...
	// }
	void print_csv_file_set(std::string root_folder /* must already exist and be a directory */, std::string /* subfolder leaf name like "401034.config" */);
		// Used to print a one-time standalone gather (e.g. config gather) in a designated subfolder,
		// where there is a file for each element type, like LDEV or Port, and lines for each instance of that element type.

};

// When it's time to do a gather for a particular Subsystem, we start by adding an empty GatherData object to contain the data points for the new subinterval.

// Then we interate over the various kinds of resources and instances of those resources, posting commands to the subsystem control interface (ivy_cmddev).

// For example:

//ivymaster_: get WP for CLPR1
//ivy_cmddev: { element="CLPR", instance="CLPR1", data = { WP = "27%" } }

//ivymaster_: get CLPRdetail
//ivy_cmddev: {
//ivy_cmddev:  { element="CLPR", instance="CLPR0", data = { size_MB = "8192", WP_MB = "1234", WP_percent = "3%"
//ivy_cmddev:     , Sidefile_MB = "0", Sidefile_percent = "0", cache_usage_MB = "8100", cache_useage_percent = "90%"
//ivy_cmddev:     , HORC_asynch_MCU_sidefile_percent = "0%", HORC_asynch_RCU_sidefile_percent = "0%"
//ivy_cmddev:     , read_hit_counter = 87340862725408823 }},
//ivy_cmddev:  { element="CLPR", instance="CLPR2", data = { size_MB = "885645", WP_MB = "4444444", WP_percent = "63%"
//ivy_cmddev:     , Sidefile_MB = "0", Sidefile_percent = "0", cache_usage_MB = "8100", cache_useage_percent = "90%"
//ivy_cmddev:     , HORC_asynch_MCU_sidefile_percent = "0%", HORC_asynch_RCU_sidefile_percent = "0%"
//ivy_cmddev:     , read_hit_counter = 25408823873408627}}
//ivy_cmddev: }

// It's the pipe_driver_thread for the Subsystem / command device that issues the ivy_cmddev commands,
// and as it parses the above responses, it timestamps each metric value with starting/ending times for the call to ivy_cmd,
// and puts the timestamped metric_value_instance in the GatherData object for that subinterval.

// If data arrives for a particular element/instance/metric more than once, newer data overwrites older data.

