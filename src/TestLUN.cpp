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

#include <fcntl.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include "TestLUN.h"
#include "Workload.h"
#include "WorkloadThread.h"
#include "LUN.h"
#include "ivytime.h"
#include "Eyeo.h"
#include "ivydefines.h"

//#define IVYDRIVER_TRACE
// IVYDRIVER_TRACE defined here in this source file rather than globally in ivydefines.h so that
//  - the CodeBlocks editor knows the symbol is defined and highlights text accordingly.
//  - you can turn on tracing separately for each class in its own source file.


unsigned int TestLUN::sum_of_maxTags()
{
#if defined(IVYDRIVER_TRACE)
    { sum_of_maxTags_callcount++; if (sum_of_maxTags_callcount <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << host_plus_lun << ':' << sum_of_maxTags_callcount << ") ";
    o << "      Entering TestLUN::sum_of_maxTags()."; log(pWorkloadThread->slavethreadlogfile,o.str()); } }
#endif

    unsigned int total {0};

    for (auto& pear : workloads)
    {
        {
            // inner block to get a fresh references each time.

            Workload& w = pear.second;
            Subinterval& s = w.subinterval_array[0];
            IosequencerInput& i = s.input;
            total += i.maxTags;
        }
    }

    return total;
}

void TestLUN::open_fd()
{
#if defined(IVYDRIVER_TRACE)
    { open_fd_callcount++; if (open_fd_callcount <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << ':' << open_fd_callcount << ") ";
    o << "      Entering TestLUN::open_fd()."; log(pWorkloadThread->slavethreadlogfile,o.str()); } }
#endif

    if (workloads.size() == 0)
    {
        std::ostringstream o;
        o << "<Error> When trying open file descriptor for test LUN "
            << host_plus_lun << ", no workloads found on this LUN." << std::endl
            << "Source code reference line " <<__LINE__ << " of " <<__FILE__ << "." << std::endl;
		pWorkloadThread->dying_words = o.str();
		log(pWorkloadThread->slavethreadlogfile, pWorkloadThread->dying_words);
		pWorkloadThread->state = ThreadState::died;
        pWorkloadThread->slaveThreadConditionVariable.notify_all();
		exit(-1);
    }

	if (pLUN->contains_attribute_name(std::string("Sector Size")))
	{
		std::string secsize = pLUN->attribute_value(std::string("Sector Size"));
		trim(secsize);
		if (!isalldigits(secsize))
		{
			std::ostringstream o;
			o << "<Error> LUN " << host_plus_lun
			    << " - bad \"Sector Size\" attribute value \"" << secsize
			    << "\" - must be all decimal digits 0-9." << std::endl;
            pWorkloadThread->dying_words = o.str();
			log(pWorkloadThread->slavethreadlogfile, o.str());
			pWorkloadThread->state = ThreadState::died;
			pWorkloadThread->slaveThreadConditionVariable.notify_all();
			exit(-1);
		}
		std::istringstream is(secsize);
		is >> sector_size;
	}
	else
	{
		sector_size = 512;
	}

	if (!pLUN->contains_attribute_name(std::string("LUN name")))
	{
		std::ostringstream o;
		o << "<Error> LUN " << host_plus_lun
		    << " does not have \"LUN name\" attribute.  In Linux, LUN names look like /dev/sdxx." << std::endl
            << "Source code reference line " <<__LINE__ << " of " <<__FILE__ << "." << std::endl;
		pWorkloadThread->dying_words = o.str();
		log(pWorkloadThread->slavethreadlogfile, pWorkloadThread->dying_words);
		pWorkloadThread->state = ThreadState::died;
        pWorkloadThread->slaveThreadConditionVariable.notify_all();
		exit(-1);
	}

	std::string LUNname = pLUN->attribute_value(std::string("LUN name"));

	fd = open64(LUNname.c_str(),O_RDWR+O_DIRECT);

	if (-1 == fd) {
		std::ostringstream o;
		o << "<Error> Failed trying to open file descriptor for LUN " << host_plus_lun
            << " - open64(\"" << LUNname << "\",O_RDWR+O_DIRECT) failed errno = "
            << errno << " (" << strerror(errno) << ')' << std::endl;
		pWorkloadThread->dying_words = o.str();
		pWorkloadThread->state = ThreadState::died;
		log(pWorkloadThread->slavethreadlogfile, pWorkloadThread->dying_words);
        pWorkloadThread->slaveThreadConditionVariable.notify_all();
		exit(-1);
	}
	return;
}

