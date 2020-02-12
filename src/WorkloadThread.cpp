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

#include <sys/prctl.h>

//#define IVYDRIVER_TRACE
// IVYDRIVER_TRACE defined here in this source file rather than globally in ivydefines.h so that
//  - the CodeBlocks editor knows the symbol is defined and highlights text accordingly.
//  - you can turn on tracing separately for each class in its own source file.

#include "WorkloadThread.h"
#include "IosequencerRandomSteady.h"
#include "IosequencerRandomIndependent.h"
#include "IosequencerSequential.h"
#include "ivydriver.h"

//#define CHECK_FOR_LONG_RUNNING_IOS

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

// Note: Setting the thread as a real-time thread was intended to reduce odd higher-than- expected service
//       times for large blocksizes.  However, this caused problems with DFC=IOPS_staircase, where the
//       interlock protocol between pipe_driver_thread and ivydriver stalled and failed after running for
//       a while.  Commenting this next bit out resolved the issue.
//
//    // Set real-time scheduler (FIFO) for this thread. Choose mid-point realtime priority.
//
//    struct sched_param sched_param;
//    sched_param.__sched_priority = (sched_get_priority_min(SCHED_FIFO) + sched_get_priority_max(SCHED_FIFO)) / 2;
//    int rc = sched_setscheduler(0, SCHED_FIFO, &sched_param);
//    if (routine_logging) {
//        std::ostringstream o;
//        o << "sched_setscheduler(0, SCHED_FIFO, &sched_param) returned " << rc;
//        if (rc < 0) {
//            o << "; errno = " << errno << " (" << std::strerror(errno) << ")";
//        }
//        o << "." << std::endl;
//        log(slavethreadlogfile, o.str());
//    }

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

		slaveThreadConditionVariable.notify_all();

        thread_view_subinterval_number = -1;

#ifdef IVYDRIVER_TRACE
        wt_callcount_linux_AIO_driver_run_subinterval = 0;
        wt_callcount_cancel_stalled_IOs = 0;
        wt_callcount_start_IOs = 0;
        wt_callcount_reap_IOs = 0;
        wt_callcount_pop_and_process_an_Eyeo = 0;
        wt_callcount_generate_an_IO = 0;
        wt_callcount_catch_in_flight_IOs_after_last_subinterval = 0;
