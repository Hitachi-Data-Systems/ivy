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
#include "iosequencer_stuff.h"
#include "IosequencerInput.h"
#include "LUN.h"
#include "Eyeo.h"
#include "WorkloadID.h"
#include "Iosequencer.h"
#include "IosequencerRandom.h"
#include "WorkloadThread.h"



bool IosequencerRandom::setFrom_IosequencerInput(IosequencerInput* p_i_i)
{
//*debug*/ log(logfilename,std::string("IosequencerRandom::setFrom_IosequencerInput() - entry.\n"));
	if (!Iosequencer::setFrom_IosequencerInput(p_i_i))
		return false;

	ivytime semence;
	semence.setToNow();
       	deafrangen.seed(string_hash(threadKey + semence.format_as_datetime_with_ns()));
//*debug*/ {ostringstream o; o<< "//*debug*/ IosequencerRandom::setFrom_IosequencerInput() coverageStartBlock=" << coverageStartBlock << ", coverageEndBlock=" << coverageEndBlock << ".\n"; log(logfilename, o.str()); }

	if (NULL != p_uniform_int_distribution) delete p_uniform_int_distribution;
	if (NULL != p_uniform_real_distribution_0_to_1) delete p_uniform_real_distribution_0_to_1;
	p_uniform_int_distribution=new std::uniform_int_distribution<uint64_t> (coverageStartBlock,coverageEndBlock);
	p_uniform_real_distribution_0_to_1 = new std::uniform_real_distribution<ivy_float>(0.0,1.0);  // for % reads and for random_independent, for inter-I/O arrival times.

	return set_hot_zone_parameters(p_i_i);
}

bool IosequencerRandom::set_hot_zone_parameters (IosequencerInput *p_ii)
{
    if (((uint64_t)0) == p_ii->hot_zone_size_bytes)
    {
        hot_zone_coverageStartBlock =   hot_zone_coverageEndBlock =   hot_zone_numberOfCoverageBlocks = (uint64_t) 0;
        return true;
    }

    hot_zone_numberOfCoverageBlocks = ( (p_ii->hot_zone_size_bytes) + (((uint64_t)p_ii->blocksize_bytes)-1) ) / ((uint64_t)p_ii->blocksize_bytes);

    if (hot_zone_numberOfCoverageBlocks >= numberOfCoverageBlocks)
    {
        std::ostringstream o;
        o << "<Error> hot_zone_size_bytes " << put_on_KiB_etc_suffix(p_ii->hot_zone_size_bytes)
            << " is bigger than LUN coverage area " << put_on_KiB_etc_suffix(((uint64_t)p_ii->blocksize_bytes)*numberOfCoverageBlocks);
        log(logfilename, o.str());
        return false;
    }
    delete p_hot_zone_block_distribution;
    delete p_other_area_block_distribution;
    uint64_t hot_start = coverageStartBlock;
    uint64_t hot_end = coverageStartBlock+hot_zone_numberOfCoverageBlocks-1;
    uint64_t other_start = coverageStartBlock+hot_zone_numberOfCoverageBlocks;
    uint64_t other_end = coverageEndBlock;

    p_hot_zone_block_distribution = new std::uniform_int_distribution<uint64_t> (hot_start,hot_end);
    p_other_area_block_distribution = new std::uniform_int_distribution<uint64_t> (other_start, other_end);

    return true;
}


