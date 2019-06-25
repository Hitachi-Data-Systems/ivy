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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>, Stephen Morgan <stephen.morgan@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.

#include <iostream>
#include <iomanip>
#include <sstream>

#include "ivyhelpers.h"
#include "dedupe_method.h"

std::string valid_dedupe_methods() {return ("Valid dedupe_method values are \"serpentine\", \"target_spread\", and \"constant_ratio\".");}

dedupe_method parse_dedupe_method(std::string s)
{
    trim(s);

    if (stringCaseInsensitiveEquality(s,std::string("serpentine")))      { return dedupe_method::serpentine; }
    if (stringCaseInsensitiveEquality(s,std::string("target_spread")))   { return dedupe_method::target_spread; }
    if (stringCaseInsensitiveEquality(s,std::string("constant_ratio")))  { return dedupe_method::constant_ratio; }
    if (stringCaseInsensitiveEquality(s,std::string("static")))          { return dedupe_method::static_method; }

    return dedupe_method::invalid;
}

std::string dedupe_method_to_string(dedupe_method dm)
{
    switch (dm)
    {
        case dedupe_method::serpentine:      return "serpentine";
        case dedupe_method::target_spread:   return "target_spread";
        case dedupe_method::constant_ratio:  return "constant_ratio";
        case dedupe_method::static_method:   return "static";
        case dedupe_method::invalid:
        default:
        {
            std::ostringstream o;
            o << "<Error> invalid dedupe_method 0x" << std::hex << std::setw(2) << std::setfill('0') << ((unsigned int) dm) << ".";
            return o.str();
        }
    }
}


