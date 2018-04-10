
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
#include <sstream>

#include "csvfile.h"

extern std::string csv_usage ;

int main(int argc,char* argv[])
{
    std::string executable_name {argv[0]};

    if (1 != argc)
    {
        std::cout << "<Error> " << executable_name << " was called with " << (argc-1) << " arguments." << std::endl;
        std::cout << executable_name << " must be called with no arguments. input is from stdin, output is to stdout." << std::endl;
        return -1;
    }

    csvfile seesv {};

    auto rv = seesv.load(std::string(""));  // reads from stdin

    if (!rv.first)
    {
        std::ostringstream o;
        o << "<Error> " << rv.second;
        return -1;
    }

    seesv.remove_empty_columns();

    std::cout << seesv;

    return 0;
}


