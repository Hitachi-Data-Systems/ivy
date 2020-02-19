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

#include "ivydriver.h"

extern bool routine_logging;

pid_t get_subthread_pid()
{
    return syscall(SYS_gettid);
}

std::ostream& operator<< (std::ostream& o, const ThreadState& s)
{
    switch (s)
    {

        case ThreadState::undefined:
            o << "ThreadState::undefined";
            break;
        case ThreadState::initial:
            o << "ThreadState::initial";
            break;
        case ThreadState::waiting_for_command:
            o << "ThreadState::waiting_for_command";
            break;
        case ThreadState::running:
            o << "ThreadState::running";
            break;
        case ThreadState::died:
            o << "ThreadState::died";
            break;
        case ThreadState::exited_normally:
            o << "ThreadState::exited_normally";
            break;
        default:
            o << "<internal programming error - unknown ThreadState value cast to int as " << ((int) s) << ">";
    }
    return o;
}


std::ostream& operator<< (std::ostream& o, const cqe_type& ct)
{
    switch (ct)
    {
        case cqe_type::eyeo:
            o << "cqe_type::eyeo";
            break;
        case cqe_type::timeout:
            o << "cqe_type::timeout";
            break;
        default:
            o << "<internal programming error - unknown cqe_type value cast to int as " << ((int) ct) << ">";
    }
    return o;
}


std::ostream& operator<<(std::ostream& o, const struct cqe_shim& s)
{
    o << "{ \"struct cqe_shim\" : { ";
    o << "\"this\" : \"" << &s << "\"";
    o << "\", type\" : \"" << s.type << "\"";

    struct timespec ts;
    switch (s.type)
    {
        case cqe_type::timeout:
            o << ", \"wait_until_this_time\" : \"" << s.wait_until_this_time.format_as_datetime_with_ns() << "\"";

            ts.tv_sec  = s.kts.tv_sec;
            ts.tv_nsec = s.kts.tv_nsec;
            o << ", \"duration\" : \"" << ivytime(ts).format_as_duration_HMMSSns() << "\"";

            o << ", \"when_inserted\" : \"" << s.when_inserted.format_as_datetime_with_ns() << "\"";

            break;

        default: ;
    }

    o << " } }";

    return o;
}


struct cqe_shim* timeout_shims::insert_timeout(const ivytime& then, const ivytime& now)
{
    if (then.isZero() || now.isZero() || then <= now)
    {
        std::ostringstream o;
        o << "<Error> timeout_shims::insert_timeout(const ivytime& then = " << then.format_as_datetime_with_ns()
            << ", const ivytime& now = " << now.format_as_datetime_with_ns() << ")"
            << " - Neither \"then\" nor \"now\" may be zero, and \"then\" must be after \"now\".";
        throw std::runtime_error(o.str());
    }

    struct cqe_shim* p_shim = new struct cqe_shim;

    if (p_shim == nullptr) {throw std::runtime_error("<Error> internal programming error - timeout_shims::insert_timeout() - failed getting memory for cqe_shim.");}

    p_shim->type = cqe_type::timeout;
    p_shim->wait_until_this_time = then;
    p_shim->when_inserted = now;

    unexpired_shims.insert(p_shim);

    return p_shim;
}

ivytime timeout_shims::earliest_timeout()
{
    ivytime earliest {0};

    for ( cqe_shim* p_shim : unexpired_shims )
    {
        if (earliest.isZero() || p_shim->wait_until_this_time < earliest) { earliest = p_shim->wait_until_this_time; }
    }

    if ( (!unsubmitted_timeout.isZero()) && (earliest.isZero() || unsubmitted_timeout < earliest)) { earliest = unsubmitted_timeout; }

    return earliest;
}

void timeout_shims::delete_shim(struct cqe_shim* p_shim)
{
    auto it = unexpired_shims.find(p_shim);

    if (it == unexpired_shims.end()) throw std::runtime_error ("failed in timeout_shims::delete_shim() - shim not found in unexpired_time_shims.");

    unexpired_shims.erase(it);

    delete p_shim;

    return;
}

WorkloadThread::WorkloadThread(std::mutex* p_imm, unsigned int pc, unsigned int h)
    : p_ivydriver_main_mutex(p_imm), physical_core(pc), hyperthread(h) {}

std::string threadStateToString (const ThreadState ts)
{
	std::ostringstream o;
    o << ts;
	return o.str();
}

std::string mainThreadCommandToString(MainThreadCommand mtc)
{
	switch (mtc)
	{
		case MainThreadCommand::start:      return "MainThreadCommand::start";
		case MainThreadCommand::keep_going: return "MainThreadCommand::keep_going";
		case MainThreadCommand::stop:       return "MainThreadCommand::stop";
		case MainThreadCommand::die:        return "MainThreadCommand::die";
		default:
        {
            std::ostringstream o;
            o << "mainThreadCommandToString (MainThreadCommand = (decimal) " << ((int) mtc) << ") - internal programming error - invalid MainThreadCommand value." ;
            return o.str();
        }
	}
}

