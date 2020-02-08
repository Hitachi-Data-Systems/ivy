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
#include <sys/timerfd.h>
#include <stdint.h>
#include <liburing.h>


#include "pattern.h"
#include "Subinterval.h"
#include "DedupeTargetSpreadRegulator.h"
#include "logger.h"
#include "ivydefines.h"

class TestLUN;
class Eyeo;


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


enum class cqe_type {timeout, eyeo};

struct cqe_shim
{
    cqe_type type {cqe_type::eyeo};
    __kernel_timespec kts;
    ivytime wait_until_this_time {0.0};
    ivytime when_inserted {0.0};

// methods
    ivytime scheduled_duration () const {
        if (wait_until_this_time.isZero() || when_inserted.isZero()) { return ivytime(0.0); }
        return wait_until_this_time - when_inserted;
    }

    ivytime since_insertion (const ivytime& now) const {
        if (now.isZero() || when_inserted.isZero()) { return ivytime(0.0); }
        return now - when_inserted;
    }

};

std::ostream& operator<<(std::ostream&, const struct cqe_shim&);

struct timeout_shims
{
    std::set<cqe_shim*> unexpired_shims {};

    ivytime unsubmitted_timeout   {0};

// methods:
    ivytime earliest_timeout();

    void delete_shim(struct cqe_shim*);

    struct cqe_shim* insert_timeout(const ivytime& then, const ivytime& now);
};

class WorkloadThread {
public:

// variables
    logger slavethreadlogfile {};

	std::mutex* p_ivydriver_main_mutex;

	pid_t my_pid;
	pid_t my_tid;
	pid_t my_pgid;

    struct timeout_shims unexpired_timeouts;

	unsigned int physical_core;

	unsigned int hyperthread;

	std::thread std_thread;

	std::vector<TestLUN*> pTestLUNs {}; // This gets rebuilt at "go".


//// thread interlock stuff:

	std::mutex slaveThreadMutex;
	std::condition_variable slaveThreadConditionVariable;

	bool ivydriver_main_posted_command = false;

	MainThreadCommand ivydriver_main_says {MainThreadCommand::die};

	ThreadState state {ThreadState::initial};

//// end of thread interlock stuff

	ivytime thread_view_subinterval_start;
	ivytime thread_view_subinterval_end;
	ivytime waitduration;

	ivytime switchover_begin;

    RunningStat<ivy_float,ivy_int> dispatching_latency_seconds;
    RunningStat<ivy_float,ivy_int> lock_aquisition_latency_seconds;
    RunningStat<ivy_float,ivy_int> switchover_completion_latency_seconds;

    bool dieImmediately {false};
    bool reported_error {false};

	std::vector<TestLUN*>::iterator pTestLUN_reap_IOs_bookmark;
    std::vector<TestLUN*>::iterator pTestLUN_pop_bookmark;
    std::vector<TestLUN*>::iterator pTestLUN_generate_bookmark;
    std::vector<TestLUN*>::iterator pTestLUN_populate_sqes_bookmark;

    int thread_view_subinterval_number {-1};

    unsigned int workload_thread_queue_depth {0};
    unsigned int workload_thread_queue_depth_limit {0};
    unsigned int workload_thread_max_queue_depth {0};
    bool have_warned_hitting_cqe_entry_limit {false};

    size_t number_of_IOs_running_at_end_of_subinterval {0};

    ivytime epoll_wait_until_time {0};


    unsigned long number_of_IO_errors {0};
    unsigned long IO_error_warning_count_limit {6};  // This value less one is the number of detailed error messages seen.

    size_t total_buffer_size {0};
    void* p_buffer_pool {nullptr};

    unsigned int total_maxTags {0};

    unsigned int requested_uring_entries {1};

    int io_uring_fd {-1};
    struct io_uring struct_io_uring;

    unsigned long
        cumulative_sqes             { 0 },
        cumulative_submits          { 0 },
        cumulative_waits            { 0 },

        cumulative_loop_passes      { 0 },
        cumulative_fruitless_passes { 0 },

        cumulative_cqes             { 0 },
        cumulative_timer_pop_cqes   { 0 },
        cumulative_eyeo_cqes        { 0 };

    unsigned int
        max_sqes_tried_to_submit_at_once { 0 },
        max_sqes_submitted_at_once       { 0 },
        max_cqes_reaped_at_once          { 0 };

    unsigned long consecutive_fruitless_passes {0};

