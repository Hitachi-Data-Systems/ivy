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

using namespace std;

#include "ivytypes.h"
#include "ivytime.h"
#include "IosequencerRandomIndependent.h"

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

bool IosequencerRandomIndependent::generate(Eyeo& slang)
{
	if (!IosequencerRandom::generate(slang))
		return false;

	if (-1 == p_IosequencerInput->IOPS)
	{	// iorate=max
		slang.scheduled_time = ivytime_zero;
	}
	else
	{
		if (previous_scheduled_time.isZero())
		{
			slang.scheduled_time.setToNow();
		}
		else
		{
			long double R=0.0, inter_IO_arrival_time;
			while (R == 0.0 || R == 1.0) R = (*p_uniform_real_distribution_0_to_1)(deafrangen);  // The "while" was just in case we actually got 0.0 or 1.0

			inter_IO_arrival_time = - log(1.0-R) / (p_IosequencerInput->IOPS);

			slang.scheduled_time = previous_scheduled_time + ivytime(inter_IO_arrival_time);
		}
		previous_scheduled_time = slang.scheduled_time;

	}

	return true;
}