void WorkloadThread::WorkloadThreadRun()
{
	if (routine_logging)
	{
		std::ostringstream o;

		o<< "WorkloadThreadRun() fireup for physical core " << physical_core << " hyperthread " << hyperthread << "." << std::endl;

		log(slavethreadlogfile,o.str());
	}
    my_pid = getpid();
    my_tid = get_subthread_pid();
    my_pgid = getpgid(my_tid);

    if (routine_logging)
    {
        std::ostringstream o;
        o << "getpid() = " << my_pid << ", gettid() = " << my_tid << ", getpgid(my_tid=" << my_tid << ") = " << my_pgid << std::endl;
        log(slavethreadlogfile,o.str());
    }

//   The workload thread is initially in "waiting for command" state.
//
//   In the outer "waiting for command" loop we wait indefinitely for
//   ivydriver to set "ivydriver_main_posted_command" to the value true.
//
//   Before turning on the "ivydriver_main_posted_command" flag and notifying,
//   ivydriver sets the actual command value into a MainThreadCommand enum class
//   variable called "ivydriver_main_says".
//
//   The MainThreadCommand enum class has four values, "start", "keep_going", "stop", and "die".
//
//       When the workload thread wakes up and sees that ivydriver_main_posted_command == true, we first turn it off for next time.
//
//       In this outer loop, there are only two commands that are recognized
//       "die" (nicely), and "start" the first subinterval running.
//
//       When we get the "start" command, first we (re) initialize stuff
//       and then we start the first subinterval running.
//
//           At the end of every running subinterval, we need to switch over to the
//           next one pretty much instantly.
//
//           When a running subinterval ends, we look for up to 15 ms
//           to see either the command "keep_running", "stop", or "die".
//
//           "stop" means :
//                 You just finished the last subinterval.
//                 Don't run another subinterval.
//                 Do wrap up activities, possibly write log records,
//                 and then loop back to the top of the outer "waiting for command" loop.
//
//           "keep_running" means :
//                 Mark this subinterval as "ready to send"
//                 Switch over to the other subinterval, which must already be marked "ready_to_run".
//                 Loop back to the top of the inner running subinterval loop.

wait_for_command:  // the "stop" command finishes by "goto wait_for_command". This to make the code easy to read compared to break statements in nested loops.

	state = ThreadState::waiting_for_command;

	slaveThreadConditionVariable.notify_all();   // ivydriver waits for the thread to be in waiting for command state.

	while (true) // loop that waits for "run" commands
	{
        // indent level in loop waiting for run commands

		{
		    // runs with lock held

			std::unique_lock<std::mutex> wkld_lk(slaveThreadMutex);
			while (!ivydriver_main_posted_command) slaveThreadConditionVariable.wait(wkld_lk);

			if (routine_logging) log(slavethreadlogfile,std::string("Received command from host - ") + mainThreadCommandToString(ivydriver_main_says));

			ivydriver_main_posted_command=false;

			if (reported_error && ivydriver_main_says != MainThreadCommand::die)
			{
			    std::ostringstream o;
			    o << "ignoring command \"" << mainThreadCommandToString(ivydriver_main_says) << "\" from ivydriver main thread "
			        << "because I prevously reported an error.  Exiting instead.";
                post_error(o.str());
                state=ThreadState::died;
                wkld_lk.unlock();
                slaveThreadConditionVariable.notify_all();
                return;
			}

			switch (ivydriver_main_says)
			{
				case MainThreadCommand::die :
				{
					if (routine_logging) log (slavethreadlogfile,std::string("WorkloadThread dutifully dying upon command.\n"));
					if (reported_error) { state=ThreadState::died; }
					else                { state=ThreadState::exited_normally; }
					wkld_lk.unlock();
					slaveThreadConditionVariable.notify_all();
					return;
				}
				case MainThreadCommand::start :
				{
					break;
				}
				default:
				{
                    post_error("in main loop waiting for commands but didn\'t get \"start\" or \"die\" when expected."s
                        + "  Instead got "s + mainThreadCommandToString(ivydriver_main_says) + "  WorkloadThread exiting."s);
                    state=ThreadState::died;
                    wkld_lk.unlock();
                    slaveThreadConditionVariable.notify_all();
                    return;
                }
			}
		}  // lock released

		// "start" command received.

		number_of_concurrent_timeouts.clear();

		slaveThreadConditionVariable.notify_all();

        thread_view_subinterval_number = -1;

//*debug*/ { std::ostringstream o; o << "b4 build uring" << std::endl; log(slavethreadlogfile,o.str());}

        build_uring_and_allocate_and_mmap_Eyeo_buffers_and_build_Eyeos();
//*debug*/ { std::ostringstream o; o << "after build uring" << std::endl; log(slavethreadlogfile,o.str());}

		step_run_time_seconds = 0.0;

		for (auto& pTestLUN : pTestLUNs)
		{
		    try
		    {
                pTestLUN->testLUN_furthest_behind_weighted_IOPS_max_skew_progress = 0.0;

                for (auto& pear: pTestLUN->workloads)
                {
//*debug*/pear.second.io_return_code_counts.clear();
                    pear.second.iops_max_io_count = 0;
                    pear.second.currentSubintervalIndex=0;
                    pear.second.otherSubintervalIndex=1;
                    pear.second.subinterval_number=0;
                    pear.second.prepare_to_run();
                    //pear.second.build_Eyeos();
                    pear.second.workload_cumulative_launch_count = 0;
                    pear.second.workload_weighted_IOPS_max_skew_progress = 0.0;

                    if (pear.second.dedupe_target_spread_regulator != nullptr) { delete pear.second.dedupe_target_spread_regulator; }

                    if (pear.second.p_current_IosequencerInput->dedupe > 1.0 && pear.second.p_current_IosequencerInput->fractionRead != 1.0  && pear.second.p_current_IosequencerInput->dedupe_type == dedupe_method::target_spread)
                    {
                        pear.second.dedupe_target_spread_regulator = new DedupeTargetSpreadRegulator(pear.second.subinterval_array[0].input.dedupe, pear.second.pattern_seed);
                        if (pear.second.p_my_iosequencer->isRandom())
                        {
                            if (pear.second.dedupe_target_spread_regulator->decide_reuse())
                            {
                                std::pair<uint64_t, uint64_t> align_pattern;
                                align_pattern = pear.second.dedupe_target_spread_regulator->reuse_seed();
                                pear.second.pattern_seed = align_pattern.first;
                                pear.second.pattern_alignment = align_pattern.second;
                                pear.second.pattern_number = pear.second.pattern_alignment;
                            } else
                            {
                                pear.second.pattern_seed = pear.second.dedupe_target_spread_regulator->random_seed();
                            }
                        }
                    }
                    //log(slavethreadlogfile, pear.second.dedupe_regulator->logmsg());

                    if (pear.second.dedupe_constant_ratio_regulator != nullptr) { delete pear.second.dedupe_constant_ratio_regulator; pear.second.dedupe_constant_ratio_regulator = nullptr;}
                    if (pear.second.p_current_IosequencerInput->dedupe > 1.0 && pear.second.p_current_IosequencerInput->fractionRead != 1.0 &&
                        pear.second.p_current_IosequencerInput->dedupe_type == dedupe_method::constant_ratio)
                    {
                        pear.second.dedupe_constant_ratio_regulator = new DedupeConstantRatioRegulator(pear.second.p_current_IosequencerInput->dedupe,
                                                                                                       pear.second.p_current_IosequencerInput->blocksize_bytes,
                                                                                                       pear.second.p_current_IosequencerInput->fraction_zero_pattern,
                                                                                                       pear.second.uint64_t_hash_of_host_plus_LUN);
                    }

                    if (pear.second.dedupe_round_robin_regulator != nullptr) { delete pear.second.dedupe_round_robin_regulator; pear.second.dedupe_round_robin_regulator = nullptr;}
                    if (pear.second.p_current_IosequencerInput->dedupe > 1.0 && pear.second.p_current_IosequencerInput->fractionRead != 1.0 &&
                        pear.second.p_current_IosequencerInput->dedupe_type == dedupe_method::round_robin)
                    {
                        pear.second.dedupe_round_robin_regulator = new DedupeRoundRobinRegulator(
                        		pear.second.p_my_iosequencer->numberOfCoverageBlocks,
                                pear.second.p_current_IosequencerInput->blocksize_bytes,
                        		pear.second.p_current_IosequencerInput->dedupe,
								pear.second.p_current_IosequencerInput->dedupe_unit_bytes,
								pear.second.p_my_iosequencer->pLUN);
                    }
                }
            }
            catch (std::exception& e)
            {
                std::ostringstream o;
                o << "failed preparing TestLUNs and Workloads to start running - "
                    << e.what() << "." << std::endl;
                log(slavethreadlogfile, o.str());
                post_error(o.str());
                goto wait_for_command;
            }
		}
//*debug*/ { std::ostringstream o; o << "aye" << std::endl; log(slavethreadlogfile,o.str());}

		dispatching_latency_seconds.clear();
        lock_aquisition_latency_seconds.clear();
        switchover_completion_latency_seconds.clear();

        set_all_queue_depths_to_zero();

        number_of_IO_errors = 0;

        // prefill precompute queues

        for (TestLUN* pTestLUN : pTestLUNs)
        {
            for (auto& pear : pTestLUN->workloads)
            {
                {
                    Workload& w = pear.second;

                    unsigned int number_generated;

                    do { number_generated = w.generate_IOs(); }
                    while (number_generated > 0);
                }
            }
        }


        if (routine_logging)
        {
            std::ostringstream o;
            o << "Thread for physical core " << physical_core << " hyperthread " << hyperthread << std::endl;
            log(slavethreadlogfile,o.str());
        }

//*debug*/ { std::ostringstream o; o << "eye" << std::endl; log(slavethreadlogfile,o.str());}

        for (auto& pTestLUN : pTestLUNs)
        {
            try { pTestLUN->prepare_linux_AIO_driver_to_start(); }
            catch (std::exception& e)
            {
                std::ostringstream o;
                o << "failed running prepare_linux_AIO_driver_to_start() for " << pTestLUN->host_plus_lun << " - "
                    << e.what() << "." << std::endl;
                post_error(o.str());
                goto wait_for_command;
            }
        }

		state=ThreadState::running;

//*debug*/ { std::ostringstream o; o << "bye" << std::endl; log(slavethreadlogfile,o.str());}


        if (routine_logging) { log(slavethreadlogfile,"Finished initialization and now about to start running subintervals."); }

        // indent level in loop waiting for run commands

        while (true) // inner loop running subintervals, until we are told "stop" or "die"
        {
            // indent level in loop running subintervals

            thread_view_subinterval_number++;

            if (thread_view_subinterval_number == 0)
            {
                thread_view_subinterval_start = ivydriver.test_start_time;
            }
            else
            {
                thread_view_subinterval_start = thread_view_subinterval_end;
            }
            thread_view_subinterval_end = thread_view_subinterval_start + ivydriver.subinterval_duration;

            if (routine_logging){
                std::ostringstream o;
                o << "Physical core " << physical_core << " hyperthread " << hyperthread << ": Top of subinterval " << thread_view_subinterval_number
                << " " << thread_view_subinterval_start.format_as_datetime_with_ns()
                << " to " << thread_view_subinterval_end.format_as_datetime_with_ns()
                << " with subinterval_duration = " << ivydriver.subinterval_duration.format_as_duration_HMMSSns()
                << std::endl;
                    log(slavethreadlogfile,o.str());
            }

//            // This nested loop to set hot zone parameters.
//            for (auto& pTestLUN : pTestLUNs)
//            {
//                for (auto& peach : pTestLUN->workloads)
//                {
//                    {
//                        Workload& wrkld = peach.second;
//
//                        if (wrkld.p_current_IosequencerInput->hot_zone_size_bytes > 0)
//                        {
//                            if (wrkld.p_my_iosequencer->instanceType() == "random_steady"
//                             || wrkld.p_my_iosequencer->instanceType() == "random_independent")
//                            {
//                                IosequencerRandom* p = (IosequencerRandom*) wrkld.p_my_iosequencer;
//                                if (!p->set_hot_zone_parameters(wrkld.p_current_IosequencerInput))
//                                {
//                                    std::ostringstream o;
//                                    o << " - working on WorkloadID " << wrkld.workloadID.workloadID
//                                        << " - call to IosequencerRandom::set_hot_zone_parameters(() failed.\n";
//                                    post_error(o.str());
//                                    goto wait_for_command;
//                                }
//                            }
//                        }
//                    }
//                }
//            }

            try
            {
                run_subinterval();
            }
            catch (std::exception& e)
            {
                std::ostringstream o;
                o << "failed running run_subinterval() - "
                    << e.what() << "." << std::endl;
                post_error(o.str());
                goto wait_for_command;
            }

            // end of subinterval, flip over to other subinterval or stop

            // First record the dispatching latency,
            // the time from when the clock clicked over to a new subinterval
            // to the time that the WorkloadThread code starts running.

//*debug*/ { std::ostringstream o; o << "back in WorkloadThreadRun() after running subinterval." << std::endl; log(slavethreadlogfile,o.str());}


            ivytime before_getting_lock, dispatch_time;
            before_getting_lock.setToNow();

            if ( pTestLUNs.size() == 0)
            {
                dispatching_latency_seconds.push(0.0);
            }
            else
            {
                switchover_begin = ivydriver.ivydriver_view_subinterval_end;

                dispatch_time = before_getting_lock - ivydriver.ivydriver_view_subinterval_end;

                dispatching_latency_seconds.push(dispatch_time.getlongdoubleseconds());
            }

            if (routine_logging)
            {
                std::ostringstream o;
                o << "Physical core " << physical_core << " hyperthread " << hyperthread << ": Bottom of subinterval " << thread_view_subinterval_number
                    << " " << thread_view_subinterval_start.format_as_datetime_with_ns()
                    << " to " << thread_view_subinterval_end.format_as_datetime_with_ns() << std::endl;
//*debug*/for (auto& pTestLUN:pTestLUNs){for (auto& pear : pTestLUN->workloads) {Workload& w = pear.second; for (auto& q : w.io_return_code_counts ){ o << w.workloadID << "I/O return code " << q.first << " occurred " << q.second.c << " times."<< std::endl;}}}
                log(slavethreadlogfile,o.str());
            }

         // indent level in loop waiting for run commands
            // indent level in loop running subintervals

            {
                // start of locked section at subinterval switchover
                std::unique_lock<std::mutex> wkld_lk(slaveThreadMutex);

//*debug*/ { std::ostringstream o; o << "Got lock to look at command which is " << mainThreadCommandToString(ivydriver_main_says) << "." << std::endl; log(slavethreadlogfile,o.str()); }

                ivytime got_lock_time;  got_lock_time.setToNow();

                ivytime lock_aquire_time = got_lock_time - before_getting_lock;

                lock_aquisition_latency_seconds.push(lock_aquire_time.getlongdoubleseconds());

                ivydriver_main_posted_command=false; // for next time

                if (ivydriver.track_long_running_IOs) { check_for_long_running_IOs(); }

                // we still have the lock

                if (MainThreadCommand::die == ivydriver_main_says)
                {
                    if (routine_logging) log(slavethreadlogfile,"WorkloadThread dutifully dying upon command.\n");
                    state=ThreadState::exited_normally;
                    wkld_lk.unlock();
                    slaveThreadConditionVariable.notify_all();
                    return;
                }

                // we still have the lock during subinterval switchover

                for (auto& pTestLUN : pTestLUNs)
                {
                    for (auto& peach : pTestLUN->workloads)
                    {
                        try
                        {
                            peach.second.switchover();
                        }
                        catch (std::exception& e)
                        {
                            std::ostringstream o;
                            o << "failed switcover to next subinterval for " << pTestLUN->host_plus_lun <<" - "
                                << e.what() << "." << std::endl;
                            post_error(o.str());
                            goto wait_for_command;
                        }
                    }
                }

        // indent level in loop waiting for run commands
            // indent level in loop running subintervals
                // indent level in locked section at subinterval switchover

                if (routine_logging)
                {
                    std::ostringstream o;

                    o << std::endl;

                    for (auto& pTestLUN : pTestLUNs)
                    {
                        for (auto& peach : pTestLUN->workloads)
                        {
                            {
                                const Workload& w = peach.second;

                                unsigned min_required_IOs = w.precomputedepth() + w.subinterval_array[0].input.maxTags;

                                unsigned max_used_Eyeos = w.eyeo_build_qty() - w.min_freeStack_level;

                                o << w.workloadID
                                    << " min_required_IOs = ( precomputedepth() + maxTags ) = " << min_required_IOs
                                    << ", max_used_Eyeos = " << max_used_Eyeos
                                    << ", max_design_spare_Eyeos_used = "; if (max_used_Eyeos > min_required_IOs) { o << (max_used_Eyeos > min_required_IOs); } else { o << "<none>"; }
                                o   << ", min_freeStack_level = " << w.min_freeStack_level
                                    << ", eyeo_build_qty() = " << w.eyeo_build_qty()
                                    << ", precompute_depth() = " << w.precomputedepth()
                                    << ", maxTags = " << w.subinterval_array[0].input.maxTags
                                    << "." << std::endl;
                            }
                        }
                    }

                    log(slavethreadlogfile, o.str());
                }

                if (MainThreadCommand::stop == ivydriver_main_says)
                {
                    if (routine_logging) log(slavethreadlogfile,std::string("WorkloadThread stopping at end of subinterval.\n"));

                    catch_in_flight_IOs_after_last_subinterval();

                    if (routine_logging)
                    {
                        {
                            std::ostringstream o;
                            o << "Subinterval count = "  << dispatching_latency_seconds.count() << std::endl
                                << "OS dispatching latency after subinterval end, seconds: "
                                << " avg = " << dispatching_latency_seconds.avg()
                                << " / min = " << dispatching_latency_seconds.min()
                                << " / max = " << dispatching_latency_seconds.max()
                                << std::endl
                                << "lock acquisition latency: "
                                << " avg = " << lock_aquisition_latency_seconds.avg()
                                << " / min = " << lock_aquisition_latency_seconds.min()
                                << " / max = " << lock_aquisition_latency_seconds.max()
                                << std::endl
                                << "complete switchover latency: "
                                << " avg = " << switchover_completion_latency_seconds.avg()
                                << " / min = " << switchover_completion_latency_seconds.min()
                                << " / max = " << switchover_completion_latency_seconds.max()
                                << std::endl;
                            log(slavethreadlogfile,o.str());
                        }

                        {
                            std::ostringstream o;

                            o << "workload_thread_max_queue_depth = " << workload_thread_max_queue_depth << std::endl;

                            for (auto& pTestLUN : pTestLUNs)
                            {
                                o << "For TestLUN" << pTestLUN->host_plus_lun << " : " << std::endl;

                                for (auto& pear : pTestLUN->workloads)
                                {
                                    o << "For Workload" << pear.first << " : "
                                        << "workload_max_queue_depth = " << pear.second.workload_max_queue_depth << std::endl;

                                }
                            }

                            log(slavethreadlogfile,o.str());
                        }

                        log_io_uring_engine_stats();
                    }

                    if (routine_logging && !ivydriver.spinloop)
                    {
                        std::ostringstream o;
                        o << "number_of_concurrent_timeouts: "
                            << "count() = " << number_of_concurrent_timeouts.count()
                            << ", avg() = "   << number_of_concurrent_timeouts.avg()
                            << ", max() = "   << number_of_concurrent_timeouts.max()
                            << ".";
                        log(slavethreadlogfile,o.str());
                    }

//*debug*/{std::ostringstream o; o << "about to shutdown_uring_and_unmap_Eyeo_buffers_and_delete_Eyeos()."; log(slavethreadlogfile,o.str());}

                    shutdown_uring_and_unmap_Eyeo_buffers_and_delete_Eyeos();

//*debug*/{std::ostringstream o; o << "about to turn off ivydriver_main_posted_command, go into waiting_for_command state, unlock, notify all ..."; log(slavethreadlogfile,o.str());}

                    ivydriver_main_posted_command = false;

                    state=ThreadState::waiting_for_command;
                    wkld_lk.unlock();
                    slaveThreadConditionVariable.notify_all();

                    goto wait_for_command;

                    // end of processing "stop" command
                }

        // indent level in loop waiting for run commands
            // indent level in loop running subintervals
                // indent level in locked section at subinterval switchover

				if (MainThreadCommand::keep_going != ivydriver_main_says)
				{
				    std::ostringstream o; o << " WorkloadThread only expects \"stop\" or \"keep_going\" commands at end of subinterval.  Received "
				        << mainThreadCommandToString(ivydriver_main_says) << ".\n";
					wkld_lk.unlock();
                    post_error(o.str());
                    goto wait_for_command;
				}

//*debug*/{std::ostringstream o; o << "processing keep_going command."; log(slavethreadlogfile,o.str());}

                // MainThreadCommand::keep_going

				for (auto& pTestLUN : pTestLUNs)
				{
				    for (auto& pear : pTestLUN->workloads)
				    {
				        Workload* pWorkload = & pear.second;
                        if ( pWorkload->subinterval_array[pWorkload->currentSubintervalIndex].subinterval_status != subinterval_state::ready_to_run )
                        {
                            std::ostringstream o; o << "WorkloadThread told to keep going, but next subinterval not marked READY_TO_RUN"
                                << " for workload " << pWorkload->workloadID.workloadID
                                << "  Master host late to post command, or has stopped.  Try increasing subinterval_seconds." << std::endl;
                            wkld_lk.unlock();
                            post_error(o.str());
                            goto wait_for_command;
                        }

                        pWorkload->p_current_subinterval->subinterval_status = subinterval_state::running;

                        // end of locked section
                    }
				}
			}

        // indent level in loop waiting for run commands
            // indent level in loop running subintervals
                // indent level in locked section at subinterval switchover

            ivytime switchover_complete;
            switchover_complete.setToNow();
            switchover_completion_latency_seconds.push(ivytime(switchover_complete-switchover_begin).getlongdoubleseconds());
			slaveThreadConditionVariable.notify_all();
//*debug*/{std::ostringstream o; o << "switchover complete."; log(slavethreadlogfile,o.str());}
		}
	}
}

