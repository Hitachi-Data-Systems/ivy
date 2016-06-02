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
#include "IogeneratorSequential.h"
#include "WorkloadThread.h"

extern std::string printable_ascii;

bool IogeneratorSequential::setFrom_IogeneratorInput(IogeneratorInput* p_i_i)
{
//*debug*/ log(logfilename,std::string("IogeneratorSequential::setFrom_IogeneratorInput() - entry.\n"));
	if (!Iogenerator::setFrom_IogeneratorInput(p_i_i)) return false;

	lastIOblockNumber = coverageStartBlock + (long long int) (p_IogeneratorInput->seqStartFractionOfCoverage * (ivy_float) numberOfCoverageBlocks);
//*debug*/{ostringstream o; o << "IogeneratorSequential::setFrom_IogeneratorInput() - lastIOblocknumber has been set to " << lastIOblockNumber; log(logfilename,o.str());}
	return true;
}


bool IogeneratorSequential::generate(Eyeo& slang) {

	slang.resetForNextIO();

	// we assume that eyeocb.data already points to the Eyeo object
	// and that eyeocb.aio_buf already points to a page-aligned I/O buffer

	slang.eyeocb.aio_fildes = p_iogenerator_stuff->fd;

	lastIOblockNumber++;
	if (lastIOblockNumber >= coverageEndBlock) lastIOblockNumber = coverageStartBlock;
	slang.eyeocb.aio_offset = p_IogeneratorInput->blocksize_bytes * lastIOblockNumber;

	slang.eyeocb.aio_nbytes = p_IogeneratorInput->blocksize_bytes;
	slang.start_time=0;
	slang.end_time=0;
	slang.return_value=-2;
	slang.errno_value=-2;

	if (0.0 == p_IogeneratorInput->fractionRead)
	{
		slang.eyeocb.aio_lio_opcode=IOCB_CMD_PWRITE;
        slang.generate_pattern();
	}
	else if (1.0 == p_IogeneratorInput->fractionRead)
	{
		slang.eyeocb.aio_lio_opcode=IOCB_CMD_PREAD;
	}
	else
	{
		ostringstream o;
		o << "IogeneratorSequential::generate() - p_IogeneratorInput->fractionRead = " << p_IogeneratorInput->fractionRead <<", but it is only supposed to be either 0.0 or 1.0.";
		log(logfilename,o.str());
		return false;
	}

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
			slang.scheduled_time = previous_scheduled_time + ivytime(1/(p_IogeneratorInput->IOPS));
		}
		previous_scheduled_time = slang.scheduled_time;
	}

	return true;
}
