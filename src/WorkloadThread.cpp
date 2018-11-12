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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>, Kumaran Subramaniam <kumaran.subramaniam@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.

#include <stdint.h> /* for uint64_t */
#include <fcntl.h>  /* for open64() O_RDWR etc. */
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdio.h>              /* for perror() */
#include <unistd.h>             /* for syscall() */
#include <sys/syscall.h>        /* for __NR_* definitions */
#include <linux/aio_abi.h>      /* for AIO types and constants */
#include <malloc.h>  /* for memalign */
#include <errno.h>   /* creates the external symbol errno */
#include <scsi/sg.h>
#include <string.h>
#include <random>  /* uniform int distribution, etc. */
#include <stack>
#include <queue>
#include <list>
#include <exception>
#include <math.h>  /* for ceil() */
#include <csignal>

#include "ivytime.h"
#include "ivydefines.h"
#include "ivyhelpers.h"
#include "RunningStat.h"
#include "Eyeo.h"
#include "LUN.h"
#include "WorkloadID.h"
#include "IosequencerInput.h"
#include "Iosequencer.h"
#include "ivylinuxcpubusy.h"
#include "RunningStat.h"
#include "Accumulators_by_io_type.h"
#include "SubintervalOutput.h"
#include "Subinterval.h"
#include "WorkloadThread.h"
#include "IosequencerRandom.h"
#include "IosequencerRandomSteady.h"
#include "IosequencerRandomIndependent.h"
#include "IosequencerSequential.h"
#include "ivyslave.h"
#include "TestLUN.h"

//#define IVYSLAVE_TRACE
// IVYSLAVE_TRACE defined here in this source file rather than globally in ivydefines.h so that
//  - the CodeBlocks editor knows the symbol is defined and highlights text accordingly.
//  - you can turn on tracing separately for each class in its own source file.

//#define TRACE_SUBINTERVAL_THUMBNAILS

extern bool routine_logging;

pid_t get_subthread_pid()
{
    return syscall(SYS_gettid);
}

