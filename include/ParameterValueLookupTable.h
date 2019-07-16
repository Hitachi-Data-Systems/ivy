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

#include <map>
#include <string>
#include <iostream>
#include <sstream>

#include "nestedit.h"

class ParameterValueLookupTable {

public:
// variables
	std::map<std::string, std::string> contents;
    static std::map<std::string,std::string> parameter_name_rehydration_table;
    nestedit workload_loopy {};

//methods
	std::pair<bool,std::string> fromString(std::string);  // true if well-formed
	std::pair<bool,std::string> addString(std::string);  // true if well-formed
		// Well formed means empty string, or
		//      identifier1 = value1, identifier2=value2, ...
		// Commas between values are optional.
		// Identifiers start with alphabetic characters and consist of alphanumeric characters and underscores ('_')
		// Values either consist of alphanumeric characters and inderscores, or are single- or double-quoted strings.
		// Single- or double-quoted strings may not contain the starting quote character.  (No escaped quotes.)
		// For example, text1 = 'bork said "oh!"' is OK,
		// but text1 = "bork said \"oh!\"" and text1 = "bork said ""oh!""" are invalid.
		// To store an empty string as a value it has to be shown as identifier="" or identifier=''.
		// A particular identifier can only have one value.  If an identifier occurs more than once,
		// the last value set is the one that is retained.

	// Keys are translated to lower case before storing them and before looking them up.
	// Values are not translated to lower case.
	std::string toString();
	static std::string rehydrate_parameter_name(const std::string&);
	bool contains(std::string key);
	std::string retrieve(std::string key);  // returns empty string if the key is not present
	std::pair<bool,std::string> containsOnlyValidParameterNames(std::string listOfValidParameterNames);


};