void WorkloadThread::run_subinterval()
{
//*debug*/short_submit_counter_map.clear(); dropped_ocurrences_map.clear();

    ivytime now; now.setToNow();

    if (routine_logging)
    {
        std::ostringstream o;
        now = now - thread_view_subinterval_end;
        o << "WorkloadThread::run_subinterval() running subinterval which will end at "
            << thread_view_subinterval_end.format_as_datetime_with_ns()
            << " - " << (thread_view_subinterval_end-now).format_as_duration_HMMSSns() << " from now " << std::endl;
        log(slavethreadlogfile,o.str());
    }

    try_generating_IOs = true;
        // The idea here is that if there's no new information since last time
        // and last time there wasn't anything to do, and more than 1/2 second
        // has passed, well then there's still nothing to do.

        // The one half second is in case there is room in the precompute queue,
        // but the most recently generated was for a time too far into the future.

        // Called PRECOMPUTE_HORIZON_SECONDS, set at time of writing this to 0.5 seconds


    ivytime too_far_in_the_future (PRECOMPUTE_HORIZON_SECONDS/2.0);
        // that were coming for very low I/O rate workloads.

    consecutive_fruitless_passes = 0;

//*debug*/unsigned torque = 0;

	while (true)
	{
        cumulative_loop_passes ++;

//*debug*/if (++torque == 100) { std::ostringstream o; o << " %%%%%%%%%%%%%% 100th pass %%%%%%%%%%%%%%%%%%"; log(slavethreadlogfile,o.str()); }

//*debug*/if (cumulative_loop_passes % 10000 == 0) { std::ostringstream o; o << " ||||||||||||||============== cumulative_loop_passes = " << cumulative_loop_passes << "."; log(slavethreadlogfile,o.str()); }

	    did_something_this_pass = false;

	    have_failed_to_get_an_sqe = false;

		now.setToNow();

        // first we harvest any pending completion events - doesn't use any system calls.

        struct io_uring_cqe* p_cqe {nullptr};

        unsigned total_cqes_reaped_this_pass = 0;

        unsigned cqe_harvest_count = 0;

        do
        {
            cqe_harvest_count = io_uring_peek_batch_cqe(&struct_io_uring, cqe_pointer_array, cqes_with_one_peek_limit);

            total_cqes_reaped_this_pass += cqe_harvest_count;
            cumulative_cqes             += cqe_harvest_count;
            workload_thread_queue_depth -= cqe_harvest_count;

            if (cqe_harvest_count > 0)
            {
                if (cqe_harvest_count > max_cqes_reaped_at_once) { max_cqes_reaped_at_once = cqe_harvest_count; }

                for (size_t i = 0; i < cqe_harvest_count; i++)
                {
                    p_cqe = cqe_pointer_array[i];

                    process_cqe(p_cqe, now);
                }

                io_uring_cq_advance(&struct_io_uring,cqe_harvest_count);
            }

        } while (cqe_harvest_count == cqes_with_one_peek_limit);

        if ( total_cqes_reaped_this_pass > 0 )                     { did_something_this_pass = true; }

        if ( now >= thread_view_subinterval_end )                  { break; }

        if ( (!ivydriver.spinloop)
          && (!unexpired_timeouts.unsubmitted_timeout.isZero()) )  { did_something_this_pass = true; try_to_submit_unsubmitted_timeout(now); }

        if ( !have_failed_to_get_an_sqe )                          { populate_sqes(); }

        if ( !did_something_this_pass )                            { generate_IOs(); }

        if ( (!ivydriver.spinloop) && (!did_something_this_pass) ) { possibly_insert_timeout(now); }

        bool fruitless_pass = (!did_something_this_pass)  && (!ivydriver.spinloop) && sqes_queued_for_submit == 0;

        if   (fruitless_pass) { consecutive_fruitless_passes++; cumulative_fruitless_passes++; }
        else                  { consecutive_fruitless_passes = 0; }

        int submitted = 0;

        if ( sqes_queued_for_submit > 0 )
        {
            // we already knew it wasn't a fruitless pass, but now we know there are sqes to submit

            if ( max_sqes_tried_to_submit_at_once < sqes_queued_for_submit ) { max_sqes_tried_to_submit_at_once = sqes_queued_for_submit; }

            submitted = io_uring_submit(&struct_io_uring); cumulative_submits++;

            if (submitted < 0)
            {
                std::ostringstream o;
                o << "io_uring_submit(&struct_io_uring) with sqes_queued_for_submit = " << sqes_queued_for_submit
                    << " failed return code " << submitted << " (" << std::strerror(-submitted) << ")." << std::endl;
                post_error(o.str());
            }

            if ( ((unsigned int) submitted) > max_sqes_submitted_at_once ) { max_sqes_submitted_at_once = ((unsigned int) submitted); }

///*debug*/                int short_submit = ((int) sqes_queued_for_submit) - submitted;
///*debug*/
///*debug*/                short_submit_counter_map[short_submit].c++;
///*debug*/
///*debug*/                unsigned dropped = *(struct_io_uring.sq.kdropped);
///*debug*/
///*debug*/                dropped_ocurrences_map[dropped].c++;
            sqes_queued_for_submit -= (unsigned) submitted;
        }
        else // There are no sqes queued for submit
        {
            if (consecutive_fruitless_passes > ivydriver.fruitless_passes_before_wait)
            {
                /*debug*/if (workload_thread_queue_depth == 0) {throw std::runtime_error("Wait with zero queue depth.");}

                submitted = io_uring_submit_and_wait(&struct_io_uring,1); cumulative_waits++;

                if (submitted < 0)
                {
                    std::ostringstream o;
                    o << "io_uring_submit_and_wait(&struct_io_uring,1) with sqes_queued_for_submit = " << sqes_queued_for_submit
                        << " failed return code " << submitted << " (" << std::strerror(-submitted) << ")." << std::endl;
                    post_error(o.str());
                }
                else
                {
//    /*debug*/                int short_submit = ((int) sqes_queued_for_submit) - submitted;
//    /*debug*/
//    /*debug*/                short_submit_counter_map[short_submit].c++;
//    /*debug*/
//    /*debug*/                unsigned dropped = *(struct_io_uring.sq.kdropped);
//    /*debug*/
//    /*debug*/                dropped_ocurrences_map[dropped].c++;

                    if ( submitted != 0 )
                    {
                        std::ostringstream o;
                        o << "io_uring_submit_and_wait(&struct_io_uring,1) was supposed to be a pure wait, but it submitted "
                            << submitted << " sqes.   \'sqes_queued_for_submit\", which is supposed to be zero was in fact " << sqes_queued_for_submit
                            << "." << std::endl;
                        post_error(o.str());
                    }
                }
            }
        }
    }

    number_of_IOs_running_at_end_of_subinterval = 0;

    if (ivydriver.track_long_running_IOs)
    {
        for (TestLUN* pTestLUN : pTestLUNs)
        {
            for (const auto& pear : pTestLUN->workloads)
            {
                number_of_IOs_running_at_end_of_subinterval += pear.second.running_IOs.size();
            }
        }
    }

    if (routine_logging)
    {
        {
            std::ostringstream o;
            o << "For step number " << ivydriver.step_number << " subinterval number " << thread_view_subinterval_number << " LUNs = " << pTestLUNs.size() << ".";

            log(slavethreadlogfile,o.str());
        }

        log_io_uring_engine_stats();
    }

    step_run_time_seconds += ivydriver.subinterval_duration.getlongdoubleseconds();

	return;
}


