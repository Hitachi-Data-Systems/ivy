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
#include "pattern.h"

std::string valid_patterns() {return ("Valid pattern values are \"random\", \"gobbledegook\", \"ascii\", \"trailing_zeros\", \"zeros\", and \"all_0xFF\".");}

pattern parse_pattern(std::string s)
{
    trim(s);
    if (stringCaseInsensitiveEquality(s,std::string("random")))         { return pattern::random; }

    if (normalized_identifier_equality(s,std::string("gobbledegook")))   { return pattern::gobbledegook;}

    if (stringCaseInsensitiveEquality(s,std::string("ascii")))          { return pattern::ascii; }

    if (normalized_identifier_equality(s,std::string("trailing_zeros"))) { return pattern::trailing_zeros; }

    if (normalized_identifier_equality(s,std::string("zeros"))) { return pattern::zeros; }

    if (normalized_identifier_equality(s,std::string("all_0x0F"))) { return pattern::all_0x0F; }

    if (normalized_identifier_equality(s,std::string("all_0xFF"))) { return pattern::all_0xFF; }

    return pattern::invalid;
}

std::string pattern_to_string(pattern p)
{
    switch (p)
    {
        case pattern::ascii: return "ascii";
        case pattern::gobbledegook: return "gobbledegook";
        case pattern::random: return "random";
        case pattern::trailing_zeros: return "trailing_zeros";
        case pattern::zeros: return "zeros";
        case pattern::all_0x0F: return "all_0x0F";
        case pattern::all_0xFF: return "all_0xFF";
        case pattern::invalid:
        default:
        {
            std::ostringstream o;
            o << "<Error> invalid pattern type 0x" << std::hex << std::setw(2) << std::setfill('0') << ((unsigned int) p) << ".";
            return o.str();
        }
    }
}
