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

class LUN
{
public:
//variables
	std::map<std::string, std::string> attributes;

//methods
	LUN(){};

	bool loadcsvline(std::string headerline, std::string dataline, std::string logfilename); // false on a failure to load the line properly
	std::string attribute_value(std::string attribute_name);
	bool contains_attribute_name(std::string attribute_name);
	void set_attribute(std::string /* name */, std::string /* value */);
	bool attribute_value_matches(std::string attribute_name, std::string value);
	std::string toString();
	void copyOntoMe(LUN* p_other); // This is used to make "the sample LUN" that has every attribute type seen in any LUN,
		// and it's used to make a copy of a LUN in a workload tracker that we then add the workload attribute to.
	void createNicknames(); // gives "host" the value of ivyscript_hostname   ... and ... ???, gives attribute "all" the value "all"?

static std::string convert_to_lower_case_and_convert_nonalphameric_to_underscore(std::string column_title);

};


