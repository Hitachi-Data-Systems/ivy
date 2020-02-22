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

#include "ivytypes.h"
#include "IosequencerInputRollup.h"
#include "ivyhelpers.h"

using namespace std::string_literals;

/*static*/ std::string IosequencerInputRollup::CSVcolumnTitles()
{
	return
		",iosequencer type"
		",blocksize"
		",maxTags"
		",IOPS input parameter setting"
		",skew weight"
		",fractionRead"
		",dedupe"
		",dedupe_method"
		",duplicate_set_size"
		",pattern"
		",compressibility"
		",RangeStart"
		",RangeEnd"
		",SeqStartPoint"
		;
}



std::string IosequencerInputRollup::CSVcolumnValues(bool detail)
{
	std::ostringstream o;
	std::map<std::string,std::map<std::string, long int>>::iterator it;

	o
	<< ',' << '\"' << getParameterTextValueByName("iosequencer",detail) << '\"'
	<< ',' << '\"' << getParameterTextValueByName("blocksize",detail) << '\"'
	<< ',' << '\"' << getParameterTextValueByName("maxTags",detail) << '\"'
	<< ',' << '\"' << getParameterTextValueByName("IOPS",false) << '\"'   // for DFC=PID, as IOPS changes every subinterval.
	<< ',' << '\"' << getParameterTextValueByName("skew_weight",detail) << '\"'
	<< ',' << '\"' << getParameterTextValueByName("fractionRead",detail) << '\"'
	<< ',' << '\"' << getParameterTextValueByName("dedupe",detail) << '\"';

	if (std::string("1") == getParameterTextValueByName("fractionRead",detail)  || std::string("1") == getParameterTextValueByName("dedupe",detail))
	{
	    o << ',' << "\"N/A\"";
	}
	else
	{
	    o << ',' << '\"' << getParameterTextValueByName("dedupe_method",detail) << '\"';
	}

	o << ',' << '\"' << getParameterTextValueByName("duplicate_set_size",detail) << '\"';

	if (std::string("1") == getParameterTextValueByName("fractionRead",detail))
	{
	    o << ',' << "\"N/A\"";
	    o << ',' << "\"N/A\"";
	}
	else
	{
	    o << ',' << '\"' << getParameterTextValueByName("pattern",detail) << '\"';
	    o << ',' << '\"' << getParameterTextValueByName("compressibility",detail) << '\"';
	}

	o
	<< ',' << '\"' << getParameterTextValueByName("RangeStart",detail) << '\"'
	<< ',' << '\"' << getParameterTextValueByName("RangeEnd",detail) << '\"'
	<< ',' << '\"' << getParameterTextValueByName("SeqStartPoint",detail) << '\"'
	;

	return o.str();

}


std::pair<bool,std::string>	IosequencerInputRollup::add(std::string parameterNameEqualsTextValueCommaSeparatedList)
{

	bool wellFormedInput{true};

	count++;

	if (0==parameterNameEqualsTextValueCommaSeparatedList.length())
	{
		return std::make_pair(false,"IosequencerInputRollup::add() called with null list of parameter name = value");
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

ivy_float IosequencerInputRollup::get_Total_IOPS_Setting(unsigned int subinterval_count)
{
	std::map<std::string,std::map<std::string,long int>>::iterator IOPS_it = values_seen.find(toUpper("IOPS"));

	if (values_seen.end() == IOPS_it) { return 0.0; }

	std::map<std::string /* metric value */, long int /* count */>& value_map = IOPS_it->second;

	if (value_map.find("max") != value_map.end()) return -1;

    ivy_float total {0.0};

    for (const auto& pear : value_map)
    {
        if (!regex_match(pear.first,float_number_regex))
        {
            std::string e = "<Error> internal programming error - IosequencerInputRollup::get_Total_IOPS_Setting() - IOPS setting \""s + pear.first
                + "\" is not \"max\" and does not look like a floating point number."s;
            std::cout << std::endl << e << std::endl << std::endl;
            throw std::runtime_error(e);
        }
        {
            std::istringstream is(pear.first);
            ivy_float v;
            is >> v;
            total += v * ((ivy_float) pear.second);
        }
    }
    return total / ((ivy_float) subinterval_count);
}


std::string IosequencerInputRollup::Total_IOPS_Setting_toString(const ivy_float v)
{
    if ( v == -1.0 ) return "max"s;
    {
        std::ostringstream o;
        o << std::fixed << std::setprecision(2) << v;
        return o.str();
    }
}


bool IosequencerInputRollup::is_only_IOPS_max()
{
    auto it = values_seen.find("IOPS"s);

    if (it == values_seen.end()) { return false; }

    if (it->second.size() != 1 /* must be only one value */) { return false; }

    for (auto pear : it->second) { if (stringCaseInsensitiveEquality(pear.first, "max"s)) { return true; } }

    return false;
}
















