//Copyright (c) 2016, 2017, 2018, 2019 Hitachi Vantara Corporation
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
#include "run_subinterval_sequence.h"

extern std::set<std::string> valid_IosequencerInput_parameters;

std::pair<bool /*success*/, std::string /* message */>
ivy_engine::go(const std::string& parameters)
{
    {
        std::ostringstream o;
        o << "ivy engine API go("
            << "parameters = \"" << parameters << "\""
            << " )" << std::endl;
        log (masterlogfile,o.str());
        std::cout << o.str();
        log(ivy_engine_calls_filename,o.str());
    }

    std::pair<bool,std::string> rc {};

    if ( !haveHosts )
    {
        std::ostringstream o;
        o << "<Error> ivy engine API \"go\" invoked with no preceeding ivy engine startup and no workloads created." << std::endl;
        rc.first=false;
        rc.second=o.str();
        return rc;
    }

    // This code starts with the get go.  I just had to say that.

    have_pid = false;
    have_measure = false;
    have_workload = false;
    have_RAID_subsystem = false;

    have_staircase_ending_IOPS = false;
    have_staircase_step = false;
    have_staircase_step_percent_increment = false;
    have_staircase_steps = false;
    staircase_starting_IOPS=-1.0;
    staircase_ending_IOPS=-1.0;
    staircase_step = -1.0;
    staircase_steps = 0;
    staircase_starting_IOPS_parameter.clear();
    staircase_ending_IOPS_parameter.clear();
    staircase_step_parameter.clear();
    staircase_steps_parameter.clear();

    trim(goParameters);

    std::pair<bool,std::string> r = go_parameters.fromString(parameters);
    if (!r.first)
    {
        std::ostringstream o;
        o << std::endl << "<Error> ivy engine API - Failed parsing [Go!] parameters \"" << parameters << "\"." << std::endl << std::endl
            << "[Go] parameters should look like:" << std::endl << "     parameter1=value1, parameter2=\"value 2\", ..." << std:: endl
            << "where parameter names are identifiers starting with an alphabetic and continuing with alphanumerics and underscores" << std::endl
            << "and where parameter values need to be enclosed in quotes if they contain any characters other than"
            << "English alphanumerics (\'a\'-\'z\', \'A\'-\'Z\', \'0\'-\'9\'), underscores (\'_\'), periods (\'.\'), or percent signs (\'%\')." << std::endl
            << "Example of OK unquoted value:  accuracy_plus_minus = 2.5%" << std::endl << std::endl
            << r.second << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }

    std::string parameter_value;

    std::string valid_parameter_names = "no_perf, suppress_perf, skip_LDEV, stepname, subinterval_seconds, warmup_seconds, measure_seconds, cooldown_seconds, cooldown_by_wp"s
        + ",cooldown_by_MP_busy, cooldown_by_MP_busy_stay_down_time_seconds, subsystem_WP_threshold, subsystem_busy_threshold, sequential_fill"s
        + ",stepcsv, storcsv, check_failed_component"s;

    std::string valid_parameters_message =
        "The following parameter names are always valid:\n"
        "    stepname, subinterval_seconds, warmup_seconds, measure_seconds, cooldown_seconds, cooldown_by_wp,\n"
        "    no_perf, skip_LDEV, subsystem_WP_threshold, cooldown_by_MP_busy, cooldown_by_MP_busy_stay_down_time_seconds,\n"
        "    subsystem_busy_threshold, sequential_fill, stepcsv, storcsv, check_failed_component.\n\n"
        "For dfc = PID, additional valid parameters are:\n"
        "    target_value, low_IOPS, low_target, high_IOPS, high_target, max_IOPS, max_ripple, gain_step,\n"
        "    ballpark_seconds, max_monotone, balanced_step_direction_by.\n\n"
        "For dfc = IOPS_staircase, additional valid parameters are:\n"
        "    starting_IOPS, ending_IOPS, step, steps.\n\n"
        "For measure = on, additional valid parameters are:\n"
        "    accuracy_plus_minus, confidence, max_wp, min_wp, max_wp_change, timeout_seconds\n\n."
        "For dfc=pid or measure=on, must have either source = workload, or source = RAID_subsystem.\n"
        "    These are usually set using \"shorthand\" such as measure=service_time_seconds or measure=PG_percent_busy.\n\n"
        "For source = workload, additional valid parameters are: category, accumulator_type, accessor.\n\n"
        "    These are usually set using \"shorthand\" such as measure=service_time_seconds or measure=PG_percent_busy.\n\n"
        "For source = RAID_subsystem, additional valid parameters are: subsystem_element, element_metric.\n\n"
        "    These are usually set using \"shorthand\" such as measure=service_time_seconds or measure=PG_percent_busy.\n\n"

R"("measure" may be set to "on" or "off", or to one of the following shorthand settings:

    measure = MB_per_second           is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=bytes_transferred, accessor=sum
    measure = IOPS                    is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=bytes_transferred, accessor=count
    measure = service_time_seconds    is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=service_time,      accessor=avg
    measure = response_time_seconds   is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=response_time,     accessor=avg
    measure = MP_core_busy_percent    is short for    measure=on, focus_rollup=all, source=RAID_subsystem, subsystem_element=MP_core, element_metric=busy_percent
    measure = PG_busy_percent         is short for    measure=on, focus_rollup=all, source=RAID_subsystem, subsystem_element=PG,      element_metric=busy_percent
    measure = CLPR_WP_percent         is short for    measure=on, focus_rollup=all, source=RAID_subsystem, subsystem_element=CLPR,    element_metric=WP_percent

    Note: In the above shorthand settings, the "focus_rollup" is only set to "all" if the user did not specify "focus_rollup".
          That is, you can override focus_rollup even if you use one of the shorthand settings.
)"
    ;

    if ( (!go_parameters.contains("no_perf"s))  && (!go_parameters.contains("suppress_perf"s)) )        go_parameters.contents[normalize_identifier(std::string("no_perf"))]            = suppress_subsystem_perf_default ? "on" : "off";
    if (!go_parameters.contains("skip_LDEV"s))              go_parameters.contents[normalize_identifier(std::string("skip_LDEV"))]                = skip_ldev_data_default          ? "on" : "off";
    if (!go_parameters.contains("stepcsv"s))                go_parameters.contents[normalize_identifier(std::string("stepcsv"))]                  = stepcsv_default                 ? "on" : "off";
    if (!go_parameters.contains("storcsv"s))                go_parameters.contents[normalize_identifier(std::string("storcsv"))]                  = storcsv_default                 ? "on" : "off";
    if (!go_parameters.contains("check_failed_component"s)) go_parameters.contents[normalize_identifier(std::string("check_failed_component"))]   = check_failed_component_default  ? "on" : "off";

    if (!go_parameters.contains(std::string("subinterval_seconds")))      go_parameters.contents[normalize_identifier(std::string("subinterval_seconds"))]      = subinterval_seconds_default;
    if (!go_parameters.contains(std::string("measure_seconds")))          go_parameters.contents[normalize_identifier(std::string("measure_seconds"))]          = measure_seconds_default;
    if (!go_parameters.contains(std::string("cooldown_by_wp")))           go_parameters.contents[normalize_identifier(std::string("cooldown_by_wp"))]           = cooldown_by_wp_default;
    if (!go_parameters.contains(std::string("cooldown_by_MP_busy")))      go_parameters.contents[normalize_identifier(std::string("cooldown_by_MP_busy"))]      = cooldown_by_MP_busy_default;
    if (!go_parameters.contains(std::string("cooldown_by_MP_busy_stay_down_time_seconds")))      go_parameters.contents[normalize_identifier(std::string("cooldown_by_MP_busy_stay_down_time_seconds"))] = go_parameters.retrieve("subinterval_seconds");
    if (!go_parameters.contains(std::string("subsystem_WP_threshold")))   go_parameters.contents[normalize_identifier(std::string("subsystem_WP_threshold"))]   = subsystem_WP_threshold_default; // used with cooldown_by_wp
    if (!go_parameters.contains(std::string("subsystem_busy_threshold"))) go_parameters.contents[normalize_identifier(std::string("subsystem_busy_threshold"))] = subsystem_busy_threshold_default; // used with cooldown_by_MP_busy

    if (go_parameters.contains(std::string("dfc")))
    {
        std::string controllerName = go_parameters.retrieve("dfc");

        if (stringCaseInsensitiveEquality("pid", controllerName))
        {
            have_pid = true;
            valid_parameter_names += ", dfc, target_value, low_IOPS, low_target, high_IOPS, high_target, max_IOPS, max_ripple, gain_step, ballpark_seconds, max_monotone, balanced_step_direction_by";
            if (!go_parameters.contains(std::string("max_ripple")))                 go_parameters.contents[normalize_identifier(std::string("max_ripple"))]       = std::string("1%");
            if (!go_parameters.contains(std::string("gain_step")))                  go_parameters.contents[normalize_identifier(std::string("gain_step"))]        = std::string("2");
            if (!go_parameters.contains(std::string("ballpark_seconds")))           go_parameters.contents[normalize_identifier(std::string("ballpark_seconds"))] = std::string("60");
            if (!go_parameters.contains(std::string("balanced_step_direction_by"))) go_parameters.contents[normalize_identifier(std::string("balanced_step_direction_by"))] = std::string("12");
            if (!go_parameters.contains(std::string("max_monotone")))               go_parameters.contents[normalize_identifier(std::string("max_monotone"))]     = std::string("5");
        }
        else if (stringCaseInsensitiveEquality("IOPS_staircase", controllerName))
        {
            have_IOPS_staircase = true;
            valid_parameter_names += ", dfc, starting_IOPS, ending_IOPS, step, steps";
        }
        else
        {
            std::ostringstream o;
            o << std::endl << "<Error> ivy engine API go() - \"dfc\", if specified, may only be set to \"pid\" or \"IOPS_staircase\"." << std::endl << std::endl;
            std::cout << o.str();
            log(masterlogfile,o.str());
            kill_subthreads_and_exit();
        }

        the_dfc.reset();
    }

    if (go_parameters.contains(std::string("measure")))
    {
        std::string measure_parameter_value = go_parameters.retrieve("measure");

        if (normalized_identifier_equality(measure_parameter_value,std::string("MB_per_second")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            if (!go_parameters.contains("focus_rollup")) go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[toLower("source")] = "workload";
            go_parameters.contents[toLower("category")] = "overall";
            go_parameters.contents[normalize_identifier("accumulator_type")] = "bytes_transferred";
            go_parameters.contents[normalize_identifier("accessor")] = "sum";
        }
        else if (stringCaseInsensitiveEquality(measure_parameter_value,std::string("IOPS")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            if (!go_parameters.contains("focus_rollup")) go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[normalize_identifier("source")] = "workload";
            go_parameters.contents[toLower("category")] = "overall";
            go_parameters.contents[normalize_identifier("accumulator_type")] = "bytes_transferred";
            go_parameters.contents[toLower("accessor")] = "count";
        }
        else if (normalized_identifier_equality(measure_parameter_value,std::string("service_time_seconds")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            if (!go_parameters.contains("focus_rollup")) go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[toLower("source")] = "workload";
            go_parameters.contents[toLower("category")] = "overall";
            go_parameters.contents[normalize_identifier("accumulator_type")] = "service_time";
            go_parameters.contents[toLower("accessor")] = "avg";
        }
        else if (normalized_identifier_equality(measure_parameter_value,std::string("response_time_seconds")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            if (!go_parameters.contains("focus_rollup")) go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[toLower("source")] = "workload";
            go_parameters.contents[toLower("category")] = "overall";
            go_parameters.contents[normalize_identifier("accumulator_type")] = "response_time";
            go_parameters.contents[toLower("accessor")] = "avg";
        }
        else if (normalized_identifier_equality(measure_parameter_value,std::string("MP_core_busy_percent")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            if (!go_parameters.contains("focus_rollup")) go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[toLower("source")] = "RAID_subsystem";
            go_parameters.contents[normalize_identifier("subsystem_element")] = "MP core";
            go_parameters.contents[normalize_identifier("element_metric")] = "Busy %";
        }
        else if (normalized_identifier_equality(measure_parameter_value,std::string("PG_busy_percent")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            if (!go_parameters.contains("focus_rollup")) go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[toLower("source")] = "RAID_subsystem";
            go_parameters.contents[normalize_identifier("subsystem_element")] = "PG";
            go_parameters.contents[normalize_identifier("element_metric")] = "busy %";
        }
        else if (normalized_identifier_equality(measure_parameter_value,std::string("CLPR_WP_percent")))
        {
            go_parameters.contents[toLower("measure")] = "on";
            if (!go_parameters.contains("focus_rollup")) go_parameters.contents[normalize_identifier("focus_rollup")] = "all";
            go_parameters.contents[toLower("source")] = "RAID_subsystem";
            go_parameters.contents[normalize_identifier("subsystem_element")] = "CLPR";
            go_parameters.contents[normalize_identifier("element_metric")] = "WP_percent";
        }

        // maybe we have a measure_focus_rollup and a pid_focus_rollup and they default to focus_rollup which defaults to "all"?

        // How hard would it be to have measure and pid each on their own focus_rollup?

        measure_parameter_value = go_parameters.retrieve("measure");

        if (stringCaseInsensitiveEquality(std::string("on"),measure_parameter_value))
        {
            have_measure = true;
            valid_parameter_names += ", measure, accuracy_plus_minus, confidence, max_wp, min_wp, max_wp_change, timeout_seconds";
            if (!go_parameters.contains(std::string("accuracy_plus_minus"))) go_parameters.contents[normalize_identifier(std::string("accuracy_plus_minus"))] = accuracy_plus_minus_default;
            if (!go_parameters.contains(std::string("confidence")))          go_parameters.contents[normalize_identifier(std::string("confidence"))]          = confidence_default;
            if (!go_parameters.contains(std::string("min_wp")))              go_parameters.contents[normalize_identifier(std::string("min_wp"))]              = min_wp_default;
            if (!go_parameters.contains(std::string("max_wp")))              go_parameters.contents[normalize_identifier(std::string("max_wp"))]              = max_wp_default;
            if (!go_parameters.contains(std::string("max_wp_change")))       go_parameters.contents[normalize_identifier(std::string("max_wp_change"))]       = max_wp_change_default;
            if (!go_parameters.contains(std::string("timeout_seconds")))     go_parameters.contents[normalize_identifier(std::string("timeout_seconds"))]     = timeout_seconds_default;
        }
        else
        {
            if (!stringCaseInsensitiveEquality(std::string("off"),measure_parameter_value))
            {
                std::ostringstream o;
                o << std::endl << "<Error> ivy engine API - go() - invalid \"measure\" setting value \"" << measure_parameter_value << "\"." << std::endl
                    << "\"measure\" may only be set to \"on\" or \"off\", or to one of the following shorthand settings:" << std::endl
                    << R"(
    measure = MB_per_second           is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=bytes_transferred, accessor=sum
    measure = IOPS                    is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=bytes_transferred, accessor=count
    measure = service_time_seconds    is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=service_time,      accessor=avg
    measure = response_time_seconds   is short for    measure=on, focus_rollup=all, source=workload, category=overall, accumulator_type=response_time,     accessor=avg
    measure = MP_core_busy_percent    is short for    measure=on, focus_rollup=all, source=RAID_subsystem, subsystem_element=MP_core, element_metric=busy_percent
    measure = PG_busy_percent         is short for    measure=on, focus_rollup=all, source=RAID_subsystem, subsystem_element=PG,      element_metric=busy_percent
    measure = CLPR_WP_percent         is short for    measure=on, focus_rollup=all, source=RAID_subsystem, subsystem_element=CLPR,    element_metric=WP_percent

    Note: In the above shorthand settings, the "focus_rollup" is only set to "all" if the user did not specify "focus_rollup".
          That is, you can override focus_rollup even if you use one of the shorthand settings.
)"
                << std::endl
                                   << "Go statement specifies parameter values " << go_parameters.toString() << std::endl;
                std::cout << o.str();
                log(masterlogfile,o.str());
                kill_subthreads_and_exit();
            }
        }

    }

    if (have_measure || have_pid)
    {
        valid_parameter_names += ", focus_rollup, source";

        if (!go_parameters.contains(std::string("focus_rollup"))) go_parameters.contents[normalize_identifier(std::string("focus_rollup"))] = focus_rollup_default;
        if (!go_parameters.contains(std::string("source")))       go_parameters.contents[normalize_identifier(std::string("source"))]       = source_default;

        source_parameter = go_parameters.retrieve("source");

        if (stringCaseInsensitiveEquality(source_parameter,std::string("workload")))
        {
            have_workload = true;
            valid_parameter_names += ", category, accumulator_type, accessor";

            if (!go_parameters.contains(std::string("category")))         go_parameters.contents[normalize_identifier(std::string("category"))]         = category_default;
            if (!go_parameters.contains(std::string("accumulator_type"))) go_parameters.contents[normalize_identifier(std::string("accumulator_type"))] = accumulator_type_default;
            if (!go_parameters.contains(std::string("accessor")))         go_parameters.contents[normalize_identifier(std::string("accessor"))]         = accessor_default;
        }
        else if (stringCaseInsensitiveEquality(source_parameter,std::string("RAID_subsystem")))
        {
            have_RAID_subsystem = true;
            valid_parameter_names += ", subsystem_element, element_metric";

            if (!go_parameters.contains(std::string("subsystem_element")))  go_parameters.contents[normalize_identifier(std::string("subsystem_element"))] = subsystem_element_default;
            if (!go_parameters.contains(std::string("element_metric")))     go_parameters.contents[normalize_identifier(std::string("element_metric"))]    = element_metric_default;
        }
        else
        {
            std::ostringstream o;
            o << std::endl << "<Error>  ivy engine API - go() - The only valid values for the \"source\" parameter are \"workload\" and \"RAID_subsystem\"." << std::endl << std::endl
                               << "[Go] statement parameter values " << go_parameters.toString() << std::endl;
            std::cout << o.str();
            log(masterlogfile,o.str());
            kill_subthreads_and_exit();
        }
    }

    auto has_workload_parms = go_parameters.contains_these_parameter_names(valid_IosequencerInput_parameters);
    if (has_workload_parms.first)
    {
        std::ostringstream o;
        o << std::endl << "<Error> ivy engine API - go() - the following workload parameter(s) were provided as [Go] parameters ==> " << has_workload_parms.second << std::endl << std::endl
                           << "If you wish to loop over a workload parameter once with a single value, put the value in parentheses like \"maxTags = ( 4 )\"." << std::endl << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }

    auto names_are_valid = go_parameters.containsOnlyValidParameterNames(valid_parameter_names);
    if (!names_are_valid.first)
    {
        std::ostringstream o;
        o << std::endl << "<Error> ivy engine API - invalid [Go] parameter(s) ==> " << names_are_valid.second << std::endl << std::endl
                           << valid_parameters_message << std::endl << std::endl
                           << "Once again, the invalid parameter(s) were " << names_are_valid.second << std::endl << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }


    {
        std::ostringstream o;
        o << std::endl << "Effective [Go!] statement parameters including defaulted parameters:" << go_parameters.toString() << std::endl << std::endl;
        std::cout << o.str();
        log(masterlogfile, o.str());
    }



//----------------------------------- sequential_fill = on
    if (go_parameters.contains("sequential_fill"))
    {
        std::string w = go_parameters.retrieve("sequential_fill");

        if ( stringCaseInsensitiveEquality(w,std::string("on")) || stringCaseInsensitiveEquality(w,std::string("true")) )
        {
            sequential_fill = true;
        }
        else
        {
            std::ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid \"sequential_fill\" parameter value \""
            << w << "\".  \"sequential_fill\", if specified, may only be set to \"on\"."  << std::endl;
            std::cout << o.str();
            log(masterlogfile,o.str());
            kill_subthreads_and_exit();
        }
    }
    else
    {
        sequential_fill = false;
    }



//----------------------------------- subinterval_seconds
    parameter_value = go_parameters.retrieve("subinterval_seconds");
    try
    {
        subinterval_seconds = number_optional_trailing_percent(parameter_value);
    }
    catch(std::invalid_argument& iaex)
    {
        std::ostringstream o;
        o << "<Error> ivy engine API - go() - Invalid subinterval_seconds parameter value \"" << parameter_value << "\"." << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }

    if (subinterval_seconds < min_subinterval_seconds || subinterval_seconds > max_subinterval_seconds )
    {
        std::ostringstream o;
        o << "<Error> ivy engine API - go() - Invalid subinterval_seconds parameter value \"" << parameter_value << "\".  Permitted range is from " << min_subinterval_seconds << " to " << max_subinterval_seconds << "." << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }
    subintervalLength = ivytime(subinterval_seconds);
    if (subinterval_seconds < 3.)
    {
        std::ostringstream o;
        o << "<Error> ivy engine API - go() - Invalid subinterval_seconds parameter value \"" << parameter_value << "\".  Permitted range is from " << min_subinterval_seconds << " to " << max_subinterval_seconds << "." << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }

//----------------------------------- warmup_seconds
    if (!go_parameters.contains(std::string("warmup_seconds")))
    {
        warmup_seconds = subinterval_seconds;
    }
    else
    {
        try
        {
            std::string ws = go_parameters.retrieve("warmup_seconds");
            trim(ws);
            warmup_seconds = number_optional_trailing_percent(rewrite_HHMMSS_to_seconds(ws));
        }
        catch(std::invalid_argument& iae)
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - [Go] statement - invalid warmup_seconds parameter \"" << go_parameters.retrieve("warmup_seconds") << "\".  Must be a number." << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }

        if (warmup_seconds < 0 || (warmup_seconds > 0 && warmup_seconds < subinterval_seconds))
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - [Go] statement - invalid warmup_seconds parameter \"" << go_parameters.retrieve("warmup_seconds")
                << "\".  Must be either zero or at least as long as subinterval_seconds = " << subinterval_seconds << "." << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }
    }

    min_warmup_count = (int) ceil( warmup_seconds / subinterval_seconds);


//----------------------------------- measure_seconds
    try
    {
        std::string ms = go_parameters.retrieve("measure_seconds");
        trim(ms);
        measure_seconds = number_optional_trailing_percent(rewrite_HHMMSS_to_seconds(ms));

    }
    catch(std::invalid_argument& iae)
    {
        ostringstream o;
        o << "<Error> ivy engine API - go() - [Go] statement - invalid measure_seconds parameter \"" << go_parameters.retrieve("measure_seconds") << "\".  Must be a number." << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }
    if (measure_seconds < subinterval_seconds)
    {
        ostringstream o;
        o << "<Error> ivy engine API - go() - [Go] statement - invalid measure_seconds parameter \"" << go_parameters.retrieve("measure_seconds") << "\".  Must be at least as long as subinterval_seconds = ." << subinterval_seconds << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }
    min_measure_count = (int) ceil( measure_seconds / subinterval_seconds);


//----------------------------------- cooldown_seconds

    if (go_parameters.contains("cooldown_seconds"s))
    {
        std::string cs;
        try
        {
            cs = go_parameters.retrieve("cooldown_seconds");
            trim(cs);
            double csd = number_optional_trailing_percent(rewrite_HHMMSS_to_seconds(cs));
            cooldown_seconds = ivytime(csd);

        }
        catch(std::invalid_argument& iae)
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - [Go] statement - invalid measure_seconds parameter \"" << cs << "\".  Must be a number." << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }
    }
    else
    {
        cooldown_seconds = ivytime_zero;
    }

//----------------------------------- suppress_perf
    if (go_parameters.contains("suppress_perf"s)) { parameter_value = go_parameters.retrieve("suppress_perf");}
    else                                          { parameter_value = go_parameters.retrieve("no_perf");}

    try
    {
        suppress_subsystem_perf = parse_boolean(parameter_value);
    }
    catch (const std::invalid_argument& ia)
    {
        std::ostringstream o;
        o << "<Error> invalid [Go] \"";
        if (go_parameters.contains("suppress_perf"s)) { o << "suppress_perf"; } else { o << "no_perf"; }
        o << "\") parameter value - " << ia.what() << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }

//----------------------------------- stepcsv
    parameter_value = go_parameters.retrieve("stepcsv");
    try
    {
        stepcsv = parse_boolean(parameter_value);
    }
    catch (const std::invalid_argument& ia)
    {
        std::ostringstream o;
        o << "<Error> invalid [Go] \"stepcsv\" parameter value - " << ia.what() << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }

//----------------------------------- storcsv
    parameter_value = go_parameters.retrieve("storcsv");
    try
    {
        storcsv = parse_boolean(parameter_value);
    }
    catch (const std::invalid_argument& ia)
    {
        std::ostringstream o;
        o << "<Error> invalid [Go] \"storcsv\" parameter value - " << ia.what() << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }
//----------------------------------- check_failed_component
    parameter_value = go_parameters.retrieve("check_failed_component");
    try
    {
        check_failed_component = parse_boolean(parameter_value);
    }
    catch (const std::invalid_argument& ia)
    {
        std::ostringstream o;
        o << "<Error> invalid [Go] \"check_failed_component\" parameter value - " << ia.what() << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }

//----------------------------------- skip_LDEV
    parameter_value = go_parameters.retrieve("skip_LDEV");
    try
    {
        skip_ldev_data = parse_boolean(parameter_value);
    }
    catch (const std::invalid_argument& ia)
    {
        std::ostringstream o;
        o << "<Error> invalid [Go] \"skip_LDEV\" parameter value - " << ia.what() << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }

//----------------------------------- cooldown_by_wp
    parameter_value = go_parameters.retrieve("cooldown_by_wp");
    try
    {
        bool b = parse_boolean(parameter_value);
        cooldown_by_wp = b ? cooldown_mode::on : cooldown_mode::off;
    }
    catch (const std::invalid_argument& ia)
    {
        std::ostringstream o;
        o << "<Error> invalid [Go] \"cooldown_by_WP\" parameter value - " << ia.what() << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }

//----------------------------------- subsystem_WP_threshold  - goes with cooldown_by_wp
    std::string subsystem_WP_threshold_parameter = go_parameters.retrieve("subsystem_WP_threshold");
    {
        std::ostringstream where_the;
        where_the << "go_statement() - when setting \"subsystem_WP_threshold\" parameter to value \""
            << subsystem_WP_threshold_parameter << "\": ";

        subsystem_WP_threshold = number_optional_trailing_percent(subsystem_WP_threshold_parameter,where_the.str());
    }


//----------------------------------- cooldown_by_MP_busy
    parameter_value = go_parameters.retrieve("cooldown_by_MP_busy");
    try
    {
        bool b = parse_boolean(parameter_value);
        cooldown_by_MP_busy = b ? cooldown_mode::on : cooldown_mode::off;
    }
    catch (const std::invalid_argument& ia)
    {
        std::ostringstream o;
        o << "<Error> invalid [Go] \"cooldown_by_MP_busy\" parameter value - " << ia.what() << std::endl;
        log(masterlogfile,o.str());
        std::cout << o.str();
        kill_subthreads_and_exit();
    }

    //----------------------------------- cooldown_by_MP_busy_stay_down_time_seconds
    try
    {
        cooldown_by_MP_busy_stay_down_time_seconds = number_optional_trailing_percent(rewrite_HHMMSS_to_seconds(go_parameters.retrieve("cooldown_by_MP_busy_stay_down_time_seconds")));
    }
    catch(std::invalid_argument& iaex)
    {
        std::ostringstream o;
        o << "<Error> ivy engine API - go() - Invalid cooldown_by_MP_busy_stay_down_time_seconds parameter value \"" << go_parameters.retrieve("cooldown_by_MP_busy_stay_down_time_seconds") << "\"." << std::endl;
        std::cout << o.str();
        log(masterlogfile,o.str());
        kill_subthreads_and_exit();
    }

    cooldown_by_MP_busy_stay_down_count_limit = (unsigned int) std::ceil(cooldown_by_MP_busy_stay_down_time_seconds / subinterval_seconds);


//----------------------------------- subsystem_busy_threshold  - goes with cooldown_by_MP_busy
    std::string subsystem_busy_threshold_parameter = go_parameters.retrieve("subsystem_busy_threshold");
    {
        std::ostringstream where_the;
        where_the << "go_statement() - when setting \"subsystem_busy_threshold\" parameter to value \""
            << subsystem_busy_threshold_parameter << "\": ";

        subsystem_busy_threshold = number_optional_trailing_percent(subsystem_busy_threshold_parameter,where_the.str());
    }


    if (have_pid || have_measure)
    {
//----------------------------------- focus_rollup
        focus_rollup_parameter = go_parameters.retrieve("focus_rollup");

        auto it = rollups.rollups.find(toLower(focus_rollup_parameter));

        if (it == rollups.rollups.end() )
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - [Go] statement - invalid focus_rollup parameter \"" << focus_rollup_parameter << "\"." << std::endl;
            o << "The rollups are"; for (auto& pear : rollups.rollups) o << "  \"" << pear.first << "\""; o << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }

        p_focus_rollup = it->second;

        if (have_workload)
        {
            source = source_enum::workload;

//----------------------------------- category
            category_parameter = go_parameters.retrieve("category");
            if (stringCaseInsensitiveEquality(category_parameter,"overall")) category = category_enum::overall;
            else if (stringCaseInsensitiveEquality(category_parameter,"read")) category = category_enum::read;
            else if (stringCaseInsensitiveEquality(category_parameter,"write")) category = category_enum::write;
            else if (stringCaseInsensitiveEquality(category_parameter,"random")) category = category_enum::random;
            else if (stringCaseInsensitiveEquality(category_parameter,"sequential")) category = category_enum::sequential;
            else if (normalized_identifier_equality(category_parameter,"random_read")) category = category_enum::random_read;
            else if (normalized_identifier_equality(category_parameter,"sequential_read")) category = category_enum::sequential_read;
            else if (normalized_identifier_equality(category_parameter,"sequential_write")) category = category_enum::sequential_write;
            else
            {
                ostringstream o;
                o << "<Error> ivy engine API - go() - [Go] statement - invalid category parameter \"" << category_parameter << "\"." << std::endl;
                o << "The categories are overall, read, write, random, sequential, random_read, random_write, sequential_read, and sequential_write."; o << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
//----------------------------------- accumulator_type
            accumulator_type_parameter = go_parameters.retrieve("accumulator_type");
            if (normalized_identifier_equality(accumulator_type_parameter,"bytes_transferred")) accumulator_type = accumulator_type_enum::bytes_transferred;
            else if (normalized_identifier_equality(accumulator_type_parameter,"service_time")) accumulator_type = accumulator_type_enum::service_time;
            else if (normalized_identifier_equality(accumulator_type_parameter,"response_time")) accumulator_type = accumulator_type_enum::response_time;
            else
            {
                ostringstream o;
                o << "<Error> ivy engine API - go() - [Go] statement - invalid accumulator_type parameter \"" << accumulator_type_parameter << "\"." << std::endl;
                o << "The accumulator_types are bytes_transferred, service_time, and response_timee."; o << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
//----------------------------------- accessor
            accessor_parameter = go_parameters.retrieve("accessor");
            if (stringCaseInsensitiveEquality(accessor_parameter,"avg")) accessor = accessor_enum::avg;
            else if (stringCaseInsensitiveEquality(accessor_parameter,"count")) accessor = accessor_enum::count;
            else if (stringCaseInsensitiveEquality(accessor_parameter,"min")) accessor = accessor_enum::min;
            else if (stringCaseInsensitiveEquality(accessor_parameter,"max")) accessor = accessor_enum::max;
            else if (stringCaseInsensitiveEquality(accessor_parameter,"sum")) accessor = accessor_enum::sum;
            else if (stringCaseInsensitiveEquality(accessor_parameter,"variance")) accessor = accessor_enum::variance;
            else if (normalized_identifier_equality(accessor_parameter,"standardDeviation")) accessor = accessor_enum::standardDeviation;
            else
            {
                ostringstream o;
                o << "<Error> ivy engine API - go() - invalid accessor parameter \"" << accessor_parameter << "\"." << std::endl;
                o << "The accessors are avg, count, min, max, sum, variance, and standardDeviation."; o << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }

        }
        else if (have_RAID_subsystem)
        {
            source = source_enum::RAID_subsystem;

            subsystem_element = go_parameters.retrieve("subsystem_element");
            element_metric = go_parameters.retrieve("element_metric");

            bool found_element {false};
            bool found_metric {false};

            for (auto& e_pear : subsystem_summary_metrics)
            {
                std::string& element = e_pear.first;

                if (stringCaseInsensitiveEquality(element,subsystem_element))
                {
//----------------------------------- subsystem_element
                    found_element = true;

                    for (const std::pair<std::string,unsigned char>& metric_pair : e_pear.second)
                    {
                        const std::string metric = metric_pair.first;

                        if (stringCaseInsensitiveEquality(metric,element_metric))
                        {
//----------------------------------- element_metric
                            found_metric = true;

                            break;
                        }
                    }

                    if (!found_metric)
                    {
                        ostringstream o;

                        o << "<Error> ivy engine API - go() - [Go] statement - invalid element_metric \"" << element_metric << "\" for subsystem_element \"" << subsystem_element << "\"." << std::endl;

                        o << "The subsystem_elements and their element_metrics are:" << std::endl;

                        for (auto& e_pear : subsystem_summary_metrics)
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

                        o << std::endl << "The definition of the these subsystems metrics which are rolled up for each rollup instance is in subsystem_summary_metrics in ivy_engine.h." << std::endl << std::endl;

                        log(masterlogfile,o.str());
                        std::cout << o.str();
                        kill_subthreads_and_exit();
                    }
                    break;
                }

            }
            if (!found_element)
            {
                ostringstream o;

                o << "<Error> ivy engine API - go() - [Go] statement - invalid subsystem_element parameter \"" << subsystem_element << "\"." << std::endl;

                o << "The subsystem_elements and their element_metrics are:" << std::endl;

                for (auto& e_pear : subsystem_summary_metrics)
                {
                    std::string& element = e_pear.first;

                    o << "   subsystem_element = " << element << std::endl;

                    for (const std::pair<std::string,unsigned char>& metric_pair : e_pear.second)
                    {
                        const std::string metric = metric_pair.first;

                        o << "       element_metric = " << element << std::endl;

                    }
                }

                o << std::endl << "The definition of these subsystem element metrics which are rolled up for each rollup instance is in subsystem_summary_metrics in ivy_engine.h." << std::endl << std::endl;

                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }
        else
        {
            ostringstream o;
            o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid source parameter \"" << source_parameter << "\".  Must be \"workload\" or \"RAID_subsystem\"." << std::endl << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }
    }

    if (have_IOPS_staircase)
    {
        // valid_parameter_names += ", starting_IOPS, ending_IOPS, step, steps";

        //----------------------------------- starting_IOPS
        if (go_parameters.contains("starting_IOPS"))
        {
            staircase_starting_IOPS_parameter = go_parameters.retrieve("starting_IOPS");
            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFC = IOPS_staircase - when setting \"starting_IOPS\" parameter to value \""
                    << staircase_starting_IOPS_parameter << "\": ";

                staircase_starting_IOPS = number_optional_trailing_percent(staircase_starting_IOPS_parameter,where_the.str());
            }

            if (staircase_starting_IOPS <= 0.0)
            {
                ostringstream o;
                o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid starting_IOPS parameter \"" << staircase_starting_IOPS_parameter << "\".  Must be greater than zero." << std::endl << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }
        else
        {
            std::string e = "<Error> ivy engine API - for DFC = IOPS_staircase, must provide the \"starting_IOPS\" parameter.\n\n";
            log(masterlogfile,e);
            std::cout << e; std::cout.flush();
            kill_subthreads_and_exit();
        }

        //----------------------------------- ending_IOPS
        if (go_parameters.contains("ending_IOPS"))
        {
            have_staircase_ending_IOPS = true;

            staircase_ending_IOPS_parameter = go_parameters.retrieve("ending_IOPS");
            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFC = IOPS_staircase - when setting \"ending_IOPS\" parameter to value \""
                    << staircase_ending_IOPS_parameter << "\": ";

                staircase_ending_IOPS = number_optional_trailing_percent(staircase_ending_IOPS_parameter,where_the.str());
            }

            if (staircase_ending_IOPS < staircase_starting_IOPS)
            {
                ostringstream o;
                o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid ending_IOPS parameter \"" << staircase_ending_IOPS_parameter
                    << "\".  Must be greater than starting_IOPS \"" << staircase_starting_IOPS_parameter << "\"." << std::endl << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }

        //----------------------------------- step
        if (go_parameters.contains("step"))
        {
            have_staircase_step = true;

            staircase_step_parameter = go_parameters.retrieve("step");

            trim(staircase_step_parameter);

            if (staircase_step_parameter.size() > 0 && staircase_step_parameter[0] == '+')
            {
                have_staircase_step_percent_increment = true;
                staircase_step_parameter.erase(0,1);
                trim(staircase_step_parameter);
            }
            else
            {
                have_staircase_step_percent_increment = false;
            }

            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFC = IOPS_staircase - when setting \"step\" parameter to value \""
                    << staircase_step_parameter << "\": ";

                staircase_step = number_optional_trailing_percent(staircase_step_parameter,where_the.str());
            }

            if (staircase_step <= 0.0)
            {
                ostringstream o;
                o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid \"step\" parameter \"" << go_parameters.retrieve("step") << "\".  Must be greater than zero." << std::endl << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }

        //----------------------------------- steps
        if (go_parameters.contains("steps"))
        {
            have_staircase_steps = true;

            staircase_steps_parameter = go_parameters.retrieve("steps");

            trim(staircase_steps_parameter);

            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFC = IOPS_staircase - when setting \"steps\" parameter to value \""
                    << staircase_steps_parameter << "\": ";

                staircase_steps = unsigned_int(staircase_steps_parameter,where_the.str());
            }

            if (staircase_steps < 2)
            {
                ostringstream o;
                o << std::endl << "<Error> ivy engine API -\"go\" - invalid \"steps\" parameter \"" << staircase_steps_parameter << "\".  Must be greater than one." << std::endl
                    << "To increase IOPS by a percentage each step, prefix the step size with a + sign," << std::endl
                    << "as in step = \"+10%\" or step = \"+0.10\", both of which are equivalent." << std::endl << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }

        if (have_staircase_ending_IOPS && (staircase_starting_IOPS > staircase_ending_IOPS))
        {
            ostringstream o;
            o << std::endl << "<Error> ivy engine API - go() - [Go] statement - starting_IOPS = \"" << staircase_starting_IOPS_parameter << "\" must be less than ending_IOPS = \"" << staircase_ending_IOPS_parameter << "\"." << std::endl << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }

        bool OK_combinations =
               (   have_staircase_ending_IOPS  &&   have_staircase_step  && (!have_staircase_steps) ) // "the" case where we have ending IOPS
            || (   have_staircase_ending_IOPS  && (!have_staircase_step) &&   have_staircase_steps  ) // fixed number of steps - this is reduced out below
            || ( (!have_staircase_ending_IOPS) &&   have_staircase_step  &&   have_staircase_steps  ) // fixed number of steps - this is reduced out below

            || ( (!have_staircase_ending_IOPS) &&   have_staircase_step  && (!have_staircase_steps) );// the case where we run to saturation, number of steps & ending IOPS not known in advance

        if (!OK_combinations)
        {
            std::ostringstream o;
            o << "<Error> ivy engine API - go() for DFC = IOPS_staircase - invalid combination of parameters \"starting_IOPS\"";
            if (have_staircase_ending_IOPS) o << " + \"ending_IOPS\"";
            if (have_staircase_step       ) o << " + \"step\"";
            if (have_staircase_steps      ) o << " + \"steps\"";
            o << "." << std::endl << std::endl
                << "Valid combinations with a fixed number of steps are:" << std::endl
                << "   \"starting_IOPS\" + \"step\" + \"ending_IOPS\"" << std::endl
                << "   \"starting_IOPS\" + \"ending_IOPS\" + \"steps\"" << std::endl
                << "   \"starting_IOPS\" + \"step\" + \"steps\"" << std::endl << std::endl
                << "To keep stepping until the attempted IOPS is not achieved:" << std::endl
                << "   \"starting_IOPS\" + \"step\"" << std::endl << std::endl
                << "To have each step be a fixed percentage higher, set \"step\" to \"+10%\", or \"+0.10\"." << std::endl << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }


        if(have_staircase_ending_IOPS && have_staircase_steps && staircase_steps < 2)
        {
            ostringstream o;
            o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid \"steps\" parameter \"" << staircase_steps_parameter
                << "\".  When ending_IOPS is provided, \"steps\" must be greater than one." << std::endl << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }

        if ( have_staircase_ending_IOPS && (!have_staircase_step) && have_staircase_steps )
        {
            // Reduce number of flavours of the case where we have a fixed number of steps

            staircase_step = (staircase_ending_IOPS - staircase_starting_IOPS) / ((ivy_float) (staircase_steps - 1));
            have_staircase_step = true;
            have_staircase_steps = false;
        }

        if ( (!have_staircase_ending_IOPS) && have_staircase_step && have_staircase_steps)
        {
            // Further reduce to a single case where we have a fixed number of steps

            staircase_ending_IOPS = staircase_starting_IOPS;

            for (unsigned int i = 1; i < staircase_steps; i++)
            {
                if (have_staircase_step_percent_increment)
                {
                    staircase_ending_IOPS *= (1.0 + staircase_step);
                }
                else
                {
                    staircase_ending_IOPS += staircase_step;
                }
            }
            have_staircase_ending_IOPS = true;
            have_staircase_steps = false;
        }

        // Now there are two types of stepping

        // 1) starting_IOPS + step + ending_IOPS, and
        // 2) starting_IOPS + step.

        // Thus "have_staircase_ending_IOPS" distinguishes between the cases.

//        {
//            std::ostringstream o;
//
//            if (go_parameters.contains("starting_IOPS")) { o << "go_parameters.retrieve(\"starting_IOPS\") = \"" << go_parameters.retrieve("starting_IOPS");} else {o << "don't have go_parameter \"starting_IOPS\"";} o << std::endl;
//            if (go_parameters.contains("ending_IOPS"))   { o << "go_parameters.retrieve(\"ending_IOPS\") = \""   << go_parameters.retrieve("ending_IOPS");}   else {o << "don't have go_parameter \"ending_IOPS\"";} o << std::endl;
//            if (go_parameters.contains("step"))          { o << "go_parameters.retrieve(\"step\") = \""          << go_parameters.retrieve("step");}          else {o << "don't have go_parameter \"step\"";} o << std::endl;
//            if (go_parameters.contains("steps"))         { o << "go_parameters.retrieve(\"steps\") = \""         << go_parameters.retrieve("steps");}         else {o << "don't have go_parameter \"steps\"";} o << std::endl;
//
//            o << "starting_IOPS = " << staircase_starting_IOPS
//                << ", ending_IOPS = " << staircase_ending_IOPS
//                << ", staircase_step = " << staircase_step
//                << ", staircase_steps = " << staircase_steps
//                << std::endl;
//            o << "have_staircase_starting_IOPS = " << (have_staircase_starting_IOPS ? "true" : "false") << std::endl;
//            o << "have_staircase_ending_IOPS = " << (have_staircase_ending_IOPS ? "true" : "false") << std::endl;
//            o << "have_staircase_step = " << (have_staircase_step ? "true" : "false") << std::endl;
//            o << "have_staircase_step_percent_increment = " << (have_staircase_step_percent_increment ? "true" : "false") << std::endl;
//            o << "have_staircase_steps = " << (have_staircase_steps ? "true" : "false") << std::endl;
//            std::cout << o.str();
//            log(masterlogfile,o.str());
//        }
    }

    if (have_pid)
    {
        // valid_parameter_names += ", dfc, target_value, low_IOPS, low_target, high_IOPS, high_target, max_IOPS, max_ripple, gain_step, ballpark_seconds, max_monotone, balanced_step_direction_by";

        if ( (!go_parameters.contains("target_value"))
          || (!go_parameters.contains("low_IOPS"))
          || (!go_parameters.contains("low_target"))
          || (!go_parameters.contains("high_IOPS"))
          || (!go_parameters.contains("high_target"))
        )
        {
            ostringstream o;
            o << "low_IOPS = \"" << go_parameters.retrieve("low_IOPS") << "\", high_IOPS = \"" << go_parameters.retrieve("high_IOPS") << "\"" << std::endl
                << "low_target = \"" << go_parameters.retrieve("low_target") << "\", target_value = \"" << go_parameters.retrieve("target_value")
                << "\", high_target = \"" <<  go_parameters.retrieve("high_target") << "\"." << std::endl;
            o << std::endl << "<Error> ivy engine API - go() - [Go] statement - when DFC = PID is used, the user must specify positive values for \n"
                              "the parameters target_value, low_IOPS, low_target, high_IOPS, and high_target.  Furthermore, high_IOPS must be\n"
                              "greater than low_IOPS and high_target must be greater than low_larget, and the target_value must be between\n"
                              "low_target and high_target." << std::endl << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }

//----------------------------------- target_value
        target_value_parameter = go_parameters.retrieve("target_value");
        {
            std::ostringstream where_the;
            where_the << "go_statement(), DFC = PID - when setting \"target_value\" parameter to value \""
                << target_value_parameter << "\": ";

            target_value = number_optional_trailing_percent(target_value_parameter,where_the.str());
        }

//----------------------------------- low_IOPS
        low_IOPS_parameter = go_parameters.retrieve("low_IOPS");
        {
            std::ostringstream where_the;
            where_the << "go_statement(), DFC = PID - when setting \"low_IOPS\" parameter to value \""
                << low_IOPS_parameter << "\": ";

            low_IOPS = number_optional_trailing_percent(low_IOPS_parameter,where_the.str());
        }

//----------------------------------- low_target
        low_target_parameter = go_parameters.retrieve("low_target");
        {
            std::ostringstream where_the;
            where_the << "go_statement(), DFC = PID - when setting \"low_target\" parameter to value \""
                << low_target_parameter << "\": ";

            low_target = number_optional_trailing_percent(low_target_parameter,where_the.str());
        }
//----------------------------------- high_IOPS
        high_IOPS_parameter = go_parameters.retrieve("high_IOPS");
        {
            std::ostringstream where_the;
            where_the << "go_statement(), DFC = PID - when setting \"high_IOPS\" parameter to value \""
                << high_IOPS_parameter << "\": ";

            high_IOPS = number_optional_trailing_percent(high_IOPS_parameter,where_the.str());
        }

//----------------------------------- high_target
        high_target_parameter = go_parameters.retrieve("high_target");
        {
            std::ostringstream where_the;
            where_the << "go_statement(), DFC = PID - when setting \"high_target\" parameter to value \""
                << high_target_parameter << "\": ";

            high_target = number_optional_trailing_percent(high_target_parameter,where_the.str());
        }


        if ( low_IOPS     <= 0.0        || low_target   <= 0.0          || high_IOPS <= 0.0 || high_target <= 0.0
          || low_IOPS     >= high_IOPS  || low_target   >= high_target
          || target_value <= low_target || target_value >= high_target
        )
        {
            ostringstream o;
            o << "low_IOPS = \"" << go_parameters.retrieve("low_IOPS") << "\", high_IOPS = \"" << go_parameters.retrieve("high_IOPS") << "\"" << std::endl
                << "low_target = \"" << go_parameters.retrieve("low_target") << "\", target_value = \"" << go_parameters.retrieve("target_value")
                << "\", high_target = \"" <<  go_parameters.retrieve("high_target") << "\"." << std::endl;
            o << std::endl << "<Error> ivy engine API - go() - [Go] statement - when DFC = PID is used, the user must specify positive values for \n"
                              "the parameters target_value, low_IOPS, low_target, high_IOPS, and high_target.  Furthermore, high_IOPS must be\n"
                              "greater than low_IOPS and high_target must be greater than low_larget, and the target_value must be between\n"
                              "low_target and high_target." << std::endl << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }


//----------------------------------- max_IOPS
        if (go_parameters.contains("max_IOPS"))
        {
            max_IOPS_parameter = go_parameters.retrieve("max_IOPS");
            {
                std::ostringstream where_the;
                where_the << "go_statement(), DFC = PID - when setting \"max_IOPS\" parameter to value \""
                    << max_IOPS_parameter << "\": ";

                max_IOPS = number_optional_trailing_percent(max_IOPS_parameter,where_the.str());
            }

            if (max_IOPS < high_IOPS)
            {
                ostringstream o;
                o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid max_IOPS parameter \"" << max_IOPS_parameter << "\".  "
                    << "max_IOPS, where optionally specified, may not be below the specified high_IOPS value " << high_IOPS_parameter << "."
                    << std::endl << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }
        else
        {
            max_IOPS = high_IOPS;
        }


//----------------------------------- gain_step
        {
            std::string parameter = go_parameters.retrieve("gain_step");

            std::ostringstream where_the; where_the << "go_statement(), DFC = PID - when setting \"gain_step\" parameter to value \"" << parameter << "\": ";

            m_s.gain_step = number_optional_trailing_percent(parameter,where_the.str());

            if (m_s.gain_step <= 0.0)
            {
                ostringstream o;
                o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid gain_step parameter \"" << parameter << "\".  Must be greater than zero." << std::endl << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }

//----------------------------------- max_ripple
        {
            std::string parameter = go_parameters.retrieve("max_ripple");

            std::ostringstream where_the; where_the << "go_statement(), DFC = PID - when setting \"max_ripple\" parameter to value \"" << parameter << "\": ";

            m_s.max_ripple = number_optional_trailing_percent(parameter,where_the.str());

            if (m_s.max_ripple <= 0.0)
            {
                ostringstream o;
                o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid max_ripple parameter \"" << parameter << "\".  Must be greater than zero." << std::endl << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }

//----------------------------------- max_monotone
        {
            std::string parameter = go_parameters.retrieve("max_monotone");

            std::ostringstream where_the; where_the << "go_statement(), DFC = PID - when setting \"max_monotone\" parameter to value \"" << parameter << "\": ";

            m_s.max_monotone = unsigned_int(parameter,where_the.str());

            if (m_s.max_monotone < 4)
            {
                ostringstream o;
                o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid max_monotone parameter \"" << parameter << "\".  Must be greater than or equal to 4." << std::endl << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }

//----------------------------------- balanced_step_direction_by
        {
            std::string parameter = go_parameters.retrieve("balanced_step_direction_by");

            std::ostringstream where_the; where_the << "go_statement(), DFC = PID - when setting \"balanced_step_direction_by\" parameter to value \"" << parameter << "\": ";

            m_s.balanced_step_direction_by = unsigned_int(parameter,where_the.str());

            if (m_s.balanced_step_direction_by < 6)
            {
                ostringstream o;
                o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid balanced_step_direction_by parameter \"" << parameter << "\".  Must be greater than or equal to 6." << std::endl << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }

//----------------------------------- ballpark_seconds
        {
            std::string parameter = go_parameters.retrieve("ballpark_seconds");

            std::ostringstream where_the; where_the << "go_statement(), DFC = PID - when setting \"ballpark_seconds\" parameter to value \"" << parameter << "\": ";

            m_s.ballpark_seconds = number_optional_trailing_percent(parameter,where_the.str());

            if (m_s.ballpark_seconds <= 0.0)
            {
                ostringstream o;
                o << std::endl << "<Error> ivy engine API - go() - [Go] statement - invalid ballpark_seconds parameter \"" << parameter << "\".  Must be greater than zero." << std::endl << std::endl;
                log(masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
        }
    }


    if (have_measure)

    {
//----------------------------------- accuracy_plus_minus
        accuracy_plus_minus_parameter = go_parameters.retrieve("accuracy_plus_minus");

        try
        {
            accuracy_plus_minus_fraction = number_optional_trailing_percent(accuracy_plus_minus_parameter,"accuracy_plus_minus");
        }
        catch (std::invalid_argument& iaex)
        {
            std::ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid accuracy_plus_minus parameter \"" << accuracy_plus_minus_parameter << "\".  Must be a number optionally with a trailing percent sign," << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }


//----------------------------------- confidence
        confidence_parameter = go_parameters.retrieve("confidence");
        try
        {
            confidence = number_optional_trailing_percent(confidence_parameter,"confidence");
        }
        catch(std::invalid_argument& iae)
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid confidence parameter \"" << confidence_parameter << "\".  Must be a number." << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }


//----------------------------------- timeout_seconds
        timeout_seconds_parameter = go_parameters.retrieve("timeout_seconds");
        try
        {
            std::string ts = go_parameters.retrieve("timeout_seconds");
            trim(ts);
            timeout_seconds = number_optional_trailing_percent(rewrite_HHMMSS_to_seconds(ts),"timeout_seconds");
        }
        catch(std::invalid_argument& iae)
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid timeout_seconds parameter \"" << timeout_seconds_parameter << "\".  Must be a number." << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }

//----------------------------------- min_wp
        min_wp_parameter = go_parameters.retrieve("min_wp");
        try
        {
            min_wp = number_optional_trailing_percent(min_wp_parameter,"min_wp");

        }
        catch (std::invalid_argument& iaex)
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid min_wp parameter \"" << min_wp_parameter << "\".  Must be from 0% to 100% or from 0.0 to 1.0." << std::endl;
            log(masterlogfile,o.str());
            kill_subthreads_and_exit();
        }
        if ( min_wp < 0.0 || min_wp > 1.0 )
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid min_wp_range parameter \"" << min_wp_parameter << "\".  Must be from 0% to 100% or from 0.0 to 1.0." << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }

//----------------------------------- max_wp
        max_wp_parameter = go_parameters.retrieve("max_wp");
        try
        {
            max_wp = number_optional_trailing_percent(go_parameters.retrieve("max_wp"), "max_wp");

        }
        catch (std::invalid_argument& iaex)
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid max_wp parameter \"" << max_wp_parameter << "\".  Must be from 0% to 100% or from 0.0 to 1.0." << std::endl;
            log(masterlogfile,o.str());
            std::cout << o.str();
            kill_subthreads_and_exit();
        }
        if ( max_wp < 0.0 || max_wp > 1.0 || max_wp < min_wp)
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid max_wp_change parameter \"" << max_wp_parameter << "\".  Must be from 0% to 100% or from 0.0 to 1.0 and must not be less than min_wp." << std::endl;
            log(masterlogfile,o.str());
            kill_subthreads_and_exit();
        }

//----------------------------------- max_wp_change
        max_wp_change_parameter = go_parameters.retrieve("max_wp_change");
        try
        {
            max_wp_change = number_optional_trailing_percent(max_wp_change_parameter,"max_wp_change");

        }
        catch (std::invalid_argument& iaex)
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid max_wp_change parameter \"" << max_wp_change_parameter << "\"." << std::endl;
            std::cout << o.str();
            log(masterlogfile,o.str());
            kill_subthreads_and_exit();
        }
        if ( max_wp_change < 0.0 || max_wp_change > 1.0 )
        {
            ostringstream o;
            o << "<Error> ivy engine API - go() - Invalid max_wp_change parameter \"" << max_wp_change_parameter << "\".  Must be from 0% to 100% or from 0.0 to 1.0." << std::endl;
            log(masterlogfile,o.str());
            kill_subthreads_and_exit();
        }
    }

    extern bool running_IOPS_curve_IOPS_equals_max;

    extern ivy_float* p_IOPS_curve_max_IOPS;

    while (go_parameters.workload_loopy.run_iteration())
    {
        goStatementsSeen++;

        get_go.setToNow();

        {
            std:: ostringstream o;
            o << "step" << std::setw(4) << std::setfill('0') << (goStatementsSeen-1);
            stepNNNN=o.str();
        }

        if (go_parameters.contains("stepname"))
        {
            stepName = go_parameters.retrieve("stepname");
        }
        else
        {
            stepName = stepNNNN;
        }

        std::string suffix = go_parameters.workload_loopy.build_stepname_suffix();

        if ( stepName.size() > 0 && suffix.size() > 0 ) { stepName += std::string(" - ");}

        stepName += suffix;

        the_dfc.reset();

        void prepare_dedupe();

        prepare_dedupe();

        run_subinterval_sequence(&the_dfc);

        if (running_IOPS_curve_IOPS_equals_max)
        {
            if (p_IOPS_curve_max_IOPS == nullptr)
            {
                std::ostringstream o; o << "<Error> internal programming error - running_IOPS_curve_IOPS_equals_max=true but p_IOPS_curve_max_IOPS = nullptr at line "
                    << __LINE__ << " of " << __FILE__ << std::endl;
                log (masterlogfile,o.str());
                std::cout << o.str();
                kill_subthreads_and_exit();
            }
            (*p_IOPS_curve_max_IOPS) = measurements.back().all_equals_all_Total_IOPS;
        }

        rc.first=true;
        {
            std::ostringstream o;

            for (unsigned int i = 0; i < measurements.size(); i++)
            {
                o << measurements[i].step_times_line(i);
            }

            rc.second = o.str();

            m_s.step_duration_lines += o.str();

            log(masterlogfile, rc.second);

            std::cout << rc.second;
        }
    }

    return rc;
}


