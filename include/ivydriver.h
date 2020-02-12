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
// ivydriver.h

#pragma once

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
#include <vector>

using namespace std;

#include "ivytime.h"
#include "WorkloadID.h"
#include "ListOfWorkloadIDs.h"
#include "LUN.h"
#include "ivydefines.h"
#include "discover_luns.h"
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

void initialize_io_time_clip_levels(); // see Accumulators_by_io_type.cpp

extern std::string printable_ascii;
extern bool routine_logging;

struct IvyDriver
{
        // There is one external instance "ivydriver" of this class
        // that is visible both to the main thread and to WorkloadThreads.

        // It's the main thread that builds and owns the Workload-TestLUN data
        // structures.

        // TestLUNs are created with the first associated [CreateWorklaod],
        // and destroyed with the last associated [DeleteWorkload].

        // Upon "go", distribute_TestLUNs_over_cores() does a
        // round robin reassignment of whatever TestLUNs exist at that time
        // to the various WorkloadThreads.

        // Thus there is a hierarchy:

        //   There is one externally visible instance of IvyDriver called ivydriver.
        //        ivydriver has WorkloadThreads, one per core.
        //            a WorkloadThread has an assigned set of pointers to TestLUNs
        //        ivydriver has a map of TestLUNs which each have one or more Workloads.

        // WorkloadThreads are only used during a "go" and then only if they have any assigned TestLUNs.




// variables

    std::string ivyscript_hostname;

    std::mutex ivydriver_main_mutex;

    std::map<unsigned int,WorkloadThread*> all_workload_threads {};

    std::map<unsigned int,WorkloadThread*>     workload_threads {}; // the ones with TestLUNs/Workloads

    std::map<unsigned int, bool> active_cores {};
        // This is used to filter Linux CPU busy data for only those Linux "processor" numbers (hyperthread numbers)
        // that have WorkloadThreads on them, and for which the associated WorkloadThread has TestLUNs/Workloads.

    std::map<std::string /* host+LUN part of workloadID */, TestLUN>
        testLUNs {};

    int step_number {-1};

    ivytime
        test_start_time,
        subinterval_duration,
        ivydriver_view_subinterval_start,
        ivydriver_view_subinterval_end,
        lasttime;

    int
        next_to_harvest_subinterval,
        otherSubinterval;

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
        input_line;

    logger slavelogfile {"/var/ivydriver_initial_log.txt"};

    ivytime last_message_time = ivytime_zero;

    std::vector<std::string> luns;

	discovered_LUNs disco;
	std::string header_line{""};
	std::string disco_line{""};

	std::map<std::string, LUN*> LUNpointers;
		// The map is from "/dev/sdxxx" to LUN*
		// We don't know what our ivyscript hostname is, as that gets glued on at the master end.
		// But the WorkloadID for each workload is named ivyscript_hostname+/dev/sdxxx+workload_name, so you can see the ivyscript_hostname there if you need it.

	std::string bracketedExit          {"[Exit]"s};
	std::string bracketedCreateWorkload{"[CreateWorkload]"s};
	std::string bracketedDeleteWorkload{"[DeleteWorkload]"s};
	std::string remainder;
	std::string subinterval_duration_as_string;

    bool spinloop {true};
    bool reported_error {false};
    ivytime time_error_reported {0};

    std::vector<std::string> workload_thread_error_messages {};
    std::vector<std::string> workload_thread_warning_messages {};

    RunningStat<double,long> digital_readouts;

    char subintervalOutput_buffer[200000]; // for printing a workload detail line

    bool warn_on_critical_temp {false};

    bool track_long_running_IOs {true}; // turn this off for pure speed but blind to long running I/Os.

    unsigned int generate_at_a_time {4}; // the number of IOs pre-generated (and for writes with pattern generateion) before we check for I/O completions;

    bool display_buffer_contents {false}; // used by operator<<(,const Eyeo

    unsigned long fruitless_passes_before_wait {1};

    unsigned sqes_per_submit_limit {1024};

    ivytime max_wait { 0.100 };

// methods

    int main(int argc, char* argv[]);

    void say(std::string s);

    void create_workload();

    void edit_workload();

    void delete_workload();

    void go();

    void distribute_TestLUNs_over_cores();

    void waitForSubintervalEndThenHarvest();

    void continue_or_cooldown();

    void stop();

    void killAllSubthreads();

    void log_TestLUN_ownership();

    void log_iosequencer_settings(const std::string&);

    void check_CPU_temperature();

    void post_error(const std::string&);

    std::string print_logfile_stats();

    void set_command(const std::string& attribute, const std::string& value);
};

extern struct IvyDriver ivydriver;



// ivydriver gets commands from pipe_driver_subthread:
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
//     We get the workload thread look and post "ivydriver posted command" "start" and drop the lock
//     Then we wait for the workload thread to turn off "ivydriver posted command",
//     then when we have the lock, we post "keep_going", and turn on ivydriver posted command.
//     This is how the workload threads keep running at the end of the first subinterval.
//
// "continue"
//     We iterate over all the threads, waiting (FOREVER?!) to get the lock.
//     We post "keep going".
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


//There are three views of subinterval numbers and starting/ending times:
//
//    - The ivydriver_view is for the purposes of the ivydriver main thread.
//
//        - for example, "test start time" is an ivydriver_view item, that is
//          referenced by all threads.
//
//        - ivydriver_view subinterval numbers and times are what you typically
//          use in ivydriver main thread log messages, and would reflect
//          where the ivydriver main thread was in the subinterval cycle.
//
//    - The thread_view is for the purposes of a WorkloadThread.
//
//        - A workload thread will iterate in general over multiple TestLUNs
//          and their Workloads for each increment of thread_view subinterval
//          numbers and starting/ending times.
//
//        - Different threads will each update their numbers / times asynchronously.
//
//        - Messages from WorkloadThread with thread_view subinterval numbers
//          and times reflect where WorkloadThread was in its subinterval cycle.
//
//    - The workload_view times are as seen by the live running workload
//      and are incremented during switchover().
//
//    Another thing to note is that the end of a subinterval at the thread
//    view level will occur after all the output data for that subinterval have
//    been processed and sent as a detail line to the ivy main host.
//
//     - This is later than when, during switchover(), the workload view
//       subinterval numbers and starting/ending times had switched to the
//       next subinterval.


















