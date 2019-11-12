//Copyright (c) 2018 Hitachi Vantara Corporation
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

#include "Workload.h"
#include "ivydriver.h"
#include "IosequencerRandomIndependent.h"
#include "IosequencerRandomSteady.h"
#include "IosequencerSequential.h"

//#define IVYDRIVER_TRACE
// IVYDRIVER_TRACE defined here in this source file rather than globally in ivydefines.h so that
//  - the CodeBlocks editor knows the symbol is defined and highlights text accordingly.
//  - you can turn on tracing separately for each class in its own source file.

Workload::Workload()
{
    p_current_subinterval      = & (subinterval_array[0]);
    p_current_IosequencerInput = & (subinterval_array[0].input);
    p_current_SubintervalOutput= & (subinterval_array[0].output);
    p_my_iosequencer = nullptr;
    dedupe_target_spread_regulator = nullptr;
    dedupe_constant_ratio_regulator = nullptr;
}

Workload::~Workload()
{
    delete p_my_iosequencer;
    for (auto& pEyeo : allEyeosThatExist) delete pEyeo;
    if (dedupe_target_spread_regulator != nullptr) delete dedupe_target_spread_regulator;
    if (dedupe_constant_ratio_regulator != nullptr) delete dedupe_constant_ratio_regulator;
}


void Workload::prepare_to_run()
{
#if defined(IVYDRIVER_TRACE)
    { workload_callcount_prepare_to_run++; if (workload_callcount_prepare_to_run <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << workloadID << ':' << workload_callcount_prepare_to_run << ") "
        << "            Entering Workload::prepare_to_run()."; log(pWorkloadThread->slavethreadlogfile,o.str()); } }
#endif

    if (subinterval_array[0].input.fractionRead == 1.0)
    {
        have_writes = false;
        doing_dedupe = false;
    }
    else
    {
        have_writes = true;
        pat = subinterval_array[0].input.pat;
        compressibility = subinterval_array[0].input.compressibility;

        if ( subinterval_array[0].input.dedupe == 1.0 )
        {
            doing_dedupe=false;
            block_seed = ( (uint64_t) std::hash<std::string>{}(workloadID.workloadID) ) ^ ivydriver.test_start_time.getAsNanoseconds();
        }
        else
        {
            doing_dedupe=true;
            threads_in_workload_name = (pattern_float_type) subinterval_array[0].input.threads_in_workload_name;
            serpentine_number = 1.0 + ( (pattern_float_type) subinterval_array[0].input.this_thread_in_workload );
            serpentine_number -= threads_in_workload_name; // this is because we increment the serpentine number by threads_in_workload before using it.
            serpentine_multiplier =
                ( 1.0 - ( (pattern_float_type)  subinterval_array[0].input.fractionRead ) )
                /       ( (pattern_float_type)  subinterval_array[0].input.dedupe );

            pattern_seed = subinterval_array[0].input.pattern_seed;
            pattern_number = 0;
        }
    }

//    if (doing_dedupe && subinterval_array[0].input.dedupe_type == dedupe_method::constant_ratio)
//    {
//        DedupeConstantRatioRegulator::lookup_sides_and_throws(subinterval_array[0].input.dedupe,constant_ratio_sides,constant_ratio_throws);
//        if (p_sides_distribution != nullptr) delete p_sides_distribution;
//        p_sides_distribution = new std::uniform_int_distribution<uint64_t> (0, constant_ratio_sides);
//        throw_group_size = (((pTestLUN->maxLBA) + 1)* sector_size_bytes) / 8192;
//    }


    workload_cumulative_completion_count = 0;

    write_io_count = 0;

    subinterval_array[0].output.clear();  // later if energetic figure out if these must already be cleared.
    subinterval_array[1].output.clear();
    subinterval_array[0].subinterval_status=subinterval_state::ready_to_run;
    subinterval_array[1].subinterval_status=subinterval_state::ready_to_run;

    p_current_subinterval       = & (subinterval_array[0]);
    p_current_IosequencerInput  = & (p_current_subinterval-> input);
    p_current_SubintervalOutput = & (p_current_subinterval->output);

    if ( p_my_iosequencer != nullptr )
    {
        delete p_my_iosequencer;
        p_my_iosequencer = nullptr;
    }

    {
        const std::string& type = p_current_IosequencerInput->iosequencer_type;
        if ( type == "random_steady" )
        {
            p_my_iosequencer = new IosequencerRandomSteady
                (pTestLUN->pLUN, pWorkloadThread->slavethreadlogfile, workloadID.workloadID, pWorkloadThread, pTestLUN, this);
        }
        else if ( type == "random_independent" )
        {
            p_my_iosequencer = new IosequencerRandomIndependent
                (pTestLUN->pLUN, pWorkloadThread->slavethreadlogfile, workloadID.workloadID, pWorkloadThread, pTestLUN, this);
        }
        else if ( type == "sequential" )
        {
            p_my_iosequencer = new IosequencerSequential
                (pTestLUN->pLUN, pWorkloadThread->slavethreadlogfile, workloadID.workloadID, pWorkloadThread, pTestLUN, this);
        }
        else
        {
            std::ostringstream o; o << "Workload " << workloadID
                << " start() - Got a \"Go!\" but the first subinterval\'s IosequencerInput\'s iosequencer_type \""
                << p_current_IosequencerInput->iosequencer_type << "\" is not one of the available iosequencer types." << std::endl;
            throw std::runtime_error(o.str());
        }
    }

    p_my_iosequencer->setFrom_IosequencerInput(p_current_IosequencerInput);
}