void WorkloadThread::examine_timer_pop(struct cqe_shim* p_shim,const ivytime& now)
{
    double scheduled_seconds = (p_shim->scheduled_duration()).getlongdoubleseconds();

    double actual_seconds = (p_shim->since_insertion(now)).getlongdoubleseconds();

    if (scheduled_seconds <= 0.0 || actual_seconds <= 0.0)
    {
        std::ostringstream o;
        o << "<Error> internal programming error - WorkloadThread::examine_timer_pop()"
            << " - one or both of scheduled duration in seconds (" << scheduled_seconds
            << ") or actual duration seconds (" << actual_seconds << ") is less than or equal to zero.";
        throw std::runtime_error(o.str());
    }

    if ( scheduled_seconds >= .001 && actual_seconds < (0.2 * scheduled_seconds) )
    {
        std::ostringstream o;
        o << "<Error> internal programming error - WorkloadThread::examine_timer_pop()"
            << " - actual duration seconds (" << actual_seconds
            << ") less than 1/5th of scheduled duration in seconds (" << scheduled_seconds << ")";
        throw std::runtime_error(o.str());
    }

//    if ( actual_seconds > 0.1 && actual_seconds > (10 * scheduled_seconds) )
//    {
//        std::ostringstream o;
//        o << "<Error> internal programming error - WorkloadThread::examine_timer_pop()"
//            << " - actual duration seconds (" << actual_seconds
//            << ") greater than 10x scheduled duration in seconds (" << scheduled_seconds << ")";
//        throw std::runtime_error(o.str());
//    }

    return;
}


