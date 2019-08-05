//Copyright (c) 2019 Hitachi Vantara Corporation
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
//Author: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.

#pragma once

#include <string>
#include <vector>

#include "ivydefines.h"

struct loop_level
{
    unsigned int             current_index {0}; // if >= values.size(), we are done this pass through values.
    std::string              attribute     {};
    std::vector<std::string> values        {};
    ivy_float                max_IOPS      {-1.0}; // used with atribute = "IOPS_curve".
    std::string              stepname_suffix {};

    void set_stepname_suffix();
};

struct nestedit
{
    unsigned int            current_level {0};
    std::vector<loop_level> loop_levels;

//methods
    void clear();
    bool run_iteration();    // returns false when we have already run last iteration.

    std::string build_stepname_suffix();
};