#endif

		for (auto& pTestLUN : pTestLUNs)
		{
		    try
		    {
                pTestLUN->testLUN_furthest_behind_weighted_IOPS_max_skew_progress = 0.0;
                pTestLUN->launch_count = 0;

    #ifdef IVYDRIVER_TRACE
                pTestLUN->sum_of_maxTags_callcount = 0;
                pTestLUN->open_fd_callcount = 0;
                pTestLUN->prepare_linux_AIO_driver_to_start_callcount = 0;
                pTestLUN->reap_IOs_callcount = 0;
                pTestLUN->pop_front_to_LaunchPad_callcount = 0;
                pTestLUN->start_IOs_callcount = 0;
                pTestLUN->catch_in_flight_IOs_callcount = 0;
                pTestLUN->ivy_cancel_IO_callcount = 0;
                pTestLUN->pop_and_process_an_Eyeo_callcount = 0;
                pTestLUN->generate_an_IO_callcount = 0;
                pTestLUN->next_scheduled_io_callcount = 0;

                pTestLUN->start_IOs_bookmark_count = 0;
                pTestLUN->start_IOs_body_count = 0;
                pTestLUN->reap_IOs_bookmark_count = 0;
                pTestLUN->reap_IOs_body_count = 0;
                pTestLUN->pop_n_p_bookmark_count = 0;
                pTestLUN->pop_n_p_body_count = 0;
                pTestLUN->generate_bookmark_count = 0;
                pTestLUN->generate_body_count = 0;
    #endif
                for (auto& pear: pTestLUN->workloads)
                {
                    pear.second.iops_max_io_count = 0;
                    pear.second.currentSubintervalIndex=0;
                    pear.second.otherSubintervalIndex=1;
                    pear.second.subinterval_number=0;
                    pear.second.prepare_to_run();
                    pear.second.build_Eyeos();
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

    #ifdef IVYDRIVER_TRACE
                    pear.second.workload_callcount_prepare_to_run = 0;
                    pear.second.workload_callcount_build_Eyeos = 0;
                    pear.second.workload_callcount_switchover = 0;
                    pear.second.workload_callcount_pop_and_process_an_Eyeo = 0;
                    pear.second.workload_callcount_front_launchable_IO = 0;
                    pear.second.workload_callcount_load_IOs_to_LaunchPad = 0;
                    pear.second.workload_callcount_generate_an_IO = 0;

                    pear.second.start_IOs_Workload_bookmark_count = 0;
                    pear.second.start_IOs_Workload_body_count = 0;
                    pear.second.pop_one_bookmark_count = 0;
                    pear.second.pop_one_body_count = 0;
                    pear.second.generate_one_bookmark_count = 0;
                    pear.second.generate_one_body_count = 0;
    #endif
                }
            }
            catch (std::exception& e)
            {
                std::ostringstream o;
                o << "failed preparing TestLUNs and Workloads to start running - "
                    << e.what() << "." << std::endl;
                post_error(o.str());
                goto wait_for_command;
            }
		}


		dispatching_latency_seconds.clear();
        lock_aquisition_latency_seconds.clear();
        switchover_completion_latency_seconds.clear();

        set_all_queue_depths_to_zero();


        for (auto& pTestLUN : pTestLUNs)
        {
            if (pTestLUN->event_fd != -1)
            {
                std::ostringstream o;
                o << "when going to set \"event_fd\" in TestLUN " << pTestLUN->host_plus_lun << ", it already had the value " << pTestLUN->event_fd << ". It was supposed to be -1."
                    << "  Going to try closing the previous value ... ";
                if (0 != close(pTestLUN->event_fd))
                {
                    o << "failed - errno = " << errno << " (" << std::strerror(errno) << ").";
                    post_error(o.str());
                    goto wait_for_command;
                }
                else
                {
                    o << "successful.";
                    post_warning(o.str());
                }
            }

            pTestLUN->event_fd = eventfd(0, 0);  // it's blocking because we are going to use epoll_wait() to detect if it's readable.

            if (pTestLUN->event_fd < 0)
            {
                std::ostringstream o;
                o << "failed trying to create eventfd for TestLUN " << pTestLUN->host_plus_lun << " - errno = " << errno << " " << strerror(errno) << std::endl;
                post_error(o.str());
                goto wait_for_command;
            }
        }

        if (epoll_fd != -1)
        {
            std::ostringstream o;
            o << "when going to set \"epoll_fd\", it already had the value " << epoll_fd << ". It was supposed to be -1.";
            o << "  Going to try closing the previous value ... ";
            if (0 != close(epoll_fd))
            {
                o << "failed - errno = " << errno << " (" << std::strerror(errno) << ").";
                post_error(o.str());
                goto wait_for_command;
            }
            else
            {
                o << "successful.";
                post_warning(o.str());
            }
        }

		epoll_fd = epoll_create(1 /* "size" ignored since Linux kernel 2.6.8 */ );

		if (epoll_fd < 0)
        {
            std::ostringstream o;
            o << "failed trying to create epoll fd - " << std::strerror(errno);
            post_error(o.str());
            goto wait_for_command;
        }

        if (timer_fd != -1)
        {
            std::ostringstream o;
            o << "when going to set \"timer_fd\", it already had the value " << timer_fd << ". It was supposed to be -1.";
            o << "  Going to try closing the previous value ... ";
            if (0 != close(timer_fd))
            {
                o << "failed - errno = " << errno << " (" << std::strerror(errno) << ").";
                post_error(o.str());
                goto wait_for_command;
            }
            else
            {
                o << "successful.";
                post_warning(o.str());
            }
        }

        timer_fd = timerfd_create(CLOCK_MONOTONIC,TFD_NONBLOCK);
		if (timer_fd == -1)
        {
            std::ostringstream o;
            o << "failed trying to create timerfd errno " << errno << " (" << std::strerror(errno) << std::endl;
            post_error(o.str());
            goto wait_for_command;
        }

        memset(&timerfd_setting,0,sizeof(timerfd_setting));

        memset(&(timerfd_epoll_event),0,sizeof(timerfd_epoll_event));
        timerfd_epoll_event.data.ptr = (void*) this;  // This epoll_event points to WorkloadThread.  The others below point to a TestLUN.
        timerfd_epoll_event.events = EPOLLIN; // | EPOLLET | EPOLLOUT;

        int rc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &timerfd_epoll_event);
        if (rc < 0)
        {
            std::ostringstream o;
            o << "epoll_ctl failed trying to add timerfd event - errno " << errno << std::strerror(errno) << std::endl;
            post_error(o.str());
            goto wait_for_command;
        }

        for (auto& pTestLUN : pTestLUNs)
        {
            memset(&(pTestLUN->epoll_ev),0,sizeof(pTestLUN->epoll_ev));
            pTestLUN->epoll_ev.data.ptr = (void*) pTestLUN;
            pTestLUN->epoll_ev.events = EPOLLIN;

            int rc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pTestLUN->event_fd, &pTestLUN->epoll_ev);
            if (rc == -1)
            {
                std::ostringstream o;
                o << "epoll_ctl failed trying to add event_fd for TestLUN " << pTestLUN->host_plus_lun << " - errno " << errno << std::strerror(errno) << std::endl;
                post_error(o.str());
                goto wait_for_command;
            }
        }

        if (routine_logging)
        {
            std::ostringstream o;
            o << "Thread for physical core " << physical_core << " hyperthread " << hyperthread << ", epoll_fd is " << epoll_fd << ", and timer_fd is " << timer_fd << std::endl;
            log(slavethreadlogfile,o.str());
        }

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

        if (p_epoll_events != nullptr) delete[] p_epoll_events;

        max_epoll_event_retrieval_count = 1 /* for timerfd */ + pTestLUNs.size();

        p_epoll_events = new epoll_event[max_epoll_event_retrieval_count]; // throws on failure.
            // Apparently if we make this array smaller than the number of LUNs + 1, the number o things added into the epollm
            // when we do an epoll_wait successively it is smart enough to page through the available fds round-robin.

            // But on this first try, we'll see if it's OK to try to get them all in one go.

		state=ThreadState::running;

        if (routine_logging) log(slavethreadlogfile,"Finished initialization and now about to start running subintervals.");

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
                linux_AIO_driver_run_subinterval();
            }
            catch (std::exception& e)
            {
                std::ostringstream o;
                o << "failed running linux_AIO_driver_run_subinterval() - "
                    << e.what() << "." << std::endl;
                post_error(o.str());
                goto wait_for_command;
            }

            // end of subinterval, flip over to other subinterval or stop

            // First record the dispatching latency,
            // the time from when the clock clicked over to a new subinterval
            // to the time that the WorkloadThread code starts running.

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
                log(slavethreadlogfile,o.str());
            }

         // indent level in loop waiting for run commands
            // indent level in loop running subintervals

            {
                // start of locked section at subinterval switchover
                std::unique_lock<std::mutex> wkld_lk(slaveThreadMutex);

                ivytime got_lock_time;  got_lock_time.setToNow();

                ivytime lock_aquire_time = got_lock_time - before_getting_lock;

                lock_aquisition_latency_seconds.push(lock_aquire_time.getlongdoubleseconds());

                ivydriver_main_posted_command=false; // for next time

#ifdef CHECK_FOR_LONG_RUNNING_IOS
                check_for_long_running_IOs();
#endif
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
#ifdef TRACE_SUBINTERVAL_THUMBNAILS
                            {
                                Workload& w = peach.second;
                                std::ostringstream o;
                                o << "Workload " << w.workloadID
                                << "output summary " << w.p_current_SubintervalOutput->thumbnail(ivydriver.subinterval_duration)
                                << " - input " << w.p_current_IosequencerInput->toString() << std::endl;
                                log(slavethreadlogfile, o.str());
                            }
#endif // TRACE_SUBINTERVAL_THUMBNAILS
                            peach.second.switchover();
                        }
                        catch (std::exception& e)
                        {
                            std::ostringstream o;
                            o << "failed switchover to next subinterval for " << pTestLUN->host_plus_lun <<" - "
                                << e.what() << "." << std::endl;
                            post_error(o.str());
                            goto wait_for_command;
                        }
                    }
                }

        // indent level in loop waiting for run commands
            // indent level in loop running subintervals
                // indent level in locked section at subinterval switchover

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


                            unsigned int overall_max_IOs_tried_to_launch_at_once = 0;
                            int overall_max_IOs_launched_at_once = 0;
                            int overall_max_IOs_reaped_at_once = 0;
                            unsigned int overall_max_consecutive_count_event_fd_writeback = 0;
                            unsigned int overall_max_consecutive_count_event_fd_behind = 0;

                            for (auto& pTestLUN : pTestLUNs)
                            {
                                if (pTestLUN->max_IOs_tried_to_launch_at_once          > overall_max_IOs_tried_to_launch_at_once)          { overall_max_IOs_tried_to_launch_at_once          = pTestLUN->max_IOs_tried_to_launch_at_once; }
                                if (pTestLUN->max_IOs_launched_at_once                 > overall_max_IOs_launched_at_once)                 { overall_max_IOs_launched_at_once                 = pTestLUN->max_IOs_launched_at_once; }
                                if (pTestLUN->max_IOs_reaped_at_once                   > overall_max_IOs_reaped_at_once)                   { overall_max_IOs_reaped_at_once                   = pTestLUN->max_IOs_reaped_at_once; }
                                if (pTestLUN->max_consecutive_count_event_fd_writeback > overall_max_consecutive_count_event_fd_writeback) { overall_max_consecutive_count_event_fd_writeback = pTestLUN->max_consecutive_count_event_fd_writeback; }
                                if (pTestLUN->max_consecutive_count_event_fd_behind    > overall_max_consecutive_count_event_fd_behind)    { overall_max_consecutive_count_event_fd_behind    = pTestLUN->max_consecutive_count_event_fd_behind; }

                                o << "For TestLUN"                            << pTestLUN->host_plus_lun << " : "
                                    << "testLUN_max_queue_depth = "           << pTestLUN->testLUN_max_queue_depth
                                    << ", max_IOs_tried_to_launch_at_once = " << pTestLUN->max_IOs_tried_to_launch_at_once
                                    << ", max_IOs_launched_at_once = "        << pTestLUN->max_IOs_launched_at_once
                                    << ", max_IOs_reaped_at_once = "          << pTestLUN->max_IOs_reaped_at_once
                                    << ", max_consecutive_count_event_fd_writeback = " << pTestLUN->max_consecutive_count_event_fd_writeback
                                    << ", max_consecutive_count_event_fd_behind = "    << pTestLUN->max_consecutive_count_event_fd_behind
                                    << std::endl;

                                for (auto& pear : pTestLUN->workloads)
                                {
                                    o << "For Workload" << pear.first << " : "
                                        << "workload_max_queue_depth = " << pear.second.workload_max_queue_depth << std::endl;

                                }
                            }
                            o << "Over all TestLUNs "
                                << ", max_IOs_tried_to_launch_at_once = " << overall_max_IOs_tried_to_launch_at_once
                                << ", max_IOs_launched_at_once = "        << overall_max_IOs_launched_at_once
                                << ", max_IOs_reaped_at_once = "          << overall_max_IOs_reaped_at_once
                                << ", max_consecutive_count_event_fd_writeback = " << overall_max_consecutive_count_event_fd_writeback
                                << ", max_consecutive_count_event_fd_behind = "    << overall_max_consecutive_count_event_fd_behind
                                << std::endl;

                            log(slavethreadlogfile,o.str());
                        }
                    }

                    close_all_fds();

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
		}
	}
}