void Workload::build_Eyeos()
{
#if defined(IVYDRIVER_TRACE)
    { workload_callcount_build_Eyeos++; if (workload_callcount_build_Eyeos <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << workloadID << ':' << workload_callcount_build_Eyeos << ") "
        << "            Entering Workload::build_Eyeos()."; log(pWorkloadThread->slavethreadlogfile,o.str()); } }
#endif

    for (auto& pEyeo : allEyeosThatExist) delete pEyeo;

    allEyeosThatExist.clear();

    while (0 < freeStack.size()) freeStack.pop(); // stacks curiously don't hve a clear() method.

    precomputeQ.clear();

    while (postprocessQ.size() > 0) postprocessQ.pop();

    running_IOs.clear();

    cancelled_IOs=0;

    precomputedepth = 2 * p_current_IosequencerInput->maxTags;
    EyeoCount = 2 * (precomputedepth + p_current_IosequencerInput->maxTags);  // the 2x is a postprocess queue allowance

    for (int i=allEyeosThatExist.size() ; i < EyeoCount; i++)
    {
        Eyeo* pEyeo = new Eyeo(this);
        allEyeosThatExist.push_back(pEyeo);
        freeStack.push(pEyeo);
    }
    int rounded_up_to_4KiB_blocksize = p_current_IosequencerInput->blocksize_bytes;

    while (0 != (rounded_up_to_4KiB_blocksize % 4096)) rounded_up_to_4KiB_blocksize++;

    long int buffer_pool_size = 4095 + EyeoCount*rounded_up_to_4KiB_blocksize;

    if (routine_logging)
    {
        std::ostringstream o;
        o << "For workload " << workloadID
            << ", getting I/O buffer memory for Eyeos.  blksize= " << p_current_IosequencerInput->blocksize_bytes
            << ", rounded_up_to_4KiB_blocksize = " << rounded_up_to_4KiB_blocksize << ", number of Eyeos = " << allEyeosThatExist.size()
            << ", memory allocation size for Eyeos with an extra 4095 bytes for alignment padding is " << buffer_pool_size << std::endl;
        log(pWorkloadThread->slavethreadlogfile,o.str());
    }

    try
    {
        ewe_neek.reset( new unsigned char[buffer_pool_size] );
    }
    catch(const std::bad_alloc& e)
    {
        throw std::runtime_error("WorkloadThread.cpp - out of memory making I/O buffer pool - "s + e.what() + "\n"s);
    }

    if (!ewe_neek)
    {
        throw std::runtime_error("<Error> WorkloadThread.cpp out of memory making I/O buffer pool\n");
    }

    void* vp = (void*) ewe_neek.get();

    memset(vp,0x0F,buffer_pool_size);

    uint64_t aio_buf_cursor = (uint64_t) vp;

    while (0 != (aio_buf_cursor % 4096)) aio_buf_cursor++;
        // In the event that the memory we got didn't start on a 4 KiB boundary,
        // we bump up the cursor until we are on a 4 KiB boundary to lay out the first I/O buffer.

    for (auto& pEyeo : allEyeosThatExist)
    {
        pEyeo->eyeocb.aio_buf    = aio_buf_cursor;
        pEyeo->eyeocb.aio_nbytes = p_current_IosequencerInput->blocksize_bytes;

        aio_buf_cursor += rounded_up_to_4KiB_blocksize;
            // successive aio_buf values start on 4 KiB boundaries.
    }

    return;
}



