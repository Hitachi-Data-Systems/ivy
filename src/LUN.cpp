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
//hostname,SCSI Bus Number (HBA),LUN Name,Hitachi Product,HDS Product,Serial Number,Port,LUN 0-255,LDEV,Nickname,LDEV type,RAID level,PG,Pool ID,CLPR,Max LBA,Size MB,Size MiB,Size GB,Size GiB,SizeTB,Size TiB,Vendor,Product,Product Revision,SCSI_IOCTL_PROBE_HOST
//cb23,2,/dev/sdbu,HM700,HUS VM,210030,1A,0,00:00,,Internal,RAID-1,01-01,,CLPR0,2097151,1073.741824,1024.000000,1.073742,1.000000,0.001074,0.000977,HITACHI ,OPEN-V          ,7303,EMC LPe12002-E 8Gb 2-port PCIe Fibre Channel Adapter on PCI bus 2e device 00 irq 16 port 0 Logical L
//172.17.19.159,1,/dev/sdb,DF800S,AMS2100,83011441,2A,4,0004,,,,,,,2247098367,1150514.364416,1097216.000000,1150.514364,1071.500000,1.150514,1.046387,HITACHI ,DF600F          ,0000,Emulex LPe11002-S 4Gb 2-port PCIe FC HBA on PCI bus 21 device 00 irq 59 port 0 Logical Link Speed: 4
//sun159,1,/dev/sdb,DF800S,AMS2100,83011441,2A,4,0004,,,,,,,2247098367,1150514.364416,1097216.000000,1150.514364,1071.500000,1.150514,1.046387,HITACHI ,DF600F          ,0000,Emulex LPe11002-S 4Gb 2-port PCIe FC HBA on PCI bus 21 device 00 irq 59 port 0 Logical Link Speed: 4

#include <cctype> // for tolower()
#include <iostream>
#include <sstream>
#include <list>
#include <algorithm>
#include <map>
#include <string>
using namespace std::string_literals;  // for std::string literals like "bork"s

#include "ivyhelpers.h"

#include "LUN.h"

void LUN::createNicknames()
{
	if (contains_attribute_name(std::string("ivyscript hostname"s)))
	{
		set_attribute("Host"s, attribute_value("ivyscript hostname"s));
	}

//	if (contains_attribute_name(std::string("PG")))
//	{
//		rewritePG(attributes["pg"s].second);
//	}

	set_attribute("all"s,"all"s);  // I can't remember why I was doing this - maybe to make an automatic all=all rollup??? maybe remove later

	set_attribute("Workload"s, "<placeholder>"s);
	set_attribute("WorkloadID"s, "<placeholder>"s);
		// We put these in here so all LUNs have same attribute names so we don't have to worry about which LUN to use as TheSampleLUN.
		// Later on, in the "workloadLUN" LUN instance that is allocated in the WorkloadTracker and that is a clone of of the discovered LUN,
		// we will set the workloadid and workload attributes in that LUN instance in the workload tracker, and then using
		// WorkloadTrackerPointerList objects, we can apply selects, such as selects for rollup instances, where we may wish to select on workload name.
		// To get a csv file for each workload thread, say "[CreateRollup] workloadID".
// NEED TO CHECK IF the csv file names are sanitized - well it oughta blow with no such directory if we embed /dev/sdxx in the filename ...

}

bool LUN::loadcsvline(const std::string& headerline, const std::string& dataline, logger& bunyan)
{
	// false on a failure to load the line properly
	int header_columns = 1 + countCSVlineUnquotedCommas(headerline);

	// Remind myself column title line up validation feature.

	// Makes sure the "SCSI Inquiry utility developer" updated the headers whenever the data column layout changed.

	// Failure to have the same number of columns in the headerline and the dataline triggers a loading failure.

	if ( countCSVlineUnquotedCommas(dataline) > countCSVlineUnquotedCommas(headerline) )
	{
		{
			std::ostringstream o;
			o << "<Error> LUN::loadcsvline() failed because a data line had more columns than the header line." << std::endl;
			o << "header columns = " << header_columns << " columns.  header: \"" << headerline << "\"." << std::endl;
			o << "data line columns = " << (1 + countCSVlineUnquotedCommas(dataline)) << " columns.  header: \"" << dataline << "\"." << std::endl;
			o << std::endl;
			log(bunyan, o.str());
			return false;
		}
	}

	attributes.clear();

	std::string original_attribute_name, normalized_name, value;

	for (int col=0; col < header_columns; col++)
	{
	    original_attribute_name = UnwrapCSVcolumn(retrieveRawCSVcolumn(headerline, col));

	    normalized_name = LUN::normalize_attribute_name(original_attribute_name);

		value = UnwrapCSVcolumn(retrieveRawCSVcolumn(dataline, col));

		attributes[normalized_name] = std::make_pair(original_attribute_name,value);
	}
	return true;
}

