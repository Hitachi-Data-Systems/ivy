//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
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





