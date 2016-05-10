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

#include "Accumulators_by_io_type.h"
#include "RunningStat.h"

class SubintervalOutput {

public:
// variables
	union u_type {
		struct a_type {
			Accumulators_by_io_type
				bytes_transferred,
				response_time,		// Application-view response including any waiting for I/O to be issued.
				service_time;		// Time from when I/O starts running until it ends.
			RunningStat<ivy_float, ivy_int>
				presubmitqueuedepth,
				postsubmitqueuedepth,
				submitcount,
				preharvestqueuedepth,
				postharvestqueuedepth,
				harvestcount,
				putback;  // between submitcount and putback we may have enough, but we are not recording how many I/Os we try to submit;
		} a;
		RunningStat<ivy_float, ivy_int> accumulator_array[sizeof(a)/sizeof(RunningStat<ivy_float, ivy_int>)];
		u_type(){
			int n = sizeof(a)/sizeof(RunningStat<ivy_float,ivy_int>);
			for (int i=0; i<n; i++) accumulator_array[i].clear();
		}
		~u_type(){}
	} u;


// methods
	inline static int RunningStatCount() {return(sizeof(u)/sizeof(RunningStat<ivy_float,ivy_int>));}
	std::string toString();
	bool fromIstream(std::istream&);
	bool fromString(std::string&);
	void add(const SubintervalOutput&);
	void clear();
	static std::string csvTitles();
	std::string csvValues(ivy_float seconds);
	std::string thumbnail(ivy_float seconds);
	RunningStat<ivy_float,ivy_int> overall_service_time_RS();
	RunningStat<ivy_float,ivy_int> overall_bytes_transferred_RS();
};

