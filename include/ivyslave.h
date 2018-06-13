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
// ivyslave.h


#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <malloc.h>
#include <random>
#include <scsi/sg.h>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <linux/aio_abi.h>
#include <semaphore.h>
#include <chrono>

using namespace std;

#include "ivytime.h"
#include "iosequencer_stuff.h"
#include "WorkloadID.h"
#include "ListOfWorkloadIDs.h"
#include "LUN.h"
#include "ivydefines.h"
#include "discover_luns.h"
#include "LDEVset.h"
#include "ivyhelpers.h"
#include "ivybuilddate.h"
#include "ivylinuxcpubusy.h"
#include "IosequencerInput.h"
#include "Eyeo.h"
#include "Iosequencer.h"
#include "RunningStat.h"
#include "Accumulators_by_io_type.h"
#include "SubintervalOutput.h"
#include "Subinterval.h"
#include "WorkloadThread.h"


// variables

std::string hostname;

std::unordered_map<std::string, WorkloadThread*>
	workload_threads;  // Key looks like "workloadName:host:00FF:/dev/sdxx:2A"

ivytime
	test_start_time,
	subinterval_duration,
	master_thread_subinterval_end_time,
	lasttime;

int
	next_to_harvest_subinterval,
	otherSubinterval,
	running_subinterval;

struct procstatcounters
	interval_start_CPU, interval_end_CPU;


// the next "harvest" bit follows the currently being harvested subinterval
// this switches over at barrier 2 to the next subinterval
int
    harvest_subinterval_number;

ivytime
    harvest_start,
    harvest_end;

std::string
	slavelogfile;

bool routine_logging {false};

std::string printable_ascii;

std::ostringstream WorkloadThread_dying_words;

ivytime last_message_time = ivytime(0);

// ivyslave gets commands from pipe_driver_subthread:
//
// "[Die, Earthling!]" kills all workload subthreads and says dying words for any "died" or "normally_exited" threads
//      - we say "[what?]" followed by a line for each "died" or "normally_exited" with last_words.
//
// "send LUN header"
//      - we say [LUNheader]...
//
// "send LUN"
//      - we say [LUN]...
//      - last time say LUN<eof>
//
// "[CreateWorkload]" - rest of command line has parameters
//      - we do it (create new workload thread, start it running) and say OK.
//
// "[DeleteWorkload]" - rest of command line has parameters
//      - we do it and say OK.
//
// "[EditWorkload]" - rest of command line has parameters - ivy_engine API call is "edit rollup" which eventually translates into {EditWorkload]" here.
//      - we do it and say OK.
//
// "get subinterval result"
//      - this runs waitForSubintervalEndThenHarvest
//        which waits for the end of the subinterval,
//        then sends the CPU line, then iterates over the workloads,
//        waiting for each to turn off the "command posted" flag
//        which indicates the workload thread has switched over to the next subinterval,
//        and thus has consumed the earlier "keep_going" command.
//        Then we send up the workload detail line, and say <end> or something like that.
//        Then we send up the sequential progres line and the latencies line.
//
// "Go!" followed by subintervalseconds as ivytime.toString() format: <seconds,nanoseconds>
//     We get the workload thread look and post "ivyslave posted command" "start" and drop the lock
//     Then we wait for the workload thread to turn off "ivyslave posted command",
//     then when we have the lock, we post "keep_going", and turn on ivyslave posted command.
//     This is how the workload threads keep running at the end of the first subinterval.
//
// "continue" or "cooldown"
//     We record with a flag if it was cooldown.
//     We iterate over all the threads, waiting (FOREVER?!) to get the lock.
//     We post "keep going" and pass along the cooldown flag.
//     We say OK to pipe_driver_subthread.
//
// "stop"
//     We iterate over all workload threads, waiting forever to get the lock without a predicate or timeout
//     After posting "stop" to a workload thread, after we drop the lock we notify_all.
//     We say OK
//
// The waitForSubintervalEndThenHarvest() routine
//   - waits for the end of the subinterval
//   - records dispatching latency from end of subinterval to first code running.
//   - harvests CPU counters from /proc
//   - say(std::string("[CPU]")+cpubusysummary.toString());
//





























