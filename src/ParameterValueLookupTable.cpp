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
#include <set>

#include "ivyhelpers.h"
#include "ivy_engine.h"

std::pair<bool,std::string> ParameterValueLookupTable::fromString(std::string s)
{
	contents.clear();
	return addString(s);
}

std::pair<bool,std::string> ParameterValueLookupTable::addString(std::string s)
{
	unsigned int i {0};

	unsigned int identifier_start, value_start, identifier_length, value_length;
	char starting_quote;

	while (true) {
		while (i < s.length() && (isspace(s[i]) || ',' == s[i]))
		{ // whitespace or commas before identifier
			i++;
		}

		if ( i >= s.length() ) return std::make_pair(true,std::string(""));

		if (!isalpha(s[i])) // start of identifier
		{
            std::ostringstream o;
			o << "ParameterValueLookupTable::fromString() invalid input string:" << std::endl << s << std::endl;
			for (unsigned int j=0; j<i; j++) o << ' ';
			o << '^' << std::endl << "Key must start with alphabetic, or value needs to be quoted." << std::endl;
			contents.clear();
			std::cout << o.str();
			log(m_s.masterlogfile, o.str());
			return std::make_pair(false,o.str());
		}

		identifier_start=i;
		identifier_length=1;
		i++;

		while ( i<s.length() && (isalnum(s[i]) || '_' == s[i]) )
		{ // identifier
			i++;
			identifier_length++;
		}

		while (i<s.length() && isspace(s[i]))
		{ // step over whitespace following identifier
			i++;
		}

		if (i>=s.length() || '=' != s[i])
		{
            std::ostringstream o;
			o << "ParameterValueLookupTable::fromString() invalid input string:" << std::endl << s << std::endl;
			for (unsigned int j=0; j<i; j++) o << ' ';
			o << '^' << std::endl << "No '=' following identifier." << std::endl;
			contents.clear();
			std::cout << o.str();
			log(m_s.masterlogfile,o.str());
			return std::make_pair(false,o.str());
		}
		i++; // step over '='

		while (i<s.length() && isspace(s[i]))
		{ // step over whitespace following '='
			i++;
		}

		if (i>=s.length())
		{
            std::ostringstream o;
			o << "ParameterValueLookupTable::fromString() invalid input string:" << std::endl << s << std::endl;
			for (unsigned int j=0; j<i; j++) o << ' ';
			o << '^' << std::endl << "No value following '='." << std::endl
				<< "To specify that a value is the null string, say identifier=\"\"" << std::endl;
			contents.clear();
			std::cout << o.str();
			log(m_s.masterlogfile,o.str());
			return std::make_pair(false,o.str());
		}

		// start of value
		if ('\'' == s[i] || '\"' == s[i])
		{ // value is quoted string
			starting_quote=s[i];
			i++;
			value_start=i;
			value_length=0;
			while (i<s.length() && starting_quote != s[i]) {i++; value_length++;}
			if (i>=s.length())
			{
                std::ostringstream o;
				o << "ParameterValueLookupTable::fromString() invalid input string:" << std::endl << s << std::endl;
				for (unsigned int j=0; j<i; j++) o << ' ';
				o << '^' << std::endl << "No ending quote." << std::endl;
				contents.clear();
				std::cout << o.str();
				log(m_s.masterlogfile,o.str());
				return std::make_pair(false,o.str());
			}
			i++; // step over ending quote
		}
		else
		{ // value is unquoted alphanumerics and underscores, periods, and percent signs
			if (!( isalnum(s[i]) || '_' == s[i] || '.' == s[i] || '%' == s[i] ))
			{
                std::ostringstream o;
				o << "ParameterValueLookupTable::fromString() invalid input string:" << std::endl << s << std::endl;
				for (unsigned int j=0; j<i; j++) o << ' ';
				o << '^' << std::endl << "Unquoted value must be all alphanumerics, underscores, periods, and percent signs." << std::endl;
				contents.clear();
				std::cout << o.str();
				log(m_s.masterlogfile,o.str());
				return std::make_pair(false,o.str());
			}
			value_start=i;
			value_length=1;
			i++;
			while (i<s.length() && (isalnum(s[i]) || '_' == s[i] || '.' == s[i] || '%' == s[i]))
			{
				i++;
				value_length++;
			}
		}

		contents[normalize_identifier(s.substr(identifier_start,identifier_length))]=s.substr(value_start,value_length);
	}

    return std::make_pair(true,std::string(""));
}

