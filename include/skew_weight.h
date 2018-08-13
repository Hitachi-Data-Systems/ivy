//Copyright (c) 2018 Hitachi Vantara Corporation
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

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <map>

#include "ivydefines.h"
#include "WorkloadID.h"
#include "ListOfWorkloadIDs.h"

struct skew_LUN
{
//variables

    std::map<std::string /*WorkloadID*/, ivy_float> workload_skew_weight;


//methods
	skew_LUN();

    void clear();

    ivy_float skew_total();

    void push(const std::string&,ivy_float);

    ivy_float normalized_skew_factor(const std::string&);

    std::string toString(std::string indent="");
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////


struct skew_data
{
//variables
    std::map<std::string,skew_LUN> LUNs;

//methods
    void clear();

    void push(const WorkloadID&, ivy_float /* skew_weight */);

    ivy_float fraction_of_total_IOPS(const std::string& wID);

    std::string toString();

};
