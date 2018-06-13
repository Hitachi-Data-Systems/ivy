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
#include <iostream>

#include "OverloadSet.h"

OverloadTable builtin_overloads;

std::ostream& operator<<(std::ostream& os,const OverloadSet& overload_set)
{
    os << "{";
    bool first_pass{true};
    for (auto& o : overload_set)
    {
        if (!first_pass) os << ",";
        first_pass=false;
        os << " \"" << o.second << "\"";

    }
    if (!first_pass) os << " ";
    os << "}";

    return os;
}

std::ostream& operator<<(std::ostream& os,const OverloadTable& ot)
{
    os << "{";
    bool table_empty{true};
    for (auto& o : ot)
    {
        if (!table_empty) os << ',';
        table_empty=false;
        os << " \"" << o.first << "\" -> " << o.second;
    }
    os << "}";

    return os;
}