void TestLUN::prepare_linux_AIO_driver_to_start()
{
#if defined(IVYDRIVER_TRACE)
    { prepare_linux_AIO_driver_to_start_callcount++; if (prepare_linux_AIO_driver_to_start_callcount <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << host_plus_lun << ':' << prepare_linux_AIO_driver_to_start_callcount << ") ";
    o << "      Entering TestLUN::prepare_linux_AIO_driver_to_start()."; } }
#endif

	// Coming in, we expect the subinterval_array[] IosequencerInput and SubintervalOutput objects to be prepared.
	// That means p_current_subinterval, p_current_IosequencerInput, and p_current_SubintervalOutput are set.

	// Note: When the iosequencer gets to the end of a subinterval, it always switches the currentSubintervalIndex and otherSubintervalIndex variables.
	//       That way, the subinterval that is sent up to ivymaster is always "otherSubintervalIndex", whether or not the iosequencer is to stop.

	//       However, when the iosequencer thread is told to "stop", what it does before stopping is copy the IosequencerInput object
	//       for the last subinterval that ran to the other one that didn't run, the other one that would have been the next one if we hadn't stopped.

	// This doesn't bother the asynchronous collection / sending up of the data for the last subinterval.

	if (ThreadState::waiting_for_command != pWorkloadThread->state)
	{
        std::ostringstream o; o << "<Error> prepare_linux_AIO_driver_to_start() was called, but the workload thread state was "
            << pWorkloadThread->state << ", not ThreadState::waiting_for_command as expected.\n" << std::endl
             << "Occured at line " << __LINE__ << " of " << __FILE__ << std::endl;
        pWorkloadThread->dying_words = o.str();
        log(pWorkloadThread->slavethreadlogfile,pWorkloadThread->dying_words);
        pWorkloadThread->state=ThreadState::died;
        pWorkloadThread->slaveThreadConditionVariable.notify_all();
        exit(-1);
	}
	// If Eyeos haven't been made, or it looks like we don't have enough for the next step, make some.
		// Figure out how many we need according to the precompute count, the post-process allowance, and maxTags parameter.
	// Empty the freeStack, precomputeQ, and postprocessQ.  Then fill the freeStack with our list of all Eyeos that exist.
	// Create the aio context.

	if (workloads.size() == 0) return;

	max_IOs_launched_at_once = 0;
	max_IOs_reaped_at_once = 0;
	max_IOs_tried_to_launch_at_once = 0;

	open_fd();

    // Now we create the eventfd that all I/Os into the LUN's aio context will use.
    // This fd will be closed when at the end of a test step we destroy the corresponding aio context.
    // There's a 64-bit unsigned counter, an 8-byte thing you read or write.

    //      A write(2) call adds the 8-byte integer value supplied in its
    //      buffer to the counter.

    //      *  If EFD_SEMAPHORE was not specified and the eventfd counter
    //         has a nonzero value, then a read(2) returns 8 bytes
    //         containing that value, and the counter's value is reset to
    //         zero.
    //
    //      *  If EFD_SEMAPHORE was specified and the eventfd counter has
    //         a nonzero value, then a read(2) returns 8 bytes containing
    //         the value 1, and the counter's value is decremented by 1.

    int rc;
	act=0;  // must be set to zero otherwise io_setup() will fail with return code 22 - invalid parameter.

	if (0 != ( rc = io_setup(sum_of_maxTags(),&act)))
	{
        std::ostringstream o;
        o << "<Error> io_setup(" << sum_of_maxTags() << "," << (&act) << ") failed return code " << rc
			<< ", errno=" << errno << " - " << strerror(errno) << std::endl
            << "Occured at line " << __LINE__ << " of " << __FILE__ << std::endl;
        pWorkloadThread->dying_words = o.str();
        log(pWorkloadThread->slavethreadlogfile,pWorkloadThread->dying_words);
        pWorkloadThread->state=ThreadState::died;
        pWorkloadThread->slaveThreadConditionVariable.notify_all();
        exit(-1);
	}

	if (routine_logging)
	{
        std::ostringstream o;
        o << "TestLUN::prepare_linux_AIO_driver_to_start() done - LUN opened with fd = " << fd
            << ", aio context successfully constructed, sharing eventfd = " << pWorkloadThread->event_fd
            << " and epoll fd " << pWorkloadThread->epoll_fd << "." << std::endl;
        log(pWorkloadThread->slavethreadlogfile,o.str());
	}

	return;
}