void WorkloadThread::linux_AIO_driver_run_subinterval()
// see also https://code.google.com/p/kernel/wiki/AIOUserGuide
{
#if defined(IVYDRIVER_TRACE)
    { wt_callcount_linux_AIO_driver_run_subinterval++; if (wt_callcount_linux_AIO_driver_run_subinterval <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << physical_core << " hyperthread " << hyperthread << ':' << wt_callcount_linux_AIO_driver_run_subinterval << ") ";
    o << "Entering WorkloadThread::linux_AIO_driver_run_subinterval()."; log(slavethreadlogfile,o.str()); } }
#endif
    main_loop_passes = IOs_generated = IOs_launched = IOs_harvested = IOs_popped = fruitless_passes = 0;

    if (routine_logging)
    {
        std::ostringstream o;
        ivytime now; now.setToNow();
        now = now - thread_view_subinterval_end;
        o << "WorkloadThread::linux_AIO_driver_run_subinterval() running subinterval which will end at "
            << thread_view_subinterval_end.format_as_datetime_with_ns()
            << " - " << now.format_as_duration_HMMSSns() << " from now " << std::endl;
        log(slavethreadlogfile,o.str());
    }

    epoll_wait_until_time.setToZero();

        // reap_IOs() waits until an I/O completion event occurs or
        // until this time when the next I/O needs to be started.

        // epoll_wait_until_time is set by start_IOs().

        // In start_IOs(), if there are no I/Os scheduled for the future
        // which already now have an available AIO slot,
        // epoll_wait_until_time is set to the
        // end of the subinterval.

        // Zero means don't wait in reap_IOs(), because we may have work to do
        // in start_IOs(), pop_and_process_an_Eyeo() or generate_an_IO().

	while (true)
	{
	    main_loop_passes++;

		now.setToNow();

		if (now >= thread_view_subinterval_end) break;

        unsigned int reaped = reap_IOs(now); IOs_harvested += reaped;

        unsigned int started = start_IOs(); IOs_launched += started;

        if ( started > 0 ) { epoll_wait_until_time.setToZero(); continue; }

        unsigned int popped = pop_and_process_an_Eyeo();  IOs_popped += popped;

        if ( popped > 0 ) { epoll_wait_until_time.setToZero(); continue; }

        unsigned int generated = generate_an_IO(); IOs_generated += generated;

        if ( generated > 0 ) { epoll_wait_until_time.setToZero(); continue; }

        if (reaped == 0) { fruitless_passes++; }

        if (ivydriver.spinloop)
        {
            epoll_wait_until_time.setToZero();
        }
        else
        {
            // We did not start any I/Os, post-process any I/Os, or generate any I/Os, so we should wait in epoll_wait.

            epoll_wait_until_time = thread_view_subinterval_end;

            now.setToNow();

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

                        if (pEyeo->scheduled_time > now)  // with IOPS=max, scheduled_time is zero.
                        {
                            // The Eyeo has an empty AIO context slot and is just waiting for the scheduled launch time.

                            if (epoll_wait_until_time > pEyeo->scheduled_time)
                            {
                                epoll_wait_until_time = pEyeo->scheduled_time;
                            }
                        }
                        else // we have an open AIO slot and an I/O ready to launch, don't wait in reap_IOs().
                        {
                            epoll_wait_until_time.setToZero();
                        }
                    }
                }
            }
        }
    }

