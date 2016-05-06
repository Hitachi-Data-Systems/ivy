//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
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

extern bool routine_logging;

bool Iogenerator::setFrom_IogeneratorInput(IogeneratorInput* p_i_i)
{
	p_IogeneratorInput = p_i_i;

	// set the first/last LBAs and "blocksize_bytes" block numbers.

	// This base class function should be called first from every eponymous derived class functions;

	long long int number_of_potential_coverage_sectors = (p_iogenerator_stuff->LUN_size_bytes / p_iogenerator_stuff->sector_size) - 1; // i.e. maxLBA which is the number of sectors not including sector 0.

	coverageStartLBA = 1 + p_IogeneratorInput->volCoverageFractionStart*number_of_potential_coverage_sectors;
	coverageEndLBA = 1 + p_IogeneratorInput->volCoverageFractionEnd*number_of_potential_coverage_sectors;
	numberOfCoverageLBAs = 1 + coverageEndLBA - coverageStartLBA;

	if (0 != (p_IogeneratorInput->blocksize_bytes % p_iogenerator_stuff->sector_size))
	{
		parameters_are_valid=false;
		ostringstream o;
		o << "Iogenerator::setFrom_IogeneratorInput() - blocksize = " << p_IogeneratorInput->blocksize_bytes << " is not a multiple of the sector size = " << p_iogenerator_stuff->sector_size << "." << std::endl;
		log(logfilename,o.str());
		return false;
	}
	long long int number_of_potential_coverage_blocks = (p_iogenerator_stuff->LUN_size_bytes / p_IogeneratorInput->blocksize_bytes) - 1;
	coverageStartBlock = 1 + p_IogeneratorInput->volCoverageFractionStart*number_of_potential_coverage_blocks;
	coverageEndBlock = 1 + p_IogeneratorInput->volCoverageFractionEnd*number_of_potential_coverage_blocks;
	numberOfCoverageBlocks = 1 + coverageEndBlock - coverageStartBlock;

	previous_scheduled_time = ivytime(0);

    if (routine_logging)
    {
        ostringstream o; o << "Iogenerator::setFrom_IogeneratorInput() - LUN_size_bytes = " << p_iogenerator_stuff->LUN_size_bytes
            << ", sector_size = " << p_iogenerator_stuff->sector_size
            << std::endl << ", number_of_potential_coverage_sectors = " << number_of_potential_coverage_sectors
            << std::endl << ", coverageStartLBA = " << coverageStartLBA << ", coverageEndLBA = " << coverageEndLBA<< ", numberOfCoverageLBAs = " << numberOfCoverageLBAs
            << std::endl << ", p_IogeneratorInput->blocksize_bytes = " << p_IogeneratorInput->blocksize_bytes
            << std::endl << ", coverageStartBlock = " << coverageStartBlock << ", coverageEndBlock = " << coverageEndBlock<< ", numberOfCoverageBlocks = " << numberOfCoverageBlocks;
        log(logfilename,o.str());
    }

	return true;

	// Note that the random_steady and random_independent iogenerators don't override this setFrom_IogeneratorInput() function.
}


