//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
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

	bool add_workload_detail_line(std::string& callers_error_msg, WorkloadID& wID, IogeneratorInput& iI, SubintervalOutput& sO);

	bool passesDataVariationValidation();

	bool makeMeasurementRollup(std::string callers_error_message, int firstMeasurementIndex, int lastMeasurementIndex);
static	std::string getDataValidationCsvTitles();
	std::string getDataValidationCsvValues();
	void printMe(std::ostream&);
};



