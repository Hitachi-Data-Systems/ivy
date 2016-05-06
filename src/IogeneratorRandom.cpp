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
#include "IogeneratorRandom.h"



bool IogeneratorRandom::setFrom_IogeneratorInput(IogeneratorInput* p_i_i)
{
//*debug*/ log(logfilename,std::string("IogeneratorRandom::setFrom_IogeneratorInput() - entry.\n"));
	if (!Iogenerator::setFrom_IogeneratorInput(p_i_i))
		return false;

	ivytime semence;
	semence.setToNow();
       	deafrangen.seed(string_hash(threadKey + semence.format_as_datetime_with_ns()));
//*debug*/ {ostringstream o; o<< "//*debug*/ IogeneratorRandom::setFrom_IogeneratorInput() coverageStartBlock=" << coverageStartBlock << ", coverageEndBlock=" << coverageEndBlock << ".\n"; log(logfilename, o.str()); }

	if (NULL != p_uniform_int_distribution) delete p_uniform_int_distribution;
	if (NULL != p_uniform_real_distribution_0_to_1) delete p_uniform_real_distribution_0_to_1;
	p_uniform_int_distribution=new std::uniform_int_distribution<uint64_t> (coverageStartBlock,coverageEndBlock);
	p_uniform_real_distribution_0_to_1 = new std::uniform_real_distribution<ivy_float>(0.0,1.0);  // for % reads and for random_independent, for inter-I/O arrival times.

	generate_count = 0;

	return true;
}



bool IogeneratorRandom::generate(Eyeo& slang) {
//*debug*/ {ostringstream o; o<< "//*debug*/ IogeneratorRandom::generate(Eyeo& slang) entry.\n"; log(logfilename, o.str()); }
	// This is the base class IogeneratorRandom::generate() function that calculates the
	// random block number / LBA &

	// Then the derived class generate() function calculates the scheduled time of the I/O.

	slang.resetForNextIO();

	// we assume that eyeocb.data already points to the Eyeo object
	// and that eyeocb.aio_buf already points to a page-aligned I/O buffer

	slang.eyeocb.aio_fildes = p_iogenerator_stuff->fd;
//*debug*/ {ostringstream o; o<< "//*debug*/ IogeneratorRandom::generate(Eyeo& slang) before getting random location.\n"; log(logfilename, o.str()); }

	if (NULL == p_uniform_int_distribution)
	{
		log(logfilename, std::string("IogeneratorRandom::generate() - p_uniform_int_distribution not initialized.\n"));
		return false;
	}
	uint64_t current_block=(*p_uniform_int_distribution)(deafrangen);
//*debug*/ {ostringstream o; o<< "//*debug*/ IogeneratorRandom::generate(Eyeo& slang) got random current_block=" << current_block << ".\n"; log(logfilename, o.str()); }

	slang.eyeocb.aio_offset = (p_IogeneratorInput->blocksize_bytes) * current_block;
	slang.eyeocb.aio_nbytes = p_IogeneratorInput->blocksize_bytes;
	slang.scheduled_time=ivytime(0);
	slang.start_time=0;
	slang.end_time=0;
	slang.return_value=-2;
	slang.errno_value=-2;
//*debug*/ {ostringstream o; o<< "//*debug*/ IogeneratorRandom::generate(Eyeo& slang) before figuring read or write.\n"; log(logfilename, o.str()); }

	if (0.0 == p_IogeneratorInput->fractionRead)
	{
//*debug*/ {ostringstream o; o<< "//*debug*/ IogeneratorRandom::generate(Eyeo& slang) no compute write.\n"; log(logfilename, o.str()); }
		slang.eyeocb.aio_lio_opcode=IOCB_CMD_PWRITE;
	}
	else if (1.0 == p_IogeneratorInput->fractionRead)
	{
//*debug*/ {ostringstream o; o<< "//*debug*/ IogeneratorRandom::generate(Eyeo& slang) no compute read.\n"; log(logfilename, o.str()); }
		slang.eyeocb.aio_lio_opcode=IOCB_CMD_PREAD;
	}
	else
	{
		ivy_float random_0_to_1 = (*p_uniform_real_distribution_0_to_1)(deafrangen);
//*debug*/ {ostringstream o; o<< "//*debug*/ IogeneratorRandom::generate(Eyeo& slang) random_0_to_1=" << random_0_to_1 << ".\n"; log(logfilename, o.str()); }
		if ( random_0_to_1 <= p_IogeneratorInput->fractionRead )
			slang.eyeocb.aio_lio_opcode=IOCB_CMD_PREAD;
		else
			slang.eyeocb.aio_lio_opcode=IOCB_CMD_PWRITE;
	}
//*debug*/ {ostringstream o; o<< "//*debug*/ IogeneratorRandom::generate(Eyeo& slang) clean exit.\n"; log(logfilename, o.str()); }

    if (slang.eyeocb.aio_lio_opcode == IOCB_CMD_PWRITE)
    {

        slang.randomize_buffer();

//        if (generate_count < 30)
//        {
//            std::ostringstream o;
//            o   << "IogeneratorRandom::generate() for write with generate_count = " << generate_count
//                << "   " << slang.buffer_first_last_16_hex()
//                << std::endl;
//            log(logfilename, o.str());
//        }
    }

    generate_count++;

	return true;
}