unsigned int /* number of I/Os reaped */ TestLUN::reap_IOs()
{
#if defined(IVYDRIVER_TRACE)
    { reap_IOs_callcount++; if (reap_IOs_callcount <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << host_plus_lun << ':' << reap_IOs_callcount << ") ";
    o << "      Entering TestLUN::reap_IOs().";
    abort_if_queue_depths_corrupted("TestLUN::reap_IOs()", reap_IOs_callcount); } }
#endif

    if (testLUN_queue_depth == 0) return 0;

    ivytime wait_duration(0);

    // inline int io_getevents(aio_context_t ctx, long min_nr, long max_nr, struct io_event *events, struct timespec *timeout)
    // 	{ return syscall(__NR_io_getevents, ctx, min_nr, max_nr, events, timeout); }

    int reaped = io_getevents(
                            act,
                            0, // zero events_needed, meaning non-blocking
                            MAX_IOEVENTS_REAP_AT_ONCE,
                            &(ReapHeap[0]),
                            &(wait_duration.t)
                            );
    if (reaped < 0)
    {
        std::ostringstream o;
        o << "<Error> internal processing error - call to Linux AIO context io_getevents() "
            << "return code " << reaped << " errno " << errno << " - " << strerror(errno) << "."
            <<  " at " << __FILE__ << " line " << __LINE__ << std::endl;
        pWorkloadThread->dying_words = o.str();
        log(pWorkloadThread->slavethreadlogfile,o.str());
        io_destroy(act);
        pWorkloadThread->state=ThreadState::died;
        pWorkloadThread->slaveThreadConditionVariable.notify_all();
        exit(-1);
    }

    if (reaped == 0) return 0;

    ivytime now; now.setToNow();

    if (reaped > max_IOs_reaped_at_once) max_IOs_reaped_at_once = reaped;

    unsigned int n {0};

    for (int i=0; i < reaped; i++)
    {
        struct io_event* p_event;
        Eyeo* p_Eyeo;
        struct iocb* p_iocb;

        p_event=&(ReapHeap[i]);
        p_Eyeo = (Eyeo*) p_event->data;
        p_iocb = (iocb*) p_event->obj;
        p_Eyeo->end_time = now;
        p_Eyeo->return_value = p_event->res;
        p_Eyeo->errno_value = p_event->res2;

        Workload* pWorkload = p_Eyeo->pWorkload;

        auto running_IOs_iter = pWorkload->running_IOs.find(p_iocb);
        if (running_IOs_iter == pWorkload->running_IOs.end())
        {
            std::ostringstream o;
            o << "<Error> internal processing error - an asynchrononus I/O completion event occurred for a Linux aio I/O tracker "
                << "that was not found in the ivy workload\'s \"running_IOs\" (set of pointers to Linux I/O tracker objects)."
                << "  The ivy Eyeo object associated with the completion event describes itself as "
                << p_Eyeo->toString()
                <<  " at " << __FILE__ << " line " << __LINE__ << std::endl;
            pWorkloadThread->dying_words = o.str();
            log(pWorkloadThread->slavethreadlogfile,o.str());
            io_destroy(act);
            pWorkloadThread->state=ThreadState::died;
            pWorkloadThread->slaveThreadConditionVariable.notify_all();
            exit(-1);
        }
        pWorkload->running_IOs.erase(running_IOs_iter);

                     pWorkload->workload_queue_depth--;
                                 testLUN_queue_depth--;
        pWorkloadThread->workload_thread_queue_depth--;

        pWorkload->postprocessQ.push(p_Eyeo);

        n++;

#ifdef IVYDRIVER_TRACE
        if (reap_IOs_callcount <= FIRST_FEW_CALLS)
        {
            std::ostringstream o;
            o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << host_plus_lun << ") =|= reaped " << p_Eyeo->thumbnail();
            log(pWorkloadThread->slavethreadlogfile,o.str());
        }
#endif

    }

    return n;
}

void TestLUN::pop_front_to_LaunchPad(Workload* pWorkload)
{
#if defined(IVYDRIVER_TRACE)
    { pop_front_to_LaunchPad_callcount++; if (pop_front_to_LaunchPad_callcount <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << host_plus_lun << ':' << pop_front_to_LaunchPad_callcount << ") ";
    o << "      Entering TestLUN::pop_front_to_LaunchPad(" << pWorkload->workloadID << ")."; log(pWorkloadThread->slavethreadlogfile,o.str());} }
#endif

    if (pWorkload == nullptr)
    {
        std::ostringstream o;
        o << "<Error> internal processing error - pop_front_to_LaunchPad() called with null pointer to Workload"
            <<  " at " << __FILE__ << " line " << __LINE__ << std::endl;
        pWorkloadThread->dying_words = o.str();
        log(pWorkloadThread->slavethreadlogfile,o.str());
        io_destroy(act);
        pWorkloadThread->state=ThreadState::died;
        pWorkloadThread->slaveThreadConditionVariable.notify_all();
        exit(-1);
    }

    if (launch_count >= MAX_IOS_LAUNCH_AT_ONCE)
    {
        std::ostringstream o;
        o << "<Error> internal processing error - pop_front_to_LaunchPad() called with no space in LaunchPad"
            <<  " at " << __FILE__ << " line " << __LINE__ << std::endl;
        pWorkloadThread->dying_words = o.str();
        log(pWorkloadThread->slavethreadlogfile,o.str());
        io_destroy(act);
        pWorkloadThread->state=ThreadState::died;
        pWorkloadThread->slaveThreadConditionVariable.notify_all();
        exit(-1);
    }

    if (pWorkload->precomputeQ.size() == 0)
    {
        std::ostringstream o;
        o << "<Error> internal processing error - pop_front_to_LaunchPad() called for a Workload with an empty precomputeQ"
            <<  " at " << __FILE__ << " line " << __LINE__ << std::endl;
        pWorkloadThread->dying_words = o.str();
        log(pWorkloadThread->slavethreadlogfile,o.str());
        io_destroy(act);
        pWorkloadThread->state=ThreadState::died;
        pWorkloadThread->slaveThreadConditionVariable.notify_all();
        exit(-1);
    }

    ivytime now; now.setToNow();

    Eyeo* pEyeo = pWorkload->precomputeQ.front();

    pWorkload->precomputeQ.pop_front();

    LaunchPad[launch_count++]=pEyeo;

    pWorkload->workload_cumulative_launch_count++;

    pEyeo->start_time=now;

    pEyeo->eyeocb.aio_resfd = pWorkloadThread->event_fd;
    pEyeo->eyeocb.aio_flags = pEyeo->eyeocb.aio_flags | IOCB_FLAG_RESFD;

#ifdef IVYDRIVER_TRACE
    if (pop_front_to_LaunchPad_callcount <= FIRST_FEW_CALLS)
    {
        std::ostringstream o;
        o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << host_plus_lun << ") =|= popped to Launchpad" << pEyeo->thumbnail();
        log(pWorkloadThread->slavethreadlogfile,o.str());
    }
#endif

    return;
}

