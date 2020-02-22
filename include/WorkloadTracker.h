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
#pragma once

#include "ivytypes.h"
#include "Iosequencer.h"

class WorkloadTracker
{
public:
	// this ivymaster object is used to map a workload ID to the attributes of the underlying LUN
//variables
	WorkloadID workloadID;
	LUN workloadLUN; // this is a copy of the underlying LUN, but with the attribute "workload" set to the WorkloadID
	IosequencerInput wT_IosequencerInput;
		// This local copy of the most up to date version of the iosequencer_input object running
		// (or to be running momentarily) remotely has two main jobs in life
			// - Serve as a test bed for every parameter change update so that we can verify
			//   that they apply cleanly to the iosequencer_input object for all target workload
			//   before we start sending out the update to ivydriver remotes hosts.

			// - Enable commands changing parameter values relative to their current values.
			//   "increase IOPS by 20%
	// workloadLUN is also there for you to stick in any attributes of a LUN that you would like to be able to select on in future

//methods
	WorkloadTracker(std::string ID, std::string iosequencer_name, LUN* pL);
	std::string toString();
};


