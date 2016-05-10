//Copyright (c) 2016 Hitachi Data Systems, Inc.
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
//Author: Allart Ian Vogelesang <ian.vogelesang@hds.com>
//
//Support:  "ivy" is not officially supported by Hitachi Data Systems.
//          Contact me (Ian) by email at ian.vogelesang@hds.com and as time permits, I'll help on a best efforts basis.
#include "master_stuff.h"
#include "ParameterValueLookupTable.h"
#include "MeasureDFC.h"
#include "run_subinterval_sequence.h"

// [Go!]
//    stepname = stepNNNN,
//    subinterval_seconds = 5,
//    warmup_seconds=5
//    measure_seconds=5
//    cooldown_by_wp = on

//    dfc = pid
//       p = 0
//       i = 0
//       d = 0
//       target_value=0
//       starting_total_IOPS=100
//       min_IOPS = 10   - this is so that if your pid loop is on service time, there will be a service time measurement to get the average of.

//    measure = on
//       accuracy_plus_minus = "2%"
//       confidence = "95%"
//       max_wp = "100%"
//       min_wp = "0%"
//       max_wp_change = "3%"

//    either dfc = pid and/or measure = on
//       focus_rollup = all
//       source = workload
//          category =  { overall, read, write, random, sequential, random_read, random_write, sequential_read, sequential_write }
//          accumulator_type = { bytes_transferred, service_time, response_time }
//          accessor = { avg, count, min, max, sum, variance, standardDeviation }
//       source = RAID_subsystem
//          subsystem_element = ""
//          element_metric = ""
//