unsigned int /* number of I/Os started */ TestLUN::start_IOs()
{
#if defined(IVYDRIVER_TRACE)
    { start_IOs_callcount++; if (start_IOs_callcount <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << host_plus_lun << ':' << start_IOs_callcount << ") ";
    o << "      Entering TestLUN::start_IOs()."; log(pWorkloadThread->slavethreadlogfile,o.str()); } }
#endif

    // This is the code that does the merge onto the TestLUN's AIO context
    // of the individual pre-compute queues of each workload

    if (workloads.size() == 0) { return 0; }

    launch_count = 0;

    std::map<std::string, Workload>::iterator wit = start_IOs_Workload_bookmark;
        // gets initialized to workloads.begin() by IvyDriver::distribute_TestLUNs_over_cores()

#if defined(IVYDRIVER_TRACE)
    (*wit).second.start_IOs_Workload_bookmark_count++;
#endif

    if (wit == workloads.end()) { return 0; }

    bool first_pass {true};

    for (auto& pear : workloads) // Here we are finding the furthest behind of the IOPS=max workloads with positive skew.
    {
        if (pear.second.is_IOPS_max_with_positive_skew())
        {
            ivy_float x =
            pear.second.workload_weighted_IOPS_max_skew_progress =
                ( (ivy_float) pear.second.workload_cumulative_launch_count ) / pear.second.p_current_IosequencerInput->skew_weight;

            if (first_pass)
            {
                first_pass = false;

                testLUN_furthest_behind_weighted_IOPS_max_skew_progress = x;
            }
            else
            {
                if (x < testLUN_furthest_behind_weighted_IOPS_max_skew_progress)
                {
                    testLUN_furthest_behind_weighted_IOPS_max_skew_progress = x;
                }
            }
        }
    }

    while (launch_count < MAX_IOS_LAUNCH_AT_ONCE)
    {
#if defined(IVYDRIVER_TRACE)
        (*wit).second.start_IOs_Workload_body_count++;
#endif
        wit->second.load_IOs_to_LaunchPad();

        wit++; if (wit == workloads.end()) { wit = workloads.begin(); }

        if (wit == start_IOs_Workload_bookmark) { break; }
    }

    start_IOs_Workload_bookmark++; if (start_IOs_Workload_bookmark == workloads.end()) { start_IOs_Workload_bookmark = workloads.begin(); }

    // Now LaunchPad is fully loaded, issue submit and then put back any non-starters.

    if (launch_count == 0) return 0;

    // Now launch and then put back any Eyeos that didn't get launched.

    if (max_IOs_tried_to_launch_at_once < launch_count) max_IOs_tried_to_launch_at_once = launch_count;

    int rc = io_submit(act, launch_count, (struct iocb **) LaunchPad /* iocb comes first in an Eyeo */);
    if (-1==rc)
    {
        std::ostringstream o;
        o << "<Error> Internal programming error - asynchronous I/O \"io_submit()\" failed, errno=" << errno << " - " << strerror(errno)
            <<  " at " << __FILE__ << " line " << __LINE__ << std::endl;
        pWorkloadThread->dying_words = o.str();
        log(pWorkloadThread->slavethreadlogfile,o.str());
        io_destroy(act); close(fd); close(pWorkloadThread->event_fd); close(pWorkloadThread->epoll_fd);
        pWorkloadThread->state=ThreadState::died;
        pWorkloadThread->slaveThreadConditionVariable.notify_all();
        exit(-1);
    }
    // return code is number of I/Os succesfully launched.

    ivytime running_timestamp;
    running_timestamp.setToNow();

    if (rc > max_IOs_launched_at_once) max_IOs_launched_at_once = rc;

    for (int i=0; i<rc; i++)
    {
        Eyeo* pEyeo = (Eyeo*)(LaunchPad[i]->eyeocb.aio_data);
        pEyeo->running_time = running_timestamp;
        Workload* pWorkload = pEyeo->pWorkload;

        auto running_IOs_iterator = pWorkload->running_IOs.insert(&(pEyeo->eyeocb));
        if (running_IOs_iterator.first == pWorkload->running_IOs.end())
        {
            std::ostringstream o;
            o << "<Error> internal processing error - failed trying to insert a Linux aio I/O tracker "
                << "into the ivy workload\'s \"running_IOs\" (set of pointers to Linux I/O tracker objects)."
                << "  The ivy Eyeo object associated with the completion event describes itself as "
                << pEyeo->toString() <<  " - at " << __FILE__ << " line " << __LINE__ << std::endl;
            pWorkloadThread->dying_words = o.str();
            log(pWorkloadThread->slavethreadlogfile,o.str());
            io_destroy(act);
            pWorkloadThread->state=ThreadState::died;
            pWorkloadThread->slaveThreadConditionVariable.notify_all();
            exit(-1);
        }

        pWorkload->workload_queue_depth++;
        if (pWorkload->workload_queue_depth     > pWorkload->workload_max_queue_depth)
            pWorkload->workload_max_queue_depth = pWorkload->workload_queue_depth;

        testLUN_queue_depth++;
        if (testLUN_queue_depth > testLUN_max_queue_depth)
            testLUN_max_queue_depth = testLUN_queue_depth;


        pWorkloadThread->workload_thread_queue_depth++;
        if (pWorkloadThread->workload_thread_queue_depth     > pWorkloadThread->workload_thread_max_queue_depth)
            pWorkloadThread->workload_thread_max_queue_depth = pWorkloadThread->workload_thread_queue_depth;

#ifdef IVYDRIVER_TRACE
        if (start_IOs_callcount <= FIRST_FEW_CALLS)
        {
            std::ostringstream o;
            o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << host_plus_lun << ") =|= started " << pEyeo->thumbnail();
            log(pWorkloadThread->slavethreadlogfile, o.str());
        }
#endif

    }

    // We put back any unsuccessfully launched back into the precomputeQ.

    if (0 == rc) log(pWorkloadThread->slavethreadlogfile, "io_submit() return code zero.  No I/Os successfully submitted.\n");

    int number_to_put_back = launch_count - rc;

    if (routine_logging && number_to_put_back>0)
    {
        std::ostringstream o;
        o << "aio context didn\'t take all the I/Os.  Putting " << number_to_put_back << " back." << std::endl;
        log(pWorkloadThread->slavethreadlogfile,o.str());
    }

    while (number_to_put_back>0) {
        // we put them back in reverse order of how they came out

        Eyeo* pEyeo = (Eyeo*)(LaunchPad[launch_count-1]->eyeocb.aio_data);

        Workload* pWorkload = pEyeo->pWorkload;

        struct iocb* p_iocb = &(pEyeo->eyeocb);

        auto running_IOs_iter = pWorkload->running_IOs.find(p_iocb);
        if (running_IOs_iter == pWorkload->running_IOs.end())
        {
            std::ostringstream o;
            o << "<Error> internal processing error - after a submit was not successful for all I/Os, when trying to "
                << "remove from \"running_IOs\" one of the I/Os that wasn\'t successfully launched, that "
                << "unsuccessful I/O was not found in \"running_IOs\" for the owning Workload."
                << "  The ivy Eyeo object associated with the completion event describes itself as "
                << pEyeo->toString()
                <<  " at " << __FILE__ << " line " << __LINE__
                ;
            pWorkloadThread->dying_words = o.str();
            log(pWorkloadThread->slavethreadlogfile,o.str());
            io_destroy(act); close(fd); close(pWorkloadThread->event_fd); close(pWorkloadThread->epoll_fd);
            pWorkloadThread->state = ThreadState::died;
            pWorkloadThread->slaveThreadConditionVariable.notify_all();
            exit(-1);
        }
        pWorkload->running_IOs.erase(running_IOs_iter);

        pWorkload->precomputeQ.push_front(pEyeo);

        pWorkload->workload_cumulative_launch_count--;

        launch_count--;
        number_to_put_back--;
    }

    return launch_count;
}