#ifdef IVYDRIVER_TRACE
    log_bookmark_counters();
#endif

    number_of_IOs_running_at_end_of_subinterval = 0;

    for (TestLUN* pTestLUN : pTestLUNs)
    {
        for (const auto& pear : pTestLUN->workloads)
        {
            number_of_IOs_running_at_end_of_subinterval += pear.second.running_IOs.size();
        }
    }

    if (routine_logging)
    {
        std::ostringstream o;
        o << "For step number " << ivydriver.step_number << " subinterval number " << thread_view_subinterval_number << " LUNs = " << pTestLUNs.size() << " - ";
        o << "main_loop_passes = " << main_loop_passes
            << ", IOs_generated = " << IOs_generated
            << ", IOs_launched = " << IOs_launched
            << ", IOs_harvested = " << IOs_harvested
            << ", IOs_popped = " << IOs_popped
            << ", fruitless_passes = " << fruitless_passes;
        if (main_loop_passes > 0) { o << ", fruitless passes as % of total passes = " << (100.0 * ((long double) fruitless_passes) / ((long double) main_loop_passes)) << "%"; }
        if (IOs_launched > 0)     { o << ", passes per I/O launched = " << ((long double) main_loop_passes) / ((long double) IOs_launched); }
        log(slavethreadlogfile,o.str());
    }


	return;
}


void WorkloadThread::cancel_stalled_IOs()
{
#if defined(IVYDRIVER_TRACE)
    { wt_callcount_cancel_stalled_IOs++; if (wt_callcount_cancel_stalled_IOs <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << physical_core << " hyperthread " << hyperthread << ':' << wt_callcount_cancel_stalled_IOs << ") ";
    o << "Entering WorkloadThread::cancel_stalled_IOs()."; log(slavethreadlogfile,o.str()); } }
#endif

    for (auto& pTestLUN : pTestLUNs)
    {
        for (auto& pear : pTestLUN->workloads)
        {
            Workload* pWorkload = &(pear.second);
            {
                std::list<struct iocb*> l;

                for (auto& p_iocb : pWorkload->running_IOs)
                {
                    Eyeo* p_Eyeo = (Eyeo*) p_iocb->aio_data;
                    if (p_Eyeo->since_start_time() > ivytime(IO_TIME_LIMIT_SECONDS)) l.push_back(p_iocb);
                }
                // I didn't want to be iterating over running_IOs when cancelling I/Os, as the cancel deletes the I/O from running_IOs.
                for (auto p_iocb : l) pTestLUN->ivy_cancel_IO(p_iocb);
            }
        }
    }
}


