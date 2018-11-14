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
#include <string.h>  /* for memset, strerror() / strerror_r() */
#include <random>  /* uniform int distribution, etc. */
#include <stack>
#include <queue>
#include <list>
#include <exception>
#include <math.h>  /* for ceil() */

#include "ivytime.h"
#include "ivydefines.h"
#include "ivyhelpers.h"
#include "RunningStat.h"
#include "Eyeo.h"
#include "LUN.h"
#include "WorkloadID.h"
#include "iosequencer_stuff.h"
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

#include "DedupePatternRegulator.h"

extern bool routine_logging;
static uint64_t offset;


// for some strange reason, there's no header file for these system call wrapper functions
inline int io_setup(unsigned nr, aio_context_t *ctxp)                   { return syscall(__NR_io_setup, nr, ctxp); }
inline int io_destroy(aio_context_t ctx)                                { return syscall(__NR_io_destroy, ctx); }
inline int io_submit(aio_context_t ctx, long nr,  struct iocb **iocbpp) { return syscall(__NR_io_submit, ctx, nr, iocbpp); }
inline int io_getevents(aio_context_t ctx, long min_nr, long max_nr, struct io_event *events, struct timespec *timeout)
	                                                                    { return syscall(__NR_io_getevents, ctx, min_nr, max_nr, events, timeout); }
inline int io_cancel(aio_context_t ctx, struct iocb * p_iocb, struct io_event *p_io_event){ return syscall(__NR_io_cancel, ctx, p_iocb, p_io_event); };
//#
//# for some strange reason, in retrospect, the reason why there's no header file
//# for these system call wapper functions, is probably becuse you would normally
//# want to use the POSIX platform-independent calls for portability, and you could very
//# well not lose any access to functionality.  That might be one reason why the
//# calls directly to the underlying Linux AIO mechanism don't have header files, duh.
//#
//# A to-do would be to see how big of a project it would be, and if we would lose
//# any flexibility if we cut over to using the POSIX AIO type calls.
//#
//# This would be a pre-requisite to build ivy on other non-Linux platforms.

//######## It turns out that POSIX AIO uses a thread model internally - that is exactly what we don't want. ########

#include <iostream>

#include "Subinterval.h"


std::ostream& operator<< (std::ostream& o, const ThreadState s)
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
        case ThreadState::stopping:
            o << "ThreadState::stopping";
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


WorkloadThread::WorkloadThread(std::string wID, LUN* pL, long long int lastLBA, std::string parms, std::mutex* p_imm)
        : workloadID(wID), pLUN(pL), maxLBA(lastLBA), iosequencerParameters(parms), dedupe_regulator(nullptr), p_ivyslave_main_mutex(p_imm)
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
	}
	{
		std::ostringstream o;
		o << "mainThreadCommandToString (MainThreadCommand = (decimal) " << ((int) mtc) << ") - internal programming error - invalid MainThreadCommand value." ;
		return o.str();
	}

}

