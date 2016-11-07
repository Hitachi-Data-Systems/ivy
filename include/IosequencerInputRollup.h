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

class IosequencerInputRollup {

// The idea here is that when we show what inputs were used in a rollup summary csv file line
// we can just show a single value if the subinterval values for each subinterval and each LUN on each host were the same.

// The top level map is by parameter name, like "IOPS".

// The 2nd level map is by string form of parameter value,
// and this maps to a count of how many times that value was seen.

	std::map<std::string,std::map<std::string, long int>> values_seen;
		// there's a count for each value of each parameter
	long int count{0};

public:
	bool add (std::string& callers_error_message, std::string parameterNameEqualsTextValueCommaSeparatedList); // returns false on malformed input
	std::string getParameterTextValueByName(std::string, bool detail=true);
	void print_values_seen(std::ostream&);
	static std::string CSVcolumnTitles(); // This function needs to be updated when you add a new iosequencer parameter
	std::string CSVcolumnValues(bool detail=true); // This function needs to be updated when you add a new iosequencer parameter
	void merge(const IosequencerInputRollup&);  // used when merging one Rollup object into another
	bool addNameEqualsTextValue(std::string);
	void clear() { values_seen.clear(); count=0;}
};


