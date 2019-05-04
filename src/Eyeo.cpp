//Copyright (c) 2016, 2017, 2018 Hitachi Vantara Corporation
//All Rights Reserved.
//
//   Licensed under the Apache License, Version 2.0 (the "License"); you may
//   not use this file except in Ŕcompliance with the License. You may obtain
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
#include <assert.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <linux/aio_abi.h>
#include <string.h> // for memset()
#include <malloc.h> // for memalign()
#include <mutex>
#include <stdlib.h>

using namespace std;

#include "ivytime.h"
#include "ivydefines.h"
#include "ivyhelpers.h"

#include "Eyeo.h"
#include "ivyhelpers.h"
#include "RunningStat.h"
#include "LUN.h"
#include "WorkloadID.h"
#include "IosequencerInput.h"
#include "Iosequencer.h"
#include "RunningStat.h"
#include "Accumulators_by_io_type.h"
#include "SubintervalOutput.h"
#include "Subinterval.h"
#include "WorkloadThread.h"
#include "Workload.h"
#include "DedupeConstantRatioRegulator.h"

extern std::string printable_ascii;

#include "../../LUN_discovery/include/printableAndHex.h"
    // display_memory_contents() is part of "printableAndHex" which is in the LUN_discovery project
    // https://github.com/Hitachi-Data-Systems/LUN_discovery

Eyeo::Eyeo(Workload* pw) : pWorkload(pw)
{
// Note that the pointer to the buffer and the blocksize in the iocb are set separately after createing the Eyeos.
// This is because we get the memory for the buffer pool in one big piece for each Workload's Eyeos.

	eyeocb.aio_data=(uint64_t) this; // see if this works ... yes
}

ivytime Eyeo::since_start_time() // returns ivytime(0) if start_time is not in the past
{
    ivytime now;
    now.setToNow();
    if (start_time >= now) return ivytime(0);
    else                   return now - start_time;
}

void Eyeo::resetForNextIO() {
	int      save_aio_fildes = eyeocb.aio_fildes;
	uint64_t save_aio_data = eyeocb.aio_data; // we put a pointer to the Eyeo object here so that when the system returns an iocb after an I/O we automatically know what Eyeo that is for.
	uint64_t save_aio_buf = eyeocb.aio_buf;
	uint64_t save_aio_nbytes = eyeocb.aio_nbytes;

	memset(&eyeocb, 0, sizeof(eyeocb));

	eyeocb.aio_fildes=save_aio_fildes;
	eyeocb.aio_data=save_aio_data;
	eyeocb.aio_buf=save_aio_buf;
	eyeocb.aio_nbytes = save_aio_nbytes;

	iops_max_counted = false;
	scheduled_time = start_time = running_time = end_time = ivytime(0);
}

std::ostream& operator<<(std::ostream& o, const struct io_event& e)
{
    o << "struct io_event at " << std::hex << std::uppercase << &e << ": ";
    o << " data (=struct iocb.aio_data) = 0x" << std::hex << std::uppercase << e.data;
    o << ", obj (iocb*) " << std::hex << std::uppercase << e.obj;
    o << ", res = "  << std::dec << e.res;
    o << ", res2 = " << std::dec << e.res2;
    Eyeo* pEyeo = (Eyeo*) e.data;
    o << ".  io_event.data -> Eyeo = " << pEyeo->toString();
    return o;
}

std::string Eyeo::toString() {
	std::ostringstream o;

	o << std::endl << std::endl; // to make the Eyeos separated in the log

	o << "Eyeo " << pWorkload->workloadID << "#" << io_sequence_number << " at " << this;

    o << ", eyeocb.aio_data (Eyeo*)= 0x" << std::hex << std::uppercase << eyeocb.aio_data;

	o << ", opcode=";

	switch (eyeocb.aio_lio_opcode) {
		case IOCB_CMD_PREAD:   o << "IOCB_CMD_PREAD"; break;
		case IOCB_CMD_PWRITE:  o << "IOCB_CMD_PWRITE"; break;
		case IOCB_CMD_FSYNC:   o << "IOCB_CMD_FSYNC"; break;
		case IOCB_CMD_FDSYNC:  o << "IOCB_CMD_FDSYNC"; break;
		case IOCB_CMD_NOOP:    o << "IOCB_CMD_NOOP"; break;
		case IOCB_CMD_PREADV:  o << "IOCB_CMD_PREADV"; break;
		case IOCB_CMD_PWRITEV: o << "IOCB_CMD_PWRITEV"; break;
		default: o << "unknown opcode 0x"
		    << std::hex << std::setw(2*sizeof(eyeocb.aio_lio_opcode)) << std::setfill('0') << std::uppercase
            << eyeocb.aio_lio_opcode;
	}

    o << ", eyeocb.aio_filedes = "  << std::dec << eyeocb.aio_fildes;
    o << ", eyeocb.aio_buf = 0x"    << std::hex << std::uppercase << eyeocb.aio_buf;
    o << ", eyeocb.aio_nbytes = "   << std::dec << eyeocb.aio_nbytes;
    o << ", eyeocb.aio_offset = 0x" << std::hex << std::uppercase << eyeocb.aio_offset;
    o << ", eyeocb.aio_flags = 0x"  << std::hex << std::uppercase << eyeocb.aio_flags;
    o << ", eyeocb.aio_resfd = "    << std::dec << eyeocb.aio_resfd;

	o << ", sched="   << scheduled_time.format_as_datetime_with_ns()
	  << ", start="   << start_time    .format_as_datetime_with_ns()
	  << ", running=" << running_time  .format_as_datetime_with_ns()
	  << ", end="     << end_time      .format_as_datetime_with_ns()
	  << ", service_time_seconds="  << std::fixed << std::setprecision(6) << service_time_seconds()
	  << ", response_time_seconds=" << std::fixed << std::setprecision(6) << response_time_seconds()
	  << ", submit_time_seconds="   << std::fixed << std::setprecision(6) << submit_time_seconds()
	  << ", running_time_seconds="  << std::fixed << std::setprecision(6) << running_time_seconds()
	  << ", ret="     << return_value
	  << ", errno="    << errno_value
	  << ", pWorkload=" << pWorkload
	  << ", iops_max_counted = ";
	if (iops_max_counted) o << "true";
	else                  o << "false";

	o << std::endl;
	o << std::endl;

	return o.str();
}