std::string ParameterValueLookupTable::toString()
{
	std::ostringstream o;
	bool first=true;
	std::string value;
	bool needs_quoting;
	char quote_char;

	for (auto& pear : contents)
	{
		needs_quoting=false;
		quote_char='\"';
		if (!first) o << ", ";
		first=false;
		o << pear.first << '=';
		value=pear.second;
		for (unsigned int i=0; i<value.length(); i++)
		{
			if (!( isalnum(value[i]) || '_' == value[i] || '.' == value[i] || '%' == value[i] ))
			{
				needs_quoting=true;
				if ('\"' == value[i]) quote_char = '\'';
			}
		}
		if (needs_quoting) o << quote_char;
		o << value;
		if (needs_quoting) o << quote_char;
	}

	return o.str();
}

bool ParameterValueLookupTable::contains(std::string key)
{
	auto m = contents.find(normalize_identifier(key));
	if (contents.end()==m) return false;
	return true;
}

std::string ParameterValueLookupTable::retrieve(std::string key)
{
	auto m = contents.find(normalize_identifier(key));
	if (contents.end()==m) return std::string("");
	return m->second;
}

bool ParameterValueLookupTable::containsOnlyValidParameterNames(std::string s) // s = listOfValidParameterNames
{
	// Input is list of valid parameter names, like "blocksize, iorate".
	// Comma separators are optional.
	// Returns false on malformed input list of valid parameter names, or if "contents" contains any invalid keys.

	unsigned int i=0;

	int identifier_start, identifier_length;
	bool all_valid=true;
	std::set<std::string> valid_parameter_names;

	while (true) {
		while (i<s.length() && (isspace(s[i]) || ',' == s[i]))
		{ // whitespace or commas before identifier
			i++;
		}

		if (i>=s.length()) break;

		if (!isalpha(s[i])) // start of identifier
		{
            std::ostringstream o;
			o << "ParameterValueLookupTable::containsOnlyValidParameterNames() invalid list of valid parameter names:" << std::endl << s << std::endl;
			for (unsigned int j=0; j<i; j++) o << ' ';
			o << '^' << std::endl << "Parameter name must start with alphabetic and consist entirely of alphanumerics and underscores." << std::endl;
			std::cout << o.str(); log(m_s.masterlogfile,o.str());
			return false;
		}

		identifier_start=i;
		identifier_length=1;
		i++;

		while ( i<s.length() && (isalnum(s[i]) || '_' == s[i]) )
		{ // identifier
			i++;
			identifier_length++;
		}
		valid_parameter_names.insert(normalize_identifier(s.substr(identifier_start,identifier_length)));

//*debug*/ os << "/*debug*/ valid_parameter_names contains ";
//*debug*/	for (auto& s: valid_parameter_names) os << " \"" << s << '\"';
//*debug*/	os << std::endl;

	}

    std::ostringstream o;

	for (auto& pear : contents)
	{
		if (valid_parameter_names.end() == valid_parameter_names.find(normalize_identifier(pear.first)))
		{
			if(all_valid) o << "ParameterValueLookupTable::containsOnlyValidParameterNames() Invalid parameter names present:" << pear.first;
			all_valid=false;
			o << ' ' << pear.first;
		}
	}

	if (!all_valid)
	{
        o << std::endl;
        log (m_s.masterlogfile,o.str());
        std::cout << o.str();
    }
	return all_valid;
}