void WorkloadThread::die_saying(std::string m)
{
    dying_words = m;
    log (slavethreadlogfile,dying_words);
    state=ThreadState::died;
    slaveThreadConditionVariable.notify_all();
    exit(-1);
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


WorkloadThread::WorkloadThread(std::mutex* p_imm, unsigned int c)
    : p_ivyslave_main_mutex(p_imm), core(c)
{}

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

		o<< "WorkloadThreadRun() fireup for core = " << core << "." << std::endl;

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
//   ivyslave to set "ivyslave_main_posted_command" to the value true.
//
//   Before turning on the "ivyslave_main_posted_command" flag and notifying,
//   ivyslave sets the actual command value into a MainThreadCommand enum class
//   variable called "ivyslave_main_says".
//
//   The MainThreadCommand enum class has four values, "start", "keep_going", "stop", and "die".
//
//       When the workload thread wakes up and sees that ivyslave_main_posted_command == true, we first turn it off for next time.
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
//                 You just finished the last cooldown subinterval.
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

	slaveThreadConditionVariable.notify_all();   // ivyslave waits for the thread to be in waiting for command state.

	while (true) // loop that waits for "run" commands
	{
        // indent level in loop waiting for run commands

		{
		    // runs with lock held

			std::unique_lock<std::mutex> wkld_lk(slaveThreadMutex);
			while (!ivyslave_main_posted_command) slaveThreadConditionVariable.wait(wkld_lk);

			if (routine_logging) log(slavethreadlogfile,std::string("Received command from host - ") + mainThreadCommandToString(ivyslave_main_says));

			ivyslave_main_posted_command=false;

			switch (ivyslave_main_says)
			{
				case MainThreadCommand::start :
				{
					break;
				}
				case MainThreadCommand::die :
				{
					if (routine_logging) log (slavethreadlogfile,std::string("WorkloadThread dutifully dying upon command.\n"));
					state=ThreadState::exited_normally;
					wkld_lk.unlock();
					slaveThreadConditionVariable.notify_all();
					return;
				}
				default:
				{
                    std::ostringstream o; o << "<Error> WorkloadThread - in main loop waiting for commands didn\'t get \"start\" or \"die\" when expected.\n"
                        << "Occured at line " << __LINE__ << " of " << __FILE__ << std::endl;
                    wkld_lk.unlock();
                    die_saying(o.str());
                }
			}
		}  // lock released

		// "start" command received.

		slaveThreadConditionVariable.notify_all();

        thread_view_subinterval_number = -1;

#ifdef IVYSLAVE_TRACE
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
		    pTestLUN->testLUN_furthest_behind_weighted_IOPS_max_skew_progress = 0.0;
		    pTestLUN->launch_count = 0;

#ifdef IVYSLAVE_TRACE
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

#ifdef IVYSLAVE_TRACE
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

		dispatching_latency_seconds.clear();
        lock_aquisition_latency_seconds.clear();
        switchover_completion_latency_seconds.clear();

        set_all_queue_depths_to_zero();

		epoll_fd = epoll_create(1 /* "size" ignored since Linux kernel 2.6.8 */ );
		if (epoll_fd == -1)
        {
            std::ostringstream o; o << "<Error> Internal programming error - WorkloadThread - failed trying to create epoll fd - "
                << std::strerror(errno) << std::endl
                << "Occured at line " << __LINE__ << " of " << __FILE__ << std::endl;
            die_saying(o.str());
        }

        for (auto& pTestLUN : pTestLUNs)
        {
            pTestLUN->prepare_linux_AIO_driver_to_start();
            pTestLUN->event_fd = eventfd(0,EFD_NONBLOCK);  // int eventfd(unsigned int initval, int flags);
            if (pTestLUN->event_fd == -1)
            {
                std::ostringstream o; o << "<Error> Internal programming error - WorkloadThread - failed trying to create eventfd for a TestLUN fd - "
                    << std::strerror(errno) << std::endl
                    << "Occured at line " << __LINE__ << " of " << __FILE__ << std::endl;
                die_saying(o.str());
            }
            memset(&(pTestLUN->epoll_ev),0,sizeof(pTestLUN->epoll_ev));
            pTestLUN->epoll_ev.data.ptr = (void*) pTestLUN;
            pTestLUN->epoll_ev.events = EPOLLIN | EPOLLET | EPOLLOUT;

            int rc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pTestLUN->event_fd, &pTestLUN->epoll_ev);
            if (rc == -1)
            {
                std::ostringstream o; o << "<Error> Internal programming error - WorkloadThread - epoll_ctl failed trying to add an event - "
                    << std::strerror(errno) << std::endl
                    << "Occured at line " << __LINE__ << " of " << __FILE__ << std::endl;
                die_saying(o.str());
            }
        }

        if (p_epoll_events != nullptr) delete[] p_epoll_events;

        p_epoll_events = new epoll_event[pTestLUNs.size()]; // throws on failure.

		state=ThreadState::running;

        if (routine_logging) log(slavethreadlogfile,"Finished initialization and now about to start running subintervals.");

        // indent level in loop waiting for run commands

        while (true) // inner loop running subintervals, until we are told "stop" or "die"
        {
            // indent level in loop running subintervals

            thread_view_subinterval_number++;

            if (thread_view_subinterval_number == 0)
            {
                thread_view_subinterval_start = ivyslave.test_start_time;
            }
            else
            {
                thread_view_subinterval_start = thread_view_subinterval_end;
            }
            thread_view_subinterval_end = thread_view_subinterval_start + ivyslave.subinterval_duration;

            if (routine_logging){
                std::ostringstream o;
                o << "core " << core << ": Top of subinterval " << thread_view_subinterval_number
                << " " << thread_view_subinterval_start.format_as_datetime_with_ns()
                << " to " << thread_view_subinterval_end.format_as_datetime_with_ns()
                << " with subinterval_duration = " << ivyslave.subinterval_duration.format_as_duration_HMMSSns()
                << std::endl;
                    log(slavethreadlogfile,o.str());
            }

            // This nested loop to set hot zone parameters.
            for (auto& pTestLUN : pTestLUNs)
            {
                for (auto& peach : pTestLUN->workloads)
                {
                    {
                        Workload& wrkld = peach.second;

                        if (wrkld.p_current_IosequencerInput->hot_zone_size_bytes > 0)
                        {
                            if (wrkld.p_my_iosequencer->instanceType() == "random_steady"
                             || wrkld.p_my_iosequencer->instanceType() == "random_independent")
                            {
                                IosequencerRandom* p = (IosequencerRandom*) wrkld.p_my_iosequencer;
                                if (!p->set_hot_zone_parameters(wrkld.p_current_IosequencerInput))
                                {
                                    std::ostringstream o;
                                    o << "<Error> Internal programming error - Workload thread for core " << core
                                        << " working on WorkloadID " << wrkld.workloadID.workloadID
                                        << " - call to IosequencerRandom::set_hot_zone_parameters(() failed.\n"
                                        << "Occurred at line " << __LINE__ << " of " << __FILE__ << std::endl;
                                    die_saying(o.str());
                                }
                            }
                        }
                    }
                }
            }

            try {
                // now we are running a subinterval
                if (!linux_AIO_driver_run_subinterval())
                {
                    std::ostringstream o; o << "<Error> Internal programming error - "
                        << "WorkloadThread for core " << core
                        << " - call to linux_AIO_driver_run_subinterval() failed.\n"
                        << "Occurred at line " << __LINE__ << " of " << __FILE__ << std::endl;
                    die_saying(o.str());
                }
            }
            catch (std::exception& e)
            {
                std::ostringstream o;
                o << "<Error> Internal programming error - failed running linux_AIO_driver_run_subinterval() - "
                    << e.what() << " - at line " << __LINE__ << " of " << __FILE__ << "." << std::endl;
                die_saying(o.str());
            }

            // end of subinterval, flip over to other subinterval or stop

            // First record the dispatching latency,
            // the time from when the clock clicked over to a new subinterval
            // to the time that the WorkloadThread code starts running.

            ivytime before_getting_lock, dispatch_time;


            if ( pTestLUNs.size() == 0)
            {
                dispatching_latency_seconds.push(0.0);

            }
            else
            {
                switchover_begin = ivyslave.ivyslave_view_subinterval_end;

                before_getting_lock.setToNow();

                dispatch_time = before_getting_lock - ivyslave.ivyslave_view_subinterval_end;

                dispatching_latency_seconds.push(dispatch_time.getlongdoubleseconds());
            }

            if (routine_logging)
            {
                std::ostringstream o;
                o << "core " << core << ": Bottom of subinterval " << thread_view_subinterval_number
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

                ivyslave_main_posted_command=false; // for next time

                // we still have the lock

                if (MainThreadCommand::die == ivyslave_main_says)
                {
                    dying_words = "WorkloadThread dutifully dying upon command.\n";
                    if (routine_logging) log(slavethreadlogfile,dying_words);
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
                        {
#ifdef TRACE_SUBINTERVAL_THUMBNAILS
                            {
                                Workload& w = peach.second;
                                std::ostringstream o;
                                o << "Workload " << w.workloadID
                                << "output summary " << w.p_current_SubintervalOutput->thumbnail(ivyslave.subinterval_duration)
                                << " - input " << w.p_current_IosequencerInput->toString() << std::endl;
                                log(slavethreadlogfile, o.str());
                            }
#endif // TRACE_SUBINTERVAL_THUMBNAILS
                            peach.second.switchover();
                        }
                    }
                }

        // indent level in loop waiting for run commands
            // indent level in loop running subintervals
                // indent level in locked section at subinterval switchover

                if (MainThreadCommand::stop == ivyslave_main_says)
                {
                    if (routine_logging) log(slavethreadlogfile,std::string("WorkloadThread stopping at end of subinterval.\n"));

                    catch_in_flight_IOs_after_last_subinterval();

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
                            o << "For TestLUN"                            << pTestLUN->host_plus_lun << " : "
                                << "testLUN_max_queue_depth = "           << pTestLUN->testLUN_max_queue_depth
                                << ", max_IOs_tried_to_launch_at_once = " << pTestLUN->max_IOs_tried_to_launch_at_once
                                << ", max_IOs_launched_at_once = "        << pTestLUN->max_IOs_launched_at_once
                                << ", max_IOs_reaped_at_once = "          << pTestLUN->max_IOs_reaped_at_once
                                << std::endl;

                            for (auto& pear : pTestLUN->workloads)
                            {
                                o << "For Workload" << pear.first << " : "
                                    << "workload_max_queue_depth = " << pear.second.workload_max_queue_depth << std::endl;

                            }
                        }

                        log(slavethreadlogfile,o.str());
                    }

                    ivyslave_main_posted_command = false;

                    state=ThreadState::waiting_for_command;
                    wkld_lk.unlock();
                    slaveThreadConditionVariable.notify_all();

                    goto wait_for_command;
                     // end of processing "stop" command
                }

        // indent level in loop waiting for run commands
            // indent level in loop running subintervals
                // indent level in locked section at subinterval switchover

				if (MainThreadCommand::keep_going != ivyslave_main_says)
				{
				    std::ostringstream o; o <<" <Error> WorkloadThread only looks for \"stop\" or \"keep_going\" commands at end of subinterval.  Received "
				        << mainThreadCommandToString(ivyslave_main_says) << ".  Occurred at " << __FILE__ << " line " << __LINE__ << "\n";
					wkld_lk.unlock();
				    die_saying(o.str());
				}

                // MainThreadCommand::keep_going

				for (auto& pTestLUN : pTestLUNs)
				{
				    for (auto& pear : pTestLUN->workloads)
				    {
				        Workload* pWorkload = & pear.second;
                        if ( pWorkload->subinterval_array[pWorkload->currentSubintervalIndex].subinterval_status != subinterval_state::ready_to_run )
                        {
                            std::ostringstream o; o << "<Error> Internal programming error - "
                                << "WorkloadThread told to keep going, but next subinterval not marked READY_TO_RUN" << std::endl
                                << " for workload " << pWorkload->workloadID.workloadID << std::endl
                                << "Occurred at " << __FILE__ << " line " << __LINE__ << "\n";
                            wkld_lk.unlock();
                            die_saying(o.str());
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

bool WorkloadThread::linux_AIO_driver_run_subinterval()
// see also https://code.google.com/p/kernel/wiki/AIOUserGuide
{
#if defined(IVYSLAVE_TRACE)
    { wt_callcount_linux_AIO_driver_run_subinterval++; if (wt_callcount_linux_AIO_driver_run_subinterval <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(core" << core << ':' << wt_callcount_linux_AIO_driver_run_subinterval << ") ";
    o << "Entering WorkloadThread::linux_AIO_driver_run_subinterval()."; log(slavethreadlogfile,o.str()); } }
#endif

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

	while (true)
	{
        top_of_loop:

		now.setToNow();

		if (now >= thread_view_subinterval_end) break;
		    // we will post-process any I/Os that ended within the subinterval before returning.

        unsigned int n;

        n = reap_IOs();

        if ( n > 0 )
        {
            goto top_of_loop;
        }

        n = start_IOs();

        if ( n > 0 ) { goto top_of_loop; }

        n = pop_and_process_an_Eyeo();

        if ( n > 0 ) { goto top_of_loop; }

        n = generate_an_IO();

        if ( n > 0 ) { goto top_of_loop; }

        // We wait for the soonest of
        //   - end of subinterval
        //   - time to start next scheduled I/O over my TestLUNs, if there is a scheduled (not IOPS=max) I/O.

        ivytime next_overall_io = ivytime(0);

        for (auto& pTestLUN : pTestLUNs)
        {
            ivytime next_this_LUN = pTestLUN->next_scheduled_io();

            if (next_overall_io == ivytime(0))
            {
                next_overall_io = next_this_LUN;
            }
            else if (next_this_LUN   != ivytime(0) && next_this_LUN < next_overall_io)
            {
                next_overall_io = next_this_LUN;
            }
        }

        ivytime wait_until_this_time = thread_view_subinterval_end;

        if (next_overall_io != ivytime(0) && next_overall_io < thread_view_subinterval_end)
        {
            wait_until_this_time = next_overall_io;
        }

        ivytime wait_duration;

        if (wait_until_this_time <= now)
        {
            wait_duration = ivytime(0);
        }
        else
        {
            wait_duration = wait_until_this_time - now;
        }

        if (ivyslave.spinloop) goto top_of_loop;

        int wait_ms = static_cast<int>(wait_duration.Milliseconds());

        int epoll_rc = epoll_wait(epoll_fd, p_epoll_events, pTestLUNs.size(), wait_ms);

        if (epoll_rc < 0)
        {
            std::ostringstream o;
            o << "<Error> Internal programming error - in WorkloadThread, epoll_wait() failed - "
                << " return code = " << epoll_rc << " - " << strerror(errno)
                << std::endl << "Occurred at " << __FILE__ << " line " << __LINE__ << "\n";
            die_saying(o.str());
        }

        if (epoll_rc > 0)
        {
            epoll_event* p_ee = p_epoll_events;

            for ( int i = 0; i < epoll_rc; i++ )
            {
               TestLUN* pTestLUN = (TestLUN*) p_ee->data.ptr;
               pTestLUN->reap_IOs();
               p_ee++;
            }
        }

        goto top_of_loop;
    }

#ifdef IVYSLAVE_TRACE
    log_bookmark_counters();
#endif

	return true;
}


void WorkloadThread::cancel_stalled_IOs()
{
#if defined(IVYSLAVE_TRACE)
    { wt_callcount_cancel_stalled_IOs++; if (wt_callcount_cancel_stalled_IOs <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(core" << core << ':' << wt_callcount_cancel_stalled_IOs << ") ";
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
#if defined(IVYSLAVE_TRACE)
    { wt_callcount_start_IOs++; if (wt_callcount_start_IOs <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(core" << core << ':' << wt_callcount_start_IOs << ") ";
    o << "Entering WorkloadThread::start_IOs().";log(slavethreadlogfile,o.str()); } }
#endif

    if (pTestLUNs.size() == 0) return 0;

    std::vector<TestLUN*>::iterator it = pTestLUN_start_IOs_bookmark;

#ifdef IVYSLAVE_TRACE
    (*it)->start_IOs_bookmark_count++;
#endif

    unsigned int n {0};

    while (true)
    {
        TestLUN* pTestLUN = *it;

#ifdef IVYSLAVE_TRACE
        pTestLUN->start_IOs_body_count++;
#endif

        n += pTestLUN->start_IOs();

        if ( n > 0 )
        {
            break; // This could have started I/Os for multiple Workloads within the LUN,
                   // but after this amount of work, we will go back to check for
                   // I/O completions before processing the next TestLUN.
                   // A round robbin iterator bookmark gives each TestLUN the same
                   // priority overall.
        }

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

unsigned int WorkloadThread::reap_IOs()
{
#if defined(IVYSLAVE_TRACE)
    { wt_callcount_reap_IOs++; if (wt_callcount_reap_IOs <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(core" << core << ':' << wt_callcount_reap_IOs << ") ";
    o << "Entering WorkloadThread::reap_IOs()."; log(slavethreadlogfile,o.str()); } }
#endif

    if (pTestLUNs.size() == 0) return 0;

    std::vector<TestLUN*>::iterator itr = pTestLUN_reap_IOs_bookmark;

#ifdef IVYSLAVE_TRACE
    (*itr)->reap_IOs_bookmark_count++;
#endif

    unsigned int n {0};

    while (true)
    {
        TestLUN* pTestLUN = *itr;

#ifdef IVYSLAVE_TRACE
        pTestLUN->reap_IOs_body_count++;
#endif

        n += pTestLUN->reap_IOs();

        itr++;

        if (itr == pTestLUNs.end())
        {
            itr = pTestLUNs.begin();
        }

        if (itr == pTestLUN_reap_IOs_bookmark) break;
    }

    pTestLUN_reap_IOs_bookmark++;

    if (pTestLUN_reap_IOs_bookmark == pTestLUNs.end())
    {
        pTestLUN_reap_IOs_bookmark = pTestLUNs.begin();
    }

    return n;
}

unsigned int WorkloadThread::pop_and_process_an_Eyeo()
{
#if defined(IVYSLAVE_TRACE)
    { wt_callcount_pop_and_process_an_Eyeo++; if (wt_callcount_pop_and_process_an_Eyeo <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(core" << core << ':' << wt_callcount_pop_and_process_an_Eyeo << ") ";
    o << "Entering WorkloadThread::pop_and_process_an_Eyeo()."; log(slavethreadlogfile,o.str()); } }
#endif

    if (pTestLUNs.size() == 0) return 0;

    std::vector<TestLUN*>::iterator it = pTestLUN_pop_bookmark;

#ifdef IVYSLAVE_TRACE
    (*it)->pop_n_p_bookmark_count++;
#endif
    unsigned int n {0};

    while (true)
    {
        TestLUN* pTestLUN = (*it);

#ifdef IVYSLAVE_TRACE
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
#if defined(IVYSLAVE_TRACE)
    { wt_callcount_generate_an_IO++; if (wt_callcount_generate_an_IO <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(core" << core << ':' << wt_callcount_generate_an_IO << ") ";
    o << "Entering WorkloadThread::generate_an_IO()."; log(slavethreadlogfile,o.str()); } }
#endif

    if (pTestLUNs.size() == 0) return 0;

    auto it = pTestLUN_generate_bookmark;

#ifdef IVYSLAVE_TRACE
    (*it)->generate_bookmark_count++;
#endif

    unsigned int n {0};

    while (true)
    {
        TestLUN* pTestLUN = (*it);

#ifdef IVYSLAVE_TRACE
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
#if defined(IVYSLAVE_TRACE)
    { wt_callcount_catch_in_flight_IOs_after_last_subinterval++; if (wt_callcount_catch_in_flight_IOs_after_last_subinterval <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(core" << core << ':' << wt_callcount_catch_in_flight_IOs_after_last_subinterval << ") ";
    o << "Entering WorkloadThread::catch_in_flight_IOs_after_last_subinterval() for core " << core << "."; log(slavethreadlogfile,o.str()); } }
#endif

    ivytime half_a_subinterval = ivytime(ivyslave.subinterval_duration.getlongdoubleseconds() / 2.0);

    ivytime limit_time;
    limit_time.setToNow();
    limit_time = limit_time + half_a_subinterval;

    // First we try to harvest I/Os normally for half a subinterval

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

            for (auto& p_iocb : pWorkload->running_IOs)
            {
                pTestLUN->ivy_cancel_IO(p_iocb);
            }

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


void WorkloadThread::post_Error_for_main_thread_to_say(const std::string& msg)
{
    std::string e {"<Error> "};

    std::string s {};

    if (startsWith(msg,e)) { s = msg; }
    else                   { s = e + msg; }

    {
        std::lock_guard<std::mutex> lk_guard(*p_ivyslave_main_mutex);
        ivyslave.workload_thread_error_messages.push_back(s);
    }
}


void WorkloadThread::post_Warning_for_main_thread_to_say(const std::string& msg)
{
    std::string w {"<Warning> "};

    std::string s {};

    if (startsWith(msg,w)) { s = msg; }
    else                   { s = w + msg; }

    {
        std::lock_guard<std::mutex> lk_guard(*p_ivyslave_main_mutex);
        ivyslave.workload_thread_warning_messages.push_back(s);
    }
}

#ifdef IVYSLAVE_TRACE
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


