void go_statement(yy::location bookmark)
{
    // This code starts with the get go.  I just had to say that.

    m_s.have_pid = false;
    m_s.have_measure = false;
    m_s.have_workload = false;
    m_s.have_RAID_subsystem = false;
    m_s.have_within = false;
    m_s.have_max_above_or_below = false;

    m_s.goStatementsSeen++;

    m_s.get_go.setToNow();

    {
        std:: ostringstream o;
        o << "step" << std::setw(4) << std::setfill('0') << (m_s.goStatementsSeen-1);
        m_s.stepNNNN=o.str();
    }
    trim(m_s.goParameters);

    if (!m_s.go_parameters.fromString(m_s.goParameters))
    {
        std::ostringstream o;
        o << std::endl << "<Error> at " << bookmark << " - Failed parsing [Go!] parameters.  Should look like  parameter1=value1, parameter2=\"value 2\"  - unable to parse \"" << m_s.goParameters << "\"." << std::endl;
        std::cout << o.str();
        log(m_s.masterlogfile,o.str());
        m_s.kill_subthreads_and_exit();
    }

    std::string valid_parameter_names = "stepname, subinterval_seconds, warmup_seconds, measure_seconds, cooldown_by_wp";

    std::string valid_parameters_message =
        "The following parameter names are always valid:    stepname, subinterval_seconds, warmup_seconds, measure_seconds, cooldown_by_wp.\n\n"
        "For dfc = pid, additional valid parameters are:    p, i, d, target_value, starting_total_IOPS, min_IOPS.\n\n"
        "For measure = on, additional valid parameters are: accuracy_plus_minus, confidence, max_wp, min_wp, max_wp_change, timeout_seconds\n\n."
        "For dfc=pid or measure=on, must have either source = workload, or source = RAID_subsystem.\n\n"
        "For source = workload, additional valid parameters are: category, accumulator_type, accessor.\n\n"
        "For source = RAID_subsystem, additional valid parameters are: subsystem_element, element_metric.\n\n";

    if (m_s.go_parameters.contains("stepname")) m_s.stepName = m_s.go_parameters.retrieve("stepname");
    else m_s.stepName = m_s.stepNNNN;

    if (!m_s.go_parameters.contains(std::string("subinterval_seconds")))  m_s.go_parameters.contents[toLower(std::string("subinterval_seconds"))]  = subinterval_seconds_default;
    if (!m_s.go_parameters.contains(std::string("warmup_seconds")))      m_s.go_parameters.contents[toLower(std::string("warmup_seconds"))]      = warmup_seconds_default;
    if (!m_s.go_parameters.contains(std::string("measure_seconds")))     m_s.go_parameters.contents[toLower(std::string("measure_seconds"))]     = measure_seconds_default;
    if (!m_s.go_parameters.contains(std::string("cooldown_by_wp")))      m_s.go_parameters.contents[toLower(std::string("cooldown_by_wp"))]      = cooldown_by_wp_default;

    if (m_s.go_parameters.contains(std::string("dfc")))
    {

        std::string controllerName = m_s.go_parameters.retrieve("dfc");

        if (stringCaseInsensitiveEquality("pid", controllerName))
        {
            m_s.have_pid = true;
            valid_parameter_names += ", dfc, p, i, d, target_value, starting_total_IOPS, min_IOPS";

            if (!m_s.go_parameters.contains(std::string("p")))                        m_s.go_parameters.contents[toLower(std::string("p"                  ))] = p_default;
            if (!m_s.go_parameters.contains(std::string("i")))                        m_s.go_parameters.contents[toLower(std::string("i"                  ))] = i_default;
            if (!m_s.go_parameters.contains(std::string("d")))                        m_s.go_parameters.contents[toLower(std::string("d"                  ))] = d_default;
            if (!m_s.go_parameters.contains(std::string("target_value")))             m_s.go_parameters.contents[toLower(std::string("target_value"       ))] = target_value_default;
            if (!m_s.go_parameters.contains(std::string("starting_total_IOPS")))      m_s.go_parameters.contents[toLower(std::string("starting_total_IOPS"))] = starting_total_IOPS_default;
            if (!m_s.go_parameters.contains(std::string("min_IOPS")))                 m_s.go_parameters.contents[toLower(std::string("min_IOPS"           ))] = min_IOPS_default;
        }
        else
        {
            std::ostringstream o;
            o << std::endl << "<Error> at " << bookmark << " - \"dfc\", if specified, may only be set to \"pid\"." << std::endl << std::endl;
            std::cout << o.str();
            log(m_s.masterlogfile,o.str());
            m_s.kill_subthreads_and_exit();
        }

//        std::map<std::string, DynamicFeedbackController*>::iterator controller_it = m_s.availableControllers.find(toLower(controllerName));
//
//        if (m_s.availableControllers.end() == controller_it)
//        {
//            std::ostringstream o;
//            o << std::endl << "Failed - Dynamic Feedback Controller (dfc) \"" << controllerName << "\" not found. Valid values are:" << std::endl;
//            for (auto& pear : m_s.availableControllers) o << pear.first << std::endl;
//            std::cout << o.str();
//            log(m_s.masterlogfile,o.str());
//            m_s.kill_subthreads_and_exit();
//        }

        m_s.the_dfc.reset();
    }

    if (m_s.go_parameters.contains(std::string("measure")))
    {
        std::string measure_parameter_value = m_s.go_parameters.retrieve("measure");
        if (stringCaseInsensitiveEquality(std::string("on"),measure_parameter_value))
        {
            m_s.have_measure = true;
            valid_parameter_names += ", measure, accuracy_plus_minus, confidence, max_wp, min_wp, max_wp_change, timeout_seconds";
            if (!m_s.go_parameters.contains(std::string("accuracy_plus_minus"))) m_s.go_parameters.contents[toLower(std::string("accuracy_plus_minus"))] = accuracy_plus_minus_default;
            if (!m_s.go_parameters.contains(std::string("confidence")))          m_s.go_parameters.contents[toLower(std::string("confidence"))]          = confidence_default;
            if (!m_s.go_parameters.contains(std::string("min_wp")))              m_s.go_parameters.contents[toLower(std::string("min_wp"))]              = min_wp_default;
            if (!m_s.go_parameters.contains(std::string("max_wp")))              m_s.go_parameters.contents[toLower(std::string("max_wp"))]              = max_wp_default;
            if (!m_s.go_parameters.contains(std::string("max_wp_change")))       m_s.go_parameters.contents[toLower(std::string("max_wp_change"))]       = max_wp_change_default;
            if (!m_s.go_parameters.contains(std::string("timeout_seconds")))     m_s.go_parameters.contents[toLower(std::string("timeout_seconds"))]     = timeout_seconds_default;
        }
        else
        {
            if (!stringCaseInsensitiveEquality(std::string("off"),measure_parameter_value))
            {
                std::ostringstream o;
                o << std::endl << "<Error>  at " << bookmark << " - \"measure\" may only be set to \"on\" or \"off\"." << std::endl << std::endl
                                   << "Go statement specifies parameter values " << m_s.go_parameters.toString() << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
                m_s.kill_subthreads_and_exit();
            }
        }

    }

    if (m_s.have_measure || m_s.have_pid)
    {
        valid_parameter_names += ", focus_rollup, source";

        if (!m_s.go_parameters.contains(std::string("focus_rollup"))) m_s.go_parameters.contents[toLower(std::string("focus_rollup"))] = focus_rollup_default;
        if (!m_s.go_parameters.contains(std::string("source")))       m_s.go_parameters.contents[toLower(std::string("source"))]       = source_default;

        m_s.source_parameter = m_s.go_parameters.retrieve("source");

        if (stringCaseInsensitiveEquality(m_s.source_parameter,std::string("workload")))
        {
            m_s.have_workload = true;
            valid_parameter_names += ", category, accumulator_type, accessor";

            if (!m_s.go_parameters.contains(std::string("category")))         m_s.go_parameters.contents[toLower(std::string("category"))]         = category_default;
            if (!m_s.go_parameters.contains(std::string("accumulator_type"))) m_s.go_parameters.contents[toLower(std::string("accumulator_type"))] = accumulator_type_default;
            if (!m_s.go_parameters.contains(std::string("accessor")))         m_s.go_parameters.contents[toLower(std::string("accessor"))]         = accessor_default;
        }
        else if (stringCaseInsensitiveEquality(m_s.source_parameter,std::string("RAID_subsystem")))
        {
            m_s.have_RAID_subsystem = true;
            valid_parameter_names += ", subsystem_element, element_metric";

            if (!m_s.go_parameters.contains(std::string("subsystem_element")))  m_s.go_parameters.contents[toLower(std::string("subsystem_element"))] = subsystem_element_default;
            if (!m_s.go_parameters.contains(std::string("element_metric")))     m_s.go_parameters.contents[toLower(std::string("element_metric"))]    = element_metric_default;
        }
        else
        {
            std::ostringstream o;
            o << std::endl << "<Error>  at " << bookmark << " - The only valid values for the \"source\" parameter are \"workload\" and \"RAID_subsystem\"." << std::endl << std::endl
                               << "[Go] statement parameter values " << m_s.go_parameters.toString() << std::endl;
            std::cout << o.str();
            log(m_s.masterlogfile,o.str());
            m_s.kill_subthreads_and_exit();
        }
    }

    if (m_s.have_pid && m_s.have_measure)
    {
        valid_parameter_names += ", within";
        if (m_s.go_parameters.contains(std::string("within")))
        {
            // within="1%"
            // must be greater than zero, less than or equal to one.

            m_s.have_within = true;

            try
            {
                m_s.within = number_optional_trailing_percent(m_s.go_parameters.retrieve("within"));
            }
            catch(std::invalid_argument& iaex)
            {
                std::ostringstream o;
                o << "<Error>  at " << bookmark << " - Invalid \"within\" parameter value \"" << m_s.go_parameters.retrieve("within") << "\"." << std::endl;
                o << "Must be a number greater than zero and less than or equal to 1." << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
                m_s.kill_subthreads_and_exit();
            }
            if (m_s.within <= 0.0 || m_s.within >1)
            {
                std::ostringstream o;
                o << "<Error>  at " << bookmark << " - Invalid \"within\" parameter value \"" << m_s.go_parameters.retrieve("within") << "\".";
                o << "  Must be a number greater than zero and less than or equal to 1." << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
                m_s.kill_subthreads_and_exit();
            }
        }

        valid_parameter_names += ", max_above_or_below";
        if (m_s.go_parameters.contains(std::string("max_above_or_below")))
        {
            // must be a positive integer

            // max number of consecutive intervals where the error signal stays the same sign

            // "within" may be what you want to use most of the time, but I'm leaving this here for now.

            m_s.have_max_above_or_below = true;

            std::string maob_parm = m_s.go_parameters.retrieve("max_above_or_below");

            trim(maob_parm);  // by reference

            if ( (!isalldigits(maob_parm))  || (0==( m_s.max_above_or_below = (unsigned int) atoi(maob_parm.c_str()) )) )
            {
                std::ostringstream o;
                o << "<Error>  at " << bookmark << " - Invalid \"max_above_or_below\" parameter value \"" << m_s.go_parameters.retrieve("max_above_or_below") << "\"." << std::endl;
                o << "Must be all digits representing a positive integer." << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile,o.str());
                m_s.kill_subthreads_and_exit();
            }
        }
    }

    if (!m_s.go_parameters.containsOnlyValidParameterNames(valid_parameter_names))
    {
        std::ostringstream o;
        o << std::endl << "<Error>  at " << bookmark << " - Invalid [Go] statement parameter." << std::endl << std::endl
                           << "Error found in [Go] statement parameter values: " << m_s.go_parameters.toString() << std::endl << std::endl
                           << valid_parameters_message << std::endl << std::endl;
        std::cout << o.str();
        log(m_s.masterlogfile,o.str());
        m_s.kill_subthreads_and_exit();
    }


    {
        std::ostringstream o;
        o << std::endl << "Effective [Go!] statement parameters including defaulted parameters:" << m_s.go_parameters.toString() << std::endl;
        std::cout << o.str();
        log(m_s.masterlogfile, o.str());
    }


