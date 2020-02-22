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
#include "logger.h"

class LUN
{
public:
//variables
	std::map<std::string /* normalized attribute name */, std::pair<std::string /* original name */, std::string /* value */>> attributes;

//methods
	LUN(){};

	bool loadcsvline(const std::string& headerline, const std::string& dataline, logger&); // false on a failure to load the line properly

	void set_attribute(const std::string& /* name */, const std::string& /* value */);

	void copyOntoMe(const LUN* p_other); // This is used to make "the sample LUN" that has every attribute type seen in any LUN,
		// and it's used to make a copy of a LUN in a workload tracker that we then add the workload attribute to.

	void createNicknames(); // gives "host" the value of ivyscript_hostname   ... and ... ???, gives attribute "all" the value "all"?

	std::string attribute_value        (const std::string& attribute_name) const;
	std::string original_name          (const std::string& attribute_name) const;
	bool        contains_attribute_name(const std::string& attribute_name) const;
	bool        attribute_value_matches(const std::string& attribute_name, const std::string& value) const;
	std::string toString() const;
	std::string valid_attribute_names() const;

	void push_attribute_values(std::map<std::string /* LUN attribute name */, std::set<std::string> /* LUN attribute values */>&) const;
    void push_attribute_names(std::set<std::string>& column_headers) const;

static std::string normalize_attribute_name(const std::string& column_title, bool preserve_case=false);

};


