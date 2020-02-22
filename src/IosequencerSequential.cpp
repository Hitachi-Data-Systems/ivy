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
#include "IosequencerSequential.h"
#include "Workload.h"

extern std::string printable_ascii;
extern bool ivydriver_wrapping;

void IosequencerSequential::setFrom_IosequencerInput(IosequencerInput* p_i_i)
{
	Iosequencer::setFrom_IosequencerInput(p_i_i);

	lastIOblockNumber = -1 + coverageStartBlock + (long long int) (p_IosequencerInput->seqStartPoint * (ivy_float) numberOfCoverageBlocks);
        // The reason we are subtracting one is that this will be incremented before the first use.

    blocks_generated = 0;

    if (0.0 == p_IosequencerInput->fractionRead)
    {
        // we are writing
        pWorkload->sequential_fill_fraction = 0.0;
        wrapping = true;
    }
    else
    {
        // we are reading
        pWorkload->sequential_fill_fraction = 1.0;
        wrapping = false;
    }
	return;
}


bool IosequencerSequential::generate(Eyeo& slang)
{
	Iosequencer::generate(slang); // Increments the I/O sequence number. Resets Eyeo for next I/O.  Always returns true.

	// we assume that eyeocb.data already points to the Eyeo object
	// and that eyeocb.aio_buf already points to a page-aligned I/O buffer

	//slang.eyeocb.aio_fildes = pTestLUN->fd;

	lastIOblockNumber++;

    if (wrapping)
    {
        blocks_generated++;
        if (blocks_generated >= numberOfCoverageBlocks)
        {
            wrapping = false;
            pWorkload -> sequential_fill_fraction = 1.0;
        }
        else
        {
            pWorkload -> sequential_fill_fraction = ( (ivy_float) blocks_generated ) / ( (ivy_float) numberOfCoverageBlocks );
        }
    }

	if (lastIOblockNumber >= coverageEndBlock)
	{
        lastIOblockNumber = coverageStartBlock;   // this doesn't mean we have necessarily written to all blocks, as we may have started part way through.
	}
	slang.sqe.off = p_IosequencerInput->blocksize_bytes * lastIOblockNumber;

	//slang.eyeocb.aio_nbytes was set when Eyeos were built for the Workload.
	slang.start_time=0;
	slang.end_time=0;
	slang.return_value=-2;
//	slang.errno_value=-2;

	if (0.0 == p_IosequencerInput->fractionRead)
	{
		slang.sqe.opcode=IORING_OP_WRITE_FIXED;
        slang.generate_pattern();
	}
	else if (1.0 == p_IosequencerInput->fractionRead)
	{
		slang.sqe.opcode=IORING_OP_READ_FIXED;
	}
	else
	{
		ostringstream o;
		o << "IosequencerSequential::generate() - p_IosequencerInput->fractionRead = " << p_IosequencerInput->fractionRead <<", but it is only supposed to be either 0.0 or 1.0.";
		log(logfilename,o.str());
		return false;
	}

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
			slang.scheduled_time = previous_scheduled_time + ivytime(1/(p_IosequencerInput->IOPS));
		}
		previous_scheduled_time = slang.scheduled_time;
	}

	return true;
}
