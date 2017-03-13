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

#include <iostream>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <map>
#include <unordered_map>
#include <vector>
#include <thread>
#include <climits>  // for UINT_MAX

#include "WorkloadTrackers.h"
#include "Subinterval_CPU.h"
#include "RollupSet.h"
#include "LUNpointerList.h"
#include "LDEVset.h"
#include "pipe_driver_subthread.h"
#include "IvyscriptLine.h"
#include "ParameterValueLookupTable.h"
#include "DynamicFeedbackController.h"
#include "MeasureDFC.h"
#include "Test_config_thumbnail.h"

enum class source_enum { error, workload, RAID_subsystem } ;
enum class category_enum { error, overall, read, write, random, sequential, random_read, random_write, sequential_read, sequential_write };
enum class accumulator_type_enum { error, bytes_transferred, service_time, response_time };
enum class accessor_enum { error, avg, count, min, max, sum, variance, standardDeviation };

std::string source_enum_to_string     (source_enum);
std::string category_enum_to_string   (category_enum);
std::string accumulator_type_to_string(accumulator_type_enum);
std::string accessor_enum_to_string   (accessor_enum);

accumulator_type_enum string_to_accumulator_type_enum (const std::string&);
std::string accumulator_types();


class master_stuff {
// One static copy of this exists in the ivymaster main thread, but it is made external
// and all code in the ivymaster executable gets access via linkage editor.
public:
	// There is one instance of this in the ivymaster main thread, but
	// all the subthreads are given access to it.

	// The mutex / condition variable are used to synchronize or interlock
	// so that only one host driver subthread at a time updates the overall CPU rollup data, for example.

	// Thus this mutex is only used when there's something to serialize between host threads.

	// Each host thread also has its own individual mutex for interlocking when it's just between the
	// ivymaster main thread and that particular host driver thread.

    bool overall_success {false};

    ivytime test_start_time;
    ivytime get_go;

    int running_subinterval {-1};

	std::mutex master_mutex;  // This is the overall one, for synchronizing access to "master_stuff".
	std::condition_variable master_cv;

	std::unordered_map<std::string /*ivyscript_hostname*/ ,pipe_driver_subthread*> host_subthread_pointers;

	WorkloadTrackers workloadTrackers;

	std::vector<std::thread> threads;

	std::vector<std::thread> ivymaster_RMLIB_threads;

	std::map<std::string, Subsystem*> subsystems;  // key is subsystem serial number.

	std::vector<Subinterval_CPU> cpu_by_subinterval;
		// At the beginning of a subinterval, the ivymaster main thread builds an empty Subinterval_CPU
		// object and adds it to the end of "cpu_by_subinterval" which is stored here so that both
		// the ivymaster main thread and the subthreads for each test host can access and modify it.

		// At "Go" time and at the end of a subinterval, ivyslave captures Linux host CPU busy cumulative counters.

		// At the end of every subinterval, ivyslave sends to ivymaster a package consisting
		// of a CPU busy line, lines for each workload showing WorkloadID, input parameter data,
		// and accumulated output data followed by an ending marker line.

		// On the ivymaster host, as each ivyslave_hostname_thread receives a CPU data line from their remote ivyslave host,
		// it gets the master lock and goes into master_stuff and adds the CPU busy data for that host into
		// the current Subinterval_CPU object put in cpu_by_subinterval by the ivymaster main thread at the top
		// of the subinterval.

		// Incidentally, the data are actually recorded and retrievable by host name in the Subinterval_CPU object.

		// This makes it possible to offer the feature where if the rollup is named "host", we only show the
		// CPU data for that one host instead of showing CPU data over all cores on all hosts.
	Subinterval_CPU measurement_rollup_CPU;
		// Built by RollupSet::makeMeasurementRollup()
		// Used for measurement_rollup csv file lines.

	std::list<std::string> hosts;
	void write_clear_script();  // this writes out "clear_hung_threads.sh"


	RollupSet rollups;  // has clear(), addWorkloadData(), addSubinterval()
	//bool need_to_rebuild_rollups {false}; // deleted 2015-12-21 to have data structures up to date for ivyscript builtin functions.
		// A [CreateWorkload] or [DeleteWorkload] statement turns this on.
		// A [Go] or an [EditRollup] will rebuild the rollups if this flag is on before they do anything else, and then they turn the flag off.

	RunningStat<ivy_float, ivy_int> createWorkloadExecutionTimeSeconds;
	RunningStat<ivy_float, ivy_int> deleteWorkloadExecutionTimeSeconds;
	RunningStat<ivy_float, ivy_int> editWorkloadExecutionTimeSeconds;

