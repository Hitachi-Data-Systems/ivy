//Copyright (c) 2016 Hitachi Data Systems, Inc.
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
//Author: Allart Ian Vogelesang <ian.vogelesang@hds.com>
//
//Support:  "ivy" is not officially supported by Hitachi Data Systems.
//          Contact me (Ian) by email at ian.vogelesang@hds.com and as time permits, I'll help on a best efforts basis.

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
#include "iogenerator_stuff.h"
#include "IogeneratorInput.h"
#include "Iogenerator.h"
#include "ivylinuxcpubusy.h"
#include "ivybuilddate.h"
#include "RunningStat.h"
#include "Accumulators_by_io_type.h"
#include "SubintervalOutput.h"
#include "Subinterval.h"
#include "WorkloadThread.h"
#include "IogeneratorRandom.h"
#include "IogeneratorRandomSteady.h"
#include "IogeneratorRandomIndependent.h"
#include "IogeneratorSequential.h"

extern bool routine_logging;


// for some strange reason, there's no header file for these system call wrapper functions
inline int io_setup(unsigned nr, aio_context_t *ctxp) { return syscall(__NR_io_setup, nr, ctxp); }
inline int io_destroy(aio_context_t ctx) { return syscall(__NR_io_destroy, ctx); }
inline int io_submit(aio_context_t ctx, long nr,  struct iocb **iocbpp) { return syscall(__NR_io_submit, ctx, nr, iocbpp); }
inline int io_getevents(aio_context_t ctx, long min_nr, long max_nr, struct io_event *events, struct timespec *timeout)
	{ return syscall(__NR_io_getevents, ctx, min_nr, max_nr, events, timeout); }


WorkloadThread::WorkloadThread(std::string wID, LUN* pL, long long int lastLBA, std::string parms, std::mutex* p_imm)
        : workloadID(wID), pLUN(pL), maxLBA(lastLBA), iogeneratorParameters(parms), p_ivyslave_main_mutex(p_imm)
{}

std::string threadStateToString (ThreadState ts)
{
	switch (ts)
	{
		case ThreadState::waiting_for_command: return "ThreadState::waiting_for_command";
		case ThreadState::running: return "ThreadState::running";
		case ThreadState::stopping: return "ThreadState::stopping";
		case ThreadState::died: return "ThreadState::died";
		case ThreadState::exited_normally: return "ThreadState::exited_normally";
	}
	{
		std::ostringstream o;
		o << "threadStateToString (ThreadState = " << ((int) ts) << ") - internal programming error - invalid ThreadState value." ;
		return o.str();
	}
}

std::string mainThreadCommandToString(MainThreadCommand mtc)
{
	switch (mtc)
	{
		case MainThreadCommand::run: return "MainThreadCommand::run";
		case MainThreadCommand::stop: return "MainThreadCommand::stop";
		case MainThreadCommand::die: return "MainThreadCommand::die";
	}
	{
		std::ostringstream o;
		o << "mainThreadCommandToString (MainThreadCommand = " << ((int) mtc) << ") - internal programming error - invalid MainThreadCommand value." ;
		return o.str();
	}

}

