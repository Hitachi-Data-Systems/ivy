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
using namespace std;

#include <string>
#include <random>
#include <fcntl.h>  // for open64, O_RDWR etc.
#include <functional> // for std::hash
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <list>
#include <map>
#include <algorithm> // for ivyhelpers.h
#include <linux/aio_abi.h>
#include <iostream>
#include <sstream>

#include "ivyhelpers.h"
#include "ivytime.h"
#include "ivydefines.h"
#include "iogenerator_stuff.h"
#include "IogeneratorInput.h"
#include "LUN.h"
#include "Eyeo.h"
#include "WorkloadID.h"
#include "Iogenerator.h"
#include "IogeneratorRandom.h"
#include "IogeneratorRandomIndependent.h"


//
//	The probability distribution of the time between successive I/O operations for the random, independent
//	case where each I/O arrives at a random time independent of the time of all other I/O operations, but
//	where the average number of I/Os per second is given by "RATE" in units of "per second" is an exponential distribution:
//
//	                - RATE * T
//	P(T) = RATE * E
//
//	The "expected value" (probability theory term) of this inter-arrival time is 1/RATE.
//
//	A random number generator is used to generate a floating point number "R" with an equiprobable distribution between 0.0 and 1.0.
//
//	"R" is then converted to an inter-arrival time "A" as follows:
//
//	         A
//	     R = S P(T) DT      ( "S" is the integration symbol )
//	         0
//
//	         A          - RATE * T
//	       = S RATE * E            DT
//	         0
//
//	     Solving for A:
//
//	     A = - LN(1 - R)/RATE
//
//	     This last formula is what we use to convert the random number R into an inter-arrival time interval.
//

bool IogeneratorRandomIndependent::generate(Eyeo& slang)
{
//*debug*/ log(logfilename,"IogeneratorRandomIndependent::generate() - entry.\n");

	if (NULL == p_uniform_real_distribution_0_to_1)
	{
		log(logfilename,std::string("IogeneratorRandomIndependent::generate() - p_uniform_real_distribution_0_to_1 was not initialized.\n"));
		return false;
	}

	if (!IogeneratorRandom::generate(slang))
		return false;
//*debug*/ log(logfilename,"IogeneratorRandomIndependent::generate() - IogeneratorRandom::generate() went OK.\n");

	if (-1 == p_IogeneratorInput->IOPS)
	{	// iorate=max
		slang.scheduled_time = ivytime(0);
	}
	else
	{
		if (ivytime(0) == previous_scheduled_time)
		{
			slang.scheduled_time.setToNow();
		}
		else
		{
			ivy_float R=0.0, inter_IO_arrival_time;
			while (R == 0.0 || R == 1.0) R = (*p_uniform_real_distribution_0_to_1)(deafrangen);  // The "while" was just in case we actually got 0.0 or 1.0

//*debug*/{ostringstream o; o << "IogeneratorRandomIndependent::generate() random number from zero to one R=" << R <<  "\n"; log(logfilename, o.str());}
			inter_IO_arrival_time = - log(1.0-R) / (p_IogeneratorInput->IOPS);

//*debug*/{ostringstream o; o << "IogeneratorRandomIndependent::generate() inter_IO_arrival_time=" << ivytime(inter_IO_arrival_time).format_as_duration_HMMSSns() <<  "\n"; log(logfilename, o.str());}
			slang.scheduled_time = previous_scheduled_time + ivytime(inter_IO_arrival_time);
		}
		previous_scheduled_time = slang.scheduled_time;

	}
//*debug*/ { ostringstream o; o << "IogeneratorRandomIndependent::generate() - scheduled_time=" << slang.scheduled_time.format_as_datetime_with_ns() << std::endl; log(logfilename,o.str());}

	return true;
}


