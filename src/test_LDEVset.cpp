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

#include <iostream>
#include <vector>

#include "LDEVset.h"

int main(int argc, char* argv[])
{
    std::vector<std::string> v
    {
        "00:00",
        "01:00-01:03, 01:05 - 01:07 00:FF",
        "01:00-01:03, 01:05 - 01:07 00:Fe",
        "01:00-01:03,,,, 01:05 - 01:07 01:FF",
        "01:00-01:03     01:05 - 01:07 01:FF",
        "01:00-01:03;;;01:05 - 01:07 01:FF",
        " 00:FF 01:00-01:03, 01:05 - 01:07",
        " 00:FF, 01:00-01:03, 01:05 - 01:07",
        " 00:Fe, 01:00-01:03, 01:05 - 01:07"
    };

    for (auto s : v)
    {
        LDEVset lset;
        lset.add(s,"/home/ivogelesang/Desktop/eraseme/test_LDEVset.log.txt");
        std::cout << "For input string \"" << s << "\"," << std::endl
        << "we got the LDEVset \"" << lset.toString() << "\"." << std::endl << std::endl;

    }







    return 0;
}