void WorkloadThread::WorkloadThreadRun() {

	if (routine_logging)
	{
		std::ostringstream o;

		o<< "WorkloadThreadRun() fireup for workloadID = \"" << workloadID.workloadID
            << "\", iosequencer parameters = \"" << iosequencerParameters << "\"." << std::endl;

		log(slavethreadlogfile,o.str());
	}
	else
	{
        log(slavethreadlogfile,"For logging of routine (non-error) events, use the ivy -log command line option, like \"ivy -log a.ivyscript\".\n\n");
    }

	if (pLUN->contains_attribute_name(std::string("Sector Size")))
	{
		std::string secsize = pLUN->attribute_value(std::string("Sector Size"));
		trim(secsize);
		if (!isalldigits(secsize))
		{
			std::ostringstream o;
			o << "<Error> WorkloadThreadRun() - workloadID = \"" << workloadID.workloadID << "\" - Sector Size attribute \"" << secsize << "\" was not all digits." << std::endl;
            dying_words = o.str();
			log(slavethreadlogfile, o.str());
			state = ThreadState::died;
			slaveThreadConditionVariable.notify_all();
			return;
		}
		std::istringstream is(secsize);
		is >> iosequencer_variables.sector_size;
	}
	else
	{
		iosequencer_variables.sector_size = 512;
	}

	iosequencer_variables.LUN_size_bytes = (1+ maxLBA) * iosequencer_variables.sector_size;

	if (!pLUN->contains_attribute_name(std::string("LUN name")))
	{
		std::ostringstream o;
		o << "<Error> WorkloadThreadRun() - LUN does not have \"LUN name\" attribute.  This is what the LUN is called in this OS.  In Linux, /dev/sdxxx." << std::endl;
		dying_words = o.str();
		log(slavethreadlogfile, dying_words);
		state = ThreadState::died;
        slaveThreadConditionVariable.notify_all();
		return;
	}

	std::string LUNname = pLUN->attribute_value(std::string("LUN name"));

	iosequencer_variables.fd=open64(LUNname.c_str(),O_RDWR+O_DIRECT);

	if (-1 == iosequencer_variables.fd) {
		std::ostringstream o;
		o << "WorkloadThreadRun() - open64(\"" << LUNname << "\",O_RDWR+O_DIRECT) failed errno = " << errno << " (" << strerror(errno) << ')' << std::endl;
		dying_words = o.str();
		state = ThreadState::died;
		log(slavethreadlogfile, dying_words);
        slaveThreadConditionVariable.notify_all();
		return;
	}



	available_iosequencers[std::string("random_steady")] = new IosequencerRandomSteady(pLUN, slavethreadlogfile, workloadID.workloadID, &iosequencer_variables, this);
	available_iosequencers[std::string("random_independent")] = new IosequencerRandomIndependent(pLUN, slavethreadlogfile, workloadID.workloadID, &iosequencer_variables, this);
	available_iosequencers[std::string("sequential")] = new IosequencerSequential(pLUN, slavethreadlogfile, workloadID.workloadID, &iosequencer_variables, this);


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
                    dying_words = o.str();
                    log (slavethreadlogfile,dying_words);
                    state=ThreadState::died;
                    wkld_lk.unlock();
                    slaveThreadConditionVariable.notify_all();
                    return;  // this is threadRun, so this is the same as exit.
                }
			}
		}  // lock released
		slaveThreadConditionVariable.notify_all();

		currentSubintervalIndex=0;
		otherSubintervalIndex=1;
		subinterval_number=0;

		dispatching_latency_seconds.clear();
        lock_aquisition_latency_seconds.clear();
        switchover_completion_latency_seconds.clear();

		p_current_subinterval = &(subinterval_array[currentSubintervalIndex]);
		p_current_IosequencerInput = & (p_current_subinterval-> input);
		p_current_SubintervalOutput = & (p_current_subinterval->output);
		p_other_subinterval = &(subinterval_array[otherSubintervalIndex]);
		p_other_IosequencerInput = & (p_other_subinterval-> input);
		p_other_SubintervalOutput = & (p_other_subinterval->output);

		std::map<std::string, Iosequencer*>::iterator iogen_ptr_it = available_iosequencers.find(p_current_IosequencerInput->iosequencer_type);
		if (available_iosequencers.end() == iogen_ptr_it)
		{
			std::ostringstream o; o << "<Error> WorkloadThread::WorkloadThreadRun() - Got a \"Go!\" but the first subinterval\'s IosequencerInput\'s iosequencer_type \""
				<< p_current_IosequencerInput->iosequencer_type << "\" is not one of the available iosequencer types." << std::endl
				 << "Occured at line " << __LINE__ << " of " << __FILE__ << std::endl;
			dying_words = o.str();
			log(slavethreadlogfile,dying_words);
			state=ThreadState::died;
            slaveThreadConditionVariable.notify_all();
			return;
		}
		p_my_iosequencer = (*iogen_ptr_it).second;
		if (!p_my_iosequencer->setFrom_IosequencerInput(p_current_IosequencerInput))
		{
			std::ostringstream o; o << "<Error> WorkloadThread::WorkloadThreadRun() - call to iosequencer::setFrom_IosequencerInput(() failed.\n" << std::endl
				 << "Occured at line " << __LINE__ << " of " << __FILE__ << std::endl;
			dying_words = o.str();
			log(slavethreadlogfile,dying_words);
			state=ThreadState::died;
            slaveThreadConditionVariable.notify_all();
			return;
		}
                dedupe_regulator = new DedupePatternRegulator(subinterval_array[0].input.dedupe, pattern_seed);
                log(slavethreadlogfile, dedupe_regulator->logmsg());

                if (p_my_iosequencer->isRandom())
                {
                    if (dedupe_regulator->decide_reuse())
                    {
                        ostringstream o;
                        std::pair<uint64_t, uint64_t> align_pattern;
                        align_pattern = dedupe_regulator->reuse_seed();
                        pattern_seed = align_pattern.first;
                        pattern_alignment = align_pattern.second;
                        offset = pattern_alignment;
                        pattern_number = pattern_alignment;
                        o << "workloadthread - Reuse pattern seed " << pattern_seed <<  " Offset: " << offset << std::endl;
                        log(slavethreadlogfile, o.str());
                    } else
                    {
                        ostringstream o;
                        pattern_seed = dedupe_regulator->random_seed();
                        o << "workloadthread - use Random pattern seed " << pattern_seed <<  std::endl;
                        log(slavethreadlogfile, o.str());
                    }
                } else
                {
                    ostringstream o;
                    o << "Sequential workloadthread - Reuse pattern seed " << pattern_seed <<  std::endl;
                    log(slavethreadlogfile, o.str());
                }

		prepare_linux_AIO_driver_to_start();

		state=ThreadState::running;

		int bufsize = p_current_IosequencerInput->blocksize_bytes;
		while (0 != (bufsize % 4096)) bufsize++;
		long int memory_allocation_size = 4095 + (bufsize * allEyeosThatExist.size());

        if (routine_logging)
        {
            std::ostringstream o;
            o << "getting I/O buffer memory for Eyeos.  blksize= " << p_current_IosequencerInput->blocksize_bytes
                << ", rounded up to multiple of 4096 is bufsize = " << bufsize << ", number of Eyeos = " << allEyeosThatExist.size()
                << ", memory allocation size with 4095 bytes for alignment padding is " << memory_allocation_size << std::endl;
            log(slavethreadlogfile,o.str());
        }

        std::unique_ptr<unsigned char[]> ewe_neek;

        try
        {
            ewe_neek.reset( new unsigned char[memory_allocation_size] );
        }
        catch(const std::bad_alloc& e)
        {
			state=ThreadState::died;
			dying_words = std::string("<Error> WorkloadThread.cpp - out of memory making I/O buffer pool - ") + e.what() + std::string("\n");
            log(slavethreadlogfile,dying_words);
            return;
        }

        if (!ewe_neek)
        {
			dying_words = "<Error> WorkloadThread.cpp out of memory making I/O buffer pool\n";
			state=ThreadState::died;
            log(slavethreadlogfile,dying_words);
            slaveThreadConditionVariable.notify_all();
			return;
        }

        void* vp = (void*) ewe_neek.get();

        uint64_t aio_buf_value = (uint64_t) vp;

        while (0 != (aio_buf_value % 4096)) aio_buf_value++;   // yes, std::align, but we are stuffing these values in iocbs as uint64_t type anyway

        for (auto& pEyeo : allEyeosThatExist)
        {
            pEyeo->eyeocb.aio_buf = aio_buf_value;
            aio_buf_value += bufsize;
        }

        if (routine_logging) log(slavethreadlogfile,"Finished setting aio_buf values in all existing Eyeos.\n");

        // indent level in loop waiting for run commands

        while (true) // inner loop running subintervals, until we are told "stop" or "die"
        {
            // indent level in loop running subintervals

            if (routine_logging){
                std::ostringstream o;
                o << "Top of subinterval " << subinterval_number << " " << p_current_subinterval->start_time.format_as_datetime_with_ns()
                    << " to " << p_current_subinterval->end_time.format_as_datetime_with_ns() << " currentSubintervalIndex=" << currentSubintervalIndex
                    << ", otherSubintervalIndex=" << otherSubintervalIndex << std::endl;
                    log(slavethreadlogfile,o.str());
            }

            if (p_current_IosequencerInput->hot_zone_size_bytes > 0)
            {
                if (p_my_iosequencer->instanceType() == "random_steady" || p_my_iosequencer->instanceType() == "random_independent")
                {
                    IosequencerRandom* p = (IosequencerRandom*) p_my_iosequencer;
                    if (!p->set_hot_zone_parameters(p_current_IosequencerInput))
                    {
                        std::ostringstream o; o << "<Error> WorkloadThread::WorkloadThreadRun() - call to IosequencerRandom::set_hot_zone_parameters(() failed.\n"
                            << "Occurred at line " << __LINE__ << " of " << __FILE__ << std::endl;
                        dying_words = o.str();
                        log(slavethreadlogfile,dying_words);
                        state=ThreadState::died;
                        slaveThreadConditionVariable.notify_all();
                        return;
                    }
                }
            }

            try {
                // now we are running a subinterval
                if (!linux_AIO_driver_run_subinterval())
                {
                    std::ostringstream o; o << "WorkloadThread::WorkloadThreadRun() - call to linux_AIO_driver_run_subinterval() failed.\n"
                        << "Occurred at line " << __LINE__ << " of " << __FILE__ << std::endl;
                    dying_words = o.str();
                    log(slavethreadlogfile,dying_words);
                    state=ThreadState::died;
                    slaveThreadConditionVariable.notify_all();
                    return;
                }
            }
            catch (std::exception& e)
            {
                std::ostringstream o;
                o << "<Error> WorkloadThread::WorkloadThreadRun() - exception during call to linux_AIO_driver_run_subinterval() - " << e.what() << std::endl
                        << "Occurred at line " << __LINE__ << " of " << __FILE__ << std::endl;
                dying_words = o.str();
                log(slavethreadlogfile,dying_words);
                state=ThreadState::died;
                slaveThreadConditionVariable.notify_all();
                return;
            }
            // end of subinterval, flip over to other subinterval or stop

            // First record the dispatching latency,
            // the time from when the clock clicked over to a new subinterval
            // to the time that the WorkloadThread code starts running.

            switchover_begin = p_current_subinterval->end_time;

            ivytime before_getting_lock; before_getting_lock.setToNow();

            ivytime dispatch_time = before_getting_lock - p_current_subinterval->end_time;

            dispatching_latency_seconds.push(dispatch_time.getlongdoubleseconds());

            if (routine_logging)
            {
                std::ostringstream o;
                o << "Bottom of subinterval " << subinterval_number << " " << p_current_subinterval->start_time.format_as_datetime_with_ns()
                << " to " << p_current_subinterval->end_time.format_as_datetime_with_ns() << std::endl;
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

                subinterval_array[currentSubintervalIndex].subinterval_status=subinterval_state::ready_to_send;

                if (0 == currentSubintervalIndex)
                {
                    currentSubintervalIndex = 1; otherSubintervalIndex = 0;
                } else {
                    currentSubintervalIndex = 0; otherSubintervalIndex = 1;
                }
                p_current_subinterval = &subinterval_array[currentSubintervalIndex];
                p_current_IosequencerInput = & (p_current_subinterval-> input);
                p_current_SubintervalOutput = & (p_current_subinterval->output);
                p_other_subinterval = &subinterval_array[otherSubintervalIndex];
                p_other_IosequencerInput = & (p_other_subinterval-> input);
                p_other_SubintervalOutput = & (p_other_subinterval->output);
                cancel_stalled_IOs();

                subinterval_number++;

        // indent level in loop waiting for run commands
            // indent level in loop running subintervals
                // indent level in locked section at subinterval switchover

                if (MainThreadCommand::stop == ivyslave_main_says)
                {
                    if (routine_logging) log(slavethreadlogfile,std::string("WorkloadThread stopping at end of subinterval.\n"));

                    state=ThreadState::stopping;

                    if (!catch_in_flight_IOs_after_last_subinterval())
                    {
                        dying_words = "<Error> WorkloadThread::WorkloadThreadRun() - call to catch_in_flight_IOs_after_last_subinterval() failed.\n";
                        log(slavethreadlogfile,dying_words);
                        state=ThreadState::died;
                        wkld_lk.unlock();
    					slaveThreadConditionVariable.notify_all();
                        return;
                    }

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
				    dying_words = o.str();
					log(slavethreadlogfile,dying_words);
					state=ThreadState::died;
                    ivyslave_main_posted_command = false;
					wkld_lk.unlock();
					slaveThreadConditionVariable.notify_all();
					return;
				}

				if(subinterval_state::ready_to_run!=subinterval_array[currentSubintervalIndex].subinterval_status)
				{
				    std::ostringstream o; o << "<Error> WorkloadThread told to keep going, but next subinterval not marked READY_TO_RUN.  Occurred at " << __FILE__ << " line " << __LINE__ << "\n";
				    dying_words = o.str();
					log(slavethreadlogfile,dying_words);
                    ivyslave_main_posted_command = false;
					state=ThreadState::died;
					wkld_lk.unlock();
					slaveThreadConditionVariable.notify_all();
					return;
				}

                subinterval_array[currentSubintervalIndex].subinterval_status = subinterval_state::running;
                // end of locked section
                ivytime switchover_complete;
                switchover_complete.setToNow();
                switchover_completion_latency_seconds.push(ivytime(switchover_complete-switchover_begin).getlongdoubleseconds());
			}

        // indent level in loop waiting for run commands
            // indent level in loop running subintervals
                // indent level in locked section at subinterval switchover

			slaveThreadConditionVariable.notify_all();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool WorkloadThread::prepare_linux_AIO_driver_to_start()
{
	// Coming in, we expect the subinterval_array[] IosequencerInput and SubintervalOutput objects to be prepared.
	// That means p_current_subinterval, p_current_IosequencerInput, and p_current_SubintervalOutput are set.

	// Note: When the iosequencer gets to the end of a stubinterval, it always switches the currentSubintervalIndex and otherSubintervalIndex variables.
	//       That way, the subinterval that is sent up to ivymaster is always "otherSubintervalIndex", whether or not the iosequencer is to stop.

	//       However, when the iosequencer thread is told to "stop", what it does before stopping is copy the IosequencerInput object
	//       for the last subinterval that ran to the other one that didn't run, the other one that would have been the next one if we hadn't stopped.

	// This doesn't bother the asynchronous collection / sending up of the data for the last subinterval.


	if (ThreadState::waiting_for_command != state)
	{
        std::ostringstream o; o << "<Error> prepare_linux_AIO_driver_to_start() was called, but the workload thread state was "
            << state << ", not ThreadState::waiting_for_command as expected.\n" << std::endl
             << "Occured at line " << __LINE__ << " of " << __FILE__ << std::endl;
        dying_words = o.str();
        log(slavethreadlogfile,dying_words);
        state=ThreadState::died;
        slaveThreadConditionVariable.notify_all();
        exit(-1);
	}
	// If Eyeos haven't been made, or it looks like we don't have enough for the next step, make some.
		// Figure out how many we need according to the precompute count, the post-process allowance, and maxTags parameter.
	// Empty the freeStack, precomputeQ, and postprocessQ.  Then fill the freeStack with our list of all Eyeos that exist.
	// Create the aio context.

	precomputedepth = 2 * p_current_IosequencerInput->maxTags;

	EyeoCount = 2 * (precomputedepth + p_current_IosequencerInput->maxTags);  // the 2x is a postprocess queue allowance
	for (int i=allEyeosThatExist.size() ; i < EyeoCount; i++)
		allEyeosThatExist.push_back(new Eyeo(i,this));

	precomputeQ.clear();
	while (0 < postprocessQ.size()) postprocessQ.pop();
	while (0 < freeStack.size()) freeStack.pop();
	for (auto& pE : allEyeosThatExist)
	{
		freeStack.push(pE);
	}

    running_IOs.clear();
    cancelled_IOs=0;

	iosequencer_variables.act=0;  // must be set to zero otherwise io_setup() will fail with return code 22 - invalid parameter.

	if (0 != (rc=io_setup(p_current_IosequencerInput->maxTags,&(iosequencer_variables.act))))
	{
        std::ostringstream o;
        o << "<Error> io_setup(" << p_current_IosequencerInput->maxTags << "," << (&iosequencer_variables.act) << ") failed return code " << rc
			<< ", errno=" << errno << " - " << strerror(errno) << std::endl
            << "Occured at line " << __LINE__ << " of " << __FILE__ << std::endl;
        dying_words = o.str();
        log(slavethreadlogfile,dying_words);
        state=ThreadState::died;
        slaveThreadConditionVariable.notify_all();
        exit(-1);
	}

	if (routine_logging) log(slavethreadlogfile,std::string("aio context successfully constructed.\n"));

	return true;
}

void WorkloadThread::popandpostprocessoneEyeo() {

	if (postprocessQ.size()==0) return;

	// collect I/O statistics / trace data for an I/O
	Eyeo* p_dun;
	p_dun=postprocessQ.front();
	postprocessQ.pop();

	ivy_float service_time_seconds, response_time_seconds, submit_time_seconds, running_time_seconds;

	bool have_response_time;

	if (ivytime(0) == p_dun->scheduled_time)
	{  // to avoid confusion, we do not post "application level" response time statistics if iorate==max.
		have_response_time = false;
		response_time_seconds = -1.0;
	}
	else
	{
		have_response_time = true;
		response_time_seconds = (ivytime(p_dun->end_time - p_dun->scheduled_time).getlongdoubleseconds());
	}

	if ( ivytime(0) == p_dun->start_time
	  || ivytime(0) == p_dun->running_time
	  || ivytime(0) == p_dun->end_time
	  || p_dun->scheduled_time > p_dun->start_time
	  || p_dun->start_time     > p_dun->running_time
	  || p_dun->running_time   > p_dun->end_time
	  )
    {
        std::ostringstream o;
        o << "<Error> Internal programming error. Source code reference " << __FILE__ << " line " << __LINE__ << std::endl
            << "When harvesting an I/O completion event and posting its measurements" << std::endl
            << "One of the following timestamps was missing or they were out of order." << std::endl
            << "I/O scheduled time = " << p_dun->scheduled_time.format_as_datetime_with_ns()
            << "start time         = " << p_dun->start_time.format_as_duration_HMMSSns() << std::endl
            << "running time       = " << p_dun->running_time.format_as_duration_HMMSSns() << std::endl
            << "end time           = " << p_dun->end_time.format_as_duration_HMMSSns() << std::endl

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
        dying_words = o.str();
        state = ThreadState::died;
		log(slavethreadlogfile,o.str());
		ivyslave_main_posted_command = false;
        slaveThreadConditionVariable.notify_all();
        exit(-1);
    }

	service_time_seconds = (ivytime(p_dun->end_time     - p_dun->start_time).getlongdoubleseconds());
	submit_time_seconds    = (ivytime(p_dun->running_time - p_dun->start_time).getlongdoubleseconds());
	running_time_seconds = (ivytime(p_dun->end_time     - p_dun->running_time).getlongdoubleseconds());

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

		bucket = Accumulators_by_io_type::get_bucket_index( submit_time_seconds );
		p_current_SubintervalOutput->u.a.submit_time.rs_array[rs][rw][bucket].push(submit_time_seconds);

		bucket = Accumulators_by_io_type::get_bucket_index( running_time_seconds );
		p_current_SubintervalOutput->u.a.running_time.rs_array[rs][rw][bucket].push(running_time_seconds);

		if (have_response_time)
		{
			bucket = Accumulators_by_io_type::get_bucket_index( response_time_seconds );

			p_current_SubintervalOutput->u.a.response_time.rs_array[rs][rw][bucket].push(response_time_seconds);
		}

	} else {
		ioerrorcount++;
		if ( 0 > p_dun->return_value ) {
			std::ostringstream o;
			o << "<Warning> Thread for " << workloadID.workloadID << " - I/O failed return code = " << p_dun->return_value << ", errno = " << errno << " - " << strerror(errno) << p_dun->toString() << std::endl;
			log(slavethreadlogfile,o.str());
				// %%%%%%%%%%%%%%% come back here later and decide if we want to re-drive failing I/Os, keeping track of the original I/O start time so we record the total respone time for all attempts.
		} else {
			std::ostringstream o;
			o << "<Warning> Thread for " << workloadID.workloadID << "- I/O only transferred " << p_dun->return_value << " out of requested " << p_current_IosequencerInput->blocksize_bytes << std::endl;
			log(slavethreadlogfile,o.str());
		}
	}
	freeStack.push(p_dun);
	return;
}


	// Note to the reader:

	// The following is a dispatcher loop that responds to real-time issues as promptly as possible.

	// It will be easer to understand if you first look at the normal way you would
	// write it down.

	// Dispatcher main loop:
	// Get "now" system timestamp (nanosecond resolution)
	// - if pending I/O completion events
	//	harvest a batch, timestamp them, put in postprocessQ and loop back for new timestamp;
	// - if cooldown is not over and it's time to submit the next I/O and we are below the max tag count
	// 	submit a block of I/Os for which it's time and for which we have tags, and loop back for a new timestamp;
	// - if the postprocessQ is not empty
	//	take an I/O out of the postcomputeQ, record statistics/trace data, and loop back for new timestamp;
	// - if there is room in the precomputeQ and the test and cooldown are not over
	//   and the scheduled time of the most recently added I/O in the precomputeQ is less than 1/4 second in the future
	// 	take an Eyeo off the freestack, generate(), and put in the precomputeQ, loop back for new timestamp;
	// - wait until either an I/O completion event is ready,
	//        or it's time to drive the next I/O and we are not yet at max queue depth
	//	  or cooldown has ended


	// So when you read the following, I was trying to have that effect.  However, the wait actually happens
	// at the top of the loop when you are doing the highest priority thing which is harvesting any pending
	// I/O completion events.


	// As a result, before you do the high priority thing, you need to know how long to wait
	// if there are no pending I/O completion events.


	// If there are I/Os waiting in the postprocessQ, we don't want to wait.
	// If there is room in the precomputeQ
	//    and the last I/O in the precomputeQ is scheduled before cooldown end time
	//    we don't want to wait.
	// If it's time to submit the next I/O and we are below max tag count we don't want to wait.
	// otherwise the wait time is until the end of the test if we are already at max tag count,
	// or if we have an available tag the wait time is until the time for the next I/O.

	// In the earliest functional partial implementations of ivy, we didn't have subintervals.
	// The earliest versions had a total test time consisting of a warmup period, then a
	// mid-stream test section, and then a cooldown phase.  The I/O workload was generated
	// all the way through from the beginning of warmup to the end of cooldown.  But the
	// measurement data was collected from the beginning to the end of the mid-stream period.
	// CPU counters were sampled at the beginning and again at the end of the mid-stream period.


