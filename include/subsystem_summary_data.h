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

#include <map>

#include "ivydefines.h"
#include "RunningStat.h"

class ivy_engine;

class subsystem_summary_data
{
    public:

    // variables

    std::map
    <
        std::string, // element,  e.g. MP_core
        std::map
        <
            std::string, // metric, e.g. busy_percent
            RunningStat<ivy_float, ivy_int>
        >
    > data;

    ivy_float repetition_factor {1.0};
    // addIn() adds the repetition factor of the other thing you are adding in.
    // Thus if you look at the sum() function, you need to divide sum values by the repetition factor.

    bool detailed_thumbnail {false};


    // methods
    subsystem_summary_data(){};
    void addIn(const subsystem_summary_data&);
    static std::string csvHeadersPartOne(std::string np = std::string(""));
    static std::string csvHeadersPartTwo(std::string np = std::string(""));
    std::string csvValuesPartOne(unsigned int divide_count_by = 1);
    std::string csvValuesPartTwo(unsigned int divide_count_by = 1);
    std::string thumbnail() const;
    void derive_metrics();
    ivy_float IOPS();  // these three methods return -1.0 if no data.
    ivy_float decimal_MB_per_second();
    ivy_float service_time_ms();
};