std::string Eyeo::thumbnail()
{
    std::ostringstream o;

	o << "Eyeo " << pWorkload->workloadID << "#" << io_sequence_number;

	if (ivytime(0) == scheduled_time)
	{
	    o << " IOPS=max";
	}
	else
	{
	    o << " scheduled" << scheduled_time.format_as_datetime();
	}

	if (end_time != ivytime(0))
	{
	    o << ", service_time=" << (service_time_seconds()/1000.0) << " ms";
	}
	else
	{
	    if (start_time == ivytime(0))
	    {
	        o << ", not started";
	    }
	    else
	    {
	        o << ", started=" << start_time.format_as_datetime();
	    }
	}

	return o.str();

}


ivy_float Eyeo::service_time_seconds()
{
	if (ivytime(0) == end_time || ivytime(0) == start_time) return -1.0;
	ivytime service_time = end_time - start_time;
	return (ivy_float) service_time;
}

ivy_float Eyeo::response_time_seconds()
{
	if (ivytime(0) == end_time || ivytime(0) == scheduled_time) return -1.0;
	ivytime response_time = end_time - scheduled_time;
	return (ivy_float) response_time;
}

ivy_float Eyeo::submit_time_seconds()
{
	if (ivytime(0) == start_time || ivytime(0) == running_time) return -1.0;
	ivytime submit_time = running_time - start_time;
	return (ivy_float) submit_time;
}

ivy_float Eyeo::running_time_seconds()
{
	if (ivytime(0) == end_time || ivytime(0) == running_time) return -1.0;
	ivytime run_time = end_time - running_time;
	return (ivy_float) run_time;
}

