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
#include "IosequencerInput.h"
#include "LUN.h"
#include "Eyeo.h"
#include "WorkloadID.h"
#include "Iosequencer.h"
#include "TestLUN.h"

extern bool routine_logging;

std::default_random_engine deafrangen;

bool Iosequencer::generate(Eyeo& e)
{
    e.resetForNextIO();

    number_of_IOs_generated++;

    e.io_sequence_number = number_of_IOs_generated;

    return true;
}


bool Iosequencer::setFrom_IosequencerInput(IosequencerInput* p_i_i)
{
	p_IosequencerInput = p_i_i;

	// set the first/last LBAs and "blocksize_bytes" block numbers.

	// This base class function should be called first from every eponymous derived class functions;

	long long int number_of_potential_coverage_sectors = (pTestLUN->LUN_size_bytes / pTestLUN->sector_size) - 1;
	    // i.e. maxLBA which is the number of sectors not including sector 0.

	long long int number_of_potential_coverage_blocks = (pTestLUN->LUN_size_bytes / p_IosequencerInput->blocksize_bytes) - 1;

    switch (p_IosequencerInput->rangeStartType)
    {
        case rangeType::fraction:
            coverageStartLBA   = 1 + p_IosequencerInput->rangeStart*number_of_potential_coverage_sectors;
	        coverageStartBlock = 1 + p_IosequencerInput->rangeStart*number_of_potential_coverage_blocks;
            break;
        case rangeType::percent:
            coverageStartLBA   = 1 + (p_IosequencerInput->rangeStart/100.0)*number_of_potential_coverage_sectors;
            coverageStartBlock = 1 + (p_IosequencerInput->rangeStart/100.0)*number_of_potential_coverage_blocks;
            break;
        case rangeType::MB:
            coverageStartLBA   = 1 + ((p_IosequencerInput->rangeStart * 1E6)/pTestLUN->sector_size);
            coverageStartBlock = 1 + ((p_IosequencerInput->rangeStart * 1E6)/p_IosequencerInput->blocksize_bytes);
            break;
        case rangeType::MiB:
            coverageStartLBA   = 1 + ((p_IosequencerInput->rangeStart * 1024* 1024)/pTestLUN->sector_size);
            coverageStartBlock = 1 + ((p_IosequencerInput->rangeStart * 1024* 1024)/p_IosequencerInput->blocksize_bytes);
            break;
        case rangeType::GB:
            coverageStartLBA   = 1 + ((p_IosequencerInput->rangeStart * 1E9)/pTestLUN->sector_size);
            coverageStartBlock = 1 + ((p_IosequencerInput->rangeStart * 1E9)/p_IosequencerInput->blocksize_bytes);
            break;
        case rangeType::GiB:
            coverageStartLBA   = 1 + ((p_IosequencerInput->rangeStart * 1024 * 1024 * 1024)/pTestLUN->sector_size);
            coverageStartBlock = 1 + ((p_IosequencerInput->rangeStart * 1024 * 1024 * 1024)/p_IosequencerInput->blocksize_bytes);
            break;
        case rangeType::TB:
            coverageStartLBA   = 1 + ((p_IosequencerInput->rangeStart * 1E12)/pTestLUN->sector_size);
            coverageStartBlock = 1 + ((p_IosequencerInput->rangeStart * 1E12)/p_IosequencerInput->blocksize_bytes);
            break;
        case rangeType::TiB:
            coverageStartLBA   = 1 + ((p_IosequencerInput->rangeStart * 1024 * 1024 * 1024 * 1024)/pTestLUN->sector_size);
            coverageStartBlock = 1 + ((p_IosequencerInput->rangeStart * 1024 * 1024 * 1024 * 1024)/p_IosequencerInput->blocksize_bytes);
            break;
        default:
        {
            std::ostringstream o;
            o << "<Error> invalid RangeStart value \"" << p_IosequencerInput -> rangeStartValue()
                << "\".  Error occurred in iosequencer for workload " << pWorkload->workloadID;
            throw std::runtime_error(o.str());
        }
    }

    if (coverageStartLBA > number_of_potential_coverage_sectors || coverageStartBlock > number_of_potential_coverage_blocks)
    {
        std::ostringstream o;
        o << "<Error> invalid RangeStart value \"" << p_IosequencerInput -> rangeStartValue()
            << "\" - this is beyond the end of the LUN.  Error occurred in iosequencer for workload " << pWorkload->workloadID;
        throw std::runtime_error(o.str());
    }

    switch (p_IosequencerInput->rangeEndType)
    {
        case rangeType::fraction:
            coverageEndLBA   = p_IosequencerInput->rangeEnd*number_of_potential_coverage_sectors;
            coverageEndBlock = p_IosequencerInput->rangeEnd*number_of_potential_coverage_blocks;
            break;
        case rangeType::percent:
            coverageEndLBA   = (p_IosequencerInput->rangeEnd/100.0)*number_of_potential_coverage_sectors;
            coverageEndBlock = (p_IosequencerInput->rangeEnd/100.0)*number_of_potential_coverage_blocks;
            break;
        case rangeType::MB:
            coverageEndLBA   = ((p_IosequencerInput->rangeEnd * 1E6)/pTestLUN->sector_size) - 1;
            coverageEndBlock = ((p_IosequencerInput->rangeEnd * 1E6)/p_IosequencerInput->blocksize_bytes) - 1;
            break;
        case rangeType::MiB:
            coverageEndLBA   = ((p_IosequencerInput->rangeEnd * 1024* 1024)/pTestLUN->sector_size) - 1;
            coverageEndBlock = ((p_IosequencerInput->rangeEnd * 1024* 1024)/p_IosequencerInput->blocksize_bytes) - 1;
            break;
        case rangeType::GB:
            coverageEndLBA   = ((p_IosequencerInput->rangeEnd * 1E9)/pTestLUN->sector_size ) - 1;
            coverageEndBlock = ((p_IosequencerInput->rangeEnd * 1E9)/p_IosequencerInput->blocksize_bytes) - 1;
            break;
        case rangeType::GiB:
            coverageEndLBA   = ((p_IosequencerInput->rangeEnd * 1024 * 1024 * 1024)/pTestLUN->sector_size) - 1;
            coverageEndBlock = ((p_IosequencerInput->rangeEnd * 1024 * 1024 * 1024)/p_IosequencerInput->blocksize_bytes) - 1;
            break;
        case rangeType::TB:
            coverageEndLBA   = ((p_IosequencerInput->rangeEnd * 1E12)/pTestLUN->sector_size) - 1;
            coverageEndBlock = ((p_IosequencerInput->rangeEnd * 1E12)/p_IosequencerInput->blocksize_bytes) - 1;
            break;
        case rangeType::TiB:
            coverageEndLBA   = ((p_IosequencerInput->rangeEnd * 1024 * 1024 * 1024 * 1024)/pTestLUN->sector_size) - 1;
            coverageEndBlock = ((p_IosequencerInput->rangeEnd * 1024 * 1024 * 1024 * 1024)/p_IosequencerInput->blocksize_bytes) - 1;
            break;
        default:
        {
            std::ostringstream o;
            o << "<Error> invalid RangeEnd value \"" << p_IosequencerInput -> rangeEndValue() << "\"."
                << "  Error occurred in iosequencer for workload " << pWorkload->workloadID;
            throw std::runtime_error(o.str());
        }
    }

    if (coverageEndLBA > number_of_potential_coverage_sectors || coverageEndBlock > number_of_potential_coverage_blocks)
    {
        std::ostringstream o;
        o << "<Error> invalid RangeEnd value \"" << p_IosequencerInput -> rangeEndValue() << "\"."
            << " - this is beyond the end of the LUN.  Error occurred in iosequencer for workload " << pWorkload->workloadID;
        throw std::runtime_error(o.str());
    }

    if (coverageStartLBA >= coverageEndLBA || coverageStartBlock >= coverageEndBlock)
    {
        std::ostringstream o;
        o << "<Error> invalid RangeStart value \"" << p_IosequencerInput -> rangeStartValue()
            << " yielding range starting block " << coverageStartBlock
            << " with RangeEnd value \"" << p_IosequencerInput -> rangeEndValue()
            << " yielding range ending block " << coverageEndBlock
            << " - range start must be before range end."
            << "  Error occurred in iosequencer for workload " << pWorkload->workloadID;
        throw std::runtime_error(o.str());
    }

	numberOfCoverageLBAs = 1 + coverageEndLBA - coverageStartLBA;

	if (0 != (p_IosequencerInput->blocksize_bytes % pTestLUN->sector_size))
	{
		parameters_are_valid=false;
		ostringstream o;
		o << "<Error> Iosequencer::setFrom_IosequencerInput() - blocksize = "
		    << p_IosequencerInput->blocksize_bytes << " is not a multiple of the sector size = "
		    << pTestLUN->sector_size << "."
            << "  Error occurred in iosequencer for workload " << pWorkload->workloadID;
        throw std::runtime_error(o.str());
	}

	numberOfCoverageBlocks = 1 + coverageEndBlock - coverageStartBlock;

	previous_scheduled_time = ivytime(0);

    if (routine_logging)
    {
        ostringstream o; o << "Iosequencer::setFrom_IosequencerInput() - LUN_size_bytes = " << pTestLUN->LUN_size_bytes
            << ", sector_size = " << pTestLUN->sector_size
            << std::endl << ", number_of_potential_coverage_sectors = " << number_of_potential_coverage_sectors
            << std::endl << ", coverageStartLBA = " << coverageStartLBA << ", coverageEndLBA = " << coverageEndLBA<< ", numberOfCoverageLBAs = " << numberOfCoverageLBAs
            << std::endl << ", p_IosequencerInput->blocksize_bytes = " << p_IosequencerInput->blocksize_bytes
            << std::endl << ", coverageStartBlock = " << coverageStartBlock << ", coverageEndBlock = " << coverageEndBlock<< ", numberOfCoverageBlocks = " << numberOfCoverageBlocks;
        log(logfilename,o.str());
    }

	return true;

	// Note that the random_steady and random_independent iosequencers don't override this setFrom_IosequencerInput() function.
}


