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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.

#include "ivytypes.h"
#include "RunningStat.h"

void RunningStat_double_long_int::clear()
{
	n=0;
	min_value=max_value=M1=M2=0.;
}

bool RunningStat_double_long_int::fromString(const std::string& s)
{
    if (sizeof(RunningStat_double_long_int) != sizeof(RunningStat<double, long int>))
    {
        std::ostringstream o;
        o << "<Error> internal programming error - RunningStat_double_long_int::fromString() - sizeof(RunningStat_double_long_int) = " << sizeof(RunningStat_double_long_int)
            << " is different from sizeof(RunningStat<double, long int>) = " << sizeof(RunningStat<double, long int>) << "." << std::endl;
        std::cout << o.str();
        throw runtime_error(o.str());
    }

    if (5 != sscanf(s.c_str(),"<%li;%lf;%lf;%lf;%lf>",&n,&M1,&M2,&min_value,&max_value)) { clear(); return false; }

	return true;
}


std::string RunningStat_double_long_int::toString()
{
    char buf[256];

    int rc = snprintf( buf, sizeof(buf), "<%li;%lg;%lg;%lg;%lg>", n, M1, M2, min_value, max_value);

    if (rc < 0 || rc >= (int) sizeof(buf))
    {
        std::ostringstream o;
        o << "<Error> avgcpubusypercent::toString() - internal programming error bad return code " << rc << " from snprintf(),";
        throw std::runtime_error(o.str());
    }

    return buf;  // convert to std::string	f
}