unsigned int WorkloadThread::start_IOs()
{
#if defined(IVYDRIVER_TRACE)
    { wt_callcount_start_IOs++; if (wt_callcount_start_IOs <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << physical_core << " hyperthread " << hyperthread << ':' << wt_callcount_start_IOs << ") ";
    o << "Entering WorkloadThread::start_IOs().";log(slavethreadlogfile,o.str()); } }
#endif


    if (pTestLUNs.size() == 0) return 0;

    std::vector<TestLUN*>::iterator it = pTestLUN_start_IOs_bookmark;

#ifdef IVYDRIVER_TRACE
    (*it)->start_IOs_bookmark_count++;
#endif

    unsigned int n {0};

    while (true)
    {
        TestLUN* pTestLUN = *it;

#ifdef IVYDRIVER_TRACE
        pTestLUN->start_IOs_body_count++;
#endif

        n += pTestLUN->start_IOs();

        it++; if (it == pTestLUNs.end()) { it = pTestLUNs.begin(); }

        if (it == pTestLUN_start_IOs_bookmark) break;
    }

    pTestLUN_start_IOs_bookmark ++;

    if (pTestLUN_start_IOs_bookmark == pTestLUNs.end())
    {
        pTestLUN_start_IOs_bookmark =  pTestLUNs.begin();
    }

    return n;
}

unsigned int /* # of I/Os */ WorkloadThread::reap_IOs(const ivytime& now)
{
#if defined(IVYDRIVER_TRACE)
    { wt_callcount_reap_IOs++; if (wt_callcount_reap_IOs <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << physical_core << " hyperthread " << hyperthread << ':' << wt_callcount_reap_IOs << ") ";
    o << "Entering WorkloadThread::reap_IOs()."; log(slavethreadlogfile,o.str()); } }
#endif

    unsigned int number_of_IOs_harvested {0};

    if (pTestLUNs.size() == 0) return number_of_IOs_harvested;

#ifdef IVYDRIVER_TRACE
    (*itr)->reap_IOs_bookmark_count++;
#endif

    // The way the eventfd / epoll thing works is that
    // an eventfd is a file descriptor connected to an
    // underlying unsigned 64 bit counter in the kernel.

    // We "tag" each I/O with the eventfd, so that when the I/O
    // completes, this causes the uint64_t number 1 to be written
    // to the eventfd's "file", which increments the 64 bit counter
    // by that number 1.

    // Thus the value of the 64 bit unsigned int represents
    // the number of I/Os that have completed since the counter
    // was last reset to zero.

    // Reading the 8 bytes uint64_t value from the eventfd's "file"
    // atomically then resets the value in the counter to zero.

    // The eventfd file descriptor is readable if the underlying
    // counter is non-zero.

    // Then we use epoll to wait until the eventfd becomes
    // readable, meaning that a new I/O completion event
    // is ready to be harvested.

    // There's an event fd for each TestLUN, and the event_fds
    // for the various TestLUNs have been added into the epoll
    // for the WorkloadThread.

    // So the way we reap I/Os is first do an epoll_wait at the
    // WorkloadThread level.  This gives us a set of epoll_events,
    // which each either have a pointer to WorkoadThread (the timerfd's event)
    // or a pointer to a TestLUN that has pending I/O events.

    // So calling epoll_wait tells us which TestLUNs have pending
    // events.  Then in each corresponding TestLUN, we read from
    // the event fd to find out how many pending I/O completions
    // there are.  Then in TestLUN, we keep calling getevents()
    // until we've harvested them all.

    if (now >= thread_view_subinterval_end) return number_of_IOs_harvested;

    ivytime wait_duration;

    if (epoll_wait_until_time <= now) // including epoll_wait_until_time == ivytime(0)
    {
        wait_duration.setToZero();
    }
    else if (thread_view_subinterval_end < epoll_wait_until_time) // I think this can't happen, because start_IOs() initializes to end of subinterval
    {
        wait_duration = thread_view_subinterval_end - now;
    }
    else
    {
        wait_duration = epoll_wait_until_time - now;
    }

    timerfd_setting.it_value.tv_sec = wait_duration.t.tv_sec;
    timerfd_setting.it_value.tv_nsec = wait_duration.t.tv_nsec;

    int rc_ts = timerfd_settime(timer_fd, 0, &timerfd_setting,0);
    if (rc_ts < 0)
    {
        std::ostringstream o;
        o << "failed setting timerfd - errno " << errno << " (" << strerror(errno) << ")." << std::endl;
        throw std::runtime_error(o.str());
    }

    int epoll_event_count {-1};

    while (true)
    {
        if (wait_duration.isZero())
        {
            epoll_event_count = epoll_wait(epoll_fd, p_epoll_events, max_epoll_event_retrieval_count, 0); // 0 means don't wait, just test the fds for readability.
        }
        else
        {
            epoll_event_count = epoll_wait(epoll_fd, p_epoll_events, max_epoll_event_retrieval_count, 125); // 125 ms is an insurance policy in case the timerfd didn't work - this was put in as a precaution after an incident
            //*debug*/epoll_event_count = epoll_wait(epoll_fd, p_epoll_events, max_epoll_event_retrieval_count, -1); //  -1 wait means waitforever.
        }

        if (epoll_event_count != -1 || errno != EINTR) break;
    }

    if (epoll_event_count == -1)
    {
        std::ostringstream o;
        o << "internal programming error - epoll_wait() in WorkloadThread::reap_IOs() for physical core " << physical_core << " hyperthread " << hyperthread
            << "failed with errno " << errno << " - " << std::strerror(errno) << std::endl;
        post_error(o.str());
        return number_of_IOs_harvested;
    }

    if (epoll_event_count == 0) return number_of_IOs_harvested;

    for (unsigned int i = 0; i < (unsigned int) epoll_event_count; i++)
    {
        void* p = (p_epoll_events+i)->data.ptr;

        if ( ((WorkloadThread*)p) == this ) continue; // the timer fd popped.

        TestLUN* pTestLUN = (TestLUN*) p;

        number_of_IOs_harvested += pTestLUN->reap_IOs();
    }

    return number_of_IOs_harvested;
}