//----------------------------------- subinterval_seconds
    try
    {
        m_s.subinterval_seconds = number_optional_trailing_percent(m_s.go_parameters.retrieve("subinterval_seconds"));
    }
    catch(std::invalid_argument& iaex)
    {
        std::ostringstream o;
        o << "<Error>  at " << bookmark << " - Invalid subinterval_seconds parameter value \"" << m_s.go_parameters.retrieve("subinterval_seconds") << "\"." << std::endl;
        std::cout << o.str();
        log(m_s.masterlogfile,o.str());
        m_s.kill_subthreads_and_exit();
    }

    if (m_s.subinterval_seconds < min_subinterval_seconds || m_s.subinterval_seconds > max_subinterval_seconds )
    {
        std::ostringstream o;
        o << "<Error>  at " << bookmark << " - Invalid subinterval_seconds parameter value \"" << m_s.go_parameters.retrieve("subinterval_seconds") << "\".  Permitted range is from " << min_subinterval_seconds << " to " << max_subinterval_seconds << "." << std::endl;
        std::cout << o.str();
        log(m_s.masterlogfile,o.str());
        m_s.kill_subthreads_and_exit();
    }
    m_s.subintervalLength = ivytime(m_s.subinterval_seconds);
    if (m_s.subinterval_seconds < 3.)
    {
        std::ostringstream o;
        o << "<Error>  at " << bookmark << " - Invalid subinterval_seconds parameter value \"" << m_s.go_parameters.retrieve("subinterval_seconds") << "\".  Permitted range is from " << min_subinterval_seconds << " to " << max_subinterval_seconds << "." << std::endl;
        std::cout << o.str();
        log(m_s.masterlogfile,o.str());
        m_s.kill_subthreads_and_exit();
    }

