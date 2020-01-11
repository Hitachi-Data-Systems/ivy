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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>, Kumaran Subramaniam <kumaran.subramaniam@hitachivantara.com>, Stephen Morgan <stephen.morgan@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.

#include <unistd.h>
#include <sys/syscall.h>
#include <assert.h>

inline int ioprio_set(int which, int who, int ioprio)
{
	return syscall(SYS_ioprio_set, which, who, ioprio);
}

inline int ioprio_get(int which, int who)
{
	return syscall(SYS_ioprio_get, which, who);
}

enum {
	IOPRIO_CLASS_NONE,
	IOPRIO_CLASS_RT,
	IOPRIO_CLASS_BE,
	IOPRIO_CLASS_IDLE,
};

enum {
	IOPRIO_WHO_PROCESS = 1,
	IOPRIO_WHO_PGRP,
	IOPRIO_WHO_USER,
};


using namespace std;

#include "Eyeo.h"
#include "LUN.h"
#include "ivydriver.h"

#include "../../LUN_discovery/include/printableAndHex.h"
    // display_memory_contents() is part of "printableAndHex" which is in the LUN_discovery project
    // https://github.com/Hitachi-Data-Systems/LUN_discovery

extern std::string printable_ascii;
extern std::default_random_engine deafrangen;

Eyeo::Eyeo(Workload* pw, uint64_t& p_buffer) : pWorkload(pw)
{
// Note that the pointer to the buffer and the blocksize in the iocb are set separately after createing the Eyeos.
// This is because we get the memory for the buffer pool in one big piece for each Workload's Eyeos.

    memset(& sqe,0,sizeof(sqe));

    //sqe.opcode - set for each I/O - IORING_OP_READ_FIXED or IORING_OP_WRITE_FIXED (or IORING_OP_TIMEOUT, but this isn't done using an Eyeo)

	sqe.flags = IOSQE_FIXED_FILE;

//	sqe.ioprio = ioprio_get(IOPRIO_WHO_USER, 0 /*root*/);
//	sqe.ioprio = 0;

	sqe.fd = pw->pTestLUN->fd_index;
	//sqe.off - set for each I/O
	sqe.addr = p_buffer;
	sqe.len = pw->subinterval_array[0].input.blocksize_bytes;
	sqe.rw_flags = 0;
	sqe.user_data =(uint64_t) this;
	sqe.buf_index = 0; // there is only one big fixed buffer

	 p_buffer += round_up_to_4096_multiple(pw->subinterval_array[0].input.blocksize_bytes);
}

ivytime Eyeo::since_start_time() // returns ivytime(0) if start_time is not in the past
{
    ivytime now;
    now.setToNow();
    if (start_time >= now) return ivytime_zero;
    else                   return now - start_time;
}

void Eyeo::resetForNextIO()
{
	scheduled_time = start_time = end_time = ivytime_zero;
}

std::ostream& format_cqe_with_associated_shims_and_IOs(std::ostream& o, const io_uring_cqe& cqe)
{
    o << std::endl << cqe;

    if (cqe.user_data != 0)
    {
        const struct cqe_shim& shim = *((cqe_shim*)cqe.user_data);

        o << std::endl << "   " << shim;

        if (shim.type == cqe_type::eyeo)
        {
            const Eyeo& eyeo = *((Eyeo*)cqe.user_data);

            o << std::endl << "      " << eyeo;
        }
    }

    return o;
}

