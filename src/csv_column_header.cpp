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

#include "ivytypes.h"
#include "csvfile.h"
#include "ivyhelpers.h"

extern std::string csv_usage ;

int main(int argc,char* argv[])
{
    if (3 != argc)
    {
        std::ostringstream o;
        o << "<Error> In \""; for (int i=0; i < argc; i++) { if (i>0) o << " "; o << argv[i]; } o << "\", "
            << argv[0] << " must be called with two arguments, the name of the csv file, and the column number.  Columns are numbered starting from 0." << std::endl;
        std::cout << o.str() << csv_usage << o.str();
        return -1;
    }

    std::string csv_filename(argv[1]);

    csvfile seesv;

    auto rv = seesv.load(csv_filename);

    if (!rv.first)
    {
        std::ostringstream o;
        o << "<Error> In \""; for (int i=0; i < argc; i++) { if (i>0) o << " "; o << argv[i]; } o << "\", "
            << rv.second;
        std::cout << o.str() << csv_usage << o.str();
        return -1;
    }

    std::string c {argv[2]};
    trim (c);
    if (!std::regex_match(c,digits_regex))
    {
        std::ostringstream o;
        o << "<Error> In \""; for (int i=0; i < argc; i++) { if (i>0) o << " "; o << argv[i]; } o << "\", "
            << " the column number \"" << argv[2] << "\" must be an unsigned integer (one or more digits) such as 5 or 0." << std::endl;
        std::cout << o.str() << csv_usage << o.str();
        return -1;
    }

    std::istringstream is {c};

    int column;
    is >> column;

    std::cout << seesv.column_header(column) << std::endl;

    return 0;
}