bool IosequencerRandom::generate(Eyeo& slang) {
	// This is the base class IosequencerRandom::generate() function that calculates the
	// random block number / LBA &

	// Then the derived class generate() function calculates the scheduled time of the I/O.

	slang.resetForNextIO();

	// we assume that eyeocb.data already points to the Eyeo object
	// and that eyeocb.aio_buf already points to a page-aligned I/O buffer

	slang.eyeocb.aio_fildes = p_iosequencer_stuff->fd;

	if (NULL == p_uniform_int_distribution)
	{
		log(logfilename, std::string("IosequencerRandom::generate() - p_uniform_int_distribution not initialized.\n"));
		return false;
	}

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
		ivy_float random_0_to_1 = (*p_uniform_real_distribution_0_to_1)(deafrangen);
		if ( random_0_to_1 <= p_IosequencerInput->fractionRead )
			slang.eyeocb.aio_lio_opcode=IOCB_CMD_PREAD;
		else
			slang.eyeocb.aio_lio_opcode=IOCB_CMD_PWRITE;
	}

    uint64_t current_block;

    if (p_IosequencerInput->hot_zone_size_bytes == 0)
    {
        if (nullptr == p_uniform_int_distribution)
        {
            log(logfilename, std::string("<Error> internal programming error - IosequencerRandom::generate() - p_uniform_int_distribution not initialized.\n"));
            return false;
        }
        current_block=(*p_uniform_int_distribution)(deafrangen);
    }
    else
    {
        if (nullptr == p_hot_zone_block_distribution)
        {
            log(logfilename, std::string("<Error> internal programming error - IosequencerRandom::generate() - p_hot_zone_block_distribution not initialized.\n"));
            return false;
        }

        if (nullptr == p_other_area_block_distribution)
        {
            log(logfilename, std::string("<Error> internal programming error - IosequencerRandom::generate() - p_other_area_block_distribution not initialized.\n"));
            return false;
        }

        ivy_float hot_zone_fraction;

        if (p_IosequencerInput->hot_zone_read_fraction == 0.0 && p_IosequencerInput->hot_zone_write_fraction == 0.0)
        {
            hot_zone_fraction = p_IosequencerInput->hot_zone_IOPS_fraction;
        }
        else
        {
            if (slang.eyeocb.aio_lio_opcode == IOCB_CMD_PREAD)
            {
                hot_zone_fraction = p_IosequencerInput->hot_zone_read_fraction;
            }
            else
            {
                hot_zone_fraction = p_IosequencerInput->hot_zone_write_fraction;
            }
        }

        if (hot_zone_fraction < 0.0 || hot_zone_fraction > 1.0)
        {
            std::ostringstream o;
                o << "<Error> internal programming error in function " << __FUNCTION__ << " at line " << __LINE__ << " of source file " << __FILE__
                << " - hot zone fraction " << hot_zone_fraction << " must be from 0.0 to 1.0 - "
                << "p_IosequencerInput->hot_zone_size_bytes = " << p_IosequencerInput->hot_zone_size_bytes
                << ", p_IosequencerInput->hot_zone_IOPS_fraction = " << p_IosequencerInput->hot_zone_IOPS_fraction
                << ", p_IosequencerInput->hot_zone_read_fraction = " << p_IosequencerInput->hot_zone_read_fraction
                << ", p_IosequencerInput->hot_zone_write_fraction = " << p_IosequencerInput->hot_zone_write_fraction
                << " for slang.eyeocb.aio_lio_opcode == ";
            if (slang.eyeocb.aio_lio_opcode == IOCB_CMD_PREAD) o << "IOCB_CMD_PREAD";
            else if (slang.eyeocb.aio_lio_opcode == IOCB_CMD_PWRITE) o << "IOCB_CMD_PWRITE";
            else o << "unrecognized I/O opcode " << slang.eyeocb.aio_lio_opcode;
            o << ", hot zone fraction = " << hot_zone_fraction << std::endl;
            log(logfilename,o.str());
            std::cout << o.str();
            exit(-1);
        }

             if (hot_zone_fraction == 1.0) { current_block=(*p_hot_zone_block_distribution)(deafrangen); }
        else if (hot_zone_fraction == 0.0) { current_block=(*p_other_area_block_distribution)(deafrangen); }
        else
        {
            ivy_float random_0_to_1 = (*p_uniform_real_distribution_0_to_1)(deafrangen);
            if ( random_0_to_1 <= hot_zone_fraction ) { current_block=(*p_hot_zone_block_distribution)(deafrangen); }
            else                                      { current_block=(*p_other_area_block_distribution)(deafrangen); }
        }
    }

	slang.eyeocb.aio_offset = (p_IosequencerInput->blocksize_bytes) * current_block;
	slang.eyeocb.aio_nbytes = p_IosequencerInput->blocksize_bytes;
	slang.scheduled_time=ivytime(0);
	slang.start_time=0;
	slang.end_time=0;
	slang.return_value=-2;
	slang.errno_value=-2;

    if (slang.eyeocb.aio_lio_opcode == IOCB_CMD_PWRITE)
    {
        slang.generate_pattern();
    }

	return true;
}


