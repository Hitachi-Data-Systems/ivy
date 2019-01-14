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
#pragma once

#include <list>
#include <string>

class WorkloadID;

class ListOfWorkloadIDs
{
	// this makes it easy to send a list of WorkloadIDs in text form over the ivymaster-ivydriver pipe.
private:
	bool valid; // retains return code from last call to fromString()

public:
	std::list<WorkloadID> workloadIDs;

//methods
	bool isValid() {return valid;}
	std::string toString();  // '<', a list of WorkloadIDs separated by commas, and '>'
	bool fromString(std::string*); // true if the string parses as '<', a list of well-formed WorkloadIDs separated by commas, and '>'
	void addAtEndAllFromOther(ListOfWorkloadIDs& other);
	void clear() {workloadIDs.clear();}
};