	uint64_t debug_c {0}, debug_m {1000};

#define cqes_with_one_peek_limit (1024)

    struct io_uring_cqe* cqe_pointer_array[cqes_with_one_peek_limit];

    unsigned sqes_queued_for_submit {0};

    bool have_failed_to_get_an_sqe {false};
    bool did_something_this_pass {false};

#define SUBINTERVAL_END_USER_DATA (MAX_UINT64 - 1)
#define NEXT_IO_SUBMIT_TIME_USER_DATA (MAX_UINT64 - 1)
    unsigned wait_until_time_count {0};
    ivytime earliest_wait_until_time {};


//    /*debug*/std::map<int     ,struct counter> short_submit_counter_map {};
//    /*debug*/std::map<unsigned,struct counter> dropped_ocurrences_map   {};

    bool try_generating_IOs {true};


    double step_run_time_seconds {-1.0};

    RunningStat<ivy_float, ivy_int> number_of_concurrent_timeouts {};



//methods
	WorkloadThread(std::mutex*,unsigned int /*physical_core*/, unsigned int /*hyperthread*/);

	inline ~WorkloadThread() {}

    void increment_LUN_workload_starting_point_iterators();


    uint64_t xorshift1024star();

	std::string stateToString();
	std::string ivydriver_main_saysToString();
	void WorkloadThreadRun();
	void run_subinterval();

    unsigned int /* # of I/Os */ populate_sqes();
    unsigned int /* # of I/Os */ generate_IOs();

    void set_all_queue_depths_to_zero();

    void check_for_long_running_IOs();

    void post_error  (const std::string&);
    void post_warning(const std::string&);

    inline double loop_passes_per_IO()      { if (cumulative_eyeo_cqes == 0) {return -1;} return ( (double) cumulative_loop_passes      ) / ( (double) cumulative_eyeo_cqes ); }
    inline double timer_pops_per_second()   { if (cumulative_eyeo_cqes == 0 || step_run_time_seconds == 0.0) {return -1;} return ( (double) cumulative_timer_pop_cqes   ) / step_run_time_seconds; }
    inline double timer_pops_per_IO()       { if (cumulative_eyeo_cqes == 0) {return -1;} return ( (double) cumulative_timer_pop_cqes   ) / ( (double) cumulative_eyeo_cqes ); }
    inline double submits_per_IO()          { if (cumulative_eyeo_cqes == 0) {return -1;} return ( (double) cumulative_submits          ) / ( (double) cumulative_eyeo_cqes ); }
    inline double fruitless_passes_per_IO() { if (cumulative_eyeo_cqes == 0) {return -1;} return ( (double) cumulative_fruitless_passes ) / ( (double) cumulative_eyeo_cqes ); }
    inline double cqes_per_loop_pass()      { if (cumulative_loop_passes==0) {return -1;} return ( (double) cumulative_cqes             ) / ( (double) cumulative_loop_passes);}
    inline double sqes_per_submit()         { if (cumulative_submits   == 0) {return -1;} return ( (double) cumulative_sqes             ) / ( (double) cumulative_submits   ); }
    inline double IOs_per_system_call()     { if (cumulative_submits   == 0) {return -1;} return ( (double) cumulative_eyeo_cqes        ) / ( (double) (cumulative_submits + cumulative_waits) ); }

    void build_uring_and_allocate_and_mmap_Eyeo_buffers_and_build_Eyeos();
    void shutdown_uring_and_unmap_Eyeo_buffers_and_delete_Eyeos();
    void fill_in_timeout_sqe(struct io_uring_sqe* p_sqe, const ivytime& timestamp, const ivytime& now);
    void process_cqe(struct io_uring_cqe*, const ivytime&);

    void try_to_submit_unsubmitted_timeout(const ivytime& now); // returns true if any pending timeouts were successfully submitted.

    void possibly_insert_timeout(const ivytime&); // sets did_something_this_pass

    void examine_timer_pop(struct cqe_shim* p_shim, const ivytime& );


    struct io_uring_sqe* get_sqe();

    void log_io_uring_engine_stats();
    void catch_in_flight_IOs_after_last_subinterval();
};