void WorkloadThread::process_cqe(struct io_uring_cqe* p_cqe, const ivytime& now)
{
    if (p_cqe == nullptr) { post_error("WorkloadThread::process_cqe() called with null pointer to struct io_uring_cqe pointer."); }

    struct cqe_shim* p_shim = (struct cqe_shim*) p_cqe->user_data;

    std::ostringstream o;

    Eyeo* pEyeo;

    switch (p_shim->type)
    {
        case cqe_type::timeout:

            cumulative_timer_pop_cqes++;

            if ( abs(p_cqe->res) != ETIME && p_cqe->res != 0 )
            {
                std::lock_guard<std::mutex> lk_guard(*p_ivydriver_main_mutex);

                if (!ivydriver.spinloop)
                {
                    ivydriver.spinloop = true;

                    o << "<Warning> internal programming issue - an IORING_OP_TIMEOUT failed return code ";
                    print_in_dec_and_hex(o, p_cqe->res);
                    o << " - " << std::strerror(abs(p_cqe->res));
                    o << " - setting spinloop to \"on\" to disable use of timeouts / waiting." << std::endl;
                    ivydriver.workload_thread_warning_messages.push_back(o.str());
                }
            }

            examine_timer_pop(p_shim, now);

            unexpired_timeouts.delete_shim(p_shim);

            break;

        case cqe_type::eyeo:

            pEyeo = (Eyeo*) p_cqe->user_data;

            pEyeo->pWorkload->post_Eyeo_result(p_cqe, pEyeo, now);

            cumulative_eyeo_cqes++;

            break;

        default:
            o << "when harvesting a completion queue event, user_data pointed to a cqe_shim of an unexpected type " << p_shim->type;
            post_error(o.str());
    }

    return;
}

unsigned int WorkloadThread::populate_sqes()
{
    if (pTestLUNs.size() == 0) return 0;

    std::vector<TestLUN*>::iterator it = pTestLUN_populate_sqes_bookmark;

    unsigned int total = 0;

    while (true)
    {
        TestLUN* pTestLUN = *it;

        total += pTestLUN->populate_sqes();

        it++; if (it == pTestLUNs.end()) { it = pTestLUNs.begin(); }

        if ( it == pTestLUN_populate_sqes_bookmark
          || have_failed_to_get_an_sqe            ) { break; }
    }

    pTestLUN_populate_sqes_bookmark ++;

    if (pTestLUN_populate_sqes_bookmark == pTestLUNs.end())
    {
        pTestLUN_populate_sqes_bookmark =  pTestLUNs.begin();
    }

    return total;
}


unsigned int WorkloadThread::generate_IOs()
{
    if (pTestLUNs.size() == 0) return 0;

    auto it = pTestLUN_generate_bookmark;

    unsigned int n {0};

    while (true)
    {
        TestLUN* pTestLUN = (*it);

        n += pTestLUN->generate_IOs();

        if ( n > 0 ) break;

        it++; if (it == pTestLUNs.end()) it = pTestLUNs.begin();

        if (it == pTestLUN_generate_bookmark) break;
    }

    pTestLUN_generate_bookmark++;

    if (pTestLUN_generate_bookmark == pTestLUNs.end())
    {
        pTestLUN_generate_bookmark =  pTestLUNs.begin();
    }

    return n;
}


void WorkloadThread::set_all_queue_depths_to_zero()
{
    workload_thread_queue_depth = 0;
    workload_thread_max_queue_depth = 0;

    for (auto& pTestLUN : pTestLUNs)
    {
        for (auto& pear : pTestLUN->workloads)
        {
            pear.second.workload_queue_depth = 0;
            pear.second.workload_max_queue_depth = 0;
        }
    }
    return;
}


void WorkloadThread::post_error(const std::string& msg)
{
    std::string s;

    {
        std::ostringstream o;
        o << "<Error> internal programming error - "s << ivydriver.ivyscript_hostname
            << " WorkloadThread physical core "s << physical_core << " hyperthread " << hyperthread
            << " - " << msg;
        s = o.str();
    }

    if (s[s.size()-1] != '\n') { s += "\n"; }

    log(slavethreadlogfile,s);
    log(ivydriver.slavelogfile,"posted by WorkloadThread: "s + s);

    reported_error = true;
    if (!ivydriver.reported_error) {ivydriver.time_error_reported.setToNow();}
    ivydriver.reported_error = true;

    std::cout << s << std::flush;  // The reason we both queue it and say it is that it's possible that when I say() it here, this may come out in the middle of a line being spoken by ivydriver.

    {
        std::lock_guard<std::mutex> lk_guard(*p_ivydriver_main_mutex);
        ivydriver.workload_thread_error_messages.push_back(s);
    }

    // ?????? should we sleep here a bit?  - gnaw.

    return;
}