//----------------------------------- warmup_seconds
    try
    {
        m_s.warmup_seconds = number_optional_trailing_percent(m_s.go_parameters.retrieve("warmup_seconds"));
    }
    catch(std::invalid_argument& iae)
    {
        ostringstream o;
        o << "<Error>  at " << bookmark << " - [Go] statement - invalid warmup_seconds parameter \"" << m_s.go_parameters.retrieve("warmup_seconds") << "\".  Must be a number." << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
    }
    if (m_s.warmup_seconds < m_s.subinterval_seconds)
    {
        ostringstream o;
        o << "<Error>  at " << bookmark << " - [Go] statement - invalid warmup_seconds parameter \"" << m_s.go_parameters.retrieve("warmup_seconds") << "\".  Must be at least as long as subinterval_seconds = ." << m_s.subinterval_seconds << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
    }
    m_s.min_warmup_count = (int) ceil( m_s.warmup_seconds / m_s.subinterval_seconds);


//----------------------------------- measure_seconds
    try
    {
        m_s.measure_seconds = number_optional_trailing_percent(m_s.go_parameters.retrieve("measure_seconds"));
    }
    catch(std::invalid_argument& iae)
    {
        ostringstream o;
        o << "<Error>  at " << bookmark << " - [Go] statement - invalid measure_seconds parameter \"" << m_s.go_parameters.retrieve("measure_seconds") << "\".  Must be a number." << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
    }
    if (m_s.measure_seconds < m_s.subinterval_seconds)
    {
        ostringstream o;
        o << "<Error>  at " << bookmark << " - [Go] statement - invalid measure_seconds parameter \"" << m_s.go_parameters.retrieve("measure_seconds") << "\".  Must be at least as long as subinterval_seconds = ." << m_s.subinterval_seconds << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
    }
    m_s.min_measure_count = (int) ceil( m_s.measure_seconds / m_s.subinterval_seconds);

