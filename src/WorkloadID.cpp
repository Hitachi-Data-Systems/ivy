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
#include "WorkloadID.h"

bool WorkloadID::set(const std::string& ID)
{
	workloadID=ID;
	ivyscript_hostname.clear();
	LUN_name.clear();
	workload_name.clear();
	isWellFormed=false;

	if (0 == ID.length()) { return isWellFormed; }

	unsigned int cursor=0;

	while ('+' != ID[cursor])
	{
		ivyscript_hostname.push_back(ID[cursor]);
		cursor++;
		if (cursor >= ID.length())  { return isWellFormed; }
	}

	cursor++;
	if (cursor >= ID.length())  { return isWellFormed; }

	while ('+' != ID[cursor])
	{
		LUN_name.push_back(ID[cursor]);
		cursor++;
		if (cursor >= ID.length())  { return isWellFormed; }
	}

	cursor++;
	if (cursor >= ID.length())  { return isWellFormed; }

	while ('+' != ID[cursor])
	{
		workload_name.push_back(ID[cursor]);
		cursor++;
		if (cursor >= ID.length())
		{
			if (ivyscript_hostname.length() > 0 && LUN_name.length()>0 && workload_name.length() > 0)
			{
				isWellFormed = true;
			}
			return isWellFormed;
		}
	}

	isWellFormed = false;
	return isWellFormed;
}

std::string WorkloadID::getHostPart() const
{
	if (isWellFormed) return ivyscript_hostname;
	else return std::string("WorkloadID::getHostPart() - WorkloadID is not well-formed - \"") + workloadID + std::string("\"");
}

std::string WorkloadID::getLunPart() const
{
	if (isWellFormed) return LUN_name;
	else return std::string("WorkloadID::getLunPart() - WorkloadID is not well-formed - \"") + workloadID + std::string("\"");
}

std::string WorkloadID::getWorkloadPart() const
{
	if (isWellFormed) return workload_name;
	else return std::string("WorkloadID::getWorkloadPart() - WorkloadID is not well-formed - \"") + workloadID + std::string("\"");
}

std::string WorkloadID::getHostLunPart() const
{
	if (isWellFormed) return ivyscript_hostname + std::string("+") + LUN_name;
	else return std::string("WorkloadID::getHostLun() - WorkloadID is not well-formed - \"") + workloadID + std::string("\"");
}


std::ostream& operator<< (std::ostream& o, const WorkloadID& wID)
{
    o << wID.workloadID;
    return o;
}

