std::ostream& operator<<(std::ostream& o, const io_uring_cqe& cqe)
{
	o << "{ \"io_uring_cqe\" : { ";

    o << "\"this\" : \"" << &cqe << "\"";

    o << ", \"user_data" << "\" : \"" << std::dec << cqe.user_data     << " (0x" << std::hex << std::uppercase << std::setw(2 * sizeof(cqe.user_data)) << std::setfill('0') << cqe.user_data << ")\"";

    o << ", \"res" << "\" : \"" << std::dec << cqe.res     << " (0x" << std::hex << std::uppercase << std::setw(2 * sizeof(cqe.res)) << std::setfill('0') << cqe.res << ")\"";

    if (cqe.res < 0) { o << ", \"strerror\" : \"" << std::strerror(abs(cqe.res)) << "\""; }

    o << ", \"flags" << "\" : \"" << std::dec << cqe.flags     << " (0x" << std::hex << std::uppercase << std::setw(2 * sizeof(cqe.flags)) << std::setfill('0') << cqe.flags << ")\"";

    o << " } }";

    return o;
}
std::ostream& operator<<(std::ostream& o, const io_uring_sqe& sqe)
{
	o << "{ \"io_uring_sqe\" : { ";

    o << "\"this\" : \"" << &sqe << "\"";

	o << ", \"opcode\" : \"0x" << std::hex << std::setw(2*sizeof(sqe.opcode)) << std::setfill('0') << ((unsigned)sqe.opcode) << " - ";

	switch (sqe.opcode) {
        case IORING_OP_NOP:             o << "IORING_OP_NOP";             break;
        case IORING_OP_READV:           o << "IORING_OP_READV";           break;
        case IORING_OP_WRITEV:          o << "IORING_OP_WRITEV";          break;
        case IORING_OP_FSYNC:           o << "IORING_OP_FSYNC";           break;
        case IORING_OP_READ_FIXED:      o << "IORING_OP_READ_FIXED";      break;
        case IORING_OP_WRITE_FIXED:     o << "IORING_OP_WRITE_FIXED";     break;
        case IORING_OP_POLL_ADD:        o << "IORING_OP_POLL_ADD";        break;
        case IORING_OP_POLL_REMOVE:     o << "IORING_OP_POLL_REMOVE";     break;
        case IORING_OP_SYNC_FILE_RANGE: o << "IORING_OP_SYNC_FILE_RANGE"; break;
        case IORING_OP_SENDMSG:         o << "IORING_OP_SENDMSG";         break;
        case IORING_OP_RECVMSG:         o << "IORING_OP_RECVMSG";         break;
        case IORING_OP_TIMEOUT:         o << "IORING_OP_TIMEOUT";         break;
        case IORING_OP_TIMEOUT_REMOVE:  o << "IORING_OP_TIMEOUT_REMOVE";  break;
        case IORING_OP_ACCEPT:          o << "IORING_OP_ACCEPT";          break;
        case IORING_OP_ASYNC_CANCEL:    o << "IORING_OP_ASYNC_CANCEL";    break;
        case IORING_OP_LINK_TIMEOUT:    o << "IORING_OP_LINK_TIMEOUT";    break;
        case IORING_OP_CONNECT:         o << "IORING_OP_CONNECT";         break;
		default: o << "<unknown opcode>";
	}
	o << "\"";

	if (sqe.opcode == IORING_OP_READ_FIXED || sqe.opcode == IORING_OP_WRITE_FIXED)
    {
        o << ", \"flags\" : \"0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (unsigned) sqe.flags;
        if (sqe.flags & IOSQE_FIXED_FILE) o << " IOSQE_FIXED_FILE";
        if (sqe.flags & IOSQE_IO_DRAIN)   o << " IOSQE_IO_DRAIN";
        if (sqe.flags & IOSQE_IO_LINK)    o << " IOSQE_IO_LINK";
        o << "\"";

        o << ", \"ioprio"    << "\" : \"" << std::dec << sqe.ioprio        << " (0x" << std::hex << std::uppercase << std::setw(2 * sizeof(sqe.ioprio   )) << std::setfill('0') << sqe.ioprio << ")\"";
        o << ", \"fd"        << "\" : \"" << std::dec << sqe.fd            << " (0x" << std::hex << std::uppercase << std::setw(2 * sizeof(sqe.fd       )) << std::setfill('0') << sqe.fd << ")\"";;
        o << ", \"off/addr2" << "\" : \"" << std::dec << sqe.off           << " (0x" << std::hex << std::uppercase << std::setw(2 * sizeof(sqe.off      )) << std::setfill('0') << sqe.off << ")\"";
        o << ", \"addr"      << "\" : \"" << std::dec << sqe.addr          << " (0x" << std::hex << std::uppercase << std::setw(2 * sizeof(sqe.addr     )) << std::setfill('0') << sqe.addr << ")\"";
        o << ", \"len"       << "\" : \"" << std::dec << sqe.len           << " (0x" << std::hex << std::uppercase << std::setw(2 * sizeof(sqe.len      )) << std::setfill('0') << sqe.len << ")\"";
        o << ", \"rw_flags"  << "\" : \"" << std::dec << sqe.rw_flags      << " (0x" << std::hex << std::uppercase << std::setw(2 * sizeof(sqe.rw_flags )) << std::setfill('0') << sqe.rw_flags << ")";
        o << "\"";

        o << ", \"user_data" << "\" : \"" << std::dec << sqe.user_data     << " (0x" << std::hex << std::uppercase << std::setw(2 * sizeof(sqe.user_data)) << std::setfill('0') << sqe.user_data << ")\"";
        o << ", \"buf_index" << "\" : \"" << std::dec << sqe.buf_index     << " (0x" << std::hex << std::uppercase << std::setw(2 * sizeof(sqe.buf_index)) << std::setfill('0') << sqe.buf_index << ")\"";


    }
    else if (sqe.opcode == IORING_OP_TIMEOUT)
    {
        o << ", \"timeout_flags" << "\" : \"" << std::dec << sqe.timeout_flags << " (0x" << std::hex << std::uppercase << std::setw(2 * sizeof(sqe.timeout_flags)) << std::setfill('0') << sqe.timeout_flags << ")";
        if (sqe.timeout_flags & IORING_TIMEOUT_ABS)    o << " IORING_TIMEOUT_ABS";
        o << "\"";

        o << ", \"fd\" : \"" << std::dec << sqe.fd << "\"";

        o << ", \"offset (event count)" << "\" : \"" << std::dec << sqe.off << " (0x" << std::hex << std::uppercase << std::setw(2 * sizeof(sqe.off      )) << std::setfill('0') << sqe.off << ")\"";

        __kernel_timespec* p_timespec64 = ( __kernel_timespec* ) sqe.addr;

        if (p_timespec64 != nullptr)
        {
            o << ", \"addr->timespec64.tv_sec"  << "\" : \"" << std::dec << p_timespec64->tv_sec  << " (0x" << std::hex << std::uppercase << std::setw(2 * sizeof(p_timespec64->tv_sec )) << std::setfill('0') << p_timespec64->tv_sec   << ")\"";
            o << ", \"addr->timespec64.tv_nsec" << "\" : \"" << std::dec << p_timespec64->tv_nsec << " (0x" << std::hex << std::uppercase << std::setw(2 * sizeof(p_timespec64->tv_nsec)) << std::setfill('0') << p_timespec64->tv_nsec  << ")\"";
            struct timespec ts;
            ts.tv_sec = p_timespec64->tv_sec;
            ts.tv_nsec = p_timespec64->tv_nsec;

            if (sqe.timeout_flags & IORING_TIMEOUT_ABS)
            {
                o << ", \"absolute\" : \"";
                interpret_struct_timespec_as_localtime(o, ts);
                o << "\"";
            }
            else
            {
                ivytime i (ts);
                o << ", \"duration\" : \"" << i.format_as_duration_HMMSSns() << "\"";
            }
        }

        o << ", \"user_data" << "\" : \"" << std::dec << sqe.user_data     << " (0x" << std::hex << std::uppercase << std::setw(2 * sizeof(sqe.user_data)) << std::setfill('0') << sqe.user_data << ")\"";
    }

	o << " } }";

	return o;
}

