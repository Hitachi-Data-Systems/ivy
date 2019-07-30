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
//Author: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.

#include <iostream>
#include <string>
#include <sstream>

#include "nestedit.h"
#include "ivy_engine.h"

void nestedit::clear()
{
    loop_levels.clear();
    current_level = 0;
}

bool nestedit::run_iteration()
{
    if (loop_levels.size() == 0)
    {
        if (current_level == 0)
        {
            current_level++;
            return true;
        }
        return false;
    }

    while (current_level > 0 && (loop_levels[current_level].current_index >= loop_levels[current_level].values.size()))
    {
        current_level--;
    }

    if (current_level == 0 && loop_levels[0].current_index >= loop_levels[0].values.size()) { return false; }

    // we know that at the current level, the current index points to a value that we are going to use.
    {
        std::ostringstream edit_rollup_text;

        {
            loop_level& ll = loop_levels[current_level];

            edit_rollup_text << ll.attribute << " = \"" << ll.values[ll.current_index] << "\"";

            ll.current_index++;
        }

        while (current_level < (loop_levels.size() - 1))
        {
            current_level++;
            {
                loop_level& ll = loop_levels[current_level];

                ll.current_index = 0;

                edit_rollup_text << ", " <<  ll.attribute << " = \"" << ll.values[ll.current_index] << "\"";

                ll.current_index = 1;
            }
        }

        auto rc = m_s.edit_rollup("all=all", edit_rollup_text.str(), false);
        if (!rc.first)
        {
            std::ostringstream o;
            o << "<Error> go() - when iterating over workload parameters, edit_rollup(\"all=all\", \"" << edit_rollup_text.str() << "\") failed - " << rc.second << std::endl;
            std::cout << o.str();
            log (m_s.masterlogfile,o.str());
            m_s.kill_subthreads_and_exit();
        }
    }

    return true;
}