//----------------------------------- cooldown_by_wp
    if      ( stringCaseInsensitiveEquality(std::string("on"),  m_s.go_parameters.retrieve("cooldown_by_wp")) ) m_s.cooldown_by_wp = true;
    else if ( stringCaseInsensitiveEquality(std::string("off"), m_s.go_parameters.retrieve("cooldown_by_wp")) ) m_s.cooldown_by_wp = false;
    else
    {
        ostringstream o;
        o << "<Error>  at " << bookmark << " - [Go] statement - invalid cooldown_by_wp parameter \"" << m_s.go_parameters.retrieve("cooldown_by_wp") << "\".  Must be \"on\" or \"off\"." << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
    }


    if (m_s.have_pid || m_s.have_measure)
    {
//----------------------------------- focus_rollup
        m_s.focus_rollup_parameter = m_s.go_parameters.retrieve("focus_rollup");

        auto it = m_s.rollups.rollups.find(toLower(m_s.focus_rollup_parameter));

        if (it == m_s.rollups.rollups.end() )
        {
            ostringstream o;
            o << "<Error>  at " << bookmark << " - [Go] statement - invalid focus_rollup parameter \"" << m_s.focus_rollup_parameter << "\"." << std::endl;
            o << "The rollups are"; for (auto& pear : m_s.rollups.rollups) o << "  \"" << pear.first << "\""; o << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();
            m_s.kill_subthreads_and_exit();
        }

        m_s.p_focus_rollup = it->second;

        if (m_s.have_workload)
        {
            m_s.source = source_enum::workload;

//----------------------------------- category
            m_s.category_parameter = m_s.go_parameters.retrieve("category");
            if (stringCaseInsensitiveEquality(m_s.category_parameter,"overall")) m_s.category = category_enum::overall;
            else if (stringCaseInsensitiveEquality(m_s.category_parameter,"read")) m_s.category = category_enum::read;
            else if (stringCaseInsensitiveEquality(m_s.category_parameter,"write")) m_s.category = category_enum::write;
            else if (stringCaseInsensitiveEquality(m_s.category_parameter,"random")) m_s.category = category_enum::random;
            else if (stringCaseInsensitiveEquality(m_s.category_parameter,"sequential")) m_s.category = category_enum::sequential;
            else if (stringCaseInsensitiveEquality(m_s.category_parameter,"random_read")) m_s.category = category_enum::random_read;
            else if (stringCaseInsensitiveEquality(m_s.category_parameter,"sequential_read")) m_s.category = category_enum::sequential_read;
            else if (stringCaseInsensitiveEquality(m_s.category_parameter,"sequential_write")) m_s.category = category_enum::sequential_write;
            else
            {
                ostringstream o;
                o << "<Error>  at " << bookmark << " - [Go] statement - invalid category parameter \"" << m_s.category_parameter << "\"." << std::endl;
                o << "The categories are overall, read, write, random, sequential, random_read, random_write, sequential_read, and sequential_write."; o << std::endl;
                log(m_s.masterlogfile,o.str());
                std::cout << o.str();
                m_s.kill_subthreads_and_exit();
            }
//----------------------------------- accumulator_type
            m_s.accumulator_type_parameter = m_s.go_parameters.retrieve("accumulator_type");
            if (stringCaseInsensitiveEquality(m_s.accumulator_type_parameter,"bytes_transferred")) m_s.accumulator_type = accumulator_type_enum::bytes_transferred;
            else if (stringCaseInsensitiveEquality(m_s.accumulator_type_parameter,"service_time")) m_s.accumulator_type = accumulator_type_enum::service_time;
            else if (stringCaseInsensitiveEquality(m_s.accumulator_type_parameter,"response_time")) m_s.accumulator_type = accumulator_type_enum::response_time;
            else
            {
                ostringstream o;
                o << "<Error>  at " << bookmark << " - [Go] statement - invalid accumulator_type parameter \"" << m_s.accumulator_type_parameter << "\"." << std::endl;
                o << "The accumulator_types are bytes_transferred, service_time, and response_timee."; o << std::endl;
                log(m_s.masterlogfile,o.str());
                std::cout << o.str();
                m_s.kill_subthreads_and_exit();
            }
//----------------------------------- accessor
            m_s.accessor_parameter = m_s.go_parameters.retrieve("accessor");
            if (stringCaseInsensitiveEquality(m_s.accessor_parameter,"avg")) m_s.accessor = accessor_enum::avg;
            else if (stringCaseInsensitiveEquality(m_s.accessor_parameter,"count")) m_s.accessor = accessor_enum::count;
            else if (stringCaseInsensitiveEquality(m_s.accessor_parameter,"min")) m_s.accessor = accessor_enum::min;
            else if (stringCaseInsensitiveEquality(m_s.accessor_parameter,"max")) m_s.accessor = accessor_enum::max;
            else if (stringCaseInsensitiveEquality(m_s.accessor_parameter,"sum")) m_s.accessor = accessor_enum::sum;
            else if (stringCaseInsensitiveEquality(m_s.accessor_parameter,"variance")) m_s.accessor = accessor_enum::variance;
            else if (stringCaseInsensitiveEquality(m_s.accessor_parameter,"standardDeviation")) m_s.accessor = accessor_enum::standardDeviation;
            else
            {
                ostringstream o;
                o << "<Error>  at " << bookmark << " - [Go] statement - invalid accessor parameter \"" << m_s.accessor_parameter << "\"." << std::endl;
                o << "The accessors are avg, count, min, max, sum, variance, and standardDeviation."; o << std::endl;
                log(m_s.masterlogfile,o.str());
                std::cout << o.str();
                m_s.kill_subthreads_and_exit();
            }

        }
        else if (m_s.have_RAID_subsystem)
        {
            m_s.source = source_enum::RAID_subsystem;

            m_s.subsystem_element = m_s.go_parameters.retrieve("subsystem_element");
            m_s.element_metric = m_s.go_parameters.retrieve("element_metric");

            bool found_element {false};
            bool found_metric {false};

            for (auto& e_pear : m_s.subsystem_summary_metrics)
            {
                std::string& element = e_pear.first;

                if (stringCaseInsensitiveEquality(element,m_s.subsystem_element))
                {
//----------------------------------- subsystem_element
                    found_element = true;

                    for (const std::pair<std::string,unsigned char>& metric_pair : e_pear.second)
                    {
                        const std::string metric = metric_pair.first;

                        if (stringCaseInsensitiveEquality(metric,m_s.element_metric))
                        {
//----------------------------------- element_metric
                            found_metric = true;

                            break;
                        }
                    }

                    if (!found_metric)
                    {
                        ostringstream o;

                        o << "<Error>  at " << bookmark << " - [Go] statement - invalid element_metric \"" << m_s.element_metric << "\" for subsystem_element \"" << m_s.subsystem_element << "\"." << std::endl;

                        o << "The subsystem_elements and their element_metrics are:" << std::endl;

                        for (auto& e_pear : m_s.subsystem_summary_metrics)
                        {
                            std::string& element = e_pear.first;

                            o << "   subsystem_element = " << element << std::endl;

                            for (const std::pair<std::string,unsigned char>& metric_pair : e_pear.second)
                            {
                                const std::string metric = metric_pair.first;

                                o << "       element_metric = " << element;
                                o << std::endl;

                            }
                        }

                        o << std::endl << "The definition of the these subsystems metrics which are rolled up for each rollup instance is in subsystem_summary_metrics in master_stuff.h." << std::endl << std::endl;

                        log(m_s.masterlogfile,o.str());
                        std::cout << o.str();
                        m_s.kill_subthreads_and_exit();
                    }
                    break;
                }

            }
            if (!found_element)
            {
                ostringstream o;

                o << "<Error>  at " << bookmark << " - [Go] statement - invalid subsystem_element parameter \"" << m_s.subsystem_element << "\"." << std::endl;

                o << "The subsystem_elements and their element_metrics are:" << std::endl;

                for (auto& e_pear : m_s.subsystem_summary_metrics)
                {
                    std::string& element = e_pear.first;

                    o << "   subsystem_element = " << element << std::endl;

                    for (const std::pair<std::string,unsigned char>& metric_pair : e_pear.second)
                    {
                        const std::string metric = metric_pair.first;

                        o << "       element_metric = " << element << std::endl;

                    }
                }

                o << std::endl << "The definition of these subsystem element metrics which are rolled up for each rollup instance is in subsystem_summary_metrics in master_stuff.h." << std::endl << std::endl;

                log(m_s.masterlogfile,o.str());
                std::cout << o.str();
                m_s.kill_subthreads_and_exit();
            }
        }
        else
        {
            ostringstream o;
            o << std::endl << "<Error>  at " << bookmark << " - [Go] statement - invalid source parameter \"" << m_s.source_parameter << "\".  Must be \"workload\" or \"RAID_subsystem\"." << std::endl << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();
            m_s.kill_subthreads_and_exit();
        }
    }

    if (m_s.have_pid)
    {
        // valid_parameter_names += ", dfc, p, i, d, target_value, starting_total_IOPS, min_IOPS";

            // If a parameter is not present, "retrieve" returns a null string.

//----------------------------------- element_metric
            m_s.target_value_parameter = m_s.go_parameters.retrieve("target_value");
            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFCcategory::PID - when setting \"target_value\" parameter to value \""
                    << m_s.target_value_parameter << "\": ";

                m_s.target_value = number_optional_trailing_percent(m_s.target_value_parameter,where_the.str());
            }

