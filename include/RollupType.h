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

#include <map>
#include <string>
#include <iostream>

class RollupSet;

#include "AttributeNameCombo.h"
#include "SubintervalOutput.h"
#include "RollupInstance.h"

class RollupType  // serial_number+Port
{
	// Note: There is no (rollup) data by RollupType.  The data is only at the RollupInstance level.
	//       However, at the RollupType level, we keep various RunningStat accumulators that describe
	//       the variation from RollupInstance to RollupInstance of this RollupType.

public:
	RollupSet* pRollupSet;
	std::map<std::string /*rollupInstanceName*/, RollupInstance*> instances;
	std::map<std::string /*workloadID*/, RollupInstance*> rollupInstanceByWorkloadID;
	AttributeNameCombo attributeNameCombo;  // for example, "serial_number+port"

	bool nocsv {false};  // suppresses printing of csv files for this rollup.

	bool haveQuantityValidation;
	unsigned int quantityRequired;

	bool have_maxDroopMaxtoMinIOPS;
	ivy_float maxDroopMaxToMinIOPS;  // Data are valid if the lowest

	AttributeNameCombo getAttributeNameCombo() { return attributeNameCombo; }

	RunningStat<ivy_float, ivy_int>
		  measurementRollupAverageIOPSRunningStat
		, measurementRollupAverageBytesPerSecRunningStat
		, measurementRollupAverageBlocksizeBytesRunningStat
		, measurementRollupAverageServiceTimeRunningStat
		, measurementRollupAverageResponseTimeRunningStat;

    RollupInstance* p_wurst_RollupInstance {nullptr};

    std::string step_subfolder_name {};

	std::string measurementRollupFolder; // e.g. "Port+PG" subfolder of testFolder

    std::string measurement_rollup_data_validation_csv_filename;   // goes in measurementRollupFolder


// methods
	RollupType
	(
		  RollupSet* pRollupSet
		, AttributeNameCombo&
		, bool nocsvSection
		, bool quantitySection
		, bool maxDroopMaxtoMinIOPSSection
		, int quantity
		, ivy_float maxDroopMaxtoMinIOPS
	);

	inline ~RollupType() { for (auto pear : instances) delete pear.second; }

	void rebuild();

	bool isValid() {return attributeNameCombo.isValid;}  // Ian: limited meaning - unfortunate choice of name

	bool add_workload_detail_line(std::string& callers_error_msg, WorkloadID& wID, IosequencerInput& iI, SubintervalOutput& sO);

	std::pair<bool,std::string> passesDataVariationValidation();

	std::pair<bool,std::string> makeMeasurementRollup(int firstMeasurementIndex, int lastMeasurementIndex);

	static	std::string getDataValidationCsvTitles();

	std::string getDataValidationCsvValues();

	void printMe(std::ostream&);

	void make_step_subfolder(); // and print config thumbnail etc.

    void print_measurement_summary_csv_line();
};