void WorkloadThread::post_warning(const std::string& msg)
{
    std::string s;

    {
        std::ostringstream o;
        o << "<Warning> "s << ivydriver.ivyscript_hostname
            << " WorkloadThread physical core "s << physical_core << " hyperthread " << hyperthread
            << " - " << msg;
        s = render_string_harmless(o.str());
    }
    s.push_back('\n');

    log(slavethreadlogfile,s);
    log(ivydriver.slavelogfile,"posted by WorkloadThread: "s + s);

    {
        std::lock_guard<std::mutex> lk_guard(*p_ivydriver_main_mutex);
        ivydriver.workload_thread_warning_messages.push_back(s);
    }

    return;
}


void WorkloadThread::check_for_long_running_IOs()
{
    // runs with the lock held at subinterval switchover
    // when I/O events are not being harvested.

    ivytime now; now.setToNow();

    for (TestLUN*& pTestLUN : pTestLUNs)
    {
        {
            RunningStat<ivy_float,ivy_int> long_running_IOs;

            for (auto& pear : pTestLUN->workloads)
            {
                for (Eyeo* pEyeo : pear.second.running_IOs)
                {
                    long double run_time_so_far = 0.0 - pEyeo->start_time.seconds_from(now);

                    if (run_time_so_far > 1.0)
                    {
                        long_running_IOs.push(run_time_so_far);
                    }
                }
            }

            if (long_running_IOs.count() > 0)
            {
                std::ostringstream o;
                o << "LUN " << pTestLUN->host_plus_lun << " has " << long_running_IOs.count()
                    << " I/Os that have been running for more than one second, the longest of which for "
                    <<  long_running_IOs.max() << " seconds." << std::endl;
                post_warning(o.str());
            }
        }
    }

    ivytime ending; ending.setToNow();

    ivytime run_time = ending-now;

    if (routine_logging)
    {
        std::ostringstream o;
        o << "WorkloadThread::check_for_long_running_IOs() held the lock and ran for " << run_time.getAsNanoseconds() << " nanoseconds." << std::endl;
        log(slavethreadlogfile,o.str());
    }

    return;
}


void WorkloadThread::build_uring_and_allocate_and_mmap_Eyeo_buffers_and_build_Eyeos()
{
    if (io_uring_fd != -1)
    {
        std::ostringstream o;
        o << "WorkloadThread::build_uring_and_allocate_and_mmap_Eyeo_buffers_and_build_Eyeos()"
            << " - upon entry, io_uring_fd was not -1, instead it was " << io_uring_fd << std::endl;
        post_error(o.str());
    }

    if (p_buffer_pool != nullptr)
    {
        std::ostringstream o;
        o << "WorkloadThread::build_uring_and_allocate_and_mmap_Eyeo_buffers_and_build_Eyeos()"
            << " - upon entry, p_buffer_pool was not nullptr, instead it was " << p_buffer_pool << std::endl;
        post_error(o.str());
    }

    cumulative_sqes             = 0;
    cumulative_submits          = 0;
    cumulative_waits            = 0;

    cumulative_loop_passes      = 0;
    cumulative_fruitless_passes = 0;

    cumulative_cqes             = 0; // all set in process_cqe
    cumulative_timer_pop_cqes   = 0;
    cumulative_eyeo_cqes        = 0;

    max_sqes_submitted_at_once      = 0;
    max_cqes_reaped_at_once         = 0;
    max_sqes_tried_to_submit_at_once = 0;


    total_buffer_size = 0;

    total_maxTags = 0;

//*debug*/ { std::ostringstream o; o << "WorkloadThread::build_uring_and_allocate_and_mmap_Eyeo_buffers_and_build_Eyeos() - b4 computing buffer pool size." << std::endl;log(slavethreadlogfile, o.str()); }

    unsigned total_Eyeos {0};

    for (TestLUN* pTestLUN : pTestLUNs)
    {
        for (auto& wkld_pear : pTestLUN->workloads)
        {
            {
                Workload& w = wkld_pear.second;

                Subinterval& s = w.subinterval_array[0];

                IosequencerInput& ii = s.input;

                total_maxTags     += ii.maxTags;

                total_buffer_size += w.io_buffer_space();

                total_Eyeos       += w.eyeo_build_qty();

                if (routine_logging)
                {
                    std::ostringstream o; o << w.workloadID << " - "
                        <<   "ii.blocksize_bytes = "                     << put_on_KiB_etc_suffix( ii.blocksize_bytes )
                        << ", ii.maxTags  = "                            << w.eyeo_build_qty()
                        << ", w.precomputedepth() = "                    << w.precomputedepth()
                        << ", w.eyeo_build_qty() = "                     << w.eyeo_build_qty()
                        << ", w.flex_Eyeos() = "                         << w.flex_Eyeos()
                        << ", blocksize rounded-up to 4 kIB multiple = " << put_on_KiB_etc_suffix( round_up_to_4096_multiple(ii.blocksize_bytes) )
                        << ", w.io_buffer_space() = "                    << put_on_KiB_etc_suffix( w.io_buffer_space() )
                        << ", running total_Eyeos = "                    << total_Eyeos
                        << ", running total_buffer_size = "              << put_on_KiB_etc_suffix( total_buffer_size )
                        ;

                    log(slavethreadlogfile,o.str());
                }
            }
        }
    }

    if (routine_logging)
    {
        std::ostringstream o;

        o << "I/O buffer pool total_buffer_size = " << put_on_KiB_etc_suffix(total_buffer_size)
            << " for " << total_Eyeos << " Eyeos"
            << " or an average of " << (total_buffer_size / total_Eyeos) << " buffer pool bytes per Eyeo.";

        o << "  Total maxTags = " << total_maxTags << std::endl;

        log(slavethreadlogfile, o.str());
    }

    int rc = posix_memalign(&p_buffer_pool, 4096, total_buffer_size);

    if (rc != 0)
    {
        p_buffer_pool = nullptr;

        std::ostringstream o;
        o << "<Error> internal programming error - WorkloadThread for physical core " << physical_core << " hyperthread " << hyperthread
            << " - posix_memalign(&p_buffer_pool, 4096, total_buffer_size = " << total_buffer_size
            << " failed with return code " << rc << " (" << std::strerror(-rc) << ")  when allocating memory for Eyeo buffer pool." << std::endl;
        throw std::runtime_error(o.str());
    }

    requested_uring_entries = 1;

    while (requested_uring_entries < 4096 && (2*requested_uring_entries) < (total_maxTags+3)) // This formulation OK for unsigned - no subtracting
    {
        requested_uring_entries *= 2;
    }

    rc = io_uring_queue_init(requested_uring_entries,&struct_io_uring,0);

    if ( rc != 0)
    {
        std::ostringstream o;
        o << "<Error> internal programming error - WorkloadThread for physical core " << physical_core << " hyperthread " << hyperthread
            << " - failed trying to initialize io_uring with return code " << rc << " (" << std::strerror(-rc) << ")." << std::endl;
        throw std::runtime_error(o.str());
    }

    io_uring_fd = struct_io_uring.ring_fd;

    if (routine_logging)
    {
        std::ostringstream o;
        o << "io_uring_queue_init requested entries = "  << requested_uring_entries
            << ", assigned submit queue entries = "     << *(struct_io_uring.sq.kring_entries)
            << ", assigned completion queue entries = " << *(struct_io_uring.cq.kring_entries)
            << ".  struct io_uring.ring_fd = " << struct_io_uring.ring_fd << "." << std::endl;
        log(slavethreadlogfile,o.str());
    }

    workload_thread_queue_depth_limit = (*(struct_io_uring.cq.kring_entries)) - 1;;

    struct iovec struct_iovec;

    struct_iovec.iov_base = p_buffer_pool;
    struct_iovec.iov_len = total_buffer_size;

    rc = io_uring_register_buffers(&struct_io_uring, &struct_iovec, 1);

    if ( rc != 0)
    {
        std::ostringstream o;
        o << "<Error> internal programming error - WorkloadThread for physical core " << physical_core << " hyperthread " << hyperthread
            << " - failed trying to register buffer pool with return code " << rc << " (" << std::strerror(errno) << ")." << std::endl;
        throw std::runtime_error(o.str());
    }

    if (routine_logging)
    {
        std::ostringstream o;
        o << "io_uring_register_buffers() performed for struct_iovec.iov_base = " << struct_iovec.iov_base
        << ", struct_iovec.iov_len = " << put_on_KiB_etc_suffix(struct_iovec.iov_len) << ".";
        log(slavethreadlogfile, o.str());
    }

    {
        size_t testluns {pTestLUNs.size()};

        int arrayof_fds[testluns];

        size_t i = 0;

        for (auto pTestLUN : pTestLUNs)
        {
            pTestLUN->open_fd();
            arrayof_fds[i] = pTestLUN->fd;
            pTestLUN->fd_index = i;
            i++;
        }

        rc = io_uring_register_files(&struct_io_uring, arrayof_fds, pTestLUNs.size());

        if ( rc != 0)
        {
            std::ostringstream o;
            o << "<Error> internal programming error - WorkloadThread for physical core " << physical_core << " hyperthread " << hyperthread
                << " - failed trying to register array of TestLUN file descriptors with return code " << rc << " (" << std::strerror(errno) << ")." << std::endl;
            throw std::runtime_error(o.str());
        }

        if (routine_logging)
        {
            std::ostringstream o;
            o << "io_uring_register_files() performed for " << testluns << " fds:";

            for (size_t i = 0; i < pTestLUNs.size();i ++)
            {
                o << " " << arrayof_fds[i];
            }

            o << ".";

            log(slavethreadlogfile, o.str());
        }
    }

    uint64_t p_buffer = (uint64_t) p_buffer_pool;

    for (auto& pTestLUN : pTestLUNs)
    {
        for (auto& pear : pTestLUN->workloads)
        {
            pear.second.build_Eyeos(p_buffer);
        }
    }

    uint64_t io_buffer_pointer_advance = p_buffer - ( (uint64_t) p_buffer_pool );

    if ( io_buffer_pointer_advance != total_buffer_size)
    {
        std::ostringstream o;
        o << "In WorkloadThread::build_uring_and_allocate_and_mmap_Eyeo_buffers_and_build_Eyeos(), "
            << "the amount by which the Eyeo I/O buffer pointer moved forward in build_Eyeos() "
            << " io_buffer_pointer_advance = ";
        print_in_dec_and_hex(o, io_buffer_pointer_advance);
        o << ", was different from the size of the buffer pool as it was allocated "
            << " based on an original pre-scan to add up the amount of memory needed ";
        print_in_dec_and_hex(o, total_buffer_size);
        o << "." << std::endl;
        post_error(o.str());
    }

    sqes_queued_for_submit = 0;

    return;
}

