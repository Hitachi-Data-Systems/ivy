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

#include "ivytypes.h"
#include "ivyhelpers.h"
#include "ivy_engine.h"

bool running_IOPS_curve_IOPS_equals_max {false};

ivy_float* p_IOPS_curve_max_IOPS {nullptr};


void loop_level::set_stepname_suffix()
{
    if (attribute == "IOPS_curve")
    {
        if (current_index == 0)
        {
            stepname_suffix = "IOPS=max";
        }
        else
        {
            ivy_float fraction = number_optional_trailing_percent(values[current_index],"<Error> internal programming error - parsing IOPS_curve valude in loop_level::set_stepname_suffix()");
            std::ostringstream o;
            o << std::fixed << std::setprecision(0) << (100.0 * fraction) << "% of max IOPS";
            stepname_suffix = o.str();
        }
    }
    else
    {
        std::ostringstream o;
        o << attribute << "=" << values[current_index];
        stepname_suffix = o.str();
    }

    return;
}

void nestedit::clear()
{
    loop_levels.clear();
    current_level = 0;

    running_IOPS_curve_IOPS_equals_max = false;
    p_IOPS_curve_max_IOPS = nullptr;
}

bool nestedit::run_iteration()
{
    running_IOPS_curve_IOPS_equals_max = false;

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

    if (current_level == 0 && loop_levels[0].current_index >= loop_levels[0].values.size())
    {
        return false;
    }

    // we know that at the current level, the current index points to a value that we are going to use.


    {
        std::ostringstream edit_rollup_text;

        {
            loop_level& ll = loop_levels[current_level];

            if (ll.attribute == "IOPS_curve")
            {
                if (ll.current_index == 0)
                {
                    running_IOPS_curve_IOPS_equals_max = true;
                    p_IOPS_curve_max_IOPS = & ll.max_IOPS;
                    edit_rollup_text << "IOPS=max";
                }
                else
                {
                    ivy_float fraction = number_optional_trailing_percent(ll.values[ll.current_index],"<Error> internal programming error - parsing IOPS_curve valude in nestedit::run_iteration");
                    ivy_float iops_this_pass = ll.max_IOPS * fraction;
                    edit_rollup_text << "Total_IOPS=" << iops_this_pass;
                }
            }
            else
            {
                edit_rollup_text << ll.attribute << " = \"" << ll.values[ll.current_index] << "\"";
                {
                    std::ostringstream o;
                    o << ll.attribute << "=" << ll.values[ll.current_index];
                    ll.stepname_suffix = o.str();
                }
            }

            ll.set_stepname_suffix();

            ll.current_index++;
        }

        while (current_level < (loop_levels.size() - 1))
        {
            current_level++;
            {
                loop_level& ll = loop_levels[current_level];

                ll.current_index = 0;

                if (ll.attribute=="IOPS_curve")
                {
                    edit_rollup_text << ", IOPS=max";
                    running_IOPS_curve_IOPS_equals_max = true;
                    p_IOPS_curve_max_IOPS = & ll.max_IOPS;
                }
                else
                {
                    edit_rollup_text << ", " <<  ll.attribute << " = \"" << ll.values[ll.current_index] << "\"";
                }


                ll.set_stepname_suffix();

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


std::string nestedit::build_stepname_suffix()
{
    std::ostringstream o;

    bool need_plus {false};

    for (auto& ll : loop_levels)
    {
        if (need_plus) o << " + ";
        need_plus = true;
        o << ll.stepname_suffix;
    }

    return o.str();
}
