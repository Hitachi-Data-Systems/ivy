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
#include <sys/types.h>
#include <mutex>
#include <condition_variable>


#include "ListOfWorkloadIDs.h"
#include "Subsystem.h"
#include "RunningStat.h"
#include "Subinterval_CPU.h"
#include "LUNpointerList.h"

// pipe_driver_subthread commands from the master thread
// - "die"

// commands for command device
//
// - "get config"
//      We say "get config" to ivy_cmddev
//      Config data comes in 4-tuples <element type> <element instance> <attribute name> <attribute value>
//      We do post-processing of received config data
//      to connect indirect attributes like making a list of pool vols for each pool.
//
// - "gather"
//      we wait until the scheduled time calculated by the master thread
//      We say "get CLPRdetail" to ivy_cmddev
//      Performance data also comes in 4-tuples <element type> <element instance> <attribute name> <attribute value>
//      We say "get MP_busy"    to ivy_cmddev
//      We say "get LDEVIO"     to ivy_cmddev
//      We say "get PORTIO"     to ivy_cmddev
//      We say "get UR_Jnl"     to ivy_cmddev
//      We do some post-processing of the performance data gather,
//      including transforming counter value deltas over successive gathers into rates.
//
//
// commands for ivydriver
//
// - "[CreateWorkload]"
//      We send to ivydriver and wait for it to say OK
//
// - "[EditWorkload]"
//      We send to ivydriver and wait for it to say OK
//
// - "[DeleteWorkload]"
//      We send to ivydriver and wait for it to say OK
//
// - "Go!<5,0>    - subinterval_seconds as seconds and nanoseconds.
// - "continue"
// - "cooldown"
// - "stop"
//      For each of these, we send to ivydriver and wait for it to say OK,
//      which means that the command has been delivered to all workload threads.
//      Then we measure the times from the beginning of the subinterval
//      that we received the OK.
//
// - "get subinterval result"
//      Then we send the command "get subinterval result", and wait for the CPU line
//      and then the detail lines for each workloadthread, and then
//      the sequential fill progress info.
//      Then when we have the CPU data, the detail lines, the seq fill data,
//      we get the master lock once and load all of the data, including
//      posting each workload thread's data into the apporopriate instance
//      of each rollup type in the RollupSet m_s.rollups.
//


class pipe_driver_subthread {
public:
// variables
	std::string
        ivyscript_hostname,
        outputfolderroot,
        outputfolderleaf,
		logfolder;
	std::string prompt, login;
	ivytime lasttime{0};  // this has the time of the previous utterance going in either direction on the piped connections

	RunningStat<ivy_float, ivy_int> perSubsystemGatherTime;

        // statistics of breakdown of the overall gather
	RunningStat<ivy_float, ivy_int> getConfigTime;
	RunningStat<ivy_float, ivy_int> getCLPRDetailTime;
	RunningStat<ivy_float, ivy_int> getMPbusyTime;
	RunningStat<ivy_float, ivy_int> getLDEVIOTime;
	RunningStat<ivy_float, ivy_int> getPORTIOTime;
	RunningStat<ivy_float, ivy_int> getUR_JnlTime;

	ivytime gather_scheduled_start_time;

    RunningStat<ivy_float,ivy_int> dispatching_latency_seconds_accumulator;
    RunningStat<ivy_float,ivy_int> lock_aquisition_latency_seconds_accumulator;
    RunningStat<ivy_float,ivy_int> switchover_completion_latency_seconds_accumulator;

    RunningStat<ivy_float,ivy_int> distribution_over_workloads_of_avg_dispatching_latency;
    RunningStat<ivy_float,ivy_int> distribution_over_workloads_of_avg_lock_acquisition;
    RunningStat<ivy_float,ivy_int> distribution_over_workloads_of_avg_switchover;

	std::string
		logfilename;

	int
		pipe_driver_subthread_to_slave_pipe[2],
		slave_to_pipe_driver_subthread_pipe[2],
		ssh_pid=-1;

        pid_t pipe_driver_pid=-1;	// same as ivymaster pid
        pid_t linux_gettid_thread_id;

	char
		to_slave_buf[sizeof(uint32_t) + IVYMAXMSGSIZE],
		from_slave_buf[sizeof(uint32_t) + IVYMAXMSGSIZE];
	int
		next_char_from_slave_buf=0,
		bytes_last_read_from_slave=0;

