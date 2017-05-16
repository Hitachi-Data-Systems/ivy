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
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdlib.h> // atoi
#include <algorithm>
#include <list>
#include <map>

#include "ivyhelpers.h"
#include "ivydefines.h"

#include "IosequencerInputRollup.h"


/*static*/ std::string IosequencerInputRollup::CSVcolumnTitles()
{
	return
		",iosequencer type"
		",blocksize"
		",maxTags"
		",IOPS input parameter setting"
		",fractionRead"
		",VolumeCoverageFractionStart"
		",VolumeCoverageFractionEnd"
		",SeqStartFractionOfCoverage"
		;
}

//std::string IosequencerInputRollup::hostnameList()
//{
//	// this prints out the set of hostnames seen in the same format you can write it in an ivyscript file
//	// 	sun159 172.17.19.159 cb26-31 172.17.19.100-104
//	std::ostringstream o;
//	std::map<std::string,std::map<std::string, long int>>::iterator host_it = values_seen.find("HOST");
//
//	std::string
//		hostname,
//		earlier_base_part{""},
//		first_numeric_suffix{""},
//		last_numeric_suffix{""},
//		base_part,
//		numeric_suffix;
//	int
//		last_suffix_as_int=-1,
//		suffix_as_int;
//
//	bool	first_host=true;
//
//	if (values_seen.end() == host_it) return std::string("");
//	for (auto& pear : (*host_it).second)
//	{
//		hostname = toLower(pear.first);
//
//		if (hostname.length() >0 && isdigit(hostname[hostname.length()-1]))
//		{
//			// have hostname with numeric suffix or IPv4 IP address
//			int i=hostname.length()-1;
//			while (i>0 && isdigit(hostname[i])) i--;
//			// hostname can't be all digits
//			base_part=hostname.substr(0,i+1);
//			numeric_suffix=hostname.substr(i+1,hostname.length()-(i+1));
//			suffix_as_int = atoi(numeric_suffix.c_str());
//		}
//		else
//		{
//			// hostname does not have a numeric suffix
//			base_part="";
//			numeric_suffix="";
//		}
//
//		if (earlier_base_part.length() >0)
//		{
//			if (0==earlier_base_part.compare(base_part) && suffix_as_int==(last_suffix_as_int+1))
//			{
//				// continuation of series
//				last_numeric_suffix=numeric_suffix;
//				last_suffix_as_int=suffix_as_int;
//			}
//			else
//			{
//				// first write out the previous hostname range
//				if (!first_host) o << ' ';
//				first_host=false;
//				o << earlier_base_part << first_numeric_suffix;
//				if (0!=first_numeric_suffix.compare(last_numeric_suffix))
//				{
//					o << '-' << last_numeric_suffix;
//				}
//
//				// now process the current hostname
//				if (numeric_suffix.length()>0)
//				{
//					// have a new hostname with a numeric suffix
//					earlier_base_part=base_part;
//					first_numeric_suffix=numeric_suffix;
//					last_numeric_suffix=numeric_suffix;
//					last_suffix_as_int=suffix_as_int;
//				}
//				else
//				{
//					o << hostname;
//					earlier_base_part="";
//					first_numeric_suffix="";
//					last_numeric_suffix="";
//					last_suffix_as_int=-1;
//				}
//			}
//		}
//		else
//		{
//			// no earlier hostname range
//			if (numeric_suffix.length()>0)
//			{
//				// have a new hostname with a numeric suffix
//				earlier_base_part=base_part;
//				first_numeric_suffix=numeric_suffix;
//				last_numeric_suffix=numeric_suffix;
//				last_suffix_as_int=suffix_as_int;
//			}
//			else
//			{
//				if (!first_host) o << ' ';
//				first_host=false;
//				o << hostname;
//				earlier_base_part="";
//				first_numeric_suffix="";
//				last_numeric_suffix="";
//				last_suffix_as_int=-1;
//			}
//		}
//	}
//	// print out any remaining range
//	if (earlier_base_part.length()>0)
//	{
//		if (!first_host) o << ' ';
//		first_host=false;
//		o << earlier_base_part << first_numeric_suffix;
//		if (0!=first_numeric_suffix.compare(last_numeric_suffix))
//		{
//			o << '-' << last_numeric_suffix;
//		}
//	}
//
//	return o.str();
//}


std::string IosequencerInputRollup::CSVcolumnValues(bool detail)
{
	std::ostringstream o;
	std::map<std::string,std::map<std::string, long int>>::iterator it;

	o
	<< ',' << '\"' << getParameterTextValueByName("iosequencer",detail) << '\"'
	<< ',' << '\"' << getParameterTextValueByName("blocksize",detail) << '\"'
	<< ',' << '\"' << getParameterTextValueByName("maxTags",detail) << '\"'
	<< ',' << '\"' << getParameterTextValueByName("IOPS",false) << '\"'   // for DFC=PID, as IOPS changes every subinterval.
	<< ',' << '\"' << getParameterTextValueByName("fractionRead",detail) << '\"'
	<< ',' << '\"' << getParameterTextValueByName("VolumeCoverageFractionStart",detail) << '\"'
	<< ',' << '\"' << getParameterTextValueByName("VolumeCoverageFractionEnd",detail) << '\"'
	<< ',' << '\"' << getParameterTextValueByName("SeqStartFractionOfCoverage",detail) << '\"'
	;

	return o.str();

}


