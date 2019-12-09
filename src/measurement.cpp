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

std::string measurement::step_times_line(unsigned int measurement_number) const
{
    std::ostringstream o;
    o << "********* " << m_s.stepNNNN;

    if (m_s.have_IOPS_staircase) { o << " measurement " << measurement_number; }

    o    << " duration "      << duration()         .format_as_duration_HMMSS()
        << " = warmup "      << warmup_duration()  .format_as_duration_HMMSS()
        << " + measurement " << measure_duration() .format_as_duration_HMMSS()
        << " + cooldown "    << cooldown_duration().format_as_duration_HMMSS();
    o   << " for step name \"" << m_s.stepName;

    if (m_s.have_IOPS_staircase)
    {
        o << ":" << edit_rollup_text;
    }

    o << "\"";

    o << std::endl;

    return o.str();
}

std::pair<bool,std::string> measurement::make_measurement_rollup_CPU()
{
		if
		(
		    m_s.cpu_by_subinterval.size() == 0
		    || firstMeasurementIndex < 0
		    || firstMeasurementIndex > lastMeasurementIndex
		    || lastMeasurementIndex >= ((int)m_s.cpu_by_subinterval.size())
		)
		{
			std::ostringstream o;
			o << "measurement::make_measurement_rollup_CPU() - Invalid first (" << firstMeasurementIndex << ") and last (" << lastMeasurementIndex << ") measurement period indices (from zero) "
			<< "with " << m_s.cpu_by_subinterval.size() << " subintervals of test host CPU busy data." << std::endl;
			return std::make_pair(false,o.str());
		}

		measurement_rollup_CPU.clear();
		measurement_rollup_CPU_temp.clear();

		for (int i = firstMeasurementIndex; i <= lastMeasurementIndex; i ++)
		{
			measurement_rollup_CPU.rollup(m_s.cpu_by_subinterval[i]);
			measurement_rollup_CPU_temp.rollup(m_s.cpu_degrees_C_from_critical_temp_by_subinterval[i]);
		}

		return std::make_pair(true,"");
}

std::string measurement::phase(int subinterval_number) const
{
    if (success)
    {
        if (subinterval_number < first_subinterval)
        {
            std::ostringstream o;
            o << "<Error> measurement::phase(int subinterval_number = " << subinterval_number << ") - subinterval_number is before first_subinterval - " << toString();
            return o.str();
        }

        if (subinterval_number < firstMeasurementIndex) return "warmup";

        if (subinterval_number >= firstMeasurementIndex && subinterval_number <= lastMeasurementIndex) return "measurement";

        if (subinterval_number > lastMeasurementIndex && subinterval_number <= last_subinterval) return "cooldown";

        {
            std::ostringstream o;
            o << "<Error> measurement::phase(int subinterval_number = " << subinterval_number << ") - subinterval_number is after last_subinterval - " << toString();
            return o.str();
        }
    }
    else
    {
        return "warmup";
    }
}

std::string measurement::toString() const
{
    std::ostringstream o;

    o << "{ \"measurement\" : { \"first_subinterval\" : " << first_subinterval
        << ", \"firstMeasurementIndex\" : " << firstMeasurementIndex
        << ", \"lastMeasurementIndex\" : " << lastMeasurementIndex
        << ", \"last_subinterval\" : " << last_subinterval
        << ", \"warmup_start\" : \""   << warmup_start .format_as_datetime_with_ns()
        << ", \"measure_start\" : \""  << measure_start.format_as_datetime_with_ns()
        << ", \"measure_end\" : \""    << measure_end  .format_as_datetime_with_ns()
        << ", \"cooldown_end\" : \""   << cooldown_end .format_as_datetime_with_ns()
        << ", success : \"" << (success ? "true" : "false") << "\" } }";
    return o.str();
}
