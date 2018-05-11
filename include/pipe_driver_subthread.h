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
#include <sys/types.h>

#include "ListOfWorkloadIDs.h"
#include "Subsystem.h"

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

	RunningStat<ivy_float, ivy_int> sendupTime;
	ivytime gather_scheduled_start_time;

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
        std::string description, // Used when there is some sort of failure to construct an error message written to the log file.
        bool reading_echo_line = false
    );

private:
    std::string real_get_line_from_pipe
    (
        ivytime timeout,
        std::string description, // Used when there is some sort of failure to construct an error message written to the log file.
        bool reading_echo_line
    );
    bool pipe_driver_subthread_has_lock;
public:

	std::string extra_from_last_time{""};  // Used by get_line_from_pipe() to store
	                                              // characters received from the ivyslave main thread that
	                                              // were past the end of the current line as marked by '\n'.
    std::string lun_csv_header;
	std::ostringstream lun_csv_body;

	struct avgcpubusypercent cpudata;
	int subinterval_seconds;
	bool command{false}, dead{false};
	bool commandComplete{false}, commandSuccess{false};
	bool commandAcknowledged{false};
	std::string commandString;

	std::string commandHost, commandLUN, commandWorkloadID;
	std::string commandIosequencerName, commandIosequencerParameters;
	ListOfWorkloadIDs commandListOfWorkloadIDs;  // used to send out parameter updates for dynamic feedback control

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

// methods
	pipe_driver_subthread(std::string Hostname, std::string OutputFolderRoot, std::string TestName, std::string lf)
		: ivyscript_hostname(Hostname), outputfolderroot(OutputFolderRoot), outputfolderleaf(TestName), logfolder(lf) {}

	void threadRun();
	void orderSlaveToDie();
	void kill_ssh_and_harvest();
	void harvest_pid();
	void send_string(std::string s); // throws std::runtime_error
	void send_and_get_OK(std::string s, const ivytime timeout);// throws std::runtime_error
	void send_and_get_OK(std::string s);// throws std::runtime_error
	std::string send_and_clip_response_upto(std::string s, std::string pattern, const ivytime timeout);// throws std::runtime_error


// ivy_cmddev variables:
	LUN* pCmdDevLUN {nullptr};  // Non-null value says to run ivy_cmddev vs. running ivyslave.
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
	ivytime duration;
	ivytime time_in_hand_before_subinterval_end;
        // This is the amount of time from when we receive the OK from ivyslave that all workload threads have
        // been posted with the "Go!" / "continue" / "cooldown" / "stop" command
        // until the end of the subinterval.

// ivy_cmddev methods:
	void get_token();  // throws std::invalid_argument, std::runtime_error
	void process_ivy_cmddev_response(GatherData& gd, ivytime start); // throws std::invalid_argument, std::runtime_error

};