void Workload::switchover()
{
#if defined(IVYDRIVER_TRACE)
    { workload_callcount_switchover++; if (workload_callcount_switchover <= FIRST_FEW_CALLS)
        {
            ivytime n; n.setToNow();
            ivytime seconds_ivytime = n - ivydriver.test_start_time;

            long double cumulative_launches_per_second = ((long double) workload_cumulative_launch_count)
               / seconds_ivytime.getlongdoubleseconds();

            std::ostringstream o;
            o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << workloadID << ':' << workload_callcount_switchover << ") "
                << "            Entering Workload::switchover() - subinterval_number " << subinterval_number
                << " workload_cumulative_launch_count = " << workload_cumulative_launch_count
                << " doing " << cumulative_launches_per_second << " cumulative I/O launches per second"
                << ".";
            log(pWorkloadThread->slavethreadlogfile,o.str());
        }
    }
#endif

    subinterval_array[currentSubintervalIndex].subinterval_status=subinterval_state::ready_to_send;

    if (0 == currentSubintervalIndex)
    {
        currentSubintervalIndex = 1; otherSubintervalIndex = 0;
    } else {
        currentSubintervalIndex = 0; otherSubintervalIndex = 1;
    }
    p_current_subinterval       = & subinterval_array[currentSubintervalIndex];
    p_current_IosequencerInput  = & (p_current_subinterval-> input);
    p_current_SubintervalOutput = & (p_current_subinterval->output);

    subinterval_number++;

    return;
}