    IosequencerInput
        random_steady_template,
        random_independent_template,
        sequential_template;

	std::unordered_map<std::string, IosequencerInput*> iosequencer_templates
        {
            {"random_steady", &random_steady_template},
            {"random_independent", &random_independent_template},
            {"sequential", &sequential_template},
        };

	std::string outputFolderRoot {default_outputFolderRoot}; // actually the default is in ivy_pgm.h
	std::string masterlogfile {"./ivy_default_log_file.txt"};
	std::string ivy_engine_logfile {"./ivy_engine_API_calls.txt"};

	// Values of the following are updated where appropriate to communicate to csv file producing code on loop iterations
	std::string testName;
	std::string testFolder;

	std::string stepNNNN;
	std::string stepFolder;

	std::string measurementRollupFolder; // e.g. "Port+PG" subfolder of testFolder
		std::string measurement_rollup_IOPS_MBpS_service_response_blocksize_csv_filename;  // goes in measurementRollupFolder
		std::string measurement_rollup_by_test_step_csv_filename;  // goes in measurementRollupFolder
		std::string measurement_rollup_by_test_step_csv_type_filename;  // goes in measurementRollupFolder
		std::string measurement_rollup_data_validation_csv_filename;  // goes in measurementRollupFolder

	std::string stepRollupFolder;  // e.g. "step0000.Fluffy/Port+PG" subfolder of stepFolder
		std::string step_rollup_IOPS_MBpS_service_response_blocksize_filename;  // goes in stepDetailFolder
		std::string step_detail_by_subinterval_filename;  // goes in stepDetailFolder

	int lastEvaluateSubintervalReturnCode = -1; // EVALUATE_SUBINTERVAL_FAILURE
	int eventualEvaluateSubintervalReturnCode = -1; // delayed presenting until after cooldown, that is.

    ivytime subintervalStart, subintervalEnd, nextSubintervalEnd;

    ivy_float catnap_time_seconds {catnap_time_seconds_default};
    // The amount of time after the end of the subinterval that the ivyslave main thread waits
    // before starting to look to see if the workload threads posted their subintervals complete.

    ivy_float post_time_limit_seconds {post_time_limit_seconds_default};


// [Go!]
//    stepname = stepNNNN,
//    subinterval_seconds = 5,
//    warmup_seconds=5
//    measure_seconds=5
//    cooldown_by_wp = on

//    dfc = pid
//       p = 0
//       i = 0
//       d = 0
//       target_value=0
//       starting_total_IOPS=100
//       min_IOPS = 10   - this is so that if your pid loop is on service time, there will be a service time measurement.

//    measure = on
//       accuracy_plus_minus = "2%"
//       confidence = "95%"
//       max_wp = "100%"
//       min_wp = "0%"
//       max_wp_change = "3%"
//       timeout_seconds = "3600"

//    either dfc = pid and/or measure = on
//       focus_rollup = all
//       source = workload
//          category =  { overall, read, write, random, sequential, random_read, random_write, sequential_read, sequential_write }
//          accumulator_type = { bytes_transferred, service_time, response_time }
//          accessor = { avg, count, min, max, sum, variance, standardDeviation }
//       source = RAID_subsystem
//          subsystem_element = ""
//          element_metric = ""

    // these flags are set when parsing the [Go!] statement
    bool have_workload       {false};         // This is just to make the code a little easier to read by separating out a first step
    bool have_RAID_subsystem {false};         // where we screen for valid parameter name combinations and set default values if not specified.
    bool have_pid            {false};
    bool have_measure        {false};         // Then later we will use these flags to control which parameter names to parse.
    bool have_within         {false};
    bool have_max_above_or_below{false};

    ivy_float within {-1};
    unsigned int max_above_or_below;

    // universal
	std::string stepName;
	ivy_float subinterval_seconds {-1}; /* ==> */ ivytime subintervalLength {ivytime(subinterval_seconds_default_int)};
	ivy_float warmup_seconds {-1};      /* ==> */ int min_warmup_count /* i.e. subinterval count */;
	ivy_float measure_seconds {-1};     /* ==> */ int min_measure_count;
	bool cooldown_by_wp {true};  // whether the feature has been selected in the ivyscript program
	bool in_cooldown_mode {false}; // whether the ivy engine is in cooldown mode after SUCCESS or FAILURE
	ivytime cooldown_start;

    // measure=on and/or dfc=pid
    std::string focus_rollup_parameter; /* ==> */ RollupType* p_focus_rollup {nullptr};
    std::string source_parameter;       /* ==> */ source_enum source {source_enum::error};

