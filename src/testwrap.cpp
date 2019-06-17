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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#include <iostream>
#include <list>

#include "ivyhelpers.h"

int main (int arc, char* argv[])
{

    std::list<std::string> input
    {
        "", ",", "a,", ",a", "a", "1", "1.3E-5", "1.5%",
        "\"a\"", "\"a", "a\"",
        ",,,"
        " , , , "
        "a,bee,sea,5,  56,1.45E-03,henrey,2,",
        "\",\",",
        "eh,bee,cee",
        "eh,Be,see2,defg"
    };

    for (auto& s : input)
    {
        std::cout
            << "for input string   \"" << s << "\" - " << countCSVlineUnquotedCommas(s) << std::endl
            << "wrapped up you get \"" << quote_wrap_csvline_except_numbers(s) << "\" - " << countCSVlineUnquotedCommas(quote_wrap_csvline_except_numbers(s))  << std::endl << std::endl;
    }
}
