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

void IosequencerRandom::setFrom_IosequencerInput(IosequencerInput* p_i_i)
{
	Iosequencer::setFrom_IosequencerInput(p_i_i);

	ivytime semence;
	semence.setToNow();

    deafrangen.seed(string_hash(threadKey + semence.format_as_datetime_with_ns()));

	if (nullptr != p_other_area_block_distribution)    delete p_other_area_block_distribution;
	if (nullptr != p_hot_zone_block_distribution)      delete p_hot_zone_block_distribution;
	if (nullptr != p_uniform_real_distribution_0_to_1) delete p_uniform_real_distribution_0_to_1;

    if ( 0.0 == p_i_i->hot_zone_read_fraction && 0.0 == p_i_i->hot_zone_write_fraction )
    {
        have_hot_zone = false;

        hot_zone_coverageStartBlock = hot_zone_coverageEndBlock = hot_zone_numberOfCoverageBlocks = (uint64_t) 0;

        other_zone_coverageStartBlock     = coverageStartBlock;
        other_zone_coverageEndBlock       = coverageEndBlock;
        other_zone_numberOfCoverageBlocks = numberOfCoverageBlocks;
    }
    else
    {
        have_hot_zone = true;

        hot_zone_numberOfCoverageBlocks = ( (p_i_i->hot_zone_size_bytes) + (((uint64_t)p_i_i->blocksize_bytes)-1) ) / ((uint64_t)p_i_i->blocksize_bytes); // rounding up

        if (hot_zone_numberOfCoverageBlocks >= numberOfCoverageBlocks)
        {
            std::ostringstream o;
            o << "<Error> hot_zone_size_bytes " << put_on_KiB_etc_suffix(p_i_i->hot_zone_size_bytes)
                << " must be smaller than rangeStart to rangeEnd LUN coverage area " << put_on_KiB_etc_suffix(((uint64_t)p_i_i->blocksize_bytes)*numberOfCoverageBlocks);
            log(logfilename, o.str());
            throw std::runtime_error(o.str());
        }

        hot_zone_coverageStartBlock = coverageStartBlock;
        hot_zone_coverageEndBlock   = hot_zone_coverageStartBlock + hot_zone_numberOfCoverageBlocks - 1;

        other_zone_numberOfCoverageBlocks = numberOfCoverageBlocks - hot_zone_numberOfCoverageBlocks;

        other_zone_coverageStartBlock = hot_zone_coverageEndBlock + 1;
        other_zone_coverageEndBlock   = coverageEndBlock;
    }

	if (have_hot_zone) { p_hot_zone_block_distribution = new std::uniform_int_distribution<uint64_t> (hot_zone_coverageStartBlock,hot_zone_coverageEndBlock);}

	p_other_area_block_distribution = new std::uniform_int_distribution<uint64_t> (other_zone_coverageStartBlock,other_zone_coverageEndBlock);

	p_uniform_real_distribution_0_to_1 = new std::uniform_real_distribution<ivy_float>(0.0,1.0);  // for % reads and for random_independent, for inter-I/O arrival times.
}



bool IosequencerRandom::generate(Eyeo& slang)
{
	Iosequencer::generate(slang); // Increments the I/O sequence number. Resets Eyeo for next I/O.  Always returns true.

	// This is the base class IosequencerRandom::generate() function that calculates the
	// random block number / LBA &

	// Then the derived class generate() function calculates the scheduled time of the I/O.


	// we assume that eyeocb.data already points to the Eyeo object
	// and that eyeocb.aio_buf already points to a page-aligned I/O buffer

    if (nullptr == p_uniform_real_distribution_0_to_1) {throw std::runtime_error("<Error> internal programming error - null pointer to uniform_real_distribution_0_to_1");}
    if (nullptr == p_other_area_block_distribution) {throw std::runtime_error("<Error> internal programming error - null pointer to other zone block distribution generator");}
    if (have_hot_zone && nullptr == p_hot_zone_block_distribution) {throw std::runtime_error("<Error> internal programming error - null pointer to hot zone block distribution generator");}

	if (0.0 == p_IosequencerInput->fractionRead)
	{
		slang.sqe.opcode=IORING_OP_WRITE_FIXED;
	}
	else if (1.0 == p_IosequencerInput->fractionRead)
	{
		slang.sqe.opcode=IORING_OP_READ_FIXED;
	}
	else
	{
		if ( (*p_uniform_real_distribution_0_to_1)(deafrangen) <= p_IosequencerInput->fractionRead )
            slang.sqe.opcode=IORING_OP_READ_FIXED;
		else
            slang.sqe.opcode=IORING_OP_WRITE_FIXED;
	}


	// generate random I/O block location

	uint64_t block_number;

	if (have_hot_zone)
	{
	    if (slang.sqe.opcode == IORING_OP_READ_FIXED)
	    {
	        if (p_IosequencerInput->hot_zone_read_fraction >= 1.0)
	        {
	            block_number = (*p_hot_zone_block_distribution)(deafrangen);
	        }
	        else if (p_IosequencerInput->hot_zone_read_fraction <= 0.0)
	        {
	            block_number = (*p_other_area_block_distribution)(deafrangen);
	        }
	        else
	        {
	            if ((*p_uniform_real_distribution_0_to_1)(deafrangen) <= p_IosequencerInput->hot_zone_read_fraction)
	            {
	                block_number = (*p_hot_zone_block_distribution)(deafrangen);
	            }
	            else
	            {
	                block_number = (*p_other_area_block_distribution)(deafrangen);
	            }
	        }
	    }
	    else // command is IOCB_CMD_PWRITE
	    {
	        if (p_IosequencerInput->hot_zone_write_fraction >= 1.0)
	        {
	            block_number = (*p_hot_zone_block_distribution)(deafrangen);
	        }
	        else if (p_IosequencerInput->hot_zone_write_fraction <= 0.0)
	        {
	            block_number = (*p_other_area_block_distribution)(deafrangen);
	        }
	        else
	        {
	            if ((*p_uniform_real_distribution_0_to_1)(deafrangen) <= p_IosequencerInput->hot_zone_write_fraction)
	            {
	               block_number = (*p_hot_zone_block_distribution)(deafrangen);
	            }
	            else
	            {
	                block_number = (*p_other_area_block_distribution)(deafrangen);
	            }
	        }
	    }
	}
	else
	{
	    block_number = (*p_other_area_block_distribution)(deafrangen);
	}

	slang.sqe.off = (p_IosequencerInput->blocksize_bytes) * block_number;
	// slang.sqe.len was set when I/Os were built for the Workload
	slang.scheduled_time=ivytime_zero;
	slang.start_time=0;
	slang.end_time=0;
	slang.return_value=-2;

    if (slang.sqe.opcode == IORING_OP_WRITE_FIXED)
    {
        slang.generate_pattern();
    }

    if ((slang.sqe.off + p_IosequencerInput->blocksize_bytes) > pTestLUN->LUN_size_bytes)
    {
        std::ostringstream o;
        o << "<Error> Internal programming error - IosequencerRandom::generate() - I/O offset from beginning of LUN = " << slang.sqe.off
            << " plus blocksize = " << p_IosequencerInput->blocksize_bytes << " is past the end of the LUN, whose size in bytes is " << pTestLUN->LUN_size_bytes << std::endl
            << "TestLUN members - maxLBA = " << pTestLUN->maxLBA
            << ", LUN_size_bytes = " << pTestLUN->LUN_size_bytes
            << ", sector_size = " << pTestLUN->sector_size
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





























