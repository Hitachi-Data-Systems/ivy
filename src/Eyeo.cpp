//Copyright (c) 2016 Hitachi Data Systems, Inc.
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
//Author: Allart Ian Vogelesang <ian.vogelesang@hds.com>
//
//Support:  "ivy" is not officially supported by Hitachi Data Systems.
//          Contact me (Ian) by email at ian.vogelesang@hds.com and as time permits, I'll help on a best efforts basis.
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
#include "iogenerator_stuff.h"
#include "IogeneratorInput.h"
#include "Iogenerator.h"
#include "RunningStat.h"
#include "Accumulators_by_io_type.h"
#include "SubintervalOutput.h"
#include "Subinterval.h"
#include "WorkloadThread.h"

// display_memory_contents() is part of "printableAndHex" which is in the LUN_discovery project - https://github.com/Hitachi-Data-Systems/LUN_discovery
#include "printableAndHex.h"

Eyeo::Eyeo(int t, WorkloadThread* pwt) : tag(t), pWorkloadThread(pwt)
{
	// When you create an Eyeo, get gets its memory page which it keeps for life,
	// althought the page can be resized.

	// Everthing else other than the pointer to the buffer needs to be initialized
	// by the nextEyeo function of the generator.

	// Well, except for the iocb's data pointer which we set to the address of the Eyeo object.
	// which should the the same as the address of the iocb, as the iocb is the first thing in an Eyeo.
	// But this made me feel better esthetically :-)

	eyeocb.aio_buf=0;
	eyeocb.aio_data=(uint64_t) this; // see if this works ... yes
	buf_size=0;

//	resize_buf(MINBUFSIZE);

}

void Eyeo::resetForNextIO() {
	// this might be excessively cautious - look here if we need to optimize performance in future.
	int save_tag = tag;
	int save_aio_fildes = eyeocb.aio_fildes;
	uint64_t save_aio_data = eyeocb.aio_data; // we put a pointer to the Eyeo object here so that when the system returns an iocb after an I/O we automatically know what Eyeo that is for.
	uint64_t save_aio_buf = eyeocb.aio_buf;
	uint64_t save_aio_nbytes = eyeocb.aio_nbytes;
	int save_buf_size = buf_size;
	WorkloadThread* save_pwt = pWorkloadThread;

	memset(this, 0, sizeof(Eyeo));

	pWorkloadThread = save_pwt;
	eyeocb.aio_fildes=save_aio_fildes;
	eyeocb.aio_data=save_aio_data;
	eyeocb.aio_buf=save_aio_buf;
	eyeocb.aio_nbytes = save_aio_nbytes;
	buf_size=save_buf_size;
	tag=save_tag;
}

std::string Eyeo::toString() {
	std::ostringstream o;
	//o << "tag_#=" << tag << ", iocb.aio_data=" << eyeocb.aio_data << ", iocb.aio_fildes=" << eyeocb.aio_fildes << ", iocb.aio_lio_opcode=";
	o << "tag_#=" << tag << ", opcode=";

	switch (eyeocb.aio_lio_opcode) {
		case IOCB_CMD_PREAD: o << "IOCB_CMD_PREAD"; break;
		case IOCB_CMD_PWRITE: o << "IOCB_CMD_PWRITE"; break;
		case IOCB_CMD_FSYNC: o << "IOCB_CMD_FSYNC"; break;
		case IOCB_CMD_FDSYNC: o << "IOCB_CMD_FDSYNC"; break;
		case IOCB_CMD_NOOP: o << "IOCB_CMD_NOOP"; break;
		case IOCB_CMD_PREADV: o << "IOCB_CMD_PREADV"; break;
		case IOCB_CMD_PWRITEV: o << "IOCB_CMD_PWRITEV"; break;
		default: o << eyeocb.aio_lio_opcode;
	}

	//o << ", iocb.aio_buf=" << eyeocb.aio_buf;
	o << ", byte offset=" << eyeocb.aio_offset;
	o << ", blksize=" << eyeocb.aio_nbytes;
	o << ", sched=" << scheduled_time.format_as_datetime_with_ns()
	  << ", start=" << start_time.format_as_datetime_with_ns()
	  << ", end=" << end_time.format_as_datetime_with_ns()
	  << ", service_time_seconds=" << std::fixed << std::setprecision(6) << service_time_seconds()
	  << ", response_time_seconds=" << std::fixed << std::setprecision(6) << response_time_seconds()
	  << ", ret=" << return_value
	  << ", errno=" << errno_value
	  << ", buf_size=" << buf_size;
	return o.str();
}