unsigned int /* number of I/Os popped and processed */ TestLUN::pop_and_process_an_Eyeo()
{
#if defined(IVYDRIVER_TRACE)
    { pop_and_process_an_Eyeo_callcount++;
        if (pop_and_process_an_Eyeo_callcount <= FIRST_FEW_CALLS)     { std::ostringstream o;
        o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << host_plus_lun << ':' << pop_and_process_an_Eyeo_callcount << ") ";
        o << "      Entering TestLUN::pop_and_process_an_Eyeo()."; log(pWorkloadThread->slavethreadlogfile,o.str()); }

        abort_if_queue_depths_corrupted("TestLUN::pop_and_process_an_Eyeo",pop_and_process_an_Eyeo_callcount);
    }
#endif

    if (workloads.size() == 0) return 0;

    unsigned int n {0};

    auto wit = pop_one_bookmark;

#ifdef IVYDRIVER_TRACE
    (*wit).second.pop_one_bookmark_count++;
#endif

    while (true)
    {
        Workload* pWorkload = &(wit->second);

#ifdef IVYDRIVER_TRACE
        pWorkload->pop_one_body_count++;
#endif
        n = pWorkload->pop_and_process_an_Eyeo();

        if ( n > 0) { break; }

        wit++; if (wit == workloads.end()) wit = workloads.begin();

        if (wit == pop_one_bookmark) break;
    }

    pop_one_bookmark++; if (pop_one_bookmark == workloads.end()) pop_one_bookmark = workloads.begin();

    return n;
}


unsigned int /* number of I/Os generated - 0 or 1  */  TestLUN::generate_an_IO()
{
#if defined(IVYDRIVER_TRACE)
    { generate_an_IO_callcount++; if (generate_an_IO_callcount <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << host_plus_lun << ':' << generate_an_IO_callcount << ") ";
    o << "      Entering TestLUN::generate_an_IO()."; log(pWorkloadThread->slavethreadlogfile,o.str()); }
    abort_if_queue_depths_corrupted("TestLUN::generate_an_IO", generate_an_IO_callcount); }
#endif

    if (workloads.size() == 0) return 0;

    unsigned int generated_one {0};

    auto wit = generate_one_bookmark;

#ifdef IVYDRIVER_TRACE
    (*wit).second.generate_one_bookmark_count++;
#endif

    while (true)
    {
        Workload* pWorkload = &(wit->second);

#ifdef IVYDRIVER_TRACE
        pWorkload->generate_one_body_count++;
#endif

        generated_one = pWorkload->generate_an_IO();
        if (generated_one) break;

        wit++; if (wit == workloads.end()) wit = workloads.begin();

        if (wit == generate_one_bookmark) break;
    }

    generate_one_bookmark++; if (generate_one_bookmark == workloads.end()) generate_one_bookmark = workloads.begin();

    return generated_one;
}


