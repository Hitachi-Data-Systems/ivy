//Copyright (c) 2019 Hitachi Vantara Corporation
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

#include <mutex>

#include "ivyhelpers.h"

struct logger
{
// variables
    std::mutex now_speaks;
    RunningStat<long double, long> durations;
    ivytime last_time {0};
    std::string logfilename {"<none>"};

// methods
    logger() {};
    logger(const std::string& s) : logfilename(s) {};
};

void log(logger&, const std::string&);

// It's done this way as this was a drop-in replacement for
// "void log(std::string filename, std::string msg);" in ivyhelpers.h
// and that way simply by changing the type of the log file from
// std::string to logger, all the existing calls to log don't need editing.

// ivy_engine.h: 	        std::string masterlogfile      { "<none>"s };
// pipe_driver_subthread.h: std::string logfilename;
// ivydriver.h              std::string slavelogfile       { "/var/ivydriver_initial_log.txt" };
// WorkloadThread.h         std::string slavethreadlogfile { };

// See "print_logfile_stats()" in both ivy_engine.h and ivydriver.h.
