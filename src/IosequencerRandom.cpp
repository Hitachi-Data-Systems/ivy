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

#include "IosequencerRandom.h"

//#define IVYDRIVER_TRACE   // Defined here in this source file, so the CodeBlocks editor knows it's defined for code highlighting,
                           // and so you can turn it off and on for each source file.

bool IosequencerRandom::setFrom_IosequencerInput(IosequencerInput* p_i_i)
{
//*debug*/ log(logfilename,std::string("IosequencerRandom::setFrom_IosequencerInput() - entry.\n"));
#if defined(IVYDRIVER_TRACE)
    { static unsigned int callcount {0}; callcount++; if (callcount <= FIRST_FEW_CALLS) { std::ostringstream o; o << "(" << callcount << ") "; o << "Entering IosequencerRandom::setFrom_IosequencerInput() for " << workloadID << "."; log(pWorkloadThread->slavethreadlogfile,o.str()); } }
#endif

	if (!Iosequencer::setFrom_IosequencerInput(p_i_i))
		return false;

	return set_hot_zone_parameters(p_i_i);
}

bool IosequencerRandom::set_hot_zone_parameters (IosequencerInput *p_ii)
{
#if defined(IVYDRIVER_TRACE)
    { static unsigned int callcount {0}; callcount++; if (callcount <= FIRST_FEW_CALLS) { std::ostringstream o; o << "(" << callcount << ") "; o << "Entering IosequencerRandom::set_hot_zone_parameters() for " << workloadID << "."; log(pWorkloadThread->slavethreadlogfile,o.str()); } }
#endif

    if ( 0.0 == p_ii->hot_zone_read_fraction && 0.0 == p_ii->hot_zone_write_fraction )
    {
        hot_zone_coverageStartBlock = hot_zone_coverageEndBlock =   hot_zone_numberOfCoverageBlocks = (uint64_t) 0;

        other_zone_coverageStartBlock     = coverageStartBlock;
        other_zone_coverageEndBlock       = coverageEndBlock;
        other_zone_numberOfCoverageBlocks = numberOfCoverageBlocks;

        have_hot_zone = false;

        return true;
    }

    have_hot_zone = true;

    hot_zone_numberOfCoverageBlocks = ( (p_ii->hot_zone_size_bytes) + (((uint64_t)p_ii->blocksize_bytes)-1) ) / ((uint64_t)p_ii->blocksize_bytes); // rounding up

    if (hot_zone_numberOfCoverageBlocks >= numberOfCoverageBlocks)
    {
        std::ostringstream o;
        o << "<Error> hot_zone_size_bytes " << put_on_KiB_etc_suffix(p_ii->hot_zone_size_bytes)
            << " must be smaller than rangeStart to rangeEnd LUN coverage area " << put_on_KiB_etc_suffix(((uint64_t)p_ii->blocksize_bytes)*numberOfCoverageBlocks);
        log(logfilename, o.str());
        return false;
    }

    hot_zone_coverageStartBlock = coverageStartBlock;
    hot_zone_coverageEndBlock   = hot_zone_coverageStartBlock + hot_zone_numberOfCoverageBlocks - 1;

    other_zone_numberOfCoverageBlocks = numberOfCoverageBlocks - hot_zone_numberOfCoverageBlocks;

    other_zone_coverageStartBlock = hot_zone_coverageEndBlock + 1;
    other_zone_coverageEndBlock   = coverageEndBlock;

    return true;
}