void WorkloadThread::shutdown_uring_and_unmap_Eyeo_buffers_and_delete_Eyeos()
{
//*debug*/ { std::ostringstream o; o << "WorkloadThread::shutdown_uring_and_unmap_Eyeo_buffers_and_delete_Eyeos() - entry." << std::endl;log(slavethreadlogfile, o.str()); }

    int rc_ub, rc_uf;
    rc_ub = io_uring_unregister_buffers(&struct_io_uring);
    rc_uf = io_uring_unregister_files  (&struct_io_uring);
            io_uring_queue_exit        (&struct_io_uring);

    io_uring_fd = -1;

    if (rc_ub != 0 || rc_uf != 0)
    {
        std::ostringstream o;
        o << "shutdown_uring_and_unmap_Eyeo_buffers_and_delete_Eyeos() - ";
        o << "io_uring_unregister_buffers() return code = " << rc_ub;  if (rc_ub != 0) { o << " (" << std::strerror(abs(rc_ub)) << ")"; }
        o << "io_uring_unregister_files() return code = "   << rc_uf;  if (rc_uf != 0) { o << " (" << std::strerror(abs(rc_uf)) << ")"; }
        post_error(o.str());
    }

//*debug*/ { std::ostringstream o; o << "WorkloadThread::shutdown_uring_and_unmap_Eyeo_buffers_and_delete_Eyeos() - after io_uring_queue_exit()." << std::endl;log(slavethreadlogfile, o.str()); }

    for (auto& pTestLUN : pTestLUNs)
    {
        int rc = close(pTestLUN->fd);

        if (rc != 0)
        {
            std::ostringstream o;
            o << "shutdown_uring_and_unmap_Eyeo_buffers_and_delete_Eyeos() - ";
            o << "return code " << rc << " (" << std::strerror(abs(rc)) << ") from close(" << pTestLUN->fd << ") for " << pTestLUN->host_plus_lun;
            post_error(o.str());
        }

        pTestLUN->fd = -1;

        for (auto& pear : pTestLUN->workloads)
        {
            {
                Workload& w = pear.second;

                for (auto& pEyeo : w.allEyeosThatExist) delete pEyeo;

                w.allEyeosThatExist.clear();

                while (0 < w.freeStack.size()) w.freeStack.pop(); // stacks curiously don't hve a clear() method.

                w.precomputeQ.clear();

                w.running_IOs.clear();
            }
        }
    }

//*debug*/ { std::ostringstream o; o << "WorkloadThread::shutdown_uring_and_unmap_Eyeo_buffers_and_delete_Eyeos() - after closing LUN fds." << std::endl;log(slavethreadlogfile, o.str()); }
    if (p_buffer_pool == nullptr)
    {
        std::ostringstream o;
        o << "shutdown_uring_and_unmap_Eyeo_buffers_and_delete_Eyeos() - ";
        o << "p_buffer_pool is nullptr.";
        post_error(o.str());
    }

    free(p_buffer_pool);

    p_buffer_pool = nullptr;
//*debug*/ { std::ostringstream o; o << "WorkloadThread::shutdown_uring_and_unmap_Eyeo_buffers_and_delete_Eyeos() - returning." << std::endl;log(slavethreadlogfile, o.str()); }

    return;
}

void WorkloadThread::possibly_insert_timeout(const ivytime& now)
{
    ivytime timer_expiry = now + ivydriver.max_wait;

    if (now > thread_view_subinterval_end) { did_something_this_pass = true; return; }

    if (timer_expiry > thread_view_subinterval_end) { timer_expiry = thread_view_subinterval_end; }

    for (TestLUN* pTestLUN : pTestLUNs)
    {
        for (auto& pear : pTestLUN->workloads)
        {
            {
                Workload& w = pear.second;

                if (w.workload_queue_depth >= w.p_current_IosequencerInput->maxTags) continue;

                if (w.is_IOPS_max_with_positive_skew() && w.hold_back_this_time()) continue;

                if (w.precomputeQ.size() == 0) continue;

                Eyeo* pEyeo = w.precomputeQ.front();

                if (pEyeo->scheduled_time.isZero()) continue;

                if (timer_expiry.isZero() || pEyeo->scheduled_time < timer_expiry)
                {
                    timer_expiry = pEyeo->scheduled_time;
                }
            }
        }
    }

//*debug*/if (timer_expiry.isZero()) {throw std::runtime_error("<Error> internal programming error - WorkloadThread::possibly_insert_timeout(const ivytime& now) - Borked!");}

    ivytime earliest_unexpired = unexpired_timeouts.earliest_timeout(); // including a possibly queued timeout


    if ( earliest_unexpired.isZero() || timer_expiry < earliest_unexpired )
    {
        // need to submit new timeout

        did_something_this_pass = true;

        if (timer_expiry <= now) return;

        if ( unexpired_timeouts.unsubmitted_timeout.isZero() )
        {
            struct io_uring_sqe* p_sqe = get_sqe();
            if (p_sqe)
            {

                fill_in_timeout_sqe(p_sqe, timer_expiry, now);

                unexpired_timeouts.unsubmitted_timeout.setToZero();
            }
            else
            {
                unexpired_timeouts.unsubmitted_timeout = timer_expiry;
            }
        }
        else
        {
            unexpired_timeouts.unsubmitted_timeout = timer_expiry;
            try_to_submit_unsubmitted_timeout(now);
        }
    }

    return;
}