ivytime TestLUN::next_scheduled_io()
        // ivytime(0) is returned if this TestLUN has no scheduled I/Os in Workload precompute queues, only possibly IOPS=max I/Os.
{
#if defined(IVYDRIVER_TRACE)
    { next_scheduled_io_callcount++; if (next_scheduled_io_callcount <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << host_plus_lun << ':' << next_scheduled_io_callcount << ") ";
    o << "      Entering TestLUN::next_scheduled_io()."; log(pWorkloadThread->slavethreadlogfile,o.str()); }
    abort_if_queue_depths_corrupted("TestLUN::next_scheduled_io", next_scheduled_io_callcount); }
#endif

    ivytime tijd {0};

    for (auto& pear : workloads)
    {
        Workload* pWorkload = &(pear.second);

        if (pWorkload->precomputeQ.size() == 0) continue;

        auto pEyeo = pWorkload->precomputeQ.front();

        if (tijd == ivytime(0))
        {
            tijd = pEyeo->scheduled_time;
        }
        else
        {
            // we know we have a previous non-zero time to compare to
            if ( pEyeo->scheduled_time != ivytime(0) )
            {
                // This one is non-zero as well, so select the earlier
                if (tijd > pEyeo->scheduled_time)
                {
                    tijd = pEyeo->scheduled_time;
                }
            }
        }
    }

    return tijd;
}

void TestLUN::catch_in_flight_IOs()
{
#if defined(IVYDRIVER_TRACE)
    { catch_in_flight_IOs_callcount++; if (catch_in_flight_IOs_callcount <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << host_plus_lun << ':' << catch_in_flight_IOs_callcount << ") ";
    o << "      Entering TestLUN::catch_in_flight_IOs()."; log(pWorkloadThread->slavethreadlogfile,o.str()); } }
#endif
    // While there are events to harvest within 1 ms, do so.

	ivytime zero(0);

    int reaped;

	while (testLUN_queue_depth > 0 && (reaped=io_getevents(act,
	                                                       0,  // need zero events, meaning non-blocking
	                                                       MAX_IOEVENTS_REAP_AT_ONCE,
	                                                       &(ReapHeap[0]),
	                                                       &(zero.t) // This would otherwise mean wait forever.
	                                                       )
                                      ) != 0
          )
	{
	    if (reaped < 0)
		{
            std::ostringstream o;
            o << "<Error> TestLUN::catch_in_flight_IOs() - io_getevents() failed return code " << reaped
                << ", errno = " << errno << " - " << strerror(errno)
                << " at " << __FILE__ << " line " << __LINE__ ;
            pWorkloadThread->dying_words= o.str();
            log(pWorkloadThread->slavethreadlogfile,o.str());
            for (auto& pTestLUN : pWorkloadThread->pTestLUNs) { io_destroy(pTestLUN->act); close(pTestLUN->fd); }
            close(pWorkloadThread->event_fd);
            close(pWorkloadThread->epoll_fd);
            pWorkloadThread->state = ThreadState::died;
            pWorkloadThread->slaveThreadConditionVariable.notify_all();
            exit(-1);
		}

        for (int i=0; i< reaped; i++)
        {
            struct io_event* p_event;
            Eyeo* p_Eyeo;
            struct iocb* p_iocb;

            p_event=&(ReapHeap[i]);
            p_Eyeo = (Eyeo*) p_event->data;
            p_iocb = (iocb*) p_event->obj;

            p_Eyeo->pWorkload->workload_queue_depth --;
            testLUN_queue_depth--;
            pWorkloadThread->workload_thread_queue_depth--;

            auto running_IOs_iter =  p_Eyeo->pWorkload->running_IOs.find(p_iocb);
            if  (running_IOs_iter == p_Eyeo->pWorkload->running_IOs.end())
            {
                std::ostringstream o;
                o << "<Error> internal processing error - in catch_in_flight_IOs(), an asynchrononus I/O completion event occurred for a Linux aio I/O tracker "
                    << "that was not found in the ivy workload thread\'s \"running_IOs\" (set of pointers to Linux I/O tracker objects)."
                    << "  The ivy Eyeo object associated with the completion event describes itself as "
                    << p_Eyeo->toString()
                    <<  "  " << __FILE__ << " line " << __LINE__
                    ;
                pWorkloadThread->dying_words = o.str();
                log(pWorkloadThread->slavethreadlogfile,o.str());
                for (auto& pTestLUN : pWorkloadThread->pTestLUNs) { io_destroy(pTestLUN->act); close(pTestLUN->fd); }
                close(pWorkloadThread->event_fd);
                close(pWorkloadThread->epoll_fd);
                pWorkloadThread->state = ThreadState::died;
                pWorkloadThread->slaveThreadConditionVariable.notify_all();
                exit(-1);
            }
            p_Eyeo->pWorkload->running_IOs.erase(running_IOs_iter);

            if (p_Eyeo->pWorkload->workload_queue_depth != p_Eyeo->pWorkload->running_IOs.size())
            {
                std::ostringstream o;
                o << "<Error> internal programming error - before cancelling any remaining I/Os found to our consternation that queue_depth was "
                    << p_Eyeo->pWorkload->workload_queue_depth
                    << " but running_IOs.size() was " << p_Eyeo->pWorkload->running_IOs.size()
                    <<  ".  Occurred  at " << __FILE__ << " line " << __LINE__ ;
                pWorkloadThread->dying_words= o.str();
                log(pWorkloadThread->slavethreadlogfile,o.str());
                for (auto& pTestLUN : pWorkloadThread->pTestLUNs) { io_destroy(pTestLUN->act); close(pTestLUN->fd); }
                close(pWorkloadThread->event_fd);
                close(pWorkloadThread->epoll_fd);
                pWorkloadThread->state = ThreadState::died;
                pWorkloadThread->slaveThreadConditionVariable.notify_all();
                exit(-1);
            }
        }
	}

    return;
}