std::string LUN::toString() const
{
	std::string result;
	bool notfirst = false;

	std::ostringstream o;

	o << "{";
	for (auto& pear : attributes)
	{
		if (notfirst) o << ", ";
		notfirst=true;
		o << "\"" << pear.second.first << "\" = \"" << pear.second.second << "\"";
	}
	o << "}";
	return o.str();
}

std::string LUN::attribute_value(const std::string& attribute_name) const
{
	std::string atname = LUN::normalize_attribute_name(attribute_name);

	auto it = attributes.find(atname);

	if (it == attributes.end()) return "< LUN::attribute_value(\""s + attribute_name + "\") - no such_attribute>"s;

	return (*it).second.second;
}

bool LUN::contains_attribute_name(const std::string& attribute_name) const
{
	std::string atname = LUN::normalize_attribute_name(attribute_name);

	auto it = attributes.find(atname);

	if (it == attributes.end()) return false;
	else                        return true;
}


void LUN::set_attribute(const std::string& attribute_name, const std::string& value)
{
	std::string name = LUN::normalize_attribute_name(attribute_name);

	if (0 == name.size())
	{
		std::stringstream o;
		o << "<Error> LUN::set_attribute(attribute_name=\"" << attribute_name << "\", value = \"" << value << "\") - attribute_name may not be null.";
		throw std::invalid_argument(o.str());
	}

	if (contains_attribute_name(attribute_name))
	{
	    // we keep the original "original" name.
	    attributes[name].second = value;
    }
    else
    {
        std::pair<std::string,std::string>& pear = attributes[name];
        pear.first=attribute_name;
        pear.second = value;
    }

	return;
}


bool LUN::attribute_value_matches(const std::string& attribute_name, const std::string& value) const
{
	std::string atname = LUN::normalize_attribute_name(attribute_name);

	auto it = attributes.find(atname);

	if (it == attributes.end()) return false;

	return stringCaseInsensitiveEquality(value,(*it).second.second);
}

std::string LUN::normalize_attribute_name(const std::string& column_title)
{
	if (0 == column_title.length())	return std::string("");

	std::string result;
	char c;

	bool last_char_was_underscore {false};

	for (unsigned int i=0; i < column_title.length(); i++)
	{
		c = tolower(column_title[i]);
		if (!isalnum(c))
		{
		    if (!last_char_was_underscore) result.push_back('_');
		    last_char_was_underscore = true;
		}
		else
		{
		    result.push_back(c);
		    last_char_was_underscore = false;
		}
	}

	while (result.size() > 0 && result[result.size()-1] == '_') { result.erase(result.size()-1,1);}
	while (result.size() > 0 && result[0]               == '_') { result.erase(0,1);}

	return result;
}

void LUN::copyOntoMe(const LUN* p_other)
{
	for (auto& pear : p_other->attributes)
	{
		set_attribute(pear.second.first,pear.second.second);
	}
	return;
}

std::string LUN::original_name(const std::string& attribute_name) const
{
    std::string normalized_name = LUN::normalize_attribute_name(attribute_name);

    auto it = attributes.find(normalized_name);
    if (it != attributes.end())
    {
        return it->second.first;
    }

    {
        std::ostringstream o;
        o << "<No such attribute \"" << attribute_name << "\">";
        return o.str();
    }
}

std::string LUN::valid_attribute_names() const
{
    if (attributes.size() == 0) return "<Empty LUN - no attributes>";

    std::ostringstream o;

    o << "{ ";
    bool needComma = false;
    for (auto& pear : attributes)
    {
        if (needComma) o << ", ";
        needComma = true;
        o << "\"" << pear.second.first << "\"";
    }
    o << " }";
    return o.str();
}

void LUN::push_attribute_values(std::map<std::string /* LUN attribute name */, std::set<std::string> /* LUN attribute values */>& LUNattributeValues) const
{
    for (auto& pear : attributes)
    {
        LUNattributeValues[pear.second.first].insert(pear.second.second);
    }
    return;
}

void LUN::push_attribute_names(std::set<std::string>& column_headers) const
{
    for (auto& pear : attributes)
    {
        if (pear.first != "all"s)
        {
            column_headers.insert(pear.second.first);
        }

    }
    return;
}
