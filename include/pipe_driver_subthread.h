//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

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
	std::string

		logfilename;

	int
		pipe_driver_subthread_to_slave_pipe[2],
		slave_to_pipe_driver_subthread_pipe[2],
		ssh_pid=-1,
		pipe_driver_pid=-1;

	char
		to_slave_buf[sizeof(uint32_t) + IVYMAXMSGSIZE],
		from_slave_buf[sizeof(uint32_t) + IVYMAXMSGSIZE];
	int
		next_char_from_slave_buf=0,
		bytes_last_read_from_slave=0;
	bool
		last_read_from_pipe=true,
		read_from_pipe(ivytime timeout),
			// Used by get_line_from_pipe() when there are no more bytes left in "from_slave_buf" to serve up.
		get_line_from_pipe // this is a wrapper around real_get_line_from_pipe to filter <Warning> and to catch <Error>
		(
			ivytime timeout,
			std::string description, // Used when there is some sort of failure to construct an error message written to the log file.
			std::string& line_from_other_end_with_terminating_newline_removed,
			bool log_it=true
		);

private:bool	real_get_line_from_pipe
		(
			ivytime timeout,
			std::string description, // Used when there is some sort of failure to construct an error message written to the log file.
			std::string& line_from_other_end_with_terminating_newline_removed,
			bool log_it=true
		);
public:

	std::string extra_from_last_time{""};  // Used by get_line_from_pipe() to store
	                                              // characters received from the ivyslave main thread that
	                                              // were past the end of the current line as marked by '\n'.
    std::ostringstream lun_csv_header;
	std::ostringstream lun_csv_body;

	struct avgcpubusypercent cpudata;
	int subinterval_seconds;
	bool command{false}, dead{false};
	bool commandComplete{false}, commandSuccess{false};
	std::string commandString;

	std::string commandHost, commandLUN, commandWorkloadID;
	std::string commandIogeneratorName, commandIogeneratorParameters;
	ListOfWorkloadIDs commandListOfWorkloadIDs;  // used to send out parameter updates for dynamic feedback control

	ivytime commandStart, commandFinish;
	std::string commandErrorMessage;

	std::mutex master_slave_lk;
	std::condition_variable master_slave_cv;
	std::string hostname;
	std::string LUNheader;
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
	bool send_string(std::string s);
	bool send_and_get_OK(std::string s);
	bool send_and_clip_response_upto(std::string s, std::string pattern, std::string& response, int timeout_seconds);


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

// ivy_cmddev methods:
	void get_token();  // throws std::invalid_argument, std::runtime_error
	void process_ivy_cmddev_response(GatherData& gd, ivytime start); // throws std::invalid_argument, std::runtime_error

};

