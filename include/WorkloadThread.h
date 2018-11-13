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
#pragma once

#include <list>
#include <stack>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <string>
#include <set>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include "pattern.h"
#include "Subinterval.h"

enum class ThreadState
{
    undefined,
    initial,
	waiting_for_command,
	running,
	died,
	exited_normally
};

std::ostream& operator<< (std::ostream& o, const ThreadState& s);

std::string threadStateToString (const ThreadState);

enum class MainThreadCommand
{
    start, keep_going, stop, die
};

std::string mainThreadCommandToString(MainThreadCommand);


class WorkloadThread {
public:

// variables
    std::string slavethreadlogfile {};

	std::mutex* p_ivyslave_main_mutex;

	pid_t my_pid;
	pid_t my_tid;
	pid_t my_pgid;

	unsigned int core;

	std::thread std_thread;

	std::vector<TestLUN*> pTestLUNs {}; // This gets rebuilt at "go".

    void increment_LUN_workload_starting_point_iterators();


//// thread interlock stuff:

	std::mutex slaveThreadMutex;
	std::condition_variable slaveThreadConditionVariable;

	bool ivyslave_main_posted_command = false;

	MainThreadCommand ivyslave_main_says {MainThreadCommand::die};

	ThreadState state {ThreadState::initial};

	std::string dying_words {};

//// end of thread interlock stuff

	ivytime now;
	ivytime thread_view_subinterval_start;
	ivytime thread_view_subinterval_end;
	ivytime waitduration;

	ivytime switchover_begin;

    RunningStat<ivy_float,ivy_int> dispatching_latency_seconds;
    RunningStat<ivy_float,ivy_int> lock_aquisition_latency_seconds;
    RunningStat<ivy_float,ivy_int> switchover_completion_latency_seconds;

    bool dieImmediately{false};

    bool cooldown {false};

	std::vector<TestLUN*>::iterator pTestLUN_reap_IOs_bookmark;
    std::vector<TestLUN*>::iterator pTestLUN_pop_bookmark;
    std::vector<TestLUN*>::iterator pTestLUN_generate_bookmark;
    std::vector<TestLUN*>::iterator pTestLUN_start_IOs_bookmark;

    int event_fd {-1};
    int epoll_fd {-1};
    epoll_event* p_epoll_events {nullptr};
    epoll_event ep_event;
    struct epoll_event epoll_ev;

/*debug*/ unsigned int debug_epoll_count {0};

//    int subinterval_number_wt {-1};
    int thread_view_subinterval_number {-1};

    unsigned int workload_thread_queue_depth {0};
    unsigned int workload_thread_max_queue_depth {0};

#ifdef IVYSLAVE_TRACE
    unsigned int wt_callcount_linux_AIO_driver_run_subinterval {0};
    unsigned int wt_callcount_cancel_stalled_IOs {0};
    unsigned int wt_callcount_start_IOs {0};
    unsigned int wt_callcount_reap_IOs {0};
    unsigned int wt_callcount_pop_and_process_an_Eyeo {0};
    unsigned int wt_callcount_generate_an_IO {0};
    unsigned int wt_callcount_catch_in_flight_IOs_after_last_subinterval {0};
#endif

//methods
	WorkloadThread(std::mutex*,unsigned int /*core*/);
    inline ~WorkloadThread() {};

    uint64_t xorshift1024star();

	std::string stateToString();
	std::string ivyslave_main_saysToString();
	void WorkloadThreadRun();
	bool linux_AIO_driver_run_subinterval();
	void catch_in_flight_IOs_after_last_subinterval();

    void cancel_stalled_IOs();

    unsigned int /* # of I/Os */ reap_IOs();
    unsigned int /* # of I/Os */ start_IOs();
    unsigned int /* # of I/Os */ post_process_an_IO();
    unsigned int /* # of I/Os */ generate_an_IO();
    unsigned int /* # of I/Os */ pop_and_process_an_Eyeo();

    void die_saying(std::string);
    void set_all_queue_depths_to_zero();
    void post_Error_for_main_thread_to_say(const std::string&);
    void post_Warning_for_main_thread_to_say(const std::string&);

    void close_all_fds();

#ifdef IVYSLAVE_TRACE
    void log_bookmark_counters();
#endif

};

