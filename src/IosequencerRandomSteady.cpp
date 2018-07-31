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
#include "iosequencer_stuff.h"
#include "IosequencerInput.h"
#include "LUN.h"
#include "Eyeo.h"
#include "WorkloadID.h"
#include "Iosequencer.h"
#include "IosequencerRandom.h"
#include "IosequencerRandomSteady.h"
#include "WorkloadThread.h"



bool IosequencerRandomSteady::generate(Eyeo& slang)
{
	if (!IosequencerRandom::generate(slang))
		return false;

	if (-1 == p_IosequencerInput->IOPS)
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
			ivy_float skew_factor;
			skew_factor = (!slang.pWorkloadThread->subinterval_array[0].input.skewed_workloads ?
                                                                      1.0 : slang.pWorkloadThread->skew_factor);

			slang.scheduled_time = previous_scheduled_time + ivytime(1/(skew_factor * p_IosequencerInput->IOPS));
		}
		previous_scheduled_time = slang.scheduled_time;
	}
//*debug*/ { ostringstream o; o << "IosequencerRandomSteady::generate() - IOPS = " << p_IosequencerInput->IOPS << ", scheduled_time = " << slang.scheduled_time.format_as_datetime_with_ns() << std::endl; log(logfilename,o.str());}
	return true;
}