std::ostream& operator<<(std::ostream& o, const Eyeo& e)
{
	o << "{ \"Eyeo\" : { ";

    o << "\"this\" : \"" << &e << "\"";



	o << ", " << e.shim;

    o << ", " << e.sqe;

    o << ", \"comment\" : ";
    o << "\"";
        {
            if (e.sqe.off == 0)
            {
                o << "<Error> submit queue entry offset may not be zero.  Thou shalt not trifle with sector zero.";
            }

            long double LUN_size_bytes_float = ((long double)e.pWorkload->pTestLUN->LUN_size_bytes);

            uint64_t just_past_Eyeo = e.sqe.off + ((uint64_t)e.sqe.len);

            if (e.sqe.off > e.pWorkload->pTestLUN->LUN_size_bytes)
            {
                o << "<Error> I/O offset is past the end of the LUN.";
            }
            else if (just_past_Eyeo > e.pWorkload->pTestLUN->LUN_size_bytes)
            {
                o << "<Error> I/O offset plus length is past the end of the LUN.";
            }

            long double just_past_Eyeo_float = (long double) just_past_Eyeo;

            if (LUN_size_bytes_float > 0)
            {
                long double from_fraction = ((long double) e.sqe.off) / LUN_size_bytes_float;

                long double just_past_fraction = just_past_Eyeo_float / LUN_size_bytes_float;

                o << "Eyeo from ";
                o << std::dec << std::fixed << std::setprecision(6) << (100.0 * from_fraction) << " % to ";
                o << std::dec << std::fixed << std::setprecision(6) << (100.0 * just_past_fraction) << " % of LUN.";
            }
        }
    o << "\"";

	o << ", \"workloadID"          << "\" : \"" << e.pWorkload->workloadID                       << "\"";
	o << ", \"io_sequence_number"  << "\" : \"" << e.io_sequence_number                          << "\"";
	o << ", \"scheduled_time"      << "\" : \"" << e.scheduled_time.format_as_datetime_with_ns() << "\"";
    o << ", \"start_time"          << "\" : \"" << e.start_time    .format_as_datetime_with_ns() << "\"";
    o << ", \"end_time"            << "\" : \"" << e.end_time      .format_as_datetime_with_ns() << "\"";
    o << ", \"return_value"        << "\" : \"" << std::dec << e.return_value   << "(0x" << std::hex << std::setw(2*sizeof(e.return_value  )) << std::setfill('0') << e.return_value << ")\"";

	o << ", \"pWorkload"           << "\" : \"" << e.pWorkload                                   << "\"";

	const uint64_t dedupe_unit_bytes = e.pWorkload->p_current_IosequencerInput->dedupe_unit_bytes;
	if (ivydriver.display_buffer_contents)
	{
	    if (e.sqe.len <= (2*dedupe_unit_bytes))
	    {
	        o << "\"buffer contents\" :\""  << std::endl;
	        display_memory_contents(o,((const unsigned char*) e.sqe.addr),(int) e.sqe.len,32);
	        o << "\"";
	    }
	    else
	    {
	        o << "\"first dedupe_unit_bytes = " << dedupe_unit_bytes << " of buffer contents\" : \"";
	        display_memory_contents(o,((const unsigned char*) e.sqe.addr),dedupe_unit_bytes,32);
	        o << "\"";

	        o << "\"last dedupe_unit_bytes = " << dedupe_unit_bytes << " of buffer contents\" : \"";
	        display_memory_contents(o,((const unsigned char*) (e.sqe.addr+e.sqe.len-dedupe_unit_bytes)),dedupe_unit_bytes,32);
	        o << "\"";
	    }
	}

	o << ", \"service_time_seconds()\" : \""  << std::fixed << std::setprecision(6) << e.service_time_seconds() << "\"";
    o << ", \"response_time_seconds()\" : \"" << std::fixed << std::setprecision(6) << e.response_time_seconds() << "\"";
    o << " } }";

	return o;
}

