//Copyright (c) 2016-2020 Hitachi Vantara Corporation
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

#include "ivyhelpers.h"

using namespace std;
//
//
//std::map<unsigned int, std::vector<unsigned int>> get_processors_by_core()
//{
////    # cat /proc/cpuinfo | grep 'processor\|core id'
////    processor	: 0
////    core id		: 0
////    processor	: 1
////    core id		: 1
////    processor	: 2
////    core id		: 2
////    processor	: 3
////    core id		: 3
////    processor	: 4
////    core id		: 0
////    processor	: 5
////    core id		: 1
////    processor	: 6
////    core id		: 2
////    processor	: 7
////    core id		: 3
////    processor	: 8
////    core id		: 0
////    processor	: 9
////    core id		: 1
////    processor	: 10
////    core id		: 2
////    processor	: 11
////    core id		: 3
////    processor	: 12
////    core id		: 0
////    processor	: 13
////    core id		: 1
////    processor	: 14
////    core id		: 2
////    processor	: 15
////    core id		: 3
//
//    std::string command {  R"(cat /proc/cpuinfo | grep 'processor\|core id')"  };
//
//    std::string command_output = GetStdoutFromCommand(command);
//
//    std::istringstream iss(command_output);
//
//    std::string line;
//
//    unsigned int processor {0}, core_id {0};
//
//    std::regex processor_regex(   R"(processor[ \t]*:[ \t]*([0-9]+))"  );
//    std::regex core_id_regex  (   R"(core id[ \t]*:[ \t]*([0-9]+))"    );
//
//    std::smatch     smash;
//    std::ssub_match subm;
//
//    std::map<unsigned int, std::vector<unsigned int>> processors_by_core {};
//
//    while (std::getline(iss, line))
//    {
//        if (std::regex_match(line, smash, processor_regex))
//        {
//            subm = smash[1];
//            std::string number = subm.str();
//            {
//                std::istringstream iss_num(number);
//                iss_num >> processor;
//            }
//        }
//        else if (std::regex_match(line, smash, core_id_regex))
//        {
//            subm = smash[1];
//            std::string number = subm.str();
//            {
//                std::istringstream iss_num(number);
//                iss_num >> core_id;
//            }
//            processors_by_core[core_id].push_back(processor);
//        }
//    }
//    return processors_by_core;
//}
//

int main()
{
    for (auto& v : get_processors_by_core())
    {
        std::cout << "core " << v.first << " has hyperthreads ";

        for (auto& p : v.second)
        {
            std::cout << p << "  ";
        }
        std::cout << std::endl;
    }

    return 0;
}
