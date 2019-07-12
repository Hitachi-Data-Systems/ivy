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
#pragma once

class AttributeNameCombo;
class RollupInstance;
class SingleSubintervalRollup;
class ivytime;

#include "RollupType.h"
#include "subsystem_summary_data.h"

class RollupSet   // one of these for each test run
{
private:
//variables
	std::string my_error_message;
	bool initialized = false;
	int subintervalIndex {-1};

//methods

public:
//variables
	std::map<std::string /*attributeNameComboID*/, RollupType*> rollups;

	typedef  std::pair<ivytime /*start_time*/, ivytime /*end_time*/> ivytime_pair;

	std::vector<ivytime_pair> starting_ending_times;
	//std::pair<ivytime /*start_time*/, ivytime /*end_time*/> measurement_starting_ending_times;
	//int measurement_first_index, measurement_last_index;

    std::vector<subsystem_summary_data> not_participating;
    std::vector<subsystem_summary_data>  not_participating_measurement;

                //=================================//
                //       see comments below        //
                //=================================//


// methods
	RollupSet(){}
	inline ~RollupSet() {  for (auto pear : rollups) delete pear.second; }

	std::pair<bool,std::string> initialize(); // false & set error_message if passed a null pointer or if there was some problem building the default "overall" rollup.
	std::string getErrorMessage(){ return my_error_message; }
	std::pair<bool,std::string> add_workload_detail_line(WorkloadID&, IosequencerInput&, SubintervalOutput&);
	std::pair<bool,std::string> addRollupType
	(
		  std::string attributeNameComboText
		, bool nocsvSection
		, bool quantitySection
		, bool maxDroopMaxtoMinIOPSSection
		, ivy_int quantity
		, ivy_float maxDroop
	);
	// returns true if the rollup was created successfully.
		// [CreateRollup] Serial_Number+LDEV
		// [CreateRollup] host               [nocsv] [quantity] 16 [maxDroopMaxtoMinIOPS] 25 %
	// error_message is clear()ed upon entry and is set with an error message if it returns false

    RollupInstance* get_all_equals_all_instance(); // throws runtime_error if it can't find the all=all rollup.

	std::pair<bool,std::string> deleteRollup(std::string attributeNameComboText);
        // error_message is clear()ed upon entry and is set with an error message if it returns false

	void resetSubintervalSequence();
	void startNewSubinterval(ivytime start, ivytime end);
	std::pair<bool,std::string> makeMeasurementRollup();

	void rebuild();

	std::pair<bool,std::string> passesDataVariationValidation();

	inline int current_index() {return subintervalIndex;}

	std::string debugListRollups();
	void make_step_rollup_subfolders() { for (auto pear : rollups) { auto& p_RollupType = pear.second; p_RollupType->make_step_subfolder(); } }

    void print_measurement_summary_csv_line(unsigned int measurement_index);
};

