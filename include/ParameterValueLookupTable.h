//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <map>
#include <string>
#include <iostream>
#include <sstream>

class ParameterValueLookupTable {

public:
	std::map<std::string, std::string> contents;

	bool fromString(std::string);  // true if well-formed
	bool addString(std::string);  // true if well-formed
		// Well formed means empty string, or
		//      identifier1 = value1, identifier2=value2, ...
		// Commas between values are optional.
		// Identifiers start with alphabetic characters and consist of alphanumeric characters and underscores ('_')
		// Values either consist of alphanumeric characters and inderscores, or are single- or double-quoted strings.
		// Single- or double-quoted strings may not contain the starting quote character.  (No escaped quoteds.)
		// For example, text1 = 'bork said "oh!"' is OK,
		// but text1 = "bork said \"oh!\"" and text1 = "bork said ""oh!""" are invalid.
		// To store an empty string as a value it has to be shown as identifier="" or identifier=''.
		// A particular identifier can only have one value.  If an identifier occurs more than once,
		// the last value set is the one that is retained.

	// Keys are translated to lower case before storing them and before looking them up.
	// Values are not translated to lower case.
	std::string toString();
	bool contains(std::string key);
	std::string retrieve(std::string key);  // returns empty string if the key is not present
	bool containsOnlyValidParameterNames(std::string listOfValidParameterNames);

};