unsigned int WorkloadThread::pop_and_process_an_Eyeo()
{
#if defined(IVYDRIVER_TRACE)
    { wt_callcount_pop_and_process_an_Eyeo++; if (wt_callcount_pop_and_process_an_Eyeo <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << physical_core << " hyperthread " << hyperthread << ':' << wt_callcount_pop_and_process_an_Eyeo << ") ";
    o << "Entering WorkloadThread::pop_and_process_an_Eyeo()."; log(slavethreadlogfile,o.str()); } }
#endif

    if (pTestLUNs.size() == 0) return 0;

    std::vector<TestLUN*>::iterator it = pTestLUN_pop_bookmark;

#ifdef IVYDRIVER_TRACE
    (*it)->pop_n_p_bookmark_count++;
#endif
    unsigned int n {0};

    while (true)
    {
        TestLUN* pTestLUN = (*it);

#ifdef IVYDRIVER_TRACE
        pTestLUN->pop_n_p_body_count++;
#endif

        n += pTestLUN->pop_and_process_an_Eyeo();

        if ( n > 0 ) break;

        it++; if (it == pTestLUNs.end()) it = pTestLUNs.begin();

        if (it == pTestLUN_pop_bookmark) break;
    }

    pTestLUN_pop_bookmark++;

    if (pTestLUN_pop_bookmark == pTestLUNs.end())
    {
        pTestLUN_pop_bookmark =  pTestLUNs.begin();
    }

    return n;
}