bool IosequencerRandom::generate(Eyeo& slang)
{
#if defined(IVYDRIVER_TRACE)
    { static unsigned int callcount {0}; callcount++; if (callcount <= FIRST_FEW_CALLS) { std::ostringstream o; o << "(" << callcount << ") "; o << "Entering IosequencerRandom::generate() for " << workloadID << " - Eyeo = " << slang.toString(); log(pWorkloadThread->slavethreadlogfile,o.str()); } }
#endif

	Iosequencer::generate(slang); // Increments the I/O sequence number. Resets Eyeo for next I/O.  Always returns true.

	// This is the base class IosequencerRandom::generate() function that calculates the
	// random block number / LBA &

	// Then the derived class generate() function calculates the scheduled time of the I/O.


	// we assume that eyeocb.data already points to the Eyeo object
	// and that eyeocb.aio_buf already points to a page-aligned I/O buffer

	slang.eyeocb.aio_fildes = pTestLUN->fd;

	if (0.0 == p_IosequencerInput->fractionRead)
	{
		slang.eyeocb.aio_lio_opcode=IOCB_CMD_PWRITE;
	}
	else if (1.0 == p_IosequencerInput->fractionRead)
	{
		slang.eyeocb.aio_lio_opcode=IOCB_CMD_PREAD;
	}
	else
	{
		long double random_0_to_1 = generate_float_between_0_and_1();
		if ( random_0_to_1 <= p_IosequencerInput->fractionRead )
			slang.eyeocb.aio_lio_opcode=IOCB_CMD_PREAD;
		else
			slang.eyeocb.aio_lio_opcode=IOCB_CMD_PWRITE;
	}


	// generate random I/O block location

	uint64_t block_number;

	if (have_hot_zone)
	{
	    if (slang.eyeocb.aio_lio_opcode == IOCB_CMD_PREAD)
	    {
	        if (p_IosequencerInput->hot_zone_read_fraction >= 1.0)
	        {
	            block_number = generate_hot_zone_block_number();
	        }
	        else if (p_IosequencerInput->hot_zone_read_fraction <= 0.0)
	        {
	            block_number = generate_other_zone_block_number();
	        }
	        else
	        {
	            if (generate_float_between_0_and_1() <= p_IosequencerInput->hot_zone_read_fraction)
	            {
	                block_number =  generate_hot_zone_block_number();
	            }
	            else
	            {
	                block_number = generate_other_zone_block_number();
	            }
	        }
	    }
	    else // command is IOCB_CMD_PWRITE
	    {
	        if (p_IosequencerInput->hot_zone_write_fraction >= 1.0)
	        {
	            block_number = generate_hot_zone_block_number();
	        }
	        else if (p_IosequencerInput->hot_zone_write_fraction <= 0.0)
	        {
	            block_number = generate_other_zone_block_number();
	        }
	        else
	        {
	            if (generate_float_between_0_and_1() <= p_IosequencerInput->hot_zone_write_fraction)
	            {
	                block_number =  generate_hot_zone_block_number();
	            }
	            else
	            {
	                block_number = generate_other_zone_block_number();
	            }
	        }
	    }
	}
	else
	{
	    block_number = generate_other_zone_block_number();
	}

	slang.eyeocb.aio_offset = (p_IosequencerInput->blocksize_bytes) * block_number;
	// slang.eyeocb.aio_nbytes was set when I/Os were built for the Workload
	slang.eyeocb.aio_nbytes = p_IosequencerInput->blocksize_bytes;
	slang.scheduled_time=ivytime_zero;
	slang.start_time=0;
	slang.end_time=0;
	slang.return_value=-2;
	slang.errno_value=-2;

    if (slang.eyeocb.aio_lio_opcode == IOCB_CMD_PWRITE)
    {
        slang.generate_pattern();
    }

    if ((slang.eyeocb.aio_offset + p_IosequencerInput->blocksize_bytes) > pTestLUN->LUN_size_bytes)
    {
        std::ostringstream o;
        o << "<Error> Internal programming error - IosequencerRandom::generate() - I/O offset from beginning of LUN = " << slang.eyeocb.aio_offset
            << " plus blocksize = " << p_IosequencerInput->blocksize_bytes << " is past the end of the LUN, whose size in bytes is " << pTestLUN->LUN_size_bytes << std::endl
            << "TestLUN members - maxLBA = " << pTestLUN->maxLBA
            << ", LUN_size_bytes = " << pTestLUN->LUN_size_bytes
            << ", sector_size = " << pTestLUN->sector_size
            << ", launch_count = " << pTestLUN->launch_count
            << std::endl
            << "IosequencerRandom - hot_zone_coverageStartBlock = " << hot_zone_coverageStartBlock
            << ", hot_zone_coverageEndBlock = " << hot_zone_coverageEndBlock
            << ", hot_zone_numberOfCoverageBlocks = " << hot_zone_numberOfCoverageBlocks
            << std::endl
            << "Iosequencer - coverageStartLBA = " << coverageStartLBA
            << ", coverageEndLBA = " << coverageEndLBA
            << ", numberOfCoverageLBAs = " << numberOfCoverageLBAs
            << ", coverageStartBlock = " << coverageStartBlock
            << ", coverageEndBlock = " << coverageEndBlock
            << ", numberOfCoverageBlocks = " << numberOfCoverageBlocks
            << std::endl;
        log(logfilename,o.str());
        std::cout << o.str();
        exit(-1);
    }

	return true;
}





