    // source = workload
    std::string category_parameter;         /* ==> */ category_enum         category         {category_enum::error};
    std::string accumulator_type_parameter; /* ==> */ accumulator_type_enum accumulator_type {accumulator_type_enum::error};
    std::string accessor_parameter;         /* ==> */ accessor_enum         accessor         {accessor_enum::error};

    // source = RAID_subsystem
    std::string subsystem_element; // e.g. PG
    std::string element_metric;    // e.g. busy_percent

    // dfc = pid
    std::string target_value_parameter; /* ==> */ ivy_float target_value {-1.};
        // number with optional trailing % sign.
    std::string p_parameter; /* ==> */ ivy_float p_multiplier {-1.};
    std::string i_parameter; /* ==> */ ivy_float i_multiplier {-1.};
    std::string d_parameter; /* ==> */ ivy_float d_multiplier {-1.};
    std::string starting_total_IOPS_parameter; /* ==> */ ivy_float starting_total_IOPS {-1.};
    std::string min_IOPS_parameter;            /* ==> */ ivy_float min_IOPS {-1.};

    MeasureDFC the_dfc;

    // measure = on
    //? ivy_float relative_plus_minus_target;  // is set before first reference
	std::string confidence_parameter;          /* ==> */ ivy_float confidence;
    std::string accuracy_plus_minus_parameter; /* ==> */ ivy_float accuracy_plus_minus_fraction;
    std::string min_wp_parameter;	           /* ==> */ ivy_float min_wp{0.0};
    std::string max_wp_parameter;              /* ==> */ ivy_float max_wp{1.0};
    std::string max_wp_change_parameter;        /* ==> */ ivy_float max_wp_change;
	std::string timeout_seconds_parameter;     /* ==> */ ivy_float timeout_seconds;

    std::string focus_caption();

	ivy_float focus_metric_value;
        // At the end of a subinterval, if the focus metric is going to be used (if dfc=pid or measure=on)
        // The approprate metric value is loaded into focus_metric_value,
        // according to the source=workload or source=RAID_subsystem parameters

	LUNpointerList allDiscoveredLUNs, availableTestLUNs, commandDeviceLUNs;
	LUN TheSampleLUN;

	bool haveCmdDev {false};
	RunningStat<ivy_float, ivy_int> overallGatherTimeSeconds;

	std::set<std::pair<std::string /*CLPR*/, Subsystem*>> cooldown_WP_watch_set;
		// the cooldown_set is those < CLPR, serial_number > pairs in the available workload LUNs
		// for which we have a command device.  This list is used at least by the MeasureDFC DFC
		// to know when it should send "IOPS=0" to all workloads and then wait for WP to empty on all cooldownlist CLPRs before
		// issuing it's final success / failure code.
	int measureDFC_failure_point;

#define fetch_metric           (0x80)

#define print_count_part_1     (0x40)
#define print_avg_part_1       (0x20)
#define print_min_max_stddev_1 (0x10)

#define print_count_part_2     (0x04)
#define print_avg_part_2       (0x02)
#define print_min_max_stddev_2 (0x01)

	std::vector< std::pair< std::string, std::vector< std::pair <std::string, unsigned char> > > > subsystem_summary_metrics
    // This is what drives creation of subsystem data rollups by RollupInstance, for DFCs to use and to drive csv file columns
    // in the by-RollupInstance csv files, both by-subinterval and measurement rollup.

    // If a metric does not have the "fetch_metric", it is derived during post processing, and doesn't have a "count" or "min_max_stddev" to print.
    // Thus for derived metrics, only "print_avg" is available.

    // Section 1 is near the beginning of the csv line, section 2 is at the end.