// Comment section referred to above regarding how subsystem focus metric data are
// filtered and rolled up by rollup instance.

        // These are where the performance data are rolled up for those subsystem components
        // that are theoretically not participating in the test.

        // We typically are interested in the LDEV, Port, PG, etc. performance data
        // for those LDEVs, Ports, or PGs, etc. that are under test.

        // ivy builds by-RollupInstance real-time subsystem rollups according
        // to ivy_engine.h's "subsystem_summary_metrics" specification where have
        // subsystem configuration elements, with a sequence of metrics for those elements.

        // The first time ivy ran with this feature, the specification was:
        //    {
        //        { "MP core",     { "Busy %", "I/O buffers" } },
        //        { "CLPR",        { "WP_percent"                 } }
        //    };

        // The above instructs ivy to prepare a by-rollup instance data structure
        // for the use of the DFC, as well as to print out csv columns in the
        // specified order.

        // For the above specification, a first csv column group was generated
        // that had 5 columns, being "MP_core count", "avg MP_core busy_percent",
        // "avg MP_core io_buffers", "CLPR count", and avg CLPR WP_percent".

        // These key columns are displayed near the beginning after the test host CPU busy columns.

        // There is also a second column group, put at the end, that shows the min, max, and std dev
        // of each each metric.

        // The two column groups appear in each RollupInstance's csv files, both
        // overall measurement summary, as well as the csv files in test step folders
        // with one line per subinterval.

        // For each subsystem performance data element type (MP_core and CLPR in the above)
        // the by-RollupInstance summary data is built by spinning through each
        // MP_core data or CLPR data instance to see if that instance is contained
        // in the set of instances in the RollupInstance's "config_filter".

        // It the performance record matches the rollup instance, it gets added in
        // to the subsystem_summary_data object for that RollupInstance.

        // The performance records for MP_cores and CLPRs in this case for
        // instances that are not "in use for this test", are posted into the
        // single "not_participating" instance in the RollupSet.

        // Looking at a "not_participating.csv" file will show if the components that are
        // not in use nevertheless display any significant amount of activity.

        // There's only one "non_participating" summary data object, because the
        // element instances in it don't appear in any rollup instance and thus don't
        // vary by rollup instance.

        // We populate the "non participating" data at the time we are populating the
        // normal subsystem_summary_data for the "all=all" rollup instance.

        // For the all=all rollup, as we filter the subsystem performance data, the ones
        // that don't go in the all=all rollup are put in the RollupSet's "not_participating"

        // In order for the matching to work, the element name (such as "CLPR") that
        // is recorded as a configuration attribute of the workload LUN/LDEV
        // needs to be the same name used in the performance data we get from the subsystem.

        // Workload LUN/LDEV configuration attributes:
            // - The original LUN attributes provided by the SCSI Inquiry LUN lister tool
            // - For LUNs that map to an LDEV on a subsystem that we have a command device to,
            //   the attributes of the LDEV in the RMLIB API are added, such as drive type
            //   or HDT tiering policy settings.
            // - The augmented workload LUN has the workload name added to it to make it
            //   easy to [select] or [CreateRollup] by workload name.
            // - Augmented LUNs may also have "nickname" attributes where the attribute value
            //   can be put into a normalized form.  For example, PG "01-01" -> "1-1" or
            //   port "1A" -> "CL1-A".

        // In the pipe_driver_subthread for a command device, after finishing inserting
        // a new "GatherData" object for a subinterval and populating it with all the
        // the data we gathered from the subsystem for that subinterval, there is a post-processing
        // phase.

        // The post-processing phase includes spinning through each subsystem
        // summary element (MP_core & CLPR), and if it matches the "config_filter"

        // For each RollupInstance, we apply the "config_filter" for that RollupInstance
        // to select the performance records that match the test configuration
        // for that rollup instance, and we post the selected metrics into the
        // subsystem_summary_data object for the rollup instance.

        // How do we build the filter for each RollupInstance?

        // During rebuild(), which takes place at the beginning of a subinterval
        // sequence, we build the config_filter for each RollupInstance.

        // This config_filter is a map from attribute name (such as "port") to
        // the set of distinct attribute values, such as { "1A", "2A" } observed
        // when iterating over each workload LUN in the workloads comprising
        // each RollupInstance during a rebuild.

        // Then for configuration elements such as "MP core" that are only indirectly
        // associated with a particular configuration attribute value such as "MPU = 2"
        // that is observed for a given LUN/LDEV, there is a post-processing phase that
        // adds indirectly associated configuration elements to the config_filter, where the
        // processing is specific to a given indirect element and we have access to
            // 1 - The LUN/LDEV attributes
            // 2 - The configuration data for the subsystem obtained from RMLIB or possibly
            //     in future from an SVP config csv file set.
            // 3 - Static tables for a given subsystem type / sub-model that give
            //     fixed relationships.

        // For HM800, there is a fixed relationship between MPU ("0", "1", "2", or "3")
        // and sub-model ("HM800S", "HM800M", or "HM800H") that shows you for
        // HM800M & MPU = "2", the associated MP number and corresponding MP_core values
        //        { "HM800M",{
        //            ...
        //            { "2",
        //                {
        //                    { "MP#08", "MP20-00" },
        //                    { "MP#09", "MP20-01" },
        //                    { "MP#0A", "MP20-02" },
        //                    { "MP#0B", "MP20-03" }
        //                }
        //            }, ...
        //        } } ...

        // Thus in post-processing the config_builter in rebuild(), for each HM880
        // instance of MPU, we add the associated MP_number and MP_core instances.

        // That way the MP_core % busy numbers will roll up appropriately for each RollupInstance.

        // {Think of perhaps a better place to put the above explanation.}

