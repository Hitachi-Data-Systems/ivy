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
#include <set>

#include "subsystem_summary_data.h"

#include "SubintervalRollup.h"
#include "SequenceOfSubintervalRollup.h"
#include "ListOfWorkloadIDs.h"
#include "Test_config_thumbnail.h"

class ivy_engine;
class RollupSet;
class RollupType;


enum class SuccessFailAbort { success, fail, abort };


class RollupInstance   // 410123+3B
// This is the granularity of Dynamic Feedback Control both for looking at data and
// as well as for sending out commands to workloads.
{
public:
// variables
	std::string attributeNameComboID;
	std::string rollupInstanceID;
	SubintervalRollup measurementRollup;
	SequenceOfSubintervalRollup subintervals;
	RollupType* pRollupType; // my parent
	RollupSet* pRollupSet; // my grandparent
	ListOfWorkloadIDs workloadIDs; // to send out Dynamic Feedback Control messages
		// Every time you post a detail line into one of these, a copy of the workload ID is put in here.

    std::map
    <
        std::string, // subsystem serial number
        std::map
        <
            std::string,  // attribute, e.g. "port'
                // Both SCSI Inquiry attributes, e.g. "port",
                // and where present, LDEV attributes from a command device, e.g. "drive_type", or "MPU".

                // Keys not altered from the form seen in LUN attributes. (not translated to lower case, etc.)

            std::set< std::string >
                // for each observed key (attribute name), the set of observed values under that key is stored.
                // e.g. { "1A", "2A" }
        >
    >
    config_filter; // populated by RollupType::rebuild()

        // Over all WorkloadIDs in this rollup instance, we keep for each subsystem, for each LUN/LDEV
        // configuration attribute, such as "port", the set of observed values, such as { "1A", "2A" }
        // in this RollupInstance.

        // This is what enables us when iterating through instances of subsystem performance data for a
        // particular type of configuration element to check if a particular instance (such as "port" = "1A")
        // is represented in this RollupInstance, and therefore to include the data for this element
        // in the fixed-column layout subsystem performance data summary csv column set for this rollup instance.

        // There is a set of performance data summary columns that are populated in a RollupInstance data structure
        // at the time rollups are done at the end of a subinterval, and made available to the DFC when it is called.

        // The definition of the subsystem performance data summary columns is "subsystem_summary_metrics"
        // in ivy_engine.h.


        // Special processing is performed for the "all=all" rollup instance.

        // Here we we make a "not_participating" filter produced by spinning through the instances of each type of
        // subsystem configuration data and if the attribute=value pair is not in all=all's config_filter,
        // we include it in the not_participating_filter.

        // We then make a "not_participating" performance data summary by filtering using the "not_participating" filter.

        // The "not_participating" csv columns appear at near the end of " measurement" and "test step by-subinterval" csv lines.

        // The subsystem performance csv column set is included as a column range in all RollupInstance csv files.

        // The "not_participating.csv" file only has the subsystem performance summary columns.

    std::vector<subsystem_summary_data> subsystem_data_by_subinterval;
    subsystem_summary_data measurement_subsystem_data;
    Test_config_thumbnail test_config_thumbnail;

    std::vector<ivy_float> focus_metric_vector;  // used only for the focus rollup type.

    std::string timestamp; // set by RollupInstance::reset() - gets printed in csv files.

    // PID variables
    ivy_float error_signal         {0.0};
    ivy_float error_derivative     {0.0};
    ivy_float error_integral       {0.0};

    ivy_float this_measurement {0.};
    ivy_float prev_measurement {0.};
    ivy_float total_IOPS {0.}, total_IOPS_last_time;
    ivy_float p_times_error, i_times_integral, d_times_derivative;

    std::ostringstream
            subinterval_number_sstream,
            target_metric_value_sstream,
            metric_value_sstream,
            error_sstream,
            error_integral_sstream,
            error_derivative_sstream,
            p_times_error_sstream,
            i_times_integral_sstream,
            d_times_derivative_sstream,
            total_IOPS_sstream,
            p_sstream, i_sstream, d_sstream;

    ivy_float plusminusfraction_percent_above_below_target;
        // set by isValidMeasurementStartingFrom()
   	ivy_float best;

   	unsigned int best_first, best_last;
   	bool best_first_last_valid {false};



   	ivy_float P {0.}, I {0.}, D {0.};
   	bool on_way_down {false};
   	bool on_way_up {false};
   	unsigned int adaptive_PID_subinterval_count {0};
   	bool have_previous_inflection {false};
   	ivy_float previous_inflection {-1.};
   	bool have_reduced_gain {false};
   	unsigned int monotone_count {0};

   	RunningStat<ivy_float,ivy_int> up_movement {};
   	RunningStat<ivy_float,ivy_int> down_movement {};
   	RunningStat<ivy_float,ivy_int> fractional_up_swings {};
   	RunningStat<ivy_float,ivy_int> fractional_down_swings {};

    unsigned int m_count {0}; // monotone triggered gain increases
    unsigned int b_count {0}; // 2/3 in one direction triggered gain increases
    unsigned int d_count {0}; // decreases

    std::ostringstream gain_history {};

    ivy_float per_instance_low_IOPS;
    ivy_float per_instance_high_IOPS;
    ivy_float per_instance_max_IOPS;

    std::string by_subinterval_csv_filename {};  // set by set_by_subinterval_filename()
    std::string csv_filename_prefix {};          // set by set_by_subinterval_filename()

    std::string measurement_rollup_by_test_step_csv_filename;      // goes in measurementRollupFolder
    std::string measurement_rollup_by_test_step_csv_type_filename; // goes in measurementRollupFolder



// methods
	RollupInstance(RollupType* pRT, RollupSet* pRS, std::string nameCombo, std::string valueCombo)
		: attributeNameComboID(nameCombo), rollupInstanceID(valueCombo), pRollupType(pRT), pRollupSet(pRS)
		{}

//	void postWorkloadSubintervalResult (int subinterval_number, std::string workloadID, IosequencerInput& detailInput, SubintervalOutput& detailOutput);
	std::pair<bool,std::string> add_workload_detail_line(WorkloadID& wID, IosequencerInput& iI, SubintervalOutput& sO);

	bool sendDynamicFeedbackControlParameterUpdate(std::string updates);


	std::pair<bool,std::string> makeMeasurementRollup(unsigned int firstMeasurementIndex, unsigned int lastMeasurementIndex);

	std::string getIDprefixTitles() {return "Test Name,Step Number,Step Name,Start Time,End Time,Subinterval Number,Subinterval Phase,Rollup Type,Rollup Instance,";}
	std::string getIDprefixValues(std::string phase, ivytime start_time, ivytime end_time);
	void printMe(std::ostream&);
    std::string config_filter_contents();
    void rebuild_test_config_thumbnail();
    ivy_float get_focus_metric(unsigned int /* subinterval_number */);

    // PID & measure=on methods:

    void collect_and_push_focus_metric();  // must be called exactly once after each subinterval, if focus metric being used.
    void reset(); // must be called once before starting a subinterval sequence, if the focus metric is being used.
    void perform_PID();
//    unsigned int most_subintervals_without_a_zero_crossing_starting(unsigned int);
    void print_console_info_line();
    void print_subinterval_column();
    void print_common_columns(std::ostream&);
    void print_common_headers(std::ostream&);
    void print_pid_csv_files();

    std::pair<bool,ivy_float> isValidMeasurementStartingFrom(unsigned int n);

    void set_by_subinterval_filename();
    void print_by_subinterval_header();
    void print_config_thumbnail();  // gets called by RollupType's make_step_folder()
    void print_subinterval_csv_line(unsigned int subinterval_number /* from zero */, bool is_provisional);
        // The first or "provisional" time we print this csv file, the lines are printed one by one as each subinterval ends,
        // but we don't know yet if the subinterval is "warmup", "measurement", or "cooldown", so this column is left blank.
        // This way, if ivy is terminated with a control-C, or if it explodes, we can at least see what had happened up to
        // the unexpected end, but of course we won't see a summary csv file line for that test step.

        // Then later, when we make the measurement rollup and print the summary csv line,
        // we re-print the by-subinterval csv files for each rollup, but this time showing "warmup", "measurment", or "cooldown",
        // as this is very handy when looking at the by-subinterval csv files, to see exactly the behaviour in each phase, and
        // to know which subinterval lines were rolled up in the measurement.

    void print_measurement_summary_csv_line();
};

