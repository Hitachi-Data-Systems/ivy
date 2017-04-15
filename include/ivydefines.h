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

#include <array>
#include <string>

//using ivy_int = long long int; using ivy_float = long double;
  using ivy_int =      long int; using ivy_float =      double;

#include "pattern.h"

#define SHOWLUNS_CMD "showluns.sh"
#define IVYSLAVE_EXECUTABLE "ivyslave"
#define IVY_CMDDEV_EXECUTABLE "ivy_cmddev"
#define default_outputFolderRoot "."
#define SLAVEUSERID "root"
#define MINBUFSIZE 4096
#define MAX_MAXTAGS 8192
#define BUF_ALIGNMENT_BOUNDARY_SIZE 4096
#define MAX_IOS_LAUNCH_AT_ONCE 8
#define MAX_IOEVENTS_REAP_AT_ONCE 16
#define MAXWAITFORINITALPROMPT 120

// When ivyslave runs on a test host, it temporarily writes its log files to the following
// folder location.  Then once ivyslave has exited, the ivy master thread copies the log files
// to the master host logs subfolder for the run, and deletes the temporary log files on the ivyslave host.
// But if something blows up and the ivyslave log files don't get moved to the master host,
// this is where you will find them on the ivyslave host:

#define IVYSLAVELOGFOLDERROOT "/var"
// This root folder must already exist when ivyslave fires up.

#define IVYSLAVELOGFOLDER "/ivyslave_logs"
// This subfolder of the ivyslave log root folder will be created if it doesn't already exist

// Then any log files called "ivyslave.hostname.log.*" are deleted if they already exists in this folder.
// The hostname here is what ivymaster told us our hostname was.
// This lets us create two ivyslave instances like "192.168.1.1" and "barney" whose log files won't step on each other.
// After the ivyslave main thread logs to ivyslave.hostname.log.txt, and the subthreads log to ivyslave.hostname.log.threadID_with_underscores.txt.
// When the test is complete, ivymaster uses an scp command to copy the log files back to the ivymaster host along with everything else.

//      IVYMAXMSGSIZE size of send/receive buffers between master and slave
#define IVYMAXMSGSIZE (32*1024)

#define MAXWAITFORIVYCMDDEV 5

#define IO_TIME_LIMIT_SECONDS (30.0)

#define get_config_timeout_seconds (30)

// The idea below where defaults for [Go] parameters are all given as character strings
// was that this would also test parser / validation logic and would arrive at same value as if specified by the user.

#define wp_empty (0.015) /* below 1.5% WP is considered empty */

#define OK_LEAF_NAME_REGEX (R"([a-zA-Z0-9_@#$%()-_=+{}\[\];:<>.,~]+)")
#define path_separator (std::string("/"))

#define sector_size_bytes (512)

#define catnap_time_seconds_default (1.0)
    // The amount of time after the end of the subinterval that the ivyslave main thread waits
    // before looking to see if the workload threads posted their subintervals complete.
#define post_time_limit_seconds_default (2.5)

// go statement universal parameters
#define subinterval_seconds_default_int 5
#define subinterval_seconds_default std::string("5")
#define warmup_seconds_default     std::string("5")
#define measure_seconds_default    std::string("60")
#define cooldown_by_wp_default     std::string("on")

// measure = on parameters
#define accuracy_plus_minus_default std::string("5%")
#define confidence_default          std::string("95%")

#define plus_minus_series_confidence_default std::string("95%")

#define min_wp_default              std::string("0%")
#define max_wp_default              std::string("100%")
#define max_wp_change_default        std::string("5%")
#define timeout_seconds_default     std::string("900")

// dfc=pid or measure=on
#define focus_rollup_default  std::string("all")
#define source_default        std::string("workload")

// source = workload parameters
#define category_default          std::string("overall")
#define accumulator_type_default  std::string("service_time")
#define accessor_default          std::string("avg")

// source = RAID_subsystem
#define subsystem_element_default std::string("")
#define element_metric_default    std::string("")

const ivy_float min_subinterval_seconds {3.0};
const ivy_float max_subinterval_seconds {60.0};

const int blocksize_bytes_default {4096};
const int maxTags_default{1};   // make sure someone is going to notice if they haven't set this.
const ivy_float IOPS_default {1};  // Default is 1.0 I/Os per second
const ivy_float fractionRead_default{1.0};
const ivy_float	volCoverageFractionStart_default {0.0};  // default is start at sector 1.  Sector 0 is considered "out of bounds".
const ivy_float	volCoverageFractionEnd_default {1.0};    // default is 1.0 maps to the last aligned block of that blocksize that fits.
const ivy_float	seqStartFractionOfCoverage_default{0.0}; // This defines where a sequential thread will start mapped from 0.0
                                                         // at the volCoverageFractionStart point up to 1.0 at the volCoverageFractionEnd point.
const pattern pattern_default {pattern::random};
const ivy_float dedupe_default {1.0};
const ivy_float compressibility_default {0.0};

const unsigned int threads_in_workload_name_default {1};
const unsigned int this_thread_in_workload_default {0};
const uint64_t pattern_seed_default {1};

#define max_move_WP_error (.01)

#define io_time_buckets 56
// This is the number of buckets defined in Accumulators_by_io_type.cpp

#define gather_lead_time_safety_margin (0.5)
// A safety margin of 0.5 means add 50% onto the average time it has taken to perform a gather so far, and start each gather at that padded lead time before the end of the subinterval.

extern std::string indent_increment;

#define unique_word_count (32*1024)

extern char* unique_words[];

#define non_random_sample_correction_factor_default (2.0) /* Applied to accuracy +/- to adjust for subintervals being consecutive rather than taking samples (subintervals) at random from a large populatyion */

#define ivy_ssh_timeout (180) /* ssh can sometimes take a long time to fire up due to DNS timeouts */
