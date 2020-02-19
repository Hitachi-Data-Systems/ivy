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

#include <set>

#include "logger.h"

using namespace std;

class LDEVset
{
public:
//variables

	// if this is too inefficient, come back later and change implementation
	std::set<unsigned int> seen;
	bool allLDEVs{false};

//methods

	bool add(std::string ldev_set, logger&);

		// An LDEV set could be a single LDEV "0F40" or "0f:40"
		// or a string representation of a set of LDEVs
		// such as "0f:40-0f:4f 10:00, EF:00 - EF:FF".

		// Note that spaces are optional around the hypen in a consecutive range,
		// and that either spaces or commas or semicolons can be used to separate either single LDEVs (singletons) or
		// consecutive hyphenated LDEV ranges.

		// add("all") sets the allLDEVs flag on and empties the set.

		// returns false upon malformed input.
		// WARNING - add/delete do the adding/deleting as they parse the LDEV set string,
		// so the adds/deletes up to the parsing error point will have been performed
		// even when they return "false" for malformed input.

	std::string toString() const; // Makes the above string representation.  If allLDEVs flag is on, returns "All"

	std::string toStringWithSemicolonSeparators();
		// calls toString() and replaces all spaces with semicolons.

	bool addWithSemicolonSeparators(std::string ldev_set, logger&);
		// this replaces all semicolons in the input string and then calls add()
		// These functions using semicolon separators are used due to the way ivymaster reads csv files.

	bool isAllLDEVs() {return allLDEVs;}

	static int LDEVtoInt(std::string s);  // returns -1 on invalid input.  supports 4 hex digits or 2 hex digits then colon then 2 hex digits.

	void merge(const LDEVset&);

	inline void clear() {seen.clear(); allLDEVs=false;}
	inline void setAll() {seen.clear(); allLDEVs=true;}
	inline bool empty() { return seen.empty() && !allLDEVs;}
	inline unsigned int size() const {return allLDEVs ? 65534 : seen.size();}
};

