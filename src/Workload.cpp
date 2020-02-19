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

#include "ivydriver.h"
#include "IosequencerRandomIndependent.h"
#include "IosequencerRandomSteady.h"
#include "IosequencerSequential.h"

Workload::Workload()
{
    p_current_subinterval      = & (subinterval_array[0]);
    p_current_IosequencerInput = & (subinterval_array[0].input);
    p_current_SubintervalOutput= & (subinterval_array[0].output);
    p_my_iosequencer = nullptr;
    dedupe_target_spread_regulator = nullptr;
    dedupe_constant_ratio_regulator = nullptr;
    dedupe_round_robin_regulator = nullptr;
}

Workload::~Workload()
{
    delete p_my_iosequencer;
    for (auto& pEyeo : allEyeosThatExist) delete pEyeo;
    if (dedupe_target_spread_regulator != nullptr) delete dedupe_target_spread_regulator;
    if (dedupe_constant_ratio_regulator != nullptr) delete dedupe_constant_ratio_regulator;
    if (dedupe_round_robin_regulator != nullptr) delete dedupe_round_robin_regulator;
}


void Workload::prepare_to_run()
{
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

    generate_at_a_time = (unsigned) ceil(ivydriver.generate_at_a_time_multiplier * ((ivy_float) p_current_IosequencerInput->maxTags));
}


void Workload::build_Eyeos(uint64_t& p_buffer)
{
    for (auto& pEyeo : allEyeosThatExist) delete pEyeo;

    allEyeosThatExist.clear();

    while (0 < freeStack.size()) freeStack.pop(); // stacks curiously don't hve a clear() method.

    precomputeQ.clear();

    running_IOs.clear();

    cancelled_IOs=0;

    const unsigned limit = eyeo_build_qty();

    for (unsigned int i = 0 ; i < limit; i++)
    {
        Eyeo* pEyeo = new Eyeo(this,p_buffer);
        allEyeosThatExist.push_back(pEyeo);
        freeStack.push(pEyeo);
    }

    min_freeStack_level = freeStack.size();

    return;
}