//----------------------------------- p
            m_s.p_parameter               = m_s.go_parameters.retrieve("p");
            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFCcategory::PID - when setting \"p\" parameter to value \""
                    << m_s.p_parameter << "\": ";

                m_s.p_multiplier = number_optional_trailing_percent(m_s.p_parameter,where_the.str());
            }

//----------------------------------- i
            m_s.i_parameter               = m_s.go_parameters.retrieve("i");
            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFCcategory::PID - when setting \"i\" parameter  to value \""
                    << m_s.i_parameter << "\": ";

                m_s.i_multiplier = number_optional_trailing_percent(m_s.i_parameter,where_the.str());
            }

//----------------------------------- d
            m_s.d_parameter               = m_s.go_parameters.retrieve("d");
            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFCcategory::PID - when setting \"d\" parameter  to value \""
                    << m_s.d_parameter << "\": ";

                m_s.d_multiplier = number_optional_trailing_percent(m_s.d_parameter,where_the.str());
            }

//----------------------------------- starting_total_IOPS
            m_s.starting_total_IOPS_parameter = m_s.go_parameters.retrieve("starting_total_IOPS");
            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFCcategory::PID - when setting \"starting_total_IOPS\" parameter to value \""
                    << m_s.starting_total_IOPS << "\": ";

                m_s.starting_total_IOPS = number_optional_trailing_percent(m_s.starting_total_IOPS_parameter,where_the.str());
                if (m_s.starting_total_IOPS < 0.0)
                {
                    ostringstream o;
                    o << std::endl << "<Error> [Go] statement - invalid starting_total_IOPS parameter \"" << m_s.starting_total_IOPS_parameter << "\".  May not be negative." << std::endl << std::endl;
                    log(m_s.masterlogfile,o.str());
                    std::cout << o.str();
                    m_s.kill_subthreads_and_exit();
                }
            }