std::pair<bool,std::string>	IosequencerInputRollup::add(std::string parameterNameEqualsTextValueCommaSeparatedList)
{

	bool wellFormedInput{true};

	count++;

	if (0==parameterNameEqualsTextValueCommaSeparatedList.length())
	{
		return std::make_pair(false,"IogeneratoInputRollup::add() called with null list of parameter name = value");
	}

	unsigned int i=0, start, len;

	while (i<parameterNameEqualsTextValueCommaSeparatedList.length()) {
		start=i;
		len=0;
		while (i<parameterNameEqualsTextValueCommaSeparatedList.length() && ',' != parameterNameEqualsTextValueCommaSeparatedList[i]) {
			i++;
			len++;
		}
		if (len>0) {
			wellFormedInput = wellFormedInput && addNameEqualsTextValue(parameterNameEqualsTextValueCommaSeparatedList.substr(start,len));
		}
		if (i<parameterNameEqualsTextValueCommaSeparatedList.length() && ',' == parameterNameEqualsTextValueCommaSeparatedList[i])
			i++; // step over comma
	}
	return std::make_pair(wellFormedInput,"");

}

void	IosequencerInputRollup::print_values_seen (std::ostream& o) {

	o << "values_seen:" << std::endl;
	for (std::map<std::string,std::map<std::string, long int>>::iterator name_it=values_seen.begin(); name_it != values_seen.end(); name_it++ ){
		o << "parameter name = \"" << (*name_it).first << "\"." << std::endl;
		for (std::map<std::string, long int>::iterator value_it=(*name_it).second.begin(); value_it != (*name_it).second.end(); value_it++ ){
			o << "parameter value \"" << (*value_it).first << "\" appeared " << (*value_it).second << " times." << std::endl;
		}
	}
}

bool	IosequencerInputRollup::addNameEqualsTextValue(std::string nev) {

	// input is well-formed if there is an equals sign with something non-blank on either sides

	unsigned int i=0;
	std::string name, value;

	while (i<nev.length() && '=' != nev[i]) i++;

	if (0==i) return false;

	name=nev.substr(0,i);

	if (0<( nev.length()-(i+1) )) {
		value=nev.substr(i+1,nev.length()-(i+1));
	} else {
		return false;
	}
	trim(name);
	trim(value);
	if (0==name.length() || 0==value.length()) return false;

	std::map<std::string, std::map<std::string, long int>>::iterator name_it = values_seen.find(toUpper(name));

	if (values_seen.end()==name_it) {
		// have never seen this parameter name before
		std::pair<std::string,long int> pear;
		pear.first=value;
		pear.second=1;
		std::map<std::string, long int> new_name_map;
		new_name_map.insert(pear);
		std::pair<std::string,std::map<std::string, long int>> map_pear;
		map_pear.first=toUpper(name);
		map_pear.second=new_name_map;
		values_seen.insert(map_pear);
	} else {
		std::map<std::string, long int>::iterator value_it = (*name_it).second.find(value);
		if ((*name_it).second.end() == value_it) {
			// have parameter name but new parameter value
			std::pair<std::string,long int> pear;
			pear.first=value;
			pear.second=1;
			(*name_it).second.insert(pear);
		} else {
			// increment one we've seen before;
			(*value_it).second++;
		}
	}
	return true;
}

void IosequencerInputRollup::merge(const IosequencerInputRollup& other)
{
	for (auto& pear : other.values_seen)
	{
		const std::string& parameterName=pear.first;
		for (auto& kumquat : pear.second)
		{
			const std::string& value_representation = kumquat.first;
			const int& count = kumquat.second;
			(values_seen[parameterName])[value_representation] += count;
		}
	}

	count += other.count;

	return;
}

std::string IosequencerInputRollup::getParameterTextValueByName(std::string parameterName, bool detail) {

	std::map<std::string,std::map<std::string,long int>>::iterator name_it = values_seen.find(toUpper(parameterName));

	if (values_seen.end() == name_it) {
		return "";
	}

	if ( (1 == (*name_it).second.size()) && ( count == (* ((*name_it).second.begin()) ).second )) {
		return (*((*name_it).second.begin())).first;
	}

	if (detail) {
		int i=0;
		std::ostringstream o;
		for (std::map<std::string,long int>::iterator value_it=(*name_it).second.begin(); value_it!=(*name_it).second.end(); value_it++) {
			if (i>0) o << "; ";
			i++;
			o << (*value_it).first;
			if ((*value_it).second != count) {
				// if not every IosequencerInput had the same parameter value, show the count in parentheses.
				o << '(' << (*value_it).second << '/' << count << ')';
			}
		}
		return o.str();
	}
	else
	{
		return std::string("multiple");
	}
}