unsigned int /* number of I/Os popped and processed.  */
    Workload::pop_and_process_an_Eyeo()
{
#if defined(IVYDRIVER_TRACE)
    { workload_callcount_pop_and_process_an_Eyeo++; if (workload_callcount_pop_and_process_an_Eyeo <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << workloadID << ':' << workload_callcount_pop_and_process_an_Eyeo << ") "
        << "            Entering Workload::pop_and_process_an_Eyeo()."; } }
#endif

	if (postprocessQ.size()==0) return 0;

	// collect I/O statistics / trace data for an I/O
	Eyeo* p_dun;
	p_dun=postprocessQ.front();
	postprocessQ.pop();

	ivy_float service_time_seconds, response_time_seconds, submit_time_seconds, running_time_seconds;

	bool have_response_time;

	if (p_dun->scheduled_time.isZero())
	{  // to avoid confusion, we do not post "application level" response time statistics if iorate==max.
		have_response_time = false;
		response_time_seconds = -1.0;
	}
	else
	{
		have_response_time = true;
		response_time_seconds = p_dun->end_time.getlongdoubleseconds() - p_dun->scheduled_time.getlongdoubleseconds();
	}

	if ( p_dun->start_time.isZero()
	  || p_dun->end_time.isZero()
	  || p_dun->scheduled_time > p_dun->start_time
	  || p_dun->start_time     > p_dun->end_time
	  || (ivydriver.measure_submit_time &&
			   ( p_dun->running_time.isZero()
			  || p_dun->start_time   > p_dun->running_time
			  || p_dun->running_time > p_dun->end_time
			   )
		 )
	   )
    {
        std::ostringstream o;
        o << "When harvesting an I/O completion event and posting its measurements" << std::endl
            << "One of the following timestamps was missing or they were out of order." << std::endl
            << "I/O scheduled time = " << p_dun->scheduled_time.format_as_datetime_with_ns() << std::endl
            << "start time         = " << p_dun->start_time.format_as_duration_HMMSSns() << std::endl
            << "running time       = " << p_dun->running_time.format_as_duration_HMMSSns() << std::endl
            << "end time           = " << p_dun->end_time.format_as_duration_HMMSSns() << std::endl
            << ", measure_submit_time = " << ((ivydriver.measure_submit_time) ? "true" : "false") << std::endl
            << ", have_response_time = ";
        if (have_response_time)
        {
            o << "true, response_time_seconds = " << response_time_seconds ;
        }
        else
        {
            o << "false";
        }
        o << std::endl;
        throw std::runtime_error(o.str());
    }

	service_time_seconds = p_dun->end_time.getlongdoubleseconds() - p_dun->start_time.getlongdoubleseconds();
	if (ivydriver.measure_submit_time) {
		submit_time_seconds = p_dun->running_time.getlongdoubleseconds() - p_dun->start_time.getlongdoubleseconds();
	} else {
		submit_time_seconds = 0.0;
	}
	running_time_seconds = p_dun->end_time.getlongdoubleseconds() - p_dun->running_time.getlongdoubleseconds();

	// NOTE:  The breakdown of bytes_transferred (MB/s) follows service time.


	if ( p_current_IosequencerInput->blocksize_bytes == (unsigned int) p_dun->return_value) {

		int rs, rw, bucket;

		if (p_my_iosequencer->isRandom())
		{
			rs = 0;
		}
		else
		{
			rs = 1;
		}

		if (IOCB_CMD_PREAD==p_dun->eyeocb.aio_lio_opcode)
		{
			rw=0;
		}
		else
		{
			rw=1;
		}

		bucket = Accumulators_by_io_type::get_bucket_index( service_time_seconds );

		p_current_SubintervalOutput->u.a.service_time.rs_array[rs][rw][bucket].push(service_time_seconds);
		p_current_SubintervalOutput->u.a.bytes_transferred.rs_array[rs][rw][bucket].push(p_current_IosequencerInput->blocksize_bytes);

		if (ivydriver.measure_submit_time) {
			bucket = Accumulators_by_io_type::get_bucket_index( submit_time_seconds );
			p_current_SubintervalOutput->u.a.submit_time.rs_array[rs][rw][bucket].push(submit_time_seconds);
		}

		bucket = Accumulators_by_io_type::get_bucket_index( running_time_seconds );
		p_current_SubintervalOutput->u.a.running_time.rs_array[rs][rw][bucket].push(running_time_seconds);

		if (have_response_time)
		{
			bucket = Accumulators_by_io_type::get_bucket_index( response_time_seconds );

			p_current_SubintervalOutput->u.a.response_time.rs_array[rs][rw][bucket].push(response_time_seconds);
		}
	}
	else
	{
		ioerrorcount++;
		if ( p_dun->return_value < 0 )
		{
		    if (p_dun->return_value == -1)
		    {
                std::ostringstream o;
                o << "Thread for physical core " << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread<< "  " << workloadID << " - I/O failed return code = " << p_dun->return_value << ", errno = " << errno << " - " << strerror(errno) << p_dun->toString() << std::endl;

                post_warning(o.str());

                    // %%%%%%%%%%%%%%% come back here later and decide if we want to re-drive failing I/Os, keeping track of the original I/O start time so we record the total respone time for all attempts.
            }
            else
            {
                int err = -p_dun->return_value;
                std::ostringstream o;
                o << "I/O failed return code = " << p_dun->return_value << " (" << strerror(err) << ") " << p_dun->toString() << std::endl;

                post_warning(o.str());
                    // %%%%%%%%%%%%%%% come back here later and decide if we want to re-drive failing I/Os, keeping track of the original I/O start time so we record the total respone time for all attempts.
            }
		}
		else
		{
			std::ostringstream o;
			o << "I/O only transferred " << p_dun->return_value << " out of requested " << p_current_IosequencerInput->blocksize_bytes << std::endl;

            post_warning(o.str());
		}
	}

	workload_cumulative_completion_count++;
	p_dun->completion_sequence_number = workload_cumulative_completion_count;

	if (routine_logging && workload_cumulative_completion_count <= LOG_FIRST_FEW_IO_COMPLETIONS)
	{
	    log(pWorkloadThread->slavethreadlogfile,p_dun->toString(true));
	}


	freeStack.push(p_dun);

#ifdef IVYDRIVER_TRACE
    if (workload_callcount_pop_and_process_an_Eyeo <= FIRST_FEW_CALLS)
    {
        std::ostringstream o;
        o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << workloadID << ") =|= popped and processed " << p_dun->thumbnail();
        log(pWorkloadThread->slavethreadlogfile,o.str());
    }
#endif

	return 1;
}