    {
        { "MP_core", {
              { "busy_percent",                     fetch_metric + print_count_part_1 + print_avg_part_1 }
            , { "io_buffers",                       fetch_metric +                      print_avg_part_2 + print_min_max_stddev_2 }
         } }
        ,{ "CLPR", {
              { "WP_percent",                       fetch_metric + print_count_part_1 + print_avg_part_1}
        } }
        ,{ "PG", {
              {"busy_percent",                      fetch_metric + print_count_part_1 + print_avg_part_1 }
            , {"random_read_busy_percent",          fetch_metric                      + print_avg_part_2 }
            , {"random_write_busy_percent",         fetch_metric                      + print_avg_part_2 }
            , {"seq_read_busy_percent",             fetch_metric                      + print_avg_part_2 }
            , {"seq_write_busy_percent",            fetch_metric                      + print_avg_part_2 }
         } }
        ,{ "LDEV", {
              { "random_read_IOPS",                 fetch_metric + print_count_part_1 + print_avg_part_2 }
 //           , { "random_read_hit_percent",          fetch_metric + print_count_part_1 + print_avg_part_2 }
            , { "random_write_IOPS",                fetch_metric                      + print_avg_part_2 }
            , { "random_read_bytes_per_second",     fetch_metric                                         }
            , { "random_write_bytes_per_second",    fetch_metric                                         }
            , { "random_read_decimal_MB_per_second",                                    print_avg_part_2 }
            , { "random_write_decimal_MB_per_second",                                   print_avg_part_2 }
            , { "random_read_blocksize_KiB",                                            print_avg_part_2 }
            , { "random_write_blocksize_KiB",                                           print_avg_part_2 }
            , { "random_blocksize_KiB",                                                 print_avg_part_2 }

            , { "sequential_read_IOPS",             fetch_metric                      + print_avg_part_2 }
            , { "sequential_write_IOPS",            fetch_metric                      + print_avg_part_2 }
            , { "sequential_read_bytes_per_second", fetch_metric                                         }
            , { "sequential_write_bytes_per_second",fetch_metric                                         }
            , { "sequential_read_decimal_MB_per_second",                                print_avg_part_2 }
            , { "sequential_write_decimal_MB_per_second",                               print_avg_part_2 }
            , { "sequential_read_blocksize_KiB",                                        print_avg_part_2 }
            , { "sequential_write_blocksize_KiB",                                       print_avg_part_2 }
            , { "sequential_blocksize_KiB",                                             print_avg_part_2 }

            , { "read_IOPS_x_response_time_ms",     fetch_metric                      + print_avg_part_2 }
            , { "write_IOPS_x_response_time_ms",    fetch_metric                      + print_avg_part_2 }
            , { "read_service_time_ms",                                                 print_avg_part_2 }
            , { "write_service_time_ms",                                                print_avg_part_2 }
        } }
    };

    Test_config_thumbnail available_LUNs_thumbnail;

    std::ifstream ivyscriptIfstream;
    std::string statementType;
    int ivyscript_line_number{0};
    IvyscriptLine ivyscriptLine;
    bool have_parsed_OutputFolderRoot {false};
    bool haveOutputFolderRoot {false};
    bool haveHosts {false};

    std::string ivyscript_line{}, trimmed_ivyscript_line{};
    bool allThreadsSentTheLUNsTheySee {false};
    int goStatementsSeen {0};
    std::string goParameters;
    ParameterValueLookupTable go_parameters;
    std::map<std::string, DynamicFeedbackController*> availableControllers;

    std::ostringstream step_times;

    ivy_float non_random_sample_correction_factor {non_random_sample_correction_factor_default /*in ivydefines.h */};

//    bool have_best_of_wurst {false};  That would be leverworst, natuurlijk.

    unsigned int best_of_wurst_first, best_of_wurst_last;
    bool have_timeout_rollup {false};

// methods
	void kill_subthreads_and_exit();
	void error(std::string);
	std::pair<bool, std::string> make_measurement_rollup_CPU(unsigned int firstMeasurementIndex, unsigned int lastMeasurementIndex);
	std::string getWPthumbnail(int subinterval_index); // throws std::invalid_argument.    Shows WP for each CLPR on the watch list as briefly as possible.

    ivy_float get_rollup_metric
    (
        const std::string&, // rollup_type_parameter
        const std::string&, // rollup_instance_parameter
        unsigned int,       // subinterval_number
        const std::string&, // accumulator_type_parameter
        const std::string&, // category_parameter
        const std::string&  // metric_parameter
    );

    bool some_cooldown_WP_not_empty();

    std::string focus_metric_ID();

    // ivy engine API methods

        std::pair<bool /*true = success*/, std::string /* message */>
    startup(
        const std::string& output_folder_root,  //    /home/ivogelesang/Desktop
        const std::string& test_name,           //    testy
            // ivyscript uses as the test name the ivyscript filename, with any path part removed, and with the .ivyscript suffix removed
            // in other words, if you ran ../redlof/testy.ivyscript, the ivyscript interpreter would set the test name to testy.

        const std::string& ivyscript_filename,  // optional - nonblank gives name of a file to be copied to the test folder
                                                //    ../redlof/testy.ivyscript
        const std::string& test_hosts,          //    horde16-31, 192.168.0.0, torque
        const std::string& select);             //    { "port" : [ "1A", "2A" ], "LDEV" : "00:00-00:FF" }

        std::pair<bool /*true = success*/, std::string /* message */>
    set_iosequencer_template(
        const std::string& template_name,       //    random_steady, random_independent, sequential
        const std::string& parameters);         //    IOPS = 100, blocksize = 4 KiB