void WorkloadThread::fill_in_timeout_sqe(struct io_uring_sqe* p_sqe, const ivytime& timestamp, const ivytime& now)
{
    if ( timestamp.isZero() || now.isZero() || now > timestamp )
    {
        std::ostringstream o;
        o << "<Error> WorkloadThread::fill_in_timeout_sqe(struct io_uring_sqe* p_sqe, const ivytime& timestamp = "
            << timestamp.format_as_datetime_with_ns() << ", const ivytime& now = " << now.format_as_datetime_with_ns() << ")"
            << " - both \"timestamp\" and \"now\" must be non-zero, and \"timestamp\" must be after \"now\"." << std::endl;
        throw std::runtime_error(o.str());
    }

    cqe_shim* p_shim = unexpired_timeouts.insert_timeout(timestamp, now);

    number_of_concurrent_timeouts.push ((ivy_float) unexpired_timeouts.unexpired_shims.size());

    ivytime delta_to_timestamp = timestamp - now;

    p_shim->kts.tv_sec  = delta_to_timestamp.t.tv_sec;
    p_shim->kts.tv_nsec = delta_to_timestamp.t.tv_nsec;

//    memset(p_sqe,0,sizeof(struct io_uring_sqe));
//    p_sqe->opcode    = IORING_OP_TIMEOUT;
//    p_sqe->fd        = -1;
//    p_sqe->addr      = (uint64_t) &(p_shim->kts);

    io_uring_prep_timeout(p_sqe, &(p_shim->kts), 0 /* count*/, 0 /* flags */);

    p_sqe->user_data = (uint64_t) p_shim;

    return;
}


struct io_uring_sqe* WorkloadThread::get_sqe()
{
    if (workload_thread_queue_depth >= workload_thread_queue_depth_limit) { have_failed_to_get_an_sqe = true; return nullptr; }

    struct io_uring_sqe* p_sqe = io_uring_get_sqe(&struct_io_uring);

    if (p_sqe == nullptr) { have_failed_to_get_an_sqe = true; return nullptr; }

    workload_thread_queue_depth++;

    cumulative_sqes++;

    if ( workload_thread_max_queue_depth < workload_thread_queue_depth )
       { workload_thread_max_queue_depth = workload_thread_queue_depth;}

    sqes_queued_for_submit++;

    did_something_this_pass = true;

    if ( workload_thread_queue_depth /* includes wt.sqes_queued_for_submit) */ >= workload_thread_queue_depth_limit )
    {
        if (!have_warned_hitting_cqe_entry_limit)
        {
            have_warned_hitting_cqe_entry_limit = true;

            std::ostringstream o;
            o << "have reached the workload_thread_queue_depth_limit value being " << workload_thread_queue_depth_limit
                << " : workload_thread_cqe_entrie.";
            post_warning(o.str());
        }
    }

    return p_sqe;
}


void WorkloadThread::try_to_submit_unsubmitted_timeout(const ivytime& now)
{
    if ( unexpired_timeouts.unsubmitted_timeout.isZero() ) { return; }

    did_something_this_pass = true;

    if (now >= unexpired_timeouts.unsubmitted_timeout)
    {
        unexpired_timeouts.unsubmitted_timeout.setToZero();
        return;
    }

    struct io_uring_sqe* p_sqe = get_sqe();

    if (p_sqe == nullptr)
    {
        have_failed_to_get_an_sqe = true;
    }
    else
    {
        fill_in_timeout_sqe(p_sqe, unexpired_timeouts.unsubmitted_timeout, now);
            // this also creates & inserts a cqe_shim for the timeout.
        unexpired_timeouts.unsubmitted_timeout.setToZero();
    }

    return;
}


void WorkloadThread::log_io_uring_engine_stats()
{
    std::ostringstream o;

    o <<   "max_cqes_reaped_at_once = "          << max_cqes_reaped_at_once;
    o << ", max_sqes_tried_to_submit_at_once = " << max_sqes_tried_to_submit_at_once;
    o << ", max_sqes_submitted_at_once = "       << max_sqes_submitted_at_once;
    o << ", sqes_per_submit_limit = "            << ivydriver.sqes_per_submit_limit;
    o << ", cqes_with_one_peek_limit = "         << cqes_with_one_peek_limit;
    o << ", generate_at_a_time_multiplier = "    << ivydriver.generate_at_a_time_multiplier;

    o << ", loop_passes_per_IO() = "             << loop_passes_per_IO();
    o << ", timer_pops_per_second() = "          << timer_pops_per_second();
    o << ", timer_pops_per_IO() = "              << timer_pops_per_IO();
    o << ", submits_per_IO() = "                 << submits_per_IO();
    o << ", fruitless_passes_per_IO() = "        << fruitless_passes_per_IO();
    o << ", cqes_per_loop_pass() = "             << cqes_per_loop_pass();
    o << ", sqes_per_submit() = "                << sqes_per_submit();
    o << ", IOs_per_system_call() = "            << IOs_per_system_call();

//*debug*/{ unsigned total {0}; o << std::endl; for (auto& pear : short_submit_counter_map) { total += pear.second.c; o << "For short submit value " << pear.first << " the number of occurrences was " << pear.second.c << "."<< std::endl;} o << "Total " << total << " occurrences." << std::endl; }
//*debug*/{ unsigned total {0}; o << std::endl; for (auto& pear : dropped_ocurrences_map  ) { total += pear.second.c; o << "For dropped value "      << pear.first << " the number of occurrences was " << pear.second.c << "."<< std::endl;} o << "Total " << total << " occurrences." << std::endl; }

    o << ", track_long_running_IOs = "   << (ivydriver.track_long_running_IOs ? "on" : "off");
    o << ", spinloop = "                 << (ivydriver.spinloop               ? "on" : "off");
    o << ", sqes_per_submit_limit = "    << ivydriver.sqes_per_submit_limit;

    log(slavethreadlogfile,o.str());

    return;
}


void WorkloadThread::catch_in_flight_IOs_after_last_subinterval()
{
    ivytime upon_entry; upon_entry.setToNow();
    ivytime limit;      limit = upon_entry + ivytime(1.0);

    unsigned timeouts {0}, eyeos {0};

    ivytime now;

    while (workload_thread_queue_depth > 0)
    {
        now.setToNow();

        if (now > limit)
        {
            std::ostringstream o;
            o << "In WorkloadThread::catch_in_flight_IOs_after_last_subinterval() after harvesting "
                << eyeos << " I/Os, " << timeouts << " timer pops"
                << " over a period of " << ivytime( now - upon_entry ).format_as_duration_HMMSSns()
                << ", there were still " << workload_thread_queue_depth << " pending completion queue events.";
            log(slavethreadlogfile,o.str());
            return;
        }

        unsigned cqe_harvest_count = io_uring_peek_batch_cqe(&struct_io_uring, cqe_pointer_array, cqes_with_one_peek_limit);

        if (cqe_harvest_count > 0)
        {
            workload_thread_queue_depth -= cqe_harvest_count;

            for (size_t i = 0; i < cqe_harvest_count; i++)
            {
                struct io_uring_cqe* p_cqe = cqe_pointer_array[i];

                struct cqe_shim* p_shim = (struct cqe_shim*) p_cqe->user_data;
                switch (p_shim->type)
                {
                    case cqe_type::eyeo:    eyeos++;    break;
                    case cqe_type::timeout: timeouts++; break;
                    default: ;
                }
            }

            io_uring_cq_advance(&struct_io_uring,cqe_harvest_count);
        }

    }

    now.setToNow();

    if (routine_logging)
    {
        std::ostringstream o;
        o << "WorkloadThread::catch_in_flight_IOs_after_last_subinterval() harvested "
            << eyeos << " I/Os and " << timeouts << " timer pops"
            << " over a period of " << ivytime( now - upon_entry ).format_as_duration_HMMSSns() << "." << std::endl;
        log(slavethreadlogfile,o.str());
    }

    return;

}



