void TestLUN::abort_if_queue_depths_corrupted(const std::string& where_from, unsigned int callcount)
{
    unsigned int workload_queue_depth_total {0};

    std::ostringstream o;

    o << " at entry to " << where_from << ": ";

    bool bad { false };

    for (auto& pear : workloads)
    {
        {
            // inner block to get fresh references each time.

            Workload& w = pear.second;

            Subinterval& s = w.subinterval_array[0];

            IosequencerInput& i = s.input;

            unsigned int maxTags_setting = i.maxTags;

            if (w.workload_queue_depth > maxTags_setting)
            {
                bad = true;

                o << "Workload " << w.workloadID
                    << " workload_queue_depth = " << w.workload_queue_depth
                    << " but maxTags = " << maxTags_setting << " - bad!" << std::endl;
            }
            else if (w.workload_queue_depth > w.workload_max_queue_depth)
            {
                bad = true;

                o << "Workload " << w.workloadID
                    << " workload_queue_depth = " << w.workload_queue_depth
                    << " but workload_max_queue_depth = " << w.workload_max_queue_depth << " - bad!" << std::endl;
            }
            else if (w.running_IOs.size() != w.workload_queue_depth)
            {
                bad = true;

                o << "Workload " << w.workloadID
                    << " workload_queue_depth = " << w.workload_queue_depth
                    << " but workload's running_IOs.size() = " << w.running_IOs.size() << " - bad!" << std::endl;
            }
            else
            {
                o << "Workload " << w.workloadID
                    << " workload_queue_depth = " << w.workload_queue_depth
                    << " workload_max_queue_depth = " << w.workload_max_queue_depth
                    << " | ";
            }

            workload_queue_depth_total += w.workload_queue_depth;
        }
    }

    if (workload_queue_depth_total != testLUN_queue_depth)
    {
        bad = true;

        o << "Total over Workloads of workload_queue_depth is " << workload_queue_depth_total
            << " but testLUN_queue_depth is " << testLUN_queue_depth << " - bad!" << std::endl;
    }
    else if (workload_queue_depth_total > testLUN_max_queue_depth)
    {
        bad = true;

        o << "Total over Workloads of workload_queue_depth is " << workload_queue_depth_total
            << " but testLUN_max_queue_depth = " << testLUN_max_queue_depth << " - bad!" << std::endl;
    }
    else
    {
        o << "testLUN_queue_depth = " << testLUN_queue_depth;
        o << ", testLUN_max_queue_depth = " << testLUN_max_queue_depth;
    }


    if (bad)
    {
        std::ostringstream oo;
        oo << "<Error> Internal programming error in TestLUN::abort_if_queue_depths_corrupted() - from " << where_from
            << " - " << o.str() << "- Occurred at line " << __LINE__ << " of " << __FILE__ << ".";

        pWorkloadThread->dying_words= oo.str();
        log(pWorkloadThread->slavethreadlogfile,oo.str());
        io_destroy(act);
        pWorkloadThread->state = ThreadState::died;
        pWorkloadThread->slaveThreadConditionVariable.notify_all();
        pWorkloadThread->post_Error_for_main_thread_to_say(oo.str());

        exit(-1);
    }

    return;
}

