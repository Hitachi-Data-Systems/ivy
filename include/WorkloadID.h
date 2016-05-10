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
#pragma once

class WorkloadID
{
public:
// variables
	std::string workloadID, ivyscript_hostname, LUN_name, workload_name;
	bool isWellFormed {false};  // set() turns this on if ID looks like "sun159+/dev/sdxy+charlie"
		// well, at least exactly 2 "+" signs and something non-blank on either side of them.

// methods
	bool set(std::string workloadID); // true if workloadID is well-formed.
	WorkloadID (std::string ID) { set(ID); return; }
	WorkloadID(){}
	std::string getHostPart();
	std::string getLunPart();
	std::string getWorkloadPart();
};



