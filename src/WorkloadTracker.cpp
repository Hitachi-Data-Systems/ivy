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
#include "ivyhelpers.h"
#include "WorkloadTracker.h"

WorkloadTracker::WorkloadTracker(std::string ID, std::string iosequencer_name, LUN* pL) : workloadID(ID)
{
	workloadLUN.copyOntoMe(pL);
	workloadLUN.set_attribute("WorkloadID"s, ID);
	workloadLUN.set_attribute("iosequencer_name"s, iosequencer_name);

	WorkloadID workloadID(ID);
	if (workloadID.isWellFormed)
	{
		workloadLUN.set_attribute("workload"s, workloadID.getWorkloadPart());
	}
	else
	{
		std::ostringstream o; o << "<WorkloadTracker::WorkloadTracker() - internal programming error - workloadID \"" << ID << "\" is malformed.";
		workloadLUN.set_attribute("Workload"s, o.str());
	}

	return;
}

std::string WorkloadTracker::toString()
{
	std::ostringstream o;
	o << "WorkloadTracker workloadID = \"" << workloadID << "\", LUN = " << workloadLUN.toString();
	return o.str();
}




