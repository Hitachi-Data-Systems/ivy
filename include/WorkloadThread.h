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

#include "pattern.h"
#include "Subinterval.h"

typedef long double pattern_float_type ;

enum class ThreadState
{
    undefined,
	waiting_for_command,
	running,
	stopping, // for example catching ing-flight I/Os after final subinterval
	died,
	exited_normally
};

enum class BallInCourt
{
    wl_orchestrator,
    wl_thread
};

std::ostream& operator<< (std::ostream& o, const ThreadState s);

std::string threadStateToString (const ThreadState);

enum class MainThreadCommand
{
	run, stop, die
};

std::string mainThreadCommandToString(MainThreadCommand);


class WorkloadThread {
public:

// variables
	WorkloadID workloadID; // to get the string form of the workload ID, refer to workloadID.workloadID.
	LUN* pLUN;
	long long int maxLBA;
	iosequencer_stuff iosequencer_variables;
	std::string iosequencerParameters; // just used to print a descriptive line when the thread fires up.
	std::mutex* p_ivyslave_main_mutex;

	std::thread std_thread;


	std::mutex slaveThreadMutex;
	std::condition_variable slaveThreadConditionVariable;

//	std::mutex ball_in_court_lk;
//	std::condition_variable ball_in_court_cv;;

	ivytime nextIO_schedule_time;  // 0 means means right away.  (This is initialized later.)
	ivytime now;
	ivytime subinterval_duration, currentSubintervalEndTime;
	ivytime waitduration;
	ivytime precompute_horizon {ivytime(0,125000000)};

    RunningStat<ivy_float,ivy_int> seconds_to_be_dispatched_after_subinterval_end;
    RunningStat<ivy_float,ivy_int> seconds_to_get_lock_at_end_of_subinterval;
    RunningStat<ivy_float,ivy_int> seconds_after_subinterval_end_completed_switchover;

	Subinterval subinterval_array[2];

	Subinterval* p_current_subinterval;
	Subinterval* p_other_subinterval;

	IosequencerInput* p_current_IosequencerInput;
	IosequencerInput* p_other_IosequencerInput;
	SubintervalOutput* p_current_SubintervalOutput;
	SubintervalOutput* p_other_SubintervalOutput;

	Iosequencer* p_my_iosequencer;

	std::list<Eyeo*> allEyeosThatExist;

	std::stack<Eyeo*> freeStack;
		// we should come back later when this code runs multiple times and make sure
		// we keep a copy of the pointers to all the Eyeos we created.
		// this way we could just discard all the pointer structures / queues at the end of a test
		// and reinitialize them from scratch, reusing the same Eyeos to populate the freeStack at the beginning


	std::list<Eyeo*> precomputeQ;  // We are using a std::list instead of a std::queue so that
	                               // if io_submit() doesn't successfully launch all of a batch
	                               // then we can put the ones that didn't launch back into
	                               // the precomputeQ in original sequence at the wrong end backwards.
	//struct procstatcounters psc_start, psc_end;

	std::queue<Eyeo*> postprocessQ;

	Eyeo* LaunchPad[MAX_IOS_LAUNCH_AT_ONCE];

	struct io_event ReapHeap[MAX_IOEVENTS_REAP_AT_ONCE];

	std::set<struct iocb*> running_IOs;  // The iocb points to the ivy Eyeo object

	ivy_int cancelled_IOs {0};

	ivytime max_io_run_time_seconds = ivytime(IO_TIME_LIMIT_SECONDS);

	std::map<std::string, Iosequencer*> available_iosequencers;

	bool dieImmediately{false};
	bool ivyslave_main_posted_command = false;  // This is used for thread interlock, so made it volatile just in case.


	MainThreadCommand ivyslave_main_says {MainThreadCommand::die};

	ThreadState state {ThreadState::died};
	BallInCourt ball_in_whose_court {BallInCourt::wl_thread};

	int EyeoCount;
	int currentSubintervalIndex, otherSubintervalIndex, subinterval_number;  // The master thread never sets these or refers to them.
	int ioerrorcount=0;
	unsigned int precomputedepth;  // will be set to be equal to the maxTags spec when we get "Go!"
	int rc;
	unsigned int queue_depth{0};
	int events_needed;
	int reaped;
	unsigned int maxqueuedepth{0};

	std::string hostname;
	std::string slavethreadlogfile;

    bool doing_dedupe;
    bool have_writes;
    ivy_float compressibility;

    pattern_float_type threads_in_workload_name;
    pattern_float_type serpentine_number;
    pattern_float_type serpentine_multiplier;

    pattern pat;
    uint64_t pattern_number;
    uint64_t pattern_seed;
    uint64_t block_seed;

    uint64_t write_io_count;

    bool cooldown {false};

    ivy_float sequential_fill_fraction {1.0};
        // IogeneratorSequential for writes sets this to a value expressing the progress from 0.0 to 1.0
        // of what fraction of all blocks in the coverage range within the LUN have been written to so far.

//methods
	WorkloadThread(std::string /*workloadID*/, LUN*, long long int /*lastLBA*/, std::string /*parms*/, std::mutex*);
	inline ~WorkloadThread() {};

    uint64_t xorshift1024star();

	std::string stateToString();
	std::string ivyslave_main_saysToString();
	void popandpostprocessoneEyeo();
	void WorkloadThreadRun();
	bool prepare_linux_AIO_driver_to_start();  // true means success. false means an error message has been logged and to abort.
	bool linux_AIO_driver_run_subinterval();
	bool catch_in_flight_IOs_after_last_subinterval();

    void ivy_cancel_IO(struct iocb*); // writes to log file indicating success or failure.
    void cancel_stalled_IOs();
};