Eyeo* Workload::front_launchable_IO(const ivytime& now)
{
#if defined(IVYDRIVER_TRACE)
    { workload_callcount_front_launchable_IO++; if (workload_callcount_front_launchable_IO <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << workloadID << ':' << workload_callcount_front_launchable_IO << ") "
        << ")             Entering Workload::front_launchable_IO(" << now.format_as_datetime_with_ns() << ") " << brief_status(); log(pWorkloadThread->slavethreadlogfile,o.str()); }
    pTestLUN->abort_if_queue_depths_corrupted("Workload::front_launchable_IO",workload_callcount_front_launchable_IO); }
#endif

    if (workload_queue_depth > p_current_IosequencerInput->maxTags)
    {
        std::ostringstream o;
        o << "At entry to Workload::front_launchable_IO(const ivytime& now = " << now.format_as_datetime_with_ns()
            << ") for Workload = " << workloadID << " workload_queue_depth = " << workload_queue_depth
            << " is greater than p_current_IosequencerInput->maxTags = " << p_current_IosequencerInput->maxTags
            << "." << std::endl;
        throw std::runtime_error(o.str());
    }

    if (workload_queue_depth == p_current_IosequencerInput->maxTags) return nullptr;

    if (precomputeQ.size() == 0) return nullptr;

    Eyeo* pEyeo = precomputeQ.front();

    if (pEyeo->scheduled_time > now) return nullptr;

#ifdef IVYDRIVER_TRACE
    if (workload_callcount_front_launchable_IO <= FIRST_FEW_CALLS)
    {
        std::ostringstream o;
        o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << workloadID << ") =|= front launchable is  " << pEyeo->thumbnail();
        log(pWorkloadThread->slavethreadlogfile,o.str());
    }
#endif

    return pEyeo;
}


void Workload::load_IOs_to_LaunchPad()
{
#if defined(IVYDRIVER_TRACE)
    { workload_callcount_load_IOs_to_LaunchPad++; if (workload_callcount_load_IOs_to_LaunchPad <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << workloadID << ':' << workload_callcount_load_IOs_to_LaunchPad << ") "
        << "            Entering Workload::load_IOs_to_LaunchPad() " << brief_status(); log(pWorkloadThread->slavethreadlogfile,o.str()); } }
#endif

    if (is_IOPS_max_with_positive_skew() && hold_back_this_time()) return;
        // If there's nobody behind, a workload thread with IOPS=max
        // Then gets to load a "batch" of I/Os, as many as fit within that
        // workload's maxTags setting, thus was thought to be more
        // efficient than doing the merge one I/O at a time.

    ivytime now; now.setToNow();

    if (pTestLUN->launch_count >= MAX_IOS_LAUNCH_AT_ONCE) return;

    unsigned int available_launch_slots = p_current_IosequencerInput->maxTags - workload_queue_depth;

    for (unsigned int i = 0; i < available_launch_slots; i++)
    {
        Eyeo* pEyeo = front_launchable_IO(now);

        if (pEyeo == nullptr) return;

        pTestLUN->pop_front_to_LaunchPad(this);

#ifdef IVYDRIVER_TRACE
        if (workload_callcount_load_IOs_to_LaunchPad <= FIRST_FEW_CALLS)
        {
            std::ostringstream o;
            o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << workloadID << ") =|= loading to LaunchPad " << pEyeo->thumbnail();
            log(pWorkloadThread->slavethreadlogfile,o.str());
        }
#endif

    }

    return;
}

