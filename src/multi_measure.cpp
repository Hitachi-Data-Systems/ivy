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

#include "ivy_engine.h"

ivy_float current_IOPS_staircase_total_IOPS;

void multi_measure_initialize_first()
{
    if (m_s.have_IOPS_staircase)
    {
        current_IOPS_staircase_total_IOPS = m_s.staircase_starting_IOPS;

        std::ostringstream o;
        o << "total_IOPS = " << std::fixed << std::setprecision(0) << current_IOPS_staircase_total_IOPS;
        m_s.measurements[0].edit_rollup_text = o.str();

        auto rc = m_s.edit_rollup("all=all", o.str(), true);

        if (!rc.first)
        {
            std::ostringstream oh;
            oh << "<Error> internal programming error - for DFC = IOPS_staircase, "
                << "failed setting starting IOPS with edit_rollup(\"all=all\", \""
                << o.str() << "\") - " << rc.second << std::endl;
            std::cout << oh.str();
            log(m_s.masterlogfile,oh.str());
            m_s.kill_subthreads_and_exit();
        }
    }

    return;
}

bool multi_measure_proceed_to_next()  // returns true if we have started a new measurement.
{
    // NOTE: A new "measurement" object only gets pushed back once this method has returned true.
    //       Here is where we only do an edit_rollup() to send out the changes for the next measurement.

    if (!m_s.have_IOPS_staircase) return false;

    measurement& m = m_s.current_measurement();

    if (!m.success)
    {
/*debug*/log(m_s.masterlogfile,"multi_measure_proceed_to_next() - !m.success");
        return false;
    }


    // NOTE: If the user specifies "step" and "steps", this gets transformed into "step" and "ending_IOPS",
    //       so if have_staircase_ending_IOPS == false, we are testing to saturation.
    if ((!m_s.have_staircase_ending_IOPS) && m.failed_to_achieve_total_IOPS_setting) return false;

    if (m_s.have_staircase_step_percent_increment)
    {
        current_IOPS_staircase_total_IOPS = (1.0 + m_s.staircase_step) * current_IOPS_staircase_total_IOPS;
    }
    else
    {
        current_IOPS_staircase_total_IOPS += m_s.staircase_step;
    }

    if (m_s.have_staircase_ending_IOPS)
    {
/*debug*/{std::ostringstream o; o << "debug: multi_measure_proceed_to_next() - current_IOPS_staircase_total_IOPS = " << current_IOPS_staircase_total_IOPS << ", n_s.staircase_ending_IOPS = " << m_s.staircase_ending_IOPS << ".";log(m_s.masterlogfile,o.str());}

        if (current_IOPS_staircase_total_IOPS > (1.001 * m_s.staircase_ending_IOPS))
        {
/*debug*/log(m_s.masterlogfile,"multi_measure_proceed_to_next() - total IOPS has reached ending IOPS.");
            return false;
        }
    }

    return true;
}

void multi_measure_edit_rollup_total_IOPS()
{
    {
        std::ostringstream to_value;

        to_value << "total_IOPS = " << std::fixed << std::setprecision(0) << current_IOPS_staircase_total_IOPS;

        auto rc = m_s.edit_rollup("all=all", to_value.str(), true);
        if (!rc.first)
        {
            std::ostringstream o;
            o << "<Error> internal programming error - in multi_measure_proceed_to_next() failed trying to edit_rollup(\"all=all\", \""
                << to_value.str() << "\") to set new total_IOPS for next staircase step, saying: " << rc.second << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << std::endl << o.str() << std::endl;
            m_s.kill_subthreads_and_exit();
        }
        m_s.current_measurement().edit_rollup_text = to_value.str();
    }

    return;
}