//template<std::size_t Len>
//struct ivy_aligned_iobuf
//{
//	alignas(4096) unsigned char data[Len];
//};
//
//bool Eyeo::resize_buf(int newsize)
//{
//
///*debug*/ { std::ostringstream o; o << " Eyeo::resize_buf(int newsize = " << newsize << ") entry with old buf_size = " << buf_size << " and MINBUFSIZE = " << MINBUFSIZE
///*debug*/ 	<< "  struct iocb.aiobuf (pointer to I/O buffer to read/write data dehydrated as uint64_t= " << std::hex << std::setw(16) << std::setfill('0') << eyeocb.aio_buf
///*debug*/	<< "  struct iocb.aio_data (pointer to owning Eyeo dehydrated as uint64_t)  = " << std::hex << std::setw(16) << std::setfill('0') << eyeocb.aio_data
///*debug*/	<< "." << std::endl; log(pWorkloadThread->slavethreadlogfile,o.str());}
//
//	if (newsize <=0) {
///*debug*/	log(pWorkloadThread->slavethreadlogfile,"Eyeo::resize_buf() failed - called with zero or negative new size.");
//		return false;
//	}
//
//	if ( newsize == buf_size)
//	{
///*debug*/	log(pWorkloadThread->slavethreadlogfile,"Eyeo::resize_buf() success - nothing to do.  Already is the requested size.");
//		return true;
//	}
//
//	if ( ( newsize <= MINBUFSIZE ) && (buf_size == MINBUFSIZE) )
//	{
///*debug*/	log(pWorkloadThread->slavethreadlogfile,"Eyeo::resize_buf() success - nothing to do.  Requested size is below MINBUFSIZE and the size already is MINBUFSIZE.");
//		return true;
//	}
//
//	unsigned int new_buf_size = (newsize < MINBUFSIZE ) ? MINBUFSIZE : newsize;
//
//	{
////		// Trying locks to see if memalign may not be thread-safe
////		std::unique_lock<std::mutex> u_lk(*(pWorkloadThread->p_ivyslave_main_mutex));
//
//		if (buf_size > 0 && NULL!=(void*)eyeocb.aio_buf)
//		{
//			/*debug*/ { std::ostringstream o; o << " Eyeo::resize_buf() about to free struct iocb.aio_buf = "
//			/*debug*/ 	<< "  struct iocb.aiobuf (pointer to I/O buffer to read/write data dehydrated as uint64_t= " << std::hex << std::setw(16) << std::setfill('0') << eyeocb.aio_buf
//			/*debug*/	<< "." << std::endl; log(pWorkloadThread->slavethreadlogfile,o.str());}
//
//			free((void*)eyeocb.aio_buf);
//			eyeocb.aio_buf = 0;
//
//			/*debug*/ { std::ostringstream o; o << " Eyeo::resize_buf() after to free struct iocb.aio_buf = "
//			/*debug*/ 	<< "  struct iocb.aiobuf (pointer to I/O buffer to read/write data dehydrated as uint64_t= " << std::hex << std::setw(16) << std::setfill('0') << eyeocb.aio_buf
//			/*debug*/	<< "." << std::endl; log(pWorkloadThread->slavethreadlogfile,o.str());}
//
//		}
//
///*debug*/ { std::ostringstream o; o << " Eyeo::resize_buf() - about to memalign(BUF_ALIGNMENT_BOUNDARY_SIZE = " << BUF_ALIGNMENT_BOUNDARY_SIZE << ", new_buf_size = " << new_buf_size << ")." << std::endl; log(pWorkloadThread->slavethreadlogfile,o.str());}
//
//		eyeocb.aio_buf = (uint64_t) new struct ivy_aligned_iobuf<new_buf_size>;
//		eyeocb.aio_buf = (uint64_t) new std::aligned_storage<new_buf_size,4096>;
//		eyeocb.aio_buf = (uint64_t) memalign(BUF_ALIGNMENT_BOUNDARY_SIZE, new_buf_size);
//		eyeocb.aio_buf = (uint64_t) aligned_alloc(BUF_ALIGNMENT_BOUNDARY_SIZE, new_buf_size); // doesn't work on my development machine
//
//
///*debug*/ { std::ostringstream o; o << " Eyeo::resize_buf() - newly allocated struct iocb.aio_buf = "
///*debug*/ 	<< "  struct iocb.aiobuf (pointer to I/O buffer to read/write data dehydrated as uint64_t= " << std::hex << std::setw(16) << std::setfill('0') << eyeocb.aio_buf
///*debug*/	<< "." << std::endl; log(pWorkloadThread->slavethreadlogfile,o.str());}
//
//
//
//
//	}
//	if (eyeocb.aio_buf != 0) {
//		buf_size=new_buf_size;
///*debug*/ { std::ostringstream o; o << " Eyeo::resize_buf() return true." << std::endl; log(pWorkloadThread->slavethreadlogfile,o.str());}
//		return true;
//	} else {
///*debug*/ { std::ostringstream o; o << " Eyeo::resize_buf() return false." << std::endl; log(pWorkloadThread->slavethreadlogfile,o.str());}
//		buf_size=0;
//		return false;
//	}
//}
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