	bool last_read_from_pipe=true;
    bool read_from_pipe(ivytime timeout);
			// Used by get_line_from_pipe() when there are no more bytes left in "from_slave_buf" to serve up.
    std::string get_line_from_pipe // this is a wrapper around real_get_line_from_pipe to filter <Warning> and to catch <Error>
    (
        ivytime timeout,
        std::string description // Used when there is some sort of failure to construct an error message written to the log file.
    );

private:
    std::string real_get_line_from_pipe
    (
        ivytime timeout,
        std::string description // Used when there is some sort of failure to construct an error message written to the log file.
    );

public:
	std::string extra_from_last_time{""};  // Used by get_line_from_pipe() to store
	                                              // characters received from the ivydriver main thread that
	                                              // were past the end of the current line as marked by '\n'.
    std::string lun_csv_header;
	std::ostringstream lun_csv_body;

	struct avgcpubusypercent cpudata;
	int subinterval_seconds;
	bool command{false}, dead{false};
	bool commandComplete{false}, commandSuccess{false};
	std::string commandString;

	std::string commandHost, commandLUN, commandWorkloadID;
	std::string commandIosequencerName, commandIosequencerParameters;
	ListOfWorkloadIDs commandListOfWorkloadIDs;  // used{ to send out parameter updates for dynamic feedback control
    std::map<std::string /* edit workload command text */, ListOfWorkloadIDs>
    workloads_by_text;
        // This is used to save up what we are later going to send.
        // This minimizes the number of interlocks with ivydriver.
    ListOfWorkloadIDs* p_edit_workload_IDs {nullptr};
	ivytime commandStart, commandFinish;
	std::string commandErrorMessage;

	std::mutex master_slave_lk;
	std::condition_variable master_slave_cv;

	std::string hostname;
    std::list<std::string> lun_csv_lines;

	LUNpointerList thisHostLUNs;

	LUN HostSampleLUN;

	bool startupComplete{false}, startupSuccessful{false};

	std::string hostcsvline{"untouched\n"};  // you get one of these from the remote end, and you leave it here for the master task to harvest after you've passed on
	bool successful_completion{false};

	ivytime last_message_time { ivytime(0) };

// methods
	pipe_driver_subthread(std::string Hostname, std::string OutputFolderRoot, std::string TestName, std::string lf)
		: ivyscript_hostname(Hostname), outputfolderroot(OutputFolderRoot), outputfolderleaf(TestName), logfolder(lf) {}

	void threadRun();
	void orderSlaveToDie();
	void kill_ssh_and_harvest();
	void harvest_pid();
	void send_string(std::string s); // throws std::runtime_error   // breaks lines up if they are longer than 4095
	void real_send_string(std::string s); // throws std::runtime_error
	void send_and_get_OK(std::string s, const ivytime timeout);// throws std::runtime_error
	void send_and_get_OK(std::string s);// throws std::runtime_error
	std::string send_and_clip_response_upto(std::string s, std::string pattern, const ivytime timeout);// throws std::runtime_error




// ivy_cmddev variables:
	LUN* pCmdDevLUN {nullptr};  // Non-null value says to run ivy_cmddev vs. running ivydriver.
	Hitachi_RAID_subsystem* p_Hitachi_RAID_subsystem;

	std::string cmddev_command {""};
	std::string parse_line{""};
	std::string token;
	unsigned int parse_cursor {0};
	unsigned int line_number {0};
	bool
		token_valid_as_value{false},
		token_identifier{false},
		token_number{false},
		token_quoted_string {false};
	std::string element, instance, attribute;
	ivytime complete { ivytime(0) };
	ivytime duration; // most recent "gather" duration.
	ivytime time_in_hand_before_subinterval_end;
        // This is the amount of time from when we receive the OK from ivydriver that all workload threads have
        // been posted with the "Go!" / "continue" / "cooldown" / "stop" command
        // until the end of the subinterval.
    std::vector<long double> gather_times_seconds;

// ivy_cmddev methods:
	void get_token();  // throws std::invalid_argument, std::runtime_error
	void process_ivy_cmddev_response(GatherData& gd, ivytime start); // throws std::invalid_argument, std::runtime_error
    void pipe_driver_gather(std::unique_lock<std::mutex>&);
    void process_cmddev_commands(std::unique_lock<std::mutex>&);
    void process_ivydriver_commands(std::unique_lock<std::mutex>&);
};

