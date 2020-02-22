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
#include "WorkloadTrackerPointerList.h"

std::string WorkloadTrackerPointerList::toString()
{
	std::ostringstream o;

	o << "WorkloadTrackerPointerList object contains " << workloadTrackerPointers.size() << " pointers." << std::endl;

	int i=0;
	for (auto& pWorkloadTracker : workloadTrackerPointers)
	{
		o << "WorkloadTrackerPointerList #" << i++ << " -> " << pWorkloadTracker->toString() << std::endl;
	}

	return o.str();
}