//----------------------------------- min_IOPS
            m_s.min_IOPS_parameter = m_s.go_parameters.retrieve("min_IOPS");
            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFCcategory::PID - when setting \"min_IOPS\" parameter to value \""
                    << m_s.min_IOPS_parameter << "\": ";

                m_s.min_IOPS = number_optional_trailing_percent(m_s.min_IOPS_parameter,where_the.str());
                if (m_s.min_IOPS < 0.0)
                {
                    ostringstream o;
                    o << std::endl << "<Error>  at " << bookmark << " - [Go] statement - invalid min_IOPS parameter \"" << m_s.min_IOPS_parameter << "\".  May not be negative." << std::endl << std::endl;
                    log(m_s.masterlogfile,o.str());
                    std::cout << o.str();
                    m_s.kill_subthreads_and_exit();
                }
            }

    }


    if (m_s.have_measure)

    {
//----------------------------------- accuracy_plus_minus
        m_s.accuracy_plus_minus_parameter = m_s.go_parameters.retrieve("accuracy_plus_minus");

        try
        {
            m_s.accuracy_plus_minus_fraction = number_optional_trailing_percent(m_s.accuracy_plus_minus_parameter);
        }
        catch (std::invalid_argument& iaex)
        {
            std::ostringstream o;
            o << "<Error>  at " << bookmark << " - Invalid accuracy_plus_minus parameter \"" << m_s.accuracy_plus_minus_parameter << "\".  Must be a number optionally with a trailing percent sign," << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();
            m_s.kill_subthreads_and_exit();
        }


//----------------------------------- confidence
        m_s.confidence_parameter = m_s.go_parameters.retrieve("confidence");
        try
        {
            m_s.confidence = number_optional_trailing_percent(m_s.confidence_parameter);
        }
        catch(std::invalid_argument& iae)
        {
            ostringstream o;
            o << "<Error>  at " << bookmark << " - Invalid confidence parameter \"" << m_s.confidence_parameter << "\".  Must be a number." << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();
            m_s.kill_subthreads_and_exit();
        }


//----------------------------------- timeout_seconds
        m_s.timeout_seconds_parameter = m_s.go_parameters.retrieve("timeout_seconds");
        try
        {
            m_s.timeout_seconds = number_optional_trailing_percent(m_s.timeout_seconds_parameter);
        }
        catch(std::invalid_argument& iae)
        {
            ostringstream o;
            o << "<Error>  at " << bookmark << " - Invalid timeout_seconds parameter \"" << m_s.timeout_seconds_parameter << "\".  Must be a number." << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();
            m_s.kill_subthreads_and_exit();
        }

//----------------------------------- min_wp
        m_s.min_wp_parameter = m_s.go_parameters.retrieve("min_wp");
        try
        {
            m_s.min_wp = number_optional_trailing_percent(m_s.min_wp_parameter);

        }
        catch (std::invalid_argument& iaex)
        {
            ostringstream o;
            o << "<Error>  at " << bookmark << " - Invalid min_wp parameter \"" << m_s.min_wp_parameter << "\".  Must be from 0% to 100% or from 0.0 to 1.0." << std::endl;
            log(m_s.masterlogfile,o.str());
            m_s.kill_subthreads_and_exit();
        }
        if ( m_s.min_wp < 0.0 || m_s.min_wp > 1.0 )
        {
            ostringstream o;
            o << "<Error>  at " << bookmark << " - Invalid min_wp_range parameter \"" << m_s.min_wp_parameter << "\".  Must be from 0% to 100% or from 0.0 to 1.0." << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();
            m_s.kill_subthreads_and_exit();
        }

//----------------------------------- max_wp
        m_s.max_wp_parameter = m_s.go_parameters.retrieve("max_wp");
        try
        {
            m_s.max_wp = number_optional_trailing_percent(m_s.go_parameters.retrieve("max_wp"));

        }
        catch (std::invalid_argument& iaex)
        {
            ostringstream o;
            o << "<Error>  at " << bookmark << " - Invalid max_wp parameter \"" << m_s.max_wp_parameter << "\".  Must be from 0% to 100% or from 0.0 to 1.0." << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();
            m_s.kill_subthreads_and_exit();
        }
        if ( m_s.max_wp < 0.0 || m_s.max_wp > 1.0 || m_s.max_wp < m_s.min_wp)
        {
            ostringstream o;
            o << "<Error>  at " << bookmark << " - Invalid max_wp_change parameter \"" << m_s.max_wp_parameter << "\".  Must be from 0% to 100% or from 0.0 to 1.0 and must not be less than min_wp." << std::endl;
            log(m_s.masterlogfile,o.str());
            m_s.kill_subthreads_and_exit();
        }

//----------------------------------- max_wp_change
        m_s.max_wp_change_parameter = m_s.go_parameters.retrieve("max_wp_change");
        try
        {
            m_s.max_wp_change = number_optional_trailing_percent(m_s.max_wp_change_parameter);

        }
        catch (std::invalid_argument& iaex)
        {
            ostringstream o;
            o << "<Error>  at " << bookmark << " - Invalid max_wp_change parameter \"" << m_s.max_wp_change_parameter << "\"." << std::endl;
            std::cout << o.str();
            log(m_s.masterlogfile,o.str());
            m_s.kill_subthreads_and_exit();
        }
        if ( m_s.max_wp_change < 0.0 || m_s.max_wp_change > 1.0 )
        {
            ostringstream o;
            o << "<Error>  at " << bookmark << " - Invalid max_wp_change parameter \"" << m_s.max_wp_change_parameter << "\".  Must be from 0% to 100% or from 0.0 to 1.0." << std::endl;
            log(m_s.masterlogfile,o.str());
            m_s.kill_subthreads_and_exit();
        }
    }

    m_s.the_dfc.reset();

    run_subinterval_sequence(&m_s.the_dfc);

    ivytime went; went.setToNow();
    ivytime flight_duration = went - m_s.get_go;

    {
        std::ostringstream o;
        o << "********* " << m_s.stepNNNN << " duration " << flight_duration.format_as_duration_HMMSS() << " \"" << m_s.stepName << "\"" << std::endl;

        m_s.step_times << o.str();
        log(m_s.masterlogfile, o.str());
        std::cout << o.str();
    }
}