bool WorkloadThread::linux_AIO_driver_run_subinterval()
// see also https://code.google.com/p/kernel/wiki/AIOUserGuide
{
	ivytime& subinterval_ending_time = p_current_subinterval->end_time;

    if (routine_logging) {std::ostringstream o; o << "WorkloadThread::linux_AIO_driver_run_subinterval() running subinterval which will end at " << subinterval_ending_time.format_as_datetime_with_ns() << std::endl; log(slavethreadlogfile,o.str());}

        int dedupe_count = 0;
        //int extra_count = 0;
        ivy_float target_dedupe = subinterval_array[0].input.dedupe;
        ivy_float modified_dedupe_factor = 1.0;
        dedupeunits = p_current_IosequencerInput->blocksize_bytes / 8192;
	while (true) {
		now.setToNow();

		if (now >= subinterval_ending_time) break;  // we will post-process any I/Os that ended within the subinterval before returning.

		if (precomputeQ.empty())
			nextIO_schedule_time = ivytime(0);
		else
			nextIO_schedule_time = precomputeQ.front()->scheduled_time;
			// If we are running at IOPS=max, the scheduled time will be ivytime(0).  It's always time to drive the next I/O.

		// We only wait for I/O ending events if the precompute queue is full and the postprocess queue is empty
		// and we aren't going to start new I/Os now because either we are already at the maxTags queue depth
		// or the scheduled time of the next I/O is in the future.

		// If we are going to wait, we wait for the end of the subinterval or until the scheduled time for the next I/O, whichever occurs earlier.

	 	if (0 < postprocessQ.size())
			waitduration = ivytime(0);
		else if ( (0.0 == p_current_IosequencerInput->IOPS) || cooldown )
		{
			waitduration=subinterval_ending_time - now;
			while (precomputeQ.size())  // may be paranoiaadd
			{
				Eyeo* pEyeo = precomputeQ.front();
				precomputeQ.pop_front();
				freeStack.push(pEyeo);
			}
		}
		else if (precomputeQ.size() < precomputedepth)
			waitduration = ivytime(0);
		else if ( (nextIO_schedule_time <= now) && (queue_depth < p_current_IosequencerInput->maxTags) )
			waitduration = ivytime(0);
		else if (queue_depth >= p_current_IosequencerInput->maxTags)
			waitduration=subinterval_ending_time - now;
		else if (nextIO_schedule_time > subinterval_ending_time)
			waitduration=subinterval_ending_time - now;
		else
			waitduration=nextIO_schedule_time - now;

		// Did your brain just explode?

		// OK, we've got the wait duration if we are going to have to wait for I/O ending events.

		if ((0 == queue_depth) && (ivytime(0) < waitduration))
		{	// The one case where we simply wait unconditionally is if there are no I/Os in flight
			// and the scheduled time to start the next I/O is in the future;

			// There are no in-flight I/Os, and it's not time to issue the next I/O yet.
			struct timespec remainder;
			if (0 != nanosleep(&(waitduration.t),&remainder) )
			{
				std::ostringstream o;
				o << "<Error> nanosleep() failed " << errno << " - " << strerror(errno) << " at " << __FILE__ << " line " << __LINE__ << "." << std::endl;
				log(slavethreadlogfile,o.str());
				//io_destroy(iosequencer_variables.act);
				dying_words = o.str();
				state=ThreadState::died;
                slaveThreadConditionVariable.notify_all();
				exit(-1);
				// later we might need to come back if we need to catch or ignore signals
			}
			continue;
		}

		// First priority is to reap I/Os.
		if (0 < queue_depth)
		{	// see if there are any pending I/O ending events

			if (ivytime(0)==waitduration) events_needed=0;
			else events_needed=1;

			// when we do the io_getevents() call, if we set the mifnimum number of events needed to 0 (zero), I expect the call to be non-blocking;

			if (0 < (reaped = io_getevents(iosequencer_variables.act,events_needed,MAX_IOEVENTS_REAP_AT_ONCE,&(ReapHeap[0]),&(waitduration.t))))
			{
				now.setToNow();  // because we may have waited

#ifdef IVY_TRACK_AIO
				p_current_SubintervalOutput->u.a.harvestcount.push((ivy_float)reaped);
				p_current_SubintervalOutput->u.a.preharvestqueuedepth.push((ivy_float)queue_depth);
#endif
				for (int i=0; i< reaped; i++) {

					struct io_event* p_event;
					Eyeo* p_Eyeo;
					struct iocb* p_iocb;


					p_event=&(ReapHeap[i]);
					p_Eyeo = (Eyeo*) p_event->data;
					p_iocb = (iocb*) p_event->obj;
					p_Eyeo->end_time = now;
					p_Eyeo->return_value = p_event->res;
					p_Eyeo->errno_value = p_event->res2;


                    auto running_IOs_iter = running_IOs.find(p_iocb);
                    if (running_IOs_iter == running_IOs.end())
                    {
                        std::ostringstream o;
                        o << "<Error> internal processing error - an asynchrononus I/O completion event occurred for a Linux aio I/O tracker "
                            << "that was not found in the ivy workload thread\'s \"running_IOs\" (set of pointers to Linux I/O tracker objects)."
                            << "  The ivy Eyeo object associated with the completion event describes itself as "
                            << p_Eyeo->toString()
                            <<  " at " << __FILE__ << " line " << __LINE__ << std::endl;
                        dying_words = o.str();
                        log(slavethreadlogfile,o.str());
                        io_destroy(iosequencer_variables.act);
                        state=ThreadState::died;
                        slaveThreadConditionVariable.notify_all();
                        exit(-1);
                    }
                    running_IOs.erase(running_IOs_iter);

					queue_depth--;
					postprocessQ.push(p_Eyeo);
				}
#ifdef IVY_TRACK_AIO
				p_current_SubintervalOutput->u.a.postharvestqueuedepth.push((ivy_float)queue_depth);
#endif
				continue;  // start back up to the top and only do other stuff if there's nothing to reap
			}
		}

		// we have fallen through from harvesting all available events and waiting

		if ( (precomputeQ.size() > 0) && (nextIO_schedule_time <= now) && (queue_depth < p_current_IosequencerInput->maxTags) ) {
			// It's time to launch the next I/O[s].

			uint64_t launch_count=0;
			while
			(
			        (!precomputeQ.empty())
			     && (now >= (precomputeQ.front()->scheduled_time))
			     && (launch_count < (MAX_IOS_LAUNCH_AT_ONCE-1))  // launch count starts at zero
			     && (queue_depth+launch_count < p_current_IosequencerInput->maxTags)
			)
			{
				LaunchPad[launch_count]=precomputeQ.front();
				precomputeQ.pop_front();
				LaunchPad[launch_count]->start_time=now;
                auto running_IOs_iterator = running_IOs.insert(&(LaunchPad[launch_count]->eyeocb));
                if (running_IOs_iterator.first == running_IOs.end())
                {
                    std::ostringstream o;
                    o << "<Error> internal processing error - failed trying to insert a Linux aio I/O tracker "
                        << "into the ivy workload thread\'s \"running_IOs\" (set of pointers to Linux I/O tracker objects)."
                        << "  The ivy Eyeo object associated with the completion event describes itself as "
                        << LaunchPad[launch_count]->toString()
                        <<  "  " << __FILE__ << " line " << __LINE__ << std::endl;
                    dying_words = o.str();
                    log(slavethreadlogfile,o.str());
                    io_destroy(iosequencer_variables.act);
                    state=ThreadState::died;
                    slaveThreadConditionVariable.notify_all();
                    exit(-1);
                }
				launch_count++;
			}
			rc = io_submit(iosequencer_variables.act, launch_count, (struct iocb **) LaunchPad /* iocb comes first in an Eyeo */);
			if (-1==rc)
			{
                std::ostringstream o;
                o << "<Error> Asynchronous I/O \"io_submit()\" failed, errno=" << errno << " - " << strerror(errno)
                    <<  " at " << __FILE__ << " line " << __LINE__ << std::endl;
                dying_words = o.str();
                log(slavethreadlogfile,o.str());
                io_destroy(iosequencer_variables.act);
                state=ThreadState::died;
                slaveThreadConditionVariable.notify_all();
                exit(-1);
			} else {
				// return code is number of I/Os succesfully launched.

			    ivytime running_timestamp;
			    running_timestamp.setToNow();
			    for (int i=0; i<rc; i++)
			    {
					Eyeo* p_Eyeo = (Eyeo*)(LaunchPad[i]->eyeocb.aio_data);
					p_Eyeo->running_time = running_timestamp;
			    }

#ifdef IVY_TRACK_AIO
				p_current_SubintervalOutput->u.a.submitcount.push((ivy_float)rc);
				p_current_SubintervalOutput->u.a.presubmitqueuedepth.push((ivy_float)queue_depth);
				p_current_SubintervalOutput->u.a.postsubmitqueuedepth.push((ivy_float)(queue_depth+rc));
#endif
				queue_depth+=rc;
				if (maxqueuedepth<queue_depth) maxqueuedepth=queue_depth;

				// We put back any unsuccessfully launched back into the precomputeQ.

				if (0==rc)log(slavethreadlogfile,
					"io_submit() return code zero.  No I/Os successfully submitted.\n");

				int number_to_put_back=launch_count - rc;

#ifdef IVY_TRACK_AIO
				p_current_SubintervalOutput->u.a.putback.push((ivy_float) number_to_put_back);
#endif
				if (routine_logging && number_to_put_back>0)
				{
					std::ostringstream o;
					o << "aio context didn\'t take all the I/Os.  Putting " << number_to_put_back << " back." << std::endl;
					log(slavethreadlogfile,o.str());
				}

				while (number_to_put_back>0) {
					// we put them back in reverse order of how they came out

					Eyeo* p_Eyeo = (Eyeo*)(LaunchPad[launch_count-1]->eyeocb.aio_data);

					struct iocb* p_iocb = &(p_Eyeo->eyeocb);

	                auto running_IOs_iter = running_IOs.find(p_iocb);
                    if (running_IOs_iter == running_IOs.end())
                    {
                        std::ostringstream o;
                        o << "<Error> internal processing error - after a submit was not successful for all I/Os, when trying to "
                            << "remove from \"running_IOs\" one of the I/Os that wasn\'t successfully launched, that "
                            << "unsuccessful I/O was not found in \"running_IOs\"."
                            << "  The ivy Eyeo object associated with the completion event describes itself as "
                            << p_Eyeo->toString()
                            <<  " at " << __FILE__ << " line " << __LINE__
                            ;
                        dying_words = o.str();
                        log(slavethreadlogfile,o.str());
                        io_destroy(iosequencer_variables.act);
                        state = ThreadState::died;
                        slaveThreadConditionVariable.notify_all();
                        exit(-1);
                    }
                    running_IOs.erase(running_IOs_iter);

					precomputeQ.push_front(p_Eyeo);
					launch_count--;
					number_to_put_back--;
				}
			}
			continue;
		}

		if (postprocessQ.size()>0) {
			popandpostprocessoneEyeo();
			continue;
		}

		if
		(
               ( precomputeQ.size() < precomputedepth )
            && ( !cooldown )
            && ( 0.0 != p_current_IosequencerInput->IOPS )
            && ( ( precomputeQ.size() == 0 ) || ( (now + precompute_horizon) > precomputeQ.back()->scheduled_time ) )
        )
		{
			// precompute an I/O
			if (freeStack.empty())
			{
				std::ostringstream o; o << "<Error> freeStack is empty - can\'t precompute - at " << __FILE__ << " line " << __LINE__ << std::endl;
				dying_words = o.str();
				log(slavethreadlogfile,dying_words);
				io_destroy(iosequencer_variables.act);
                state = ThreadState::died;
                slaveThreadConditionVariable.notify_all();
                exit(-1);
				// should not occur unless there's a design fault or maybe if CPU-bound
			}
			Eyeo* p_Eyeo = freeStack.top();
			freeStack.pop();

            std::ostringstream o;
            // deterministic generation of "seed" for this I/O.
            if (have_writes)
            {
                if (doing_dedupe)
                {
                    bool old_method {false};
                    if (old_method)
                    {
                        serpentine_number += threads_in_workload_name;
                        uint64_t new_pattern_number = ceil(serpentine_number * serpentine_multiplier);
                        while (pattern_number < new_pattern_number)
                        {
                            xorshift64star(pattern_seed);
                            pattern_number++;
                        }
                        block_seed = pattern_seed ^ pattern_number;
                    } else
                    {   // new method

                        int bsindex = 0;
                        int dedupeunitsvar = dedupeunits;
                        while (dedupeunitsvar-- > 0)
                        {
                            std::ostringstream o;

                        if (dedupe_count == 0) {
                            modified_dedupe_factor = dedupe_regulator->dedupe_distribution(target_dedupe, p_my_iosequencer->isRandom());
                            dedupe_count = (uint64_t) modified_dedupe_factor;
                            o << "modified_dedupe_factor: " << modified_dedupe_factor << std::endl;
                            //log(slavethreadlogfile,o.str());
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
            log(slavethreadlogfile,o.str());
#endif
                        }
                    }
                }
                else
                {
                    xorshift64star(block_seed);
                }
            }

            spectrum = dedupe_regulator->get_spectrum();
            bias = false;
#if 0
            o << "slang bias: " << bias << " pattern number: " << pattern_number << " pattern_seed: " << pattern_seed  << " block_seed:" << block_seed << std::endl;
            log(slavethreadlogfile,o.str());
#endif
			// now need to call the iosequencer function to populate this Eyeo.
			if (!p_my_iosequencer->generate(*p_Eyeo)) {
				std::ostringstream o; o << "<Error> iosequencer::generate() failed - at " << __FILE__ << " line " << __LINE__ << std::endl;
				dying_words = o.str();
				log(slavethreadlogfile,dying_words);
				io_destroy(iosequencer_variables.act);
                state = ThreadState::died;
                slaveThreadConditionVariable.notify_all();
                exit(-1);
			}
			precomputeQ.push_back(p_Eyeo);
		}

	}

	while (postprocessQ.size()>0) popandpostprocessoneEyeo();
		// This does make it slightly non-responsive at the end of a subinterval ...


	return true;
}

bool WorkloadThread::catch_in_flight_IOs_after_last_subinterval()
{
	ivytime catch_wait_duration(10); // seconds. Some disk drive error recovery can take 30 seconds or on un-mirrored SATA boot drives as long as two minutes.

    ivytime entry; entry.setToNow();
    ivytime timeout = entry + catch_wait_duration;

    ivytime now;

	while (0 < queue_depth)
	{
        ivytime now;

        now.setToNow();

        if ( now >= timeout ) break;

		int events_needed=1;

		// when we do the io_getevents() call, if we set the minimum number of events needed to 0 (zero), I expect the call to be non-blocking;

		if (0 < (reaped = io_getevents(iosequencer_variables.act,events_needed,MAX_IOEVENTS_REAP_AT_ONCE,&(ReapHeap[0]),&(catch_wait_duration.t))))
		{
			queue_depth-=reaped;

            for (int i=0; i< reaped; i++)
            {
                struct io_event* p_event;
                Eyeo* p_Eyeo;
                struct iocb* p_iocb;

                p_event=&(ReapHeap[i]);
                p_Eyeo = (Eyeo*) p_event->data;
                p_iocb = (iocb*) p_event->obj;

                auto running_IOs_iter = running_IOs.find(p_iocb);
                if (running_IOs_iter == running_IOs.end())
                {
                    std::ostringstream o;
                    o << "<Error> internal processing error - during catch in flight IOs after last subinterval, an asynchrononus I/O completion event occurred for a Linux aio I/O tracker "
                        << "that was not found in the ivy workload thread\'s \"running_IOs\" (set of pointers to Linux I/O tracker objects)."
                        << "  The ivy Eyeo object associated with the completion event describes itself as "
                        << p_Eyeo->toString()
                        <<  "  " << __FILE__ << " line " << __LINE__
                        ;
                    dying_words = o.str();
                    log(slavethreadlogfile,o.str());
                    io_destroy(iosequencer_variables.act);
                    state = ThreadState::died;
                    slaveThreadConditionVariable.notify_all();
                    exit(-1);
                }
                running_IOs.erase(running_IOs_iter);
            }
		}
		else if (0== reaped)
		{
			break; // timed out
		}
		else
		{
            std::ostringstream o;
            o << "<Error> WorkloadThread::catch_in_flight_IOs_after_last_subinterval() - io_getevents() failed at " << __FILE__ << " line " << __LINE__ ;
            dying_words= o.str();
            log(slavethreadlogfile,o.str());
            io_destroy(iosequencer_variables.act);
            state = ThreadState::died;
            slaveThreadConditionVariable.notify_all();
            exit(-1);
		}
	}

    if (queue_depth != running_IOs.size())
    {
        std::ostringstream o;
        o << "<Error> internal processing error - before cancelling any remaining I/Os still running more than " << catch_wait_duration.format_as_duration_HMMSS()
            << " after subinterval end.  Found to our consternation that queue_depth was "<< queue_depth << " but running_IOs.size() was " << running_IOs.size()
            <<  "  " << __FILE__ << " line " << __LINE__ ;
        dying_words= o.str();
        log(slavethreadlogfile,o.str());
        io_destroy(iosequencer_variables.act);
        state = ThreadState::died;
        slaveThreadConditionVariable.notify_all();
        exit(-1);
    }

    for (auto& p_iocb : running_IOs)
    {
        ivy_cancel_IO(p_iocb);
    }

    running_IOs.clear();

	io_destroy(iosequencer_variables.act);

	return true;
}

void WorkloadThread::ivy_cancel_IO(struct iocb* p_iocb)
{
    Eyeo* p_Eyeo = (Eyeo*) p_iocb->aio_data;


    auto it = running_IOs.find(p_iocb);
    if (it == running_IOs.end())
    {
        std::ostringstream o;
        o << "<Error> ivy_cancel_IO was called to cancel an I/O that ivy wasn\'t tracking as running.  The I/O describes itself as " << p_Eyeo->toString()
            << ".  Occurred at line " << __LINE__ << " of " << __FILE__ << ".";
        dying_words= o.str();
        log(slavethreadlogfile,o.str());
        io_destroy(iosequencer_variables.act);
        state = ThreadState::died;
        slaveThreadConditionVariable.notify_all();
        exit(-1);
    }

    {
        std::ostringstream o;
        o << "<Warning> about to cancel an I/O operation that has been running for " << (p_Eyeo->since_start_time()).format_as_duration_HMMSSns()
            << " described as " << p_Eyeo->toString();
        log(slavethreadlogfile,o.str());
    }

    struct io_event ioev;

    long rc = EAGAIN;

    ivy_int retry_count = 0;

    while (rc == EAGAIN)
    {
        retry_count++;
        if (retry_count > 100)
        {
            std::ostringstream o;
            o << "<Error> after over 100 retries of the system call to cancel an I/O, ivy_cancel_IO() is abandoning the attempt."
                << "  Occurred at line " << __LINE__ << " of " << __FILE__ << ".";;
            dying_words= o.str();
            log(slavethreadlogfile,o.str());
            io_destroy(iosequencer_variables.act);
            state = ThreadState::died;
            slaveThreadConditionVariable.notify_all();
            exit(-1);
        }
        rc = io_cancel(iosequencer_variables.act,p_iocb, &ioev);
    }

    if (rc == 0)
    {
        running_IOs.erase(it);
        cancelled_IOs++;
        return;
    }

    {
        std::ostringstream o;
        o << "<Error> in " << __FILE__ << " line " << __LINE__ << " in ivy_cancel_IO(), system call to cancel the I/O failed saying - ";
        switch (rc)
        {
            case EINVAL:
                o << "EINVAL - invalid AIO context";
                break;
            case EFAULT:
                o << "EFAULT - one of the data structures points to invalid data";
                break;
            case ENOSYS:
                o << "ENOSYS - io_cancel() not supported on this system.";
                break;
            default:
                o << "unknown error return code " << rc;
        }
        log(slavethreadlogfile,o.str());
    }
    return;
}

void WorkloadThread::cancel_stalled_IOs()
{
    std::list<struct iocb*> l;

    for (auto& p_iocb : running_IOs)
    {
        Eyeo* p_Eyeo = (Eyeo*) p_iocb->aio_data;
        if (p_Eyeo->since_start_time() > max_io_run_time_seconds) l.push_back(p_iocb);
    }
    // I didn't want to be iterating over running_IOs when cancelling I/Os, as the cancel deletes the I/O from running_IOs.
    for (auto p_iocb : l) ivy_cancel_IO(p_iocb);
}
