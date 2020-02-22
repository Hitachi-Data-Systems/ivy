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

#define _LARGE_FILE_API

#include "ivytypes.h"
#include "ivyhelpers.h"
#include "ivydriver.h"

unsigned int TestLUN::sum_of_maxTags()
{
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


void TestLUN::post_warning(const std::string& msg) { pWorkloadThread->post_warning (host_plus_lun + " - "s + msg); }


void TestLUN::open_fd()
{
    if (workloads.size() == 0)
    {
        std::ostringstream o;
        o << "when trying open file descriptor for test LUN " << host_plus_lun << ", no workloads found on this LUN." << std::endl;
        throw std::runtime_error(o.str());
    }

	if (pLUN->contains_attribute_name(std::string("Sector Size")))
	{
		std::string secsize = pLUN->attribute_value(std::string("Sector Size"));
		trim(secsize);
		if (!isalldigits(secsize))
		{
			std::ostringstream o;
			o << "bad \"Sector Size\" attribute value \"" << secsize << "\" - must be all decimal digits 0-9." << std::endl;
            throw std::runtime_error(o.str());
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
		o << "LUN " << host_plus_lun << " does not have \"LUN name\" attribute.  In Linux, LUN names look like /dev/sdxx." << std::endl;
        throw std::runtime_error(o.str());
	}

	std::string LUNname = pLUN->attribute_value(std::string("LUN name"));

	if (fd != -1)
	{
	    std::ostringstream o;
	    o << "TestLUN::open_fd() - upon entry, fd was supposed to be closed but had value " << fd << ". Closing it before re-opening." << std::endl;
	    if (0 != close(fd))
	    {
	        o << "Close for previous fd value failed errno " << errno << " (" << std::strerror(errno) << ")." << std::endl;
	        throw std::runtime_error(o.str());
	    }
	    post_warning(o.str());

	    fd=-1;
    }

	fd = open64(LUNname.c_str(),O_RDWR+O_DIRECT);

	if (-1 == fd) {
		std::ostringstream o;
		o << "Failed - open64(\"" << LUNname << "\",O_RDWR+O_DIRECT) - errno " << errno << " (" << strerror(errno) << ')' << std::endl;
		throw std::runtime_error(o.str());
	}
	return;
}

void TestLUN::prepare_linux_AIO_driver_to_start()
{
//*debug*/ { std::ostringstream o; o << "TestLUN::prepare_linux_AIO_driver_to_start() - entry." << std::endl; log(pWorkloadThread->slavethreadlogfile,o.str());}

	// Coming in, we expect the subinterval_array[] IosequencerInput and SubintervalOutput objects to be prepared.
	// That means p_current_subinterval, p_current_IosequencerInput, and p_current_SubintervalOutput are set.

	// Note: When the iosequencer gets to the end of a subinterval, it always switches the currentSubintervalIndex and otherSubintervalIndex variables.
	//       That way, the subinterval that is sent up to ivymaster is always "otherSubintervalIndex", whether or not the iosequencer is to stop.

	//       However, when the iosequencer thread is told to "stop", what it does before stopping is copy the IosequencerInput object
	//       for the last subinterval that ran to the other one that didn't run, the other one that would have been the next one if we hadn't stopped.

	// This doesn't bother the asynchronous collection / sending up of the data for the last subinterval.

	if (ThreadState::waiting_for_command != pWorkloadThread->state)
	{
        std::ostringstream o; o << "prepare_linux_AIO_driver_to_start() was called, but the workload thread state was "
            << pWorkloadThread->state << ", not ThreadState::waiting_for_command as expected.\n" << std::endl;
        throw std::runtime_error(o.str());
	}
	// If Eyeos haven't been made, or it looks like we don't have enough for the next step, make some.
		// Figure out how many we need according to the precompute count, the post-process allowance, and maxTags parameter.
	// Empty the freeStack, precomputeQ, and postprocessQ.  Then fill the freeStack with our list of all Eyeos that exist.
	// Create the aio context.

	if (workloads.size() == 0) return;

	testLUN_furthest_behind_weighted_IOPS_max_skew_progress = 0.0;

	all_workloads_are_IOPS_max = true;

	for(auto& pear: workloads)
	{
	    {
	        Workload& w = pear.second;
	        Subinterval& s = w.subinterval_array[0];
	        IosequencerInput& ii = s.input;
            if (ii.IOPS != -1)
            {
                all_workloads_are_IOPS_max = false;
                break;
            }
	    }
	}

//*debug*/ { std::ostringstream o; o << "TestLUN::prepare_linux_AIO_driver_to_start() - returning normally." << std::endl; log(pWorkloadThread->slavethreadlogfile,o.str());}
	return;
}


unsigned int /* number of I/Os started */ TestLUN::populate_sqes()
{
    // This is the code that does the merge onto the TestLUN's AIO context
    // of the individual pre-compute queues of each workload

    if (workloads.size() == 0 || pWorkloadThread->have_failed_to_get_an_sqe) { return 0; }

    std::map<std::string, Workload>::iterator wit = start_IOs_Workload_bookmark;
        // gets initialized to workloads.begin() by IvyDriver::distribute_TestLUNs_over_cores()

    if (wit == workloads.end()) { return 0; }

    unsigned int total = 0;

    while (true)
    {
        total += wit->second.populate_sqes();

        wit++; if (wit == workloads.end()) { wit = workloads.begin(); }

        if (wit == start_IOs_Workload_bookmark || pWorkloadThread->have_failed_to_get_an_sqe) { break; }
    }

    start_IOs_Workload_bookmark++; if (start_IOs_Workload_bookmark == workloads.end()) { start_IOs_Workload_bookmark = workloads.begin(); }

    // recalculate furthest behind IOPS=max with positive skew workload.
    // This way at the bottom of the loop, we can use Workload::hold_back_this_time().

    bool first_pass = true;

    for (auto& pear : workloads)
    {
        {
            Workload& w = pear.second;

            if (w.is_IOPS_max_with_positive_skew()) // Here we are finding the furthest behind of the IOPS=max workloads with positive skew.
            {
                ivy_float x =
                w.workload_weighted_IOPS_max_skew_progress =
                    ( (ivy_float) w.workload_cumulative_launch_count ) / w.p_current_IosequencerInput->skew_weight;

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
    }

    return total;
}

unsigned int /* number of I/Os generated < ivydriver.  */  TestLUN::generate_IOs()
{
    if (workloads.size() == 0) return 0;

    unsigned int generated_qty {0};

    auto wit = generate_one_bookmark;

    while (true)
    {
        Workload* pWorkload = &(wit->second);

        generated_qty += pWorkload->generate_IOs();

        wit++; if (wit == workloads.end()) wit = workloads.begin();

        if (wit == generate_one_bookmark) break;
    }

    generate_one_bookmark++; if (generate_one_bookmark == workloads.end()) generate_one_bookmark = workloads.begin();

    return generated_qty;
}



void TestLUN::abort_if_queue_depths_corrupted(const std::string& where_from, unsigned int callcount)
{
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
            else if (ivydriver.track_long_running_IOs && w.running_IOs.size() != w.workload_queue_depth)
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
        }
    }

    if (bad)
    {
        std::ostringstream oo;
        oo << "in TestLUN::abort_if_queue_depths_corrupted() - from " << where_from << " - " << o.str() << "." << std::endl;
        throw std::runtime_error(oo.str());
    }

    return;
}


// Comments from populate_sqes() moved to bottom of source file:

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