void WorkloadThread::WorkloadThreadRun() {

	if (routine_logging)
	{
		std::ostringstream o;

		o<< "WorkloadThreadRun() fireup for workloadID = \"" << workloadID.workloadID << "\", iogenerator parameters = \"" << iogeneratorParameters << "\", ivy build date " << IVYBUILDDATE << std::endl;

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
			o << "WorkloadThreadRun() - workloadID = \"" << workloadID.workloadID << "\" - Sector Size attribute was not all digits." << std::endl;
			log(slavethreadlogfile, o.str());
			return;
		}
		std::istringstream is(secsize);
		is >> iogenerator_variables.sector_size;
	}
	else
	{
		iogenerator_variables.sector_size = 512;
	}

	iogenerator_variables.LUN_size_bytes = (1+ maxLBA) * iogenerator_variables.sector_size;

	if (!pLUN->contains_attribute_name(std::string("LUN name")))
	{
		std::ostringstream o;
		o << "WorkloadThreadRun() - LUN does not have \"LUN name\" attribute.  This is what the LUN is called in this OS.  In Linux, /dev/sdxxx." << std::endl;
		log(slavethreadlogfile, o.str());
		return;
	}

	std::string LUNname = pLUN->attribute_value(std::string("LUN name"));

	iogenerator_variables.fd=open64(LUNname.c_str(),O_RDWR+O_DIRECT);

	if (-1 == iogenerator_variables.fd) {
		std::ostringstream o;
		o << "WorkloadThreadRun() - open64(\"" << LUNname << "\",O_RDWR+O_DIRECT) failed errno = " << errno << " (" << strerror(errno) << ')' << std::endl;
		log(slavethreadlogfile, o.str());
		return;
	}



	available_iogenerators[std::string("random_steady")] = new IogeneratorRandomSteady(pLUN, slavethreadlogfile, workloadID.workloadID, &iogenerator_variables, this);
	available_iogenerators[std::string("random_independent")] = new IogeneratorRandomIndependent(pLUN, slavethreadlogfile, workloadID.workloadID, &iogenerator_variables, this);
	available_iogenerators[std::string("sequential")] = new IogeneratorSequential(pLUN, slavethreadlogfile, workloadID.workloadID, &iogenerator_variables, this);

	state = ThreadState::waiting_for_command;

	while (true) // loop that waits for "run" commands
	{
		{ // runs with lock held
			std::unique_lock<std::mutex> u_lk(slaveThreadMutex);
			while (!ivyslave_main_posted_command) slaveThreadConditionVariable.wait(u_lk);

			if (routine_logging) log(slavethreadlogfile,std::string("Received command from host - ") + mainThreadCommandToString(ivyslave_main_says));

			ivyslave_main_posted_command=false;
			log(slavethreadlogfile,"turning off ivyslave_main_posted_command flag.");

			switch (ivyslave_main_says)
			{
				case MainThreadCommand::die:
				{
					if (routine_logging) log (slavethreadlogfile,std::string("WorkloadThread dutifully dying upon command.\n"));
					state=ThreadState::exited_normally;
					break;
				}
				case MainThreadCommand::run:
				{
					break;
				}
				default:
				log (slavethreadlogfile,std::string("WorkloadThread - in main loop waiting for commands didn\'t get \"run\" or \"die\" when expected.\n"));
				state=ThreadState::died;
				break;
			}
		}  // lock no longer held

		slaveThreadConditionVariable.notify_all();

		if (ThreadState::died == state || ThreadState::exited_normally == state) return; // politely dropping the lock first

		currentSubintervalIndex=0;
		otherSubintervalIndex=1;
		subinterval_number=0;

		p_current_subinterval = &(subinterval_array[currentSubintervalIndex]);
		p_current_IogeneratorInput = & (p_current_subinterval-> input);
		p_current_SubintervalOutput = & (p_current_subinterval->output);
		p_other_subinterval = &(subinterval_array[otherSubintervalIndex]);
		p_other_IogeneratorInput = & (p_other_subinterval-> input);
		p_other_SubintervalOutput = & (p_other_subinterval->output);

		std::map<std::string, Iogenerator*>::iterator iogen_ptr_it = available_iogenerators.find(p_current_IogeneratorInput->iogenerator_type);
		if (available_iogenerators.end() == iogen_ptr_it)
		{
			std::ostringstream o; o << "WorkloadThread::WorkloadThreadRun() - Got a \"Go!\" but the first subinterval\'s IogeneratorInput\'s iogenerator_type \""
				<< p_current_IogeneratorInput->iogenerator_type << "\" is not one of the available iogenerator types." << std::endl;
			log(slavethreadlogfile,o.str());
			state=ThreadState::died;
			return;
		}
		p_my_iogenerator = (*iogen_ptr_it).second;
		if (!p_my_iogenerator->setFrom_IogeneratorInput(p_current_IogeneratorInput))
		{
			log(slavethreadlogfile,std::string("WorkloadThread::WorkloadThreadRun() - call to iogenerator::setFrom_IogeneratorInput(() failed.\n"));
			state=ThreadState::died;
			return;
		}
//*debug*/ log(slavethreadlogfile,std::string("WorkloadThread::WorkloadThreadRun() - iogenerator::setFrom_IogeneratorInput went OK.\n"));

		if (!prepare_linux_AIO_driver_to_start())
		{
			log(slavethreadlogfile,std::string("WorkloadThread::WorkloadThreadRun() - call to prepare_linux_AIO_driver_to_start() failed.\n"));
			state=ThreadState::died;
			return;
		}

		state=ThreadState::running;

		int bufsize = p_current_IogeneratorInput->blocksize_bytes;
		while (0 != (bufsize % 4096)) bufsize++;
		long int memory_allocation_size = 4095 + (bufsize * allEyeosThatExist.size());

        if (routine_logging)
        {
            std::ostringstream o;
            o << "getting I/O buffer memory for Eyeos.  blksize= " << p_current_IogeneratorInput->blocksize_bytes
                << ", rounded up to multiple of 4096 is bufsize = " << bufsize << ", number of Eyeos = " << allEyeosThatExist.size()
                << ", memory allocation size with 4095 bytes for alignment padding is " << memory_allocation_size << std::endl;
            log(slavethreadlogfile,o.str()); }
		{

			std::unique_ptr<unsigned char[]> ewe_neek;

			try
			{
				ewe_neek.reset( new unsigned char[memory_allocation_size] );
			}
			catch(const std::bad_alloc& e)
			{
				log(slavethreadlogfile,std::string("WorkloadThread.cpp - out of memory making I/O buffer pool - ") + e.what() + std::string("\n"));
				exit(0);
			}

			if (!ewe_neek)
			{
				log(slavethreadlogfile,"WorkloadThread.cpp out of memory making I/O buffer pool\n");
				exit(0);
			}

			void* vp = (void*) ewe_neek.get();

			uint64_t aio_buf_value = (uint64_t) vp;

			while (0 != (aio_buf_value % 4096)) aio_buf_value++;   // yes, std::align, but we are stuffing these values in iocbs as uint64_t type anyway

			for (auto& pEyeo : allEyeosThatExist)
			{
				pEyeo->eyeocb.aio_buf = aio_buf_value;
				aio_buf_value += bufsize;
			}

/*debug*/ if (routine_logging) log(slavethreadlogfile,"Finished setting aio_buf values in all existing Eyeos.\n");

			while (true) // loop running subintervals, until we are told "stop"
			{

                if (routine_logging){
                    std::ostringstream o;
                    o << "Top of subinterval " << subinterval_number << " " << p_current_subinterval->start_time.format_as_datetime_with_ns()
                        << " to " << p_current_subinterval->end_time.format_as_datetime_with_ns() << " currentSubintervalIndex=" << currentSubintervalIndex
                        << ", otherSubintervalIndex=" << otherSubintervalIndex << std::endl;
                        log(slavethreadlogfile,o.str());
                }

				try {
					// now we are running a subinterval
					if (!linux_AIO_driver_run_subinterval())
					{
						log(slavethreadlogfile,std::string("WorkloadThread::WorkloadThreadRun() - call to linux_AIO_driver_run_subinterval() failed.\n"));
						state=ThreadState::died;
						return;
					}
				}
				catch (std::exception& e)
				{
					std::ostringstream o;
					o << "WorkloadThread::WorkloadThreadRun() - exception during call to linux_AIO_driver_run_subinterval() - " << e.what() << std::endl;
					log(slavethreadlogfile, o.str());
				}
				// end of subinterval, flip over to other subinterval or stop

                if (routine_logging)
                {
                    std::ostringstream o;
                    o << "Bottom of subinterval " << subinterval_number << " " << p_current_subinterval->start_time.format_as_datetime_with_ns()
                    << " to " << p_current_subinterval->end_time.format_as_datetime_with_ns() << std::endl;
                    log(slavethreadlogfile,o.str());
                }

				bool have_lock_at_subinterval_end {false};

				ivytime before_getting_lock;
				before_getting_lock.setToNow();

				for (int i=0; i<10; i++)
				{
					if (slaveThreadMutex.try_lock())
					{
						if (i>0) // i.e. it didn't work on the first try - this call is documented to fail spuriously.
						{
							std::ostringstream o;
							o << std::endl << "[Exception] - it took " << (i+1) << " attempts to get the lock at the end of the subinterval." << std::endl;
							o << "The std::mutex::try_lock() call is documented to fail spuriously, so at this point we just want to see how often this is happening." << std::endl;
							log(slavethreadlogfile, o.str());
						}
                        have_lock_at_subinterval_end = true;
                        break;
					}
                    if (i>1) {std::this_thread::sleep_for(std::chrono::milliseconds(1));}
				}

				if (!have_lock_at_subinterval_end)
				{
					// we do not have the lock
					ivytime t; t.setToNow();
					std::ostringstream o;
					o << "<Error> Workload thread failed to get the lock within " << ivytime(t-before_getting_lock).format_as_duration_HMMSSns() << " at the end of the subinterval." << std::endl;
					log (slavethreadlogfile,o.str());
					state=ThreadState::died;
					slaveThreadConditionVariable.notify_all();
					return;
				}

				// we have the lock
//*debug*/{ std::ostringstream o; o << "Have the lock at end of subinterval " << subinterval_number << std::endl; log(slavethreadlogfile,o.str()); }
				if (!ivyslave_main_posted_command)
				{
					log(slavethreadlogfile,"<Error> Workload thread got the lock at the end of the subinterval, but no command was posted.\n");
					state=ThreadState::died;
					slaveThreadMutex.unlock();
					slaveThreadConditionVariable.notify_all();
					return;
				}

				ivyslave_main_posted_command=false;
				log(slavethreadlogfile,"turning off ivyslave_main_posted_command flag.");
//*debug*/debug_command_log("WorkloadThread.cpp at bottom of subinterval after turning off ivyslave_main_posted_command");

				if (MainThreadCommand::die == ivyslave_main_says)
				{
					if (routine_logging) log(slavethreadlogfile,std::string("WorkloadThread dutifully dying upon command at end of subinterval.\n"));
					state=ThreadState::exited_normally;
					slaveThreadMutex.unlock();
					slaveThreadConditionVariable.notify_all();
//*debug*/ log(slavethreadlogfile,"Dropped the lock\n");
					return;
				}

				subinterval_array[currentSubintervalIndex].subinterval_status=subinterval_state::ready_to_send;
/*debug*/{std::ostringstream o; o << "Workload thread marking subinterval_array[" << currentSubintervalIndex << "].subinterval_status = subinterval_state::ready_to_send"; log(slavethreadlogfile, o.str());}

				if (0 == currentSubintervalIndex)
				{
					currentSubintervalIndex = 1; otherSubintervalIndex = 0;
				} else {
					currentSubintervalIndex = 0; otherSubintervalIndex = 1;
				}
				p_current_subinterval = &subinterval_array[currentSubintervalIndex];
				p_current_IogeneratorInput = & (p_current_subinterval-> input);
				p_current_SubintervalOutput = & (p_current_subinterval->output);
				p_other_subinterval = &subinterval_array[otherSubintervalIndex];
				p_other_IogeneratorInput = & (p_other_subinterval-> input);
				p_other_SubintervalOutput = & (p_other_subinterval->output);

				subinterval_number++;

				if (MainThreadCommand::stop == ivyslave_main_says)
				{
					if (routine_logging) log(slavethreadlogfile,std::string("WorkloadThread stopping at end of subinterval.\n"));

					state=ThreadState::stopping;
					slaveThreadMutex.unlock(); slaveThreadConditionVariable.notify_all();
//*debug*/ log(slavethreadlogfile,"Dropped the lock\n");

					if (!catch_in_flight_IOs_after_last_subinterval())
					{
						log(slavethreadlogfile,std::string("WorkloadThread::WorkloadThreadRun() - call to catch_in_flight_IOs_after_last_subinterval() failed.\n"));
						state=ThreadState::died;
						return;
					}

					{ // runs with lock held
						std::unique_lock<std::mutex> u_lk(slaveThreadMutex);
						state=ThreadState::waiting_for_command;
					}  // lock no longer held
					slaveThreadConditionVariable.notify_all();

					break;  // go back to waiting for a command
				}
				if (MainThreadCommand::run != ivyslave_main_says)
				{
					log(slavethreadlogfile,std::string("WorkloadThread unrecognized command at end of subinterval.\n"));
					state=ThreadState::died;
					slaveThreadMutex.unlock();
					slaveThreadConditionVariable.notify_all();
//*debug*/ log(slavethreadlogfile,"Dropped the lock\n");
					return;
				}

				if(subinterval_state::ready_to_run!=subinterval_array[currentSubintervalIndex].subinterval_status)
				{
					log(slavethreadlogfile,std::string("WorkloadThread told to continue, but next subinterval not marked READY_TO_RUN.\n"));
					state=ThreadState::died;
					slaveThreadMutex.unlock();
					slaveThreadConditionVariable.notify_all();
//*debug*/ log(slavethreadlogfile,"Dropped the lock\n");
					return;
				}

                subinterval_array[currentSubintervalIndex].subinterval_status = subinterval_state::running;

				slaveThreadMutex.unlock();
				slaveThreadConditionVariable.notify_all();
//*debug*/ log(slavethreadlogfile,"Dropped the lock\n");
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool WorkloadThread::prepare_linux_AIO_driver_to_start()
{
	// Coming in, we expect the subinterval_array[] IogeneratorInput and SubintervalOutput objects to be prepared.
	// That means p_current_subinterval, p_current_IogeneratorInput, and p_current_SubintervalOutput are set.

	// Note: When the iogenerator gets to the end of a stubinterval, it always switches the currentSubintervalIndex and otherSubintervalIndex variables.
	//       That way, the subinterval that is sent up to ivymaster is always "otherSubintervalIndex", whether or not the iogenerator is to stop.

	//       However, when the iogenerator thread is told to "stop", what it does before stopping is copy the IogeneratorInput object
	//       for the last subinterval that ran to the other one that didn't run, the other one that would have been the next one if we hadn't stopped.

	// This doesn't bother the asynchronous collection / sending up of the data for the last subinterval.


	if (ThreadState::waiting_for_command != state)
	{
		log(slavethreadlogfile,std::string("prepare_linux_AIO_driver_to_start() was called, but the workload thread state was not ThreadState::waiting_for_command\n"));
		return false;
	}
	// If Eyeos haven't been made, or it looks like we don't have enough for the next step, make some.
		// Figure out how many we need according to the precompute count, the post-process allowance, and maxTags parameter.
	// Empty the freeStack, precomputeQ, and postprocessQ.  Then fill the freeStack with our list of all Eyeos that exist.
	// Create the aio context.

	precomputedepth = 2 * p_current_IogeneratorInput->maxTags;

	EyeoCount = 2 * (precomputedepth + p_current_IogeneratorInput->maxTags);  // the 2x is a postprocess queue allowance
	for (int i=allEyeosThatExist.size() ; i < EyeoCount; i++)
		allEyeosThatExist.push_back(new Eyeo(i,this));

	precomputeQ.clear();
	while (0 < postprocessQ.size()) postprocessQ.pop();
	while (0 < freeStack.size()) freeStack.pop();
	for (auto& pE : allEyeosThatExist)
	{
//
///*debug*/ if (0 == pE->buf_size)
///*debug*/{
//
//		if (!pE->resize_buf(p_current_IogeneratorInput->blocksize_bytes))
//		{
//			log(slavethreadlogfile,std::string("WorkloadThread::prepare_linux_AIO_driver_to_start() - Eyeo.resize_buf failed.\n"));
//			return false;
//		}
//
///*debug*/} else {log(slavethreadlogfile,"debug - skipped resizing the Eyeo.\n");}

		freeStack.push(pE);
	}

///*debug*/log(slavethreadlogfile,"Finished resizing Eyeos.\n");

	iogenerator_variables.act=0;  // must be set to zero otherwise io_setup() will fail with return code 22 - invalid parameter.

	if (0 != (rc=io_setup(p_current_IogeneratorInput->maxTags,&(iogenerator_variables.act)))) {
		std::ostringstream o;
		o << "io_setup(" << p_current_IogeneratorInput->maxTags << "," << (&iogenerator_variables.act) << ") failed return code " << rc
			<< ", errno=" << errno << " - " << strerror(errno) << std::endl;
		log(slavethreadlogfile,o.str());
		return false;
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

	ivy_float service_time_seconds, response_time_seconds;
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

	service_time_seconds = (ivytime(p_dun->end_time - p_dun->start_time).getlongdoubleseconds());

    if (service_time_seconds < 0. || (have_response_time && response_time_seconds < 0.))
    {
        std::ostringstream o;
        o << "WorkloadThread::popandpostprocessoneEyeo(): Negative I/O service time or negative response time - "
            << "I/O scheduled time = " << p_dun->scheduled_time.format_as_datetime_with_ns()
            << ", start time = " << p_dun->start_time.format_as_datetime_with_ns()
            << ", end time = " << p_dun->end_time.format_as_datetime_with_ns()
            << ", service_time = " << service_time_seconds
            << ", have_response_time = ";
        if (have_response_time)
        {
            o << "true";
        }
        else
        {
            o << "false";
        }
        o << ", response_time_seconds = " << response_time_seconds;
		log(slavethreadlogfile,o.str());
        throw std::runtime_error(o.str());
    }

	// NOTE:  The breakdown of bytes_transferred (MB/s) follows service time.


	if ( p_current_IogeneratorInput->blocksize_bytes == (unsigned int) p_dun->return_value) {

		int rs, rw, bucket;

		if (p_my_iogenerator->isRandom())
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
		p_current_SubintervalOutput->u.a.bytes_transferred.rs_array[rs][rw][bucket].push(p_current_IogeneratorInput->blocksize_bytes);

		if (have_response_time)
		{
			bucket = Accumulators_by_io_type::get_bucket_index( response_time_seconds );

			p_current_SubintervalOutput->u.a.response_time.rs_array[rs][rw][bucket].push(response_time_seconds);
		}

	} else {
		ioerrorcount++;
		if ( 0 > p_dun->return_value ) {
			std::ostringstream o;
			o << "Thread for " << workloadID.workloadID << " - I/O failed return code = " << p_dun->return_value << ", errno = " << errno << " - " << strerror(errno) << p_dun->toString() << std::endl;
			log(slavethreadlogfile,o.str());
				// %%%%%%%%%%%%%%% come back here later and decide if we want to re-drive failing I/Os, keeping track of the original I/O start time so we record the total respone time for all attempts.
		} else {
			std::ostringstream o;
			o << "Thread for " << workloadID.workloadID << "- I/O only transferred " << p_dun->return_value << " out of requested " << p_current_IogeneratorInput->blocksize_bytes << std::endl;
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
//*debug*/ {std::ostringstream o; o << "WorkloadThread::linux_AIO_driver_run_subinterval() Entered with IOPS=" << p_current_IogeneratorInput->IOPS << std::endl;log(slavethreadlogfile,o.str());}

	ivytime& subinterval_ending_time = p_current_subinterval->end_time;

    if (routine_logging) {std::ostringstream o; o << "WorkloadThread::linux_AIO_driver_run_subinterval() running subinterval which will end at " << subinterval_ending_time.format_as_datetime_with_ns() << std::endl; log(slavethreadlogfile,o.str());}

	while (true) {
		now.setToNow();
//*debug*/{ostringstream o; o << "\n/*debug*/ run_subinterval() top of loop at " << now.format_as_datetime_with_ns() << " queue depth = " << queue_depth << ", precomputeQ.size()=" << precomputeQ.size() << ", postprocessQ.size()=" << postprocessQ.size()<< std::endl; log(slavethreadlogfile,o.str());}
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
		else if ( 0.0 == p_current_IogeneratorInput->IOPS )
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
		else if ( (nextIO_schedule_time <= now) && (queue_depth < p_current_IogeneratorInput->maxTags) )
			waitduration = ivytime(0);
		else if (queue_depth >= p_current_IogeneratorInput->maxTags)
			waitduration=subinterval_ending_time - now;
		else if (nextIO_schedule_time > subinterval_ending_time)
			waitduration=subinterval_ending_time - now;
		else
			waitduration=nextIO_schedule_time - now;

		// Did your brain just explode?

//*debug*/{ostringstream o; o << "/*debug*/ WorkloadThread::linux_AIO_driver_run_subinterval() waitduration = " << waitduration.format_as_duration_HMMSSns() << std::endl; log(slavethreadlogfile,o.str());}
//*debug*/{ostringstream o; o << "/*debug*/ WorkloadThread::linux_AIO_driver_run_subinterval() queue depth = " << queue_depth << std::endl; log(slavethreadlogfile,o.str());}

		// OK, we've got the wait duration if we are going to have to wait for I/O ending events.

		if ((0 == queue_depth) && (ivytime(0) < waitduration))
		{	// The one case where we simply wait unconditionally is if there are no I/Os in flight
			// and the scheduled time to start the next I/O is in the future;
//*debug*/{ostringstream o; o << "/*debug*/ WorkloadThread::linux_AIO_driver_run_subinterval() no inflight I/Os.  Waiting for " << waitduration.format_as_duration_HMMSSns() << " until end of subinterval or time to start next I/O." << std::endl; log(slavethreadlogfile,o.str());}

			// There are no in-flight I/Os, and it's not time to issue the next I/O yet.
			struct timespec remainder;
			if (0!=nanosleep(&(waitduration.t),&remainder)){
				std::ostringstream o;
				o << "Possible construction zone.  nanosleep() failed " << errno << " - " << strerror(errno) << std::endl;
				log(slavethreadlogfile,o.str());
				//io_destroy(iogenerator_variables.act);
				return false;
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

			if (0 < (reaped = io_getevents(iogenerator_variables.act,events_needed,MAX_IOEVENTS_REAP_AT_ONCE,&(ReapHeap[0]),&(waitduration.t)))) {
//*debug*/{ostringstream o; o << "/*debug*/ WorkloadThread::linux_AIO_driver_run_subinterval() reaped = " << reaped << std::endl; log(slavethreadlogfile,o.str());}
				now.setToNow();  // because we may have waited

				p_current_SubintervalOutput->u.a.harvestcount.push((ivy_float)reaped);
				p_current_SubintervalOutput->u.a.preharvestqueuedepth.push((ivy_float)queue_depth);

				for (int i=0; i< reaped; i++) {

					struct io_event* p_event;
					Eyeo* p_Eyeo;

					p_event=&(ReapHeap[i]);
					p_Eyeo = (Eyeo*) p_event->data;
					p_Eyeo->end_time = now;
					p_Eyeo->return_value = p_event->res;
					p_Eyeo->errno_value = p_event->res2;

					queue_depth--;
					postprocessQ.push(p_Eyeo);
				}
				p_current_SubintervalOutput->u.a.postharvestqueuedepth.push((ivy_float)queue_depth);
				continue;  // start back up to the top and only do other stuff if there's nothing to reap
			}
		}

		// we have fallen through from harvesting all available events and waiting
//*debug*/{ostringstream o; o << "/*debug*/ WorkloadThread::linux_AIO_driver_run_subinterval() we have fallen through from harvesting all available events and waiting. " << std::endl; log(slavethreadlogfile,o.str());}
		if ( (precomputeQ.size() > 0) && (nextIO_schedule_time <= now) && (queue_depth < p_current_IogeneratorInput->maxTags) ) {
			// It's time to launch the next I/O[s].

			uint64_t launch_count=0;
			while
			(
			        (!precomputeQ.empty())
			     && (now >= (precomputeQ.front()->scheduled_time))
			     && (launch_count < (MAX_IOS_LAUNCH_AT_ONCE-1))  // launch count starts at zero
			     && (queue_depth+launch_count < p_current_IogeneratorInput->maxTags)
			)
			{
				LaunchPad[launch_count]=precomputeQ.front();
				precomputeQ.pop_front();
				LaunchPad[launch_count]->start_time=now;
				launch_count++;
			}
			rc = io_submit(iogenerator_variables.act, launch_count, (struct iocb **) LaunchPad /* iocb comes first in an Eyeo */);
			if (-1==rc) {
				std::ostringstream o;
				o << "io_submit() failed, errno=" << errno << " - " << strerror(errno) << std::endl;
				log(slavethreadlogfile,o.str());
				io_destroy(iogenerator_variables.act);
				return false;
			} else {
//*debug*/{ostringstream o; o << "/*debug*/ WorkloadThread::linux_AIO_driver_run_subinterval() launched " << rc << " I/Os." << std::endl; log(slavethreadlogfile,o.str());}
				// return code is number of I/Os succesfully launched.
				p_current_SubintervalOutput->u.a.submitcount.push((ivy_float)rc);
				p_current_SubintervalOutput->u.a.presubmitqueuedepth.push((ivy_float)queue_depth);
				p_current_SubintervalOutput->u.a.postsubmitqueuedepth.push((ivy_float)(queue_depth+rc));
				queue_depth+=rc;
				if (maxqueuedepth<queue_depth) maxqueuedepth=queue_depth;

				// We put back any unsuccessfully launched back into the precomputeQ.

				if (0==rc)log(slavethreadlogfile,
					"io_submit() return code zero.  No I/Os successfully submitted.\n");

				int number_to_put_back=launch_count - rc;
				p_current_SubintervalOutput->u.a.putback.push((ivy_float) number_to_put_back);

				if (routine_logging && number_to_put_back>0)
				{
					std::ostringstream o;
					o << "aio context didn\'t take all the I/Os.  Putting " << number_to_put_back << " back." << std::endl;
					log(slavethreadlogfile,o.str());
				}

				while (number_to_put_back>0) {
					// we put them back in reverse order of how they came out
					precomputeQ.push_front((Eyeo*)(LaunchPad[launch_count-1]->eyeocb.aio_data));
					launch_count--;
					number_to_put_back--;
				}
			}
			continue;
		}

		if (postprocessQ.size()>0) {
//*debug*/{ostringstream o; o << "/*debug*/ WorkloadThread::linux_AIO_driver_run_subinterval() before post processing an I/O. " << std::endl; log(slavethreadlogfile,o.str());}
			popandpostprocessoneEyeo();
			continue;
		}

//*debug*/{ostringstream o; o << "/*debug*/ WorkloadThread::linux_AIO_driver_run_subinterval() precomputeQ.size()=" << precomputeQ.size() << ", precomputedepth=" << precomputedepth << ". " << std::endl; log(slavethreadlogfile,o.str());}

		if ( precomputeQ.size() < precomputedepth && !cooldown && (precomputeQ.size() == 0 || (now + precompute_horizon) > precomputeQ.back()->scheduled_time) )
		{
			// precompute an I/O
			if (freeStack.empty()) {
				log(slavethreadlogfile,"freeStack is empty - can\'t precompute.\n");
				io_destroy(iogenerator_variables.act);
				return false;  // should not occur unless there's a design fault or maybe if CPU-bound
			}
			Eyeo* p_Eyeo = freeStack.top();
			freeStack.pop();

//*debug*/{std::ostringstream o; o << "/*debug*/ WorkloadThread::linux_AIO_driver_run_subinterval() calling generate() to precompute an I/O. " << std::endl; log(slavethreadlogfile,o.str());}

//*debug*/{std::ostringstream o; o << "/*debug*/ WorkloadThread::linux_AIO_driver_run_subinterval() p_my_iogenerator->instanceType()=\"" << p_my_iogenerator->instanceType() << '\"' << std::endl; log(slavethreadlogfile,o.str());}

            // deterministic generation of "seed" for this I/O.
            if (have_writes)
            {
                if (doing_dedupe)
                {
                    serpentine_number += threads_in_workload_name;
                    uint64_t new_pattern_number = ceil(serpentine_number * serpentine_multiplier);
                    while (pattern_number < new_pattern_number)
                    {
                        xorshift64star(pattern_seed);
                        pattern_number++;
                    }
                    block_seed = pattern_seed ^ pattern_number;
                }
                else
                {
                    xorshift64star(block_seed);
                }
            }

			// now need to call the iogenerator function to populate this Eyeo.
			if (!p_my_iogenerator->generate(*p_Eyeo)) {
				log(slavethreadlogfile,"iogenerator::generate() failed.\n");
				io_destroy(iogenerator_variables.act);
				return false;  // come back when we need to
			}
//*debug*/{ostringstream o; o << "/*debug*/ precomputed I/O. = " << p_Eyeo->toString() << std::endl; log(slavethreadlogfile,o.str());}
			precomputeQ.push_back(p_Eyeo);
		}

	}
//*debug*/{ostringstream o; o << "/*debug*/ WorkloadThread::linux_AIO_driver_run_subinterval() End of subinterval, postprocessing any leftover completed I/Os. " << std::endl; log(slavethreadlogfile,o.str());}

	while (postprocessQ.size()>0) popandpostprocessoneEyeo();
		// This does make it slightly non-responsive at the end of a subinterval ...


	return true;
}

bool WorkloadThread::catch_in_flight_IOs_after_last_subinterval()
{
//*debug*/{ std::ostringstream o; o << "bool WorkloadThread::catch_in_flight_IOs_after_last_subinterval() - entry with queue_depth = " << queue_depth << std::endl; log(slavethreadlogfile,o.str());}
	ivytime catch_wait_duration(120); // seconds.  Allows for a super long time for error recovery.

	while (0 < queue_depth)
	{
		int events_needed=1;

		// when we do the io_getevents() call, if we set the minimum number of events needed to 0 (zero), I expect the call to be non-blocking;

		if (0 < (reaped = io_getevents(iogenerator_variables.act,events_needed,MAX_IOEVENTS_REAP_AT_ONCE,&(ReapHeap[0]),&(catch_wait_duration.t))))
		{
			queue_depth-=reaped;
//*debug*/{ std::ostringstream o; o << "bool WorkloadThread::catch_in_flight_IOs_after_last_subinterval() - reaped " << reaped << " I/Os resulting in new queue_depth = " << queue_depth << std::endl; log(slavethreadlogfile,o.str());}
		}
		else if (0== reaped)
		{
			log(slavethreadlogfile,std::string("WorkloadThread::catch_in_flight_IOs_after_last_subinterval() - io_getevents() timed out.  There was an I/O that took over two minutes and still hadn\'t completed.\n"));
			return false;
		}
		else
		{
			log(slavethreadlogfile,std::string("WorkloadThread::catch_in_flight_IOs_after_last_subinterval() - io_getevents() failed."));
			return false;
		}
	}

	io_destroy(iogenerator_variables.act);

	return true;

}

