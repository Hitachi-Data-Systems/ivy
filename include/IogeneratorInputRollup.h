//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <map>
#include <string>

class IogeneratorInputRollup {

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
	static std::string CSVcolumnTitles(); // This function needs to be updated when you add a new iogenerator parameter
	std::string CSVcolumnValues(bool detail=true); // This function needs to be updated when you add a new iogenerator parameter
	void merge(const IogeneratorInputRollup&);  // used when merging one Rollup object into another
	bool addNameEqualsTextValue(std::string);
	void clear() { values_seen.clear(); count=0;}
};


