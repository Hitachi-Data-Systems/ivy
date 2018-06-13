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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>, Kumaran Subramaniam <kumaran.subramaniam@hitachivantara.com>
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

#include "ivyhelpers.h"

#include "LUN.h"

void LUN::createNicknames()
{
	if (contains_attribute_name(std::string("ivyscript_hostname")))
	{
		attributes[std::string("host")] = attributes[std::string("ivyscript_hostname")];
	}

	if (contains_attribute_name(std::string("pg")))
	{
		rewritePG(attributes[std::string("pg")]);

	}

	attributes[std::string("all")] = std::string("all");  // I can't remember why I was doing this - maybe to make an automatic all=all rollup??? maybe remove later

	attributes[std::string("workload")] = std::string("<placeholder>");
	attributes[std::string("workloadid")] = std::string("<placeholder>");
		// We put these in here so all LUNs have same attribute names so we don't have to worry about which LUN to use as TheSampleLUN.
		// Later on, in the "workloadLUN" LUN instance that is allocated in the WorkloadTracker and that is a clone of of the discovered LUN,
		// we will set the workloadid and workload attributes in that LUN instance in the workload tracker, and then using
		// WorkloadTrackerPointerList objects, we can apply selects, such as selects for rollup instances, where we may wish to select on workload name.
		// To get a csv file for each workload thread, say "[CreateRollup] workloadID".
// NEED TO CHECK IF the csv file names are sanitized - well it oughta blow with no such directory if we embed /dev/sdxx in the filename ...

}

bool LUN::loadcsvline(std::string headerline, std::string dataline, std::string logfilename)
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
			fileappend(logfilename, o.str());
			return false;
		}
	}

	attributes.clear();

	std::string keyword, value;

	for (int col=0; col < header_columns; col++)
	{
		keyword =
			convert_to_lower_case_and_convert_nonalphameric_to_underscore
			(
				UnwrapCSVcolumn(retrieveRawCSVcolumn(headerline, col))
			);
		value = UnwrapCSVcolumn(retrieveRawCSVcolumn(dataline, col));
		attributes[keyword] = value;
	}
	return true;
}

std::string LUN::toString()
{
	std::string result;
	bool notfirst = false;

	std::ostringstream o;

	o << "{";
	for (auto& pear : attributes)
	{
		if (notfirst) o << ", ";
		notfirst=true;
		o << pear.first << " = \"" << pear.second << "\"";
	}
	o << "}";
	return o.str();
}

std::string LUN::attribute_value(std::string attribute_name)
{
	std::map<std::string, std::string>::iterator it;

	std::string atname = LUN::convert_to_lower_case_and_convert_nonalphameric_to_underscore(UnwrapCSVcolumn(attribute_name));

	it = attributes.find(atname);

	if (it == attributes.end()) return std::string("No_such_attribute");

	return (*it).second;
}

bool LUN::contains_attribute_name(std::string attribute_name)
{
	std::map<std::string, std::string>::iterator it;

	std::string atname = LUN::convert_to_lower_case_and_convert_nonalphameric_to_underscore(UnwrapCSVcolumn(attribute_name));

	it = attributes.find(atname);

	if (it == attributes.end()) return false;
	else return true;
}


void LUN::set_attribute(std::string attribute_name, std::string value)
{
	std::string name = LUN::convert_to_lower_case_and_convert_nonalphameric_to_underscore(UnwrapCSVcolumn(attribute_name));

	if (0 == name.size())
	{
		std::stringstream o;
		o << "<Error> LUN::set_attribute(attribute_name=\"" << attribute_name << "\", value = \"" << value << "\") - attribute_name may not be null.";
		throw std::invalid_argument(o.str());
	}

	attributes[name] = value;

	return;
}


bool LUN::attribute_value_matches(std::string attribute_name, std::string value)
{
	std::map<std::string, std::string>::iterator it;

	std::string atname = LUN::convert_to_lower_case_and_convert_nonalphameric_to_underscore(UnwrapCSVcolumn(attribute_name));

	it = attributes.find(atname);

	if (it == attributes.end()) return false;

	return stringCaseInsensitiveEquality(UnwrapCSVcolumn(value),(*it).second);
}

std::string LUN::convert_to_lower_case_and_convert_nonalphameric_to_underscore(std::string column_title)
{
	if (0 == column_title.length())	return std::string("");

	std::string result;
	char c;

	for (unsigned int i=0; i < column_title.length(); i++)
	{
		c = tolower(column_title[i]);
		if (!isalnum(c)) c = '_';
		result.push_back(c);
	}

	return result;
}

void LUN::copyOntoMe(LUN* p_other)
{
	for (auto& pear : p_other->attributes)
	{
		attributes[pear.first] = pear.second;
	}
	return;
}