unsigned int Workload::generate_an_IO()
{
#if defined(IVYDRIVER_TRACE)
    { workload_callcount_generate_an_IO++; if (workload_callcount_generate_an_IO <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << workloadID << ':' << workload_callcount_generate_an_IO << ") "
        << "            Entering Workload::generate_an_IO() " << workloadID << "."; log(pWorkloadThread->slavethreadlogfile,o.str()); } }
#endif

    if (precomputeQ.size() >= precomputedepth) return 0;

    if (0.0 == p_current_IosequencerInput->IOPS) return 0;

    ivytime now; now.setToNow();

    if (precomputeQ.size() > 0 && (now + ivytime(PRECOMPUTE_HORIZON_SECONDS)) < precomputeQ.back()->scheduled_time ) return 0; // if we are already that much ahead of now.

    // precompute an I/O
    if (freeStack.empty())
    {
        std::ostringstream o; o << "freeStack is empty - can\'t precompute." << std::endl;
        throw std::runtime_error(o.str());
        // should not occur unless there's a design fault or maybe if CPU-bound
    }

    Eyeo* p_Eyeo = freeStack.top();

    freeStack.pop();

    uint64_t new_pattern_number;
    int bsindex;
    int dedupeunitsvar;


    std::ostringstream o;
    // deterministic generation of "seed" for this I/O.
    if (have_writes)
    {
        if (doing_dedupe)
        {
            switch(subinterval_array[0].input.dedupe_type)
            {
                case dedupe_method::serpentine:
                    serpentine_number += threads_in_workload_name;
                    new_pattern_number = ceil(serpentine_number * serpentine_multiplier);
                    while (pattern_number < new_pattern_number)
                    {
                        xorshift64star(pattern_seed);
                        pattern_number++;
                    }
                    block_seed = pattern_seed ^ pattern_number;
                    break;

                case dedupe_method::target_spread:
                    if (p_my_iosequencer->isRandom())
                    {
                        if (pattern_number > dedupe_target_spread_regulator->pattern_number_reuse_threshold)
                        {
                            if (dedupe_target_spread_regulator->decide_reuse())
                            {
                                ostringstream o;
                                std::pair<uint64_t, uint64_t> align_pattern;
                                align_pattern = dedupe_target_spread_regulator->reuse_seed();
                                pattern_seed = align_pattern.first;
                                pattern_alignment = align_pattern.second;
                                pattern_number = pattern_alignment;
                                o << "workloadthread - Reset pattern seed " << pattern_seed <<  " Offset: " << pattern_alignment << std::endl;
                                //log(pWorkloadThread->slavethreadlogfile, o.str());
                            }
                        }
                    }

                    dedupeunits = p_current_IosequencerInput->blocksize_bytes / 8192;

                    bsindex = 0;
                    dedupeunitsvar = dedupeunits;
                    while (dedupeunitsvar-- > 0)
                    {
                        std::ostringstream o;

                        if (dedupe_count == 0) {
                            modified_dedupe_factor = dedupe_target_spread_regulator->dedupe_distribution();
                            dedupe_count = (uint64_t) modified_dedupe_factor;
#if 0
                            o << "modified_dedupe_factor: " << modified_dedupe_factor << std::endl;
                            //log(pWorkloadThread->slavethreadlogfile,o.str());
#endif
                        }

                        if (dedupe_count == modified_dedupe_factor)
                        {
                            int count = dedupe_count;
                            while (count > 0)
                            {
                                xorshift64star(pattern_seed);
                                pattern_number++;
                                count--;
                            }
                        }
                        block_seed = pattern_seed ^ pattern_number;
                        last_block_seeds[bsindex++] = block_seed;
                        dedupe_count--;
#if 0
                        o << "bsindex: " << bsindex << " dedupe_count: " << dedupe_count << " pattern number: " << pattern_number << " pattern_seed: " << pattern_seed  << " block_seed:" << block_seed << std::endl;
                        log(pWorkloadThread->slavethreadlogfile,o.str());
#endif
                    }
                    break;

                case dedupe_method::constant_ratio:
                case dedupe_method::static_method:

                    // Handled elsewhere, as the seed generation depends on the LBA.

                    break;

                case dedupe_method::invalid:
                default:
                {
                    std::ostringstream o; o << "dedupe_method is invalid - can\'t precompute.";
                    throw std::runtime_error(o.str());

                }
            }
        }
        else
        {
            xorshift64star(block_seed);
        }
    }

#if 0
    o << "pattern number: " << pattern_number << " pattern_seed: " << pattern_seed  << " block_seed:" << block_seed << std::endl;
    log(pWorkloadThread->slavethreadlogfile,o.str());
#endif

    // now need to call the iosequencer function to populate this Eyeo.
    if (!p_my_iosequencer->generate(*p_Eyeo))
    {
        throw std::runtime_error("iosequencer::generate() failed.");
    }
    precomputeQ.push_back(p_Eyeo);

#ifdef IVYDRIVER_TRACE
    if (workload_callcount_generate_an_IO <= FIRST_FEW_CALLS)
    {
        std::ostringstream o;
        o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << workloadID << ") =|= generated " << p_Eyeo->thumbnail();
        log(pWorkloadThread->slavethreadlogfile,o.str());
    }
#endif


    return 1;
}


std::string Workload::brief_status()
{
    std::ostringstream o;
    o << "Workload = "     << workloadID
        << ", preQ = "     << precomputeQ.size()
        << ", running = "  << running_IOs.size()
        << ", postQ = "    << postprocessQ.size()
        << ", WQ = "  << workload_queue_depth
        << ", TLQ = "  << pTestLUN->testLUN_queue_depth
        << ", WTQ = "  << pTestLUN->pWorkloadThread->workload_thread_queue_depth
        << ", WMQ = "  << workload_max_queue_depth
        << ", TLMQ = "  << pTestLUN->testLUN_max_queue_depth
        << ", WTMQ = "  << pTestLUN->pWorkloadThread->workload_thread_max_queue_depth
        << ", I/Os = " <<  workload_cumulative_launch_count
        ;
    return o.str();
}
