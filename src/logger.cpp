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

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "logger.h"

void log(logger& bunyan, const std::string& s)
{
	std::unique_lock<std::mutex> m_lk(bunyan.now_speaks);

	ivytime now; now.setToNow();

    std::ostream* p_ostream = &std::cout;

    std::ofstream o;

    bool is_ofstream = bunyan.logfilename != "<none>"s;

    if (is_ofstream)
    {
        o.open(bunyan.logfilename, ios::out | ios::app );

        if (o.fail())
        {
            std::ostringstream o;
            o << "<Error> internal programming error - logger::log() failed trying to open \"" << bunyan.logfilename << "\" for output+append to say \"" << s << "\".";
            throw std::runtime_error(o.str());
        }
        p_ostream = &o;
    }

    (*p_ostream) << now.format_as_datetime_with_ns();

    if (bunyan.last_time != ivytime_zero)
    {
        (*p_ostream) << " +" << ivytime(now-bunyan.last_time).format_as_duration_HMMSSns();
    }
    else
    {
        (*p_ostream) << " +" << ivytime_zero.format_as_duration_HMMSSns();
    }

    (*p_ostream) << " "<< s;

	if (s.length() > 0 && '\n' != s[s.length()-1]) (*p_ostream) << std::endl;

	(*p_ostream).flush();

    if (is_ofstream) o.close();

    ivytime after; after.setToNow();

    bunyan.last_time = now;
    bunyan.durations.push(ivytime(after-now).getlongdoubleseconds());

    m_lk.unlock();

    return;
}

