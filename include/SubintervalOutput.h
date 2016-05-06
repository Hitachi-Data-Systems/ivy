//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
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