unsigned int WorkloadThread::generate_an_IO()
{
#if defined(IVYDRIVER_TRACE)
    { wt_callcount_generate_an_IO++; if (wt_callcount_generate_an_IO <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << physical_core << " hyperthread " << hyperthread << ':' << wt_callcount_generate_an_IO << ") ";
    o << "Entering WorkloadThread::generate_an_IO()."; log(slavethreadlogfile,o.str()); } }
#endif

    if (pTestLUNs.size() == 0) return 0;

    auto it = pTestLUN_generate_bookmark;

#ifdef IVYDRIVER_TRACE
    (*it)->generate_bookmark_count++;
#endif

    unsigned int n {0};

    while (true)
    {
        TestLUN* pTestLUN = (*it);

#ifdef IVYDRIVER_TRACE
        pTestLUN->generate_body_count++;
#endif

        n += pTestLUN->generate_an_IO();

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

void WorkloadThread::catch_in_flight_IOs_after_last_subinterval()
{
#if defined(IVYDRIVER_TRACE)
    { wt_callcount_catch_in_flight_IOs_after_last_subinterval++; if (wt_callcount_catch_in_flight_IOs_after_last_subinterval <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << physical_core << " hyperthread " << hyperthread << ':' << wt_callcount_catch_in_flight_IOs_after_last_subinterval << ") ";
    o << "Entering WorkloadThread::catch_in_flight_IOs_after_last_subinterval()."; log(slavethreadlogfile,o.str()); } }
#endif

    ivytime limit_time;
    limit_time.setToNow();
    limit_time = limit_time + ivytime(1.5);

    // First we try to harvest I/Os normally for 1.5 seconds

    while (workload_thread_queue_depth > 0)
    {
        ivytime now, then;
        now.setToNow();
        then = now + ivytime(0.1); // 1/10th of a second

        if (now > limit_time) break;

        for (auto& pTestLUN : pTestLUNs)
        {
            if (pTestLUN->testLUN_queue_depth > 0)
            {
                pTestLUN->catch_in_flight_IOs();
                // This harvests as many as it can waiting up to 1 ms.
            }
        }
        then.waitUntilThisTime();
    }

    // Now that we have tried to harvest running I/Os for a full
    // subinterval duration, we cancel any remaining I/Os.

    for (auto& pTestLUN : pTestLUNs)
    {
        for (auto& pear : pTestLUN->workloads)
        {
            auto pWorkload = &pear.second;

//            for (auto& p_iocb : pWorkload->running_IOs)
//            {
//                pTestLUN->ivy_cancel_IO(p_iocb);
//// =================================================================  This commented out because cancelling I/Os always fails.
//// =================================================================  Hoping that when I destroy the AIO context and close the fds, this will deal with any still-running I/Os.
//            }

            pWorkload->running_IOs.clear();
            pWorkload->workload_queue_depth=0;
        }
        pTestLUN->testLUN_queue_depth = 0;
    }

    workload_thread_queue_depth = 0;
}

void WorkloadThread::set_all_queue_depths_to_zero()
{
    workload_thread_queue_depth = 0;
    workload_thread_max_queue_depth = 0;

    for (auto& pTestLUN : pTestLUNs)
    {
        pTestLUN->testLUN_queue_depth = 0;
        pTestLUN->testLUN_max_queue_depth = 0;

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

bool WorkloadThread::close_all_fds()
{
    int rc;

    bool success {true};

    for (auto& pTestLUN : pTestLUNs)
    {
        if (0 != (rc = io_destroy(pTestLUN->act)))
        {
            std::ostringstream o;
            o << "in WorkloadThread::close_all_fds() - io_destroy for AIO context failed return code "
                << rc << ", errno " << errno << " (" << std::strerror(errno) << ")." << std::endl;
            post_error(o.str());
            success = false;
        }

        if(pTestLUN->fd != -1)
        {
            if (0 != (rc = close(pTestLUN->fd)))
            {
                std::ostringstream o;
                o << "in WorkloadThread::close_all_fds() for " << pTestLUN->host_plus_lun << " - close for pTestLUN->fd = " << pTestLUN->fd << " failed return code "
                    << rc << ", errno " << errno << " (" << std::strerror(errno) << ")." << std::endl;
                post_error(o.str());
                success = false;
            }
            else
            {
                pTestLUN->fd = -1;
            }
        }

        if (pTestLUN->event_fd != -1)
        {
            if (0 != (rc = close(pTestLUN->event_fd)))
            {
                std::ostringstream o;
                o << "in WorkloadThread::close_all_fds() - close for event_fd = " << pTestLUN->event_fd << " failed return code "
                    << rc << ", errno " << errno << " (" << std::strerror(errno) << ")." << std::endl;
                post_error(o.str());
                success = false;
            }
            else
            {
                pTestLUN->event_fd = -1;
            }
        }
    }

    if (epoll_fd != -1)
    {
        if (0 != (rc = close(epoll_fd)))
        {
            std::ostringstream o;
            o << "in WorkloadThread::close_all_fds() - close for epoll_fd = " << epoll_fd << " failed return code "
                << rc << ", errno " << errno << " (" << std::strerror(errno) << ")." << std::endl;
            post_error(o.str());
            success = false;
        }
        else
        {
            epoll_fd = -1;
        }
    }

    if (timer_fd != -1)
    {
        if (0 != (rc = close(timer_fd)))
        {
            std::ostringstream o;
            o << "in WorkloadThread::close_all_fds() - close for timer_fd = " << timer_fd << " failed return code "
                << rc << ", errno " << errno << " (" << std::strerror(errno) << ")." << std::endl;
            post_error(o.str());
            success = false;
        }
        else
        {
            timer_fd = -1;
        }
    }

    return success;
}

#ifdef IVYDRIVER_TRACE
void WorkloadThread::log_bookmark_counters()
{
    std::ostringstream o;

    o << std::endl;

    o << "Cumulative bookmark totals:" << std::endl;

    unsigned int total_start_IOs_bookmark_count = 0;
    unsigned int total_reap_IOs_bookmark_count = 0;
    unsigned int total_pop_n_p_bookmark_count = 0;
    unsigned int total_generate_bookmark_count = 0;

    unsigned int total_start_IOs_body_count = 0;
    unsigned int total_reap_IOs_body_count = 0;
    unsigned int total_pop_n_p_body_count = 0;
    unsigned int total_generate_body_count = 0;

    for (auto& pTestLUN : pTestLUNs)
    {
        total_start_IOs_bookmark_count += pTestLUN->start_IOs_bookmark_count;
        total_reap_IOs_bookmark_count  += pTestLUN->reap_IOs_bookmark_count;
        total_pop_n_p_bookmark_count   += pTestLUN->pop_n_p_bookmark_count;
        total_generate_bookmark_count  += pTestLUN->generate_bookmark_count;

        total_start_IOs_body_count += pTestLUN->start_IOs_body_count;
        total_reap_IOs_body_count  += pTestLUN->reap_IOs_body_count;
        total_pop_n_p_body_count   += pTestLUN->pop_n_p_body_count ;
        total_generate_body_count  += pTestLUN->generate_body_count;

        pTestLUN->total_start_IOs_Workload_bookmark_count = 0;
        pTestLUN->total_start_IOs_Workload_body_count = 0;
        pTestLUN->total_pop_one_bookmark_count = 0;
        pTestLUN->total_pop_one_body_count = 0;
        pTestLUN->total_generate_one_bookmark_count = 0;
        pTestLUN->total_generate_one_body_count = 0;

        for (auto& pear: pTestLUN->workloads)
        {
            pTestLUN->total_start_IOs_Workload_bookmark_count += pear.second.start_IOs_Workload_bookmark_count;
            pTestLUN->total_start_IOs_Workload_body_count += pear.second.start_IOs_Workload_body_count;
            pTestLUN->total_pop_one_bookmark_count += pear.second.pop_one_bookmark_count;
            pTestLUN->total_pop_one_body_count += pear.second.pop_one_body_count;
            pTestLUN->total_generate_one_bookmark_count += pear.second.generate_one_bookmark_count;
            pTestLUN->total_generate_one_body_count += pear.second.generate_one_body_count;
        }
    }

    for (auto& pTestLUN : pTestLUNs)
    {
        o << pTestLUN->host_plus_lun << ": ";

        o << "start_IOs"

            << " bookmark " << pTestLUN->start_IOs_bookmark_count
                << " (" << std::fixed << std::setprecision(2)
                << (100.0 * ((ivy_float) pTestLUN->start_IOs_bookmark_count) / ((ivy_float) total_start_IOs_bookmark_count)) << "%)"

            << " body " << pTestLUN->start_IOs_body_count
                << " (" << std::fixed << std::setprecision(2)
                << (100.0 * ((ivy_float) pTestLUN->start_IOs_body_count) / ((ivy_float) total_start_IOs_body_count)) << "%)";

        o << ", reap_IOs"

            << " bookmark " << pTestLUN->reap_IOs_bookmark_count
                << " (" << std::fixed << std::setprecision(2)
                << (100.0 * ((ivy_float) pTestLUN->reap_IOs_bookmark_count) / ((ivy_float) total_reap_IOs_bookmark_count)) << "%)"

            << " body " << pTestLUN->reap_IOs_body_count
                << " (" << std::fixed << std::setprecision(2)
                << (100.0 * ((ivy_float) pTestLUN->reap_IOs_body_count) / ((ivy_float) total_reap_IOs_body_count)) << "%)";

        o << ", pop_n_p"

            << " bookmark " << pTestLUN->pop_n_p_bookmark_count
                << " (" << std::fixed << std::setprecision(2)
                << (100.0 * ((ivy_float) pTestLUN->pop_n_p_bookmark_count) / ((ivy_float) total_pop_n_p_bookmark_count)) << "%)"

            << " body " << pTestLUN->pop_n_p_body_count
                << " (" << std::fixed << std::setprecision(2)
                << (100.0 * ((ivy_float) pTestLUN->pop_n_p_body_count) / ((ivy_float) total_pop_n_p_body_count)) << "%)";


        o << ", generate"

            << " bookmark " << pTestLUN->generate_bookmark_count
                << " (" << std::fixed << std::setprecision(2)
                << (100.0 * ((ivy_float) pTestLUN->generate_bookmark_count) / ((ivy_float) total_generate_bookmark_count)) << "%)"

            << " body " << pTestLUN->generate_body_count
                << " (" << std::fixed << std::setprecision(2)
                << (100.0 * ((ivy_float) pTestLUN->generate_body_count) / ((ivy_float) total_generate_body_count)) << "%)";

        o << std::endl;


        for (auto& pear: pTestLUN->workloads)
        {
            o << pear.second.workloadID << " : " << pear.second.brief_status() << std::endl;

            o << "start_IOs_Workload"

                << " bookmark " << pear.second.start_IOs_Workload_bookmark_count
                    << " (" << std::fixed << std::setprecision(2)
                    << (100.0 * ((ivy_float) pear.second.start_IOs_Workload_bookmark_count ) / ((ivy_float) pTestLUN->total_start_IOs_Workload_bookmark_count)) << "%)"

                << " body " << pear.second.start_IOs_Workload_body_count
                    << " (" << std::fixed << std::setprecision(2)
                    << (100.0 * ((ivy_float) pear.second.start_IOs_Workload_body_count) / ((ivy_float) pTestLUN->total_start_IOs_Workload_body_count)) << "%)";

            o << "pop_one"

                << " bookmark " << pear.second.pop_one_bookmark_count
                    << " (" << std::fixed << std::setprecision(2)
                    << (100.0 * ((ivy_float) pear.second.pop_one_bookmark_count ) / ((ivy_float) pTestLUN->total_pop_one_bookmark_count)) << "%)"

                << " body " << pear.second.pop_one_body_count
                    << " (" << std::fixed << std::setprecision(2)
                    << (100.0 * ((ivy_float) pear.second.pop_one_body_count) / ((ivy_float) pTestLUN->total_pop_one_body_count)) << "%)";

            o << "generate_one"

                << " bookmark " << pear.second.generate_one_bookmark_count
                    << " (" << std::fixed << std::setprecision(2)
                    << (100.0 * ((ivy_float) pear.second.generate_one_bookmark_count ) / ((ivy_float) pTestLUN->total_generate_one_bookmark_count)) << "%)"

                << " body " << pear.second.generate_one_body_count
                    << " (" << std::fixed << std::setprecision(2)
                    << (100.0 * ((ivy_float) pear.second.generate_one_body_count) / ((ivy_float) pTestLUN->total_generate_one_body_count)) << "%)";


            o << std::endl;
        }
    }

    log(slavethreadlogfile,o.str());

    return;
}
#endif



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
                for (struct iocb* p_iocb : pear.second.running_IOs)
                {
                    Eyeo* pEyeo = (Eyeo*) p_iocb;

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

    {
        std::ostringstream o;
        o << "WorkloadThread::check_for_long_running_IOs() held the lock and ran for " << run_time.getAsNanoseconds() << " nanoseconds." << std::endl;
        log(slavethreadlogfile,o.str());
    }

    return;
}






















