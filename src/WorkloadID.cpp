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
#include <sstream>

#include "WorkloadID.h"


bool WorkloadID::set(std::string ID)
{
	workloadID=ID;

	if (0 == ID.length()) { isWellFormed = false; return isWellFormed; }

	unsigned int cursor=0;

	while ('+' != ID[cursor])
	{
		ivyscript_hostname.push_back(ID[cursor]);
		cursor++;
		if (cursor >= ID.length())  { isWellFormed = false; return isWellFormed; }
	}

	cursor++;
	if (cursor >= ID.length())  { isWellFormed = false; return isWellFormed; }

	while ('+' != ID[cursor])
	{
		LUN_name.push_back(ID[cursor]);
		cursor++;
		if (cursor >= ID.length())  { isWellFormed = false; return isWellFormed; }
	}

	cursor++;
	if (cursor >= ID.length())  { isWellFormed = false; return isWellFormed; }

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
			else
			{
				isWellFormed = false;
			}
			return isWellFormed;

		}
	}

	isWellFormed = false;
	return isWellFormed;
}

std::string WorkloadID::getHostPart()
{
	if (isWellFormed) return ivyscript_hostname;
	else return std::string("WorkloadID::getHostPart() - WorkloadID is not well-formed - \"") + workloadID + std::string("\"");
}

std::string WorkloadID::getLunPart()
{
	if (isWellFormed) return LUN_name;
	else return std::string("WorkloadID::getLunPart() - WorkloadID is not well-formed - \"") + workloadID + std::string("\"");
}

std::string WorkloadID::getWorkloadPart()
{
	if (isWellFormed) return workload_name;
	else return std::string("WorkloadID::getWorkloadPart() - WorkloadID is not well-formed - \"") + workloadID + std::string("\"");
}