void Eyeo::generate_pattern()
{
    uint64_t blocksize = eyeocb.aio_nbytes;
    uint64_t uint64_t_count;
    uint64_t* p_uint64;
    char* p_c;
    uint64_t d;
    unsigned int first_bite;
    unsigned int word_index;
    char* past_buf;
    char* p_word;
    int bsindex = 0;
    int count = 0;

    pWorkload->write_io_count++;

    switch (pWorkload->pat)
    {
        case pattern::random:

            p_uint64 = (uint64_t*) eyeocb.aio_buf;
            uint64_t_count = blocksize / 8;

            for (uint64_t i=0; i < uint64_t_count; i++)
            {
                if (pWorkload->doing_dedupe && i % 1024 == 0)
                {
                    if (pWorkload->subinterval_array[0].input.dedupe_type == dedupe_method::target_spread)
                    {
                        pWorkload->block_seed = pWorkload->last_block_seeds[bsindex++];
                    }
                    else if (pWorkload->subinterval_array[0].input.dedupe_type == dedupe_method::constant_ratio)
                    {
                        pWorkload->block_seed = pWorkload->dedupe_constant_ratio_regulator->get_seed(eyeocb.aio_offset / 8192 + i / 1024);
                    }
                    else assert(false);
                }
                xorshift64star(pWorkload->block_seed);
                (*(p_uint64 + i)) = pWorkload->block_seed;
            }

            break;

        case pattern::ascii:
            p_c = (char*) eyeocb.aio_buf;
            uint64_t_count = blocksize / 8;

            for (uint64_t i=0; i < uint64_t_count; i++)
            {
                if (pWorkload->doing_dedupe && i % 1024 == 0)
                {
                    if (pWorkload->subinterval_array[0].input.dedupe_type == dedupe_method::target_spread)
                    {
                        pWorkload->block_seed = pWorkload->last_block_seeds[bsindex++];
                    }
                    else if (pWorkload->subinterval_array[0].input.dedupe_type == dedupe_method::constant_ratio)
                    {
                        pWorkload->block_seed = pWorkload->dedupe_constant_ratio_regulator->get_seed(eyeocb.aio_offset / 8192 + i / 1024);
                    }
                    else assert(false);
                }
                xorshift64star(pWorkload->block_seed);

                d = pWorkload->block_seed;

                *p_c++ = printable_ascii[d % printable_ascii.size()]; // 1
                d /= printable_ascii.size();
                *p_c++ = printable_ascii[d % printable_ascii.size()]; // 2
                d /= printable_ascii.size();
                *p_c++ = printable_ascii[d % printable_ascii.size()]; // 3
                d /= printable_ascii.size();
                *p_c++ = printable_ascii[d % printable_ascii.size()]; // 4
                d /= printable_ascii.size();
                *p_c++ = printable_ascii[d % printable_ascii.size()]; // 5
                d /= printable_ascii.size();
                *p_c++ = printable_ascii[d % printable_ascii.size()]; // 6
                d /= printable_ascii.size();
                *p_c++ = printable_ascii[d % printable_ascii.size()]; // 7
                d /= printable_ascii.size();
                *p_c++ = printable_ascii[d % printable_ascii.size()]; // 8
                d /= printable_ascii.size();
            }

            break;

        case pattern::trailing_zeros:

            p_uint64 = (uint64_t*) eyeocb.aio_buf;
            uint64_t_count = ceil(  (1.0-pWorkload->compressibility)  *  ((double)(blocksize / 8))  );

            for (uint64_t i=0; i < uint64_t_count; i++)
            {
                if (pWorkload->doing_dedupe && i % 1024 == 0)
                {
                    if (pWorkload->subinterval_array[0].input.dedupe_type == dedupe_method::target_spread)
                    {
                        pWorkload->block_seed = pWorkload->last_block_seeds[bsindex++];
                    }
                    else if (pWorkload->subinterval_array[0].input.dedupe_type == dedupe_method::constant_ratio)
                    {
                        pWorkload->block_seed = pWorkload->dedupe_constant_ratio_regulator->get_seed(eyeocb.aio_offset / 8192 + i / 1024);
                    }
                    else assert(false);
                }
                xorshift64star(pWorkload->block_seed);
                (*(p_uint64 + i)) = pWorkload->block_seed;
            }

            first_bite = floor((1.0-pWorkload->compressibility) * blocksize );
            past_buf = ((char*) eyeocb.aio_buf)+blocksize;

            for (p_c = (((char*) eyeocb.aio_buf)+first_bite); p_c < past_buf; p_c++)
            {
                *p_c = (char) 0;
            }

            break;

        case pattern::zeros:

            memset((void*)eyeocb.aio_buf,0,blocksize);

            break;

        case pattern::all_0xFF:

            memset((void*)eyeocb.aio_buf,0xFF,blocksize);

            break;

        case pattern::all_0x0F:

            memset((void*)eyeocb.aio_buf,0x0F,blocksize);

            break;

        case pattern::gobbledegook:

            p_c = (char*)eyeocb.aio_buf;
            past_buf = ((char*) eyeocb.aio_buf)+blocksize;
            count = 0;
            while(p_c < past_buf)
            {
                if (pWorkload->doing_dedupe && count % 1024 == 0)
                {
                    if (pWorkload->subinterval_array[0].input.dedupe_type == dedupe_method::target_spread)
                    {
                        pWorkload->block_seed = pWorkload->last_block_seeds[bsindex++];
                    }
                    else if (pWorkload->subinterval_array[0].input.dedupe_type == dedupe_method::constant_ratio)
                    {
                        pWorkload->block_seed = pWorkload->dedupe_constant_ratio_regulator->get_seed(eyeocb.aio_offset / 8192 + count / 1024);
                    }
                    else assert(false);
                }
                xorshift64star(pWorkload->block_seed);
                word_index = pWorkload->block_seed % unique_word_count;
                p_word = unique_words[word_index];
                while ( (*p_word) && (p_c < past_buf) )
                {
                    *p_c++ = *p_word++;
                    count++;
                }
                if (p_c < past_buf) {*p_c++ = ' '; count++;}
            }

            break;

        default:
            {
                std::ostringstream o;
                o << "Internal programming error - invalid pattern type in Eyeo::generate_pattern() at line " << __LINE__ << " of " << __FILE__;
                throw std::runtime_error(o.str());
            }
    }

    return;
}

std::string Eyeo::buffer_first_last_16_hex()
{
    std::ostringstream o;

    if (eyeocb.aio_nbytes >= 32)
    {
        unsigned char*
        pc = (unsigned char*) eyeocb.aio_buf;
        for (int i=0; i<16; i++)
        {
            o << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)(*pc);
            pc++;
        }

        o << " ... ";

        pc = (unsigned char*) (eyeocb.aio_buf+eyeocb.aio_nbytes-16);
        for (int i=0; i<16; i++)
        {
            o << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)(*pc);
            pc++;
        }
    }

    o << "(length = " << std::dec << eyeocb.aio_nbytes << ")";

    return o.str();
}