//    io_uring timeout shims
//
//    The io_uring supports a timeout sqe (submit queue element) opcode,
//    which lets us not have to use timer_fds, epoll_waits, etc.
//
//    Instead you submit timeouts like you submit I/Os,and you get an
//    io_uring completion event could be for either an I/O or a timeout.
//
//    With ivy's original Linux aio support, we needed an aio context for each LUN,
//    whereas with io_uring we have one ring with its associated fd for all the
//    TestLUNs owned by a particular core thread.  This collapses the number of
//    system calls needed.
//
//    We submit a timeout sqe for a particular scheduled time in the future,
//    and when there are no completion events to harvest, no I/Os to start
//    and all workload precompute queues are full, we wait at the top of
//    the loop for at least 1 io_uring completion event.
//
//    A timeout completion event will wake us up if necesary to start scheduled I/Os
//    at the right time.
//
//    We submit using an sqe with opcode IORING_OP_TIMEOUT and where we set the timeout
//    duration into sqe.addr as a "timespec64", which is a struct timespec variant
//    that has 64-bit tv.sec and 64-bit tv_nsec.values.
//
//    There's also a sqe opcode flag field that ivy doesn't use where for
//    sqe.opcode == IORING_OP_TIMEOUT we could set sqe.timeout_flags to IORING_TIMEOUT_ABS.
//
//    We don't need it, but in any case the documentation as of 2019-12-13 doesn't yet
//    describe the encoding of the timespec64 to hold an absolute date-timestamp.
//
//    ivytime uses CLOCK_MONOTONIC_RAW for internal operations, which is a form of
//    raw hardware CPU clock cycles since boot.  In ivy we need to use this clock
//    as resetting the clock while ivy's running will likely cause it to explode
//    as the realtime interlocks to apparently time out.
//
//    However, whenever ivy formats an ivytime (struct timespec) value for displaying
//    or printing, it show like a CLOCK_REALTIME value because once at the beginning ivy
//    gets a CLOCK_REALTIME time as well as the usual CLOCK_MONOTONIC_RAW
//    and computes and saves the "ivytime reference delta" which gets added onto
//    every ivytime CLOCK_MONOTONIC_RAW to print it as a date/time.
//
//    So there are three distinct kinds of io_uring sqe used by ivy:
//    - timeout with duration to subinterval end
//    - timeout with duration to time to start the next sheduled I/O.
//    - I/O operation
//
//    To enable us to recognise when we get an io_uring cqe (complete queue entry)
//    what type of sqe it was for, we use a "cqe_shim".
//
//    This a two part stub with class enum shim type, and a second field only used
//    for timeouts which has the scheduled "wait until" time that was associated
//    to the expiry of the timespec64 duration we requested.
//
//    With every io_uring sqe there is an 8-byte user_data field that we set with a pointer
//    to a cqe_shim.
//
//    Every Eyeo object begin with a cqe-shim of type "Eyeo", so a pointer
//    to an Eyeo is also a pointer to a cqe_shim.
//
//    When we submit an I/O operation we put a pointer to an Eyeo into sqe.user_data.
//
//    This way when we get a completion event, if it's of type Eyeo
//    we cast user_data to an Eyeo pointer and process the I/O completion event.
//
//    If it's a timeout completion cqe, then when we see the subinterval end timeout,
//    this is what triggers WorkloadThread to switch over to the next
//    subinterval.  We don't do anything if it's not a subinterval end timeout,
//    other than do remove from the set of unexpired timeouts.
//
//    Timeouts are used to wake up at the appointed time either to switch to the
//    next subinterval, or to issue I/Os that are supposed to be started at that time.
//
//    It's possible that when waiting, an I/O completion event may come in that
//    opens up an available "tag" for a workload "head of queue" I/O scheduled
//    for an even earlier start time on a different workload.  For this, we need
//    to submit a timeout scheduled for that even earlier start time.
//
//    We don't remove redundant timeouts that are no longer needed.
//    Even though you could submit an sqe with opcode IORING_OP_TIMEOUT_REMOVE,
//    this would not save any resource since all a timeout does is wake up
//    ivy if sleeping its easier to check if there's anything to do.
//
//    At any point in time we keep track of earliest scheduled time across
//    a set week keep of unexpired timeouts.
//
//    This way when we have an available tag for a workoad, and its next Eyeo
//    in the precompue queue is for a time in the future, we compare with the
//    earliest unexpired timeout to see if there is already an earlier or same
//    time unexpired timeout, in which case we don't need to submit a new timeout sqe.
//
//    To be know what the earliest scheduled unexpired timeouts, we use the
//    scheduled time field in the cqe_shim, and we maintain a set of pointers
//    to currently unexpired timeouts.