void Workload::switchover()
{
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



void Workload::post_Eyeo_result(struct io_uring_cqe* p_cqe, Eyeo* pEyeo, const ivytime& now)
{
    struct io_uring_cqe& cqe = *p_cqe;
    Eyeo&                e   = *pEyeo;
    struct io_uring_sqe& sqe = e.sqe;

    e.end_time     = now;
    e.return_value = cqe.res;
    e.cqe_flags    = cqe.flags;

//*debug*/io_return_code_counts[e.return_value].c++;

    if (ivydriver.track_long_running_IOs)
    {
        auto running_IOs_iter = running_IOs.find(pEyeo);
        if (running_IOs_iter == running_IOs.end())
        {
            std::ostringstream o;
            o << "an asynchrononus I/O completion event occurred for an I/O "
                << "that was not found in the ivy workload\'s \"running_IOs\" (set of pointers to ivy Eyeo objects)."
                << "  The ivy Eyeo object associated with the completion event describes itself as "
                << e << std::endl;
            throw std::runtime_error(o.str());
        }
        running_IOs.erase(running_IOs_iter);
    }

    workload_queue_depth--;

	ivy_float service_time_seconds, response_time_seconds;

	bool have_response_time;

	if (e.scheduled_time.isZero())
	{  // to avoid confusion, we do not post "application level" response time statistics if iorate==max.
		have_response_time = false;
		response_time_seconds = -1.0;
	}
	else
	{
		response_time_seconds = e.end_time.getlongdoubleseconds() - e.scheduled_time.getlongdoubleseconds();

		if (response_time_seconds < 100)
		{
            have_response_time = true;
        }
        else
        {
            have_response_time = false;
            // this happens when the IOPS is set to a value that cannot be achieved.
        }
	}

	service_time_seconds = e.end_time.getlongdoubleseconds() - e.start_time.getlongdoubleseconds();

	// NOTE:  The breakdown of bytes_transferred (MB/s) follows service time.


	if ( p_current_IosequencerInput->blocksize_bytes == (unsigned int) e.return_value) {

		int rs, rw, bucket;

		if (p_my_iosequencer->isRandom())
		{
			rs = 0;
		}
		else
		{
			rs = 1;
		}

		if ( IORING_OP_READ_FIXED == sqe.opcode)
		{
			rw=0;
		}
		else // IORING_OP_WRITE_FIXED
		{
			rw=1;
		}

		try
		{
            bucket = Accumulators_by_io_type::get_bucket_index( service_time_seconds );
		}
        catch (std::invalid_argument& e)
        {
            std::string s = e.what();
            trim(s);

            std::ostringstream o;
            o << s << " - " << (*pEyeo);
            throw std::runtime_error(o.str());
        }

		p_current_SubintervalOutput->u.a.service_time     .rs_array[rs][rw][bucket].push(service_time_seconds);
		p_current_SubintervalOutput->u.a.bytes_transferred.rs_array[rs][rw][bucket].push(p_current_IosequencerInput->blocksize_bytes);

		if (have_response_time)
		{
			bucket = Accumulators_by_io_type::get_bucket_index( response_time_seconds );

			p_current_SubintervalOutput->u.a.response_time.rs_array[rs][rw][bucket].push(response_time_seconds);
		}
	}
	else
	{
		ioerrorcount++;
		if ( e.return_value < 0 )
		{
            int err = -e.return_value;

            WorkloadThread& wt = *pWorkloadThread;

            wt.number_of_IO_errors++;

            if (wt.number_of_IO_errors < wt.IO_error_warning_count_limit)
            {
                std::ostringstream o;
                o << "I/O failed return code = " << e.return_value << " (" << strerror(err) << ") " << e << std::endl;
                post_warning(o.str());
            }
            else if (wt.number_of_IO_errors == wt.IO_error_warning_count_limit)
            {
                std::ostringstream o;
                o << "The maximum number of I/O errors for which a detailed <Warning> is printed is " << (wt.IO_error_warning_count_limit - 1)
                    << ".  This limit has been exceeded and no further detailed I/O error <Warnings> will be issued." << std::endl;
                post_warning(o.str());
            }
                // %%%%%%%%%%%%%%% come back here later and decide if we want to re-drive failing I/Os, keeping track of the original I/O start time so we record the total respone time for all attempts.
		}
		else
		{
			std::ostringstream o;
			o << "I/O only transferred " << e.return_value << " out of requested " << p_current_IosequencerInput->blocksize_bytes << std::endl;

            post_warning(o.str());
		}
	}

	workload_cumulative_completion_count++;
	e.completion_sequence_number = workload_cumulative_completion_count;

	if (routine_logging && workload_cumulative_completion_count <= LOG_FIRST_FEW_IO_COMPLETIONS)
	{
        bool save = ivydriver.display_buffer_contents;

        ivydriver.display_buffer_contents = true;

        std::ostringstream o; o << e; log(pWorkloadThread->slavethreadlogfile,o.str());

	    ivydriver.display_buffer_contents = save;
	}

	freeStack.push(pEyeo);

	return;
}


Eyeo* Workload::front_launchable_IO(const ivytime& now)
{
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

    return pEyeo;
}


unsigned int Workload::populate_sqes()
{
    if (is_IOPS_max_with_positive_skew() && hold_back_this_time()) return 0;
        // If there's nobody behind, a workload thread with IOPS=max
        // Then gets to load a "batch" of I/Os, as many as fit within that
        // workload's maxTags setting, thus was thought to be more
        // efficient than doing the merge one I/O at a time.

    ivytime now; now.setToNow();

    TestLUN& tl = *pTestLUN;

    WorkloadThread& wt = *tl.pWorkloadThread;

    unsigned int total = 0;

    while (true)
    {
        Eyeo* pEyeo = front_launchable_IO(now);

        if (pEyeo == nullptr) { return total; } // No ready to launch I/Os.

        struct io_uring_sqe* p_sqe = pWorkloadThread->get_sqe(); // This increments the number of sqes queued for submit

        if (p_sqe == nullptr) { return total; } // No ready to launch I/Os.

        if ( wt.workload_thread_queue_depth /* includes wt.sqes_queued_for_submit) */ >= wt.workload_thread_queue_depth_limit )
        {
            if (!wt.have_warned_hitting_cqe_entry_limit)
            {
                wt.have_warned_hitting_cqe_entry_limit = true;

                std::ostringstream o;
                o << "have reached the workload_thread_queue_depth_limit value being " << wt.workload_thread_queue_depth_limit
                    << " : workload_thread_cqe_entrie.";
                wt.post_warning("");
            }
        }

        workload_queue_depth++;

        if (workload_max_queue_depth < workload_queue_depth) { workload_max_queue_depth = workload_queue_depth; }

        total++;

        precomputeQ.pop_front();

        (*p_sqe) = pEyeo->sqe; // default copy assignment operators should be fine.

        workload_cumulative_launch_count++;

        if (ivydriver.track_long_running_IOs) running_IOs.insert(pEyeo);

        pEyeo->start_time=now;
    }

    return total;
}


unsigned int Workload::generate_IOs()
{
    bool trace {false};

    if(trace) {WorkloadThread& wt = *pWorkloadThread; if (wt.debug_c++ < wt.debug_m){std::ostringstream o; o << "Entry to Workload::generate_IOs() for " << workloadID; log(wt.slavethreadlogfile, o.str());}}

    unsigned int generated_qty {0};

    const unsigned limit = precomputedepth();

    while (generated_qty < generate_at_a_time)
    {
        if (precomputeQ.size() >= limit) { return generated_qty; }

        if ( p_current_IosequencerInput->IOPS == 0.0 ) { return generated_qty; }

        ivytime now; now.setToNow();

        if (precomputeQ.size() > 0 && (now + ivytime(PRECOMPUTE_HORIZON_SECONDS)) < precomputeQ.back()->scheduled_time ) { return generated_qty; }

        // precompute an I/O
        if (freeStack.empty())
        {
            WorkloadThread& wt = *pWorkloadThread;

            std::ostringstream o; o << "Workload::generate_IOs() for " << workloadID << " - freeStack is empty - can\'t precompute." << std::endl;
            log(wt.slavethreadlogfile, o.str());
            wt.post_error(o.str());
        }

        pWorkloadThread->did_something_this_pass = true; // generated an I/O

        Eyeo* p_Eyeo = freeStack.top();

        freeStack.pop();

        if (min_freeStack_level > freeStack.size()) { min_freeStack_level = freeStack.size(); }

        uint64_t new_pattern_number;
        int bsindex;
        int dedupeunitsvar;

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
                        }
                        break;

                	case dedupe_method::constant_ratio:
	                case dedupe_method::static_method:
	                case dedupe_method::round_robin:

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
        // now need to call the iosequencer function to populate this Eyeo.
        if (!p_my_iosequencer->generate(*p_Eyeo))
        {
            WorkloadThread& wt = *pWorkloadThread;

            std::ostringstream o; o << "Workload::generate_IOs() for " << workloadID << " - iosequencer::generate(->Eyeo) failed." << std::endl;
            log(wt.slavethreadlogfile, o.str());
            wt.post_error(o.str());
        }
        precomputeQ.push_back(p_Eyeo);
        generated_qty++;
    }

    return generated_qty;
}


std::string Workload::brief_status()
{
    std::ostringstream o;
    o << "Workload = "     << workloadID
        << ", preQ = "     << precomputeQ.size()
        << ", running = "  << running_IOs.size()
        << ", WQ = "  << workload_queue_depth
        << ", WTQ = "  << pTestLUN->pWorkloadThread->workload_thread_queue_depth
        << ", WMQ = "  << workload_max_queue_depth
        << ", WTMQ = "  << pTestLUN->pWorkloadThread->workload_thread_max_queue_depth
        << ", I/Os = " <<  workload_cumulative_launch_count
        ;
    return o.str();
}
