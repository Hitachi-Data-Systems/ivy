//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <set>

using namespace std;

class LDEVset
{
public:
	// if this is too inefficient, come back later and change implementation
	std::set<unsigned int> seen;
	std::set<unsigned int>::iterator it;
	bool allLDEVs{false};

	bool add(std::string ldev_set, std::string logfilename);

		// An LDEV set could be a single LDEV "0F40" or "0f:40"
		// or a string representation of a set of LDEVs
		// such as "0f:40-0f:4f 10:00 EF:00-EF:FF"

		// add("all") sets the allLDEVs flag on and empties the set.

		// returns false upon malformed input.
		// WARNING - add/delete do the adding/deleting as they parse the LDEV set string,
		// so the adds/deletes up to the parsing error point will have been performed
		// even when they return "false" for malformed input.

	std::string toString() const; // Makes the above string representation.  If allLDEVs flag is on, returns "All"

	std::string toStringWithSemicolonSeparators();
		// calls toString() and replaces all spaces with semicolons.

	bool addWithSemicolonSeparators(std::string ldev_set, std::string logfilename);
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