void TestLUN::ivy_cancel_IO(struct iocb* p_iocb)
{
#if defined(IVYDRIVER_TRACE)
    { ivy_cancel_IO_callcount++; if (ivy_cancel_IO_callcount <= FIRST_FEW_CALLS) { std::ostringstream o;
    o << "(physical core" << pWorkloadThread->physical_core << " hyperthread " << pWorkloadThread->hyperthread << '-' << host_plus_lun << ':' << ivy_cancel_IO_callcount << ") ";
    o << "      Entering TestLUN::ivy_cancel_IO(struct iocb* p_iocb)."; log(pWorkloadThread->slavethreadlogfile,o.str()); } }
#endif

    Eyeo* p_Eyeo = (Eyeo*) p_iocb->aio_data;


    auto it = p_Eyeo->pWorkload->running_IOs.find(p_iocb);
    if (it == p_Eyeo->pWorkload->running_IOs.end())
    {
        std::ostringstream o;
        o << "<Error> ivy_cancel_IO was called to cancel an I/O that ivy wasn\'t tracking as running.  The I/O describes itself as " << p_Eyeo->toString()
            << ".  Occurred at line " << __LINE__ << " of " << __FILE__ << ".";
        pWorkloadThread->dying_words= o.str();
        log(pWorkloadThread->slavethreadlogfile,o.str());
        io_destroy(act);
        pWorkloadThread->state = ThreadState::died;
        pWorkloadThread->slaveThreadConditionVariable.notify_all();
        exit(-1);
    }

    {
        std::ostringstream o;
        o << "<Warning> about to cancel an I/O operation that has been running for " << (p_Eyeo->since_start_time()).format_as_duration_HMMSSns()
            << " described as " << p_Eyeo->toString();

        pWorkloadThread->post_Warning_for_main_thread_to_say(o.str());

        log(pWorkloadThread->slavethreadlogfile,o.str());
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
            pWorkloadThread->dying_words= o.str();
            log(pWorkloadThread->slavethreadlogfile,o.str());
            io_destroy(act);
            pWorkloadThread->state = ThreadState::died;
            pWorkloadThread->slaveThreadConditionVariable.notify_all();
            exit(-1);
        }
        rc = io_cancel(act,p_iocb, &ioev);
    }

    if (rc == 0)
    {
        p_Eyeo->pWorkload->running_IOs.erase(it);
        p_Eyeo->pWorkload->cancelled_IOs++;
        return;
    }

    {
        std::ostringstream o;
        o << "<Warning> in " << __FILE__ << " line " << __LINE__ << " in ivy_cancel_IO(), system call to cancel the I/O failed saying - ";
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

        pWorkloadThread->post_Warning_for_main_thread_to_say(o.str());

        log(pWorkloadThread->slavethreadlogfile,o.str());
    }
    return;
}


// Comments from start_IOs() moved to bottom of source file:

    // How the merge from multiple Workloads to start I/Os on the AIO context works:

    // First we do a pass through all the TestLUNs and their Workloads
    // where we pop as many "launchable" scheduled Eyeos and IOPS=max
    // but without positive "skew" I/Os from the precomputeQ for that workload
    // as we can, and add the Eyeo* to the LaunchPad of Eyeo* that we will use
    // in the AIO submit call.  This first phase leaves behind the
    // IOPS=max workloads with positive skew to merge later.

    // A scheduled Eyeo is one that, once we reach the scheduled time,
    // we launch the Eyeo as quickly as we can.

    // An IOPS=max I/O has a scheduled start time of ivytime(0) which
    // is always before "now", and so the I/Os run as fast as possible.

    // IOPS=max is coded as IOPS = -1 in the IosequencerInput object.

    // We do this first pass only to put one Eyeo at a time into LaunchPad.
    // If a workload has a launchable Eyeo, and that Eyeo is a scheduled I/O,
    // or an Eyeo for a Workload wtih IOPS= max but for negative skew
    // then we pop it off the Workload's precomputeQ, and add it to LaunchPad.

    // Then you repeat asking if it has a launchable Eyeo, and if that Eyeo
    // is a scheduled Eyeo, popping and adding to LaunchPad.

    // This process is done one I/O at a time, and if at any point,
    // LaunchPad becomes full, we skip ahead to where we do the AIO submit().

    // We use a "bookmark" iterator that we use as a round-robbin
    // starting point to look for I/Os to put on LaunchPad[].

    // This way every time the TestLUN tries to start some I/Os,
    // we start with the bookmarked round-robbin iterator,
    // and then when we do the AIO submit, we also increment the bookmark.
    // This is intended to give even priority to each Workload
    // in terms of access to submit I/Os to the TestLUN's AIO context.

    // The TestLUN's AIO context size is set to the sum of the maxTags
    // values for all Workloads on the TestLUN.  This means that as
    // long as the underlying I/O subsystem has a big enough
    // queue depth capability, all Workloads will have enough "tags"
    // to physically drive up to maxTags I/Os at once.

    // Now we come to merging IOPS=max I/O sequences from
    // multiple layered Workloads on a TestLUN's LaunchPad.

    // Note: A Workload must either issue only scheduled I/Os
    //       or IOPS=max I/Os.  Thus it is specifically not
    //       supported to change back and forth between
    //       scheduled & IOPS=max I/Os.
    //       You have been warned if you use "edit rollup" to do this.

    // Each Workload object has an EyeoCount value since "go".
    // I.e. this counter gets reset to zero when you get a "go".

    // Thus the interesting situation is where multiple Workloads
    // are running at IOPS=max.

    // For each IOPS=max workload, we divide ivy_float representation
    // of the I/O count so far by the Workload's skew_factor to yield
    // a scaled floating point "progress_indicator".

    // If your Workload IOPS is heavier, you need to do more I/Os
    // to make the same progress.

    // For IOPS=max workloads with positive skew, when it comes
    // time to launch I/Os, a workload is skipped in the rotation
    // to start I/Os if that workload is ahead of another workload's
    // skew progress.  If it's at the back, or tied for the back,
    // then it's allowed to submit as many I/Os this go-around.
    // Otherwise, we might otherwise limit the number of I/Os
    // issued at once, and fall back to issuing one I/O at a time.