        std::pair<bool /*true = success*/, std::string /* message */>
    createWorkload(
        const std::string& workload_name,
        const std::string& select,              //    { "PG" : [ "1-*", "2-1:8" ], "LDEV" : "00:00-00:FF" }
        const std::string& iosequencer,         //    random_steady, random_independent, sequential
        const std::string& parameters);

        std::pair<bool /*true = success*/, std::string /* message */>
    deleteWorkload(
        const std::string& workload_name,
        const std::string& select_string);

        std::pair<bool /*true = success*/, std::string /* message */>
    create_rollup(
        std::string rollup_name
        , bool dont_make_csv_files  /* true means don't make csv files for this rollup */
        , bool have_quantity_validation
        , bool have_max_IOPS_droop_validation
        , ivy_int quantity
        , ivy_float max_droop);

        std::pair<bool /*true = success*/, std::string /* message */>
    edit_rollup(
        const std::string& rollup_name,        //   serial_number+Port = { 410321+1A, 410321+2A }
            // all=all gets every workload thread
        const std::string& parameters);

        std::pair<bool /*true = success*/, std::string /* message */>
    delete_rollup(
        const std::string& rollup_name);       //   serial_number+port


        std::pair<bool /*true = success*/, std::string /* message */>
    go(
        const std::string& parameters);        //   measure_seconds = 300

        std::pair<bool,std::string>
    shutdown_subthreads();

    // accessor API methods to help scripting

    // This next group of methods is for ivy engine API users to easily locate ivy output csv files

    std::string output_folder_root(){return outputFolderRoot;}
        // This is specified on the ivy engine API "startup" call.
        // The place where ivy will make a subfolder for this test run.

    std::string test_name()         {return testName;}
        // This is specified on the ivy engine API "startup" call.
        // An identifier (name) for this test run.
        // - used as the subfolder name from the output folder root
        // - used as part of csv file names, so that even if you dump all the csv files into one folder, they won't collide.
        // ivyscript sets the test name to be what the user specified as in input filename, but with any path part removed.

    std::string test_folder()       {return testFolder;}
        // output_folder_root / test_name

    // These next ones are updated after every "test step", or "go" call to run a subinterval sequence
    std::string last_result();    // look for "success"
    std::string step_NNNN()         {return stepNNNN;}
    std::string step_name()         {return stepName;}
    std::string step_folder()       {return stepFolder;}

    // ivy comes with a handy general purpose "treat a csv file as a spreadsheet" class.
    // The idea is you build the csv file name using the above, then you use the csvfile class
    // to easily pick out the value for row (test step number) 2, column "IOPS".

// ==== start of clip from csvfile.h
//        int rows();
//            // returns the number of rows following the header row.
//            // returns 0 if there is only a header row.
//            // returns -1 if file loaded didn't exist or was empty.
//
//        int header_columns();
//            // returns -1 if file loaded didn't exist or was empty.
//
//        int columns_in_row(int row /* from -1 */);
//            // row number -1 means header row
//            // returns -1 for an invalid row
//
//        std::string column_header(int column /* from 0*/ );
//
//        std::string raw_cell_value(int /* row from -1 */, int                 /* column number from 0 */ );
//        std::string raw_cell_value(int /* row from -1 */, const std::string&  /* column header */        );
//            // returns the exact characters found between commas in the source file
//            // returns the empty string on invalid row or column
//
//        std::string cell_value(int /* row from -1 */, int                /* column number from 0 */ );
//        std::string cell_value(int /* row from -1 */, const std::string& /* column header */ );
//            // This "unwraps" the raw cell value.
//
//            // First if it starts with =" and ends with ", we just clip those off and return the rest.
//
//            // Otherwise, an unwrapped CSV column value first has leading/trailing whitespace removed and then if what remains is a single quoted string,
//            // the quotes are removed and any internal "escaped" quotes have their escaping backslashes removed, i.e. \" -> ".
//
//        std::string row(int); // -1 gives you the header row
//            // e.g. nsome,56.2,="bork"
//
//        std::string column(int                /* column number */);
//        std::string column(const std::string& /* column header */);
//            // This returns a column slice, e.g. "IOPS,56,67,88"
//
//        int lookup_column(const std::string& /* header name */);
// ==== start of clip from csvfile.h

    // Misc.

    std::string logfile()           {return masterlogfile;}   // if API user would like to append to ivy command line master thread log file

    std::string show_rollup_structure();   // available for diagnostic use
};

extern master_stuff m_s;