std::string Eyeo::thumbnail()
{
    std::ostringstream o;

	o << "Eyeo " << pWorkload->workloadID << "#" << io_sequence_number;

	if (scheduled_time.isZero())
	{
	    o << " IOPS=max";
	}
	else
	{
	    o << " scheduled" << scheduled_time.format_as_datetime();
	}

	if (end_time != ivytime_zero)
	{
	    o << ", service_time=" << (service_time_seconds()/1000.0) << " ms";
	}
	else
	{
	    if (start_time == ivytime_zero)
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


ivy_float Eyeo::service_time_seconds() const
{
	if (end_time.isZero() || start_time.isZero()) return -1.0;
	ivytime service_time = end_time - start_time;
	return (ivy_float) service_time;
}

ivy_float Eyeo::response_time_seconds() const
{
	if (end_time.isZero() || scheduled_time.isZero()) return -1.0;
	ivytime response_time = end_time - scheduled_time;
	return (ivy_float) response_time;
}

// This method corrects for the difference between compressibility desired by the caller and
// the actual compression ratio that ivy should use, based on measurements of ADR compression.
// (Possibly, it should be optionally invoked based on a user-supplied parameter.)
//
// The method works by looking up a table of actual requested vs. measured compression ratios.
// For values requested that are in between actual measurement points, the method interpolates
// and probabilistically determines a corrected compression ratio. The method returns either
// a corrected compression ratio (usually higher than the desired one) or zero (0.0) if the
// given block should not be compressed (probabilistically).
ivy_float Eyeo::lookup_probabilistic_compressibility(ivy_float desired_compressibility)
{
    ivy_float requested_compressibility = 0.0;
    ivy_float measured_compressibility = 0.0;
    ivy_float probability = 0.0;
    static bool identified_vendor = false;
    static bool vendor_is_hitachi = false;

    struct table {
        ivy_float measured_compressibility;
        ivy_float requested_compressibility;
    } correction_table[] = {
        {0.0625, 0.10}, //  1/16
        {0.1250, 0.15}, //  2/16
        {0.1875, 0.25}, //  3/16
        {0.2500, 0.30}, //  4/16
        {0.3125, 0.35}, //  5/16
        {0.3750, 0.45}, //  6/16
        {0.4375, 0.50}, //  7/16
        {0.5000, 0.55}, //  8/16
        {0.5625, 0.60}, //  9/16
        {0.6250, 0.70}, // 10/16
        {0.6875, 0.75}, // 11/16
        {0.7500, 0.80}, // 12/16
        {0.8125, 0.85}, // 13/16
        {0.8750, 0.95}, // 14/16
        {0.9375, 0.97}, // 15/16
        {1.0000, 0.97}, // 16/16 (impossible)
    };

    assert((desired_compressibility >= 0.0) && (desired_compressibility <= 1.0));

    // Short-circuit for common case.

    if (desired_compressibility == 0.0)
        return 0.0;

    // Correction table only applies to Hitachi subsystem.

    if (! identified_vendor)
    {
        if (pWorkload->pTestLUN->pLUN->attribute_value_matches("Vendor", "Hitachi"))
            vendor_is_hitachi = true;

        identified_vendor = true;
    }

    if (! vendor_is_hitachi)
        return desired_compressibility;

    // Since the table is so short, just use a linear lookup.
    // Start halfway through table if desired_compressibility is large.

    const unsigned int full = sizeof(correction_table) / sizeof(struct table);
    const unsigned int half = (full / 2) - 1;
    unsigned int i = (desired_compressibility < correction_table[half].measured_compressibility) ? 0 : half;
    for (; i < full; i++)
    {
        measured_compressibility = correction_table[i].measured_compressibility;
        if (measured_compressibility >= desired_compressibility)
        {
            if (i == full - 1)
            {
                // Can't go past 15/16 (i.e., 93.75%) on Hitachi subsystems.
                probability = 1.0;
            }
            else
            {
                // If not an exact match, interpolate.
                probability = (measured_compressibility == desired_compressibility) ? 1.0 : desired_compressibility / measured_compressibility;
            }
            requested_compressibility = correction_table[i].requested_compressibility;
            break;
        }
    }

    // Probabilistically compress this block.

    if (probability == 1.0)
        return requested_compressibility;
    else
        return (distribution(generator) < probability) ? requested_compressibility : 0.0;
}

void Eyeo::generate_pattern()
{
    uint64_t blocksize = sqe.len;
    uint64_t* p_uint64;
    char* p_c;
    uint64_t d;
    unsigned int first_bite;
    unsigned int word_index;
    char* past_buf;
    char* past_piece;
    char* p_word;
    int bsindex = 0;
    uint64_t count = 0;

    uint64_t pieces;
    uint64_t remainder;

    const uint64_t& dedupe_unit_bytes = pWorkload->p_current_IosequencerInput->dedupe_unit_bytes;

    ivy_float probabilistic_compressibility = lookup_probabilistic_compressibility(pWorkload->compressibility);

    pWorkload->write_io_count++;

    switch (pWorkload->pat)
    {
        case pattern::random:

            p_uint64 = (uint64_t*) sqe.addr;
            count = blocksize / 8;

            // For dedupe_method  = constant_ratio or target_spread, we have already confirmed that the blocksize is a multiple of 8192 in setMultipleParameters

            for (uint64_t i=0; i < count; i++)
            {
                if (pWorkload->doing_dedupe && i % (dedupe_unit_bytes/8) == 0)
                {
                    if (pWorkload->p_current_IosequencerInput->dedupe_type == dedupe_method::target_spread)
                    {
                        pWorkload->block_seed = pWorkload->last_block_seeds[bsindex++];
                    }
                    else if (pWorkload->p_current_IosequencerInput->dedupe_type == dedupe_method::constant_ratio)
                    {
                        pWorkload->block_seed = pWorkload->dedupe_constant_ratio_regulator->get_seed(this, sqe.off + i * sizeof(uint64_t));
                        if (pWorkload->block_seed == 0)
                        {
                            void* p = (void*) (sqe.addr + (i * 8));
                            memset(p,0x00,dedupe_unit_bytes);
                            i += ((dedupe_unit_bytes/8)-1);
                            goto past_random_pattern_8byte_write;
                        }
                    }
                    else if (pWorkload->p_current_IosequencerInput->dedupe_type == dedupe_method::static_method)
                    {
                        pWorkload->block_seed = duplicate_set_sub_block_starting_seed( i * 8 );
                        if (pWorkload->block_seed == 0)
                        {
                            void* p = (void*) (sqe.addr + (i * 8));
                            memset(p,0x00,dedupe_unit_bytes);
                            i += ((dedupe_unit_bytes/8)-1);
                            goto past_random_pattern_8byte_write;
                        }
                    }
                    else if (pWorkload->p_current_IosequencerInput->dedupe_type == dedupe_method::serpentine) {
                        // Do nothing special.
                    }
                    else assert(false);
                }
                xorshift64star(pWorkload->block_seed);
                (*(p_uint64 + i)) = pWorkload->block_seed;

past_random_pattern_8byte_write:;
            }

            break;

        case pattern::ascii:
            p_c = (char*) sqe.addr;
            count = blocksize / 8;

            for (uint64_t i=0; i < count; i++)
            {
                if (pWorkload->doing_dedupe && i % (dedupe_unit_bytes/8) == 0)
                {
                    if (pWorkload->p_current_IosequencerInput->dedupe_type == dedupe_method::target_spread)
                    {
                        pWorkload->block_seed = pWorkload->last_block_seeds[bsindex++];
                    }
                    else if (pWorkload->p_current_IosequencerInput->dedupe_type == dedupe_method::constant_ratio)
                    {
                        pWorkload->block_seed = pWorkload->dedupe_constant_ratio_regulator->get_seed(this, sqe.off + i * sizeof(uint64_t));
                        if (pWorkload->block_seed == 0)
                        {
                            void* p = (void*) (sqe.addr + (i * 8));
                            memset(p,0x00,dedupe_unit_bytes);
                            i += ((dedupe_unit_bytes/8)-1);
                            p_c += dedupe_unit_bytes;
                            goto past_ascii_pattern_8byte_write;
                        }
                    }
                    else if (pWorkload->p_current_IosequencerInput->dedupe_type == dedupe_method::static_method)
                    {
                        pWorkload->block_seed = duplicate_set_sub_block_starting_seed( i * 8 );
                        if (pWorkload->block_seed == 0)
                        {
                            void* p = (void*) (sqe.addr + (i * 8));
                            memset(p,0x00,dedupe_unit_bytes);
                            i += ((dedupe_unit_bytes/8)-1);
                            p_c += dedupe_unit_bytes;
                            goto past_ascii_pattern_8byte_write;
                        }
                    }
                    else if (pWorkload->p_current_IosequencerInput->dedupe_type == dedupe_method::serpentine) {
                        // Do nothing special.
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
past_ascii_pattern_8byte_write:;
            }

            break;

        case pattern::trailing_blanks:

            p_uint64 = (uint64_t*) sqe.addr;

            past_buf = ((char*) sqe.addr)+blocksize;

            pieces = blocksize / dedupe_unit_bytes;
            remainder = blocksize % dedupe_unit_bytes;

            // For dedupe_method  = constant_ratio, static, or target_spread, we have already confirmed that the blocksize is a multiple of dedupe_unit_bytes in setMultipleParameters

            for (uint64_t piece = 0; piece < pieces; piece++)
            {
                count = ceil(  (1.0-probabilistic_compressibility)  *  ((double) (dedupe_unit_bytes/8))  );

                for (uint64_t i=0; i < count; i++)
                {
                    if (pWorkload->doing_dedupe && i == 0)
                    {
                        if (pWorkload->p_current_IosequencerInput->dedupe_type == dedupe_method::target_spread)
                        {
                            pWorkload->block_seed = pWorkload->last_block_seeds[bsindex++];
                        }
                        else if (pWorkload->p_current_IosequencerInput->dedupe_type == dedupe_method::constant_ratio)
                        {
                            pWorkload->block_seed = pWorkload->dedupe_constant_ratio_regulator->get_seed(this, sqe.off + (piece * dedupe_unit_bytes) /* + i * sizeof(uint64_t))*/ );
                            if (pWorkload->block_seed == 0)
                            {
                                void* p = (void*) (sqe.addr + (piece * dedupe_unit_bytes));
                                memset(p,0x00,dedupe_unit_bytes);
                                i += ((dedupe_unit_bytes/8)-1);
                                p_uint64 = (uint64_t*) (sqe.addr + (piece+1)*dedupe_unit_bytes);
                                goto past_trailing_blanks_pattern_8byte_write;
                            }
                        }
                        else if (pWorkload->p_current_IosequencerInput->dedupe_type == dedupe_method::static_method)
                        {
                            pWorkload->block_seed = duplicate_set_sub_block_starting_seed(piece * dedupe_unit_bytes);
                            if (pWorkload->block_seed == 0)
                            {
                                void* p = (void*) (sqe.addr + (piece * dedupe_unit_bytes));
                                memset(p,0x00,dedupe_unit_bytes);
                                i += ((dedupe_unit_bytes/8)-1);
                                p_uint64 = (uint64_t*) (sqe.addr + (piece+1)*dedupe_unit_bytes);
                                goto past_trailing_blanks_pattern_8byte_write;
                            }
                        }
                        else if (pWorkload->p_current_IosequencerInput->dedupe_type == dedupe_method::serpentine)
                        {
                            // Do nothing special.
                        }
                        else assert(false);
                    }
                    xorshift64star(pWorkload->block_seed);
                    (*(p_uint64 + i)) = pWorkload->block_seed;
                }

                first_bite = floor((1.0-probabilistic_compressibility) * dedupe_unit_bytes );

                past_piece = ((char*) sqe.addr) + dedupe_unit_bytes * (piece +1);

                for (p_c = (((char*) sqe.addr) + (piece * dedupe_unit_bytes) + first_bite); p_c < past_piece; p_c++)
                {
                    *p_c = ' ';
                }
                p_uint64 += (dedupe_unit_bytes/8);

past_trailing_blanks_pattern_8byte_write:;
            }

            if (remainder > 0) // can only happen for dedupe_method = serpentine
            {
                count = ceil(  (1.0-probabilistic_compressibility)  *  ((double)(remainder / 8))  );

                for (uint64_t i=0; i < count; i++)
                {
                    xorshift64star(pWorkload->block_seed);
                    (*(p_uint64 + i)) = pWorkload->block_seed;
                }

                first_bite = floor((1.0-probabilistic_compressibility) * remainder );

                for (p_c = (((char*) sqe.addr) + (dedupe_unit_bytes * pieces) + first_bite); p_c < past_buf; p_c++)
                {
                    *p_c = ' ';
                }
            }

            break;

        case pattern::zeros:

            memset((void*)sqe.addr,0,blocksize);

            break;

        case pattern::all_0xFF:

            memset((void*)sqe.addr,0xFF,blocksize);

            break;

        case pattern::all_0x0F:

            memset((void*)sqe.addr,0x0F,blocksize);

            break;

        case pattern::gobbledegook:

            pieces = blocksize / dedupe_unit_bytes;
            remainder = blocksize % dedupe_unit_bytes;

            for (uint64_t piece = 0; piece < pieces; piece++)
            {
                p_c = (char*) (((uint64_t) sqe.addr) + piece * dedupe_unit_bytes);
                past_buf = p_c + dedupe_unit_bytes;
                count = 0;
                while(p_c < past_buf)
                {
                    if (pWorkload->doing_dedupe && count == 0)
                    {
                        if (pWorkload->p_current_IosequencerInput->dedupe_type == dedupe_method::target_spread)
                        {
                            pWorkload->block_seed = pWorkload->last_block_seeds[bsindex++];
                        }
                        else if (pWorkload->p_current_IosequencerInput->dedupe_type == dedupe_method::constant_ratio)
                        {
                            pWorkload->block_seed = pWorkload->dedupe_constant_ratio_regulator->get_seed(this, sqe.off + piece*dedupe_unit_bytes + count * sizeof(uint64_t));
                            if (pWorkload->block_seed == 0)
                            {
                                void* p = (void*) (sqe.addr + (piece * dedupe_unit_bytes));
                                memset(p,0x00,dedupe_unit_bytes);
                                goto past_trailing_gobbldegook_pattern_write;
                            }
                        }
                        else if (pWorkload->p_current_IosequencerInput->dedupe_type == dedupe_method::static_method)
                        {
                            pWorkload->block_seed = duplicate_set_sub_block_starting_seed(piece * dedupe_unit_bytes);
                            if (pWorkload->block_seed == 0)
                            {
                                void* p = (void*) (sqe.addr + (piece * dedupe_unit_bytes));
                                memset(p,0x00,dedupe_unit_bytes);
                                goto past_trailing_gobbldegook_pattern_write;
                            }
                        }
                        else if (pWorkload->p_current_IosequencerInput->dedupe_type == dedupe_method::serpentine) {
                            // Do nothing special.
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
past_trailing_gobbldegook_pattern_write:;
            }

            if (remainder > 0) // this doesn't happen when we're doing dedupe.
            {
                p_c      = (char*) (((uint64_t) sqe.addr) + pieces * dedupe_unit_bytes);
                past_buf = (char*) (((uint64_t) sqe.addr) + blocksize);
                while(p_c < past_buf)
                {
                    xorshift64star(pWorkload->block_seed);
                    word_index = pWorkload->block_seed % unique_word_count;
                    p_word = unique_words[word_index];
                    while ( (*p_word) && (p_c < past_buf) )
                    {
                        *p_c++ = *p_word++;
                    }
                    if (p_c < past_buf) {*p_c++ = ' ';}
                }
            }

            break;

        case pattern::whatever: // whatever is already in the memory buffer - don't generate a pattern

            break;

        case pattern::invalid:
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

    if (sqe.len >= 32)
    {
        unsigned char*
        pc = (unsigned char*) sqe.addr;
        for (int i=0; i<16; i++)
        {
            o << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)(*pc);
            pc++;
        }

        o << " ... ";

        pc = (unsigned char*) (sqe.addr+sqe.len-16);
        for (int i=0; i<16; i++)
        {
            o << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)(*pc);
            pc++;
        }
    }

    o << "(length = " << std::dec << sqe.len << ")";

    return o.str();
}


uint64_t Eyeo::zero_pattern_filtered_sub_block_number(uint64_t offset_within_this_Eyeo_buffer)
{

    uint64_t& dedupe_unit_bytes = pWorkload->p_current_IosequencerInput->dedupe_unit_bytes;

    uint64_t unfiltered_sub_block_number = (((uint64_t) sqe.off) + offset_within_this_Eyeo_buffer) / dedupe_unit_bytes;

    if (unfiltered_sub_block_number == 0)
    {
        std::ostringstream o;
        o << "Eyeo::zero_pattern_filtered_sub_block_number() was called for block zero.  We never write to block zero, so we don\'t over-write partition table / boot sector."
                << "  This Eyeo toString() = " << *this;
        throw std::runtime_error(o.str());
    }

    const long double& fraction_zero_pattern = pWorkload->p_current_IosequencerInput->fraction_zero_pattern;

    uint64_t n           = (uint64_t) (((long double)(unfiltered_sub_block_number  ))*(1.0-fraction_zero_pattern));
    uint64_t n_minus_one = (uint64_t) (((long double)(unfiltered_sub_block_number-1))*(1.0-fraction_zero_pattern));

    if (n == n_minus_one) return 0;

    return n;
}

#if 0
uint64_t Eyeo::fixed_pattern_sub_block_starting_seed(uint64_t offset_within_this_Eyeo_buffer)
{
    uint64_t filtered_sub_block_number = zero_pattern_filtered_sub_block_number(offset_within_this_Eyeo_buffer);

    if (filtered_sub_block_number == 0) return 0;

    const ivy_float& dedupe = pWorkload->p_current_IosequencerInput->dedupe;

    return pWorkload->uint64_t_hash_of_host_plus_LUN  ^ (1 + ((uint64_t) (((long double) filtered_sub_block_number)/dedupe)));
}
#endif // 0


uint64_t Eyeo::duplicate_set_filtered_sub_block_number(uint64_t unfiltered_sub_block_number)
{
    const long double& dedupe_percent = (long double) 1.0 - ((long double) 1.0 / pWorkload->p_current_IosequencerInput->dedupe);

    uint64_t n           = (uint64_t) (((long double)(unfiltered_sub_block_number  ))*(1.0-dedupe_percent));
    uint64_t n_minus_one = (uint64_t) (((long double)(unfiltered_sub_block_number-1))*(1.0-dedupe_percent));

    if (n == n_minus_one) return 0;

    return n;
}

uint64_t Eyeo::duplicate_set_sub_block_starting_seed(uint64_t offset_within_this_Eyeo_buffer)
{
    // pduplicate_set points to an array of duplicate block seeds.

    static uint64_t* pduplicate_set = nullptr;

    // Final seed we are trying to compute.

    uint64_t block_seed;

    // If this block should be zero-filled, zero_sub_block_number will be 0.

    uint64_t zero_sub_block_number = zero_pattern_filtered_sub_block_number(offset_within_this_Eyeo_buffer);

    if (zero_sub_block_number == 0)
    {
        return 0;
    }

    // If this block should be a duplicate, duplicate_set_sub_block_number will be 0.

    uint64_t duplicate_set_sub_block_number = duplicate_set_filtered_sub_block_number(zero_sub_block_number);

    if (duplicate_set_sub_block_number == 0)
    {
        const uint64_t& duplicate_set_size = pWorkload->p_current_IosequencerInput->duplicate_set_size;
        const uint64_t init_seed = 0x0123456789abcdefULL;

        assert(duplicate_set_size > 0);

        // Instantiate duplicate set if necessary.

        if (pduplicate_set == nullptr)
        {
            pduplicate_set = new (std::nothrow) uint64_t[duplicate_set_size];

            if (pduplicate_set == nullptr)
                return init_seed;

            pduplicate_set[0] = init_seed;
            xorshift64star(pduplicate_set[0]);
            for (unsigned int i = 1; i < duplicate_set_size; i++)
            {
                pduplicate_set[i] = pduplicate_set[i-1];
                xorshift64star(pduplicate_set[i]);
            }
        }

        // Choose the appropriate duplicate set member for this block seed.

        const long double& dedupe_percent = (long double) 1.0 - ((long double) 1.0 / pWorkload->p_current_IosequencerInput->dedupe);

        uint64_t relative_block_number = (uint64_t) (((long double) zero_sub_block_number) * (1.0 - dedupe_percent));

        block_seed = pduplicate_set[relative_block_number % duplicate_set_size];
    }
    else
    {
        block_seed = 1 + duplicate_set_sub_block_number;
    }

    // XOR block_seed with hash of (host plus LUN).

    return pWorkload->uint64_t_hash_of_host_plus_LUN ^ block_seed;
}


//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
#include <time.h>


std::ostream& interpret_struct_timespec_as_localtime(std::ostream& o, const struct timespec& ts)
{
    char buffer[100];

    {
        std::strftime(buffer, sizeof buffer, "%D %T", std::localtime(&ts.tv_sec));
        o << buffer << "." << std::dec << std::fixed << std::setw(9) << std::setfill('0') << ts.tv_nsec;

        o << " or ";

        std::strftime(buffer, sizeof buffer, "%D %T", std::gmtime(&ts.tv_sec));
        o << buffer << "." << std::dec << std::fixed << std::setw(9) << std::setfill('0') << ts.tv_nsec << " UTC";
    }

    return o;
}

