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

#include <string>

const std::string ivy_version {"4.00.00"};

//using ivy_int = long long int; using ivy_float = long double;
  using ivy_int =      long int; using ivy_float =      double;

#include "pattern.h"
#include "dedupe_method.h"

#define SHOWLUNS_CMD std::string("showluns.sh")
#define IVYDRIVER_EXECUTABLE std::string("ivydriver")
#define IVY_CMDDEV_EXECUTABLE std::string("ivy_cmddev")
#define default_outputFolderRoot "."
#define SLAVEUSERID "root"
#define MAX_MAXTAGS 8192
#define MAX_DUPSETSIZE 1073741824LL
#define BUF_ALIGNMENT_BOUNDARY_SIZE 4096
#define MAX_IOS_LAUNCH_AT_ONCE 128
#define MAX_IOEVENTS_REAP_AT_ONCE 128
#define MAXWAITFORINITALPROMPT 120
#define PRECOMPUTE_HORIZON_SECONDS (0.5)
// When ivydriver runs on a test host, it temporarily writes its log files to the following
// folder location.  Then once ivydriver has exited, the ivy master thread copies the log files
// to the master host logs subfolder for the run, and deletes the temporary log files on the ivydriver host.
// But if something blows up and the ivydriver log files don't get moved to the master host,
// this is where you will find them on the ivydriver host:

//      IVYMAXMSGSIZE size of send/receive buffers between master and slave
#define IVYMAXMSGSIZE ((1024*1024)-sizeof(uint32_t))
#define MAXSAYATONCE (4095)
#define MAXWAITFORIVYCMDDEV 5

#define IO_TIME_LIMIT_SECONDS (30.0)

#define get_config_timeout_seconds (30)

// The idea below where defaults for [Go] parameters are all given as character strings
// was that this would also test parser / validation logic and would arrive at same value as if specified by the user.

#define wp_empty (0.015) /* below 1.5% WP is considered empty */

#define OK_LEAF_NAME_REGEX (R"([a-zA-Z0-9_@#$%()-_=+{}\[\];:<>.,~]+)")
#define path_separator (std::string("/"))

#define sector_size_bytes (512)

#define universal_seed 1234567

// go statement universal parameters
#define subinterval_seconds_default_int 5
#define subinterval_seconds_default std::string("5")
#define measure_seconds_default     std::string("60")
#define cooldown_by_wp_default      std::string("on")
#define subsystem_WP_threshold_default std::string("1.5%")
#define cooldown_by_MP_busy_default std::string("on")
#define subsystem_busy_threshold_default std::string("5%")

// measure = on parameters
#define accuracy_plus_minus_default std::string("5%")
#define confidence_default          std::string("95%")

#define plus_minus_series_confidence_default std::string("95%")

#define min_wp_default              std::string("0%")
#define max_wp_default              std::string("100%")
#define max_wp_change_default       std::string("100%")
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
const ivy_float max_subinterval_seconds {3600.0};

const int duplicate_set_size_default {128};
const int blocksize_bytes_default {8192};
const int dedupe_unit_bytes_default {8192};
const int maxTags_default{1};   // make sure someone is going to notice if they haven't set this.
const ivy_float IOPS_default {1};  // Default is 1.0 I/Os per second
const ivy_float skew_weight_default {-1.0};
const ivy_float fractionRead_default{1.0};
const ivy_float	rangeStart_default {0.0};  // default is start at sector 1.  Sector 0 is considered "out of bounds".
const ivy_float	rangeEnd_default {1.0};    // default is 1.0 maps to the last aligned block of that blocksize that fits.
const ivy_float	seqStartPoint_default{0.0}; // This defines where a sequential thread will start mapped from 0.0
                                                         // at the rangeStart point up to 1.0 at the rangeEnd point.
const pattern pattern_default {pattern::random};
const dedupe_method dedupe_method_default {dedupe_method::round_robin};
const ivy_float dedupe_default {1.0};
const ivy_float compressibility_default {0.0};

const unsigned int threads_in_workload_name_default {1};
const unsigned int this_thread_in_workload_default {0};
const uint64_t pattern_seed_default {1};

#define max_move_WP_error (.01)

#define io_time_buckets 65
// This is the number of buckets defined in Accumulators_by_io_type.cpp

extern std::string indent_increment;

#define unique_word_count (32*1024)

extern char* unique_words[];

#define non_random_sample_correction_factor_default (2.0) /* Applied to accuracy +/- to adjust for subintervals being consecutive rather than taking samples (subintervals) at random from a large populatyion */

#define ivy_ssh_timeout (180) /* ssh can sometimes take a long time to fire up due to DNS timeouts */

#define FIRST_FEW_CALLS 250

#define LOG_FIRST_FEW_IO_COMPLETIONS (0)