extern std::string printable_ascii;

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

    pWorkloadThread->write_io_count++;

    switch (pWorkloadThread->pat)
    {
        case pattern::random:

            p_uint64 = (uint64_t*) eyeocb.aio_buf;
            uint64_t_count = blocksize / 8;

            for (uint64_t i=0; i < uint64_t_count; i++)
            {
                xorshift64star(pWorkloadThread->block_seed);
                (*(p_uint64 + i)) = pWorkloadThread->block_seed;
            }

            break;

        case pattern::ascii:
            p_c = (char*) eyeocb.aio_buf;
            uint64_t_count = blocksize / 8;

            for (uint64_t i=0; i < uint64_t_count; i++)
            {
                xorshift64star(pWorkloadThread->block_seed);

                d = pWorkloadThread->block_seed;

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
            uint64_t_count = ceil(  (1.0-pWorkloadThread->compressibility)  *  ((double)(blocksize / 8))  );

            for (uint64_t i=0; i < uint64_t_count; i++)
            {
                xorshift64star(pWorkloadThread->block_seed);
                (*(p_uint64 + i)) = pWorkloadThread->block_seed;
            }

            first_bite = floor((1.0-pWorkloadThread->compressibility) * blocksize );
            past_buf = ((char*) eyeocb.aio_buf)+blocksize;

            for (p_c = (((char*) eyeocb.aio_buf)+first_bite); p_c < past_buf; p_c++)
            {
                *p_c = (char) 0;
            }

            break;

        case pattern::gobbledegook:

            p_c = (char*)eyeocb.aio_buf;
            past_buf = ((char*) eyeocb.aio_buf)+blocksize;
            while(p_c < past_buf)
            {
                xorshift64star(pWorkloadThread->block_seed);
                word_index = pWorkloadThread->block_seed % unique_word_count;
                p_word = unique_words[word_index];
                while ( (*p_word) && (p_c < past_buf) )
                {
                    *p_c++ = *p_word++;
                }
                if (p_c < past_buf) {*p_c++ = ' ';}
            }

            break;

        default:
            {
                std::ostringstream o;
                o << "Internal programming error - invalid pattern type in Eyeo::generate_pattern() at line " << __LINE__ << " of " << __FILE__;
                throw std::runtime_error(o.str());
            }
    }
/*debug*/ if (pWorkloadThread->write_io_count < 10)
/*debug*/ {
/*debug*/     std::ostringstream o;
/*debug*/
/*debug*/     o << "Generated pattern for write I/O number " << pWorkloadThread->write_io_count << std::endl;
/*debug*/
/*debug*/     // display_memory_contents is part of "printableAndHex" which is in the LUN_discovery project - https://github.com/Hitachi-Data-Systems/LUN_discovery
/*debug*/     display_memory_contents(o, (unsigned char*)eyeocb.aio_buf, blocksize, 32 /* int perlinemax=16, std::string eachlineprefix=""*/);
/*debug*/
/*debug*/     log(pWorkloadThread->slavethreadlogfile,o.str());
/*debug*/ }
    return;
}

std::string Eyeo::buffer_first_last_16_hex()
{
    std::ostringstream o;

    if (eyeocb.aio_nbytes>=32)
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
