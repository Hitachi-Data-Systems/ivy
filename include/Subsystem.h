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

#include <vector>
#include <set>

#include "LUN.h"
#include "GatherData.h"

class ivy_engine;

class Subsystem
{
public:
// variables
	std::string serial_number;
	std::vector<LUN*> LUNpointers;
	std::map<std::string /* LUN attribute name */, std::set<std::string> /* LUN attribute values */> LUNattributeValues;
		// This will have a set of LDEVs, a set of PGs, a set of CLPRs, etc.
		// This could drive data gather.
	std::vector<GatherData> gathers;
		// NOTE: We do the first gather (#0) at t=0, so
		// subinterval #0 is bounded by gathers #0 and #1.

		// Some element types and metrics don't appear in the first gather, such as metrics that are derived from the delta between cumulative counts in successive gathers.

		// Some element instances (e.g. instances of LDEV) may not have the same set of metrics (e.g. DP-vols have tier metrics, but not drive busy time)

// methods
	Subsystem(std::string sn); // Note constructor leaves LUNpointers empty
	~Subsystem();

	virtual std::string type();

	void add(LUN* pLUN)
	{
		LUNpointers.push_back(pLUN);
		for (auto& attribute : pLUN->attributes) LUNattributeValues[attribute.first].insert(attribute.second);
	}

	virtual void gather();
	std::string get_metric_value(
		int subinterval_number
		, std::string config_attribute_name /* e.g. LDEV */
		, std::string config_attribute_value /* 00:00 */
		, std::string metric_nmw /* total I/O count */
	);

	void print_subinterval_csv_line_set( std::string subfolder_leaf_name /* like "401034.perf" */);
		// Used to print the contents of the GatherData object, such as a performance data gather for a subinterval, where
		// in the designated subfolder, further subfolders for each element type such as LDEV or Port are created.
		// Then in these subfolders, a separate file is used for each instance of the element type.
		// With this method, a single line is appended to the each file, corresponding to the currently being printed GatherData object.
		// This is used in the step folder, so that for each LDEV you get a line for each subinterval.
};

class pipe_driver_subthread;

class Hitachi_RAID_subsystem : public Subsystem
{
public:
// variables
	LUN* pCmdDevLUN {nullptr};
	pipe_driver_subthread* pRMLIBthread {nullptr};
	std::string command_device_description {};  // is set when we are about to connect to a discovered command device


	GatherData configGatherData;
	//  GatherData contains:
    //	std::map
    //	<
    //      std::string, // Configuration element type name, e.g. LDEV
    //		std::map
    //		<
    //          std::string, // Configuration element instance, e.g. 00:FF
    //			std::map
    //			<
    //              std::string, // Metric, e.g. "total I/O count"
    //				metric_value
    //            >
    //        >
    //    >
    //    data;


	ivytime config_gather_time;

	std::map
	<
        std::string, // Pool ID as a string, like "0" or "1"
        std::set     // Set of pointers to pool volume instances in configGatherData
        <
    		std::pair
    		<
                const std::string, // Configuration element instance, e.g. 00:FF
    			std::map
    			<
                    std::string, // Metric, e.g. "total I/O count"
    				metric_value
                >
            >*
        >
 	> pool_vols;


// methods
	std::string type() override;
	void gather() override;
	Hitachi_RAID_subsystem(std::string serial_number, LUN* pL);
	~Hitachi_RAID_subsystem();
//	void add(LUN*) override;

	ivy_float get_wp(const std::string& CLPR, int subinterval);  // throws std::invalid_argument
	ivy_float get_wp_change_from_last_subinterval(const std::string& CLPR, unsigned int subinterval);  // throws std::invalid_argument

	std::string pool_vols_by_pool_ID();
};
