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

#pragma once

#include <unistd.h>
#include <syscall.h>

extern bool routine_logging;

// We can't use POSIX AIO, because it uses a thread model internally.

class Workload;
class WorkloadThread;
class LUN;
class Eyeo;

class TestLUN
{
    // There is the original "LUN" class - LUN.h

    // The LUN class tracks a physical LUN, containing
    // attribute name - attribute value pairs as populated using
    // the output csv file from showluns.sh.

    // Ivydriver builds those LUNs when it fires up and discovers its LUNs.

    // Back in "ivy" on the central host, for each test host the same LUNs
    // are first reconstitued from what ivydriver sent, and then
    // if there's a command device connector running, these
    // LUNs are augmented with information from the subsystem configuration
    // including things you get indirectly, like what's the drive_type for a DP-vol?
    // One of the things we do is find the pool volumes for each pool,
    // and then capture what we need from the pool volumes like drive type
    // as attributes of a LUN.

    // Back on ivydriver, we have the original set of those LUNs built with
    // SCSI Inquiry attribute values, the ones that we sent up to the master host.

    // A "TestLUN" by contrast is only concerned with Workloads mapped to a
    // particular physical LUN like /dev/sdx.

    // In "create workload" we use a workload ID like sun159+/dev/sdc+fluffy.

    // All the workloads layered on the same underlying physical LUN
    // share the same AIO context which is in the TestLUN object.

    // For the first create workload on each LUN, a TestLUN springs
    // into existance to drive the shared AIO context.

    // Correspondingly, when the last workload on a LUN is deleted,
    // so is the TestLUN.

    // So the existence of a TestLUN means that there is one or more
    // Workloads currently in existence on the LUN.


public:
//variables

    std::map<std::string /*WorkloadID host+lun+workload_name*/, Workload> workloads {};

    std::map<std::string, Workload>::iterator start_IOs_Workload_bookmark;
	std::map<std::string, Workload>::iterator pop_one_bookmark;
	std::map<std::string, Workload>::iterator generate_one_bookmark;

    std::vector<Workload*> iops_max_workloads;

    std::string host_plus_lun {};

	LUN* pLUN;

	WorkloadThread* pWorkloadThread {};

	long long int maxLBA;

	ivy_float skew_total;

	uint64_t LUN_size_bytes;

	int sector_size=512;

	int fd {-1};
	int fd_index {-1}; // index of fd in io_uring array of fds.

    ivy_float testLUN_furthest_behind_weighted_IOPS_max_skew_progress {0.0};

    bool all_workloads_are_IOPS_max {false};


//methods
    TestLUN(){}
    ~TestLUN(){}
    unsigned int sum_of_maxTags();
	void prepare_linux_AIO_driver_to_start(); // calls eponymous method of Workload

    void open_fd();

    unsigned int /* # of I/Os */ populate_sqes();
    unsigned int /* # of I/Os */ generate_IOs();

//    void catch_in_flight_IOs();
//    void ivy_cancel_IO(struct iocb*); // writes to log file indicating success or failure.
    //ivytime next_scheduled_io();
        // ivytime(0) means this TestLUN has no workloads at all or it has only IOPS=max workloads
    void pop_front_to_LaunchPad(Workload*);

    void abort_if_queue_depths_corrupted(const std::string& where_from, unsigned int call_count);

    void post_warning(const std::string& msg);

};












