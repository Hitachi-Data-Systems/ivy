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

#include "ivytypes.h"
#include "ivydefines.h"

class IosequencerInputRollup {

// The idea here is that when we show what inputs were used in a rollup summary csv file line
// we can just show a single value if the subinterval values for each subinterval and each LUN on each host were the same.

// The top level map is by parameter name, like "IOPS".

// The 2nd level map is by string form of parameter value,
// and this maps to a count of how many times that value was seen.

	std::map<std::string /* metric name in upper case */, std::map<std::string /* metric value */, long int /* count */>> values_seen;
		// there's a count for each value of each parameter
	long int count{0};

public:
	std::pair<bool,std::string> add                        (std::string parameterNameEqualsTextValueCommaSeparatedList); // returns false on malformed input
	std::string                 getParameterTextValueByName(std::string, bool detail=true);
	void                        print_values_seen          (std::ostream&);
	static std::string          CSVcolumnTitles            (); // This function needs to be updated when you add a new iosequencer parameter
	std::string                 CSVcolumnValues            (bool detail=true); // This function needs to be updated when you add a new iosequencer parameter
	void                        merge                      (const IosequencerInputRollup&);  // used when merging one Rollup object into another
	bool                        addNameEqualsTextValue     (std::string);
	void                        clear                      () { values_seen.clear(); count=0; }
	ivy_float                   get_Total_IOPS_Setting     (const unsigned int subinterval_count = 1); // returns -1 if there was an IOPS=max.
	static std::string          Total_IOPS_Setting_toString (const ivy_float); // returns a number or "max".
	bool                        is_only_IOPS_max           ();
};


